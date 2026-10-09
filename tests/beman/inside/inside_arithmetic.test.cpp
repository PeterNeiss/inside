// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <limits>
#include <type_traits>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;

// inside add
TEST(InsideArithmeticTest, inside_add) {
    using u8 = inside<{10, 255}>;
    constexpr u8 a{16};
    constexpr u8 b{220};
    static_assert(a + one == 17);
    static_assert(b + one == 221);

    static_assert(a + b == 236);

    // -ve result widens to signed
    static_assert(a - b == -204);

    // works at uint64 max
    using u64 = inside<{{0u, std::numeric_limits<std::uint64_t>::max()}, 1}>;
    constexpr u64 biggest{std::numeric_limits<std::uint64_t>::max()};
    static_assert(biggest == rational{std::numeric_limits<std::uint64_t>::max()});
}

// inside add: mixed notch with offset storage
TEST(InsideArithmeticTest, inside_add_mixed_notch_with_offset_storage) {
    // Regression: the notch-offset add path must scale each operand's raw from
    // its own notch up to the result notch (lhs_widen = notch_of<L>/notch_of<result>).
    // A previous inversion left the scale at 1 whenever notches differed, so the
    // sum was only correct when the lhs offset happened to be 0 or notches were
    // equal. These exercise different notches AND a non-zero (and negative-Lower)
    // offset, which is where the bug surfaced.
    using fine   = inside<{{-4, 4}, per<16>}, round_nearest>;
    using coarse = inside<{{-8, 8}, per<256>}, round_nearest>;
    using offset = inside<{{0, 4}, per<16>}, round_nearest>;

    ASSERT_EQ((fine{0} + coarse{2}), 2);   // was -1.75
    ASSERT_EQ((offset{1} + coarse{2}), 3); // was 2.0625
    ASSERT_EQ((fine{3} + coarse{2}), 5);
    ASSERT_EQ((coarse{2} - fine{1}), 1); // sub routes through add(-r)

    // exact fractional result via the integer-pair accessors
    auto frac = fine{rational{1, 2}} + coarse{rational{1, 4}};
    ASSERT_EQ(frac.numerator(), 3); // 3/4
    ASSERT_EQ(frac.denominator(), 4);
}

// inside mul
TEST(InsideArithmeticTest, inside_mul) {
    using r = inside<{10, 255, 1}>;
    constexpr r a{16};
    constexpr r b{102};
    static_assert(a * b == 1632);

    // Fractional-notch ops require runtime construction from `double` (the
    // f64-valued assignment specialization is not constexpr-evaluable).
    using u4 = inside<{{0.75, 10.5}, 0.25}>;
    u4   d{3};
    u4   e{3.25};
    auto f = d * e;
    ASSERT_EQ(f, *(39_r / 4));

    using n4 = u4::negative;
    n4   g{-2};
    n4   h{-2.25};
    auto i = g * h;
    ASSERT_EQ(i, *(9_r / 2));

    // Mixed signs
    ASSERT_EQ(d * g, -6);
    ASSERT_EQ(g * d, -6);
}

// inside div: rational vs integer paths
TEST(InsideArithmeticTest, inside_div_rational_vs_integer_paths) {
    {
        SCOPED_TRACE("default returns rational raw");
        using r = inside<{1, 255}>;
        constexpr r    a{102};
        constexpr r    b{16};
        constexpr auto c = a / b; // zero-free divisor: a plain value
        static_assert(c == *(51_r / 8));
    }

    {
        SCOPED_TRACE("snap selects integer-storage div path");
        using ui = inside<{0, 100}, snap>;
        constexpr ui   a{51}, b{8};
        constexpr auto e = a / b;
        static_assert(!(std::is_same_v<typename decltype(e)::value_type::raw_type, rational>));
        static_assert(*e == 6);
    }

    {
        SCOPED_TRACE("per-call snap");
        using u100 = inside<{0, 100}>;
        constexpr u100 a{51}, b{8};
        constexpr auto d = div(a, b, snapped);
        static_assert(!(std::is_same_v<typename decltype(d)::value_type::raw_type, rational>));
        static_assert(*d == 6);
    }

    {
        SCOPED_TRACE("division by zero -> errc::division_by_zero");
        using ui = inside<{0, 100}, snap>;
        ui a{51}, zero{0};
        ASSERT_FALSE((a / zero).has_value());
        ASSERT_EQ((a / zero).error(), errc::division_by_zero);
    }

    {
        SCOPED_TRACE("offset-encoded storage takes integer path");
        using off = inside<{5, 100}>;
        static_assert(!(!index_raw<off>));
        static_assert(index_raw<off>);

        constexpr off  a{50}, b{10};
        constexpr auto q = div(a, b, snapped);
        // off's grid {5,100} excludes zero, so div returns a plain inside (no expected).
        static_assert(!(std::is_same_v<typename decltype(q)::raw_type, rational>));
        static_assert(q == 5);
    }

    {
        SCOPED_TRACE("non-unit notch trunc");
        using step2 = inside<{{0, 10}, 2}>;
        constexpr step2 a{10}, b{6};
        constexpr auto  q = div(a, b, snapped);
        static_assert(*q == 1);
    }

    {
        SCOPED_TRACE("offset + non-unit notch");
        using step5 = inside<{{5, 15}, 5}>;
        constexpr step5 a{15}, b{5};
        constexpr auto  q = div(a, b, snapped);
        // {5,15} excludes zero → plain inside result.
        static_assert(q == 3);
    }

    {
        SCOPED_TRACE("signed integer division truncates toward zero");
        using si = inside<{-100, 100}, snap>;
        constexpr si   a{-7}, b{2};
        constexpr auto q = a / b;
        static_assert(*q == -3);
    }

    {
        SCOPED_TRACE("divisor grid excluding zero yields a non-expected result");
        using num   = inside<{0, 100}, snap>;
        using pos   = inside<{1, 10}, snap>;  // grid excludes zero
        using spanz = inside<{-5, 10}, snap>; // grid straddles zero

        constexpr num   a{42};
        constexpr pos   p{3};
        constexpr spanz s{2};

        // Divisor proven nonzero at compile time → plain inside, no unwrap needed.
        static_assert(insidable<decltype(a / p)>);
        static_assert(!(is_expected_v<decltype(a / p)>));
        static_assert(a / p == 14);

        // Divisor whose grid contains zero → std::expected<inside, errc>.
        static_assert(is_expected_v<decltype(a / s)>);
        ASSERT_TRUE((a / s).has_value());
        ASSERT_EQ(*(a / s), 21);
    }
    {
        SCOPED_TRACE("exact division: a zero-free divisor whose quotient provably fits");
        using val = inside<{-100, 100}>;
        using pos = inside<{1, 100}>;
        constexpr val n{-7};
        constexpr pos d{2};
        // The exact quotient's 64-bit rational cannot overflow on these grids, and
        // the divisor excludes zero: a plain value, nothing to unwrap.
        static_assert(!is_expected_v<decltype(n / d)>);
        static_assert(static_cast<rational>(n / d) == rational{-7, 2});
        // A divisor grid containing zero still reports.
        static_assert(is_expected_v<decltype(n / val{2})>);
        // Grids whose values need 2^40 denominators: the quotient may pass 64
        // bits, so the checked rational path stays.
        using fine = inside<{{0, 1}, rational{1, umax{1} << 40}}>;
        using fpos = inside<{{1, 2}, rational{1, umax{1} << 40}}>;
        static_assert(is_expected_v<decltype(fine{0.5} / fpos{1.5})>);
        EXPECT_EQ(static_cast<rational>(*(fine{0.5} / fpos{1.5})), (rational{1, 3}));
    }
}

// inside modulo
TEST(InsideArithmeticTest, inside_modulo) {
    using ui = inside<{0, 100}, snap>;
    constexpr ui a{17}, b{5};
    static_assert(*(a % b) == 2);

    constexpr ui c{100}, d{10};
    static_assert(*(c % d) == 0);

    // mod-by-zero — runtime only: the `is_constant_evaluated()` throw short-circuits.
    ui a_rt{17}, zero{0};
    ASSERT_FALSE((a_rt % zero).has_value());
    ASSERT_EQ((a_rt % zero).error(), errc::division_by_zero);

    using si = inside<{-100, 100}, snap>;
    constexpr si sa{-17}, sb{5};
    static_assert(*(sa % sb) == -2);

    using u50 = inside<{0, 50}>;
    constexpr u50  e{23}, f{7};
    constexpr auto r = mod(e, f, snapped);
    static_assert(*r == 2);
}

// inside expected ops propagate the error
TEST(InsideArithmeticTest, inside_expected_ops_propagate_error) {
    using u8 = inside<{1, 255}>;
    constexpr u8 a{100}, b{10};
    struct ex {
        static constexpr std::expected<u8, errc> ok(u8 v) { return v; }
        static constexpr std::expected<u8, errc> none() { return std::unexpected{errc::overflow}; }
    };

    // +
    static_assert(*(ex::ok(a) + b) == 110);
    static_assert(*(a + ex::ok(b)) == 110);
    static_assert(*(ex::ok(a) + ex::ok(b)) == 110);
    static_assert(!((ex::none() + b).has_value()));
    static_assert(!((a + ex::none()).has_value()));
    static_assert((a + ex::none()).error() == errc::overflow);

    // -
    static_assert(*(ex::ok(a) - b) == 90);
    static_assert(!((ex::none() - b).has_value()));

    // *
    static_assert(*(ex::ok(a) * b) == 1000);
    static_assert(!((a * ex::none()).has_value()));

    // /
    static_assert((ex::ok(a) / b).has_value());
    static_assert(*(ex::ok(a) / b) == 10);
    static_assert(!((ex::none() / b).has_value()));
}

// inside rational-storage expected ops do not double-wrap
TEST(InsideArithmeticTest, inside_rational_storage_expected_ops_do_not_double_wrap) {
    using frac = inside<{{-10, 10}, 0}>;
    frac f1    = *(2_r / 3);
    frac f2    = *(1_r / 3);
    auto ok    = [](frac v) -> std::expected<frac, errc> { return v; };
    auto none  = []() -> std::expected<frac, errc> { return std::unexpected{errc::overflow}; };

    ASSERT_EQ(*(ok(f1) + f2), 1);
    ASSERT_EQ(*(f1 + ok(f2)), 1);
    ASSERT_FALSE((none() + f2).has_value());
    static_assert(std::is_same_v<decltype(ok(f1) + f2), decltype(f1 + f2)>);

    ASSERT_EQ(*(ok(f1) - f2), *(1_r / 3));
    ASSERT_EQ(*(ok(f1) * f2), *(2_r / 9));

    using pfrac = inside<{{1, 10}, 0}>;
    pfrac p1 = 3, p2 = 2;
    auto  pok = [](pfrac v) -> std::expected<pfrac, errc> { return v; };
    ASSERT_EQ(*(pok(p1) / p2), *(3_r / 2));
}

// inside action-first arithmetic overloads
TEST(InsideArithmeticTest, inside_action_first_arithmetic_overloads) {
    using c100 = inside<{0, 100}, checked>;
    c100 d{50}, z{0};

    bool fired = false;
    errc seen{};
    auto q = div(d, z, on_overflow([&](auto& res, errc c) {
                     fired = true;
                     seen  = c;
                     res   = std::remove_cvref_t<decltype(res)>{0};
                 }));
    (void)q;

    ASSERT_TRUE(fired);
    ASSERT_EQ(seen, errc::division_by_zero);

    // No-action default: errc::division_by_zero
    auto q_def = div(d, z);
    ASSERT_FALSE(q_def.has_value());
    ASSERT_EQ(q_def.error(), errc::division_by_zero);
}

// inside rational-storage on_overflow free fn
TEST(InsideArithmeticTest, inside_rational_storage_on_overflow_free_fn) {
    // Disparate large denominators force cross-multiplication overflow even
    // though the result interval [0, 2] fits.
    constexpr umax M = std::numeric_limits<umax>::max();
    using unit       = inside<{{0_r, 1_r}, 0}, checked>;

    auto a = unit::from_raw(rational{1u, static_cast<imax>(M / 2)});
    auto b = unit::from_raw(rational{1u, static_cast<imax>(M / 2 - 1)});

    bool fired = false;
    errc seen{};
    auto sum = add(a, b, on_overflow([&](auto& res, errc code) {
                       fired = true;
                       seen  = code;
                       res   = unit::from_raw(0_r); // unit is rational-storage; reset to value 0
                   }));
    (void)sum;

    ASSERT_TRUE(fired);
    ASSERT_EQ(seen, errc::overflow);
}

// inside just<N>
TEST(InsideArithmeticTest, inside_just_n) {
    static_assert(just<1> == 1);
    static_assert(just<42> == 42);
}

// unary -_ins composes through the literal parser
TEST(InsideArithmeticTest, unary_ins_composes_through_the_literal_parser) {
    // `-1.5_ins` parses as `-(1.5_ins)`. Verifies that `inside::operator-()` returns
    // a point on the negated grid for rational-storage point bounds.
    static_assert(rational{-1.5_ins} == -1.5_r);
    static_assert(rational{-5_ins} == rational{-5});
    static_assert(rational{-0x1p-8_ins} == -0x1p-8_r);

    // Compose: `5_ins + (-1.5_ins)` — point + point. The result may be wrapped
    // in expected by the grid arithmetic; either way the value is 3.5.
    static_assert((5_ins + -1.5_ins) == 3.5_r);
}

// constexpr arithmetic
TEST(InsideArithmeticTest, constexpr_arithmetic) {
    using u100 = inside<{0, 100}>;
    constexpr u100 a{30}, b{20};
    static_assert(a + b == 50);
    static_assert(a - b == 10);
    static_assert(b - a == -10);

    using u10 = inside<{1, 10}>;
    constexpr u10 m{3}, n{7};
    static_assert(m * n == 21);
    static_assert(-a == -30);

    using s50 = inside<{-50, 50}>;
    constexpr s50 sa{-20}, sb{30};
    static_assert(sa + sb == 10);
    static_assert(sa - sb == -50);
    static_assert(sa * sb == -600);
}

// scalars need a grid to join inside arithmetic
TEST(InsideArithmeticTest, scalars_need_a_grid_to_join_inside_arithmetic) {
    using bin_t = inside<{0, 9}>;
    bin_t b{5};

    // A literal given a grid (`_ins` / `just<N>`) widens like any other inside, so
    // the result stays in the bounded world — no escape to rational/double.
    static_assert(insidable<decltype(b + 1_ins)>);
    static_assert(insidable<decltype(2_ins * b)>);
    ASSERT_EQ(b + 1_ins, 6);
    ASSERT_EQ(b - 1_ins, 4);
    ASSERT_EQ(b * 2_ins, 10);
    ASSERT_TRUE(just<1> + b == 6);
    ASSERT_EQ(2_ins * b, 10);

    // Raw `int` / `double` operands are intentionally ill-formed (see the
    // static_assert guidance in arithmetic.hpp): expressions like `b + 1`,
    // `1 + b`, `b * 2.5` AND compound forms like `b += 1` do NOT compile — the
    // programmer must give the scalar a grid (`1_ins` / `just<1>`). Only comparisons
    // (`b == 5`) against a raw scalar stay ergonomic.
    ASSERT_EQ(b, 5);
    b += 1_ins;
    ASSERT_EQ(b, 6);
}

// exact scalar math stays in inside-space
TEST(InsideArithmeticTest, exact_scalar_math_stays_in_inside_space) {
    using rn = inside<{{-100, 100}, per<16>}, round_nearest>;
    rn a{0.5_r}; // 0.5

    // The old mixed-mode `inside op rational` overloads are gone: a scalar joins
    // inside arithmetic by wearing a grid (`_ins` / `just<>`), and the result stays
    // an inside — no escape into rational.
    static_assert(insidable<decltype(a + 0.5_ins)>);
    static_assert(insidable<decltype(a * 2_ins)>);

    // Exact values come back out through the integer-pair accessors, not a
    // rational: 0.5 + 0.5 == 1, 0.5 - 0.25 == 1/4, 0.5 * 2 == 1.
    auto sum = a + 0.5_ins;
    ASSERT_EQ(sum.numerator(), 1);
    ASSERT_EQ(sum.denominator(), 1);

    auto diff = a - 0.25_ins;
    ASSERT_EQ(diff.numerator(), 1);
    ASSERT_EQ(diff.denominator(), 4);

    ASSERT_EQ((a * 2_ins), 1);
    ASSERT_EQ((2_ins * a), 1);
}

// beman::inside::sum - bulk reduction with one deferred check
TEST(InsideArithmeticTest, beman_inside_sum_bulk_reduction_with_one_deferred_check) {
    // Integer raws (fast path): matches the naive += total.
    using elem = inside<{0, 200'000}, checked>;
    std::vector<elem> v(1000);
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = static_cast<int>(i % 5);
    ASSERT_TRUE(beman::inside::sum<elem>(v) == 2000); // 200·(0+1+2+3+4)

    // The TOTAL is validated, not the running prefix: a clamp target clips
    // once at the end.
    using clamped = inside<{0, 100}, clamp>;
    ASSERT_TRUE(beman::inside::sum<clamped>(v) == 100);

    // Q-format elements (index raw): exact fractional accumulation.
    using q = inside<{{0, 4}, per<256>}, round_nearest>;
    std::vector<q> qs(3, q{rational{1, 256}});
    using qsum = inside<{{0, 16}, per<256>}, round_nearest>;
    ASSERT_EQ(rational{beman::inside::sum<qsum>(qs)}, (rational{3, 256}));

    // f64 storage falls to the exact rational fold — same result.
    using r = inside<{{0, 4}, per<256>}, round_nearest | f64>;
    std::vector<r> rs(3, r{rational{1, 256}});
    ASSERT_EQ(rational{beman::inside::sum<qsum>(rs)}, (rational{3, 256}));

    // 64-bit raws whose index total passes 64 bits: summed exactly.
    using u64 = inside<{0, std::numeric_limits<umax>::max()}>;
    std::vector<u64> big(4, u64{std::numeric_limits<umax>::max()});
    using wide_total = inside<{0, std::numeric_limits<umax>::max()}, clamp>;
    ASSERT_TRUE(beman::inside::sum<wide_total>(big) == std::numeric_limits<umax>::max());

    // A continuous total past the 64-bit rational is reported through the
    // target's policy (not bad_expected_access).
    using cont = inside<{{0, 1}, 0}>;
    std::vector<cont> c{cont{rational{1, (imax{1} << 62) - 1}}, cont{rational{1, (imax{1} << 62) - 3}}};
    ASSERT_THROW((void)beman::inside::sum<cont>(c), inside_error);
}

// Issue #7 closure (user decision 2026-06-12): `inside op raw-scalar` is the
// DESIGNED ban — `1_ins` / `just<V>` / `inside<{lo,hi}>{n}` are the API. These
// pins guard the mechanism: the guidance overloads stay SFINAE-transparent
// (probing compiles; the assert fires only on real instantiation), and they
// stay the ONLY match — their `B` return type distinguishes them from any
// accidentally-introduced real widening overload, whose result grid would be
// a different type.
// scalar-operand ban: guidance overloads pinned
TEST(InsideArithmeticTest, scalar_operand_ban_guidance_overloads_pinned) {
    using pct = inside<{0, 100}>;

    // SFINAE-transparent: the probes are well-formed...
    static_assert(requires(pct b) { b + 1; });
    static_assert(requires(pct b) { b - 1; });
    static_assert(requires(pct b) { b * 2; });
    static_assert(requires(pct b) { b / 2; });
    static_assert(requires(pct b) { 1 + b; });

    // ...and resolve to the guidance overloads (return type B), not to a f64
    // widening operator (whose result grid would be a different inside type).
    static_assert(std::same_as<decltype(std::declval<pct>() + 1), pct>);
    static_assert(std::same_as<decltype(std::declval<pct>() * 2), pct>);
    static_assert(std::same_as<decltype(2.5 * std::declval<pct>()), pct>);

    // The sanctioned spellings stay open: literals widen, compound narrows,
    // comparisons are free.
    pct  b{5};
    auto w = b + 1_ins;
    static_assert(!std::same_as<decltype(w), pct>); // widened grid
    ASSERT_EQ(w, 6);
    b += 1_ins;
    ASSERT_EQ(b, 6);
    ASSERT_TRUE(b < 10);
}

// inside-space geometry helpers: dot / cross / lerp
TEST(InsideArithmeticTest, inside_space_geometry_helpers_dot_cross_lerp) {
    using coord = inside<{-10, 10}>;

    // dot(a, b) = ax*bx + ay*by ; cross(a, b) = ax*by - ay*bx (z-component).
    static_assert(dot(coord{3}, coord{4}, coord{1}, coord{2}) == 11);
    static_assert(cross(coord{3}, coord{4}, coord{1}, coord{2}) == 2);

    // Perpendicular vectors: dot is zero, cross is the signed area.
    static_assert(dot(coord{1}, coord{0}, coord{0}, coord{1}) == 0);
    static_assert(cross(coord{1}, coord{0}, coord{0}, coord{1}) == 1);

    // Each result widens past either input grid — no overflow possible.
    static_assert(!std::same_as<decltype(dot(coord{1}, coord{0}, coord{0}, coord{1})), coord>);

    // lerp(a, b, t) = a + (b - a) * t, with t a [0, 1] fixed-point inside.
    using val = inside<{0, 10}>;
    using t_t = inside<{{0, 1}, per<4>}, round_nearest>;
    static_assert(lerp(val{2}, val{8}, t_t{0}) == 2);   // t = 0 -> a
    static_assert(lerp(val{2}, val{8}, t_t{1}) == 8);   // t = 1 -> b
    static_assert(lerp(val{2}, val{8}, t_t{0.5}) == 5); // midpoint (exact)
}
