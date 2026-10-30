#ifndef CPP_KANA2_TESTS_HARNESS_H
#define CPP_KANA2_TESTS_HARNESS_H

// A ~90-line assertion harness instead of a vendored test framework.
//
// Why: this library is a few thousand lines, and Catch2's amalgamated headers are ~25,000 lines of
// third-party code plus their own licence file and a permanent exemption in the format gate. What the
// suite actually needs is "run every case, print the failures, exit non-zero" -- which is this file. It
// is not a framework and must not grow into one: if the suite ever needs tags, sections or fixtures,
// take Catch2 then, with its notices, in its own commit.
//
// A test that cannot fail is not a test: TEST_CASE registers a function, CHECK/CHECK_EQ record a
// failure with file:line, and RunTestCases() returns 1 when anything failed.

#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

namespace harness
{

    struct TestCase {
        const char *name;
        void (*body)();
    };

    inline std::vector<TestCase> &Cases() {
        static std::vector<TestCase> cases;
        return cases;
    }

    inline int &Failures() {
        static int failures = 0;
        return failures;
    }

    inline const char *&CurrentCase() {
        static const char *current = "";
        return current;
    }

    struct Registrar {
        Registrar(const char *name, void (*body)()) { Cases().push_back(TestCase{name, body}); }
    };

    inline void ReportFailure(const char *file, int line, const std::string &message) {
        ++Failures();
        std::printf("FAIL %s (%s:%d): %s\n", CurrentCase(), file, line, message.c_str());
    }

    inline std::string Spell(const std::string &value) {
        std::string out = "\"";
        for (const char c : value) {
            if (c == '\n') {
                out += "\\n";
            } else if (c == '\t') {
                out += "\\t";
            } else {
                out += c;
            }
        }
        out += "\"";
        return out;
    }

    inline std::string Spell(const char *value) {
        return std::string("\"") + (value != nullptr ? value : "(null)") + "\"";
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    inline std::string Spell(T value) {
        return std::to_string(static_cast<long long>(value));
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>, typename = void>
    inline std::string Spell(T value) {
        return std::to_string(value);
    }

} // namespace harness

#define HARNESS_CONCAT_INNER(a, b) a##b
#define HARNESS_CONCAT(a, b) HARNESS_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                                                \
    static void HARNESS_CONCAT(harness_case_, __LINE__)();                                                             \
    static ::harness::Registrar HARNESS_CONCAT(harness_registrar_,                                                     \
                                               __LINE__)(name, &HARNESS_CONCAT(harness_case_, __LINE__));              \
    static void HARNESS_CONCAT(harness_case_, __LINE__)()

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ::harness::ReportFailure(__FILE__, __LINE__, "expected " #condition);                                      \
        }                                                                                                              \
    }                                                                                                                  \
    while (false)

#define CHECK_EQ(actual, expected)                                                                                     \
    do {                                                                                                               \
        const auto harness_actual = (actual);                                                                          \
        const auto harness_expected = (expected);                                                                      \
        if (!(harness_actual == harness_expected)) {                                                                   \
            ::harness::ReportFailure(__FILE__, __LINE__,                                                               \
                                     #actual " == " #expected " (actual " + ::harness::Spell(harness_actual) +         \
                                         ", expected " + ::harness::Spell(harness_expected) + ")");                    \
        }                                                                                                              \
    }                                                                                                                  \
    while (false)

#endif // CPP_KANA2_TESTS_HARNESS_H
