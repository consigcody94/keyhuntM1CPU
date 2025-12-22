/**
 * @file logger.h
 * @brief Modern C++ logging framework for keyhunt
 *
 * Provides thread-safe, high-performance logging with multiple output targets,
 * structured logging support, and zero-allocation fast path for disabled levels.
 */

#ifndef KEYHUNT_CORE_LOGGER_H
#define KEYHUNT_CORE_LOGGER_H

#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdio>

namespace keyhunt {
namespace core {

/**
 * @brief Log severity levels
 */
enum class LogLevel : uint8_t {
    TRACE = 0,    ///< Detailed tracing information
    DEBUG = 1,    ///< Debug information for development
    INFO = 2,     ///< General informational messages
    WARN = 3,     ///< Warning conditions
    ERROR = 4,    ///< Error conditions
    FATAL = 5,    ///< Fatal errors, program will terminate
    OFF = 6       ///< Logging disabled
};

/**
 * @brief Convert log level to string representation
 */
constexpr std::string_view to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::OFF:   return "OFF  ";
    }
    return "UNKNOWN";
}

/**
 * @brief ANSI color codes for terminal output
 */
namespace colors {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* RED     = "\033[1;31m";
    constexpr const char* GREEN   = "\033[1;32m";
    constexpr const char* YELLOW  = "\033[1;33m";
    constexpr const char* BLUE    = "\033[1;34m";
    constexpr const char* MAGENTA = "\033[1;35m";
    constexpr const char* CYAN    = "\033[1;36m";
    constexpr const char* WHITE   = "\033[1;37m";
    constexpr const char* GRAY    = "\033[0;37m";
}

/**
 * @brief Get color for log level
 */
constexpr const char* level_color(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return colors::GRAY;
        case LogLevel::DEBUG: return colors::CYAN;
        case LogLevel::INFO:  return colors::GREEN;
        case LogLevel::WARN:  return colors::YELLOW;
        case LogLevel::ERROR: return colors::RED;
        case LogLevel::FATAL: return colors::MAGENTA;
        default: return colors::RESET;
    }
}

/**
 * @brief Log entry with metadata
 */
struct LogEntry {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string_view file;
    int line;
    std::string_view function;
    std::string message;
    std::thread::id thread_id;
};

/**
 * @brief Abstract log sink interface
 */
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
};

/**
 * @brief Console log sink with color support
 */
class ConsoleSink : public LogSink {
public:
    explicit ConsoleSink(bool use_colors = true, FILE* output = stderr)
        : use_colors_(use_colors), output_(output) {}

    void write(const LogEntry& entry) override {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        std::lock_guard<std::mutex> lock(mutex_);

        if (use_colors_) {
            fprintf(output_, "%s[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s%s%s] %s%s\n",
                colors::GRAY,
                tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                static_cast<int>(ms.count()),
                level_color(entry.level),
                std::string(to_string(entry.level)).c_str(),
                colors::RESET,
                colors::RESET,
                entry.message.c_str());
        } else {
            fprintf(output_, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] %s\n",
                tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                static_cast<int>(ms.count()),
                std::string(to_string(entry.level)).c_str(),
                entry.message.c_str());
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        fflush(output_);
    }

private:
    bool use_colors_;
    FILE* output_;
    std::mutex mutex_;
};

/**
 * @brief File log sink with rotation support
 */
class FileSink : public LogSink {
public:
    explicit FileSink(const std::string& filename,
                      size_t max_size = 10 * 1024 * 1024,  // 10MB default
                      int max_files = 5)
        : filename_(filename)
        , max_size_(max_size)
        , max_files_(max_files)
        , current_size_(0) {
        open_file();
    }

    void write(const LogEntry& entry) override {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        std::ostringstream oss;
        oss << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count()
            << "] [" << to_string(entry.level) << "] "
            << "[" << entry.file << ":" << entry.line << "] "
            << entry.message << "\n";

        std::string line = oss.str();

        std::lock_guard<std::mutex> lock(mutex_);

        if (current_size_ + line.size() > max_size_) {
            rotate();
        }

        if (file_.is_open()) {
            file_ << line;
            current_size_ += line.size();
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }

private:
    void open_file() {
        file_.open(filename_, std::ios::app);
        if (file_.is_open()) {
            file_.seekp(0, std::ios::end);
            current_size_ = file_.tellp();
        }
    }

    void rotate() {
        file_.close();

        // Remove oldest file
        std::string oldest = filename_ + "." + std::to_string(max_files_);
        std::remove(oldest.c_str());

        // Shift existing files
        for (int i = max_files_ - 1; i >= 1; --i) {
            std::string old_name = filename_ + "." + std::to_string(i);
            std::string new_name = filename_ + "." + std::to_string(i + 1);
            std::rename(old_name.c_str(), new_name.c_str());
        }

        // Rename current file
        std::string backup = filename_ + ".1";
        std::rename(filename_.c_str(), backup.c_str());

        // Open new file
        current_size_ = 0;
        open_file();
    }

    std::string filename_;
    size_t max_size_;
    int max_files_;
    size_t current_size_;
    std::ofstream file_;
    std::mutex mutex_;
};

/**
 * @brief Main logger class - singleton pattern
 */
class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void set_level(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
    }

    LogLevel level() const {
        return level_.load(std::memory_order_relaxed);
    }

    bool is_enabled(LogLevel level) const {
        return level >= level_.load(std::memory_order_relaxed);
    }

    void add_sink(std::shared_ptr<LogSink> sink) {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_.push_back(std::move(sink));
    }

    void clear_sinks() {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_.clear();
    }

    void log(LogLevel level, std::string_view file, int line,
             std::string_view function, const std::string& message) {
        if (!is_enabled(level)) return;

        LogEntry entry{
            level,
            std::chrono::system_clock::now(),
            file,
            line,
            function,
            message,
            std::this_thread::get_id()
        };

        std::lock_guard<std::mutex> lock(sinks_mutex_);
        for (auto& sink : sinks_) {
            sink->write(entry);
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        for (auto& sink : sinks_) {
            sink->flush();
        }
    }

private:
    Logger() : level_(LogLevel::INFO) {
        // Default console sink
        add_sink(std::make_shared<ConsoleSink>());
    }

    std::atomic<LogLevel> level_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::mutex sinks_mutex_;
};

/**
 * @brief Stream-style log builder for efficient message construction
 */
class LogStream {
public:
    LogStream(LogLevel level, std::string_view file, int line, std::string_view function)
        : level_(level), file_(file), line_(line), function_(function) {}

    ~LogStream() {
        Logger::instance().log(level_, file_, line_, function_, oss_.str());
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        oss_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::string_view file_;
    int line_;
    std::string_view function_;
    std::ostringstream oss_;
};

/**
 * @brief Null stream for disabled log levels (zero cost)
 */
class NullStream {
public:
    template<typename T>
    NullStream& operator<<(const T&) { return *this; }
};

} // namespace core
} // namespace keyhunt

// Macro helpers for extracting just filename from __FILE__
#define KEYHUNT_FILENAME \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
     (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))

// Main logging macros with compile-time level check
#define LOG_IMPL(level) \
    keyhunt::core::Logger::instance().is_enabled(level) && \
    keyhunt::core::LogStream(level, KEYHUNT_FILENAME, __LINE__, __func__)

#define LOG_TRACE LOG_IMPL(keyhunt::core::LogLevel::TRACE)
#define LOG_DEBUG LOG_IMPL(keyhunt::core::LogLevel::DEBUG)
#define LOG_INFO  LOG_IMPL(keyhunt::core::LogLevel::INFO)
#define LOG_WARN  LOG_IMPL(keyhunt::core::LogLevel::WARN)
#define LOG_ERROR LOG_IMPL(keyhunt::core::LogLevel::ERROR)
#define LOG_FATAL LOG_IMPL(keyhunt::core::LogLevel::FATAL)

// Convenience function for setting log level from string
inline keyhunt::core::LogLevel parse_log_level(std::string_view str) {
    if (str == "trace" || str == "TRACE") return keyhunt::core::LogLevel::TRACE;
    if (str == "debug" || str == "DEBUG") return keyhunt::core::LogLevel::DEBUG;
    if (str == "info"  || str == "INFO")  return keyhunt::core::LogLevel::INFO;
    if (str == "warn"  || str == "WARN")  return keyhunt::core::LogLevel::WARN;
    if (str == "error" || str == "ERROR") return keyhunt::core::LogLevel::ERROR;
    if (str == "fatal" || str == "FATAL") return keyhunt::core::LogLevel::FATAL;
    if (str == "off"   || str == "OFF")   return keyhunt::core::LogLevel::OFF;
    return keyhunt::core::LogLevel::INFO; // default
}

#endif // KEYHUNT_CORE_LOGGER_H
