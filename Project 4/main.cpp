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

// merge local dictionaries into the global dictionary
void mergeDictionaries(
    const std::unordered_map<std::string, int>& local_dict, 
    const std::vector<std::string>& local_id_to_data, 
    Dictionary& global_dict, 
    std::mutex& global_mutex) {

    for (const auto& pair : local_dict) {
        std::lock_guard<std::mutex> lock(global_mutex);
        if (global_dict.data_to_id.find(pair.first) == global_dict.data_to_id.end()) {
            int global_id = static_cast<int>(global_dict.id_to_data.size());
            global_dict.data_to_id[pair.first] = global_id;
            global_dict.id_to_data.push_back(pair.first);
        }
    }
}

//  dictionary encoding with multithreading
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

    // wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }


    std::mutex global_mutex;
    for (int t = 0; t < num_threads; ++t) {
        mergeDictionaries(local_dicts[t], local_id_to_data[t], encoded_column.dictionary, global_mutex);
    }

    // update encoded data with global IDs
    for (int t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        for (size_t i = 0; i < local_encoded_chunks[t].size(); ++i) {
            const auto& local_item = local_id_to_data[t][local_encoded_chunks[t][i]];
            encoded_column.encoded_data[start + i] = encoded_column.dictionary.data_to_id[local_item];
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

    __m256i prefix_vec = _mm256_set1_epi8(0); 
    std::memcpy(&prefix_vec, prefix.c_str(), std::min(prefix_length, size_t(32))); 

    for (size_t i = 0; i < encoded_column.dictionary.id_to_data.size(); ++i) {
        const std::string& dict_entry = encoded_column.dictionary.id_to_data[i];

        size_t dict_length = dict_entry.size();
        if (dict_length < prefix_length) {
            continue; 
        }

        // load current dictionary entry into SIMD 
        __m256i dict_vec = _mm256_setzero_si256();
        std::memcpy(&dict_vec, dict_entry.c_str(), std::min(dict_length, size_t(32)));

        // compare prefix and dictionary entry using SIMD
        __m256i cmp = _mm256_cmpeq_epi8(prefix_vec, dict_vec);
        int mask = _mm256_movemask_epi8(cmp);

        // check if the first `prefix_length` bytes match
        if ((mask & ((1 << prefix_length) - 1)) == ((1 << prefix_length) - 1)) {
            std::vector<int> indices;

            // find all indices where this dictionary entry appears in the encoded data
            for (size_t j = 0; j < encoded_column.encoded_data.size(); ++j) {
                if (encoded_column.encoded_data[j] == static_cast<int>(i)) {
                    indices.push_back(j);
                }
            }

            results.emplace_back(dict_entry, indices);
        }
    }

    return results;
}



// prefix query with no simd
std::vector<std::pair<std::string, std::vector<int>>> vanillaPrefixQuery(const EncodedColumn& encoded_column, const std::string& prefix) {
    std::vector<std::pair<std::string, std::vector<int>>> results;

    size_t prefix_length = prefix.size();
    if (prefix_length == 0) {
        std::cerr << "Error: Prefix length cannot be zero.\n";
        return results;
    }

    for (size_t i = 0; i < encoded_column.dictionary.id_to_data.size(); ++i) {
        const std::string& dict_entry = encoded_column.dictionary.id_to_data[i];

        if (dict_entry.compare(0, prefix_length, prefix) == 0) { 
            std::vector<int> indices;

            for (size_t j = 0; j < encoded_column.encoded_data.size(); ++j) {
                if (encoded_column.encoded_data[j] == static_cast<int>(i)) {
                    indices.push_back(j);
                }
            }

            results.emplace_back(dict_entry, indices);
        }
    }

    return results;
}


// SIMD for singular item
std::vector<int> simdQueryEncodedColumn(const EncodedColumn& encoded_column, const std::string& query) {
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
std::vector<int> vanillaSearch(const EncodedColumn& encoded_column, const std::string& query) {
    std::vector<int> indices;

    // check if the query exists in the dictionary
    auto it = encoded_column.dictionary.data_to_id.find(query);
    if (it == encoded_column.dictionary.data_to_id.end()) {
        return indices; 
    }

    int query_id = it->second;

    // linear scan for matching IDs in the encoded data
    for (size_t i = 0; i < encoded_column.encoded_data.size(); ++i) {
        if (encoded_column.encoded_data[i] == query_id) {
            indices.push_back(i);
        }
    }

    return indices;
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
    auto raw_data = readColumnFromFile(input_file);

    Timer timer;
    timer.start();
    auto encoded_column = encodeDictionary(raw_data, num_threads);
    std::cout << "Encoding time: " << timer.elapsed() << " ms\n";

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
            timer.start();
            auto results = simdQueryEncodedColumn(encoded_column, query);
            std::cout << "SIMD single search query time: " << timer.elapsed() << " ms\n";

            timer.start();
            auto resultsVanil = vanillaSearch(encoded_column, query);
            std::cout << "Vanilla single search query time: " << timer.elapsed() << " ms\n";


            std::cout << "SIMD Indices for query '" << query << "': ";
            for (int index : results) {
                std::cout << index << " ";
            }
            std::cout << "\n";


        }
        else if (selection == "p"){
            std::string prefix ; 
            std::cout << "Type your prefix" << std::endl;
            std::cin >> prefix;

            timer.start();
            auto simd_prefix_results = simdPrefixQuery(encoded_column, prefix);
            std::cout << "SIMD Prefix query time: " << timer.elapsed() << " ms\n";

            timer.start();
            auto vanilla_prefix_results = vanillaPrefixQuery(encoded_column, prefix);
            std::cout << "Vanilla Prefix query time: " << timer.elapsed() << " ms\n";

            // results of the SIMD prefix query
            std::cout << "SIMD Prefix query results for prefix '" << prefix << "':\n";
            for (const auto& [data, indices] : simd_prefix_results) {
                std::cout << "Data: " << data << " with indices: ";
                for (int index : indices) {
                    std::cout << index << " ";
                }
                std::cout << "\n";
            }
        }
    }

}
