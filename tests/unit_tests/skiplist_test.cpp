#include <gtest/gtest.h>
#include "skiplist.hpp"
#include "skiplist_atomic_sw.hpp"
#include "skiplist_mutex.hpp"
#include <random>
#include <algorithm>
#include <memory>

template <typename T>
class SkipListTestFixture : public ::testing::Test
{
protected:
    static constexpr size_t TEST_HEIGHT = 5;
    std::unique_ptr<T> sl;

    void SetUp() override
    {
        sl = std::make_unique<T>(TEST_HEIGHT);
    }
};

using SkipListTypes = ::testing::Types<
    SkipList<int, int>,
    SkipListAtomicSingleWriter<int, int>,
    SkipListMutex<int, int>>;

TYPED_TEST_SUITE(SkipListTestFixture, SkipListTypes);

TYPED_TEST(SkipListTestFixture, InsertAndFind)
{
    this->sl->upsert(1, 10);
    this->sl->upsert(2, 20);
    this->sl->upsert(3, 30);

    EXPECT_EQ(*(this->sl->find(1)), 10);
    EXPECT_EQ(*(this->sl->find(2)), 20);
    EXPECT_EQ(*(this->sl->find(3)), 30);
}

TYPED_TEST(SkipListTestFixture, NotFound)
{
    this->sl->upsert(1, 10);
    this->sl->upsert(3, 30);

    EXPECT_FALSE(this->sl->find(2).has_value());
    EXPECT_FALSE(this->sl->find(4).has_value());
}

TYPED_TEST(SkipListTestFixture, Update)
{
    this->sl->upsert(1, 10);
    EXPECT_EQ(*(this->sl->find(1)), 10);

    this->sl->upsert(1, 20);
    EXPECT_EQ(*(this->sl->find(1)), 20);
}

TYPED_TEST(SkipListTestFixture, LargeSequentialInsert)
{
    for (int i = 0; i < 1000; i++)
    {
        this->sl->upsert(i, i * 2);
    }

    for (int i = 0; i < 1000; i++)
    {
        EXPECT_EQ(*(this->sl->find(i)), i * 2);
    }
}

TYPED_TEST(SkipListTestFixture, RandomInsert)
{
    std::vector<int> numbers(1000);
    std::iota(numbers.begin(), numbers.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(numbers.begin(), numbers.end(), g);

    for (int num : numbers)
    {
        this->sl->upsert(num, num * 2);
    }

    for (int i = 0; i < 1000; i++)
    {
        EXPECT_EQ(*(this->sl->find(i)), i * 2);
    }
}

TYPED_TEST(SkipListTestFixture, MultipleUpdates)
{
    for (int i = 0; i < 100; i++)
    {
        this->sl->upsert(i, i);
    }

    // Update all values
    for (int i = 0; i < 100; i++)
    {
        this->sl->upsert(i, i * 3);
    }

    // Verify updates
    for (int i = 0; i < 100; i++)
    {
        EXPECT_EQ(*(this->sl->find(i)), i * 3);
    }
}

TYPED_TEST(SkipListTestFixture, SparseInserts)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->sl->upsert(i, i);
    }

    for (int i = 0; i < 100; i++)
    {
        if (i % 10 == 0)
        {
            EXPECT_EQ(*(this->sl->find(i)), i);
        }
        else
        {
            EXPECT_FALSE(this->sl->find(i).has_value());
        }
    }
}

TYPED_TEST(SkipListTestFixture, NegativeKeys)
{
    this->sl->upsert(-1, 10);
    this->sl->upsert(-5, 50);
    this->sl->upsert(-10, 100);

    EXPECT_EQ(*(this->sl->find(-1)), 10);
    EXPECT_EQ(*(this->sl->find(-5)), 50);
    EXPECT_EQ(*(this->sl->find(-10)), 100);
    EXPECT_FALSE(this->sl->find(-2).has_value());
}

TYPED_TEST(SkipListTestFixture, MixedOperations)
{
    // Insert some initial values
    this->sl->upsert(1, 10);
    this->sl->upsert(3, 30);
    this->sl->upsert(5, 50);

    // Verify initial values
    EXPECT_EQ(*(this->sl->find(1)), 10);
    EXPECT_EQ(*(this->sl->find(3)), 30);
    EXPECT_EQ(*(this->sl->find(5)), 50);

    // Update existing values
    this->sl->upsert(1, 15);
    this->sl->upsert(3, 35);

    // Insert new values between existing ones
    this->sl->upsert(2, 20);
    this->sl->upsert(4, 40);

    // Verify all values after mixed operations
    EXPECT_EQ(*(this->sl->find(1)), 15);
    EXPECT_EQ(*(this->sl->find(2)), 20);
    EXPECT_EQ(*(this->sl->find(3)), 35);
    EXPECT_EQ(*(this->sl->find(4)), 40);
    EXPECT_EQ(*(this->sl->find(5)), 50);
}
