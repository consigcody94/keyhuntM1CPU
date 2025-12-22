/**
 * @file config.h
 * @brief Configuration management for keyhunt
 *
 * Provides type-safe configuration with validation, environment variable support,
 * JSON/YAML file loading, and command-line argument parsing integration.
 */

#ifndef KEYHUNT_CORE_CONFIG_H
#define KEYHUNT_CORE_CONFIG_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <optional>
#include <vector>
#include <functional>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <mutex>

#include "error.h"

namespace keyhunt {
namespace core {

/**
 * @brief Configuration value types
 */
using ConfigValue = std::variant<
    bool,
    int64_t,
    double,
    std::string,
    std::vector<std::string>
>;

/**
 * @brief Search mode configuration
 */
enum class SearchMode : uint8_t {
    XPOINT = 0,
    ADDRESS = 1,
    BSGS = 2,
    RMD160 = 3,
    PUB2RMD = 4,
    MINIKEYS = 5,
    VANITY = 6
};

/**
 * @brief Key search type (compressed/uncompressed)
 */
enum class KeySearchType : uint8_t {
    UNCOMPRESSED = 0,
    COMPRESSED = 1,
    BOTH = 2
};

/**
 * @brief BSGS search direction
 */
enum class BSGSMode : uint8_t {
    SEQUENTIAL = 0,
    BACKWARD = 1,
    BOTH = 2,
    RANDOM = 3,
    DANCE = 4
};

/**
 * @brief Main configuration class
 */
class Config {
public:
    /**
     * @brief Get global configuration instance
     */
    static Config& instance() {
        static Config instance;
        return instance;
    }

    // Search Configuration
    SearchMode search_mode = SearchMode::ADDRESS;
    KeySearchType key_search_type = KeySearchType::COMPRESSED;
    BSGSMode bsgs_mode = BSGSMode::SEQUENTIAL;

    // Range Configuration
    int bit_range = 66;
    std::string range_start;
    std::string range_end;
    std::string stride = "1";

    // File Configuration
    std::string input_file = "addresses.txt";
    std::string output_file = "KEYFOUNDKEYFOUND.txt";
    std::string bloom_file;
    std::string checkpoint_file = "keyhunt.checkpoint";

    // Performance Configuration
    int num_threads = 0;  // 0 = auto-detect
    int k_factor = 1;
    uint64_t bsgs_m = 4194304;  // 2^22 default
    bool use_gpu = false;
    int gpu_device = 0;

    // BSGS Specific
    uint64_t baby_step_workload = 1048576;  // 1M per thread
    int bloom_multiplier = 1;

    // Output Configuration
    int status_interval_seconds = 30;
    bool quiet_mode = false;
    bool skip_checksum = false;
    bool random_start = false;

    // Notification Configuration
    std::string discord_webhook_url;
    int discord_update_interval = 600;  // 10 minutes

    // Checkpointing
    bool enable_checkpoint = true;
    int checkpoint_interval = 300;  // 5 minutes

    /**
     * @brief Load configuration from environment variables
     */
    void load_from_env() {
        auto get_env = [](const char* name) -> std::optional<std::string> {
            const char* val = std::getenv(name);
            return val ? std::optional<std::string>(val) : std::nullopt;
        };

        if (auto val = get_env("KEYHUNT_THREADS")) {
            num_threads = std::stoi(*val);
        }
        if (auto val = get_env("KEYHUNT_BIT_RANGE")) {
            bit_range = std::stoi(*val);
        }
        if (auto val = get_env("KEYHUNT_K_FACTOR")) {
            k_factor = std::stoi(*val);
        }
        if (auto val = get_env("KEYHUNT_INPUT_FILE")) {
            input_file = *val;
        }
        if (auto val = get_env("KEYHUNT_DISCORD_WEBHOOK")) {
            discord_webhook_url = *val;
        }
        if (auto val = get_env("KEYHUNT_GPU")) {
            use_gpu = (*val == "1" || *val == "true" || *val == "yes");
        }
        if (auto val = get_env("KEYHUNT_GPU_DEVICE")) {
            gpu_device = std::stoi(*val);
        }
    }

    /**
     * @brief Load configuration from JSON file
     */
    bool load_from_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // Simple JSON parser for our config format
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        auto parse_string = [&content](const std::string& key) -> std::optional<std::string> {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return std::nullopt;

            pos = content.find(':', pos);
            if (pos == std::string::npos) return std::nullopt;

            size_t start = content.find('"', pos + 1);
            if (start == std::string::npos) return std::nullopt;

            size_t end = content.find('"', start + 1);
            if (end == std::string::npos) return std::nullopt;

            return content.substr(start + 1, end - start - 1);
        };

        auto parse_int = [&content](const std::string& key) -> std::optional<int64_t> {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return std::nullopt;

            pos = content.find(':', pos);
            if (pos == std::string::npos) return std::nullopt;

            // Skip whitespace
            pos++;
            while (pos < content.size() && std::isspace(content[pos])) pos++;

            if (pos >= content.size()) return std::nullopt;

            try {
                return std::stoll(content.substr(pos));
            } catch (...) {
                return std::nullopt;
            }
        };

        auto parse_bool = [&content](const std::string& key) -> std::optional<bool> {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return std::nullopt;

            pos = content.find(':', pos);
            if (pos == std::string::npos) return std::nullopt;

            if (content.find("true", pos) != std::string::npos &&
                content.find("true", pos) < content.find(',', pos)) {
                return true;
            }
            if (content.find("false", pos) != std::string::npos &&
                content.find("false", pos) < content.find(',', pos)) {
                return false;
            }
            return std::nullopt;
        };

        // Parse configuration values
        if (auto val = parse_int("threads")) num_threads = *val;
        if (auto val = parse_int("bit_range")) bit_range = *val;
        if (auto val = parse_int("k_factor")) k_factor = *val;
        if (auto val = parse_int("bsgs_m")) bsgs_m = *val;
        if (auto val = parse_int("status_interval")) status_interval_seconds = *val;
        if (auto val = parse_string("input_file")) input_file = *val;
        if (auto val = parse_string("output_file")) output_file = *val;
        if (auto val = parse_string("range_start")) range_start = *val;
        if (auto val = parse_string("range_end")) range_end = *val;
        if (auto val = parse_string("discord_webhook")) discord_webhook_url = *val;
        if (auto val = parse_bool("use_gpu")) use_gpu = *val;
        if (auto val = parse_bool("quiet_mode")) quiet_mode = *val;
        if (auto val = parse_bool("random_start")) random_start = *val;

        return true;
    }

    /**
     * @brief Save configuration to JSON file
     */
    bool save_to_file(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << "{\n";
        file << "  \"threads\": " << num_threads << ",\n";
        file << "  \"bit_range\": " << bit_range << ",\n";
        file << "  \"k_factor\": " << k_factor << ",\n";
        file << "  \"bsgs_m\": " << bsgs_m << ",\n";
        file << "  \"status_interval\": " << status_interval_seconds << ",\n";
        file << "  \"input_file\": \"" << input_file << "\",\n";
        file << "  \"output_file\": \"" << output_file << "\",\n";
        if (!range_start.empty()) {
            file << "  \"range_start\": \"" << range_start << "\",\n";
        }
        if (!range_end.empty()) {
            file << "  \"range_end\": \"" << range_end << "\",\n";
        }
        if (!discord_webhook_url.empty()) {
            file << "  \"discord_webhook\": \"" << discord_webhook_url << "\",\n";
        }
        file << "  \"use_gpu\": " << (use_gpu ? "true" : "false") << ",\n";
        file << "  \"quiet_mode\": " << (quiet_mode ? "true" : "false") << ",\n";
        file << "  \"random_start\": " << (random_start ? "true" : "false") << "\n";
        file << "}\n";

        return true;
    }

    /**
     * @brief Validate configuration
     */
    void validate() const {
        if (bit_range < 1 || bit_range > 256) {
            throw ConfigException("bit_range must be between 1 and 256");
        }
        if (k_factor < 1) {
            throw ConfigException("k_factor must be >= 1");
        }
        if (num_threads < 0) {
            throw ConfigException("num_threads must be >= 0 (0 = auto)");
        }
        if (status_interval_seconds < 1) {
            throw ConfigException("status_interval_seconds must be >= 1");
        }
    }

    /**
     * @brief Get effective thread count
     */
    int effective_threads() const {
        if (num_threads > 0) return num_threads;

        // Auto-detect
        int count = std::thread::hardware_concurrency();
        return count > 0 ? count : 4;  // Fallback to 4
    }

    /**
     * @brief Print configuration summary
     */
    void print_summary() const {
        printf("\n=== Configuration Summary ===\n");
        printf("Search Mode:     %d\n", static_cast<int>(search_mode));
        printf("Bit Range:       %d\n", bit_range);
        printf("Threads:         %d\n", effective_threads());
        printf("K-Factor:        %d\n", k_factor);
        printf("BSGS M:          %lu\n", bsgs_m);
        printf("Input File:      %s\n", input_file.c_str());
        printf("GPU:             %s\n", use_gpu ? "enabled" : "disabled");
        if (!discord_webhook_url.empty()) {
            printf("Discord:         enabled\n");
        }
        printf("=============================\n\n");
    }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};

/**
 * @brief Command-line argument parser
 */
class ArgParser {
public:
    ArgParser() = default;

    void add_flag(char short_name, const std::string& long_name,
                  const std::string& description, bool* target) {
        flags_[short_name] = {long_name, description, target};
    }

    void add_option(char short_name, const std::string& long_name,
                    const std::string& description, std::string* target) {
        options_[short_name] = {long_name, description, target};
    }

    void add_int_option(char short_name, const std::string& long_name,
                        const std::string& description, int* target) {
        int_options_[short_name] = {long_name, description, target};
    }

    bool parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
                // Short option
                char key = arg[1];

                if (flags_.count(key)) {
                    *flags_[key].target = true;
                    continue;
                }

                if (options_.count(key)) {
                    if (i + 1 >= argc) {
                        error_ = "Missing value for -" + std::string(1, key);
                        return false;
                    }
                    *options_[key].target = argv[++i];
                    continue;
                }

                if (int_options_.count(key)) {
                    if (i + 1 >= argc) {
                        error_ = "Missing value for -" + std::string(1, key);
                        return false;
                    }
                    try {
                        *int_options_[key].target = std::stoi(argv[++i]);
                    } catch (...) {
                        error_ = "Invalid integer for -" + std::string(1, key);
                        return false;
                    }
                    continue;
                }

                error_ = "Unknown option: -" + std::string(1, key);
                return false;
            }

            // Positional arguments
            positional_.push_back(arg);
        }

        return true;
    }

    const std::string& error() const {
        return error_;
    }

    const std::vector<std::string>& positional() const {
        return positional_;
    }

    void print_help(const std::string& program_name) const {
        printf("Usage: %s [OPTIONS]\n\n", program_name.c_str());
        printf("Options:\n");

        for (const auto& [key, flag] : flags_) {
            printf("  -%c, --%s\n", key, flag.long_name.c_str());
            printf("      %s\n", flag.description.c_str());
        }

        for (const auto& [key, opt] : options_) {
            printf("  -%c, --%s <value>\n", key, opt.long_name.c_str());
            printf("      %s\n", opt.description.c_str());
        }

        for (const auto& [key, opt] : int_options_) {
            printf("  -%c, --%s <number>\n", key, opt.long_name.c_str());
            printf("      %s\n", opt.description.c_str());
        }
    }

private:
    struct Flag {
        std::string long_name;
        std::string description;
        bool* target;
    };

    struct Option {
        std::string long_name;
        std::string description;
        std::string* target;
    };

    struct IntOption {
        std::string long_name;
        std::string description;
        int* target;
    };

    std::unordered_map<char, Flag> flags_;
    std::unordered_map<char, Option> options_;
    std::unordered_map<char, IntOption> int_options_;
    std::vector<std::string> positional_;
    std::string error_;
};

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_CONFIG_H
