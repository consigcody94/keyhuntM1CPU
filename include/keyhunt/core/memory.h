/**
 * @file memory.h
 * @brief Safe memory management utilities for keyhunt
 *
 * Provides RAII wrappers, aligned memory allocation, memory pools,
 * and secure memory handling for cryptographic data.
 */

#ifndef KEYHUNT_CORE_MEMORY_H
#define KEYHUNT_CORE_MEMORY_H

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <new>

#include "error.h"

// Platform-specific includes for secure memory
#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

namespace keyhunt {
namespace core {

/**
 * @brief Aligned memory allocator for SIMD operations
 */
template<typename T, size_t Alignment = 64>
class AlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    AlignedAllocator() noexcept = default;

    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    pointer allocate(size_type n) {
        if (n == 0) return nullptr;

        void* ptr = nullptr;

#if defined(_WIN32)
        ptr = _aligned_malloc(n * sizeof(T), Alignment);
#elif defined(__APPLE__)
        // macOS doesn't have posix_memalign with alignments > page size
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            ptr = nullptr;
        }
#else
        ptr = std::aligned_alloc(Alignment, n * sizeof(T));
#endif

        if (!ptr) {
            throw std::bad_alloc();
        }

        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) noexcept {
        if (!p) return;

#if defined(_WIN32)
        _aligned_free(p);
#else
        std::free(p);
#endif
    }

    bool operator==(const AlignedAllocator&) const noexcept { return true; }
    bool operator!=(const AlignedAllocator&) const noexcept { return false; }
};

/**
 * @brief Vector with cache-aligned storage for SIMD
 */
template<typename T>
using AlignedVector = std::vector<T, AlignedAllocator<T, 64>>;

/**
 * @brief Secure memory that is zeroed on destruction
 *
 * Uses volatile writes to prevent compiler optimization.
 * Optionally locks memory to prevent swapping.
 */
template<typename T>
class SecureBuffer {
public:
    explicit SecureBuffer(size_t count, bool lock_memory = true)
        : size_(count * sizeof(T))
        , locked_(false) {

        if (count == 0) {
            data_ = nullptr;
            return;
        }

        // Allocate aligned memory
        data_ = static_cast<T*>(AlignedAllocator<T, 64>().allocate(count));

        // Lock memory to prevent swapping (if requested and supported)
        if (lock_memory) {
            lock();
        }

        // Zero-initialize
        secure_zero();
    }

    ~SecureBuffer() {
        if (data_) {
            secure_zero();
            unlock();
            AlignedAllocator<T, 64>().deallocate(data_, size_ / sizeof(T));
        }
    }

    // Non-copyable
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // Moveable
    SecureBuffer(SecureBuffer&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , locked_(other.locked_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.locked_ = false;
    }

    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            if (data_) {
                secure_zero();
                unlock();
                AlignedAllocator<T, 64>().deallocate(data_, size_ / sizeof(T));
            }
            data_ = other.data_;
            size_ = other.size_;
            locked_ = other.locked_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.locked_ = false;
        }
        return *this;
    }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    size_t size() const noexcept { return size_ / sizeof(T); }
    size_t bytes() const noexcept { return size_; }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    T* begin() noexcept { return data_; }
    T* end() noexcept { return data_ + size_ / sizeof(T); }
    const T* begin() const noexcept { return data_; }
    const T* end() const noexcept { return data_ + size_ / sizeof(T); }

    bool is_locked() const noexcept { return locked_; }

    void secure_zero() {
        if (data_ && size_ > 0) {
            volatile unsigned char* p = reinterpret_cast<volatile unsigned char*>(data_);
            for (size_t i = 0; i < size_; ++i) {
                p[i] = 0;
            }
            // Memory barrier to ensure writes complete
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }

private:
    void lock() {
#if defined(_WIN32)
        locked_ = VirtualLock(data_, size_) != 0;
#elif defined(__unix__) || defined(__APPLE__)
        locked_ = mlock(data_, size_) == 0;
#endif
    }

    void unlock() {
        if (locked_) {
#if defined(_WIN32)
            VirtualUnlock(data_, size_);
#elif defined(__unix__) || defined(__APPLE__)
            munlock(data_, size_);
#endif
            locked_ = false;
        }
    }

    T* data_;
    size_t size_;
    bool locked_;
};

/**
 * @brief Memory pool for fast allocation of fixed-size blocks
 */
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    MemoryPool() : current_block_(nullptr), current_pos_(0), blocks_allocated_(0) {}

    ~MemoryPool() {
        for (auto* block : blocks_) {
            std::free(block);
        }
    }

    // Non-copyable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!current_block_ || current_pos_ >= BlockSize) {
            allocate_block();
        }

        return &current_block_[current_pos_++];
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Reset position but keep blocks
        current_pos_ = 0;
        if (!blocks_.empty()) {
            current_block_ = blocks_[0];
        }
    }

    size_t allocated_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return blocks_allocated_ * BlockSize + current_pos_;
    }

    size_t memory_usage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return blocks_.size() * BlockSize * sizeof(T);
    }

private:
    void allocate_block() {
        T* new_block = static_cast<T*>(std::malloc(BlockSize * sizeof(T)));
        if (!new_block) {
            throw std::bad_alloc();
        }

        blocks_.push_back(new_block);
        current_block_ = new_block;
        current_pos_ = 0;
        ++blocks_allocated_;
    }

    std::vector<T*> blocks_;
    T* current_block_;
    size_t current_pos_;
    size_t blocks_allocated_;
    mutable std::mutex mutex_;
};

/**
 * @brief RAII wrapper for C-style resources
 */
template<typename T, typename Deleter>
class UniqueResource {
public:
    explicit UniqueResource(T resource, Deleter deleter)
        : resource_(resource)
        , deleter_(std::move(deleter))
        , owns_(true) {}

    ~UniqueResource() {
        reset();
    }

    // Non-copyable
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;

    // Moveable
    UniqueResource(UniqueResource&& other) noexcept
        : resource_(other.resource_)
        , deleter_(std::move(other.deleter_))
        , owns_(other.owns_) {
        other.owns_ = false;
    }

    UniqueResource& operator=(UniqueResource&& other) noexcept {
        if (this != &other) {
            reset();
            resource_ = other.resource_;
            deleter_ = std::move(other.deleter_);
            owns_ = other.owns_;
            other.owns_ = false;
        }
        return *this;
    }

    T get() const noexcept { return resource_; }
    T operator*() const noexcept { return resource_; }

    explicit operator bool() const noexcept { return owns_; }

    T release() noexcept {
        owns_ = false;
        return resource_;
    }

    void reset() {
        if (owns_) {
            deleter_(resource_);
            owns_ = false;
        }
    }

private:
    T resource_;
    Deleter deleter_;
    bool owns_;
};

/**
 * @brief Create a unique resource from a C-style resource
 */
template<typename T, typename Deleter>
UniqueResource<T, Deleter> make_unique_resource(T resource, Deleter deleter) {
    return UniqueResource<T, Deleter>(resource, std::move(deleter));
}

/**
 * @brief Safe file handle wrapper
 */
class FileHandle {
public:
    explicit FileHandle(const std::string& path, const char* mode)
        : file_(std::fopen(path.c_str(), mode)) {
        if (!file_) {
            throw IOException("Failed to open file: " + path);
        }
    }

    explicit FileHandle(FILE* f) : file_(f) {}

    ~FileHandle() {
        if (file_) {
            std::fclose(file_);
        }
    }

    // Non-copyable
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // Moveable
    FileHandle(FileHandle&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }

    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (file_) std::fclose(file_);
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }

    FILE* get() const noexcept { return file_; }
    operator FILE*() const noexcept { return file_; }

    bool is_open() const noexcept { return file_ != nullptr; }

    void close() {
        if (file_) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

private:
    FILE* file_;
};

/**
 * @brief Memory statistics tracker
 */
class MemoryStats {
public:
    static MemoryStats& instance() {
        static MemoryStats instance;
        return instance;
    }

    void record_allocation(size_t bytes) {
        allocated_.fetch_add(bytes, std::memory_order_relaxed);
        total_allocations_.fetch_add(1, std::memory_order_relaxed);

        size_t current = allocated_.load(std::memory_order_relaxed);
        size_t peak = peak_allocated_.load(std::memory_order_relaxed);
        while (current > peak &&
               !peak_allocated_.compare_exchange_weak(peak, current,
                   std::memory_order_relaxed, std::memory_order_relaxed));
    }

    void record_deallocation(size_t bytes) {
        allocated_.fetch_sub(bytes, std::memory_order_relaxed);
        total_deallocations_.fetch_add(1, std::memory_order_relaxed);
    }

    size_t current_allocated() const {
        return allocated_.load(std::memory_order_relaxed);
    }

    size_t peak_allocated() const {
        return peak_allocated_.load(std::memory_order_relaxed);
    }

    size_t total_allocations() const {
        return total_allocations_.load(std::memory_order_relaxed);
    }

    size_t total_deallocations() const {
        return total_deallocations_.load(std::memory_order_relaxed);
    }

    void print_stats() const {
        printf("\n=== Memory Statistics ===\n");
        printf("Current Allocated: %.2f MB\n", current_allocated() / (1024.0 * 1024.0));
        printf("Peak Allocated:    %.2f MB\n", peak_allocated() / (1024.0 * 1024.0));
        printf("Total Allocations: %zu\n", total_allocations());
        printf("Total Deallocations: %zu\n", total_deallocations());
        printf("=========================\n\n");
    }

private:
    MemoryStats() = default;

    std::atomic<size_t> allocated_{0};
    std::atomic<size_t> peak_allocated_{0};
    std::atomic<size_t> total_allocations_{0};
    std::atomic<size_t> total_deallocations_{0};
};

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_MEMORY_H
