/*
 * Keyhunt - Bitcoin/Ethereum Private Key Hunter
 * Platform abstraction layer for cross-platform compatibility
 */

#ifndef KEYHUNT_PLATFORM_H
#define KEYHUNT_PLATFORM_H

#include "config.h"

#include <cstdint>
#include <cstddef>
#include <cstring>

// Platform-specific includes
#if defined(KEYHUNT_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <process.h>
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/time.h>
#endif

#if defined(KEYHUNT_PLATFORM_LINUX)
    #include <sys/random.h>
#elif defined(KEYHUNT_PLATFORM_MACOS)
    #include <Security/SecRandom.h>
#endif

namespace keyhunt {
namespace platform {

// ============================================================================
// Threading Abstractions
// ============================================================================

#if defined(KEYHUNT_PLATFORM_WINDOWS)
    using thread_handle_t = HANDLE;
    using mutex_handle_t = HANDLE;
    using thread_id_t = DWORD;

    inline mutex_handle_t create_mutex() {
        return CreateMutex(NULL, FALSE, NULL);
    }

    inline void destroy_mutex(mutex_handle_t& mtx) {
        if (mtx != NULL) {
            CloseHandle(mtx);
            mtx = NULL;
        }
    }

    inline void lock_mutex(mutex_handle_t mtx) {
        WaitForSingleObject(mtx, INFINITE);
    }

    inline void unlock_mutex(mutex_handle_t mtx) {
        ReleaseMutex(mtx);
    }

    inline void sleep_ms(int milliseconds) {
        Sleep(milliseconds);
    }

    inline int get_processor_count() {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return static_cast<int>(sysinfo.dwNumberOfProcessors);
    }

#else  // POSIX systems (Linux, macOS)
    using thread_handle_t = pthread_t;
    using mutex_handle_t = pthread_mutex_t;
    using thread_id_t = pthread_t;

    inline mutex_handle_t create_mutex() {
        pthread_mutex_t mtx;
        pthread_mutex_init(&mtx, NULL);
        return mtx;
    }

    inline void destroy_mutex(mutex_handle_t& mtx) {
        pthread_mutex_destroy(&mtx);
    }

    inline void lock_mutex(mutex_handle_t& mtx) {
        pthread_mutex_lock(&mtx);
    }

    inline void unlock_mutex(mutex_handle_t& mtx) {
        pthread_mutex_unlock(&mtx);
    }

    inline void sleep_ms(int milliseconds) {
        usleep(milliseconds * 1000);
    }

    inline int get_processor_count() {
        return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    }
#endif

// ============================================================================
// RAII Mutex Lock
// ============================================================================

class ScopedLock {
public:
    explicit ScopedLock(mutex_handle_t& mtx) : m_mutex(mtx) {
        lock_mutex(m_mutex);
    }

    ~ScopedLock() {
        unlock_mutex(m_mutex);
    }

    // Non-copyable
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    mutex_handle_t& m_mutex;
};

// ============================================================================
// Secure Random Number Generation
// ============================================================================

inline bool secure_random(void* buffer, size_t size) {
#if defined(KEYHUNT_PLATFORM_WINDOWS)
    HCRYPTPROV hProvider = 0;
    if (!CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL,
                               CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        return false;
    }
    BOOL result = CryptGenRandom(hProvider, static_cast<DWORD>(size),
                                  static_cast<BYTE*>(buffer));
    CryptReleaseContext(hProvider, 0);
    return result != FALSE;

#elif defined(KEYHUNT_PLATFORM_LINUX)
    ssize_t bytes_read = getrandom(buffer, size, 0);
    return bytes_read == static_cast<ssize_t>(size);

#elif defined(KEYHUNT_PLATFORM_MACOS)
    return SecRandomCopyBytes(kSecRandomDefault, size, buffer) == errSecSuccess;

#else
    // Fallback: read from /dev/urandom
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) return false;
    size_t read = fread(buffer, 1, size, f);
    fclose(f);
    return read == size;
#endif
}

// ============================================================================
// High-Resolution Timer
// ============================================================================

inline uint64_t get_time_ms() {
#if defined(KEYHUNT_PLATFORM_WINDOWS)
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
           static_cast<uint64_t>(tv.tv_usec) / 1000ULL;
#endif
}

// ============================================================================
// Memory Utilities
// ============================================================================

inline void secure_zero(void* buffer, size_t size) {
#if defined(KEYHUNT_PLATFORM_WINDOWS)
    SecureZeroMemory(buffer, size);
#else
    volatile unsigned char* p = static_cast<volatile unsigned char*>(buffer);
    while (size--) {
        *p++ = 0;
    }
#endif
}

// ============================================================================
// Console Colors (for output formatting)
// ============================================================================

namespace color {
#if defined(KEYHUNT_PLATFORM_WINDOWS)
    inline void set_green() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    inline void set_red() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }
    inline void set_yellow() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    inline void reset() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
#else
    inline void set_green()  { printf("\033[1;32m"); }
    inline void set_red()    { printf("\033[1;31m"); }
    inline void set_yellow() { printf("\033[1;33m"); }
    inline void reset()      { printf("\033[0m"); }
#endif
} // namespace color

} // namespace platform
} // namespace keyhunt

#endif // KEYHUNT_PLATFORM_H
