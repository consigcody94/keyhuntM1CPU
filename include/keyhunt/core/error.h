/**
 * @file error.h
 * @brief Modern exception hierarchy and error handling for keyhunt
 *
 * Provides a comprehensive exception hierarchy with error codes,
 * stack traces (when available), and structured error information.
 */

#ifndef KEYHUNT_CORE_ERROR_H
#define KEYHUNT_CORE_ERROR_H

#include <exception>
#include <string>
#include <string_view>
#include <sstream>
#include <memory>
#include <optional>
#include <cstring>

namespace keyhunt {
namespace core {

/**
 * @brief Error categories for classification
 */
enum class ErrorCategory : uint8_t {
    UNKNOWN = 0,
    IO,           ///< File I/O errors
    MEMORY,       ///< Memory allocation errors
    CRYPTO,       ///< Cryptographic operation errors
    PARSE,        ///< Parsing errors
    NETWORK,      ///< Network errors
    CONFIG,       ///< Configuration errors
    VALIDATION,   ///< Input validation errors
    RUNTIME,      ///< General runtime errors
    SYSTEM        ///< System/OS errors
};

/**
 * @brief Convert error category to string
 */
constexpr std::string_view to_string(ErrorCategory cat) {
    switch (cat) {
        case ErrorCategory::IO:         return "I/O";
        case ErrorCategory::MEMORY:     return "Memory";
        case ErrorCategory::CRYPTO:     return "Crypto";
        case ErrorCategory::PARSE:      return "Parse";
        case ErrorCategory::NETWORK:    return "Network";
        case ErrorCategory::CONFIG:     return "Config";
        case ErrorCategory::VALIDATION: return "Validation";
        case ErrorCategory::RUNTIME:    return "Runtime";
        case ErrorCategory::SYSTEM:     return "System";
        default:                        return "Unknown";
    }
}

/**
 * @brief Source location information
 */
struct SourceLocation {
    const char* file;
    int line;
    const char* function;

    static SourceLocation current(const char* file = __builtin_FILE(),
                                   int line = __builtin_LINE(),
                                   const char* function = __builtin_FUNCTION()) {
        return {file, line, function};
    }
};

/**
 * @brief Base exception class for keyhunt
 */
class Exception : public std::exception {
public:
    explicit Exception(std::string message,
                       ErrorCategory category = ErrorCategory::UNKNOWN,
                       SourceLocation location = SourceLocation::current())
        : message_(std::move(message))
        , category_(category)
        , location_(location) {
        build_what();
    }

    Exception(std::string message,
              std::exception_ptr nested,
              ErrorCategory category = ErrorCategory::UNKNOWN,
              SourceLocation location = SourceLocation::current())
        : message_(std::move(message))
        , category_(category)
        , location_(location)
        , nested_(nested) {
        build_what();
    }

    const char* what() const noexcept override {
        return what_.c_str();
    }

    const std::string& message() const noexcept {
        return message_;
    }

    ErrorCategory category() const noexcept {
        return category_;
    }

    const SourceLocation& location() const noexcept {
        return location_;
    }

    std::exception_ptr nested() const noexcept {
        return nested_;
    }

    bool has_nested() const noexcept {
        return nested_ != nullptr;
    }

private:
    void build_what() {
        std::ostringstream oss;
        oss << "[" << to_string(category_) << "] " << message_;

        // Add location info in debug builds
#ifndef NDEBUG
        if (location_.file) {
            // Extract just filename
            const char* file = strrchr(location_.file, '/');
            if (!file) file = strrchr(location_.file, '\\');
            file = file ? file + 1 : location_.file;

            oss << " (at " << file << ":" << location_.line;
            if (location_.function) {
                oss << " in " << location_.function;
            }
            oss << ")";
        }
#endif

        if (nested_) {
            try {
                std::rethrow_exception(nested_);
            } catch (const std::exception& e) {
                oss << "\n  Caused by: " << e.what();
            } catch (...) {
                oss << "\n  Caused by: unknown exception";
            }
        }

        what_ = oss.str();
    }

    std::string message_;
    ErrorCategory category_;
    SourceLocation location_;
    std::exception_ptr nested_;
    std::string what_;
};

// Specific exception types

class IOException : public Exception {
public:
    explicit IOException(std::string message,
                         SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::IO, location) {}
};

class MemoryException : public Exception {
public:
    explicit MemoryException(std::string message,
                             SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::MEMORY, location) {}
};

class CryptoException : public Exception {
public:
    explicit CryptoException(std::string message,
                             SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::CRYPTO, location) {}
};

class ParseException : public Exception {
public:
    explicit ParseException(std::string message,
                            SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::PARSE, location) {}
};

class NetworkException : public Exception {
public:
    explicit NetworkException(std::string message,
                              SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::NETWORK, location) {}
};

class ConfigException : public Exception {
public:
    explicit ConfigException(std::string message,
                             SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::CONFIG, location) {}
};

class ValidationException : public Exception {
public:
    explicit ValidationException(std::string message,
                                 SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::VALIDATION, location) {}
};

class RuntimeException : public Exception {
public:
    explicit RuntimeException(std::string message,
                              SourceLocation location = SourceLocation::current())
        : Exception(std::move(message), ErrorCategory::RUNTIME, location) {}
};

class SystemException : public Exception {
public:
    explicit SystemException(std::string message,
                             int error_code = 0,
                             SourceLocation location = SourceLocation::current())
        : Exception(build_message(message, error_code), ErrorCategory::SYSTEM, location)
        , error_code_(error_code) {}

    int error_code() const noexcept {
        return error_code_;
    }

private:
    static std::string build_message(const std::string& msg, int code) {
        if (code == 0) return msg;
        std::ostringstream oss;
        oss << msg << " (errno=" << code << ": " << strerror(code) << ")";
        return oss.str();
    }

    int error_code_;
};

/**
 * @brief Result type for operations that can fail
 *
 * Inspired by Rust's Result<T, E> pattern
 */
template<typename T, typename E = Exception>
class Result {
public:
    // Success case
    static Result ok(T value) {
        return Result(std::move(value));
    }

    // Error case
    static Result err(E error) {
        return Result(std::move(error), false);
    }

    bool is_ok() const noexcept {
        return has_value_;
    }

    bool is_err() const noexcept {
        return !has_value_;
    }

    T& value() & {
        if (!has_value_) throw error_;
        return value_;
    }

    const T& value() const& {
        if (!has_value_) throw error_;
        return value_;
    }

    T&& value() && {
        if (!has_value_) throw error_;
        return std::move(value_);
    }

    E& error() & {
        return error_;
    }

    const E& error() const& {
        return error_;
    }

    T value_or(T default_value) const {
        return has_value_ ? value_ : std::move(default_value);
    }

    template<typename F>
    auto map(F&& f) -> Result<decltype(f(std::declval<T>())), E> {
        using U = decltype(f(std::declval<T>()));
        if (has_value_) {
            return Result<U, E>::ok(f(value_));
        }
        return Result<U, E>::err(error_);
    }

    // Implicit conversion to bool
    explicit operator bool() const noexcept {
        return has_value_;
    }

private:
    explicit Result(T value)
        : value_(std::move(value)), has_value_(true) {}

    explicit Result(E error, bool)
        : error_(std::move(error)), has_value_(false) {}

    T value_{};
    E error_{""};
    bool has_value_;
};

// Helper macros for error handling
#define KEYHUNT_TRY(expr) \
    do { \
        auto result_ = (expr); \
        if (!result_.is_ok()) { \
            throw result_.error(); \
        } \
    } while (0)

#define KEYHUNT_THROW(ExceptionType, message) \
    throw ExceptionType(message, keyhunt::core::SourceLocation::current())

#define KEYHUNT_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            KEYHUNT_THROW(keyhunt::core::RuntimeException, \
                std::string("Assertion failed: ") + message); \
        } \
    } while (0)

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_ERROR_H
