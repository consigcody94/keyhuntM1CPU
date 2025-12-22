/**
 * @file bsgs.h
 * @brief Modern BSGS (Baby Step Giant Step) algorithm abstraction
 *
 * Provides a clean, type-safe interface to the BSGS algorithm
 * with support for multiple backends (CPU, CUDA).
 */

#ifndef KEYHUNT_CORE_BSGS_H
#define KEYHUNT_CORE_BSGS_H

#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <optional>

#include "types.h"
#include "config.h"
#include "thread_pool.h"
#include "memory.h"
#include "logger.h"

namespace keyhunt {
namespace core {

/**
 * @brief BSGS search result
 */
struct BSGSResult {
    bool found;
    PrivateKey private_key;
    Hash160 target_hash;
    std::string address;
    std::chrono::steady_clock::time_point found_at;

    BSGSResult() : found(false) {}
};

/**
 * @brief BSGS search progress information
 */
struct BSGSProgress {
    uint64_t keys_checked;
    uint64_t keys_per_second;
    double progress_percent;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
    UInt256 current_position;
    size_t results_found;

    std::string format_speed() const {
        if (keys_per_second >= 1000000000000ULL) {
            return std::to_string(keys_per_second / 1000000000000ULL) + " Tkeys/s";
        } else if (keys_per_second >= 1000000000ULL) {
            return std::to_string(keys_per_second / 1000000000ULL) + " Gkeys/s";
        } else if (keys_per_second >= 1000000ULL) {
            return std::to_string(keys_per_second / 1000000ULL) + " Mkeys/s";
        } else if (keys_per_second >= 1000ULL) {
            return std::to_string(keys_per_second / 1000ULL) + " Kkeys/s";
        }
        return std::to_string(keys_per_second) + " keys/s";
    }

    std::string format_elapsed() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        int hours = elapsed / 3600;
        int minutes = (elapsed % 3600) / 60;
        int seconds = elapsed % 60;

        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
        return std::string(buf);
    }
};

/**
 * @brief Callback for progress updates
 */
using ProgressCallback = std::function<void(const BSGSProgress&)>;

/**
 * @brief Callback for found keys
 */
using ResultCallback = std::function<void(const BSGSResult&)>;

/**
 * @brief BSGS algorithm parameters
 */
struct BSGSParams {
    // Search range
    KeyRange range;

    // Algorithm parameters
    uint64_t m;           // Baby step table size (sqrt of range)
    int k_factor;         // Space-time trade-off factor

    // Threading
    int num_threads;

    // Search direction
    BSGSMode mode;

    // Key type
    KeySearchType key_type;

    // Bloom filter settings
    int bloom_bits_per_element;
    int bloom_hash_functions;

    // Memory limits
    size_t max_memory_mb;

    BSGSParams()
        : m(4194304)  // 2^22
        , k_factor(1)
        , num_threads(0)  // Auto
        , mode(BSGSMode::SEQUENTIAL)
        , key_type(KeySearchType::COMPRESSED)
        , bloom_bits_per_element(14)
        , bloom_hash_functions(10)
        , max_memory_mb(0)  // No limit
    {}
};

/**
 * @brief Abstract BSGS engine interface
 */
class IBSGSEngine {
public:
    virtual ~IBSGSEngine() = default;

    /**
     * @brief Initialize the engine with targets
     */
    virtual void initialize(const std::vector<Hash160>& targets) = 0;

    /**
     * @brief Set search parameters
     */
    virtual void set_params(const BSGSParams& params) = 0;

    /**
     * @brief Start the search
     */
    virtual void start() = 0;

    /**
     * @brief Stop the search
     */
    virtual void stop() = 0;

    /**
     * @brief Pause the search
     */
    virtual void pause() = 0;

    /**
     * @brief Resume the search
     */
    virtual void resume() = 0;

    /**
     * @brief Check if search is running
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Get current progress
     */
    virtual BSGSProgress get_progress() const = 0;

    /**
     * @brief Get results found so far
     */
    virtual std::vector<BSGSResult> get_results() const = 0;

    /**
     * @brief Set progress callback
     */
    virtual void set_progress_callback(ProgressCallback callback) = 0;

    /**
     * @brief Set result callback
     */
    virtual void set_result_callback(ResultCallback callback) = 0;

    /**
     * @brief Save checkpoint
     */
    virtual bool save_checkpoint(const std::string& filename) = 0;

    /**
     * @brief Load checkpoint
     */
    virtual bool load_checkpoint(const std::string& filename) = 0;
};

/**
 * @brief CPU-based BSGS engine
 */
class CPUBSGSEngine : public IBSGSEngine {
public:
    CPUBSGSEngine();
    ~CPUBSGSEngine() override;

    void initialize(const std::vector<Hash160>& targets) override;
    void set_params(const BSGSParams& params) override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    bool is_running() const override;
    BSGSProgress get_progress() const override;
    std::vector<BSGSResult> get_results() const override;
    void set_progress_callback(ProgressCallback callback) override;
    void set_result_callback(ResultCallback callback) override;
    bool save_checkpoint(const std::string& filename) override;
    bool load_checkpoint(const std::string& filename) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#ifdef CUDA_ENABLED
/**
 * @brief CUDA GPU-based BSGS engine
 */
class CUDABSGSEngine : public IBSGSEngine {
public:
    explicit CUDABSGSEngine(int device_id = 0);
    ~CUDABSGSEngine() override;

    void initialize(const std::vector<Hash160>& targets) override;
    void set_params(const BSGSParams& params) override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    bool is_running() const override;
    BSGSProgress get_progress() const override;
    std::vector<BSGSResult> get_results() const override;
    void set_progress_callback(ProgressCallback callback) override;
    void set_result_callback(ResultCallback callback) override;
    bool save_checkpoint(const std::string& filename) override;
    bool load_checkpoint(const std::string& filename) override;

    // CUDA-specific
    static int get_device_count();
    static std::string get_device_name(int device_id);
    static size_t get_device_memory(int device_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif

/**
 * @brief Factory to create appropriate BSGS engine
 */
class BSGSEngineFactory {
public:
    enum class EngineType {
        AUTO,   // Automatically select best available
        CPU,
        CUDA
    };

    static std::unique_ptr<IBSGSEngine> create(EngineType type = EngineType::AUTO,
                                                int device_id = 0) {
        switch (type) {
            case EngineType::CPU:
                return std::make_unique<CPUBSGSEngine>();

#ifdef CUDA_ENABLED
            case EngineType::CUDA:
                return std::make_unique<CUDABSGSEngine>(device_id);

            case EngineType::AUTO:
                if (CUDABSGSEngine::get_device_count() > 0) {
                    return std::make_unique<CUDABSGSEngine>(device_id);
                }
                return std::make_unique<CPUBSGSEngine>();
#else
            case EngineType::CUDA:
                throw RuntimeException("CUDA support not compiled");

            case EngineType::AUTO:
                return std::make_unique<CPUBSGSEngine>();
#endif
        }

        return std::make_unique<CPUBSGSEngine>();
    }
};

/**
 * @brief High-level BSGS search coordinator
 *
 * Manages multiple engines, handles checkpointing, notifications, etc.
 */
class BSGSSearch {
public:
    BSGSSearch() = default;

    /**
     * @brief Add target addresses to search for
     */
    void add_target(const Hash160& hash) {
        targets_.push_back(hash);
    }

    /**
     * @brief Add target from address string
     */
    void add_target(const std::string& address);

    /**
     * @brief Load targets from file
     */
    size_t load_targets_from_file(const std::string& filename);

    /**
     * @brief Set search parameters
     */
    void set_params(const BSGSParams& params) {
        params_ = params;
    }

    /**
     * @brief Set progress callback
     */
    void on_progress(ProgressCallback callback) {
        progress_callback_ = std::move(callback);
    }

    /**
     * @brief Set result callback
     */
    void on_result(ResultCallback callback) {
        result_callback_ = std::move(callback);
    }

    /**
     * @brief Run the search (blocking)
     */
    std::vector<BSGSResult> run();

    /**
     * @brief Start async search
     */
    void start_async();

    /**
     * @brief Stop the search
     */
    void stop();

    /**
     * @brief Get current progress
     */
    BSGSProgress get_progress() const;

private:
    std::vector<Hash160> targets_;
    BSGSParams params_;
    std::unique_ptr<IBSGSEngine> engine_;
    ProgressCallback progress_callback_;
    ResultCallback result_callback_;
    std::atomic<bool> running_{false};
};

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_BSGS_H
