import numpy as np
import time

def measure_latency(array_size, access_type="read"):
    arr = np.ones(array_size, dtype=np.float64)
    start_time = time.perf_counter()
    
    if access_type == "read":
        for i in range(len(arr)):
            _ = arr[i]
    elif access_type == "write":
        for i in range(len(arr)):
            arr[i] = i

    end_time = time.perf_counter()
    latency = (end_time - start_time) / len(arr)  # average latency per access
    return latency

# Example usage:
cache_size = 1024 * 1024 // 8  # small array fitting in cache
main_memory_size = 100 * cache_size  # larger array to ensure it spills into main memory

cache_latency_read = measure_latency(cache_size, "read")
cache_latency_write = measure_latency(cache_size, "write")
memory_latency_read = measure_latency(main_memory_size, "read")
memory_latency_write = measure_latency(main_memory_size, "write")

print(f"Cache Read Latency: {cache_latency_read * 1e9} ns")
print(f"Cache Write Latency: {cache_latency_write * 1e9} ns")
print(f"Main Memory Read Latency: {memory_latency_read * 1e9} ns")
print(f"Main Memory Write Latency: {memory_latency_write * 1e9} ns")
