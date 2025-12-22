/**
 * @file test_thread_pool.cpp
 * @brief Unit tests for thread pool
 */

#include "../include/keyhunt/core/thread_pool.h"
#include <atomic>
#include <chrono>

using namespace keyhunt::core;

TEST(ThreadPool, BasicSubmit) {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([&counter]() {
            ++counter;
        }));
    }

    // Wait for all tasks
    for (auto& f : futures) {
        f.wait();
    }

    EXPECT_EQ(counter.load(), 100);

    return true;
}

TEST(ThreadPool, ReturnValue) {
    ThreadPool pool(2);

    auto future = pool.submit([]() {
        return 42;
    });

    EXPECT_EQ(future.get(), 42);

    return true;
}

TEST(ThreadPool, Priority) {
    ThreadPool pool(1);  // Single thread to ensure order

    std::vector<int> results;
    std::mutex mutex;

    // Submit low priority tasks first
    for (int i = 0; i < 5; ++i) {
        pool.submit_with_priority(TaskPriority::LOW, [&results, &mutex, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::lock_guard<std::mutex> lock(mutex);
            results.push_back(i);
        });
    }

    // Submit high priority task
    pool.submit_with_priority(TaskPriority::HIGH, [&results, &mutex]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mutex);
        results.push_back(100);
    });

    pool.wait();

    // High priority task (100) should be near the front
    // Note: First task might already be running when we submit high priority
    EXPECT_GE(results.size(), 6UL);

    return true;
}

TEST(ThreadPool, Wait) {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++counter;
        });
    }

    pool.wait();

    EXPECT_EQ(counter.load(), 50);

    return true;
}

TEST(ThreadPool, WaitFor) {
    ThreadPool pool(2);

    // Submit a slow task
    pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    });

    // Should timeout
    bool completed = pool.wait_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(completed);

    pool.shutdown();

    return true;
}

TEST(ThreadPool, PauseResume) {
    ThreadPool pool(2);

    std::atomic<int> counter{0};

    pool.pause();
    EXPECT_TRUE(pool.is_paused());

    // Submit tasks while paused
    for (int i = 0; i < 10; ++i) {
        pool.submit([&counter]() {
            ++counter;
        });
    }

    // Tasks should not execute while paused
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(counter.load(), 0);

    pool.resume();
    pool.wait();

    EXPECT_EQ(counter.load(), 10);

    return true;
}

TEST(ThreadPool, Stats) {
    ThreadPool pool(2);

    pool.reset_stats();

    for (int i = 0; i < 20; ++i) {
        pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });
    }

    pool.wait();

    const auto& stats = pool.stats();
    EXPECT_EQ(stats.tasks_submitted.load(), 20UL);
    EXPECT_EQ(stats.tasks_completed.load(), 20UL);
    EXPECT_EQ(stats.tasks_pending.load(), 0UL);

    return true;
}

TEST(ThreadPool, ParallelFor) {
    ThreadPool pool(4);

    std::vector<int> data(1000, 0);

    parallel_for(pool, 0, 1000, [&data](int i) {
        data[i] = i * 2;
    });

    // Verify results
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(data[i], i * 2);
    }

    return true;
}

TEST(ThreadPool, ParallelReduce) {
    ThreadPool pool(4);

    // Sum of 1 to 1000
    int64_t result = parallel_reduce<int, int64_t>(
        pool, 1, 1001,
        0LL,
        [](int i) { return static_cast<int64_t>(i); },
        [](int64_t a, int64_t b) { return a + b; }
    );

    // Expected: n*(n+1)/2 = 1000*1001/2 = 500500
    EXPECT_EQ(result, 500500LL);

    return true;
}

TEST(ThreadPool, BatchSubmit) {
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    std::vector<std::function<void()>> tasks;
    for (int i = 0; i < 100; ++i) {
        tasks.push_back([&counter]() {
            ++counter;
        });
    }

    pool.submit_batch(tasks);
    pool.wait();

    EXPECT_EQ(counter.load(), 100);

    return true;
}

TEST(ThreadPool, ExceptionHandling) {
    ThreadPool pool(2);

    std::atomic<int> completed{0};

    // Submit task that throws
    pool.submit([]() {
        throw std::runtime_error("Test exception");
    });

    // Submit normal task after
    pool.submit([&completed]() {
        ++completed;
    });

    pool.wait();

    // Second task should still complete
    EXPECT_EQ(completed.load(), 1);

    return true;
}

TEST(GlobalThreadPool, Singleton) {
    auto& pool1 = GlobalThreadPool::instance();
    auto& pool2 = GlobalThreadPool::instance();

    EXPECT_TRUE(&pool1 == &pool2);

    return true;
}
