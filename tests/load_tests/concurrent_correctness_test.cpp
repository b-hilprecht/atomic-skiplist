#include <thread>
#include <vector>
#include <random>
#include <atomic>
#include <cassert>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <barrier>
#include <memory>
#include "skiplist.hpp"
#include "skiplist_atomic_sw.hpp"
#include "skiplist_mutex.hpp"

static constexpr size_t MAX_VALUE = 1'000'000;
static constexpr size_t HEIGHT = 22;

template <typename T>
std::unique_ptr<T> create_skiplist(size_t height)
{
    return std::make_unique<T>(height);
}

template <typename SkipList>
class ConcurrentCorrectnessTest
{
private:
    struct ThreadStats
    {
        size_t writes{0};
        size_t reads{0};
        size_t validation_failures{0};
    };

    SkipList &skiplist;
    size_t num_writers;
    size_t num_readers;
    std::unique_ptr<std::barrier<>> sync_point;

    std::vector<int> generateThreadSequence(size_t thread_id)
    {
        std::vector<int> sequence;
        for (int i = thread_id + 1; i <= MAX_VALUE; i += num_writers)
        {
            sequence.push_back(i);
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(sequence.begin(), sequence.end(), gen);

        return sequence;
    }

    std::vector<int> generateReaderSequence()
    {
        std::vector<int> sequence;
        for (int i = 1; i <= MAX_VALUE; i++)
        {
            sequence.push_back(i);
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(sequence.begin(), sequence.end(), gen);

        return sequence;
    }

    void readerThread(size_t thread_id, ThreadStats &stats)
    {
        auto sequence = generateReaderSequence();

        sync_point->arrive_and_wait();

        for (int key : sequence)
        {
            auto result = skiplist.find(key);
            stats.reads++;

            // Value should either be null or equal to the key
            if (result && *result != key)
            {
                stats.validation_failures++;
                std::cerr << "Reader " << thread_id
                          << " validation failed - Key: " << key
                          << " Expected: " << key
                          << " Got: " << *result
                          << std::endl;
            }
        }
    }

    void writerThread(size_t thread_id, ThreadStats &stats)
    {
        auto sequence = generateThreadSequence(thread_id);
        size_t current_idx = 0;

        sync_point->arrive_and_wait();

        while (current_idx < sequence.size())
        {
            int key = sequence[current_idx];
            int value = key;

            skiplist.upsert(key, value);
            stats.writes++;

            if (current_idx > 0)
            {
                int prev_key = sequence[current_idx - 1];
                int expected_value = prev_key;

                auto result = skiplist.find(prev_key);
                stats.reads++;

                if (!result || *result != expected_value)
                {
                    stats.validation_failures++;
                    std::cerr << "Writer " << thread_id
                              << " validation failed - Key: " << prev_key
                              << " Expected: " << expected_value
                              << " Got: " << (result ? std::to_string(*result) : "null")
                              << std::endl;
                }
            }

            current_idx++;
        }
    }

public:
    ConcurrentCorrectnessTest(SkipList &sl, size_t writers, size_t readers)
        : skiplist(sl),
          num_writers(writers),
          num_readers(readers)
    {
        sync_point = std::make_unique<std::barrier<>>(writers + readers + 1);
    }

    void run()
    {
        std::vector<std::thread> threads;
        std::vector<ThreadStats> stats(num_writers + num_readers);

        std::cout << "Starting correctness test with:" << std::endl
                  << "- " << num_writers << " writers" << std::endl
                  << "- " << num_readers << " readers" << std::endl;

        // Launch regular writers
        for (size_t i = 0; i < num_writers; i++)
        {
            threads.emplace_back([this, i, &stat = stats[i]]()
                                 { writerThread(i, stat); });
        }

        // Launch readers
        for (size_t i = 0; i < num_readers; i++)
        {
            size_t thread_idx = num_writers + i;
            threads.emplace_back([this, i, &stat = stats[thread_idx]]()
                                 { readerThread(i, stat); });
        }

        sync_point->arrive_and_wait();

        for (auto &thread : threads)
        {
            thread.join();
        }

        // Print results
        size_t total_writes = 0, total_reads = 0, total_failures = 0;

        std::cout << "\nTest Results:" << std::endl;
        std::cout << "=============" << std::endl;

        // Regular writers stats
        for (size_t i = 0; i < num_writers; i++)
        {
            const auto &stat = stats[i];
            std::cout << "Writer " << i << ": "
                      << stat.writes << " writes, "
                      << stat.reads << " reads, "
                      << stat.validation_failures << " validation failures"
                      << std::endl;

            total_writes += stat.writes;
            total_reads += stat.reads;
            total_failures += stat.validation_failures;
        }

        // Readers stats
        std::cout << "\nReaders:" << std::endl;
        for (size_t i = 0; i < num_readers; i++)
        {
            const auto &stat = stats[num_writers + i];
            std::cout << "Reader " << i << ": "
                      << stat.reads << " reads, "
                      << stat.validation_failures << " validation failures"
                      << std::endl;

            total_reads += stat.reads;
            total_failures += stat.validation_failures;
        }

        std::cout << "\nOverall Results:" << std::endl
                  << "Total writes: " << total_writes << std::endl
                  << "Total reads: " << total_reads << std::endl
                  << "Total validation failures: " << total_failures << std::endl;

        if (total_failures > 0)
        {
            std::cerr << "\nWARNING: Test detected " << total_failures
                      << " validation failures!" << std::endl;
        }
        else
        {
            std::cout << "\nSUCCESS: No validation failures detected!" << std::endl;
        }
    }
};

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0]
                  << " <skiplist_type: 0=normal, 1=atomic_sw, 2=atomic_mw> "
                  << " <num_readers> <num_writers>" << std::endl;
        return 1;
    }

    int skiplist_type = std::stoi(argv[1]);
    size_t num_readers = std::stoul(argv[2]);
    size_t num_writers = std::stoul(argv[3]);

    if (skiplist_type == 0)
    {
        throw std::runtime_error("Normal skiplist cannot handle concurrent operations");
    }
    else if (skiplist_type == 1)
    {
        auto skiplist = create_skiplist<SkipListAtomicSingleWriter<int, int>>(HEIGHT);
        ConcurrentCorrectnessTest test(*skiplist, num_writers, num_readers);
        test.run();
    }
    else if (skiplist_type == 2)
    {
        auto skiplist = create_skiplist<SkipListMutex<int, int>>(HEIGHT);
        ConcurrentCorrectnessTest test(*skiplist, num_writers, num_readers);
        test.run();
    }
    else
    {
        throw std::runtime_error("Invalid skiplist type");
    }

    return 0;
}
