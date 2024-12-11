import pandas as pd
import matplotlib.pyplot as plt

# Read timing results
results = pd.read_csv("timing_results.csv")

# Plot timings
plt.figure(figsize=(8, 6))
plt.bar(results['Method'], results['Time(ms)'])
plt.xlabel('Compression Method')
plt.ylabel('Time (ms)')
plt.title('Compression Timing Comparison')
plt.savefig("timing_comparison.png")
plt.show()
