// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Sanity checks for the overflow-detecting wrappers (GCC/Clang builtins):
// boundary values per width, signedness, and use in constant evaluation.

#include <beman/inside/detail/overflow.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace beman::inside;

TEST(OverflowTest, signed_boundaries) {
    using L = std::numeric_limits<std::int64_t>;
    std::int64_t r{};
    EXPECT_FALSE(add_overflow<std::int64_t>(L::max() - 1, 1, &r));
    EXPECT_EQ(r, L::max());
    EXPECT_TRUE(add_overflow<std::int64_t>(L::max(), 1, &r));
    EXPECT_FALSE(sub_overflow<std::int64_t>(-1, L::max(), &r));
    EXPECT_EQ(r, L::min());
    EXPECT_TRUE(sub_overflow<std::int64_t>(0, L::min(), &r));
    EXPECT_FALSE(sub_overflow<std::int64_t>(-1, L::min(), &r));
    EXPECT_EQ(r, L::max());
    EXPECT_TRUE(mul_overflow<std::int64_t>(L::min(), -1, &r));
    EXPECT_FALSE(mul_overflow<std::int64_t>(L::min() / 2, 2, &r));
    EXPECT_EQ(r, L::min());
}

TEST(OverflowTest, unsigned_and_narrow_boundaries) {
    std::uint64_t u{};
    EXPECT_TRUE(add_overflow<std::uint64_t>(~0ull, 1, &u));
    EXPECT_EQ(u, 0u);
    EXPECT_TRUE(sub_overflow<std::uint64_t>(0, 1, &u));
    EXPECT_FALSE(mul_overflow<std::uint64_t>(1ull << 32, (1ull << 32) - 1, &u));
    EXPECT_TRUE(mul_overflow<std::uint64_t>(1ull << 32, 1ull << 32, &u));
    std::int8_t s{};
    EXPECT_TRUE(mul_overflow<std::int8_t>(100, 100, &s));
    EXPECT_TRUE(sub_overflow<std::int8_t>(5, std::numeric_limits<std::int8_t>::min(), &s));
    EXPECT_FALSE(add_overflow<std::int8_t>(-128, 127, &s));
    EXPECT_EQ(s, -1);
}

TEST(OverflowTest, usable_in_constant_evaluation) {
    static_assert([] {
        std::int32_t r{};
        return !add_overflow<std::int32_t>(2, 3, &r) && r == 5;
    }());
    static_assert([] {
        std::int8_t r{};
        return mul_overflow<std::int8_t>(100, 100, &r);
    }());
    SUCCEED();
}
