import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

# Use a simple style that's always available
plt.style.use("default")

# Read the CSV file
results_dir = Path(__file__).parent / "results"
df = pd.read_csv(results_dir / "skiplist_benchmarks.csv")

# Create figure with two subplots
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

# Plot read throughput
ax1.plot(
    df["num_readers"],
    df["mutex_read_throughput"],
    marker="o",
    label="Mutex",
    color="#1f77b4",
)
ax1.plot(
    df["num_readers"],
    df["atomic_read_throughput"],
    marker="s",
    label="Atomic",
    color="#ff7f0e",
)
ax1.set_xlabel("Number of Readers")
ax1.set_ylabel("Read Throughput (ops/sec)")
ax1.set_title("Read Throughput vs Number of Readers")
ax1.grid(True, alpha=0.3)
ax1.legend()

# Plot write throughput
ax2.plot(
    df["num_readers"],
    df["mutex_write_throughput"],
    marker="o",
    label="Mutex",
    color="#1f77b4",
)
ax2.plot(
    df["num_readers"],
    df["atomic_write_throughput"],
    marker="s",
    label="Atomic",
    color="#ff7f0e",
)
ax2.set_xlabel("Number of Readers")
ax2.set_ylabel("Write Throughput (ops/sec)")
ax2.set_title("Write Throughput vs Number of Readers")
ax2.grid(True, alpha=0.3)
ax2.legend()

plt.tight_layout()
plt.savefig(results_dir / "throughput_comparison.png", dpi=300, bbox_inches="tight")

# Print summary statistics
print("Average throughput comparison:")
print("\nRead Throughput:")
print(f"Mutex: {df['mutex_read_throughput'].mean():.2f} ops/sec")
print(f"Atomic: {df['atomic_read_throughput'].mean():.2f} ops/sec")

print("\nWrite Throughput:")
print(f"Mutex: {df['mutex_write_throughput'].mean():.2f} ops/sec")
print(f"Atomic: {df['atomic_write_throughput'].mean():.2f} ops/sec")

print("\nThroughput improvement of Atomic over Mutex:")
read_improvement = (
    df["atomic_read_throughput"].mean() / df["mutex_read_throughput"].mean() - 1
) * 100
write_improvement = (
    df["atomic_write_throughput"].mean() / df["mutex_write_throughput"].mean() - 1
) * 100
print(f"Read: {read_improvement:.1f}%")
print(f"Write: {write_improvement:.1f}%")

plt.show()
