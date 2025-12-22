/**
 * @file distributed.h
 * @brief Multi-GPU and distributed computing coordination
 *
 * Provides abstractions for coordinating multiple GPUs,
 * distributed workers, and work partitioning.
 */

#ifndef KEYHUNT_CORE_DISTRIBUTED_H
#define KEYHUNT_CORE_DISTRIBUTED_H

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <optional>

#include "types.h"
#include "bsgs.h"
#include "config.h"
#include "logger.h"

namespace keyhunt {
namespace core {

/**
 * @brief Work unit for distributed processing
 */
struct WorkUnit {
    uint64_t id;
    KeyRange range;
    std::string assigned_worker;
    std::chrono::steady_clock::time_point assigned_at;
    std::chrono::steady_clock::time_point completed_at;
    bool completed = false;
    std::optional<BSGSResult> result;

    std::string to_string() const {
        return "WorkUnit{id=" + std::to_string(id) +
               ", range=" + range.start.to_hex() + ":" + range.end.to_hex() +
               ", completed=" + (completed ? "true" : "false") + "}";
    }
};

/**
 * @brief Worker status information
 */
struct WorkerStatus {
    std::string id;
    std::string hostname;
    std::string device_info;
    bool connected = false;
    bool busy = false;
    uint64_t work_units_completed = 0;
    uint64_t keys_per_second = 0;
    std::chrono::steady_clock::time_point last_heartbeat;

    std::chrono::seconds time_since_heartbeat() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_heartbeat);
    }
};

/**
 * @brief Work coordinator for multi-GPU and distributed processing
 */
class WorkCoordinator {
public:
    WorkCoordinator() = default;
    ~WorkCoordinator();

    // Non-copyable
    WorkCoordinator(const WorkCoordinator&) = delete;
    WorkCoordinator& operator=(const WorkCoordinator&) = delete;

    /**
     * @brief Initialize with a search range
     */
    void initialize(const KeyRange& range, size_t work_unit_size = 1ULL << 40);

    /**
     * @brief Start coordination
     */
    void start();

    /**
     * @brief Stop coordination
     */
    void stop();

    /**
     * @brief Register a worker
     */
    void register_worker(const std::string& worker_id,
                         const std::string& hostname,
                         const std::string& device_info);

    /**
     * @brief Unregister a worker
     */
    void unregister_worker(const std::string& worker_id);

    /**
     * @brief Get next work unit for a worker
     */
    std::optional<WorkUnit> get_next_work(const std::string& worker_id);

    /**
     * @brief Report work completion
     */
    void report_completion(uint64_t work_id, const std::optional<BSGSResult>& result);

    /**
     * @brief Report worker heartbeat
     */
    void heartbeat(const std::string& worker_id, uint64_t keys_per_second);

    /**
     * @brief Get overall progress
     */
    double get_progress() const;

    /**
     * @brief Get all results found
     */
    std::vector<BSGSResult> get_results() const;

    /**
     * @brief Get worker statuses
     */
    std::vector<WorkerStatus> get_workers() const;

    /**
     * @brief Set result callback
     */
    void on_result(std::function<void(const BSGSResult&)> callback) {
        result_callback_ = std::move(callback);
    }

    /**
     * @brief Get total keys per second across all workers
     */
    uint64_t get_total_kps() const;

    /**
     * @brief Get number of pending work units
     */
    size_t pending_work_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_work_.size();
    }

    /**
     * @brief Get number of in-progress work units
     */
    size_t in_progress_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return in_progress_.size();
    }

private:
    void generate_work_units();
    void check_timeouts();
    void reassign_timeout_work();

    mutable std::mutex mutex_;
    std::condition_variable work_available_;

    KeyRange total_range_;
    size_t work_unit_size_ = 0;

    std::queue<WorkUnit> pending_work_;
    std::map<uint64_t, WorkUnit> in_progress_;
    std::vector<WorkUnit> completed_work_;

    std::map<std::string, WorkerStatus> workers_;

    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> timeout_thread_;

    std::function<void(const BSGSResult&)> result_callback_;

    uint64_t next_work_id_ = 1;
    std::chrono::seconds work_timeout_{300};  // 5 minutes
};

/**
 * @brief Multi-GPU coordinator for local systems
 */
class MultiGPUCoordinator {
public:
    explicit MultiGPUCoordinator(int num_gpus = -1);  // -1 = auto-detect
    ~MultiGPUCoordinator();

    /**
     * @brief Initialize all GPUs
     */
    void initialize(const std::vector<Hash160>& targets, const BSGSParams& params);

    /**
     * @brief Start search on all GPUs
     */
    void start(const KeyRange& range);

    /**
     * @brief Stop all GPUs
     */
    void stop();

    /**
     * @brief Get combined progress
     */
    BSGSProgress get_progress() const;

    /**
     * @brief Get results from all GPUs
     */
    std::vector<BSGSResult> get_results() const;

    /**
     * @brief Set result callback
     */
    void on_result(ResultCallback callback);

    /**
     * @brief Get number of GPUs
     */
    int gpu_count() const { return static_cast<int>(gpu_engines_.size()); }

    /**
     * @brief Get GPU info
     */
    struct GPUInfo {
        int device_id;
        std::string name;
        size_t memory_total;
        size_t memory_free;
        int compute_capability;
    };

    std::vector<GPUInfo> get_gpu_info() const;

private:
    void worker_thread(int gpu_id, const KeyRange& range);

    std::vector<std::unique_ptr<IBSGSEngine>> gpu_engines_;
    std::vector<std::unique_ptr<std::thread>> gpu_threads_;
    std::atomic<bool> running_{false};
    ResultCallback result_callback_;
    mutable std::mutex results_mutex_;
    std::vector<BSGSResult> all_results_;
};

/**
 * @brief Remote worker client for distributed search
 */
class DistributedWorker {
public:
    DistributedWorker(const std::string& coordinator_host, uint16_t port);
    ~DistributedWorker();

    /**
     * @brief Connect to coordinator
     */
    bool connect();

    /**
     * @brief Disconnect from coordinator
     */
    void disconnect();

    /**
     * @brief Run the worker loop
     */
    void run();

    /**
     * @brief Stop the worker
     */
    void stop();

    /**
     * @brief Set the BSGS engine to use
     */
    void set_engine(std::unique_ptr<IBSGSEngine> engine) {
        engine_ = std::move(engine);
    }

    /**
     * @brief Get worker ID
     */
    const std::string& worker_id() const { return worker_id_; }

private:
    void heartbeat_loop();
    void process_work(const WorkUnit& work);

    std::string coordinator_host_;
    uint16_t coordinator_port_;
    std::string worker_id_;
    std::unique_ptr<IBSGSEngine> engine_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> heartbeat_thread_;
};

/**
 * @brief Work range partitioner
 */
class RangePartitioner {
public:
    /**
     * @brief Split a range into n equal parts
     */
    static std::vector<KeyRange> split_equal(const KeyRange& range, size_t n);

    /**
     * @brief Split a range into chunks of specified size
     */
    static std::vector<KeyRange> split_by_size(const KeyRange& range, const UInt256& chunk_size);

    /**
     * @brief Split for optimal GPU utilization
     *
     * Takes into account different GPU capabilities
     */
    static std::vector<KeyRange> split_for_gpus(
        const KeyRange& range,
        const std::vector<std::pair<int, double>>& gpu_weights);  // (gpu_id, relative_speed)

    /**
     * @brief Estimate optimal chunk size for given number of workers
     */
    static UInt256 optimal_chunk_size(const KeyRange& range,
                                       size_t num_workers,
                                       std::chrono::seconds target_chunk_time);
};

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_DISTRIBUTED_H
