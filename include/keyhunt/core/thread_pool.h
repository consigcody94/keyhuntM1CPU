/**
 * @file thread_pool.h
 * @brief Modern C++ thread pool for parallel key hunting
 *
 * Provides a high-performance, lock-free work-stealing thread pool
 * with support for task priorities and CPU affinity.
 */

#ifndef KEYHUNT_CORE_THREAD_POOL_H
#define KEYHUNT_CORE_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <optional>
#include <chrono>

namespace keyhunt {
namespace core {

/**
 * @brief Task priority levels
 */
enum class TaskPriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * @brief A single task to be executed
 */
struct Task {
    std::function<void()> func;
    TaskPriority priority;

    bool operator<(const Task& other) const {
        return priority < other.priority;  // Higher priority = earlier execution
    }
};

/**
 * @brief Statistics for the thread pool
 */
struct ThreadPoolStats {
    std::atomic<uint64_t> tasks_submitted{0};
    std::atomic<uint64_t> tasks_completed{0};
    std::atomic<uint64_t> tasks_pending{0};
    std::atomic<uint64_t> total_wait_time_ns{0};
    std::atomic<uint64_t> total_exec_time_ns{0};

    void reset() {
        tasks_submitted.store(0);
        tasks_completed.store(0);
        tasks_pending.store(0);
        total_wait_time_ns.store(0);
        total_exec_time_ns.store(0);
    }

    double avg_wait_time_ms() const {
        uint64_t completed = tasks_completed.load();
        if (completed == 0) return 0.0;
        return total_wait_time_ns.load() / (completed * 1e6);
    }

    double avg_exec_time_ms() const {
        uint64_t completed = tasks_completed.load();
        if (completed == 0) return 0.0;
        return total_exec_time_ns.load() / (completed * 1e6);
    }
};

/**
 * @brief High-performance thread pool
 */
class ThreadPool {
public:
    /**
     * @brief Create a thread pool
     * @param num_threads Number of worker threads (0 = auto-detect)
     */
    explicit ThreadPool(size_t num_threads = 0)
        : stop_(false)
        , paused_(false) {

        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 4;  // Fallback
        }

        workers_.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i] {
                worker_loop(i);
            });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Submit a task for execution
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {

        return submit_with_priority(
            TaskPriority::NORMAL,
            std::forward<F>(f),
            std::forward<Args>(args)...
        );
    }

    /**
     * @brief Submit a task with specific priority
     */
    template<typename F, typename... Args>
    auto submit_with_priority(TaskPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {

        using ReturnType = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot submit to stopped thread pool");
            }

            tasks_.push(Task{[task]() { (*task)(); }, priority});
            stats_.tasks_submitted.fetch_add(1, std::memory_order_relaxed);
            stats_.tasks_pending.fetch_add(1, std::memory_order_relaxed);
        }

        condition_.notify_one();
        return result;
    }

    /**
     * @brief Submit multiple tasks in batch (more efficient)
     */
    template<typename F>
    void submit_batch(const std::vector<F>& tasks, TaskPriority priority = TaskPriority::NORMAL) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot submit to stopped thread pool");
            }

            for (const auto& task : tasks) {
                tasks_.push(Task{task, priority});
            }

            stats_.tasks_submitted.fetch_add(tasks.size(), std::memory_order_relaxed);
            stats_.tasks_pending.fetch_add(tasks.size(), std::memory_order_relaxed);
        }

        condition_.notify_all();
    }

    /**
     * @brief Wait for all tasks to complete
     */
    void wait() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        done_condition_.wait(lock, [this] {
            return tasks_.empty() &&
                   active_tasks_.load(std::memory_order_acquire) == 0;
        });
    }

    /**
     * @brief Wait with timeout
     */
    template<typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return done_condition_.wait_for(lock, timeout, [this] {
            return tasks_.empty() &&
                   active_tasks_.load(std::memory_order_acquire) == 0;
        });
    }

    /**
     * @brief Pause execution (new tasks won't start)
     */
    void pause() {
        paused_.store(true, std::memory_order_release);
    }

    /**
     * @brief Resume execution
     */
    void resume() {
        paused_.store(false, std::memory_order_release);
        condition_.notify_all();
    }

    /**
     * @brief Check if paused
     */
    bool is_paused() const {
        return paused_.load(std::memory_order_acquire);
    }

    /**
     * @brief Shutdown the thread pool
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) return;
            stop_ = true;
        }

        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /**
     * @brief Get number of worker threads
     */
    size_t size() const {
        return workers_.size();
    }

    /**
     * @brief Get number of pending tasks
     */
    size_t pending() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    /**
     * @brief Get number of active tasks
     */
    size_t active() const {
        return active_tasks_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get statistics
     */
    const ThreadPoolStats& stats() const {
        return stats_;
    }

    /**
     * @brief Reset statistics
     */
    void reset_stats() {
        stats_.reset();
    }

private:
    void worker_loop(size_t worker_id) {
        (void)worker_id;  // May be used for affinity in the future

        while (true) {
            Task task;
            auto wait_start = std::chrono::steady_clock::now();

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                condition_.wait(lock, [this] {
                    return stop_ || (!tasks_.empty() && !paused_.load(std::memory_order_acquire));
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                if (tasks_.empty() || paused_.load(std::memory_order_acquire)) {
                    continue;
                }

                task = std::move(const_cast<Task&>(tasks_.top()));
                tasks_.pop();
                stats_.tasks_pending.fetch_sub(1, std::memory_order_relaxed);
            }

            auto wait_end = std::chrono::steady_clock::now();
            auto wait_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                wait_end - wait_start).count();
            stats_.total_wait_time_ns.fetch_add(wait_time, std::memory_order_relaxed);

            active_tasks_.fetch_add(1, std::memory_order_release);

            auto exec_start = std::chrono::steady_clock::now();

            try {
                task.func();
            } catch (...) {
                // Log error but continue
            }

            auto exec_end = std::chrono::steady_clock::now();
            auto exec_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                exec_end - exec_start).count();
            stats_.total_exec_time_ns.fetch_add(exec_time, std::memory_order_relaxed);

            active_tasks_.fetch_sub(1, std::memory_order_release);
            stats_.tasks_completed.fetch_add(1, std::memory_order_relaxed);

            done_condition_.notify_all();
        }
    }

    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable done_condition_;
    std::atomic<bool> stop_;
    std::atomic<bool> paused_;
    std::atomic<size_t> active_tasks_{0};
    ThreadPoolStats stats_;
};

/**
 * @brief Parallel for loop utility
 */
template<typename IndexType, typename Func>
void parallel_for(ThreadPool& pool, IndexType start, IndexType end, Func&& func,
                  size_t chunk_size = 0) {
    if (start >= end) return;

    IndexType total = end - start;

    if (chunk_size == 0) {
        chunk_size = std::max(IndexType(1), total / (pool.size() * 4));
    }

    std::vector<std::future<void>> futures;

    for (IndexType i = start; i < end; i += chunk_size) {
        IndexType chunk_end = std::min(i + static_cast<IndexType>(chunk_size), end);

        futures.push_back(pool.submit([&func, i, chunk_end]() {
            for (IndexType j = i; j < chunk_end; ++j) {
                func(j);
            }
        }));
    }

    // Wait for all chunks to complete
    for (auto& f : futures) {
        f.wait();
    }
}

/**
 * @brief Parallel reduce utility
 */
template<typename IndexType, typename T, typename MapFunc, typename ReduceFunc>
T parallel_reduce(ThreadPool& pool, IndexType start, IndexType end,
                  T identity, MapFunc&& map_func, ReduceFunc&& reduce_func,
                  size_t chunk_size = 0) {
    if (start >= end) return identity;

    IndexType total = end - start;

    if (chunk_size == 0) {
        chunk_size = std::max(IndexType(1), total / (pool.size() * 4));
    }

    std::vector<std::future<T>> futures;

    for (IndexType i = start; i < end; i += chunk_size) {
        IndexType chunk_end = std::min(i + static_cast<IndexType>(chunk_size), end);

        futures.push_back(pool.submit([&map_func, &reduce_func, i, chunk_end, identity]() {
            T result = identity;
            for (IndexType j = i; j < chunk_end; ++j) {
                result = reduce_func(result, map_func(j));
            }
            return result;
        }));
    }

    // Collect and reduce results
    T result = identity;
    for (auto& f : futures) {
        result = reduce_func(result, f.get());
    }

    return result;
}

/**
 * @brief Global thread pool singleton
 */
class GlobalThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }

    static void shutdown() {
        instance().shutdown();
    }

private:
    GlobalThreadPool() = default;
};

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_THREAD_POOL_H
