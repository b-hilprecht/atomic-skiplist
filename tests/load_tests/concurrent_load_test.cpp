#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <atomic>
#include <iomanip>
#include <numeric>
#include <barrier>
#include <memory>

#include "skiplist.hpp"
#include "skiplist_atomic_sw.hpp"
#include "skiplist_mutex.hpp"

enum class SkipListType
{
    Normal,
    AtomicSW
};

// Per-thread statistics with latency tracking
struct ThreadStats
{
    size_t operations{0};
    uint64_t total_latency_ns{0};
    std::vector<uint64_t> latencies;

    ThreadStats()
    {
        latencies.reserve(1000000);
    }

    double avg_latency() const
    {
        return operations > 0 ? static_cast<double>(total_latency_ns) / operations : 0;
    }

    double throughput(double duration_sec) const
    {
        return operations / duration_sec;
    }

    double get_percentile(double p) const
    {
        if (latencies.empty())
            return 0;

        size_t idx = static_cast<size_t>(p * (latencies.size() - 1));
        return static_cast<double>(latencies[idx]);
    }

    void finalize()
    {
        std::sort(latencies.begin(), latencies.end());
    }
};

// Combined test results
struct TestResults
{
    std::vector<ThreadStats> reader_stats;
    std::vector<ThreadStats> writer_stats;
    double duration_sec;

    double total_read_throughput() const
    {
        return std::accumulate(reader_stats.begin(), reader_stats.end(), 0.0,
                               [this](double sum, const ThreadStats &stats)
                               { return sum + stats.throughput(duration_sec); });
    }

    double total_write_throughput() const
    {
        return std::accumulate(writer_stats.begin(), writer_stats.end(), 0.0,
                               [this](double sum, const ThreadStats &stats)
                               { return sum + stats.throughput(duration_sec); });
    }

    double avg_read_latency() const
    {
        uint64_t total_ops = 0, total_latency = 0;
        for (const auto &stats : reader_stats)
        {
            total_ops += stats.operations;
            total_latency += stats.total_latency_ns;
        }
        return total_ops > 0 ? static_cast<double>(total_latency) / total_ops : 0;
    }

    std::vector<uint64_t> get_combined_read_latencies() const
    {
        std::vector<uint64_t> combined;
        size_t total_size = 0;
        for (const auto &stats : reader_stats)
        {
            total_size += stats.latencies.size();
        }
        combined.reserve(total_size);

        for (const auto &stats : reader_stats)
        {
            combined.insert(combined.end(), stats.latencies.begin(), stats.latencies.end());
        }
        std::sort(combined.begin(), combined.end());
        return combined;
    }

    double get_read_percentile(double p) const
    {
        auto combined = get_combined_read_latencies();
        if (combined.empty())
            return 0;

        size_t idx = static_cast<size_t>(p * (combined.size() - 1));
        return static_cast<double>(combined[idx]);
    }
};

void print_results(const TestResults &results)
{
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nOverall Results:" << std::endl
              << "===============" << std::endl
              << "Total Read Throughput:  " << results.total_read_throughput() << " ops/sec" << std::endl
              << "Total Write Throughput: " << results.total_write_throughput() << " ops/sec" << std::endl;

    std::cout << "\nRead Latency Statistics (ns):" << std::endl
              << "============================" << std::endl
              << "Average:     " << results.avg_read_latency() << std::endl
              << "50th %ile:   " << results.get_read_percentile(0.50) << std::endl
              << "75th %ile:   " << results.get_read_percentile(0.75) << std::endl
              << "90th %ile:   " << results.get_read_percentile(0.90) << std::endl
              << "95th %ile:   " << results.get_read_percentile(0.95) << std::endl
              << "99th %ile:   " << results.get_read_percentile(0.99) << std::endl
              << "99.9th %ile: " << results.get_read_percentile(0.999) << std::endl;

    // Per-thread statistics
    std::cout << "\nPer-reader Thread Stats:" << std::endl;
    for (size_t i = 0; i < results.reader_stats.size(); i++)
    {
        const auto &stats = results.reader_stats[i];
        std::cout << "Reader " << i << ": "
                  << stats.throughput(results.duration_sec) << " ops/sec, "
                  << "p50: " << stats.get_percentile(0.50) << " ns, "
                  << "p99: " << stats.get_percentile(0.99) << " ns" << std::endl;
    }

    std::cout << "\nPer-writer Thread Stats:" << std::endl;
    for (size_t i = 0; i < results.writer_stats.size(); i++)
    {
        const auto &stats = results.writer_stats[i];
        std::cout << "Writer " << i << ": "
                  << stats.throughput(results.duration_sec) << " ops/sec, "
                  << "p50: " << stats.get_percentile(0.50) << " ns, "
                  << "p99: " << stats.get_percentile(0.99) << " ns" << std::endl;
    }
}

template <typename SkipList>
class ConcurrentTest
{
private:
    SkipList &skiplist;
    std::atomic<bool> running{true};
    const size_t test_duration_sec;
    const size_t initial_size;
    const size_t num_readers;
    const size_t num_writers;
    std::unique_ptr<std::barrier<>> sync_point;

    void writer_thread(ThreadStats &stats)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1000000);

        sync_point->arrive_and_wait();

        while (running)
        {
            int key = dis(gen);
            int value = dis(gen);

            auto start = std::chrono::high_resolution_clock::now();
            skiplist.upsert(key, value);
            auto end = std::chrono::high_resolution_clock::now();

            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            stats.total_latency_ns += latency;
            stats.latencies.push_back(latency);
            stats.operations++;
        }
    }

    void reader_thread(ThreadStats &stats)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1000000);

        sync_point->arrive_and_wait();

        while (running)
        {
            int key = dis(gen);

            auto start = std::chrono::high_resolution_clock::now();
            auto result = skiplist.find(key);
            auto end = std::chrono::high_resolution_clock::now();

            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            stats.total_latency_ns += latency;
            stats.latencies.push_back(latency);
            stats.operations++;
        }
    }

public:
    ConcurrentTest(SkipList &sl, size_t duration_sec, size_t initial_data_size,
                   size_t readers, size_t writers)
        : skiplist(sl),
          test_duration_sec(duration_sec),
          initial_size(initial_data_size),
          num_readers(readers),
          num_writers(writers)
    {
        sync_point = std::make_unique<std::barrier<>>(readers + writers);
    }

    TestResults run()
    {
        std::cout << "Initializing skiplist with " << initial_size << " elements..." << std::endl;
        for (size_t i = 0; i < initial_size; i++)
        {
            skiplist.upsert(i, i);
        }

        std::cout << "\nStarting concurrent test with:" << std::endl
                  << "- " << num_writers << " writer thread(s)" << std::endl
                  << "- " << num_readers << " reader thread(s)" << std::endl
                  << "- " << test_duration_sec << " seconds duration" << std::endl;

        std::vector<ThreadStats> reader_stats(num_readers);
        std::vector<ThreadStats> writer_stats(num_writers);
        std::vector<std::thread> threads;

        // Start reader threads
        for (size_t i = 0; i < num_readers; i++)
        {
            threads.emplace_back([this, &stats = reader_stats[i]]
                                 { reader_thread(stats); });
        }

        // Start writer threads
        for (size_t i = 0; i < num_writers; i++)
        {
            threads.emplace_back([this, &stats = writer_stats[i]]
                                 { writer_thread(stats); });
        }

        std::this_thread::sleep_for(std::chrono::seconds(test_duration_sec));
        running = false;

        for (auto &thread : threads)
        {
            thread.join();
        }

        // Sort latencies for percentile calculation
        for (auto &stats : reader_stats)
            stats.finalize();
        for (auto &stats : writer_stats)
            stats.finalize();

        return TestResults{
            std::move(reader_stats),
            std::move(writer_stats),
            static_cast<double>(test_duration_sec)};
    }
};

template <typename T>
std::unique_ptr<T> create_skiplist(size_t height)
{
    return std::make_unique<T>(height);
}

int main(int argc, char **argv)
{
    const size_t initial_size = 100000;
    const size_t height = 22;
    const size_t test_duration_sec = 10;

    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0] << " <skiplist_type: 0=normal, 1=atomic> <num_readers> <num_writers>" << std::endl;
        return 1;
    }

    int skiplist_type = std::stoi(argv[1]);
    size_t num_readers = std::stoi(argv[2]);
    size_t num_writers = std::stoi(argv[3]);

    if (skiplist_type == 0)
    {
        throw std::runtime_error("Normal skiplist cannot handle concurrent operations");
    }
    else if (skiplist_type == 1)
    {
        auto skiplist = create_skiplist<SkipListAtomicSingleWriter<int, int>>(height);
        ConcurrentTest test(*skiplist, test_duration_sec, initial_size, num_readers, num_writers);
        auto results = test.run();
        print_results(results);
    }
    else if (skiplist_type == 2)
    {
        auto skiplist = create_skiplist<SkipListMutex<int, int>>(height);
        ConcurrentTest test(*skiplist, test_duration_sec, initial_size, num_readers, num_writers);
        auto results = test.run();
        print_results(results);
    }
    else
    {
        throw std::runtime_error("Invalid skiplist type");
    }

    return 0;
}
