#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <immintrin.h> // For SIMD
#include <algorithm>
#include <queue>
#include <functional>
#include <condition_variable>
#include <cstdint>
#include <cstring>

// Timer for performance measurement
class Timer {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_time;

public:
    void start() { start_time = Clock::now(); }

    double elapsed() {
        auto end_time = Clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    }
};


// Data Structures for Dictionary Encoding
struct Dictionary {
    std::unordered_map<std::string, int> data_to_id;
    std::vector<std::string> id_to_data;
};

struct EncodedColumn {
    Dictionary dictionary;
    std::vector<int> encoded_data;
};

// Thread worker for encoding chunks
void encodeChunk(
    const std::vector<std::string>& input_data, 
    size_t start, 
    size_t end, 
    std::unordered_map<std::string, int>& local_dict, 
    std::vector<std::string>& local_id_to_data, 
    std::vector<int>& encoded_chunk) {

    for (size_t i = start; i < end; ++i) {
        const auto& item = input_data[i];
        if (local_dict.find(item) == local_dict.end()) {
            int id = static_cast<int>(local_id_to_data.size());
            local_dict[item] = id;
            local_id_to_data.push_back(item);
        }
        encoded_chunk.push_back(local_dict[item]);
    }
}

// Merge local dictionaries into the global dictionary
void mergeDictionaries(
    const std::unordered_map<std::string, int>& local_dict, 
    const std::vector<std::string>& local_id_to_data, 
    Dictionary& global_dict, 
    std::unordered_map<int, int>& local_to_global_map, 
    std::mutex& global_mutex) {

    for (size_t local_id = 0; local_id < local_id_to_data.size(); ++local_id) {
        const std::string& item = local_id_to_data[local_id];
        std::lock_guard<std::mutex> lock(global_mutex);
        if (global_dict.data_to_id.find(item) == global_dict.data_to_id.end()) {
            int global_id = static_cast<int>(global_dict.id_to_data.size());
            global_dict.data_to_id[item] = global_id;
            global_dict.id_to_data.push_back(item);
        }
        local_to_global_map[local_id] = global_dict.data_to_id[item];
    }
}

// Perform dictionary encoding with multithreading
EncodedColumn encodeDictionary(const std::vector<std::string>& input_data, int num_threads) {
    size_t data_size = input_data.size();
    EncodedColumn encoded_column;
    encoded_column.encoded_data.resize(data_size);

    // Determine chunk size
    size_t chunk_size = (data_size + num_threads - 1) / num_threads;

    // Thread-specific structures
    std::vector<std::unordered_map<std::string, int>> local_dicts(num_threads);
    std::vector<std::vector<std::string>> local_id_to_data(num_threads);
    std::vector<std::vector<int>> local_encoded_chunks(num_threads);

    // Launch threads to process chunks
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        size_t end = std::min(start + chunk_size, data_size);

        if (start < end) {
            threads.emplace_back(
                encodeChunk,
                std::cref(input_data),
                start,
                end,
                std::ref(local_dicts[t]),
                std::ref(local_id_to_data[t]),
                std::ref(local_encoded_chunks[t]));
        }
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Merge local dictionaries into the global dictionary
    std::mutex global_mutex;
    std::vector<std::unordered_map<int, int>> local_to_global_maps(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        mergeDictionaries(local_dicts[t], local_id_to_data[t], encoded_column.dictionary, local_to_global_maps[t], global_mutex);
    }

    // Remap encoded data with global IDs
    for (int t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        for (size_t i = 0; i < local_encoded_chunks[t].size(); ++i) {
            encoded_column.encoded_data[start + i] = local_to_global_maps[t][local_encoded_chunks[t][i]];
        }
    }

    return encoded_column;
}


// SIMD prefix query search
std::vector<std::pair<std::string, std::vector<int>>> simdPrefixQuery(const EncodedColumn& encoded_column, const std::string& prefix) {
    std::vector<std::pair<std::string, std::vector<int>>> results;

    size_t prefix_length = prefix.size();
    if (prefix_length == 0) {
        std::cerr << "Error: Prefix length cannot be zero.\n";
        return results;
    }

    // Load the prefix into a 256-bit SIMD register
    __m256i prefix_vec = _mm256_setzero_si256();
    std::memcpy(&prefix_vec, prefix.c_str(), std::min(prefix_length, size_t(32)));

    const auto& dict_entries = encoded_column.dictionary.id_to_data;

    // Process dictionary in batches of 8 for SIMD acceleration
    size_t i = 0;
    for (; i + 8 <= dict_entries.size(); i += 8) {
        __m256i dict_vecs[8];

        // Load 8 dictionary entries into SIMD registers
        for (int j = 0; j < 8; ++j) {
            const auto& dict_entry = dict_entries[i + j];
            dict_vecs[j] = _mm256_setzero_si256();
            std::memcpy(&dict_vecs[j], dict_entry.c_str(), std::min(dict_entry.size(), size_t(32)));
        }

        // Compare each dictionary entry with the prefix
        for (int j = 0; j < 8; ++j) {
            __m256i cmp = _mm256_cmpeq_epi8(prefix_vec, dict_vecs[j]);
            int mask = _mm256_movemask_epi8(cmp);

            // Check if the first `prefix_length` bytes match
            if ((mask & ((1 << prefix_length) - 1)) == ((1 << prefix_length) - 1)) {
                const std::string& dict_entry = dict_entries[i + j];
                std::vector<int> indices;

                // Find all indices where this dictionary entry appears in the encoded data
                for (size_t k = 0; k < encoded_column.encoded_data.size(); ++k) {
                    if (encoded_column.encoded_data[k] == static_cast<int>(i + j)) {
                        indices.push_back(k);
                    }
                }

                results.emplace_back(dict_entry, indices);
            }
        }
    }

    // Handle remaining dictionary entries (scalar processing)
    for (; i < dict_entries.size(); ++i) {
        const std::string& dict_entry = dict_entries[i];

        if (dict_entry.size() >= prefix_length &&
            std::memcmp(dict_entry.c_str(), prefix.c_str(), prefix_length) == 0) {
            std::vector<int> indices;

            for (size_t k = 0; k < encoded_column.encoded_data.size(); ++k) {
                if (encoded_column.encoded_data[k] == static_cast<int>(i)) {
                    indices.push_back(k);
                }
            }

            results.emplace_back(dict_entry, indices);
        }
    }

    return results;
}




// prefix query with no simd

std::vector<std::string> vanillaPrefixQuery(const std::vector<std::string>& data, const std::string& prefix) {
    std::vector<std::string> results;
    size_t prefix_length = prefix.size();

    // Iterate through each string in the dataset
    for (const auto& str : data) {
        // Check if the string starts with the given prefix
        if (str.size() >= prefix_length && str.compare(0, prefix_length, prefix) == 0) {
            results.push_back(str);
        }
    }

    return results;
}


// SIMD for singular item
std::vector<int> simdQuery(const EncodedColumn& encoded_column, const std::string& query) {
    std::vector<int> indices;

    // check if the query exists in the dictionary
    auto it = encoded_column.dictionary.data_to_id.find(query);
    if (it == encoded_column.dictionary.data_to_id.end()) {
        return indices; 
    }

    int query_id = it->second;

    // SIMD search for matching IDs in the encoded data
    size_t data_size = encoded_column.encoded_data.size();
    size_t simd_width = 8; 
    __m256i query_vec = _mm256_set1_epi32(query_id);

    size_t i = 0;
    for (; i + simd_width <= data_size; i += simd_width) {
        __m256i data_vec = _mm256_loadu_si256((__m256i*)&encoded_column.encoded_data[i]);
        __m256i cmp = _mm256_cmpeq_epi32(data_vec, query_vec);
        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));

        for (int j = 0; j < simd_width; ++j) {
            if (mask & (1 << j)) {
                indices.push_back(i + j);
            }
        }
    }

    // scalar processing for remaining elements
    for (; i < data_size; ++i) {
        if (encoded_column.encoded_data[i] == query_id) {
            indices.push_back(i);
        }
    }

    return indices;
}


// vanilla search for singular item
std::vector<int> vanillaSearch(const std::vector<std::string>& raw_data, const std::string& query) {
    std::vector<int> result_indices;
    for (size_t i = 0; i < raw_data.size(); ++i) {
        if (raw_data[i] == query) {
            result_indices.push_back(i);
        }
    }
    return result_indices;
}



// File Handling
std::vector<std::string> readColumnFromFile(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::string> data;
    std::string line;

    while (std::getline(file, line)) {
        data.push_back(line);
    }
    return data;
}

void writeEncodedColumnToFile(const EncodedColumn& encoded_column, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Error: Unable to open file for writing: " << filename << "\n";
        return;
    }

    // Write the dictionary to the file
    file << "Dictionary:\n";
    for (size_t i = 0; i < encoded_column.dictionary.id_to_data.size(); ++i) {
        file << i << ": " << encoded_column.dictionary.id_to_data[i] << "\n";
    }

    // Write the encoded data
    file << "\nEncoded Data:\n";
    for (int id : encoded_column.encoded_data) {
        file << id << " ";
    }
    file << "\n";

    file.close();
}


int main() {
    std::string input_file = "Column.txt";
    std::string output_file = "encoded_data.txt";
    int num_threads ; 
    std::cout << "How many threads do you want for multithread encoding?" << std::endl;
    std::cin >> num_threads;
    auto data = readColumnFromFile(input_file);

    auto start = std::chrono::high_resolution_clock::now();
    auto encoded_column = encodeDictionary(data, num_threads);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Encoding time: " << elapsed.count() << " s\n";

    writeEncodedColumnToFile(encoded_column, output_file);

    std::string selection;
    
    bool quitting = false;
    while (quitting  == false){
        std::cout << "Do you want singular search (s) or prefix search (p)? (Type x to cancel the program)" << std::endl;
        std::cin >> selection;
        if (selection == "x"){
            quitting = true;
        }
        else if (selection == "s"){
            std::string query; 
            std::cout << "Please enter a query for what you want to search for" << std::endl;
            std::cin >> query;

            // SIMD Single Query Test
            std::cout << "\nTesting SIMD Single Query for: " << query << std::endl;
            auto start1 = std::chrono::high_resolution_clock::now();
            auto simd_single_results = simdQuery(encoded_column, query);
            auto end1 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed1 = end1 - start1;
            
            std::cout << "SIMD single search query time: " << elapsed1.count() << " s\n";
            std::cout << "SIMD Single Query Results: ";
            for (int idx : simd_single_results) {
                std::cout << idx << " ";
            }
            std::cout << std::endl;

            // Vanilla Single Query Test
            std::cout << "\nTesting Vanilla Single Query for: " << query << std::endl;
            auto start2 = std::chrono::high_resolution_clock::now();
            auto vanilla_single_results = vanillaSearch(data, query);
            auto end2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed2 = end2 - start2;
            std::cout << "Vanilla single search query time: " << elapsed2.count() << " s\n";

        }
        else if (selection == "p"){
            std::string prefix ; 
            std::cout << "Type your prefix" << std::endl;
            std::cin >> prefix;
            // SIMD Prefix Query Test
            std::cout << "\nTesting SIMD Prefix Query for prefix: " << prefix << std::endl;
            auto start3 = std::chrono::high_resolution_clock::now();
            auto simd_prefix_results = simdPrefixQuery(encoded_column, prefix);
            auto end3 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed3 = end3 - start3;
            std::cout << "SIMD Prefix query time: " << elapsed3.count() << " s\n";
            std::cout << "SIMD Prefix Query Results: ";
            for (const auto& result : simd_prefix_results) {
                const std::string& dict_entry = result.first;  // Get the dictionary entry (string)
                const std::vector<int>& indices = result.second;  // Get the list of indices
                std::cout << "Data: " << dict_entry << " with indices: ";
                for (int idx : indices) {
                    std::cout << idx << " ";
                }
                std::cout << "\n";
            }
            std::cout << std::endl;

            // Vanilla Prefix Query Test
            std::cout << "\nTesting Vanilla Prefix Query for prefix: " << prefix << std::endl;
            auto start4 = std::chrono::high_resolution_clock::now();
            auto vanilla_prefix_results = vanillaPrefixQuery(data, prefix);
            auto end4 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed4 = end4 - start4;
            std::cout << "Vanilla prefix query time: " << elapsed4.count() << " s\n";

        }
    }

}
