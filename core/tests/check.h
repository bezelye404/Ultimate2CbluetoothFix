// Tiny test helpers shared by the test files (no framework).
#pragma once
#include <cstdio>

namespace u2ctest {
inline long g_checks = 0;
inline long g_failures = 0;
}  // namespace u2ctest

#define CHECK(...)                                                                     \
    do {                                                                               \
        ++u2ctest::g_checks;                                                           \
        if (!(__VA_ARGS__)) { ++u2ctest::g_failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } \
    } while (0)

#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        ++u2ctest::g_checks;                                                           \
        const long long va = static_cast<long long>(a), vb = static_cast<long long>(b); \
        if (va != vb) { ++u2ctest::g_failures; std::printf("FAIL %s:%d  %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, va, vb); } \
    } while (0)

void test_settings();
void test_translations();
void test_rate_limiter();
