#pragma once

#include "../error_reporting.hpp"
#include <cstdint>
#include <cstddef>

#ifdef min
    #undef min
#endif
#ifdef max
    #undef max
#endif

namespace tp {

template <typename T>
constexpr T min(const T& a, const T& b) {
    return (a > b) ? b : a;
}

template <typename T>
constexpr T min(const T& a, const T& b, const T& c) {
    return min(min(a, b), c);
}

template <typename T>
constexpr T min(const T& a, const T& b, const T& c, const T& d) {
    return min(min(a, b), min(c, d));
}

template <typename T>
constexpr T max(const T& a, const T& b) {
    return (a < b) ? b : a;
}

template <typename T>
constexpr T max(const T& a, const T& b, const T& c) {
    return max(max(a, b), c);
}

template <typename T>
constexpr T max(const T& a, const T& b, const T& c, const T& d) {
    return max(max(a, b), max(c, d));
}

template <typename T>
constexpr T clamp(const T& a, const T& b, const T& v) {
    return max(min(v, b), a);
}

constexpr bool containsAllBits(uint32_t mask, uint32_t bits) {
    return (mask & bits) == bits;
}

inline uint8_t log2(uint32_t v) {
    // https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
    static const uint8_t table[32] = { 0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
                                       8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31 };

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return table[v * 0x07c4acddU >> 27];
}

inline uint8_t log2(uint64_t v) {
    // http://stackoverflow.com/a/23000588/2044117
    static const uint8_t table[64] = { 0,  58, 1,  59, 47, 53, 2,  60, 39, 48, 27, 54, 33, 42, 3,  61,
                                       51, 37, 40, 49, 18, 28, 20, 55, 30, 34, 11, 43, 14, 22, 4,  62,
                                       57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19, 29, 10, 13, 21, 56,
                                       45, 25, 31, 35, 16, 9,  12, 44, 24, 15, 8,  23, 7,  6,  5,  63 };

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return table[v * 0x03f6eaf2cd271461U >> 58];
}

inline uint64_t safeRoundToUint(float v) {
    uint64_t i = 0;
    if (v > 18446742974197923840.0f) // largest representable float value smaller than UINT64_MAX
        i = UINT64_MAX;
    else if (v > 0.0f)
        i = static_cast<uint64_t>(std::round(v));
    return i;
}

template <typename T>
constexpr T roundUpToMultiple(T v, T m) {
    return ((v + m - 1) / m) * m;
}

inline uint64_t roundUpToPoTMultiple(uint64_t v, uint64_t m) {
    TEPHRA_ASSERT(m != 0);
    TEPHRA_ASSERTD((m & (m - 1)) == 0, "Multiple must be a power of two.");
    return (v + m - 1) & ~(m - 1);
}

}
