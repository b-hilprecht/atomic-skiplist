# Skip List Implementation

Implements an append only skip list that supports a single writer and multiple readers using atomic operations. This kind of skiplist can be useful for LSM trees.

![benchmark results](scripts/results/throughput_comparison.png)

## Building and Running

```bash
mkdir build
cd build
cmake ..
make

# For release build run instead
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Build Instructions for TSAN

```bash
mkdir build-tsan
cd build-tsan

# Configure with TSan
export TSAN_OPTIONS="history_size=7 verbosity=1"
cmake -DCMAKE_BUILD_TYPE=Tsan ..
make
```

### Running Tests

```bash
# Run unit tests
./skiplist_test

# Load testing (concurrent)
# <skiplist_type: 0=normal, 1=atomic_sw, 2=mutex>
# <num_readers> <num_writers>
./concurrent_load_test 1 4 1  # Single-writer atomic skiplist
./concurrent_load_test 2 4 1  # Mutex

# Correctness testing
# <skiplist_type: 0=normal, 1=atomic_sw, 2=mutex>
# <num_writers> <num_readers>
./concurrent_correctness_test 1 4 1  # Single-writer atomic skiplist
./concurrent_correctness_test 2 4 1  # Mutex

```

### Running Benchmark Script

```
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

python3 scripts/benchmark.py
python3 scripts/plot.py
```

### Results

Average throughput comparison:

```
Average throughput comparison:

Read Throughput:
Mutex: 1884104.18 ops/sec
Atomic: 4512728.44 ops/sec

Write Throughput:
Mutex: 310107.40 ops/sec
Atomic: 851981.04 ops/sec

Throughput improvement of Atomic over Mutex:
Read: 139.5%
Write: 174.7%
```
