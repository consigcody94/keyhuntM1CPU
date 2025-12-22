/**
 * @file benchmark_main.cpp
 * @brief Comprehensive benchmarking suite for keyhunt
 *
 * Measures performance of critical operations including:
 * - Elliptic curve operations
 * - Hash functions
 * - Bloom filter operations
 * - Thread pool throughput
 * - Memory allocation
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <functional>
#include <iomanip>
#include <cmath>
#include <thread>
#include <atomic>
#include <random>

#include "../include/keyhunt/core/thread_pool.h"
#include "../include/keyhunt/core/memory.h"
#include "../include/keyhunt/core/types.h"
#include "../include/keyhunt/core/simd.h"

using namespace keyhunt::core;

// Benchmark framework
namespace benchmark {

struct Result {
    std::string name;
    size_t iterations;
    double total_time_ms;
    double ops_per_second;
    double time_per_op_ns;

    void print() const {
        std::cout << std::left << std::setw(40) << name
                  << std::right << std::setw(12) << iterations << " ops  "
                  << std::setw(10) << std::fixed << std::setprecision(2)
                  << total_time_ms << " ms  "
                  << std::setw(12) << std::fixed << std::setprecision(0)
                  << ops_per_second << " ops/s  "
                  << std::setw(10) << std::fixed << std::setprecision(2)
                  << time_per_op_ns << " ns/op"
                  << std::endl;
    }
};

template<typename Func>
Result run(const std::string& name, size_t iterations, Func&& func) {
    // Warmup
    for (size_t i = 0; i < std::min(iterations / 10, size_t(1000)); ++i) {
        func();
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        func();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double total_ms = duration.count() / 1e6;
    double ops_per_sec = iterations / (total_ms / 1000.0);
    double ns_per_op = static_cast<double>(duration.count()) / iterations;

    return {name, iterations, total_ms, ops_per_sec, ns_per_op};
}

template<typename Func>
Result run_timed(const std::string& name, double target_seconds, Func&& func) {
    // Estimate iterations needed
    auto start = std::chrono::high_resolution_clock::now();
    size_t warmup_iters = 1000;
    for (size_t i = 0; i < warmup_iters; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto warmup_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double ns_per_op = static_cast<double>(warmup_duration.count()) / warmup_iters;
    size_t target_iters = static_cast<size_t>(target_seconds * 1e9 / ns_per_op);
    target_iters = std::max(target_iters, size_t(1000));

    return run(name, target_iters, std::forward<Func>(func));
}

} // namespace benchmark

// Hash function benchmarks
namespace {

// Simple reference hash implementations for benchmarking
uint32_t simple_hash32(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = 0x811c9dc5;
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= 0x01000193;
    }
    return hash;
}

uint64_t simple_hash64(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

} // anonymous namespace

void run_hash_benchmarks() {
    std::cout << "\n=== Hash Function Benchmarks ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    uint8_t data32[32];
    uint8_t data64[64];

    for (int i = 0; i < 32; ++i) data32[i] = i;
    for (int i = 0; i < 64; ++i) data64[i] = i;

    volatile uint32_t sink32 = 0;
    volatile uint64_t sink64 = 0;

    benchmark::run_timed("Hash32 (32 bytes)", 1.0, [&]() {
        sink32 = simple_hash32(data32, 32);
    }).print();

    benchmark::run_timed("Hash64 (32 bytes)", 1.0, [&]() {
        sink64 = simple_hash64(data32, 32);
    }).print();

    benchmark::run_timed("Hash64 (64 bytes)", 1.0, [&]() {
        sink64 = simple_hash64(data64, 64);
    }).print();

    (void)sink32;
    (void)sink64;
}

void run_memory_benchmarks() {
    std::cout << "\n=== Memory Benchmarks ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    // Standard allocation
    benchmark::run_timed("malloc/free (1KB)", 1.0, []() {
        void* p = std::malloc(1024);
        std::free(p);
    }).print();

    benchmark::run_timed("malloc/free (1MB)", 1.0, []() {
        void* p = std::malloc(1024 * 1024);
        std::free(p);
    }).print();

    // Aligned allocation
    AlignedAllocator<uint8_t, 64> alloc;
    benchmark::run_timed("Aligned alloc/free (1KB)", 1.0, [&]() {
        uint8_t* p = alloc.allocate(1024);
        alloc.deallocate(p, 1024);
    }).print();

    // Vector push_back
    benchmark::run("vector<int> push_back (1M)", 100, []() {
        std::vector<int> v;
        v.reserve(1000000);
        for (int i = 0; i < 1000000; ++i) {
            v.push_back(i);
        }
    }).print();

    // AlignedVector
    benchmark::run("AlignedVector<int> push_back (1M)", 100, []() {
        AlignedVector<int> v;
        v.reserve(1000000);
        for (int i = 0; i < 1000000; ++i) {
            v.push_back(i);
        }
    }).print();
}

void run_thread_pool_benchmarks() {
    std::cout << "\n=== Thread Pool Benchmarks ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    ThreadPool pool(std::thread::hardware_concurrency());

    // Empty task submission
    benchmark::run_timed("Submit empty task", 1.0, [&]() {
        pool.submit([]() {});
    }).print();

    pool.wait();

    // Task with return value
    benchmark::run_timed("Submit task with result", 1.0, [&]() {
        auto f = pool.submit([]() { return 42; });
        (void)f;  // Don't wait
    }).print();

    pool.wait();

    // Submit and wait
    benchmark::run("Submit + wait (1000 tasks)", 100, [&]() {
        for (int i = 0; i < 1000; ++i) {
            pool.submit([]() {
                volatile int x = 0;
                for (int j = 0; j < 100; ++j) x += j;
            });
        }
        pool.wait();
    }).print();

    // Parallel for
    std::vector<int> data(100000);
    benchmark::run_timed("parallel_for (100K elements)", 1.0, [&]() {
        parallel_for(pool, size_t(0), data.size(), [&](size_t i) {
            data[i] = i * 2;
        });
    }).print();
}

void run_type_benchmarks() {
    std::cout << "\n=== Type Benchmarks ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    UInt256 a = *UInt256::from_hex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    UInt256 b = *UInt256::from_hex("0000000000000000000000000000000000000000000000000000000000000001");

    volatile UInt256 result;

    benchmark::run_timed("UInt256 addition", 1.0, [&]() {
        result = a + b;
    }).print();

    benchmark::run_timed("UInt256 subtraction", 1.0, [&]() {
        result = a - b;
    }).print();

    benchmark::run_timed("UInt256 comparison", 1.0, [&]() {
        volatile bool r = a < b;
        (void)r;
    }).print();

    benchmark::run_timed("UInt256 from_hex", 1.0, [&]() {
        auto r = UInt256::from_hex("deadbeefcafebabe1234567890abcdef");
        (void)r;
    }).print();

    benchmark::run_timed("UInt256 to_hex", 1.0, [&]() {
        std::string s = a.to_hex();
        (void)s;
    }).print();

    Hash256 hash;
    for (int i = 0; i < 32; ++i) hash[i] = i;

    benchmark::run_timed("Hash256 from_hex", 1.0, [&]() {
        auto h = Hash256::from_hex(
            "0000000000000000000000000000000000000000000000000000000000000001");
        (void)h;
    }).print();

    benchmark::run_timed("Hash256 to_hex", 1.0, [&]() {
        std::string s = hash.to_hex();
        (void)s;
    }).print();

    (void)result;
}

void run_simd_benchmarks() {
    std::cout << "\n=== SIMD Benchmarks ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    auto caps = simd::SimdCapabilities::detect();
    caps.print();
    std::cout << std::endl;

    alignas(64) uint32_t data_a[4] = {1, 2, 3, 4};
    alignas(64) uint32_t data_b[4] = {5, 6, 7, 8};
    alignas(64) uint32_t result[4];

    // Scalar baseline
    benchmark::run_timed("Scalar 4x add", 1.0, [&]() {
        for (int i = 0; i < 4; ++i) {
            result[i] = data_a[i] + data_b[i];
        }
    }).print();

    // SIMD add
    benchmark::run_timed("SIMD 4x add (UInt32x4)", 1.0, [&]() {
        simd::UInt32x4 a = simd::UInt32x4::load(data_a);
        simd::UInt32x4 b = simd::UInt32x4::load(data_b);
        simd::UInt32x4 c = a + b;
        c.store(result);
    }).print();

    // XOR
    benchmark::run_timed("SIMD 4x xor (UInt32x4)", 1.0, [&]() {
        simd::UInt32x4 a = simd::UInt32x4::load(data_a);
        simd::UInt32x4 b = simd::UInt32x4::load(data_b);
        simd::UInt32x4 c = a ^ b;
        c.store(result);
    }).print();

    // Rotate
    benchmark::run_timed("SIMD 4x rotl<7> (UInt32x4)", 1.0, [&]() {
        simd::UInt32x4 a = simd::UInt32x4::load(data_a);
        simd::UInt32x4 c = a.rotl<7>();
        c.store(result);
    }).print();
}

void run_bloom_filter_benchmarks() {
    std::cout << "\n=== Bloom Filter Benchmarks ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    // Simple bloom filter implementation for benchmarking
    const size_t NUM_BITS = 1000000;
    const size_t NUM_HASHES = 7;
    std::vector<uint8_t> filter((NUM_BITS + 7) / 8, 0);

    auto add = [&](uint32_t value) {
        for (size_t i = 0; i < NUM_HASHES; ++i) {
            uint64_t hash = simple_hash64(&value, sizeof(value));
            hash ^= i * 0x9e3779b97f4a7c15ULL;
            size_t pos = hash % NUM_BITS;
            filter[pos / 8] |= (1 << (pos % 8));
        }
    };

    auto check = [&](uint32_t value) -> bool {
        for (size_t i = 0; i < NUM_HASHES; ++i) {
            uint64_t hash = simple_hash64(&value, sizeof(value));
            hash ^= i * 0x9e3779b97f4a7c15ULL;
            size_t pos = hash % NUM_BITS;
            if (!(filter[pos / 8] & (1 << (pos % 8)))) {
                return false;
            }
        }
        return true;
    };

    benchmark::run_timed("Bloom filter add", 1.0, [&]() {
        static uint32_t counter = 0;
        add(counter++);
    }).print();

    // Add some items first
    for (uint32_t i = 0; i < 10000; ++i) {
        add(i);
    }

    benchmark::run_timed("Bloom filter check (positive)", 1.0, [&]() {
        static uint32_t counter = 0;
        volatile bool r = check(counter++ % 10000);
        (void)r;
    }).print();

    benchmark::run_timed("Bloom filter check (negative)", 1.0, [&]() {
        static uint32_t counter = 100000;
        volatile bool r = check(counter++);
        (void)r;
    }).print();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              Keyhunt Benchmark Suite                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    std::cout << "\nSystem: " << std::thread::hardware_concurrency() << " CPU cores\n";
    std::cout << "SIMD width: " << simd::SIMD_WIDTH << " bytes\n";

    run_hash_benchmarks();
    run_memory_benchmarks();
    run_thread_pool_benchmarks();
    run_type_benchmarks();
    run_simd_benchmarks();
    run_bloom_filter_benchmarks();

    std::cout << "\n=== Benchmark Complete ===" << std::endl;

    return 0;
}
