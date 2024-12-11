// to complie g++ -o main main.cpp `pkg-config --cflags --libs opencv4` -pthread -mavx2


#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <immintrin.h>
#include <fstream>
#include <jpeglib.h>
#include <cstdint> 

namespace fs = std::filesystem;

// Function to compress an image and save to a folder
void compressImage(const fs::path& inputPath, const fs::path& outputFolder, int quality) {
    cv::Mat image = cv::imread(inputPath.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cerr << "Could not read image: " << inputPath << std::endl;
        return;
    }

    std::vector<int> compressionParams = {cv::IMWRITE_JPEG_QUALITY, quality};
    fs::path outputPath = outputFolder / inputPath.filename();
    outputPath.replace_extension(".jpg");
    cv::imwrite(outputPath.string(), image, compressionParams);
}

// Single-threaded compression
void singleThreadedCompression(const fs::path& inputFolder, const fs::path& outputFolder, int quality) {
    for (const auto& entry : fs::directory_iterator(inputFolder)) {
        compressImage(entry.path(), outputFolder, quality);
    }
}

// Multithreaded compression with configurable thread count
void multithreadedCompression(const fs::path& inputFolder, const fs::path& outputFolder, int quality, int threadCount) {
    std::queue<fs::path> files;
    std::mutex queueMutex;

    for (const auto& entry : fs::directory_iterator(inputFolder)) {
        files.push(entry.path());
    }

    auto worker = [&]() {
        while (true) {
            fs::path filePath;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (files.empty()) break;
                filePath = files.front();
                files.pop();
            }
            compressImage(filePath, outputFolder, quality);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& thread : threads) {
        thread.join();
    }
}


// Helper function to read a JPEG file into a raw RGB buffer
bool readJPEG(const std::string& filename, std::vector<uint8_t>& buffer, int& width, int& height, int& channels) {
    FILE* infile = fopen(filename.c_str(), "rb");
    if (!infile) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;
    channels = cinfo.output_components;

    size_t row_stride = width * channels;
    buffer.resize(row_stride * height);

    while (cinfo.output_scanline < cinfo.output_height) {
        uint8_t* row_pointer = &buffer[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return true;
}

// Helper function to write a raw RGB buffer to a JPEG file
bool writeJPEG(const std::string& filename, const std::vector<uint8_t>& buffer, int width, int height, int channels, int quality) {
    FILE* outfile = fopen(filename.c_str(), "wb");
    if (!outfile) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = channels;
    cinfo.in_color_space = (channels == 3) ? JCS_RGB : JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    size_t row_stride = width * channels;
    while (cinfo.next_scanline < cinfo.image_height) {
        const uint8_t* row_pointer = &buffer[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    return true;
}

// SIMD-based downsampling function
void downsampleWithSIMD(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, int width, int height, int channels) {
    int new_width = width / 2;
    int new_height = height / 2;

    output.resize(new_width * new_height * channels);

    for (int y = 0; y < new_height; ++y) {
        for (int x = 0; x < new_width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int idx1 = (2 * y * width + 2 * x) * channels + c;
                int idx2 = (2 * y * width + 2 * x + 1) * channels + c;
                int idx3 = ((2 * y + 1) * width + 2 * x) * channels + c;
                int idx4 = ((2 * y + 1) * width + 2 * x + 1) * channels + c;

                // Load four pixels into SIMD registers
                __m128i p1 = _mm_set1_epi16(input[idx1]);
                __m128i p2 = _mm_set1_epi16(input[idx2]);
                __m128i p3 = _mm_set1_epi16(input[idx3]);
                __m128i p4 = _mm_set1_epi16(input[idx4]);

                // Compute the average
                __m128i sum = _mm_add_epi16(_mm_add_epi16(p1, p2), _mm_add_epi16(p3, p4));
                __m128i avg = _mm_srli_epi16(sum, 2); // Divide by 4

                output[(y * new_width + x) * channels + c] = static_cast<uint8_t>(_mm_extract_epi16(avg, 0));
            }
        }
    }
}


int main(int argc, char* argv[]) {
     if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input> " << std::endl;
        return 1;
    }
    fs::path inputFolder = argv[1];
    fs::path singleThreadedFolder = "results/output_single";
    fs::path multithreadedFolder = "results/output_multithreaded";
    fs::path simdFolder = "results/output_simd";

    fs::create_directories(singleThreadedFolder);
    fs::create_directories(multithreadedFolder);
    fs::create_directories(simdFolder);

    int quality = 90; // JPEG compression quality
    std::ofstream resultsFile("timing_results.csv");
    resultsFile << "Method,Threads,Time(ms)\n";

    //single threaded test 
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& entry : fs::directory_iterator(inputFolder)) {
        compressImage(entry.path(), singleThreadedFolder, quality);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double singleThreadedTime = std::chrono::duration<double, std::milli>(end - start).count();
    resultsFile << "Single-threaded,1," << singleThreadedTime << "\n";

    //multithreaded test
    int threadCount;
    std::cout << "Enter the number of threads to use: ";
    std::cin >> threadCount;

    start = std::chrono::high_resolution_clock::now();
    multithreadedCompression(inputFolder, multithreadedFolder, quality, threadCount);
    end = std::chrono::high_resolution_clock::now();
    double multithreadedTime = std::chrono::duration<double, std::milli>(end - start).count();
    resultsFile << "Multithreaded," << threadCount << "," << multithreadedTime << "\n";


    
    //SIMD test
    double simdTime = 0.0;
    int processedFiles = 0;

    for (const auto& entry : fs::directory_iterator(inputFolder)) {
        if (!entry.is_regular_file()) continue;

        const std::string inputFilename = entry.path().string();
        const std::string outputFilename = (simdFolder / entry.path().filename()).string();

        std::vector<uint8_t> inputBuffer, outputBuffer;
        int width, height, channels;

        // Read the input JPEG file
        if (!readJPEG(inputFilename, inputBuffer, width, height, channels)) {
            std::cerr << "Error reading file: " << inputFilename << std::endl;
            continue;
        }

        auto start = std::chrono::high_resolution_clock::now();

        downsampleWithSIMD(inputBuffer, outputBuffer, width, height, channels);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        //logStream << inputFilename << "," << duration.count() << "\n";
        simdTime += duration.count();
        ++processedFiles;

        // Write the compressed image
        if (!writeJPEG(outputFilename, outputBuffer, width / 2, height / 2, channels, 75)) {
            std::cerr << "Error writing file: " << outputFilename << std::endl;
            continue;
        }
    }
    //double simdTime = std::chrono::duration<double, std::milli>(end - start).count();
    resultsFile << "SIMD,1,"<< simdTime << "\n";

    resultsFile.close();

    std::cout << "Compression complete. Timing results saved to timing_results.csv.\n";

    
    return 0;
}
