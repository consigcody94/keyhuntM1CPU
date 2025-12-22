/**
 * @file test_main.cpp
 * @brief Main entry point for keyhunt unit tests
 *
 * Uses a minimal test framework that doesn't require external dependencies.
 */

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <cstring>

// Simple test framework
namespace test {

struct TestCase {
    std::string name;
    std::function<bool()> func;
    std::string file;
    int line;
};

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }

    void add_test(const std::string& name, std::function<bool()> func,
                  const char* file, int line) {
        tests_.push_back({name, func, file, line});
    }

    int run_all() {
        int passed = 0;
        int failed = 0;

        std::cout << "\n========================================\n";
        std::cout << "Running " << tests_.size() << " tests\n";
        std::cout << "========================================\n\n";

        auto start = std::chrono::steady_clock::now();

        for (const auto& test : tests_) {
            std::cout << "[ RUN      ] " << test.name << std::endl;

            auto test_start = std::chrono::steady_clock::now();
            bool success = false;

            try {
                success = test.func();
            } catch (const std::exception& e) {
                std::cout << "  Exception: " << e.what() << std::endl;
                success = false;
            } catch (...) {
                std::cout << "  Unknown exception" << std::endl;
                success = false;
            }

            auto test_end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                test_end - test_start).count();

            if (success) {
                std::cout << "[       OK ] " << test.name
                          << " (" << duration << " ms)" << std::endl;
                ++passed;
            } else {
                std::cout << "[  FAILED  ] " << test.name
                          << " at " << test.file << ":" << test.line << std::endl;
                ++failed;
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();

        std::cout << "\n========================================\n";
        std::cout << "Tests finished in " << total_duration << " ms\n";
        std::cout << "Passed: " << passed << "/" << tests_.size() << "\n";
        if (failed > 0) {
            std::cout << "FAILED: " << failed << "\n";
        }
        std::cout << "========================================\n\n";

        return failed > 0 ? 1 : 0;
    }

private:
    std::vector<TestCase> tests_;
};

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<bool()> func,
                  const char* file, int line) {
        TestRunner::instance().add_test(name, func, file, line);
    }
};

} // namespace test

#define TEST(suite, name) \
    bool suite##_##name##_impl(); \
    static test::TestRegistrar suite##_##name##_registrar( \
        #suite "." #name, suite##_##name##_impl, __FILE__, __LINE__); \
    bool suite##_##name##_impl()

#define EXPECT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cout << "  EXPECT_TRUE failed: " << #expr << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_FALSE(expr) \
    do { \
        if (expr) { \
            std::cout << "  EXPECT_FALSE failed: " << #expr << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cout << "  EXPECT_EQ failed: " << #a << " != " << #b << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            std::cout << "  EXPECT_NE failed: " << #a << " == " << #b << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_LT(a, b) \
    do { \
        if (!((a) < (b))) { \
            std::cout << "  EXPECT_LT failed: " << #a << " >= " << #b << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_LE(a, b) \
    do { \
        if (!((a) <= (b))) { \
            std::cout << "  EXPECT_LE failed: " << #a << " > " << #b << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_GT(a, b) \
    do { \
        if (!((a) > (b))) { \
            std::cout << "  EXPECT_GT failed: " << #a << " <= " << #b << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_GE(a, b) \
    do { \
        if (!((a) >= (b))) { \
            std::cout << "  EXPECT_GE failed: " << #a << " < " << #b << std::endl; \
            return false; \
        } \
    } while (0)

#define EXPECT_NEAR(a, b, epsilon) \
    do { \
        if (std::abs((a) - (b)) > (epsilon)) { \
            std::cout << "  EXPECT_NEAR failed: |" << #a << " - " << #b << "| > " << epsilon << std::endl; \
            return false; \
        } \
    } while (0)

// Include actual tests
#include "test_types.cpp"
#include "test_memory.cpp"
#include "test_thread_pool.cpp"
#include "test_bloom_filter.cpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║     Keyhunt Unit Test Suite          ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    return test::TestRunner::instance().run_all();
}
