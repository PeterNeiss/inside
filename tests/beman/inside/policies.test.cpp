// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Policies and their hooks: one-shot policies and the policy handles
// (x.policy<F>()), the actions on_clamp/on_wrap/on_error, the error
// handler and the error vocabulary.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/arithmetic.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// policy_actions
namespace policy_actions_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// clamp policy on assignment
TEST(PolicyActionsTest, clamp_policy_on_assignment) {
    using pct = inside<{0, 100}, clamp>;
    pct over{150};
    ASSERT_EQ(over, 100);

    pct under{-5};
    ASSERT_EQ(under, 0);

    pct ok{50};
    ASSERT_EQ(ok, 50);

    // compound +
    pct a{90};
    a += inside<{0, 100}>{20};
    ASSERT_EQ(a, 100);
}

// wrap policy on assignment
TEST(PolicyActionsTest, wrap_policy_on_assignment) {
    using angle = inside<{0, 359}, wrap>;
    angle a{370};
    ASSERT_EQ(a, 10);
    angle b{-10};
    ASSERT_EQ(b, 350);
    angle c{360};
    ASSERT_EQ(c, 0);
    angle d{180};
    ASSERT_EQ(d, 180);
}

// try_make on fixed-point grids
TEST(PolicyActionsTest, try_make_on_fixed_point_grids) {
    {
        SCOPED_TRACE("Q8.8 (notch 1/256) — try_make + in-place ++");
        using fp = inside<{{0, 255}, 1.0 / 256}>;
        static_assert(sizeof(fp) == 2);

        // notch-aligned in-range value
        auto ok = fp::try_make(42.5);
        ASSERT_TRUE(ok.has_value());
        ASSERT_TRUE(ok->to<double>().value() == 42.5);

        // out-of-range produces errc::overflow
        auto high = fp::try_make(300.0);
        ASSERT_FALSE(high.has_value());
        ASSERT_EQ(high.error(), errc::overflow);

        auto low = fp::try_make(-0.5);
        ASSERT_FALSE(low.has_value());

        // in-place increment past upper bound reports through the checked policy
        fp top = fp::try_make(255.0).value();
        ASSERT_ANY_THROW(++top);
    }

    {
        SCOPED_TRACE("half-step (notch 0.5) — uint8 storage");
        using sensor = inside<{{0, 50}, 0.5}>;
        static_assert(sizeof(sensor) == 1);

        auto s = sensor::try_make(23.5);
        ASSERT_TRUE(s.has_value());
        ASSERT_TRUE(s->to<double>().value() == 23.5);

        ASSERT_FALSE(sensor::try_make(50.5).has_value());
        ASSERT_FALSE(sensor::try_make(-1.0).has_value());
    }

    {
        SCOPED_TRACE("signed Q1.14 (notch 1/16384) — uint16 storage");
        using sample = inside<{{-1, 1}, per<16384>}, checked | round_nearest>;
        static_assert(sizeof(sample) == 2);

        auto s = sample::try_make(0.5);
        ASSERT_TRUE(s.has_value());
        ASSERT_EQ(double(*s), 0.5);

        ASSERT_FALSE(sample::try_make(1.5).has_value());
        ASSERT_FALSE(sample::try_make(-1.25).has_value());

        // boundaries ±1 are inclusive
        ASSERT_TRUE(sample::try_make(1.0).has_value());
        ASSERT_TRUE(sample::try_make(-1.0).has_value());
    }

    {
        SCOPED_TRACE("Q16.16 (notch 1/65536) — uint32 storage");
        using fp = inside<{{0, 65535}, per<65536>}>;
        static_assert(sizeof(fp) == 4);

        auto v = fp::try_make(1000.125);
        ASSERT_TRUE(v.has_value());
        ASSERT_TRUE(v->to<double>().value() == 1000.125);

        ASSERT_FALSE(fp::try_make(-0.001).has_value());
        ASSERT_FALSE(fp::try_make(70000.0).has_value());
    }
}

// on_wrap action receives inside& and carry
TEST(PolicyActionsTest, on_wrap_action_receives_inside_and_carry) {
    using sec = inside<{0, 59}, wrap>;
    sec  s{0};
    imax carry = 0;
    s.on_wrap([&](auto& self, auto c) {
        carry = as<imax>(c);
        (void)self;
    })         = 65;
    ASSERT_EQ(s, 5);
    ASSERT_EQ(carry, 1);

    // handler may override the wrapped value
    sec s2{0};
    s2.on_wrap([](auto& self, auto c) {
        if (c > 0)
            self = 0;
    }) = 65;
    ASSERT_EQ(s2, 0);
}

// on_wrap carry is an inside for an inside RHS
TEST(PolicyActionsTest, on_wrap_carry_is_an_inside_for_an_inside_rhs) {
    // A insidable RHS routes the wrap through inside arithmetic, so the carry is a
    // inside<excess-grid>. `carry + just<0>` is inside+inside (compiles only for an inside;
    // an imax carry would hit the grid-less-scalar guidance overload).
    using w10 = inside<{0, 9}, wrap>; // wraps at 10
    w10            x{8};
    inside<{0, 5}> add{4}; // inside RHS
    imax           carry_val = -99;
    x.on_wrap([&](auto&, auto carry) { carry_val = as<imax>(carry + just<0>); }) += add;
    ASSERT_EQ(x, 2);         // 8 + 4 = 12 -> 12 mod 10
    ASSERT_EQ(carry_val, 1); // floor(12 / 10)

    // Fractional grid (notch 1/2) exercises the rational wrap path; the carry is
    // still an inside (range = 3 + 0.5 = 3.5).
    using pos = inside<{{0, 3}, per<2>}, wrap>;
    pos                      p{2.5};
    inside<{{0, 2}, per<2>}> fadd{1.5};
    imax                     fcarry = -99;
    p.on_wrap([&](auto&, auto carry) { fcarry = as<imax>(carry + just<0>); }) += fadd;
    ASSERT_TRUE((p == just<1, 2>)); // 2.5 + 1.5 = 4.0 -> 4.0 - 3.5 = 0.5
    ASSERT_EQ(fcarry, 1);           // floor(4.0 / 3.5)
}

// on_clamp action receives overshoot
TEST(PolicyActionsTest, on_clamp_action_receives_overshoot) {
    using u100 = inside<{0, 100}>;
    u100 x{0};
    imax overshoot = 0;
    x.on_clamp([&](auto& self, auto over) {
        overshoot = as<imax>(over); // an inside for every source
        (void)self;
    })             = 150;
    ASSERT_EQ(x, 100);
    ASSERT_EQ(overshoot, 50);
}

// on_clamp overshoot is an inside for an inside RHS
TEST(PolicyActionsTest, on_clamp_overshoot_is_an_inside_for_an_inside_rhs) {
    // A insidable RHS routes clamp through inside arithmetic, so the overshoot is a
    // inside<grid_of<R> - grid_of<L>>, read with as<imax>() (it would not compile
    // against a raw expected<rational>).
    using c100 = inside<{0, 100}, clamp>;
    c100             x{0};
    inside<{0, 200}> v{150}; // overlaps [0,100], runtime out of range
    imax             ov                                        = 0;
    x.on_clamp([&](auto&, auto over) { ov = as<imax>(over); }) = v;
    ASSERT_EQ(x, 100);
    ASSERT_EQ(ov, 50); // overshoot 150 - 100
}

// on_error action receives code and message
TEST(PolicyActionsTest, on_error_action_receives_code_and_message) {
    using c100 = inside<{0, 100}, checked>;
    c100 e{50};
    bool fired = false;
    e.on_error([&](auto& self, errc code, std::string_view msg) {
        fired = (code == errc::overflow) && !msg.empty();
        self  = 0;
    })         = 200;
    ASSERT_TRUE(fired);
    ASSERT_EQ(e, 0);
}

// A hook on a compound op: the sum overshoots the target, and the store back
// fires on_clamp.
TEST(PolicyActionsTest, compound_op_fires_on_clamp_on_the_store_back) {
    using c100 = inside<{0, 100}, checked>;
    c100 b{50};
    bool cl   = false;
    imax over = 0;
    b.on_clamp([&](auto&, auto o) {
        cl   = true;
        over = as<imax>(o);
    }) += 200_ins; // 50+200=250 overshoots [0,100]
    ASSERT_TRUE(cl);
    ASSERT_EQ(over, 150);
    ASSERT_EQ(b, 100);
}

// The free functions take a policy or an errc&, no hook: a zero divisor is in
// the result, as for `/` (on_clamp / on_wrap / on_error are rejected, see
// fail/arith_unused_action).
TEST(PolicyActionsTest, free_div_and_mod_return_a_zero_divisor) {
    using c100   = inside<{0, 100}, checked>;
    using u100ic = inside<{0, 100}, checked | round_toward_zero>;
    c100 d{50}, z{0};
    auto q = div(d, z, make_policy<round_nearest>());
    ASSERT_FALSE(q.has_value());
    ASSERT_EQ(q.error(), errc::division_by_zero);
    u100ic l{7}, r{0};
    auto   m = mod(l, r);
    ASSERT_FALSE(m.has_value());
    ASSERT_EQ(m.error(), errc::division_by_zero);
}

// A compound op whose exact result is off the grid fails like a store:
// on_error takes it, and the value is unchanged.
TEST(PolicyActionsTest, compound_store_failure_reaches_on_error) {
    using frac = inside<{{0, 1}, max_denominator{imax{1} << 62}}>;
    frac f{rational{1, (imax{1} << 62) - 1}};
    errc seen{};
    f.on_error([&](auto&, errc c, const char*) { seen = c; }) += frac{rational{1, (imax{1} << 62) - 3}};
    ASSERT_EQ(seen, errc::rounding_error); // the sum's denominator passes the bound
    ASSERT_EQ(f, (rational{1, (imax{1} << 62) - 1}));
}

// free-fn div with beman::inside::errc& sets ec on div/0 / success path leaves ec clear
TEST(PolicyActionsTest, free_fn_div_with_beman_inside_errc_sets_ec_on_div_0__success_path_leaves_ec_clear) {
    using c100 = inside<{0, 100}, checked>;
    beman::inside::errc ec{};
    auto                q = div(c100{10}, c100{0}, ec);
    ASSERT_EQ(ec, errc::division_by_zero);
    ASSERT_FALSE(q.has_value());

    {
        SCOPED_TRACE("success path leaves ec clear");
        beman::inside::errc ec2{};
        auto                r = div(c100{10}, c100{2}, ec2);
        ASSERT_EQ(ec2, errc{});
        ASSERT_TRUE(r.has_value());
    }
}

// free-fn mod with beman::inside::errc& sets ec on div/0
TEST(PolicyActionsTest, free_fn_mod_with_beman_inside_errc_sets_ec_on_div_0) {
    using u100ic = inside<{0, 100}, checked | round_toward_zero>;
    beman::inside::errc ec{};
    auto                m = mod(u100ic{7}, u100ic{0}, ec);
    ASSERT_EQ(ec, errc::division_by_zero);
    ASSERT_FALSE(m.has_value());
}

// free-fn add with beman::inside::errc& compiles and clears on success
TEST(PolicyActionsTest, free_fn_add_with_beman_inside_errc_compiles_and_clears_on_success) {
    using c100 = inside<{0, 100}, checked>;
    beman::inside::errc ec{};
    auto                sum = add(c100{40}, c100{50}, ec);
    ASSERT_EQ(ec, errc{});
    ASSERT_EQ(sum, 90);

    // SFINAE: the ec form must bind for add / sub / mul / div / mod.
    static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { add(x, y, e); });
    static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { sub(x, y, e); });
    static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { mul(x, y, e); });
    static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { div(x, y, e); });
}

// no-arg div on div/0 returns an error without throwing
TEST(PolicyActionsTest, no_arg_div_on_div_0_returns_error_without_throwing) {
    // Regression guard for fix 1b's empty_ref/error_ref gate: the no-arg form
    // must NOT throw on div/0 — it should return errc::division_by_zero.
    using c100 = inside<{0, 100}, checked>;
    ASSERT_NO_THROW((void)([] {
        auto q = div(c100{10}, c100{0});
        ASSERT_FALSE(q.has_value());
        ASSERT_EQ(q.error(), errc::division_by_zero);
    }()));
}

// on_error catches rounding_error on float assignment
TEST(PolicyActionsTest, on_error_catches_rounding_error_on_float_assignment) {
    using coarse = inside<{{0, 10}, 2}>; // notch 2: 3.0 doesn't land
    coarse c{0};
    errc   seen{};
    bool   fired = false;
    c.on_error([&](auto& self, errc code, std::string_view) {
        fired = true;
        seen  = code;
        self  = 4; // recover to a valid notch value
    })           = 3.0;
    ASSERT_TRUE(fired);
    ASSERT_EQ(seen, errc::rounding_error);
    ASSERT_EQ(c, 4);
}

// policy(ec) catches rounding_error on float assignment
TEST(PolicyActionsTest, policy_ec_catches_rounding_error_on_float_assignment) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse              c{0};
    beman::inside::errc ec{};
    c.policy(ec) = 3.0;
    ASSERT_EQ(ec, errc::rounding_error);
}

// The on_wrap carry is an inside for every source kind, so it adds straight
// into another inside; an `imax` callback parameter still binds.
TEST(PolicyActionsTest, on_wrap_carry_is_an_inside_for_every_source) {
    using sec = inside<{0, 59}, wrap>;
    using min = inside<{0, 59}>;
    auto run  = [](auto src) {
        sec seconds{0};
        min minutes{0};
        seconds.on_wrap([&](auto&, auto carry) {
            static_assert(insidable<decltype(carry)>);
            minutes += carry;
        }) = src;
        EXPECT_EQ(seconds, 5);
        EXPECT_EQ(minutes, 2);
    };
    run(125);
    run(std::uint8_t{125});
    run(125.0);
    run(125_ins);

    // An integer source's carry grid comes from its type's limits.
    sec s{0};
    s.on_wrap([](auto&, auto carry) {
        static_assert(grid_of<decltype(carry)> == grid{-3, 2}); // signed char into {0, 59}
    }) = static_cast<signed char>(5);

    imax c                                             = 0;
    s.on_wrap([&](auto&, auto k) { c = as<imax>(k); }) = 130;
    EXPECT_EQ(c, 2);
}

// The on_clamp overshoot is an inside for every source kind, as the wrap carry
// is: the source minus the limit, exactly. Its grid holds every value of the
// source kind minus either limit.
TEST(PolicyActionsTest, on_clamp_overshoot_is_an_inside_for_every_source) {
    using pct = inside<{0, 100}>;
    auto run  = [](auto src, imax expected) {
        pct  x{0};
        imax got = -1;
        x.on_clamp([&](auto&, auto over) {
            static_assert(insidable<decltype(over)>);
            got = as<imax>(over);
        })       = src;
        EXPECT_EQ(x, 100);
        EXPECT_EQ(got, expected);
    };
    run(150, 50);
    run(std::uint8_t{200}, 100);
    run(150.0, 50);
    run(rational{150}, 50);
    run(150_ins, 50);

    // An integer source's overshoot grid is its type's limits minus the limits.
    pct y{0};
    y.on_clamp([](auto&, auto over) {
        static_assert(grid_of<decltype(over)> == grid{-100, 255}); // uint8_t into {0, 100}
    }) = std::uint8_t{101};

    // Below the range the overshoot is negative; it keeps the source's fraction.
    using q4 = inside<{{0, 10}, per<4>}>;
    q4   f{0};
    bool exact                                                      = false;
    f.on_clamp([&](auto&, auto over) { exact = over == 2.25_ins; }) = 12.25;
    EXPECT_EQ(f, 10);
    EXPECT_TRUE(exact);
    f.on_clamp([&](auto&, auto over) { exact = over == -0.5_ins; }) = -0.5;
    EXPECT_EQ(f, 0);
    EXPECT_TRUE(exact);
}

// The overshoot is exact for every source: a double's fraction below the
// target's notch, a rational's, a long double's past a double's precision, and
// a double past the imax range. ±∞ reports the overshoot grid's end.
TEST(PolicyActionsTest, on_clamp_overshoot_is_exact) {
    using pct = inside<{0, 100}>;
    pct  x{0};
    bool exact = false;
    // 150.7 is not 1507/10 in binary: the overshoot is the double minus 100.
    x.on_clamp([&](auto&, auto over) { exact = over == inside<{{0, 64}, 0x1p-45}>{150.7 - 100.0}; }) = 150.7;
    EXPECT_TRUE(exact);

    inside<{0, 100}, round_nearest> r{0};
    r.on_clamp([&](auto&, auto over) { exact = over == inside<{{0, 64}, per<6>}>{rational{301, 6}}; }) =
        rational{901, 6};
    EXPECT_TRUE(exact);

    if constexpr (std::numeric_limits<long double>::digits > 53) {
        const long double ld = 150.25L + 0x1p-55L; // past a double's precision at 150
        x.on_clamp([&](auto&, auto over) { exact = over == inside<{{0, 64}, 0x1p-55}>{50.25L + 0x1p-55L}; }) = ld;
        EXPECT_TRUE(exact);
    }

    using wide = inside<{{-0x1p70, 0x1p70}, max_denominator{3}}>;
    wide w{0};
    w.on_clamp([&](auto&, auto over) { exact = over == inside<{-0x1p71, 0}>{-0x1p70}; }) = -0x1p71;
    EXPECT_TRUE(exact);

    x.on_clamp([&](auto&, auto over) { exact = over == just<upper_of<decltype(over)>>; }) =
        std::numeric_limits<double>::infinity();
    EXPECT_TRUE(exact);
    EXPECT_EQ(x, 100);
    x.on_clamp([&](auto&, auto over) { exact = over == just<lower_of<decltype(over)>>; }) =
        -std::numeric_limits<double>::infinity();
    EXPECT_TRUE(exact);
    EXPECT_EQ(x, 0);
}
} // namespace policy_actions_tests

// with_methods
namespace with_methods_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// policy<clamp>
//---------------------------------------------------------------------------
// policy<clamp>: integer rhs, both directions
TEST(PolicyHandleTest, clamp_integer_rhs_both_directions) {
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
TEST(PolicyHandleTest, clamp_float_rhs) {
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
TEST(PolicyHandleTest, clamp_inside_rhs) {
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
TEST(PolicyHandleTest, clamp_signed_inside) {
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
TEST(PolicyHandleTest, wrap_integer_rhs_both_directions) {
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
TEST(PolicyHandleTest, wrap_signed_inside) {
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
TEST(PolicyHandleTest, wrap_inside_rhs) {
    using src = inside<{0, 100}>;
    using dst = inside<{0, 9}>;
    dst d{0};

    src high{45};
    d.policy<wrap>() = high;
    ASSERT_EQ(d, 5); // 45 % 10
}

//---------------------------------------------------------------------------
// policy<round_toward_zero> (truncates toward zero)
//---------------------------------------------------------------------------
// policy<round_toward_zero>: inside rhs with finer notch
TEST(PolicyHandleTest, round_toward_zero_inside_rhs_with_finer_notch) {
    using coarse = inside<{{0, 10}, 2}>;
    using fine   = inside<{{0, 10}, 1}>;

    coarse c{0};
    c.policy<round_toward_zero>() = fine{3};
    ASSERT_EQ(c, 2); // 3 truncates toward zero -> 2
    c.policy<round_toward_zero>() = fine{4};
    ASSERT_EQ(c, 4); // exact
    c.policy<round_toward_zero>() = fine{5};
    ASSERT_EQ(c, 4); // 5 -> 4
    c.policy<round_toward_zero>() = fine{7};
    ASSERT_EQ(c, 6); // 7 -> 6
}

// policy<round_toward_zero>: float rhs not on notch
TEST(PolicyHandleTest, round_toward_zero_float_rhs_not_on_notch) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<round_toward_zero>() = 3.0;
    ASSERT_EQ(c, 2); // 3 truncates to 2
    c.policy<round_toward_zero>() = 3.99;
    ASSERT_EQ(c, 2);
    c.policy<round_toward_zero>() = 4.0;
    ASSERT_EQ(c, 4); // exact
}

//---------------------------------------------------------------------------
// policy<round_nearest> (round half away)
//---------------------------------------------------------------------------
// policy<round_nearest>: float rhs
TEST(PolicyHandleTest, round_nearest_float_rhs) {
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
TEST(PolicyHandleTest, round_nearest_inside_rhs_incompatible_notches) {
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
// policy<round_toward_zero> vs policy<round_nearest> divergence
//---------------------------------------------------------------------------
// policy<round_toward_zero> vs policy<round_nearest> disagree on halfway
TEST(PolicyHandleTest, round_toward_zero_and_round_nearest_differ_on_a_tie) {
    using coarse = inside<{{0, 10}, 2}>;
    coarse c{0};

    c.policy<round_toward_zero>() = 3.0;
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
TEST(PolicyHandleTest, the_handle_selects_the_rounding_mode) {
    using coarse = inside<{{-10, 10}, 2}>; // grid: … -4 -2 0 2 4 …
    coarse c{0};

    // +3 is halfway between 2 and 4.
    c.policy<round_toward_zero>() = 3.0;
    ASSERT_EQ(c, 2); // default == truncate toward zero
    c.policy<round_toward_zero>() = 3.0;
    ASSERT_EQ(c, 2); // explicit round_toward_zero == the default
    c.policy<round_floor>() = 3.0;
    ASSERT_EQ(c, 2); // toward −∞
    c.policy<round_ceil>() = 3.0;
    ASSERT_EQ(c, 4); // toward +∞
    c.policy<round_nearest>() = 3.0;
    ASSERT_EQ(c, 4); // half away from zero
    c.policy<round_half_even>() = 3.0;
    ASSERT_EQ(c, 4); // tie → even quotient

    // −3 is halfway between −4 and −2: floor / ceil / truncate now disagree.
    c.policy<round_toward_zero>() = -3.0;
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

// A snapped temporary returned by value: policy<round_toward_zero>() on an rvalue yields a
// value-owning policy_buffer (moves the product in), so this is safe — the buffer
// outlives the `num*num` temporary. Defined at namespace scope so the temporary is
// truly gone by the time the caller reads the result.
namespace {
using small_t = beman::inside::inside<{{0, 10}, per<10>}, clamp>;
auto square(small_t n) { return (n * n).policy<round_toward_zero>(); }
} // namespace

//---------------------------------------------------------------------------
// policy<round_toward_zero>() as a value. On an lvalue it converts through a (reference) policy_ref;
// on a *temporary* it returns a value-owning policy_buffer, so the snapped result can
// be returned/stored without dangling. Either way the target's own policy (clamp) and
// the handle's rounding merge via uses_flag.
//---------------------------------------------------------------------------
// policy<round_toward_zero>() proxy converts to a value (RHS / return position)
TEST(PolicyHandleTest, the_handle_converts_to_a_value) {
    using small = small_t;
    small a     = 2.5; // exact (dyadic) on the 1/10 grid

    // a*a is 6.25 on the finer 1/100 grid — off small's notch. These compile because
    // the proxy converts to `small`, applying clamp (range) + round_toward_zero (notch).
    small trunc   = (a * a).policy<round_toward_zero>(); // 6.25 → 6.2 (truncate)
    small nearest = (a * a).policy<round_nearest>();     // 6.25 → 6.3 (nearest)
    ASSERT_EQ(trunc, 6.2_r);
    ASSERT_EQ(nearest, 6.3_r);

    // Out-of-range still clamps, via small's own policy merged in at conversion.
    small four = 4;
    ASSERT_EQ(small((four * four).policy<round_toward_zero>()), 10); // 16 → clamp → 10

    // Safety: `auto square(){ return (n*n).policy<round_toward_zero>(); }` returns the value-owning
    // buffer; converting it to `small` after the temporary is gone is well-defined.
    small from_temp = square(small{2.5});
    small sq        = square(small{4});
    ASSERT_EQ(from_temp, 6.2_r);
    ASSERT_EQ(sq, 10);

    // Receiver category picks the proxy kind: lvalue → policy_ref (reference),
    // rvalue → policy_buffer (owns the value). Both convert to small; neither to a
    // target whose interval the value can never enter (conversion stays constrained).
    using big = decltype(small{} * small{});
    static_assert(std::is_same_v<decltype(std::declval<big&>().policy<round_toward_zero>()),
                                 policy_ref<big, call_policy<flags_of<big> | round_toward_zero>>>);
    static_assert(std::is_same_v<decltype(std::declval<big>().policy<round_toward_zero>()),
                                 policy_buffer<big, call_policy<flags_of<big> | round_toward_zero>>>);
    static_assert(std::is_convertible_v<decltype(std::declval<big&>().policy<round_toward_zero>()), small>);
    static_assert(std::is_convertible_v<decltype(std::declval<big>().policy<round_toward_zero>()), small>);
    // big keeps small's clamp (a result keeps its operands' shared policy), so it
    // clamps into any target; a checked product stays constrained.
    static_assert(
        std::is_convertible_v<decltype(std::declval<big&>().policy<round_toward_zero>()), inside<{{1000, 2000}, 1}>>);
    using checked_big = decltype(inside<{0, 10}>{} * inside<{0, 10}>{});
    static_assert(!std::is_convertible_v<decltype(std::declval<checked_big&>().policy<round_toward_zero>()),
                                         inside<{{1000, 2000}, 1}>>);
}
} // namespace with_methods_tests

// The replaceable failure handler (beman/inside/inside/detail/error.hpp). A checked violation
// funnels through beman::inside::detail::raise -> the installed handler. By default that
// throws beman::inside::inside_error carrying the originating errc; set_error_handler swaps
// in a user handler (e.g. a bare-metal reset/log) without any <system_error>
// dependency. These tests cover both the default and a custom handler.
namespace error_handler_tests {
using namespace beman::inside;

namespace {
// A custom [[noreturn]] handler that records the code, then escapes via a
// throw of its own type so the test can observe it (a bare-metal handler
// would reset/halt instead).
struct handler_fired {
    errc        code;
    const char* what;
};
errc g_seen{};

[[noreturn]] void recording_handler(errc code, const char* what) {
    g_seen = code;
    throw handler_fired{code, what};
}

// RAII guard so a thrown handler still restores the default.
struct scoped_handler {
    error_handler_t prev;
    explicit scoped_handler(error_handler_t h) : prev(set_error_handler(h)) {}
    ~scoped_handler() { set_error_handler(prev); }
};
} // namespace

// default handler throws inside_error carrying the code / overflow on out-of-range assignment
TEST(ErrorHandlerTest, default_handler_throws_inside_error_carrying_the_code__overflow_on_out_of_range_assignment) {
    using c100 = inside<{0, 100}, checked>;

    {
        SCOPED_TRACE("overflow on out-of-range assignment");
        try {
            c100 x{200};
            (void)x;
            FAIL() << "expected throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::overflow);
        }
    }

    // what() defaults to the static category message.
    try {
        c100 x{200};
        (void)x;
    } catch (const inside_error& e) {
        ASSERT_EQ(std::string_view{e.what()}, errc_message(errc::overflow));
    }
}

// default handler throws inside_error carrying the code / rounding_error on off-notch checked cast
TEST(ErrorHandlerTest,
     default_handler_throws_inside_error_carrying_the_code__rounding_error_on_off_notch_checked_cast) {
    using c100 = inside<{0, 100}, checked>;

    {
        SCOPED_TRACE("rounding_error on off-notch checked cast");
        using coarse = inside<{{0, 10}, 2}>; // notch 2: 3 doesn't land
        try {
            (void)checked_cast<coarse>(3);
            FAIL() << "expected throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::rounding_error);
        }
    }

    // what() defaults to the static category message.
    try {
        c100 x{200};
        (void)x;
    } catch (const inside_error& e) {
        ASSERT_EQ(std::string_view{e.what()}, errc_message(errc::overflow));
    }
}

// default handler throws inside_error carrying the code / not_finite on non-finite double input
TEST(ErrorHandlerTest, default_handler_throws_inside_error_carrying_the_code__not_finite_on_non_finite_double_input) {
    using c100 = inside<{0, 100}, checked>;

    {
        SCOPED_TRACE("not_finite on non-finite double input");
        using R = inside<{0.0, 1.0}>;
        try {
            R r{std::numeric_limits<double>::infinity()};
            (void)r;
            FAIL() << "expected throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::not_finite);
        }
    }

    // what() defaults to the static category message.
    try {
        c100 x{200};
        (void)x;
    } catch (const inside_error& e) {
        ASSERT_EQ(std::string_view{e.what()}, errc_message(errc::overflow));
    }
}

// set_error_handler redirects failures and is restorable
TEST(ErrorHandlerTest, set_error_handler_redirects_failures_and_is_restorable) {
    using c100 = inside<{0, 100}, checked>;

    error_handler_t before = get_error_handler();
    {
        scoped_handler guard{&recording_handler};
        ASSERT_EQ(get_error_handler(), &recording_handler);

        g_seen = errc{};
        try {
            c100 x{200};
            (void)x;
        } catch (const handler_fired& f) {
            ASSERT_EQ(f.code, errc::overflow);
        }
        ASSERT_EQ(g_seen, errc::overflow);
    }
    // Default restored after the guard.
    ASSERT_EQ(get_error_handler(), before);
    ASSERT_THROW((void)((c100{200})), inside_error);

    // A null handler restores the default too.
    set_error_handler(&recording_handler);
    set_error_handler(nullptr);
    ASSERT_EQ(get_error_handler(), before);
}

// A zero divisor in a compound division reaches the installed handler like
// any failed store. (No inside stores a std::expected; a rational's
// arithmetic, whose zero divisor is a compile error, runs only at compile time.)
TEST(ErrorHandlerTest, compound_zero_divisor_reaches_the_handler) {
    scoped_handler guard{&recording_handler};
    g_seen = errc{};
    try {
        inside<{0, 10}>      x{5};
        const inside<{0, 1}> zero{0};
        x /= zero;
    } catch (const handler_fired& f) {
        EXPECT_EQ(f.code, errc::division_by_zero);
    }
    EXPECT_EQ(g_seen, errc::division_by_zero);
}

// The handler is atomic: installing one while other threads raise is no data
// race (the weekly TSan run checks it).
TEST(ErrorHandlerTest, handler_swaps_while_other_threads_raise) {
    using c100 = inside<{0, 100}, checked>;
    // No shared state in the handler: only the handler pointer is contended.
    constexpr error_handler_t quiet  = [](errc code, const char* what) { throw handler_fired{code, what}; };
    const error_handler_t     before = get_error_handler();
    std::atomic<int>          caught{0};
    std::vector<std::thread>  raisers;
    for (int t = 0; t < 4; ++t)
        raisers.emplace_back([&] {
            for (int i = 0; i < 200; ++i)
                try {
                    c100 x{200};
                    (void)x;
                } catch (const inside_error&) {
                    ++caught;
                } catch (const handler_fired&) {
                    ++caught;
                }
        });
    for (int i = 0; i < 200; ++i) {
        const error_handler_t prev = set_error_handler(quiet);
        set_error_handler(prev);
    }
    for (auto& r : raisers)
        r.join();
    EXPECT_EQ(caught.load(), 4 * 200);
    EXPECT_EQ(get_error_handler(), before);
}
} // namespace error_handler_tests

// The error vocabulary has two shapes, each by role (see docs/internals.md):
//   policy cascade        — narrowing INTO an inside,
//   std::expected<T,errc> — every fallible result: inside arithmetic (division,
//                           modulo, checked rational overflow), queries and math.
// These tests cover the expected-lift operators that chain those results.
namespace error_vocabulary_tests {
using namespace beman::inside;

namespace {
// Mixed-sign sqrt: deterministic expected results under both engines.
using sq_in = inside<{{-4, 4}, per<256>}, round_nearest>;
using num_t = inside<{0, 100}>;
using den_t = inside<{-5, 5}>; // spans zero → division returns expected

std::expected<num_t, errc> num_ok(int v) { return num_t{v}; }
std::expected<num_t, errc> num_err(errc e) { return std::unexpected{e}; }
} // namespace

// expected-lift operators keep the cause end to end
TEST(ErrorVocabularyTest, expected_lift_operators_keep_the_cause_end_to_end) {
    // Value path: identical to the manual unwrap.
    auto r = math::sqrt(sq_in{1}) + num_t{4};
    static_assert(detail::expected_like<decltype(r)>);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, *math::sqrt(sq_in{1}) + num_t{4});
    ASSERT_EQ(*r, 5);

    // Error path: the errc propagates through the whole chain.
    auto e = (math::sqrt(sq_in{-1}) + num_t{4}) * num_t{2};
    ASSERT_TRUE(!e.has_value());
    ASSERT_EQ(e.error(), errc::domain_error);

    // First (left) error wins when two errors meet.
    auto both = num_err(errc::domain_error) + num_err(errc::overflow);
    ASSERT_TRUE(!both.has_value());
    ASSERT_EQ(both.error(), errc::domain_error);
}

// inside division returns expected with the division_by_zero cause
TEST(ErrorVocabularyTest, inside_division_reports_division_by_zero) {
    auto q = num_t{10} / den_t{2};
    static_assert(detail::expected_like<decltype(q)>);
    ASSERT_TRUE(q.has_value());
    ASSERT_EQ(rational{*q}, 5);

    auto z = num_t{10} / den_t{0};
    ASSERT_TRUE(!z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);

    // A division error keeps its cause through the rest of the chain.
    auto chain = num_t{10} / den_t{0} * num_t{3} + num_t{1};
    ASSERT_TRUE(!chain.has_value());
    ASSERT_EQ(chain.error(), errc::division_by_zero);
}

// division inside an expected chain reports its own cause
TEST(ErrorVocabularyTest, division_inside_an_expected_chain_reports_its_cause) {
    auto q = num_ok(10) / den_t{2};
    ASSERT_TRUE(q.has_value());
    ASSERT_EQ(rational{*q}, 5);

    auto z = num_ok(10) / den_t{0};
    ASSERT_TRUE(!z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);
}

// operator% chains through expected
TEST(ErrorVocabularyTest, modulo_chains_through_expected) {
    // mod requires a rounding mode in the merged policy (integer-valued grids).
    using mnum = inside<{0, 100}, round_toward_zero>;
    using nz   = inside<{1, 5}, round_toward_zero>;  // divisor grid excludes zero
    using span = inside<{-5, 5}, round_toward_zero>; // divisor grid spans zero
    auto en    = []() -> std::expected<mnum, errc> { return mnum{17}; };

    auto m = en() % nz{5};
    ASSERT_TRUE(m.has_value());
    ASSERT_EQ(*m, 2);

    // value path and zero-divisor cause.
    auto q = en() % span{5};
    ASSERT_TRUE(q.has_value());
    ASSERT_EQ(rational{*q}, 2);
    auto z = en() % span{0};
    ASSERT_TRUE(!z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);
}
} // namespace error_vocabulary_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

template <class T, class U = T>
constexpr bool proxy_add_ok = requires(T a, U b, errc& e) { a.policy(e) += b; };
template <class T, class U = T>
constexpr bool plain_add_ok = requires(T a, U b) { a += b; };
template <class Ref, class E>
concept stores_expected = requires(Ref r, E e) { r = e; };

// += of an inside under wrap: the new raw's offset from a negative raw_lo
// passed the work type.
TEST(PolicyTest, wrap_offset_from_a_negative_lower) {
    using L = inside<{-(1ll << 62), 1ll << 61}, wrap>;
    using R = inside<{0, 1ll << 62}>;
    L x{1ll << 61};
    x += R{1ll << 62};
    EXPECT_EQ(x, -1_ins);
    L y{-(1ll << 62)};
    y -= R{1ll << 62};
    EXPECT_EQ(to_string(y), "-2305843009213693951");
}

// wrap folds a value whose 64-bit rational steps overflow exactly: it
// reported errc::overflow (a double past 2^63, INT64_MIN into a 1/4 grid, a
// far value on a 2^-62 grid).
TEST(PolicyTest, wrap_folds_past_the_64_bit_rational) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    using quarter_wrap             = inside<{{-2, 3}, per<4>}, wrap>;
    using halves_ceil              = inside<{{0.5, 10.5}, 1}, wrap | round_ceil>;
    using digit                    = inside<{0, 9}, wrap>;
    using fine                     = inside<{{0, 1}, per<(1ll << 62)>}, wrap>;
    EXPECT_EQ(*quarter_wrap::try_make(full{i64_min}), 2.5_ins);
    EXPECT_EQ(*halves_ceil::try_make(full{i64_min}), 3.5_ins);
    EXPECT_EQ(*digit::try_make(-0x1p63), 2_ins);
    EXPECT_EQ(*digit::try_make(1e30), 6_ins); // the double's exact value, mod 10
    EXPECT_EQ(to_string(*full::try_make<wrap>(1e30)), "5076964154930102272");
    EXPECT_EQ(*fine::try_make(rational{4}), (rational{(1ull << 62) - 3, 1ll << 62})); // 4 mod (1 + 2^-62)
    EXPECT_EQ(digit::try_make(1.5).error(), errc::rounding_error);                    // no rounding flag
}

// A flag (`clamp`, `round_floor`) or a const policy(ec) as the constructor's
// one-shot policy did not compile: report() was non-const.
TEST(PolicyTest, named_policy_in_a_constructor) {
    using halves = inside<{{0.5, 10.5}, 1}>; // Lower off the notch's multiples
    using digit  = inside<{0, 9}>;
    static_assert(digit{12, clamp} == 9_ins && digit{12, wrap} == 2_ins);
    static_assert(halves{7, round_floor} == 6.5_ins);
    errc       ec{};
    const auto pol = make_policy<checked>(ec);
    digit      y{12, pol};
    (void)y;
    EXPECT_EQ(ec, errc::overflow);
}

// clamp saturated every out-of-range value but a double finer than 2^-62:
// on a continuous grid it reported overflow.
TEST(PolicyTest, continuous_grid_clamps_a_fine_double) {
    constexpr grid sevenths{interval{0, 1}, max_denominator{7}};
    EXPECT_EQ(*(inside<{{0, 1}, max_denominator{1000}}, clamp>::try_make(-1e-20)), 0);
    EXPECT_EQ(*(inside<{{1, 2}, max_denominator{1000}}, clamp>::try_make(1e-20)), 1);
    EXPECT_EQ((*inside<sevenths, clamp>::try_make(-1e-20)), 0);
    EXPECT_EQ((inside<{{0, 1}, max_denominator{1000}}>::try_make(-1e-20).error()), errc::overflow);
}

TEST(PolicyTest, policy_handle_compound_is_constrained) {
    using sixteenth = inside<{{0, 8}, per<16>}>;
    using whole     = inside<{-100, 100}>;
    static_assert(!proxy_add_ok<whole, sixteenth> && !plain_add_ok<whole, sixteenth>);
    static_assert(proxy_add_ok<inside<{0, 100}>> && proxy_add_ok<sixteenth, whole>);
}

// on_error was dropped when the value to store came from a continuous inside
// (a quotient) and was off the notch: the store reported to the policy, which
// threw, instead of calling the handler.
TEST(PolicyTest, on_error_sees_rounding_from_a_continuous_source) {
    using I = inside<{0, 100}>;
    I    w{7};
    errc cb{};
    w.on_error([&](auto&, errc c, const char*) { cb = c; }) /= I{2}; // 7/2
    EXPECT_EQ(cb, errc::rounding_error);
    EXPECT_EQ(w, 7);
}

TEST(PolicyTest, policy_handle_expected_sink_reports_into_errc) {
    using ref = decltype(std::declval<inside<{0, 100}>&>().policy(std::declval<errc&>()));
    static_assert(!stores_expected<ref, std::expected<rational, errc>>);
    static_assert(stores_expected<ref, rational>);
}

// A failed read-out through the errc& proxy returned an uninitialized
// target (a compile error in constant evaluation).
TEST(PolicyTest, policy_handle_failed_read_out_is_initialized) {
    static_assert([] {
        errc                 ec{};
        inside<{0, 100}>     x{50};
        const inside<{0, 9}> s = x.policy(ec); // 50 is past 9: overflow into ec
        return ec == errc::overflow && s == 0;
    }());
}

// wrap on a bounded continuous grid folded before it rounded.
TEST(PolicyTest, wrap_rounds_first_on_a_bounded_grid) {
    using S = inside<{{0, 1}, max_denominator{2}}, wrap | round_toward_zero>;
    using N = inside<{{0, 1}, max_denominator{2}}, wrap | round_nearest>;
    using C = inside<{{0, 1}, max_denominator{7}}, wrap | round_ceil>;
    EXPECT_EQ(S{-1.3}, 0);
    EXPECT_EQ(N{-1.25}, rational(1, 2));
    EXPECT_EQ(C{rational(-97, 8)}, 0);
    EXPECT_EQ(N{2.75}, 0);
}

// on_clamp did not fire for an infinity, though it was clamped.
TEST(PolicyTest, on_clamp_hears_of_infinities) {
    inside<{0, 100}> x{50};
    int              fired = 0;
    bool             top   = false;
    x.on_clamp([&](auto&, auto o) { // an inside: ±∞ reports the end of its grid
        ++fired;
        top = o == just<upper_of<decltype(o)>>;
    })                     = std::numeric_limits<double>::infinity();
    EXPECT_EQ(x, 100);
    EXPECT_EQ(fired, 1);
    EXPECT_TRUE(top);
    x.on_clamp([&](auto&, auto) { ++fired; }) = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(x, 0);
    EXPECT_EQ(fired, 2);
}

// An on_error action ranked above the type's clamp / wrap policy: the store
// reported overflow (and kept the old value) instead of saturating or folding.
TEST(PolicyTest, on_error_does_not_preempt_clamp_or_wrap) {
    errc code{};
    auto record = [&](auto&, errc c, const char*) { code = c; };

    inside<{0, 100}, clamp> c{0};
    c.on_error(record) = 150;
    EXPECT_EQ(code, errc{});
    EXPECT_EQ(c, 100);

    inside<{0, 59}, wrap> w{0};
    w.on_error(record) = 61;
    EXPECT_EQ(code, errc{});
    EXPECT_EQ(w, 1);

    inside<{{rational{1, 2}, rational{21, 2}}, 1}, round_half_even | clamp> h{rational{1, 2}};
    h.on_error(record) = 1e30;
    EXPECT_EQ(code, errc{});
    EXPECT_EQ(h, rational(21, 2));

    inside<{0, 100}> u{7}; // no clamp: on_error still replaces the failure
    u.on_error(record) = 150;
    EXPECT_EQ(code, errc::overflow);
    EXPECT_EQ(u, 7);
}

//---------------------------------------------------------------------------
// assignment<L, double> (assignment.hpp)
//
// `assign(insidable, double R)` checked for `uses_flag<L, P, clamp>` but not
// for `uses_flag<L, P, wrap>`. An `inside<{...}, wrap>` constructed from a
// double silently stores the unwrapped value (which may be out of range)
// because it falls through `range_fail` without `checked` set.
//
// Runtime-only: wrap policy is bypassed in constant evaluation (the
// `is_constant_evaluated()` throw in assignment::assign fires before the
// policy machinery can react).
//---------------------------------------------------------------------------
TEST(PolicyTest, wrap_fires_for_a_double) {
    using L = inside<{0, 100}, wrap>;

    // 120 wraps once into [0, 100] → 19 (since the range is 101 inclusive).
    ASSERT_EQ(L{double{120.0}}, 19);

    // negative wraps to the upper side
    ASSERT_EQ(L{double{-5.0}}, 96);

    // signed-range wrap
    using S = inside<{-50, 50}, wrap>;
    // 75 wraps once: 75 - 101 = -26.
    ASSERT_EQ(S{double{75.0}}, -26);
}

TEST(PolicyTest, one_shot_policy_applies_to_dyadic_target) {
    using RealSmall = beman::inside::inside<{{0, 4}, beman::inside::per<4>}>;
    using RealWide  = beman::inside::inside<{{-8, 8}, beman::inside::per<4>}>;
    using namespace beman::inside;
    const RealWide big = RealWide{7.5}, low = RealWide{-1.0};
    EXPECT_EQ(rational{(RealSmall{big, clamp})}, rational{4.0});
    EXPECT_EQ(rational{(RealSmall{low, clamp})}, rational{0.0});
    EXPECT_EQ(rational{(RealSmall{big, wrap})}, rational{7.5 - 4.25});
    RealSmall r       = RealSmall{1.0};
    r.policy<clamp>() = big;
    EXPECT_EQ(rational{r}, rational{4.0});
}

//---------------------------------------------------------------------------
// clamp / wrap take an integral source whose whole type range misses the grid.
//---------------------------------------------------------------------------
TEST(PolicyTest, clamp_wrap_from_disjoint_integral_type) {
    static_assert(inside<{{1000, 2000}, 1}, clamp>{std::uint8_t{5}}.as<int>() == 1000);
    static_assert(inside<{{1000, 1009}, 1}, wrap>{std::uint8_t{5}}.as<int>() == 1005);
    static_assert(inside<{1000, 2000}>{std::uint8_t{5}, clamp}.as<int>() == 1000);
}

// A policy_ref compound /= or %= with a zero divisor reports (or, under
// unsafe, no-ops) like the member operator, and never divides by zero.
TEST(PolicyTest, policy_handle_zero_divisor_does_not_divide) {
    using X = inside<{0, 10}, checked | round_toward_zero>;
    using Z = inside<{0, 10}, unsafe>;
    X b{6};
    b.policy<unsafe>() /= X{0};
    b.policy<unsafe>() %= X{0};
    b.policy() /= Z{0};
    EXPECT_EQ(b, 6);
    EXPECT_THROW(b.policy() /= X{0}, inside_error);
    EXPECT_THROW(b.policy() %= X{0}, inside_error);
    errc ec{};
    b.policy(ec) %= X{0};
    EXPECT_EQ(ec, errc::division_by_zero);
    EXPECT_EQ(b, 6);
    b.policy() /= X{3};
    EXPECT_EQ(b, 2);
}

//---------------------------------------------------------------------------
// inside.hpp — operator+=(insidable) result out of range, checked policy with
// no clamp/wrap handler -> report (throws).
//---------------------------------------------------------------------------
// checked += insidable overflow reports (throws)
TEST(PolicyTest, checked_compound_overflow_throws) {
    using c100 = inside<{0, 100}, checked>;
    c100 a{80};
    c100 b{50};
    ASSERT_THROW((void)(a += b), beman::inside::inside_error); // 130 not in [0,100]
}

//---------------------------------------------------------------------------
// assignment<L, double> (assignment.hpp) — an error_action on an
// out-of-interval *fractional* rhs.
// The existing on_error test assigns an integer (a different assign overload);
// this drives the fractional (double) overload.
//---------------------------------------------------------------------------
// on_error fires for an out-of-range double assignment
TEST(PolicyTest, on_error_fires_for_an_out_of_range_double_assignment) {
    using c100 = inside<{0, 100}, checked>;
    c100 e{50};
    bool fired = false;
    e.on_error([&](auto& self, errc code, std::string_view msg) {
        fired = (code == errc::overflow) && !msg.empty();
        self  = 0;
    })         = 200.5; // fractional, out of [0,100]
    ASSERT_TRUE(fired);
    ASSERT_EQ(e, 0);
}

//---------------------------------------------------------------------------
// inside/detail/call_policy.hpp — action-decorated operator-=(insidable) on the path that does
// not fail (existing tests only exercise the failing arm).
//---------------------------------------------------------------------------
// on_error compound subtract that does not fail
TEST(PolicyTest, on_error_compound_subtract_that_does_not_fail) {
    using c100 = inside<{0, 100}, checked>;
    c100 acc{50};
    bool fired = false;
    acc.on_error([&](auto&, errc, const char*) { fired = true; }) -= 10_ins; // 40, in range
    ASSERT_FALSE(fired);
    ASSERT_EQ(acc, 40);
}

//---------------------------------------------------------------------------
// an off-notch fractional store (assignment.hpp) on a policy with `clamp`:
// clamp handles only the range, so without a rounding mode the value is a rounding
// error; `clamp | round_toward_zero` truncates an in-range off-notch value.
//---------------------------------------------------------------------------
// clamp | round_toward_zero truncates an in-range off-notch value; clamp
// alone handles only the range, so the off-notch value is a rounding error.
TEST(PolicyTest, clamp_truncates_an_off_notch_value_in_range) {
    using strict = inside<{{0, 10}, per<2>}, clamp>;
    ASSERT_EQ(strict::try_make(0.3).error(), errc::rounding_error);

    using b = inside<{{0, 10}, per<2>}, clamp | round_toward_zero>; // notch 1/2
    b x{0};
    x = 0.3; // in range, off the 1/2 grid → truncates to 0
    ASSERT_EQ(x, 0);

    x = 0.9; // 0.9 → 1.8 half-notches → truncates to 1 half → 0.5
    ASSERT_TRUE((static_cast<rational>(x) == rational{1u, 2}));
}

// errc_message stringifies every errc
TEST(PolicyTest, errc_message_stringifies_every_errc) {
    ASSERT_EQ(std::string_view{errc_message(errc::domain_error)}, "argument outside the function's domain");
    ASSERT_EQ(std::string_view{errc_message(errc::division_by_zero)}, "division by zero");
    ASSERT_EQ(std::string_view{errc_message(errc::overflow)}, "value does not fit its range");
    ASSERT_EQ(std::string_view{errc_message(errc::rounding_error)}, "value is not on the grid");
    ASSERT_EQ(std::string_view{errc_message(errc::not_finite)}, "non-finite floating-point value");
    ASSERT_EQ(std::string_view{errc_message(errc::invalid_format)}, "malformed number");
    ASSERT_EQ(std::string_view{errc_message(static_cast<errc>(999))}, "unknown inside error");
}

//---------------------------------------------------------------------------
// Case 1 — runtime range branch elision under `unsafe`
//---------------------------------------------------------------------------
// unsafe + no action: out-of-range int rhs stores without throwing
TEST(PolicyTest, unsafe_stores_without_a_range_check) {
    using L = inside<{0, 100}, unsafe>;
    // The += must not throw, even with an out-of-range value. Storage holds
    // the raw value as-is (UB on read, but that's the `unsafe` contract).
    L b{50};
    // A delta inside with Lower==0 keeps the raw-add fast path (a singleton `200_ins`
    // would widen to a disjoint grid that only clamp/wrap can absorb).
    ASSERT_NO_THROW((void)((b += inside<{0, 200}>{200})));
    // No assertion on the value — `unsafe` doesn't promise meaningful behaviour
    // for out-of-range writes, only that they don't throw.
}

// checked + no action: range check still fires
TEST(PolicyTest, checked_range_check_fires) {
    using L = inside<{0, 100}, checked>;
    L b{50};
    ASSERT_THROW((void)((b += inside<{0, 200}>{200})), beman::inside::inside_error);
}

// clamp policy: range check still fires
TEST(PolicyTest, clamp_range_check_fires) {
    using L = inside<{0, 100}, clamp>;
    L b{50};
    b += inside<{0, 200}>{200};
    ASSERT_EQ(b, 100);
}

// wrap policy: range check still fires
TEST(PolicyTest, wrap_range_check_fires) {
    using L = inside<{0, 100}, wrap>;
    L b{50};
    b += inside<{0, 200}>{200};
    ASSERT_EQ(b, 48); // (50 + 200 - 0) mod 101 + 0 = 250 mod 101 = 48
}

} // namespace pinned_tests
