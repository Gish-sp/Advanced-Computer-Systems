#include <iostream>
#include <vector>
#include <thread>
#include <immintrin.h> // For SIMD instructions
#include <chrono>
#include <random>
#include <functional>



// Define the matrix as a 2D vector
typedef std::vector<std::vector<float>> Matrix;

// Function to generate matrices
Matrix generate_matrix(int rows, int cols, float sparsity);

// Function to multiply matrices without any optimization
Matrix multiply_native(const Matrix& A, const Matrix& B);

// Function to multiply matrices using multithreading
Matrix multiply_multithreaded(const Matrix& A, const Matrix& B, int num_threads);

// Function to multiply matrices using SIMD
Matrix multiply_simd(const Matrix& A, const Matrix& B);

// Function to multiply matrices using cache optimization
Matrix multiply_cache_optimized(const Matrix& A, const Matrix& B);

// Helper function to perform block multiplication
void block_multiply(const Matrix& A, const Matrix& B, Matrix& result, int i_start, int i_end, int j_start, int j_end, int k_start, int k_end);

// Function to combine all optimizations
Matrix multiply_optimized(const Matrix& A, const Matrix& B, int num_threads, bool use_simd, bool use_cache_optimization);

// Function to benchmark
void benchmark(Matrix (*mult_func)(const Matrix&, const Matrix&), const Matrix& A, const Matrix& B, const std::string& label);

// Function to benchmark multithreaded proccesses
void benchmark_multithreading(const Matrix& A, const Matrix& B, const std::vector<int>& thread_counts);

// Function to benchmark all processes
void benchmark_optimized(const Matrix& A, const Matrix& B, int num_threads, bool use_simd, bool use_cache_optimization);






int main(int argc, char** argv) {
    // Parse command-line arguments for configurations
    int num_threads = 1; // default number of threads
    int matrix_size; 
    std::vector<int> thread_counts = {1, 2, 4, 8, 16, 32};
    bool terminate = false;
    char check = 'n';

    while(terminate != true){

        bool use_simd = false;
        bool use_cache_optimization = false;
        std::cout << "Type a value for matrix size: " << std::endl; // Type a number and press enter
        std::cin >> matrix_size; 
        float sparcity1;
        std::cout << "Type a value for matrix A sparcity: " << std::endl; // Type a number and press enter
        std::cin >> sparcity1; 
        float sparcity2;
        std::cout << "Type a value for matrix B sparcity: " << std::endl; // Type a number and press enter
        std::cin >> sparcity2; 

        // Generate matrices for benchmarking
        Matrix A = generate_matrix(matrix_size, matrix_size, sparcity1);  // Dense matrix
        Matrix B = generate_matrix(matrix_size, matrix_size, sparcity2);  // Sparse matrix

        int usr_choice;
        std::cout << "How would you like the program to run? " << std::endl; // Type a number and press enter
        std::cout << "1: Native " << std::endl; // Type a number and press enter
        std::cout << "2: Multithreading " << std::endl; // Type a number and press enter
        std::cout << "3: SIMD " << std::endl; // Type a number and press enter
        std::cout << "4: Cache-Optimized " << std::endl; // Type a number and press enter
        std::cout << "5: All Optimizations " << std::endl; // Type a number and press enter
        std::cin >> usr_choice;

        std::cout << "Benchmarking Matrix of size: " << matrix_size << "x" << matrix_size << std::endl;
        std::cout << "Sparcity: A = " << sparcity1 << " B = " << sparcity2 << std::endl;
    // Perform benchmark on user specified matrices
        if (usr_choice == 1){
            benchmark(multiply_native, A, B, "Native");
        } else if (usr_choice == 2){
            std::cout << "Multithreaded:" << std::endl;
            benchmark_multithreading(A, B, thread_counts);
        } else if (usr_choice == 3){
            benchmark(multiply_simd, A, B, "SIMD");
        } else if (usr_choice == 4){
            benchmark(multiply_cache_optimized, A, B, "Cache Optimized");
        } else if (usr_choice == 5){
            std::cout << "How many threads would you like to use? " << std::endl; // Type a number and press enter
            std::cin >> num_threads;
            use_simd = true;
            use_cache_optimization = true;
            benchmark_optimized(A, B, num_threads, use_simd, use_cache_optimization);
        }
        std::cout << "Would you like to end the program? y or n" << std::endl;
        std::cin >> check;
        if (check == 'y'){
            terminate = true;
        }
    }
    return 0;
}

void benchmark_optimized(const Matrix& A, const Matrix& B, int num_threads, bool use_simd, bool use_cache_optimization){
    auto start = std::chrono::high_resolution_clock::now();
    Matrix result = multiply_optimized(A,B,num_threads, use_simd, use_cache_optimization);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "All optimizations " << ": " << elapsed.count() << " seconds" << std::endl;
}


Matrix multiply_native(const Matrix& A, const Matrix& B) {
    int rows_A = A.size();        // Number of rows in matrix A
    int cols_A = A[0].size();     // Number of columns in matrix A
    int cols_B = B[0].size();     // Number of columns in matrix B

    // Initialize result matrix with zeros
    Matrix result(rows_A, std::vector<float>(cols_B, 0));

    // Perform the matrix multiplication: result[i][j] = sum(A[i][k] * B[k][j])
    for (int i = 0; i < rows_A; ++i) {
        for (int j = 0; j < cols_B; ++j) {
            for (int k = 0; k < cols_A; ++k) {
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    return result;
}


Matrix multiply_multithreaded(const Matrix& A, const Matrix& B, int num_threads) {
    int rows_A = A.size();
    int cols_A = A[0].size();
    int cols_B = B[0].size();

    Matrix result(rows_A, std::vector<float>(cols_B, 0));

    auto worker = [&](int start_row, int end_row) {
        for (int i = start_row; i < end_row; ++i) {
            for (int k = 0; k < cols_A; ++k) {
                for (int j = 0; j < cols_B; ++j) {
                    result[i][j] += A[i][k] * B[k][j];
                }
            }
        }
    };

    std::vector<std::thread> threads;
    int rows_per_thread = rows_A / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        int start_row = t * rows_per_thread;
        int end_row = (t == num_threads - 1) ? rows_A : start_row + rows_per_thread;
        threads.emplace_back(worker, start_row, end_row);
    }

    for (auto& t : threads) {
        t.join();
    }

    return result;
}



Matrix multiply_simd(const Matrix& A, const Matrix& B) {
    int rows_A = A.size();
    int cols_A = A[0].size();
    int cols_B = B[0].size();

    Matrix result(rows_A, std::vector<float>(cols_B, 0));

    for (int i = 0; i < rows_A; i++) {
        for (int j = 0; j < cols_B; j++) {
            __m256 sum = _mm256_setzero_ps(); // SIMD sum
            for (int k = 0; k < cols_A; k += 8) {
                __m256 a = _mm256_loadu_ps(&A[i][k]);
                __m256 b = _mm256_loadu_ps(&B[k][j]);
                 sum = _mm256_fmadd_ps(a, b, sum); // SIMD fused multiply-add
            }
            float temp[8];
            _mm256_storeu_ps(temp, sum);
            result[i][j] = temp[0] + temp[1] + temp[2] + temp[3] + temp[4] + temp[5] + temp[6] + temp[7];
        }
    }

    return result;
}


Matrix multiply_cache_optimized(const Matrix& A, const Matrix& B) {
    int block_size = 64; // Choose an optimal block size based on cache size
    int rows_A = A.size();
    int cols_A = A[0].size();
    int cols_B = B[0].size();

    Matrix result(rows_A, std::vector<float>(cols_B, 0));

    for (int i = 0; i < rows_A; i += block_size) {
        for (int j = 0; j < cols_B; j += block_size) {
            for (int k = 0; k < cols_A; k += block_size) {
                for (int ii = i; ii < std::min(i + block_size, rows_A); ++ii) {
                    for (int kk = k; kk < std::min(k + block_size, cols_A); ++kk) {
                        for (int jj = j; jj < std::min(j + block_size, cols_B); ++jj) {
                            result[ii][jj] += A[ii][kk] * B[kk][jj];
                        }
                    }
                }
            }
        }
    }

    return result;
}


void benchmark(Matrix (*mult_func)(const Matrix&, const Matrix&), const Matrix& A, const Matrix& B, const std::string& label) {
    auto start = std::chrono::high_resolution_clock::now();
    Matrix result = mult_func(A, B);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << label << ": " << elapsed.count() << " seconds" << std::endl;
}

// Function to benchmark multithreaded matrix multiplication for various thread counts
void benchmark_multithreading(const Matrix& A, const Matrix& B, const std::vector<int>& thread_counts) {
    for (int num_threads : thread_counts) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Call the multithreaded matrix multiplication
        Matrix result = multiply_multithreaded(A, B, num_threads);
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        std::cout << "Threads: " << num_threads << " | Time: " << elapsed.count() << " seconds" << std::endl;
    }
}

Matrix generate_matrix(int rows, int cols, float sparsity) {
    Matrix matrix(rows, std::vector<float>(cols, 0.0f));

    // Random number generator to populate matrix elements
    std::random_device rd;  // Seed
    std::mt19937 gen(rd()); // Random number generator
    std::uniform_real_distribution<> dis(0.0, 1.0); // Distribution for sparsity
    std::uniform_real_distribution<> value_dis(-10.0, 10.0); // Distribution for values

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (dis(gen) > sparsity) {
                matrix[i][j] = value_dis(gen); // Assign random value to the matrix element
            }
        }
    }

    return matrix;
}

// Helper function to perform block multiplication
void block_multiply(const Matrix& A, const Matrix& B, Matrix& result, int i_start, int i_end, int j_start, int j_end, int k_start, int k_end) {
    for (int i = i_start; i < i_end; ++i) {
        for (int k = k_start; k < k_end; ++k) {
            for (int j = j_start; j < j_end; ++j) {
                result[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Optimized matrix multiplication function
Matrix multiply_optimized(const Matrix& A, const Matrix& B, int num_threads, bool use_simd, bool use_cache_optimization) {
    int rows_A = A.size();
    int cols_A = A[0].size();
    int cols_B = B[0].size();

    Matrix result(rows_A, std::vector<float>(cols_B, 0));

    auto worker = [&](int start_row, int end_row) {
        int block_size = use_cache_optimization ? 64 : cols_A;

        for (int i = start_row; i < end_row; i += block_size) {
            for (int j = 0; j < cols_B; j += block_size) {
                for (int k = 0; k < cols_A; k += block_size) {
                    // Choose the appropriate sub-matrix bounds
                    int i_end = std::min(i + block_size, rows_A);
                    int j_end = std::min(j + block_size, cols_B);
                    int k_end = std::min(k + block_size, cols_A);

                    if (use_simd) {
                        // SIMD Optimized Block Multiplication
                        for (int ii = i; ii < i_end; ++ii) {
                            for (int jj = j; jj < j_end; ++jj) {
                                __m256 sum = _mm256_setzero_ps(); // SIMD sum
                                for (int kk = k; kk < k_end; kk += 8) {
                                    __m256 a = _mm256_loadu_ps(&A[ii][kk]);  // Load 8 elements from A
                                    __m256 b = _mm256_loadu_ps(&B[kk][jj]);  // Load 8 elements from B
                                    sum = _mm256_fmadd_ps(a, b, sum);        // Fused multiply-add
                                }
                                // Store the sum result
                                float temp[8];
                                _mm256_storeu_ps(temp, sum);
                                result[ii][jj] += temp[0] + temp[1] + temp[2] + temp[3] + temp[4] + temp[5] + temp[6] + temp[7];
                            }
                        }
                    } else {
                        // Standard block multiplication
                        block_multiply(A, B, result, i, i_end, j, j_end, k, k_end);
                    }
                }
            }
        }
    };

    // Create multiple threads to parallelize computation
    std::vector<std::thread> threads;
    int rows_per_thread = rows_A / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        int start_row = t * rows_per_thread;
        int end_row = (t == num_threads - 1) ? rows_A : start_row + rows_per_thread;
        threads.emplace_back(worker, start_row, end_row);
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    return result;
}



