// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// policy<clamp>
//---------------------------------------------------------------------------
// policy<clamp>: integer rhs, both directions
TEST(WithMethodsTest, with_clamp_integer_rhs_both_directions) {
    using u100 = inside<{0, 100}>;
    u100 x{50};

    x.policy<clamp>() = 150;
    ASSERT_EQ(x, 100);
    x.policy<clamp>() = -50;
    ASSERT_EQ(x, 0);
    x.policy<clamp>() = 75;
    ASSERT_EQ(x, 75); // in-range passes through
}

// policy<clamp>: float rhs
TEST(WithMethodsTest, with_clamp_float_rhs) {
    using u10 = inside<{{0, 10}, 0.5}>;
    u10 x{5};

    x.policy<clamp>() = 100.0;
    ASSERT_EQ(x, 10);
    x.policy<clamp>() = -5.0;
    ASSERT_EQ(x, 0);
    x.policy<clamp>() = 3.5;
    ASSERT_EQ(x, 3.5_r);
}

// policy<clamp>: inside rhs
TEST(WithMethodsTest, with_clamp_inside_rhs) {
    using wide   = inside<{0, 200}>;
    using narrow = inside<{0, 100}>;
    narrow n{0};
    wide   over{150};

    n.policy<clamp>() = over;
    ASSERT_EQ(n, 100);

    wide under_zero{0};
    n.policy<clamp>() = under_zero;
    ASSERT_EQ(n, 0);
}

// policy<clamp>: signed inside
TEST(WithMethodsTest, with_clamp_signed_inside) {
    using s = inside<{-100, 100}>;
    s x{0};

    x.policy<clamp>() = 200;
    ASSERT_EQ(x, 100);
    x.policy<clamp>() = -200;
    ASSERT_EQ(x, -100);
}

//---------------------------------------------------------------------------
// policy<wrap>
//---------------------------------------------------------------------------
// policy<wrap>: integer rhs, both directions
TEST(WithMethodsTest, with_wrap_integer_rhs_both_directions) {
    using u10 = inside<{0, 9}>;
    u10 x{5};

    x.policy<wrap>() = 13;
    ASSERT_EQ(x, 3); // 13 % 10
    x.policy<wrap>() = 23;
    ASSERT_EQ(x, 3); // 23 % 10
    x.policy<wrap>() = -1;
    ASSERT_EQ(x, 9); // -1 wraps to top
    x.policy<wrap>() = -11;
    ASSERT_EQ(x, 9);
    x.policy<wrap>() = 5;
    ASSERT_EQ(x, 5); // in-range
}

// policy<wrap>: signed inside
TEST(WithMethodsTest, with_wrap_signed_inside) {
    using s = inside<{-5, 5}>; // 11 values
    s x{0};

    // 6 ≡ -5 (mod 11) within [-5, 5]
    x.policy<wrap>() = 6;
    ASSERT_EQ(x, -5);
    // 17 = 6 + 11 ≡ -5 (mod 11)
    x.policy<wrap>() = 17;
    ASSERT_EQ(x, -5);
    // -6 ≡ 5 (mod 11)
    x.policy<wrap>() = -6;
    ASSERT_EQ(x, 5);
}

// policy<wrap>: inside rhs
TEST(WithMethodsTest, with_wrap_inside_rhs) {
    using src = inside<{0, 100}>;
    using dst = inside<{0, 9}>;
    dst d{0};

    src high{45};
    d.policy<wrap>() = high;
    ASSERT_EQ(d, 5); // 45 % 10
}

//---------------------------------------------------------------------------
// policy<snap> (truncates toward zero)
//---------------------------------------------------------------------------
// policy<snap>: inside rhs with finer notch
TEST(WithMethodsTest, with_snap_inside_rhs_with_finer_notch) {
    using coarse = inside<{{0, 10}, 2}>;
    using fine   = inside<{{0, 10}, 1}>;

    coarse c{0};
    c.policy<snap>() = fine{3};
    ASSERT_EQ(c, 2); // 3 truncates toward zero -> 2
    c.policy<snap>() = fine{4};
    ASSERT_EQ(c, 4); // exact
    c.policy<snap>() = fine{5};
    ASSERT_EQ(c, 4); // 5 -> 4
    c.policy<snap>() = fine{7};
    ASSERT_EQ(c, 6); // 7 -> 6
}

// policy<snap>: float rhs not on notch
TEST(WithMethodsTest, with_snap_float_rhs_not_on_notch) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<snap>() = 3.0;
    ASSERT_EQ(c, 2); // 3 truncates to 2
    c.policy<snap>() = 3.99;
    ASSERT_EQ(c, 2);
    c.policy<snap>() = 4.0;
    ASSERT_EQ(c, 4); // exact
}

//---------------------------------------------------------------------------
// policy<round_nearest> (round half away)
//---------------------------------------------------------------------------
// policy<round_nearest>: float rhs
TEST(WithMethodsTest, with_snap_round_nearest_float_rhs) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<round_nearest>() = 3.0;
    ASSERT_EQ(c, 4); // halfway-up: 3 -> 4
    c.policy<round_nearest>() = 2.99;
    ASSERT_EQ(c, 2);
    c.policy<round_nearest>() = 3.01;
    ASSERT_EQ(c, 4);
    c.policy<round_nearest>() = 4.0;
    ASSERT_EQ(c, 4); // exact

    using half = inside<{{0, 10}, 0.5}>;
    half h;
    h.policy<round_nearest>() = 3.3;
    ASSERT_EQ(h, 3.5_r); // 3.5
    h.policy<round_nearest>() = 3.2;
    ASSERT_EQ(h, 3); // 3.0
}

// policy<round_nearest>: inside rhs incompatible notches
TEST(WithMethodsTest, with_snap_round_nearest_inside_rhs_incompatible_notches) {
    using coarse = inside<{{0, 10}, 2}>;
    using fine   = inside<{{0, 10}, 1}>;

    coarse c{0};
    c.policy<round_nearest>() = fine{3};
    ASSERT_EQ(c, 4); // 3 -> 4 (round half up)
    c.policy<round_nearest>() = fine{5};
    ASSERT_EQ(c, 6); // 5 -> 6
    c.policy<round_nearest>() = fine{4};
    ASSERT_EQ(c, 4); // exact
}

//---------------------------------------------------------------------------
// policy<snap> vs policy<round_nearest> divergence
//---------------------------------------------------------------------------
// policy<snap> vs policy<round_nearest> disagree on halfway
TEST(WithMethodsTest, with_snap_vs_with_snap_round_nearest_disagree_on_halfway) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<snap>() = 3.0;
    ASSERT_EQ(c, 2); // toward zero
    c.policy<round_nearest>() = 3.0;
    ASSERT_EQ(c, 4); // half away from zero
}

//---------------------------------------------------------------------------
// policy<Mode> — the Mode template parameter selects the rounding mode.
// One value driven through every mode; +3 / -3 sit halfway between notches so
// floor / ceil / truncate diverge by sign.
//---------------------------------------------------------------------------
// policy<Mode> selects the rounding mode
TEST(WithMethodsTest, with_snap_mode_selects_the_rounding_mode) {
    using coarse = inside<{{-10, 10}, 2}>; // grid: … -4 -2 0 2 4 …
    coarse c{0};

    // +3 is halfway between 2 and 4.
    c.policy<snap>() = 3.0;
    ASSERT_EQ(c, 2); // default == truncate toward zero
    c.policy<snap>() = 3.0;
    ASSERT_EQ(c, 2); // explicit snap == the default
    c.policy<round_floor>() = 3.0;
    ASSERT_EQ(c, 2); // toward −∞
    c.policy<round_ceil>() = 3.0;
    ASSERT_EQ(c, 4); // toward +∞
    c.policy<round_nearest>() = 3.0;
    ASSERT_EQ(c, 4); // half away from zero
    c.policy<round_half_even>() = 3.0;
    ASSERT_EQ(c, 4); // tie → even quotient

    // −3 is halfway between −4 and −2: floor / ceil / truncate now disagree.
    c.policy<snap>() = -3.0;
    ASSERT_EQ(c, -2); // truncate toward zero
    c.policy<round_floor>() = -3.0;
    ASSERT_EQ(c, -4); // toward −∞
    c.policy<round_ceil>() = -3.0;
    ASSERT_EQ(c, -2); // toward +∞
    c.policy<round_nearest>() = -3.0;
    ASSERT_EQ(c, -4); // half away from zero
    c.policy<round_half_even>() = -3.0;
    ASSERT_EQ(c, -4); // tie → even quotient
}

// A snapped temporary returned by value: policy<snap>() on an rvalue yields a
// value-owning policy_buffer (moves the product in), so this is safe — the buffer
// outlives the `num*num` temporary. Defined at namespace scope so the temporary is
// truly gone by the time the caller reads the result.
namespace {
using small_t = beman::inside::inside<{{0, 10}, per<10>}, clamp>;
auto square(small_t n) { return (n * n).policy<snap>(); }
} // namespace

//---------------------------------------------------------------------------
// policy<snap>() as a value. On an lvalue it converts through a (reference) policy_ref;
// on a *temporary* it returns a value-owning policy_buffer, so the snapped result can
// be returned/stored without dangling. Either way the target's own policy (clamp) and
// the proxy's snap merge via uses_flag.
//---------------------------------------------------------------------------
// policy<snap>() proxy converts to a value (RHS / return position)
TEST(WithMethodsTest, with_snap_proxy_converts_to_a_value_rhs_return_position) {
    using small = small_t;
    small a     = 2.5; // exact (dyadic) on the 1/10 grid

    // a*a is 6.25 on the finer 1/100 grid — off small's notch. These compile because
    // the proxy converts to `small`, applying clamp (range) + snap (notch).
    small trunc   = (a * a).policy<snap>();          // 6.25 → 6.2 (truncate)
    small nearest = (a * a).policy<round_nearest>(); // 6.25 → 6.3 (nearest)
    ASSERT_EQ(trunc, 6.2_r);
    ASSERT_EQ(nearest, 6.3_r);

    // Out-of-range still clamps, via small's own policy merged in at conversion.
    small four = 4;
    ASSERT_EQ(small((four * four).policy<snap>()), 10); // 16 → clamp → 10

    // Safety: `auto square(){ return (n*n).policy<snap>(); }` returns the value-owning
    // buffer; converting it to `small` after the temporary is gone is well-defined.
    small from_temp = square(small{2.5});
    small sq        = square(small{4});
    ASSERT_EQ(from_temp, 6.2_r);
    ASSERT_EQ(sq, 10);

    // Receiver category picks the proxy kind: lvalue → policy_ref (reference),
    // rvalue → policy_buffer (owns the value). Both convert to small; neither to a
    // target whose interval the value can never enter (conversion stays constrained).
    using big = decltype(small{} * small{});
    static_assert(std::is_same_v<decltype(std::declval<big&>().policy<snap>()),
                                 policy_ref<big, call_policy<flags_of<big> | snap>>>);
    static_assert(std::is_same_v<decltype(std::declval<big>().policy<snap>()),
                                 policy_buffer<big, call_policy<flags_of<big> | snap>>>);
    static_assert(std::is_convertible_v<decltype(std::declval<big&>().policy<snap>()), small>);
    static_assert(std::is_convertible_v<decltype(std::declval<big>().policy<snap>()), small>);
    // big keeps small's clamp (a result keeps its operands' shared policy), so it
    // clamps into any target; a checked product stays constrained.
    static_assert(std::is_convertible_v<decltype(std::declval<big&>().policy<snap>()), inside<{{1000, 2000}, 1}>>);
    using checked_big = decltype(inside<{0, 10}>{} * inside<{0, 10}>{});
    static_assert(
        !std::is_convertible_v<decltype(std::declval<checked_big&>().policy<snap>()), inside<{{1000, 2000}, 1}>>);
}
