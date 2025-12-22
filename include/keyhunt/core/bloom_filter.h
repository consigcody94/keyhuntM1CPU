/**
 * @file bloom_filter.h
 * @brief Modern C++ bloom filter implementation
 *
 * High-performance bloom filter with cascading support,
 * persistence, and thread-safety.
 */

#ifndef KEYHUNT_CORE_BLOOM_FILTER_H
#define KEYHUNT_CORE_BLOOM_FILTER_H

#include <vector>
#include <memory>
#include <cstdint>
#include <cmath>
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

#include "memory.h"
#include "error.h"

namespace keyhunt {
namespace core {

/**
 * @brief Bloom filter statistics
 */
struct BloomFilterStats {
    size_t bits;
    size_t hash_functions;
    size_t items_added;
    size_t memory_bytes;
    double expected_fp_rate;
    std::atomic<uint64_t> queries{0};
    std::atomic<uint64_t> positives{0};

    double actual_positive_rate() const {
        uint64_t q = queries.load();
        if (q == 0) return 0.0;
        return static_cast<double>(positives.load()) / q;
    }
};

/**
 * @brief High-performance bloom filter
 */
class BloomFilter {
public:
    /**
     * @brief Create bloom filter with specific parameters
     * @param expected_items Expected number of items
     * @param fp_rate Target false positive rate (e.g., 0.001 for 0.1%)
     */
    BloomFilter(size_t expected_items, double fp_rate = 0.001);

    /**
     * @brief Create bloom filter with explicit size
     * @param num_bits Number of bits
     * @param num_hashes Number of hash functions
     */
    BloomFilter(size_t num_bits, size_t num_hashes, bool);

    ~BloomFilter() = default;

    // Non-copyable but moveable
    BloomFilter(const BloomFilter&) = delete;
    BloomFilter& operator=(const BloomFilter&) = delete;
    BloomFilter(BloomFilter&&) = default;
    BloomFilter& operator=(BloomFilter&&) = default;

    /**
     * @brief Add an item to the filter
     */
    void add(const void* data, size_t len);

    /**
     * @brief Add an item (templated version)
     */
    template<typename T>
    void add(const T& item) {
        add(&item, sizeof(T));
    }

    /**
     * @brief Check if item might be in the filter
     * @return true if possibly present, false if definitely not present
     */
    bool possibly_contains(const void* data, size_t len) const;

    /**
     * @brief Check if item might be in the filter (templated)
     */
    template<typename T>
    bool possibly_contains(const T& item) const {
        return possibly_contains(&item, sizeof(T));
    }

    /**
     * @brief Clear all bits
     */
    void clear();

    /**
     * @brief Get statistics
     */
    const BloomFilterStats& stats() const { return stats_; }

    /**
     * @brief Save filter to file
     */
    bool save(const std::string& filename) const;

    /**
     * @brief Load filter from file
     */
    bool load(const std::string& filename);

    /**
     * @brief Get memory usage in bytes
     */
    size_t memory_usage() const { return bits_.size(); }

    /**
     * @brief Get number of bits
     */
    size_t num_bits() const { return num_bits_; }

    /**
     * @brief Get number of hash functions
     */
    size_t num_hashes() const { return num_hashes_; }

    /**
     * @brief Calculate optimal number of bits
     */
    static size_t optimal_bits(size_t items, double fp_rate) {
        return static_cast<size_t>(-1.0 * items * std::log(fp_rate) /
                                   (std::log(2) * std::log(2)));
    }

    /**
     * @brief Calculate optimal number of hash functions
     */
    static size_t optimal_hashes(size_t bits, size_t items) {
        return static_cast<size_t>(std::round(
            static_cast<double>(bits) / items * std::log(2)));
    }

private:
    // XXHash-based hash function
    uint64_t hash(const void* data, size_t len, uint64_t seed) const;

    // Set bit at position
    void set_bit(size_t pos) {
        size_t byte_idx = pos / 8;
        size_t bit_idx = pos % 8;
        bits_[byte_idx] |= (1 << bit_idx);
    }

    // Test bit at position
    bool test_bit(size_t pos) const {
        size_t byte_idx = pos / 8;
        size_t bit_idx = pos % 8;
        return (bits_[byte_idx] >> bit_idx) & 1;
    }

    std::vector<uint8_t> bits_;
    size_t num_bits_;
    size_t num_hashes_;
    BloomFilterStats stats_;
    mutable std::mutex mutex_;
};

/**
 * @brief Cascading bloom filter for multi-level filtering
 *
 * Uses multiple bloom filters with decreasing size to
 * dramatically reduce false positive rates.
 */
class CascadingBloomFilter {
public:
    /**
     * @brief Create cascading filter with specified levels
     * @param expected_items Expected number of items
     * @param levels Number of cascade levels
     * @param base_fp_rate Base false positive rate per level
     */
    CascadingBloomFilter(size_t expected_items, size_t levels = 3,
                         double base_fp_rate = 0.01);

    /**
     * @brief Add an item to all levels
     */
    void add(const void* data, size_t len);

    template<typename T>
    void add(const T& item) {
        add(&item, sizeof(T));
    }

    /**
     * @brief Check if item might be in the filter
     *
     * Checks each level in sequence. If any level returns false,
     * the item is definitely not present.
     */
    bool possibly_contains(const void* data, size_t len) const;

    template<typename T>
    bool possibly_contains(const T& item) const {
        return possibly_contains(&item, sizeof(T));
    }

    /**
     * @brief Get total memory usage
     */
    size_t memory_usage() const;

    /**
     * @brief Get number of levels
     */
    size_t num_levels() const { return filters_.size(); }

    /**
     * @brief Get filter at level
     */
    const BloomFilter& level(size_t idx) const { return *filters_[idx]; }

    /**
     * @brief Save all levels to files
     */
    bool save(const std::string& basename) const;

    /**
     * @brief Load all levels from files
     */
    bool load(const std::string& basename);

    /**
     * @brief Get combined false positive rate
     */
    double combined_fp_rate() const;

private:
    std::vector<std::unique_ptr<BloomFilter>> filters_;
    size_t items_added_ = 0;
};

/**
 * @brief Partitioned bloom filter for parallel access
 *
 * Divides the filter into partitions that can be
 * accessed independently with minimal locking.
 */
class PartitionedBloomFilter {
public:
    /**
     * @brief Create partitioned filter
     * @param expected_items Expected number of items
     * @param partitions Number of partitions (typically 256)
     * @param fp_rate Target false positive rate
     */
    PartitionedBloomFilter(size_t expected_items, size_t partitions = 256,
                           double fp_rate = 0.001);

    /**
     * @brief Add an item
     */
    void add(const void* data, size_t len);

    template<typename T>
    void add(const T& item) {
        add(&item, sizeof(T));
    }

    /**
     * @brief Check if item might be present
     */
    bool possibly_contains(const void* data, size_t len) const;

    template<typename T>
    bool possibly_contains(const T& item) const {
        return possibly_contains(&item, sizeof(T));
    }

    /**
     * @brief Get total memory usage
     */
    size_t memory_usage() const;

    /**
     * @brief Get number of partitions
     */
    size_t num_partitions() const { return filters_.size(); }

private:
    // Get partition index for data
    size_t get_partition(const void* data, size_t len) const;

    std::vector<std::unique_ptr<BloomFilter>> filters_;
    std::vector<std::unique_ptr<std::mutex>> mutexes_;
    size_t num_partitions_;
};

/**
 * @brief Counting bloom filter (supports removal)
 */
class CountingBloomFilter {
public:
    /**
     * @brief Create counting bloom filter
     * @param expected_items Expected number of items
     * @param fp_rate Target false positive rate
     * @param counter_bits Bits per counter (4 = max count 15)
     */
    CountingBloomFilter(size_t expected_items, double fp_rate = 0.001,
                        size_t counter_bits = 4);

    /**
     * @brief Add an item (increment counters)
     */
    void add(const void* data, size_t len);

    /**
     * @brief Remove an item (decrement counters)
     * @return true if item was present
     */
    bool remove(const void* data, size_t len);

    /**
     * @brief Check if item might be present
     */
    bool possibly_contains(const void* data, size_t len) const;

    /**
     * @brief Get minimum count across all hash positions
     */
    size_t min_count(const void* data, size_t len) const;

private:
    std::vector<uint8_t> counters_;
    size_t num_counters_;
    size_t num_hashes_;
    size_t counter_bits_;
    size_t max_count_;
};

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_BLOOM_FILTER_H
