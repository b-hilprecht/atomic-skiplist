import subprocess
import csv
import re
from typing import NamedTuple
from pathlib import Path


class BenchmarkResult(NamedTuple):
    read_throughput: float
    write_throughput: float
    read_latency_p50: float
    read_latency_p99: float
    write_latency_p50: float
    write_latency_p99: float


def parse_output(output: str) -> BenchmarkResult:
    # Extract overall throughput
    read_throughput = float(
        re.search(r"Total Read Throughput:\s+(\d+\.\d+)", output).group(1)
    )
    write_throughput = float(
        re.search(r"Total Write Throughput:\s+(\d+\.\d+)", output).group(1)
    )

    # Extract read latencies
    read_latencies = {}
    read_section = output[output.find("Read Latency Statistics") :]
    read_latencies["p50"] = float(
        re.search(r"50th %ile:\s+(\d+\.\d+)", read_section).group(1)
    )
    read_latencies["p99"] = float(
        re.search(r"99th %ile:\s+(\d+\.\d+)", read_section).group(1)
    )

    # Extract writer latencies from the per-thread stats
    writer_section = output[output.find("Per-writer Thread Stats") :]
    writer_stats = re.search(
        r"Writer 0:.*p50: (\d+\.\d+).*p99: (\d+\.\d+)", writer_section
    )
    write_latency_p50 = float(writer_stats.group(1))
    write_latency_p99 = float(writer_stats.group(2))

    return BenchmarkResult(
        read_throughput=read_throughput,
        write_throughput=write_throughput,
        read_latency_p50=read_latencies["p50"],
        read_latency_p99=read_latencies["p99"],
        write_latency_p50=write_latency_p50,
        write_latency_p99=write_latency_p99,
    )


def find_load_test_executable() -> Path:
    """Find the concurrent_load_test executable in common build directories."""
    current_dir = Path(__file__).parent.parent
    common_build_dirs = ["build", "build-release"]

    for build_dir in common_build_dirs:
        executable = current_dir / build_dir / "concurrent_load_test"
        if executable.exists():
            return executable

    raise FileNotFoundError(
        "Could not find concurrent_load_test executable. "
        "Please build the project first using:\n"
        "mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make"
    )


def run_benchmark(
    executable: Path, skiplist_type: int, num_readers: int
) -> BenchmarkResult:
    cmd = [
        str(executable),
        str(skiplist_type),
        str(num_readers),
        "1",
    ]  # Always 1 writer
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return parse_output(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error running benchmark: {e}")
        print(f"Output: {e.output}")
        raise


def main():
    # Ensure output directory exists
    output_dir = Path(__file__).parent / "results"
    output_dir.mkdir(exist_ok=True)

    try:
        executable = find_load_test_executable()
    except FileNotFoundError as e:
        print(e)
        return

    reader_counts = [1, 2, 4, 8, 16]
    skiplist_types = {"mutex": 2, "atomic": 1}

    results = []

    # Run benchmarks
    for num_readers in reader_counts:
        row = {"num_readers": num_readers}

        for impl_name, impl_type in skiplist_types.items():
            print(f"Running benchmark: {impl_name} with {num_readers} readers...")
            result = run_benchmark(executable, impl_type, num_readers)

            row.update(
                {
                    f"{impl_name}_read_throughput": result.read_throughput,
                    f"{impl_name}_write_throughput": result.write_throughput,
                    f"{impl_name}_read_latency_p50": result.read_latency_p50,
                    f"{impl_name}_read_latency_p99": result.read_latency_p99,
                    f"{impl_name}_write_latency_p50": result.write_latency_p50,
                    f"{impl_name}_write_latency_p99": result.write_latency_p99,
                }
            )

        results.append(row)

    # Write results to CSV
    fieldnames = [
        "num_readers",
        "mutex_read_throughput",
        "mutex_write_throughput",
        "mutex_read_latency_p50",
        "mutex_read_latency_p99",
        "mutex_write_latency_p50",
        "mutex_write_latency_p99",
        "atomic_read_throughput",
        "atomic_write_throughput",
        "atomic_read_latency_p50",
        "atomic_read_latency_p99",
        "atomic_write_latency_p50",
        "atomic_write_latency_p99",
    ]

    output_file = output_dir / "skiplist_benchmarks.csv"
    with open(output_file, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)

    print(f"\nBenchmark results have been written to {output_file}")


if __name__ == "__main__":
    main()
