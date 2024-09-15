import numpy as np
import time
def matrix_multiply(size):
    A = np.random.rand(size, size)
    B = np.random.rand(size, size)
    
    start_time = time.perf_counter()
    result = np.dot(A, B)
    end_time = time.perf_counter()
    
    execution_time = end_time - start_time
    return execution_time

# Example usage with different matrix sizes to induce cache misses:
matrix_sizes = [64, 256, 512, 1024, 2048]  # Varying matrix sizes

for size in matrix_sizes:
    exec_time = matrix_multiply(size)
    print(f"Matrix Size: {size}, Execution Time: {exec_time} seconds")
