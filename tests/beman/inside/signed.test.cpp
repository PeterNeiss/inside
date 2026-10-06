// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <expected>
#include <limits>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

// signed storage type selection
TEST(SignedTest, signed_storage_type_selection) {
    using s8  = inside<{-127, 127}>;
    using s16 = inside<{-32000, 32000}>;
    using s32 = inside<{-100000, 100000}>;
    static_assert(std::is_same_v<typename s8::raw_type, std::int8_t>);
    static_assert(std::is_same_v<typename s16::raw_type, std::int16_t>);
    static_assert(std::is_same_v<typename s32::raw_type, std::int32_t>);

    // fractional notch with negative lower stays unsigned (notch storage)
    using frac_neg = inside<{{-10, 10}, 0.25}>;
    static_assert(std::is_unsigned_v<typename frac_neg::raw_type>);
}

// signed construction and arithmetic
TEST(SignedTest, signed_construction_and_arithmetic) {
    using s32 = inside<{-100000, 100000}>;
    s32 a{42};
    s32 b{-300};
    ASSERT_EQ(a, 42);
    ASSERT_EQ(b, -300);

    ASSERT_EQ(-a, -42);
    ASSERT_EQ(-b, 300);

    ASSERT_EQ(a + b, -258);
    ASSERT_EQ(a - b, 342);

    s32 x{-7}, y{14};
    ASSERT_EQ(x * y, -98);
    ASSERT_EQ(y * x, -98);

    auto quot = x / y;
    ASSERT_EQ(*quot, -0.5_r);

    s32 acc{100}, delta{-30};
    acc += delta;
    ASSERT_EQ(acc, 70);

    s32 acc2{50};
    acc2 += -75_ins;
    ASSERT_EQ(acc2, -25);
}

// signed storage uses the type's full range, including its minimum
TEST(SignedTest, signed_storage_uses_full_range_including_minimum) {
    using s8 = inside<{-128, 127}>;
    static_assert(sizeof(s8) == 1);
    s8 min_val{-128};
    ASSERT_EQ(min_val, -128);
    ASSERT_EQ(min_val.raw(), std::numeric_limits<std::int8_t>::min());
    ASSERT_EQ(-min_val, 128); // negation widens the grid
}

// Regression: signed-direct storage previously recorded the *offset*
// (rhs - Lower) instead of the value when constructed from f64-valued
// rhs (rational/double) or from another inside via the inside-to-inside
// store path. Both paths now route through `raw_from_offset<L>`, which
// adds lower_of<L> back for direct-storage targets.
// signed-direct ctor from rational/double preserves value
TEST(SignedTest, signed_direct_ctor_from_rational_double_preserves_value) {
    using temp = inside<{-40, 60}>;

    ASSERT_EQ(temp{-40_r}, -40);
    ASSERT_EQ(temp{rational{42}}, 42);
    ASSERT_EQ(temp{rational{0}}, 0);
    ASSERT_EQ(temp{-40.0}, -40);
    ASSERT_EQ(temp{42.0}, 42);

    // Raw must match value for direct storage.
    ASSERT_EQ(temp{-40_r}.raw(), -40);
    ASSERT_EQ(temp{rational{42}}.raw(), 42);
}

// inside-to-inside assignment preserves value across direct storages
TEST(SignedTest, inside_to_inside_assignment_preserves_value_across_direct_storages) {
    // unsigned-direct → signed-direct
    using upos = inside<{0, 30}>;
    using s40  = inside<{-40, 50}>;
    upos u{25};
    s40  s{u};
    ASSERT_EQ(s, 25);
    ASSERT_EQ(s.raw(), 25);

    // signed-direct → signed-direct (different grids)
    using s2 = inside<{-20, 30}>;
    s2  s2v{15};
    s40 s40v{s2v};
    ASSERT_EQ(s40v, 15);
    ASSERT_EQ(s40v.raw(), 15);

    // same-grid stays a fast copy (Offset == 0, Factor == 1)
    s40 s40w{s40v};
    ASSERT_EQ(s40w, 15);
    ASSERT_EQ(s40w.raw(), 15);
}

// mixed signed/unsigned arithmetic
TEST(SignedTest, mixed_signed_unsigned_arithmetic) {
    using u100 = inside<{0, 100}>;
    using s32  = inside<{-100000, 100000}>;
    u100 u{80};
    s32  s{-500};
    auto mixed = u + s;
    ASSERT_EQ(mixed, -420);

    using u8 = inside<{0, 255}>;
    u8   p{10}, q{200};
    auto d = p - q;
    ASSERT_EQ(d, -190);
}

// signed clamp / wrap
TEST(SignedTest, signed_clamp_wrap) {
    using sc = inside<{-100, 100}, clamp>;
    ASSERT_EQ(sc{200}, 100);
    ASSERT_EQ(sc{-200}, -100);

    using sw = inside<{-100, 100}, wrap>;
    ASSERT_EQ(sw{150}, -51);
    ASSERT_EQ(sw{-150}, 51);
}

// signed expected helpers
TEST(SignedTest, signed_expected_helpers) {
    using s32 = inside<{-100000, 100000}>;
    auto a    = []() -> std::expected<s32, errc> { return s32{42}; };
    auto none = []() -> std::expected<s32, errc> { return std::unexpected{errc::overflow}; };

    auto sum = a() + s32{-100};
    ASSERT_TRUE(sum.has_value());
    ASSERT_EQ(*sum, -58);
    ASSERT_FALSE((none() + s32{-100}).has_value());
}

// comparison across grids
TEST(SignedTest, comparison_across_grids) {
    using u100 = inside<{0, 100}>;
    using u50  = inside<{0, 50}>;
    u100 a{30}, b{50};
    u50  c{30};

    ASSERT_TRUE(a < b);
    ASSERT_EQ(a, c);

    ASSERT_EQ(a, 30);
    ASSERT_TRUE(b > 25);

    using s100 = inside<{-100, 100}>;
    s100 neg{-30}, pos{30};
    ASSERT_TRUE(neg < pos);
}
