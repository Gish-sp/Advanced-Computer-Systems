from concurrent.futures import ThreadPoolExecutor
import numpy as np
import time
def memory_access_simulation(arr, start, end, access_type="read"):
    if access_type == "read":
        for i in range(start, end):
            _ = arr[i]
    elif access_type == "write":
        for i in range(start, end):
            arr[i] = i

def measure_latency_throughput(array_size, num_threads, access_type="read"):
    arr = np.ones(array_size, dtype=np.float64)
    chunk_size = array_size // num_threads
    start_time = time.perf_counter()

    with ThreadPoolExecutor(max_workers=num_threads) as executor:
        futures = []
        for i in range(num_threads):
            start_idx = i * chunk_size
            end_idx = (i + 1) * chunk_size
            futures.append(executor.submit(memory_access_simulation, arr, start_idx, end_idx, access_type))

        for future in futures:
            future.result()

    end_time = time.perf_counter()
    latency = (end_time - start_time) / num_threads
    return latency

# Example usage with varying threads:
threads = [1, 2, 4, 8, 16]  # different concurrency levels
array_size = 10**6

for t in threads:
    latency = measure_latency_throughput(array_size, t, "read")
    print(f"Threads: {t}, Latency: {latency * 1e6} µs")
