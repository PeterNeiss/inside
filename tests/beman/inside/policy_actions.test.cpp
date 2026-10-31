// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string_view>

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
        overshoot = over; // integer RHS: over is imax
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
    using u100ic = inside<{0, 100}, checked | snap>;
    c100 d{50}, z{0};
    auto q = div(d, z, make_policy<round_nearest>());
    ASSERT_FALSE(q.has_value());
    ASSERT_EQ(q.error(), errc::division_by_zero);
    u100ic l{7}, r{0};
    auto   m = mod(l, r);
    ASSERT_FALSE(m.has_value());
    ASSERT_EQ(m.error(), errc::division_by_zero);
}

// A rational right-hand side past the 64-bit rational fails like a store:
// on_error takes it, and the value is unchanged.
TEST(PolicyActionsTest, rational_rhs_overflow_reaches_on_error) {
    using frac = inside<{{0, 1}, max_denominator{imax{1} << 62}}>;
    frac f{rational{1, (imax{1} << 62) - 1}};
    errc seen{};
    f.on_error([&](auto&, errc c, const char*) { seen = c; }) += rational{1, (imax{1} << 62) - 3};
    ASSERT_EQ(seen, errc::overflow);
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
    using u100ic = inside<{0, 100}, checked | snap>;
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
