/**
 * @file security.h
 * @brief Security hardening utilities for keyhunt
 *
 * Provides input validation, secure parsing, rate limiting,
 * and other security measures.
 */

#ifndef KEYHUNT_CORE_SECURITY_H
#define KEYHUNT_CORE_SECURITY_H

#include <string>
#include <string_view>
#include <optional>
#include <regex>
#include <limits>
#include <cctype>
#include <algorithm>

#include "error.h"
#include "types.h"

namespace keyhunt {
namespace core {
namespace security {

/**
 * @brief Input validation result
 */
struct ValidationResult {
    bool valid;
    std::string error_message;

    operator bool() const { return valid; }

    static ValidationResult ok() {
        return {true, ""};
    }

    static ValidationResult fail(std::string msg) {
        return {false, std::move(msg)};
    }
};

/**
 * @brief Validate hex string
 */
inline ValidationResult validate_hex_string(std::string_view hex,
                                            size_t min_length = 0,
                                            size_t max_length = std::numeric_limits<size_t>::max()) {
    // Remove 0x prefix if present
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex = hex.substr(2);
    }

    if (hex.size() < min_length) {
        return ValidationResult::fail("Hex string too short");
    }

    if (hex.size() > max_length) {
        return ValidationResult::fail("Hex string too long");
    }

    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return ValidationResult::fail("Invalid hex character: " + std::string(1, c));
        }
    }

    return ValidationResult::ok();
}

/**
 * @brief Validate Bitcoin address
 */
inline ValidationResult validate_bitcoin_address(std::string_view address) {
    if (address.empty()) {
        return ValidationResult::fail("Address is empty");
    }

    if (address.size() < 26 || address.size() > 35) {
        return ValidationResult::fail("Invalid address length");
    }

    // Base58 character set
    const std::string base58_chars =
        "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

    for (char c : address) {
        if (base58_chars.find(c) == std::string::npos) {
            // Check for Bech32
            if (address.size() >= 4 &&
                (address.substr(0, 4) == "bc1q" || address.substr(0, 4) == "tb1q" ||
                 address.substr(0, 4) == "bc1p" || address.substr(0, 4) == "tb1p")) {
                // Bech32 uses different character set
                const std::string bech32_chars = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
                for (size_t i = 4; i < address.size(); ++i) {
                    if (bech32_chars.find(std::tolower(address[i])) == std::string::npos) {
                        return ValidationResult::fail("Invalid Bech32 character");
                    }
                }
                return ValidationResult::ok();
            }
            return ValidationResult::fail("Invalid Base58 character: " + std::string(1, c));
        }
    }

    // Check prefix
    char prefix = address[0];
    if (prefix != '1' && prefix != '3' && prefix != 'm' && prefix != 'n' && prefix != '2') {
        return ValidationResult::fail("Invalid address prefix");
    }

    return ValidationResult::ok();
}

/**
 * @brief Validate file path
 */
inline ValidationResult validate_file_path(std::string_view path,
                                           bool must_exist = false) {
    if (path.empty()) {
        return ValidationResult::fail("Path is empty");
    }

    // Check for null bytes
    if (path.find('\0') != std::string_view::npos) {
        return ValidationResult::fail("Path contains null byte");
    }

    // Check for directory traversal
    if (path.find("..") != std::string_view::npos) {
        return ValidationResult::fail("Path contains directory traversal");
    }

    // Length check
    if (path.size() > 4096) {
        return ValidationResult::fail("Path too long");
    }

    // TODO: Add existence check if must_exist is true
    (void)must_exist;

    return ValidationResult::ok();
}

/**
 * @brief Validate integer in range
 */
template<typename T>
inline ValidationResult validate_integer(T value, T min_value, T max_value,
                                         const std::string& name = "Value") {
    if (value < min_value) {
        return ValidationResult::fail(name + " is below minimum (" +
                                      std::to_string(min_value) + ")");
    }

    if (value > max_value) {
        return ValidationResult::fail(name + " exceeds maximum (" +
                                      std::to_string(max_value) + ")");
    }

    return ValidationResult::ok();
}

/**
 * @brief Safe string to integer conversion
 */
template<typename T>
inline std::optional<T> safe_stoi(std::string_view str) {
    if (str.empty()) {
        return std::nullopt;
    }

    // Remove leading whitespace
    size_t start = 0;
    while (start < str.size() && std::isspace(str[start])) {
        ++start;
    }
    str = str.substr(start);

    if (str.empty()) {
        return std::nullopt;
    }

    bool negative = false;
    if (str[0] == '-') {
        if (!std::is_signed_v<T>) {
            return std::nullopt;  // Can't be negative for unsigned
        }
        negative = true;
        str = str.substr(1);
    } else if (str[0] == '+') {
        str = str.substr(1);
    }

    if (str.empty()) {
        return std::nullopt;
    }

    T result = 0;
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return std::nullopt;
        }

        T digit = c - '0';

        // Check for overflow
        if (result > (std::numeric_limits<T>::max() - digit) / 10) {
            return std::nullopt;
        }

        result = result * 10 + digit;
    }

    if (negative) {
        result = -result;
    }

    return result;
}

/**
 * @brief Safe hex string to bytes conversion
 */
inline std::optional<std::vector<uint8_t>> safe_hex_to_bytes(std::string_view hex) {
    // Remove 0x prefix
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex = hex.substr(2);
    }

    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        auto high = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        int h = high(hex[i]);
        int l = high(hex[i + 1]);

        if (h < 0 || l < 0) {
            return std::nullopt;
        }

        result.push_back(static_cast<uint8_t>((h << 4) | l));
    }

    return result;
}

/**
 * @brief Sanitize string for safe display/logging
 */
inline std::string sanitize_for_display(std::string_view input,
                                        size_t max_length = 256) {
    std::string result;
    result.reserve(std::min(input.size(), max_length));

    for (size_t i = 0; i < input.size() && i < max_length; ++i) {
        char c = input[i];
        if (std::isprint(static_cast<unsigned char>(c)) && c != '\\') {
            result += c;
        } else {
            // Escape non-printable characters
            char buf[5];
            snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
            result += buf;
        }
    }

    if (input.size() > max_length) {
        result += "...";
    }

    return result;
}

/**
 * @brief Rate limiter for operations
 */
class RateLimiter {
public:
    explicit RateLimiter(size_t max_requests, std::chrono::seconds window)
        : max_requests_(max_requests)
        , window_(window)
        , request_count_(0)
        , window_start_(std::chrono::steady_clock::now()) {}

    bool try_acquire() {
        auto now = std::chrono::steady_clock::now();

        // Check if window has expired
        if (now - window_start_ >= window_) {
            // Reset window
            window_start_ = now;
            request_count_ = 0;
        }

        if (request_count_ >= max_requests_) {
            return false;
        }

        ++request_count_;
        return true;
    }

    void reset() {
        request_count_ = 0;
        window_start_ = std::chrono::steady_clock::now();
    }

private:
    size_t max_requests_;
    std::chrono::seconds window_;
    size_t request_count_;
    std::chrono::steady_clock::time_point window_start_;
};

/**
 * @brief Secure comparison (constant-time)
 */
inline bool secure_compare(const void* a, const void* b, size_t len) {
    const volatile uint8_t* pa = static_cast<const volatile uint8_t*>(a);
    const volatile uint8_t* pb = static_cast<const volatile uint8_t*>(b);

    uint8_t result = 0;
    for (size_t i = 0; i < len; ++i) {
        result |= pa[i] ^ pb[i];
    }

    return result == 0;
}

/**
 * @brief Secure memory zeroing (prevents compiler optimization)
 */
inline void secure_wipe(void* ptr, size_t len) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) {
        *p++ = 0;
    }
    // Memory barrier
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

/**
 * @brief RAII guard for secure memory wiping
 */
template<typename T>
class SecureWipeGuard {
public:
    explicit SecureWipeGuard(T* ptr) : ptr_(ptr) {}

    ~SecureWipeGuard() {
        if (ptr_) {
            secure_wipe(ptr_, sizeof(T));
        }
    }

    // Non-copyable
    SecureWipeGuard(const SecureWipeGuard&) = delete;
    SecureWipeGuard& operator=(const SecureWipeGuard&) = delete;

    // Moveable
    SecureWipeGuard(SecureWipeGuard&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    SecureWipeGuard& operator=(SecureWipeGuard&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                secure_wipe(ptr_, sizeof(T));
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void release() {
        ptr_ = nullptr;
    }

private:
    T* ptr_;
};

} // namespace security
} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_SECURITY_H
