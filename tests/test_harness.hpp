#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <stdexcept>
#include <sstream>

#if defined(_MSC_VER)
#pragma warning(disable: 4127) // conditional expression is constant
#endif

namespace test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline bool register_test(const std::string& name, std::function<void()> func) {
    get_registry().push_back({name, std::move(func)});
    return true;
}

class TestFailureException : public std::runtime_error {
public:
    explicit TestFailureException(const std::string& msg) : std::runtime_error(msg) {}
};

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream _oss; \
            _oss << "Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__; \
            throw ::test::TestFailureException(_oss.str()); \
        } \
    } while (0,0)

#define REQUIRE_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::ostringstream _oss; \
            _oss << "Assertion failed: (" #a " == " #b ") [" << (a) << " != " << (b) << "] at " << __FILE__ << ":" << __LINE__; \
            throw ::test::TestFailureException(_oss.str()); \
        } \
    } while (0,0)

#define REQUIRE_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            std::ostringstream _oss; \
            _oss << "Assertion failed: (" #a " != " #b ") at " << __FILE__ << ":" << __LINE__; \
            throw ::test::TestFailureException(_oss.str()); \
        } \
    } while (0,0)

#define TEST_CASE(name) \
    static void name(); \
    static const bool _registered_##name = ::test::register_test(#name, name); \
    static void name()

inline int run_all_tests() {
    int passed = 0;
    int failed = 0;

    std::cout << "==================================================\n";
    std::cout << "Running Sinew Test Suite (" << get_registry().size() << " test cases)\n";
    std::cout << "==================================================\n";

    for (const auto& test_case : get_registry()) {
        std::cout << "[ RUN      ] " << test_case.name << "\n";
        try {
            test_case.func();
            std::cout << "[       OK ] " << test_case.name << "\n";
            passed++;
        } catch (const TestFailureException& e) {
            std::cerr << "[  FAILED  ] " << test_case.name << " -> " << e.what() << "\n";
            failed++;
        } catch (const std::exception& e) {
            std::cerr << "[  FAILED  ] " << test_case.name << " (Unexpected exception: " << e.what() << ")\n";
            failed++;
        }
    }

    std::cout << "==================================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "==================================================\n";

    return (failed == 0) ? 0 : 1;
}

} // namespace test
