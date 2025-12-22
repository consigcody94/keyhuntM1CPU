/**
 * @file test_bloom_filter.cpp
 * @brief Unit tests for bloom filter implementations
 */

// Minimal bloom filter implementation for tests
// (The full implementation would be in src/core/bloom_filter.cpp)

#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace {

// Simple hash function for testing
uint64_t simple_hash(const void* data, size_t len, uint64_t seed) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = seed;

    for (size_t i = 0; i < len; ++i) {
        hash = hash * 31 + bytes[i];
        hash ^= (hash >> 17);
        hash *= 0x85ebca6b;
    }

    return hash;
}

class SimpleBloomFilter {
public:
    SimpleBloomFilter(size_t num_bits, size_t num_hashes)
        : bits_((num_bits + 7) / 8, 0)
        , num_bits_(num_bits)
        , num_hashes_(num_hashes) {}

    void add(const void* data, size_t len) {
        for (size_t i = 0; i < num_hashes_; ++i) {
            uint64_t h = simple_hash(data, len, i);
            size_t pos = h % num_bits_;
            bits_[pos / 8] |= (1 << (pos % 8));
        }
    }

    bool possibly_contains(const void* data, size_t len) const {
        for (size_t i = 0; i < num_hashes_; ++i) {
            uint64_t h = simple_hash(data, len, i);
            size_t pos = h % num_bits_;
            if (!(bits_[pos / 8] & (1 << (pos % 8)))) {
                return false;
            }
        }
        return true;
    }

    void clear() {
        std::fill(bits_.begin(), bits_.end(), 0);
    }

    size_t memory_usage() const { return bits_.size(); }

private:
    std::vector<uint8_t> bits_;
    size_t num_bits_;
    size_t num_hashes_;
};

} // anonymous namespace

TEST(BloomFilter, BasicOperations) {
    SimpleBloomFilter filter(10000, 7);

    // Initially empty
    uint32_t test_value = 12345;
    EXPECT_FALSE(filter.possibly_contains(&test_value, sizeof(test_value)));

    // Add value
    filter.add(&test_value, sizeof(test_value));
    EXPECT_TRUE(filter.possibly_contains(&test_value, sizeof(test_value)));

    return true;
}

TEST(BloomFilter, NoFalseNegatives) {
    SimpleBloomFilter filter(100000, 7);

    // Add 1000 values
    std::vector<uint32_t> values;
    for (uint32_t i = 0; i < 1000; ++i) {
        values.push_back(i * 7 + 13);
        filter.add(&values.back(), sizeof(uint32_t));
    }

    // All added values must be found
    for (const auto& v : values) {
        EXPECT_TRUE(filter.possibly_contains(&v, sizeof(uint32_t)));
    }

    return true;
}

TEST(BloomFilter, FalsePositiveRate) {
    // Create filter for 1000 items with ~1% FP rate
    // Optimal bits = -n * ln(p) / (ln(2)^2)
    size_t n = 1000;
    double p = 0.01;
    size_t bits = static_cast<size_t>(-n * std::log(p) / (std::log(2) * std::log(2)));
    size_t hashes = static_cast<size_t>(std::round(bits / n * std::log(2)));

    SimpleBloomFilter filter(bits, hashes);

    // Add values
    for (uint32_t i = 0; i < n; ++i) {
        filter.add(&i, sizeof(uint32_t));
    }

    // Test for false positives with values not in the filter
    int false_positives = 0;
    int tests = 10000;

    for (int i = 0; i < tests; ++i) {
        uint32_t test_value = n + i;  // Values not in filter
        if (filter.possibly_contains(&test_value, sizeof(uint32_t))) {
            ++false_positives;
        }
    }

    double actual_fp_rate = static_cast<double>(false_positives) / tests;

    // Allow 3x expected rate due to simple hash function
    EXPECT_LT(actual_fp_rate, 0.03);

    return true;
}

TEST(BloomFilter, Clear) {
    SimpleBloomFilter filter(10000, 5);

    uint32_t value = 42;
    filter.add(&value, sizeof(value));
    EXPECT_TRUE(filter.possibly_contains(&value, sizeof(value)));

    filter.clear();
    EXPECT_FALSE(filter.possibly_contains(&value, sizeof(value)));

    return true;
}

TEST(BloomFilter, DifferentDataTypes) {
    SimpleBloomFilter filter(10000, 7);

    // Test with different data types
    int32_t int_val = -12345;
    filter.add(&int_val, sizeof(int_val));
    EXPECT_TRUE(filter.possibly_contains(&int_val, sizeof(int_val)));

    double double_val = 3.14159;
    filter.add(&double_val, sizeof(double_val));
    EXPECT_TRUE(filter.possibly_contains(&double_val, sizeof(double_val)));

    const char* str = "Hello, World!";
    filter.add(str, strlen(str));
    EXPECT_TRUE(filter.possibly_contains(str, strlen(str)));

    return true;
}

TEST(BloomFilter, LargeDataset) {
    SimpleBloomFilter filter(1000000, 10);

    // Add 10000 hash values
    for (uint64_t i = 0; i < 10000; ++i) {
        uint8_t hash[20];
        for (int j = 0; j < 20; ++j) {
            hash[j] = static_cast<uint8_t>((i * 37 + j) & 0xFF);
        }
        filter.add(hash, sizeof(hash));
    }

    // Verify all are found
    for (uint64_t i = 0; i < 10000; ++i) {
        uint8_t hash[20];
        for (int j = 0; j < 20; ++j) {
            hash[j] = static_cast<uint8_t>((i * 37 + j) & 0xFF);
        }
        EXPECT_TRUE(filter.possibly_contains(hash, sizeof(hash)));
    }

    return true;
}

TEST(BloomFilter, MemoryUsage) {
    SimpleBloomFilter filter(1000000, 7);

    // Should use approximately 125KB (1M bits = 125K bytes)
    EXPECT_GE(filter.memory_usage(), 125000UL);
    EXPECT_LE(filter.memory_usage(), 126000UL);

    return true;
}

// Cascading Bloom Filter tests (simulated)
TEST(CascadingBloomFilter, MultiLevel) {
    // Simulate 3-level cascade
    SimpleBloomFilter level1(100000, 5);  // Coarse filter
    SimpleBloomFilter level2(50000, 7);   // Medium filter
    SimpleBloomFilter level3(25000, 10);  // Fine filter

    // Add values to all levels
    for (uint32_t i = 0; i < 1000; ++i) {
        level1.add(&i, sizeof(i));
        level2.add(&i, sizeof(i));
        level3.add(&i, sizeof(i));
    }

    // Test cascade logic
    auto cascade_check = [&](const uint32_t& v) {
        if (!level1.possibly_contains(&v, sizeof(v))) return false;
        if (!level2.possibly_contains(&v, sizeof(v))) return false;
        if (!level3.possibly_contains(&v, sizeof(v))) return false;
        return true;
    };

    // All added values should pass
    for (uint32_t i = 0; i < 1000; ++i) {
        EXPECT_TRUE(cascade_check(i));
    }

    // Count false positives through cascade
    int fp_count = 0;
    for (uint32_t i = 1000; i < 11000; ++i) {
        if (cascade_check(i)) {
            ++fp_count;
        }
    }

    // Cascade should significantly reduce FP rate
    double fp_rate = static_cast<double>(fp_count) / 10000;
    EXPECT_LT(fp_rate, 0.001);  // < 0.1% with cascade

    return true;
}
