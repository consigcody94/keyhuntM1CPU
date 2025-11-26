/*
 * Keyhunt - Bitcoin/Ethereum Private Key Hunter
 * Configuration header for cross-platform compatibility
 */

#ifndef KEYHUNT_CONFIG_H
#define KEYHUNT_CONFIG_H

// Version information
#define KEYHUNT_VERSION_MAJOR 0
#define KEYHUNT_VERSION_MINOR 3
#define KEYHUNT_VERSION_PATCH 0
#define KEYHUNT_VERSION_STRING "0.3.0"

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define KEYHUNT_PLATFORM_WINDOWS 1
    #define KEYHUNT_PLATFORM_NAME "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #define KEYHUNT_PLATFORM_MACOS 1
    #define KEYHUNT_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define KEYHUNT_PLATFORM_LINUX 1
    #define KEYHUNT_PLATFORM_NAME "Linux"
#elif defined(__unix__)
    #define KEYHUNT_PLATFORM_UNIX 1
    #define KEYHUNT_PLATFORM_NAME "Unix"
#else
    #define KEYHUNT_PLATFORM_UNKNOWN 1
    #define KEYHUNT_PLATFORM_NAME "Unknown"
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define KEYHUNT_ARCH_X64 1
    #define KEYHUNT_ARCH_NAME "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define KEYHUNT_ARCH_ARM64 1
    #define KEYHUNT_ARCH_NAME "ARM64"
#elif defined(__i386__) || defined(_M_IX86)
    #define KEYHUNT_ARCH_X86 1
    #define KEYHUNT_ARCH_NAME "x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define KEYHUNT_ARCH_ARM 1
    #define KEYHUNT_ARCH_NAME "ARM"
#else
    #define KEYHUNT_ARCH_UNKNOWN 1
    #define KEYHUNT_ARCH_NAME "Unknown"
#endif

// SIMD detection
#if KEYHUNT_ARCH_X64 || KEYHUNT_ARCH_X86
    #if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        #define KEYHUNT_HAS_SSE2 1
    #endif
    #if defined(__SSE4_1__) || defined(__AVX__)
        #define KEYHUNT_HAS_SSE41 1
    #endif
    #if defined(__AVX2__)
        #define KEYHUNT_HAS_AVX2 1
    #endif
#elif KEYHUNT_ARCH_ARM64
    #define KEYHUNT_HAS_NEON 1
#endif

// Thread local storage
#if defined(__cplusplus) && __cplusplus >= 201103L
    #define KEYHUNT_THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
    #define KEYHUNT_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
    #define KEYHUNT_THREAD_LOCAL __thread
#else
    #define KEYHUNT_THREAD_LOCAL
#endif

// Inline hints
#if defined(_MSC_VER)
    #define KEYHUNT_FORCE_INLINE __forceinline
    #define KEYHUNT_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define KEYHUNT_FORCE_INLINE __attribute__((always_inline)) inline
    #define KEYHUNT_NO_INLINE __attribute__((noinline))
#else
    #define KEYHUNT_FORCE_INLINE inline
    #define KEYHUNT_NO_INLINE
#endif

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
    #define KEYHUNT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define KEYHUNT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define KEYHUNT_LIKELY(x)   (x)
    #define KEYHUNT_UNLIKELY(x) (x)
#endif

// Alignment
#if defined(_MSC_VER)
    #define KEYHUNT_ALIGN(x) __declspec(align(x))
#elif defined(__GNUC__) || defined(__clang__)
    #define KEYHUNT_ALIGN(x) __attribute__((aligned(x)))
#else
    #define KEYHUNT_ALIGN(x)
#endif

// Export/Import for shared libraries
#if defined(KEYHUNT_PLATFORM_WINDOWS)
    #if defined(KEYHUNT_BUILDING_DLL)
        #define KEYHUNT_API __declspec(dllexport)
    #elif defined(KEYHUNT_USING_DLL)
        #define KEYHUNT_API __declspec(dllimport)
    #else
        #define KEYHUNT_API
    #endif
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define KEYHUNT_API __attribute__((visibility("default")))
    #else
        #define KEYHUNT_API
    #endif
#endif

// Fixed-width integer types
#include <cstdint>
#include <cstddef>

// Crypto constants
#define KEYHUNT_CRYPTO_BTC 1
#define KEYHUNT_CRYPTO_ETH 2

// Search modes
#define KEYHUNT_MODE_XPOINT   0
#define KEYHUNT_MODE_ADDRESS  1
#define KEYHUNT_MODE_BSGS     2
#define KEYHUNT_MODE_RMD160   3
#define KEYHUNT_MODE_PUB2RMD  4
#define KEYHUNT_MODE_MINIKEYS 5
#define KEYHUNT_MODE_VANITY   6

// Search types
#define KEYHUNT_SEARCH_UNCOMPRESS 0
#define KEYHUNT_SEARCH_COMPRESS   1
#define KEYHUNT_SEARCH_BOTH       2

// Default values
#define KEYHUNT_DEFAULT_THREADS 4
#define KEYHUNT_CPU_GRP_SIZE 1024

#endif // KEYHUNT_CONFIG_H
