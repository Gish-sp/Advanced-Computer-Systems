import numpy as np
import time
def measure_bandwidth(array_size, access_granularity, read_write_ratio=(1, 0)):
    arr = np.ones(array_size, dtype=np.float64)
    total_reads, total_writes = read_write_ratio
    
    start_time = time.perf_counter()

    for i in range(0, len(arr), access_granularity):
        # Read according to the ratio
        for _ in range(total_reads):
            _ = arr[i:i + access_granularity]
        
        # Write according to the ratio
        for _ in range(total_writes):
            arr[i:i + access_granularity] = i

    end_time = time.perf_counter()
    total_time = end_time - start_time
    data_transferred = array_size * 8  # 8 bytes per float64
    bandwidth = data_transferred / total_time / (1024**3)  # GB/s
    return bandwidth

# Example usage for different granularities and read/write ratios:
array_size = 10**7
granularities = [64 // 8, 256 // 8, 1024 // 8]  # in elements (64B, 256B, 1024B)
ratios = [(1, 0), (0, 1), (7, 3), (5, 5)]  # read-only, write-only, 70:30, 50:50

for gran in granularities:
    for ratio in ratios:
        bw = measure_bandwidth(array_size, gran, ratio)
        print(f"Granularity: {gran * 8}B, Read {ratio[0]}, Write {ratio[1]}, Bandwidth: {bw} GB/s")
