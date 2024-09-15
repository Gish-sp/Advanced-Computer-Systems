import numpy as np
import time
def tlb_miss_simulation(array_size, stride):
    arr = np.ones(array_size, dtype=np.float64)
    start_time = time.perf_counter()

    for i in range(0, array_size, stride):
        arr[i] = arr[i] * 2

    end_time = time.perf_counter()
    execution_time = end_time - start_time
    return execution_time

# Example usage with varying stride sizes to induce TLB misses:
array_size = 10**7
strides = [1, 16, 64, 256, 1024]  # Different strides to vary TLB miss ratios

for stride in strides:
    exec_time = tlb_miss_simulation(array_size, stride)
    print(f"Stride: {stride}, Execution Time: {exec_time} seconds")
