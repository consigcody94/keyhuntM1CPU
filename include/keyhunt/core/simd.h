/**
 * @file simd.h
 * @brief SIMD abstractions for cross-platform vectorization
 *
 * Provides a unified interface for SIMD operations across:
 * - ARM NEON (Apple Silicon M1/M2/M3/M4)
 * - x86 SSE/AVX
 * - Fallback scalar implementations
 */

#ifndef KEYHUNT_CORE_SIMD_H
#define KEYHUNT_CORE_SIMD_H

#include <cstdint>
#include <cstddef>
#include <array>

// Detect SIMD capabilities
#if defined(__aarch64__) || defined(_M_ARM64)
    #define KEYHUNT_SIMD_NEON 1
    #include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64)
    #if defined(__AVX2__)
        #define KEYHUNT_SIMD_AVX2 1
    #endif
    #if defined(__SSE4_1__) || defined(__AVX2__)
        #define KEYHUNT_SIMD_SSE4 1
    #endif
    #if defined(__SSE2__) || defined(__AVX2__) || defined(__SSE4_1__)
        #define KEYHUNT_SIMD_SSE2 1
    #endif
    #ifdef KEYHUNT_SIMD_SSE2
        #include <immintrin.h>
    #endif
#endif

namespace keyhunt {
namespace core {
namespace simd {

/**
 * @brief SIMD register width in bytes
 */
#if defined(KEYHUNT_SIMD_AVX2)
    constexpr size_t SIMD_WIDTH = 32;
#elif defined(KEYHUNT_SIMD_NEON) || defined(KEYHUNT_SIMD_SSE2)
    constexpr size_t SIMD_WIDTH = 16;
#else
    constexpr size_t SIMD_WIDTH = 8;  // Scalar fallback
#endif

/**
 * @brief 128-bit vector of 4x 32-bit unsigned integers
 */
class UInt32x4 {
public:
    UInt32x4() = default;

    // Broadcast single value
    explicit UInt32x4(uint32_t v) {
#if defined(KEYHUNT_SIMD_NEON)
        data_ = vdupq_n_u32(v);
#elif defined(KEYHUNT_SIMD_SSE2)
        data_ = _mm_set1_epi32(v);
#else
        for (int i = 0; i < 4; ++i) data_[i] = v;
#endif
    }

    // Set individual values
    UInt32x4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
#if defined(KEYHUNT_SIMD_NEON)
        uint32_t tmp[4] = {a, b, c, d};
        data_ = vld1q_u32(tmp);
#elif defined(KEYHUNT_SIMD_SSE2)
        data_ = _mm_set_epi32(d, c, b, a);  // Reversed order for SSE
#else
        data_[0] = a; data_[1] = b; data_[2] = c; data_[3] = d;
#endif
    }

    // Load from memory (aligned)
    static UInt32x4 load(const uint32_t* ptr) {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vld1q_u32(ptr);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = ptr[i];
#endif
        return result;
    }

    // Load from memory (unaligned)
    static UInt32x4 loadu(const uint32_t* ptr) {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vld1q_u32(ptr);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = ptr[i];
#endif
        return result;
    }

    // Store to memory (aligned)
    void store(uint32_t* ptr) const {
#if defined(KEYHUNT_SIMD_NEON)
        vst1q_u32(ptr, data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        _mm_store_si128(reinterpret_cast<__m128i*>(ptr), data_);
#else
        for (int i = 0; i < 4; ++i) ptr[i] = data_[i];
#endif
    }

    // Store to memory (unaligned)
    void storeu(uint32_t* ptr) const {
#if defined(KEYHUNT_SIMD_NEON)
        vst1q_u32(ptr, data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), data_);
#else
        for (int i = 0; i < 4; ++i) ptr[i] = data_[i];
#endif
    }

    // Addition
    UInt32x4 operator+(const UInt32x4& other) const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vaddq_u32(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_add_epi32(data_, other.data_);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] + other.data_[i];
#endif
        return result;
    }

    // Subtraction
    UInt32x4 operator-(const UInt32x4& other) const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vsubq_u32(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_sub_epi32(data_, other.data_);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] - other.data_[i];
#endif
        return result;
    }

    // Bitwise AND
    UInt32x4 operator&(const UInt32x4& other) const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vandq_u32(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_and_si128(data_, other.data_);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] & other.data_[i];
#endif
        return result;
    }

    // Bitwise OR
    UInt32x4 operator|(const UInt32x4& other) const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vorrq_u32(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_or_si128(data_, other.data_);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] | other.data_[i];
#endif
        return result;
    }

    // Bitwise XOR
    UInt32x4 operator^(const UInt32x4& other) const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = veorq_u32(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_xor_si128(data_, other.data_);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] ^ other.data_[i];
#endif
        return result;
    }

    // Left shift by immediate
    template<int N>
    UInt32x4 shl() const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vshlq_n_u32(data_, N);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_slli_epi32(data_, N);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] << N;
#endif
        return result;
    }

    // Right shift by immediate
    template<int N>
    UInt32x4 shr() const {
        UInt32x4 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vshrq_n_u32(data_, N);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_srli_epi32(data_, N);
#else
        for (int i = 0; i < 4; ++i) result.data_[i] = data_[i] >> N;
#endif
        return result;
    }

    // Rotate left
    template<int N>
    UInt32x4 rotl() const {
        return shl<N>() | shr<32 - N>();
    }

    // Rotate right
    template<int N>
    UInt32x4 rotr() const {
        return shr<N>() | shl<32 - N>();
    }

    // Extract element
    uint32_t operator[](size_t i) const {
#if defined(KEYHUNT_SIMD_NEON)
        return vgetq_lane_u32(data_, i & 3);
#elif defined(KEYHUNT_SIMD_SSE4)
        return _mm_extract_epi32(data_, i & 3);
#elif defined(KEYHUNT_SIMD_SSE2)
        alignas(16) uint32_t tmp[4];
        _mm_store_si128(reinterpret_cast<__m128i*>(tmp), data_);
        return tmp[i & 3];
#else
        return data_[i & 3];
#endif
    }

private:
#if defined(KEYHUNT_SIMD_NEON)
    uint32x4_t data_;
#elif defined(KEYHUNT_SIMD_SSE2)
    __m128i data_;
#else
    uint32_t data_[4];
#endif
};

/**
 * @brief 128-bit vector of 2x 64-bit unsigned integers
 */
class UInt64x2 {
public:
    UInt64x2() = default;

    explicit UInt64x2(uint64_t v) {
#if defined(KEYHUNT_SIMD_NEON)
        data_ = vdupq_n_u64(v);
#elif defined(KEYHUNT_SIMD_SSE2)
        data_ = _mm_set1_epi64x(v);
#else
        data_[0] = data_[1] = v;
#endif
    }

    UInt64x2(uint64_t a, uint64_t b) {
#if defined(KEYHUNT_SIMD_NEON)
        uint64_t tmp[2] = {a, b};
        data_ = vld1q_u64(tmp);
#elif defined(KEYHUNT_SIMD_SSE2)
        data_ = _mm_set_epi64x(b, a);
#else
        data_[0] = a; data_[1] = b;
#endif
    }

    static UInt64x2 load(const uint64_t* ptr) {
        UInt64x2 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vld1q_u64(ptr);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
#else
        result.data_[0] = ptr[0];
        result.data_[1] = ptr[1];
#endif
        return result;
    }

    void store(uint64_t* ptr) const {
#if defined(KEYHUNT_SIMD_NEON)
        vst1q_u64(ptr, data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        _mm_store_si128(reinterpret_cast<__m128i*>(ptr), data_);
#else
        ptr[0] = data_[0];
        ptr[1] = data_[1];
#endif
    }

    UInt64x2 operator+(const UInt64x2& other) const {
        UInt64x2 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = vaddq_u64(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_add_epi64(data_, other.data_);
#else
        result.data_[0] = data_[0] + other.data_[0];
        result.data_[1] = data_[1] + other.data_[1];
#endif
        return result;
    }

    UInt64x2 operator^(const UInt64x2& other) const {
        UInt64x2 result;
#if defined(KEYHUNT_SIMD_NEON)
        result.data_ = veorq_u64(data_, other.data_);
#elif defined(KEYHUNT_SIMD_SSE2)
        result.data_ = _mm_xor_si128(data_, other.data_);
#else
        result.data_[0] = data_[0] ^ other.data_[0];
        result.data_[1] = data_[1] ^ other.data_[1];
#endif
        return result;
    }

    uint64_t operator[](size_t i) const {
#if defined(KEYHUNT_SIMD_NEON)
        return vgetq_lane_u64(data_, i & 1);
#elif defined(KEYHUNT_SIMD_SSE2)
        alignas(16) uint64_t tmp[2];
        _mm_store_si128(reinterpret_cast<__m128i*>(tmp), data_);
        return tmp[i & 1];
#else
        return data_[i & 1];
#endif
    }

private:
#if defined(KEYHUNT_SIMD_NEON)
    uint64x2_t data_;
#elif defined(KEYHUNT_SIMD_SSE2)
    __m128i data_;
#else
    uint64_t data_[2];
#endif
};

/**
 * @brief SIMD-optimized SHA-256 message schedule (W array)
 */
inline void sha256_schedule_simd(const uint32_t* message, uint32_t* W) {
    // Load first 16 words
    for (int i = 0; i < 16; ++i) {
        W[i] = message[i];
    }

    // Extend to 64 words using SIMD
    for (int i = 16; i < 64; i += 4) {
        UInt32x4 w15 = UInt32x4::loadu(&W[i - 15]);
        UInt32x4 w2 = UInt32x4::loadu(&W[i - 2]);
        UInt32x4 w16 = UInt32x4::loadu(&W[i - 16]);
        UInt32x4 w7 = UInt32x4::loadu(&W[i - 7]);

        // σ0 = ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3)
        UInt32x4 s0 = w15.rotr<7>() ^ w15.rotr<18>() ^ w15.shr<3>();

        // σ1 = ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10)
        UInt32x4 s1 = w2.rotr<17>() ^ w2.rotr<19>() ^ w2.shr<10>();

        UInt32x4 result = w16 + s0 + w7 + s1;
        result.storeu(&W[i]);
    }
}

/**
 * @brief Query SIMD capabilities at runtime
 */
struct SimdCapabilities {
    bool has_neon = false;
    bool has_sse2 = false;
    bool has_sse4 = false;
    bool has_avx2 = false;

    static SimdCapabilities detect() {
        SimdCapabilities caps;
#if defined(KEYHUNT_SIMD_NEON)
        caps.has_neon = true;
#endif
#if defined(KEYHUNT_SIMD_SSE2)
        caps.has_sse2 = true;
#endif
#if defined(KEYHUNT_SIMD_SSE4)
        caps.has_sse4 = true;
#endif
#if defined(KEYHUNT_SIMD_AVX2)
        caps.has_avx2 = true;
#endif
        return caps;
    }

    void print() const {
        printf("SIMD Capabilities:\n");
        printf("  NEON: %s\n", has_neon ? "yes" : "no");
        printf("  SSE2: %s\n", has_sse2 ? "yes" : "no");
        printf("  SSE4: %s\n", has_sse4 ? "yes" : "no");
        printf("  AVX2: %s\n", has_avx2 ? "yes" : "no");
    }
};

} // namespace simd
} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_SIMD_H
