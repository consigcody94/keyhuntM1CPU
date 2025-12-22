/**
 * @file test_memory.cpp
 * @brief Unit tests for memory management utilities
 */

#include "../include/keyhunt/core/memory.h"
#include <thread>
#include <atomic>

using namespace keyhunt::core;

// AlignedAllocator Tests

TEST(AlignedAllocator, BasicAllocation) {
    AlignedAllocator<int, 64> alloc;

    int* ptr = alloc.allocate(100);
    EXPECT_TRUE(ptr != nullptr);

    // Check alignment
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0UL);

    alloc.deallocate(ptr, 100);

    return true;
}

TEST(AlignedVector, BasicOperations) {
    AlignedVector<double> vec;

    for (int i = 0; i < 1000; ++i) {
        vec.push_back(i * 1.5);
    }

    EXPECT_EQ(vec.size(), 1000UL);

    // Check alignment of data
    EXPECT_EQ(reinterpret_cast<uintptr_t>(vec.data()) % 64, 0UL);

    // Check values
    EXPECT_NEAR(vec[0], 0.0, 0.001);
    EXPECT_NEAR(vec[100], 150.0, 0.001);

    return true;
}

// SecureBuffer Tests

TEST(SecureBuffer, BasicOperations) {
    SecureBuffer<uint8_t> buffer(1024);

    EXPECT_EQ(buffer.size(), 1024UL);
    EXPECT_TRUE(buffer.data() != nullptr);

    // Should be zero-initialized
    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i], 0);
    }

    // Write some data
    buffer[0] = 0xAB;
    buffer[100] = 0xCD;

    EXPECT_EQ(buffer[0], 0xAB);
    EXPECT_EQ(buffer[100], 0xCD);

    return true;
}

TEST(SecureBuffer, SecureZero) {
    SecureBuffer<uint8_t> buffer(256);

    // Fill with data
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<uint8_t>(i);
    }

    // Secure zero
    buffer.secure_zero();

    // All bytes should be zero
    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i], 0);
    }

    return true;
}

TEST(SecureBuffer, MoveSemantics) {
    SecureBuffer<uint8_t> buffer1(100);
    buffer1[0] = 42;

    SecureBuffer<uint8_t> buffer2 = std::move(buffer1);

    EXPECT_EQ(buffer2[0], 42);
    EXPECT_EQ(buffer2.size(), 100UL);

    return true;
}

// MemoryPool Tests

TEST(MemoryPool, BasicAllocation) {
    MemoryPool<int, 1024> pool;

    std::vector<int*> ptrs;
    for (int i = 0; i < 100; ++i) {
        int* p = pool.allocate();
        EXPECT_TRUE(p != nullptr);
        *p = i;
        ptrs.push_back(p);
    }

    // Verify values
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(*ptrs[i], i);
    }

    return true;
}

TEST(MemoryPool, ThreadSafety) {
    MemoryPool<int, 1024> pool;
    std::atomic<int> count{0};

    auto worker = [&pool, &count]() {
        for (int i = 0; i < 1000; ++i) {
            int* p = pool.allocate();
            if (p) {
                *p = i;
                ++count;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(count.load(), 4000);

    return true;
}

// FileHandle Tests

TEST(FileHandle, BasicOperations) {
    // Create a temporary file
    const char* filename = "/tmp/keyhunt_test_file.txt";

    {
        FileHandle file(filename, "w");
        EXPECT_TRUE(file.is_open());
        fprintf(file.get(), "Hello, World!\n");
    }

    // File should be closed now
    {
        FileHandle file(filename, "r");
        EXPECT_TRUE(file.is_open());

        char buffer[100];
        EXPECT_TRUE(fgets(buffer, sizeof(buffer), file.get()) != nullptr);
        EXPECT_EQ(strcmp(buffer, "Hello, World!\n"), 0);
    }

    // Clean up
    std::remove(filename);

    return true;
}

TEST(FileHandle, MoveSemantics) {
    const char* filename = "/tmp/keyhunt_test_move.txt";

    FileHandle file1(filename, "w");
    fprintf(file1.get(), "test");

    FileHandle file2 = std::move(file1);
    EXPECT_TRUE(file2.is_open());

    file2.close();
    std::remove(filename);

    return true;
}

// MemoryStats Tests

TEST(MemoryStats, BasicTracking) {
    auto& stats = MemoryStats::instance();

    size_t initial = stats.current_allocated();

    stats.record_allocation(1024);
    EXPECT_EQ(stats.current_allocated(), initial + 1024);

    stats.record_allocation(512);
    EXPECT_EQ(stats.current_allocated(), initial + 1536);

    stats.record_deallocation(512);
    EXPECT_EQ(stats.current_allocated(), initial + 1024);

    stats.record_deallocation(1024);
    EXPECT_EQ(stats.current_allocated(), initial);

    return true;
}

TEST(MemoryStats, PeakTracking) {
    auto& stats = MemoryStats::instance();

    size_t initial_peak = stats.peak_allocated();

    stats.record_allocation(10000);
    size_t new_peak = stats.peak_allocated();
    EXPECT_GE(new_peak, initial_peak);

    stats.record_deallocation(10000);
    // Peak should remain at high water mark
    EXPECT_EQ(stats.peak_allocated(), new_peak);

    return true;
}
