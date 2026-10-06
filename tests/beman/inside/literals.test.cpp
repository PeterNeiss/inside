// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/detail/rational.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

// _r literal: integer forms
TEST(LiteralsTest, r_literal_integer_forms) {
    static_assert(5_r == rational{5});
    static_assert(1'000_r == rational{1000});
    static_assert(0xff_r == rational{255});
    static_assert(0xFF_r == rational{255});
    static_assert(0b1010_r == rational{10});
    static_assert(0b1'010_r == rational{10});
}

// _r literal: decimal forms
TEST(LiteralsTest, r_literal_decimal_forms) {
    static_assert(1.25_r == rational{5, 4});
    static_assert(.5_r == rational{1, 2});
    static_assert(0.1_r == rational{1, 10}); // exact, no double round-trip
    static_assert(0.01_r == rational{1, 100});
    static_assert(3.14_r == rational{157, 50});
}

// _r literal: decimal scientific (e)
TEST(LiteralsTest, r_literal_decimal_scientific_e) {
    static_assert(1.5e2_r == rational{150});
    static_assert(2.5e-1_r == rational{1, 4});
    static_assert(1e3_r == rational{1000});
    static_assert(1e-3_r == rational{1, 1000});
}

// _r literal: hex float / binary exponent (p)
TEST(LiteralsTest, r_literal_hex_float_binary_exponent_p) {
    static_assert(0x1p15_r == rational{32768});
    static_assert(0x1p-15_r == rational{1, 32768});
    static_assert(0x3p-4_r == rational{3, 16});
    static_assert(0x1.8p3_r == rational{12});
    static_assert(0x1.8p0_r == rational{3, 2});
    static_assert(0x1p-14_r == rational{1, 16384}); // Q14 notch
}

// _ins literal: produces point inside (just<value>)
TEST(LiteralsTest, ins_literal_produces_point_inside_just_value) {
    constexpr auto five = 5_ins;
    static_assert(lower_of<decltype(five)> == 5);
    static_assert(upper_of<decltype(five)> == 5);

    constexpr auto quarter = 0.25_ins;
    static_assert(lower_of<decltype(quarter)> == rational{1, 4});
    static_assert(upper_of<decltype(quarter)> == rational{1, 4});

    constexpr auto q14_notch = 0x1p-14_ins;
    static_assert(lower_of<decltype(q14_notch)> == rational{1, 16384});
}

// a_b / b_b ~= rational{a,b} - value-equivalent (expected-wrapped)
TEST(LiteralsTest, a_b_b_b_rational_a_b_value_equivalent_expected_wrapped) {
    // Point bounds: the divisor is a nonzero point and the quotient provably
    // fits, so `inside / inside` is a plain point inside with grid {a/b, a/b}.
    constexpr auto three_quarters = 3_ins / 4_ins;
    static_assert(three_quarters == rational{3, 4});
    static_assert(!is_expected_v<decltype(three_quarters)>);

    using point_t = std::remove_cv_t<decltype(three_quarters)>;
    static_assert(lower_of<point_t> == rational{3, 4});
    static_assert(upper_of<point_t> == rational{3, 4});
}

// _r and _ins agree
TEST(LiteralsTest, r_and_ins_agree) {
    static_assert(1.25_ins == 1.25_r);
    static_assert(0x1p-14_ins == 0x1p-14_r);
    static_assert(1.5e2_ins == 1.5e2_r);
}
