// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Arithmetic: result grids of + − × and comparison across grids, signed
// values, the tight result grids, std::expected operands and lift,
// mul_into/div_into, and bounded continuous grids.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/numbers.hpp>
#include <beman/inside/inside/detail/rational.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/grid.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/predicates.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

#include "support.hpp"

using beman::inside::test::exact;

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// inside_arithmetic
namespace inside_arithmetic_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

// inside add
TEST(InsideArithmeticTest, inside_add) {
    using u8 = inside<{10, 255}>;
    constexpr u8 a{16};
    constexpr u8 b{220};
    static_assert(a + just<1> == 17);
    static_assert(b + just<1> == 221);

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
    // double-valued assignment specialization is not constexpr-evaluable).
    using u4 = inside<{{0.75, 10.5}, 0.25}>;
    u4   d{3};
    u4   e{3.25};
    auto f = d * e;
    ASSERT_EQ(f, (39_r / 4));

    using n4 = u4::negated_type;
    n4   g{-2};
    n4   h{-2.25};
    auto i = g * h;
    ASSERT_EQ(i, (9_r / 2));

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
        static_assert(c == (51_r / 8));
    }

    {
        SCOPED_TRACE("round_toward_zero selects the integer-storage division");
        using ui = inside<{0, 100}, round_toward_zero>;
        constexpr ui   a{51}, b{8};
        constexpr auto e = a / b;
        static_assert(!(std::is_same_v<typename decltype(e)::value_type::raw_type, rational>));
        static_assert(*e == 6);
    }

    {
        SCOPED_TRACE("per-call round_toward_zero");
        using u100 = inside<{0, 100}>;
        constexpr u100 a{51}, b{8};
        constexpr auto d = div(a, b, round_toward_zero);
        static_assert(!(std::is_same_v<typename decltype(d)::value_type::raw_type, rational>));
        static_assert(*d == 6);
    }

    {
        SCOPED_TRACE("round_toward_zero truncates both signs");
        using s100 = inside<{-100, 100}>;
        using pos  = inside<{1, 100}>;
        static_assert(div(s100{-7}, pos{2}, round_toward_zero) == -3);
        static_assert(div(s100{7}, pos{2}, round_toward_zero) == 3);
        static_assert(round_toward_zero == round_toward_zero);
        using t = inside<{0, 10}, round_toward_zero>;
        static_assert(t{rational{29, 10}} == 2);
    }

    {
        SCOPED_TRACE("division by zero -> errc::division_by_zero");
        using ui = inside<{0, 100}, round_toward_zero>;
        ui a{51}, zero{0};
        ASSERT_FALSE((a / zero).has_value());
        ASSERT_EQ((a / zero).error(), errc::division_by_zero);
    }

    {
        SCOPED_TRACE("offset-encoded storage takes integer path");
        using off = inside<{200, 300}>;
        static_assert(!(value_storage<off>));
        static_assert(index_storage<off>);

        constexpr off  a{300}, b{200};
        constexpr auto q = div(a, b, round_toward_zero);
        // off's grid {200,300} excludes zero, so div returns a plain inside (no expected).
        static_assert(!(std::is_same_v<typename decltype(q)::raw_type, rational>));
        static_assert(q == 1);
    }

    {
        SCOPED_TRACE("non-unit notch trunc");
        using step2 = inside<{{0, 10}, 2}>;
        constexpr step2 a{10}, b{6};
        constexpr auto  q = div(a, b, round_toward_zero);
        static_assert(*q == 1);
    }

    {
        SCOPED_TRACE("offset + non-unit notch");
        using step5 = inside<{{5, 15}, 5}>;
        constexpr step5 a{15}, b{5};
        constexpr auto  q = div(a, b, round_toward_zero);
        // {5,15} excludes zero → plain inside result.
        static_assert(q == 3);
    }

    {
        SCOPED_TRACE("signed integer division truncates toward zero");
        using si = inside<{-100, 100}, round_toward_zero>;
        constexpr si   a{-7}, b{2};
        constexpr auto q = a / b;
        static_assert(*q == -3);
    }

    {
        SCOPED_TRACE("divisor grid excluding zero yields a non-expected result");
        using num   = inside<{0, 100}, round_toward_zero>;
        using pos   = inside<{1, 10}, round_toward_zero>;  // grid excludes zero
        using spanz = inside<{-5, 10}, round_toward_zero>; // grid straddles zero

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
        // Grids whose values need 2^40 denominators: the quotient's
        // denominators reach 2^81, past the 64-bit rational: a fraction sized
        // from the bound holds it.
        using fine = inside<{{0, 1}, rational{1, std::uint64_t{1} << 40}}>;
        using fpos = inside<{{1, 2}, rational{1, std::uint64_t{1} << 40}}>;
        static_assert(!is_expected_v<decltype(fine{0.5} / fpos{1.5})>);
        EXPECT_TRUE(lift_unwrap(fine{0.5} / fpos{1.5}) == (just<1, 3>));
    }
}

// inside modulo
TEST(InsideArithmeticTest, inside_modulo) {
    using ui = inside<{0, 100}, round_toward_zero>;
    constexpr ui a{17}, b{5};
    static_assert(*(a % b) == 2);

    constexpr ui c{100}, d{10};
    static_assert(*(c % d) == 0);

    // mod-by-zero — runtime only: the `is_constant_evaluated()` throw short-circuits.
    ui a_rt{17}, zero{0};
    ASSERT_FALSE((a_rt % zero).has_value());
    ASSERT_EQ((a_rt % zero).error(), errc::division_by_zero);

    using si = inside<{-100, 100}, round_toward_zero>;
    constexpr si sa{-17}, sb{5};
    static_assert(*(sa % sb) == -2);

    using u50 = inside<{0, 50}>;
    constexpr u50  e{23}, f{7};
    constexpr auto r = mod(e, f, round_toward_zero);
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
    using frac   = inside<{{-10, 10}, max_denominator{100}}>;
    frac f1      = (2_r / 3);
    frac f2      = (1_r / 3);
    auto ok      = [](frac v) -> std::expected<frac, errc> { return v; };
    auto failing = []() -> std::expected<frac, errc> { return std::unexpected{errc::overflow}; };

    ASSERT_EQ(*(ok(f1) + f2), 1);
    ASSERT_EQ(*(f1 + ok(f2)), 1);
    ASSERT_FALSE((failing() + f2).has_value());
    static_assert(std::is_same_v<decltype(ok(f1) + f2), std::expected<decltype(f1 + f2), errc>>);
    static_assert(std::is_same_v<decltype(ok(f1) / f2), decltype(f1 / f2)>); // already expected

    ASSERT_EQ(*(ok(f1) - f2), (1_r / 3));
    ASSERT_EQ(*(ok(f1) * f2), (2_r / 9));

    using pfrac = inside<{{1, 10}, max_denominator{100}}>;
    pfrac p1 = 3, p2 = 2;
    auto  pok = [](pfrac v) -> std::expected<pfrac, errc> { return v; };
    ASSERT_EQ(*(pok(p1) / p2), (3_r / 2));
}

// The free div reports a zero divisor in its result, and into an errc&.
TEST(InsideArithmeticTest, free_div_reports_a_zero_divisor) {
    using c100 = inside<{0, 100}, checked>;
    c100 d{50}, z{0};

    errc seen{};
    auto q = div(d, z, seen);
    ASSERT_EQ(seen, errc::division_by_zero);
    ASSERT_FALSE(q.has_value());
    ASSERT_EQ(q.error(), errc::division_by_zero);

    auto q_def = div(d, z);
    ASSERT_FALSE(q_def.has_value());
    ASSERT_EQ(q_def.error(), errc::division_by_zero);
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

    // 64-bit raws whose index total passes 64 bits: summed exactly.
    using u64 = inside<{0, std::numeric_limits<std::uint64_t>::max()}>;
    std::vector<u64> big(4, u64{std::numeric_limits<std::uint64_t>::max()});
    using wide_total = inside<{0, std::numeric_limits<std::uint64_t>::max()}, clamp>;
    ASSERT_TRUE(beman::inside::sum<wide_total>(big) == std::numeric_limits<std::uint64_t>::max());

    // A continuous total past the target is reported through the target's
    // policy (not bad_expected_access).
    using cont = inside<{{0, 1}, max_denominator{10}}>;
    std::vector<cont> c{cont{rational{3, 5}}, cont{rational{7, 10}}};
    ASSERT_THROW((void)beman::inside::sum<cont>(c), inside_error);

    // Blocks: 32-bit lanes take ⌊(2^32 − 1)/max raw⌋ elements (unsigned)
    // or ⌊(2^31 − 1)/max |raw|⌋ (signed) at a time; ranges longer than a
    // block, at every raw's maximum, add exactly.
    using u200k = inside<{0, 200'000}>;
    static_assert(detail::lane32_block<u200k> == 21474);
    std::vector<u200k> hi(50'000, u200k{200'000});
    using total_t = inside<{0, 20'000'000'000}>;
    ASSERT_TRUE(beman::inside::sum<total_t>(hi) == 10'000'000'000);
    using s100k = inside<{-100'000, 100'000}>;
    static_assert(detail::lane32_block<s100k> == 21474);
    std::vector<s100k> lo(50'001, s100k{-100'000});
    lo.back()          = 7;
    using signed_total = inside<{-10'000'000'000, 10'000'000'000}>;
    ASSERT_TRUE(beman::inside::sum<signed_total>(lo) == -4'999'999'993);
    // Q-format index raws (offsets from Lower) in lanes too.
    using qi = inside<{{-8, 8}, per<16384>}>;
    std::vector<qi> qv(40'000, qi{rational{-3, 16384}});
    using qtotal = inside<{{-200'000, 200'000}, per<16384>}>;
    ASSERT_EQ(rational{beman::inside::sum<qtotal>(qv)}, (rational{-120'000, 16384}));
    // Raws near 2^31: a 32-bit lane holds two, too few, so int64 blocks.
    using near = inside<{0, 0x7fff'fff0}>;
    static_assert(detail::lane32_block<near> == 2);
    std::vector<near> nv(5, near{0x7fff'fff0});
    using ntotal = inside<{0, 0x7'ffff'ffff}>;
    ASSERT_TRUE(beman::inside::sum<ntotal>(nv) == 5 * std::int64_t{0x7fff'fff0});
}

// Issue #7 closure (user decision 2026-06-12): `inside op raw-scalar` is the
// DESIGNED ban — `1_ins` / `just<V>` / `inside<{lo,hi}>{n}` are the API. Since
// 2026-10-31 the guidance overloads are deleted (their reason is the fix), so
// probes answer false like for every operation that cannot compile. An
// accidentally-introduced real widening overload would make a probe true.
template <class A, class B>
concept plus_ok = requires(A a, B b) { a + b; };
template <class A, class B>
concept minus_ok = requires(A a, B b) { a - b; };
template <class A, class B>
concept times_ok = requires(A a, B b) { a * b; };
template <class A, class B>
concept over_ok = requires(A a, B b) { a / b; };
// scalar-operand ban: guidance overloads pinned
TEST(InsideArithmeticTest, scalar_operand_ban_guidance_overloads_pinned) {
    using pct = inside<{0, 100}>;

    static_assert(!plus_ok<pct, int> && !minus_ok<pct, int> && !times_ok<pct, int> && !over_ok<pct, int>);
    static_assert(!plus_ok<int, pct> && !times_ok<double, pct> && !times_ok<pct, double>);

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
} // namespace inside_arithmetic_tests

// Cross-grid comparison and arithmetic against an exact rational oracle.
//
// The fuzzer covers same-grid comparison and same-notch cross-grid addition;
// this fills the gap: comparing and combining bounds whose grids differ in
// Lower, Upper AND notch. Reconciling two different (Lower, Notch) encodings is
// where off-by-one / sign / scaling bugs hide. The oracle is exact rational
// arithmetic: each inside decodes to a rational, and the inside-level result must
// equal the rational-level result exactly (+, −, × are lossless on the widened
// result grid; comparison is order-exact).
namespace cross_grid_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
// Values on a 1/4 grid so they land exactly on the coarsest notch used below
// (halves and quarters); each `A`/`B` snaps losslessly, keeping the oracle exact.
std::vector<double> sweep(double lo, double hi, double step) {
    std::vector<double> v;
    for (double x = lo; x <= hi + 1e-9; x += step)
        v.push_back(x);
    return v;
}

template <class A, class B>
void check_cross(const std::vector<double>& va, const std::vector<double>& vb) {
    for (double xa : va)
        for (double xb : vb) {
            A              a  = xa;
            B              b  = xb;
            const rational ar = static_cast<rational>(a);
            const rational br = static_cast<rational>(b);

            // --- comparison: inside-level must match rational-level order ---
            if constexpr (requires { a <=> b; }) {
                auto got = (a <=> b);
                SCOPED_TRACE(::testing::Message() << "a=" << to_string(ar) << " b=" << to_string(br));
                ASSERT_EQ((got < 0), (ar < br));
                ASSERT_EQ((got > 0), (ar > br));
                ASSERT_EQ((got == 0), (ar == br));
            }
            if constexpr (requires { a == b; }) {
                SCOPED_TRACE(::testing::Message() << "== a=" << to_string(ar) << " b=" << to_string(br));
                ASSERT_EQ((a == b), (ar == br));
                ASSERT_EQ((a != b), (ar != br));
            }
            if constexpr (requires { a < b; }) {
                SCOPED_TRACE(::testing::Message() << "< a=" << to_string(ar) << " b=" << to_string(br));
                ASSERT_EQ((a < b), (ar < br));
                ASSERT_EQ((a <= b), (ar <= br));
                ASSERT_EQ((a > b), (ar > br));
                ASSERT_EQ((a >= b), (ar >= br));
            }

            // --- arithmetic: exact on the widened result grid ---
            if constexpr (requires { a + b; }) {
                SCOPED_TRACE(::testing::Message() << "+ a=" << to_string(ar) << " b=" << to_string(br));
                ASSERT_TRUE(exact{a + b} == exact{ar} + exact{br});
            }
            if constexpr (requires { a - b; }) {
                SCOPED_TRACE(::testing::Message() << "- a=" << to_string(ar) << " b=" << to_string(br));
                ASSERT_TRUE(exact{a - b} == exact{ar} - exact{br});
            }
            if constexpr (requires { a * b; }) {
                SCOPED_TRACE(::testing::Message() << "* a=" << to_string(ar) << " b=" << to_string(br));
                ASSERT_TRUE(exact{a * b} == exact{ar} * exact{br});
            }
        }
}
} // namespace

// cross-grid compare/arith: differing notches (1/2 vs 1/4)
TEST(CrossGridTest, cross_grid_compare_arith_differing_notches_1_2_vs_1_4) {
    using A = inside<{{-8, 8}, per<2>}>;
    using B = inside<{{-8, 8}, per<4>}>;
    check_cross<A, B>(sweep(-8, 8, 0.5), sweep(-8, 8, 0.25));
}

// cross-grid compare/arith: integer vs half-notch, different offset
TEST(CrossGridTest, cross_grid_compare_arith_integer_vs_half_notch_different_offset) {
    using A = inside<{0, 100}>;         // notch 1, Lower 0
    using B = inside<{{-50, 50}, 0.5}>; // notch 1/2, Lower -50
    check_cross<A, B>(sweep(0, 100, 1.0), sweep(-50, 50, 0.5));
}

// cross-grid compare/arith: quarter-notch vs unit signed
TEST(CrossGridTest, cross_grid_compare_arith_quarter_notch_vs_unit_signed) {
    using A = inside<{{-8, 8}, per<4>}>;
    using B = inside<{-3, 7}>;
    check_cross<A, B>(sweep(-8, 8, 0.25), sweep(-3, 7, 1.0));
}

// cross-grid compare/arith: Q8.8 vs Q1.14 (power-of-two notches)
TEST(CrossGridTest, cross_grid_compare_arith_q8_8_vs_q1_14_power_of_two_notches) {
    using A = inside<{{0, 255}, per<256>}>;
    using B = inside<{{-1, 1}, per<16384>}>;
    // Values chosen on both grids' common refinement (1/256) so the oracle stays exact.
    check_cross<A, B>(sweep(0, 4, 1.0 / 256 * 37), sweep(-1, 1, 1.0 / 256 * 5));
}

// cross-grid compare/arith: asymmetric offsets, same notch
TEST(CrossGridTest, cross_grid_compare_arith_asymmetric_offsets_same_notch) {
    using A = inside<{{-7, 11}, 0.25}>;
    using B = inside<{{3, 30}, 0.25}>;
    check_cross<A, B>(sweep(-7, 11, 0.5), sweep(3, 30, 0.75));
}

// Explicit regression for the fractional-operand truncation bug: adding a
// fractional notch-offset inside to a value-storage (integer) inside used to
// drop the fractional part (-7.75 + (-3) silently became -10, not -10.75)
// because the dispatch chose the integer `to_value` path. The integer path is
// now gated on integer_lattice of BOTH operands (matching multiplication).
// regression: fractional + integer value-storage keeps the fraction
TEST(CrossGridTest, regression_fractional_plus_integer_direct_keeps_the_fraction) {
    using Frac = inside<{{-8, 8}, per<4>}>; // fractional, notch-offset storage
    using Int  = inside<{-3, 7}>;           // integer, value storage

    ASSERT_TRUE((static_cast<rational>(Frac{-7.75} + Int{-3}) == rational{43, -4})); // -10.75
    ASSERT_TRUE((static_cast<rational>(Frac{0.25} + Int{2}) == rational{9u, 4}));    //   2.25
    ASSERT_TRUE((static_cast<rational>(Frac{5.25} + Int{7}) == rational{49u, 4}));   //  12.25
    // subtraction routes through add(-rhs); same path.
    ASSERT_TRUE((static_cast<rational>(Frac{5.25} - Int{3}) == rational{9u, 4})); //   2.25
    ASSERT_TRUE((static_cast<rational>(Int{4} - Frac{1.5}) == rational{5u, 2}));  //   2.5
}

//---------------------------------------------------------------------------
// Integer-raw sums of mixed grids stay on integer storage (the wrapping
// value-index path).
//---------------------------------------------------------------------------
TEST(CrossGridTest, integer_paths_stay_engaged_and_fp_stays_excluded) {
    using whole    = inside<{0, 100}>;
    using quarters = inside<{{0, 1}, per<4>}>;
    static_assert(!detail::rational_storage<decltype(whole{} + quarters{})>);
    static_assert(whole{3} + quarters{0.75_r} == 3.75_r);

    using tenths       = inside<{{0, 100}, per<10>}>;
    using quarter_grid = inside<{{0, 100}, per<4>}, round_nearest>;
    static_assert(detail::assignment<quarter_grid, tenths>::affine_map.Ok);

    // fp-backed operands: the sum is exact either way.
    using coarse_real = inside<{{0, (std::uint64_t{1} << 40)}, per<2>}>;
    using fine_real   = inside<{{0, 1}, per<(1u << 20)>}>;
    EXPECT_TRUE(coarse_real{1.5_r} + fine_real{0.25_r} == 1.75_r);

    // value check across a negative Lower, at compile time (constexpr path).
    using signed_whole = inside<{-50, 50}>;
    using eighths      = inside<{{-2, 2}, per<8>}>;
    static_assert(rational{signed_whole{-7} + eighths{-0.625_ins}} == rational{std::uint64_t{61}, std::int64_t{-8}});
}

// regression: cross-grid assign onto rational storage keeps the value
TEST(CrossGridTest, cross_grid_assign_keeps_the_value) {
    // An inside source stores its exact value; rounding only where the source
    // is off the target grid.
    using thirds = inside<{{0, 4}, per<3>}, round_nearest>;

    using index_src = inside<{{0, 4}, per<3>}, round_nearest>;
    thirds from_index;
    from_index = index_src{rational{7, 3}};
    ASSERT_EQ(rational{from_index}, (rational{7, 3}));

    using wide_thirds = inside<{{-4, 4}, per<3>}, round_nearest>;
    thirds from_exact;
    from_exact = wide_thirds{rational{5, 3}};
    ASSERT_EQ(rational{from_exact}, (rational{5, 3}));

    using value_src = inside<{0, 4}, round_toward_zero>;
    thirds from_value_raw;
    from_value_raw = value_src{3};
    ASSERT_EQ(rational{from_value_raw}, rational{3});

    using dyadic_src = inside<{{0, 4}, per<256>}, round_nearest>;
    using dyadic     = inside<{{0, 4}, per<256>}, round_nearest>;
    dyadic from_dyadic;
    from_dyadic = dyadic_src{rational{513, 256}};
    ASSERT_EQ(rational{from_dyadic}, (rational{513, 256}));

    // rounding still happens when the source is off the target grid
    using units = inside<{{0, 4}, 1}, round_nearest>;
    units rounded;
    rounded = index_src{rational{7, 3}}; // 2.33 -> 2 on the unit grid
    ASSERT_EQ(rational{rounded}, rational{2});
}

// scalar comparison integer arm agrees with the rational decode
TEST(CrossGridTest, scalar_comparison_integer_arm_agrees_with_the_rational_decode) {
    // Q-format (index raw) vs integral scalar takes the cross-multiplied
    // integer arm; every verdict must match the exact rational comparison,
    // including at A's numeric_limits extremes.
    using q88 = inside<{{0, 255}, per<256>}, round_nearest>;
    static_assert(scalar_index_cmp_fits<q88, int>);

    auto agree = [](auto probe, auto scalar) {
        rational exact_lhs = as_rational(probe);
        rational exact_rhs{scalar};
        ASSERT_EQ((probe == scalar), (exact_lhs == exact_rhs));
        ASSERT_EQ((probe < scalar), (exact_lhs < exact_rhs));
        ASSERT_EQ((probe > scalar), (exact_lhs > exact_rhs));
    };
    for (int scalar : {std::numeric_limits<int>::min(), -1, 0, 41, 42, 43, 255, std::numeric_limits<int>::max()}) {
        agree(q88{42}, scalar);
        agree(q88{rational{10753, 256}}, scalar); // 42 + 1/256: != 42, > 42
        agree(q88{rational{10751, 256}}, scalar); // 42 - 1/256: != 42, < 42
        agree(q88{0}, scalar);
        agree(q88{255}, scalar);
    }

    // offset index grid (negative Lower -> nonzero bias)
    using offset_q = inside<{{-8, 8}, per<16384>}, round_nearest>;
    static_assert(scalar_index_cmp_fits<offset_q, int>);
    for (int scalar : {std::numeric_limits<int>::min(), -9, -8, -1, 0, 1, 8, std::numeric_limits<int>::max()}) {
        agree(offset_q{-8}, scalar);
        agree(offset_q{rational{-1, 16384}}, scalar);
        agree(offset_q{0}, scalar);
        agree(offset_q{8}, scalar);
    }

    // 64-bit scalars whose cross term c·d can overflow int64 are excluded and
    // fall back to the exact rational path — still correct.
    static_assert(!scalar_index_cmp_fits<q88, long long>);
    ASSERT_TRUE(q88{42} < std::numeric_limits<long long>::max());
    ASSERT_TRUE(q88{42} > std::numeric_limits<long long>::min());
    ASSERT_TRUE(!(q88{42} == std::numeric_limits<long long>::max()));
}

// A continuous operand makes a sum continuous (gcd(0, 1) = 1 is a point's
// rule, not a continuous grid's): 3/10 + 5 is 53/10, not a notch-1 value.
TEST(CrossGridTest, continuous_plus_notched_is_continuous) {
    using C = inside<{{-8, 8}, max_denominator{10}}>;
    using I = inside<{0, 10}>;
    static_assert(notch_of<decltype(C{} + I{})> == 0);
    EXPECT_EQ((rational{C{rational{3, 10}} + I{5}}), (rational{53, 10}));
    EXPECT_EQ((rational{I{5} - C{rational{3, 10}}}), (rational{47, 10}));
    // a point still shifts the lattice
    static_assert(notch_of<decltype(I{} + just<rational{1, 2}>)> == 1);
}
} // namespace cross_grid_tests

// signed
namespace signed_tests {
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

// Regression: signed value-storage storage previously recorded the *offset*
// (rhs - Lower) instead of the value when constructed from fractional
// rhs (rational/double) or from another inside via the inside-to-inside
// store path. Both paths now route through `raw_from_offset<L>`, which
// adds lower_of<L> back for value-storage targets.
// signed value-storage ctor from rational/double preserves value
TEST(SignedTest, signed_direct_ctor_from_rational_double_preserves_value) {
    using temp = inside<{-40, 60}>;

    ASSERT_EQ(temp{-40_r}, -40);
    ASSERT_EQ(temp{rational{42}}, 42);
    ASSERT_EQ(temp{rational{0}}, 0);
    ASSERT_EQ(temp{-40.0}, -40);
    ASSERT_EQ(temp{42.0}, 42);

    // Raw must match value for value storage.
    ASSERT_EQ(temp{-40_r}.raw(), -40);
    ASSERT_EQ(temp{rational{42}}.raw(), 42);
}

// inside-to-inside assignment preserves value across value storages
TEST(SignedTest, inside_to_inside_assignment_preserves_value_across_direct_storages) {
    // unsigned value-storage → signed value-storage
    using upos = inside<{0, 30}>;
    using s40  = inside<{-40, 50}>;
    upos u{25};
    s40  s{u};
    ASSERT_EQ(s, 25);
    ASSERT_EQ(s.raw(), 25);

    // signed value-storage → signed value-storage (different grids)
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
    using s32    = inside<{-100000, 100000}>;
    auto a       = []() -> std::expected<s32, errc> { return s32{42}; };
    auto failing = []() -> std::expected<s32, errc> { return std::unexpected{errc::overflow}; };

    auto sum = a() + s32{-100};
    ASSERT_TRUE(sum.has_value());
    ASSERT_EQ(*sum, -58);
    ASSERT_FALSE((failing() + s32{-100}).has_value());
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
} // namespace signed_tests

// Result grids that hold exactly the values an operation can produce: the
// remainder by every rounding mode, min/max, |x|, even powers and lerp.
// Result types freeze at 1.0, so these pin the tight forms.
namespace result_grids_tests {
using namespace beman::inside;

namespace {
template <class T>
struct unwrap {
    using type = T;
};
template <class T, class E>
struct unwrap<std::expected<T, E>> {
    using type = T;
};

template <class T>
constexpr std::int64_t lo() {
    return detail::trunc(detail::to_rational(lower_of<typename unwrap<T>::type>));
}
template <class T>
constexpr std::int64_t hi() {
    return detail::trunc(detail::to_rational(upper_of<typename unwrap<T>::type>));
}

// r = a − round(a/b)·b on plain integers.
enum class mode { trunc, floor, ceil, nearest };
constexpr std::int64_t reference_remainder(std::int64_t a, std::int64_t b, mode m) {
    std::int64_t       q   = a / b;
    const std::int64_t r   = a - q * b;
    const bool         neg = (a < 0) != (b < 0);
    if (r != 0) {
        if (m == mode::floor && neg)
            --q;
        else if (m == mode::ceil && !neg)
            ++q;
        else if (m == mode::nearest && 2 * (r < 0 ? -r : r) >= (b < 0 ? -b : b))
            q += neg ? -1 : 1;
    }
    return a - q * b;
}

// Every remainder of every pair lies in the result type's range, and the
// range's ends are reached (no slack) except at nearest-mode ties.
template <mode M, auto Flag, grid GA, grid GB>
void expect_tight_remainder(bool ends_reached) {
    using R                    = decltype(mod(inside<GA>{}, inside<GB>{}, Flag));
    std::int64_t       seen_lo = hi<R>() + 1;
    std::int64_t       seen_hi = lo<R>() - 1;
    const std::int64_t a_lo = lo<inside<GA>>(), a_hi = hi<inside<GA>>();
    const std::int64_t b_lo = lo<inside<GB>>(), b_hi = hi<inside<GB>>();
    for (std::int64_t a = a_lo; a <= a_hi; ++a)
        for (std::int64_t b = b_lo; b <= b_hi; ++b) {
            if (b == 0)
                continue;
            const std::int64_t r = reference_remainder(a, b, M);
            ASSERT_LE(lo<R>(), r) << "a " << a << " b " << b;
            ASSERT_LE(r, hi<R>()) << "a " << a << " b " << b;
            seen_lo = std::min(seen_lo, r);
            seen_hi = std::max(seen_hi, r);
        }
    if (ends_reached) {
        EXPECT_EQ(seen_lo, lo<R>());
        EXPECT_EQ(seen_hi, hi<R>());
    }
}

template <grid GA, grid GB>
void expect_all_modes() {
    expect_tight_remainder<mode::trunc, round_toward_zero, GA, GB>(true);
    expect_tight_remainder<mode::floor, round_floor, GA, GB>(true);
    expect_tight_remainder<mode::ceil, round_ceil, GA, GB>(true);
    expect_tight_remainder<mode::nearest, round_nearest, GA, GB>(false);
}
} // namespace

TEST(ResultGridsTest, remainder_holds_exactly_what_each_mode_produces) {
    expect_all_modes<grid{-12, 12}, grid{-7, 5}>();
    expect_all_modes<grid{0, 30}, grid{1, 9}>();
    expect_all_modes<grid{-30, 0}, grid{1, 9}>();
    expect_all_modes<grid{-30, -2}, grid{-9, -1}>();
    expect_all_modes<grid{0, 3}, grid{1, 100}>();
    expect_all_modes<grid{-3, 40}, grid{2, 8}>();
    expect_all_modes<grid{5, 9}, grid{-4, -2}>();
    expect_all_modes<grid{-50, 50}, grid{-3, 3}>();
}

TEST(ResultGridsTest, remainder_is_bounded_by_the_dividend) {
    using small = inside<{0, 3}>;
    using big   = inside<{1, 100}>;
    static_assert(std::is_same_v<decltype(small{} % big{}), inside<{0, 3}>>);
    static_assert(std::is_same_v<decltype(inside<{-100, 0}>{} % big{}), inside<{-99, 0}>>);
    static_assert(std::is_same_v<decltype(mod(inside<{-100, 100}>{}, big{}, round_floor)), inside<{0, 99}>>);
    static_assert(std::is_same_v<decltype(divmod(small{}, big{}).Remainder), inside<{0, 3}>>);
}

TEST(ResultGridsTest, min_and_max_hold_the_values_they_can_take) {
    using a_t = inside<{0, 100}>;
    using c_t = inside<{0, 10}>;
    static_assert(std::is_same_v<decltype(min(a_t{}, c_t{})), inside<{0, 10}>>);
    static_assert(std::is_same_v<decltype(max(a_t{}, c_t{})), inside<{0, 100}>>);
    static_assert(std::is_same_v<decltype(min(inside<{-100, 0}>{}, inside<{5, 10}>{})), inside<{-100, 0}>>);
    static_assert(std::is_same_v<decltype(max(inside<{-100, 0}>{}, inside<{5, 10}>{})), inside<{5, 10}>>);
    static_assert(std::is_same_v<decltype(min(a_t{}, a_t{})), a_t>);

    EXPECT_TRUE(min(a_t{50}, c_t{3}) == 3_ins);
    EXPECT_TRUE(max(a_t{50}, c_t{3}) == 50_ins);
    EXPECT_TRUE(min(a_t{2}, c_t{3}) == 2_ins);
    EXPECT_TRUE(min(inside<{{0.5, 10.5}, 1}>{2.5}, c_t{3}) == 2.5_r);
    EXPECT_TRUE(max(inside<{{0.5, 10.5}, 1}>{2.5}, c_t{3}) == 3_ins);
}

TEST(ResultGridsTest, abs_and_even_powers_never_reach_below_their_least_magnitude) {
    static_assert(std::is_same_v<decltype(math::abs(inside<{5, 10}>{5})), inside<{5, 10}>>);
    static_assert(std::is_same_v<decltype(math::abs(inside<{-10, -5}>{-5})), inside<{5, 10}>>);
    static_assert(std::is_same_v<decltype(math::abs(inside<{-10, 10}>{})), inside<{0, 10}>>);
    static_assert(std::is_same_v<decltype(math::pown<2>(inside<{-10, 10}>{})), inside<{0, 100}>>);
    static_assert(std::is_same_v<decltype(math::pown<2>(inside<{-10, -2}>{-2})), inside<{4, 100}>>);
    static_assert(std::is_same_v<decltype(math::pown<3>(inside<{-10, 10}>{})), inside<{-1000, 1000}>>);

    EXPECT_TRUE(math::abs(inside<{-10, -5}>{-7}) == 7_ins);
    EXPECT_TRUE(math::pown<2>(inside<{-10, 10}>{-7}) == 49_ins);
    EXPECT_TRUE(math::pown<4>(inside<{-3, 2}>{-3}) == 81_ins);
}

TEST(ResultGridsTest, lerp_stays_between_its_ends) {
    using v_t = inside<{0, 100}>;
    using t_t = inside<{{0, 1}, per<4>}>;
    static_assert(std::is_same_v<decltype(lerp(v_t{}, v_t{}, t_t{})), inside<{{0, 100}, per<4>}>>);
    static_assert(std::is_same_v<decltype(lerp(inside<{0, 10}>{}, v_t{}, t_t{})), inside<{{0, 100}, per<4>}>>);
    // t past [0, 1] extrapolates: the wide grid stays.
    static_assert(lower_of<decltype(lerp(v_t{}, v_t{}, inside<{{0, 2}, per<4>}>{}))> < 0);

    EXPECT_TRUE(lerp(v_t{10}, v_t{30}, t_t{0.25}) == 15_ins);
    EXPECT_TRUE(lerp(v_t{30}, v_t{10}, t_t{1}) == 10_ins);
}

TEST(ResultGridsTest, quantities_take_the_numbers_tight_types) {
    using namespace beman::inside::units;
    const quantity<si::metre, inside<{0, 100}>>         a{10};
    const quantity<si::kilo(si::metre), inside<{0, 1}>> b{1};
    const inside<{{0, 1}, per<4>}>                      t{0.25};
    // In the common unit, metres: lerp between [0, 100] m and [0, 1000] m.
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(lerp(a, b, t).numerical_value())>, inside<{{0, 1000}, per<4>}>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(min(a, b).numerical_value())>, inside<{0, 100}>>);
    EXPECT_TRUE((lerp(a, b, t) == just<2575, 10> * si::metre));
    EXPECT_TRUE(min(a, b) == just<10> * si::metre);
}
} // namespace result_grids_tests

// expected_arithmetic
namespace expected_arithmetic_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using exp_r = std::expected<rational, errc>;

// Fallible-result stand-ins: std::expected travels only as a return value or
// parameter, so the tests build operands with these instead of storing them.
constexpr exp_r good(rational r) { return r; }
constexpr exp_r bad(errc e = errc::overflow) { return std::unexpected{e}; }
} // namespace

// No inside stores a std::expected - not by construction, =, or a compound
// op: check it and store *e, or use try_make(e), which passes the error on.
TEST(ExpectedArithmeticTest, inside_does_not_store_an_expected) {
    using b_t = inside<{{0, 1}, per<16>}, round_nearest>;
    static_assert(!std::is_constructible_v<b_t, exp_r> && !std::is_convertible_v<exp_r, b_t>);
    static_assert(!std::is_assignable_v<b_t&, exp_r>);
    static_assert(!std::is_constructible_v<b_t, std::expected<b_t, errc>>);

    const auto g = good(0.5_r);
    ASSERT_TRUE(g.has_value());
    b_t v{*g};
    ASSERT_EQ(rational{v}, 0.5_r);
    ASSERT_EQ(*b_t::try_make(good(0.25_r)), 0.25_r);
    ASSERT_EQ(b_t::try_make(bad(errc::division_by_zero)).error(), errc::division_by_zero);
}

// A division whose divisor grid holds 0, into a clamp type: try_make passes
// the zero divisor on (clamp handles the range, not the error); a nonzero
// divisor stores, rounded and clamped by the type.
TEST(ExpectedArithmeticTest, zero_divisor_into_a_clamp_type_is_reported) {
    inside<{0, 10}> a{4};
    inside<{0, 2}>  b{0};
    using c_t = inside<{0, 100}, clamp | round_nearest>;
    EXPECT_EQ(c_t::try_make(a / b).error(), errc::division_by_zero);
    b = 2;
    EXPECT_EQ(rational{*c_t::try_make(a / b)}, 2);
}

// expected<inside> + a point propagates the error. (An inside or an
// expected<inside> with a rational is deleted: it would leave the grid.)
template <class A, class B>
concept adds_to = requires(A a, B b) { a + b; };
TEST(ExpectedArithmeticTest, expected_inside_plus_point_propagates_error) {
    using b_t    = inside<{{0, 1}, per<16>}, round_nearest>;
    auto some    = []() -> std::expected<b_t, errc> { return b_t{0.5_r}; };
    auto failing = []() -> std::expected<b_t, errc> { return std::unexpected{errc::division_by_zero}; };
    static_assert(!adds_to<std::expected<b_t, errc>, rational> && !adds_to<b_t, rational> && !adds_to<rational, b_t>);

    auto r = some() + 0.25_ins;
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 0.75_r);

    ASSERT_FALSE((failing() + 0.25_ins).has_value());
    ASSERT_FALSE((0.25_ins + failing()).has_value());
    ASSERT_FALSE((failing() - 0.25_ins).has_value());
    ASSERT_FALSE((failing() * 0.25_ins).has_value());
    ASSERT_FALSE((failing() / 0.25_ins).has_value());
    ASSERT_EQ((failing() * 0.25_ins).error(), errc::division_by_zero);
}

// expected<rational> value_or
TEST(ExpectedArithmeticTest, expected_rational_value_or) {
    ASSERT_EQ(bad().value_or(7_r), 7_r);
    ASSERT_EQ(good(0.5_r).value_or(7_r), 0.5_r);
}
} // namespace expected_arithmetic_tests

// lift
namespace lift_tests {
using namespace beman::inside;

using beman::inside::detail::lift;

namespace {
// Fallible-argument stand-ins: std::expected is passed as an argument, never
// stored, so the tests produce it from these functions.
constexpr std::expected<int, errc> good(int v) { return v; }
constexpr std::expected<int, errc> bad(errc e = errc::overflow) { return std::unexpected{e}; }
} // namespace

// lift basics: pure values
TEST(LiftTest, lift_basics_pure_values) {
    auto plus = [](int a, int b) { return a + b; };
    static_assert(*lift(plus, 2, 3) == 5);
    static_assert(std::is_same_v<decltype(lift(plus, 2, 3)), std::expected<int, errc>>);
}

// lift with expected arg(s) / (good, value)
TEST(LiftTest, lift_with_expected_arg_good_value) {
    auto plus = [](int a, int b) { return a + b; };
    auto r    = lift(plus, good(2), 3);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 5);
}

// lift with expected arg(s) / (error, value) -> error
TEST(LiftTest, lift_with_expected_arg_error_value_to_error) {
    auto plus = [](int a, int b) { return a + b; };
    auto r    = lift(plus, bad(errc::domain_error), 3);
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::domain_error);
}

// lift with expected arg(s) / (value, error) -> error
TEST(LiftTest, lift_with_expected_arg_value_error_to_error) {
    auto plus = [](int a, int b) { return a + b; };
    auto r    = lift(plus, 2, bad(errc::division_by_zero));
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::division_by_zero);
}

// lift with expected arg(s) / (good, good)
TEST(LiftTest, lift_with_expected_arg_good_good) {
    auto plus = [](int a, int b) { return a + b; };
    auto r    = lift(plus, good(2), good(3));
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 5);
}

// lift: the leftmost error wins
TEST(LiftTest, lift_leftmost_error_wins) {
    auto plus = [](int a, int b) { return a + b; };
    auto r    = lift(plus, bad(errc::domain_error), bad(errc::overflow));
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error(), errc::domain_error);
}

// lift auto-flatten when op returns expected
TEST(LiftTest, lift_auto_flatten_when_op_returns_expected) {
    auto checked_div = [](int a, int b) -> std::expected<int, errc> {
        if (b == 0)
            return std::unexpected{errc::division_by_zero};
        return a / b;
    };

    auto r = lift(checked_div, 6, 2);
    static_assert(std::is_same_v<decltype(r), std::expected<int, errc>>);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 3);

    auto z = lift(checked_div, 6, 0);
    ASSERT_FALSE(z.has_value());
    ASSERT_EQ(z.error(), errc::division_by_zero);
}

// lift with three args
TEST(LiftTest, lift_with_three_args) {
    auto sum3 = [](int a, int b, int c) { return a + b + c; };

    // all values
    ASSERT_EQ((*lift(sum3, 1, 2, 3)), 6);

    // one error -> error
    ASSERT_FALSE((lift(sum3, 1, bad(), 3).has_value()));

    // all good
    auto r = lift(sum3, good(1), good(2), good(3));
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 6);
}

// lift propagates exceptions thrown by op
TEST(LiftTest, lift_propagates_exceptions_thrown_by_op) {
    auto thrower = [](int) -> int { throw 42; };
    ASSERT_THROW((void)(lift(thrower, 1)), int);
}
} // namespace lift_tests

// mul_into<Out> / div_into<Out>: the exact product or quotient rounded once
// onto Out, without forming the product or quotient grid.
namespace into_tests {
using namespace beman::inside;

namespace {
inline constexpr rational wei{1, 1'000'000'000'000'000'000};
using eth   = inside<{{0, 1'000'000'000}, wei}>;              // a wide raw
using price = inside<{{0, 100'000'000}, per<100>}>;           // dollars per ETH, to the cent
using usd   = inside<{{0, 1'000'000'000'000'000}, per<100>}>; // dollars to the cent
} // namespace

TEST(IntoTest, mul_into_rounds_the_exact_product_once) {
    // eth × price has a 10^-20 notch: rounded once onto usd's grid.
    const eth   balance = *parse<eth>("123456789.123456789123456789");
    const price p       = *parse<price>("3141.59");
    using usd_nearest   = inside<grid_of<usd>, round_nearest>;
    EXPECT_EQ(to_string(mul_into<usd_nearest>(balance, p)), "387850614142.36");
    EXPECT_EQ(to_string(mul_into<usd, round_floor>(balance, p)), "387850614142.36");
    EXPECT_EQ(to_string(mul_into<usd, round_ceil>(balance, p)), "387850614142.37");
    // Off the cents grid under a checked Out: reported, not rounded.
    EXPECT_THROW((void)mul_into<usd>(balance, p), inside_error);
    // On the grid: exact.
    EXPECT_EQ(mul_into<usd>(eth{2}, p), (usd{rational{628318, 100}}));
}

TEST(IntoTest, mul_into_range_follows_out_policy) {
    using small   = inside<{0, 100}>;
    using clamped = inside<{0, 100}, clamp>;
    const small a{20}, b{30};
    EXPECT_THROW((void)mul_into<small>(a, b), inside_error);
    EXPECT_EQ(mul_into<clamped>(a, b), 100);
    EXPECT_EQ(mul_into<small>(small{5}, a), 100);
}

TEST(IntoTest, div_into) {
    using third = inside<{{0, 100}, per<3>}, round_nearest>;
    using pos   = inside<{1, 100}>;
    using any   = inside<{0, 100}>;
    // A zero-free divisor: a plain Out.
    const auto q = div_into<third>(any{7}, pos{3});
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(q)>, third>);
    EXPECT_EQ(q, (third{rational{7, 3}}));
    // Rounded once onto a coarser Out.
    using tenth = inside<{{0, 100}, per<10>}, round_nearest>;
    EXPECT_EQ(div_into<tenth>(any{7}, pos{3}), (tenth{rational{23, 10}}));
    // A divisor that may be 0: every failure is the expected's error.
    const auto r = div_into<third>(any{7}, any{3});
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(r)>, std::expected<third, errc>>);
    EXPECT_EQ(*r, (third{rational{7, 3}}));
    EXPECT_EQ(div_into<third>(any{7}, any{0}).error(), errc::division_by_zero);
    using one = inside<{0, 1}, round_nearest>;
    EXPECT_EQ(div_into<one>(any{7}, any{2}).error(), errc::overflow);
    using ten = inside<{0, 10}>;
    EXPECT_EQ(div_into<ten>(any{7}, any{2}).error(), errc::rounding_error);
    // A quotient with no 64-bit fraction, straight onto a grid.
    using fine  = inside<{{0, 2}, per<(std::uint64_t{1} << 62)>}, round_nearest>;
    const eth a = *parse<eth>("1.000000000000000001"), b = *parse<eth>("0.999999999999999999");
    EXPECT_EQ(div_into<fine>(a, b).value(), fine::from_raw((std::uint64_t{1} << 62) + 9));
}

// The native paths (the exact product inside, or a 64-bit rational) round as
// the wide path does: every value pair of small grids, every Out policy, the
// same result or the same error.
namespace {
template <class Out, policy_flag F, class Fn>
std::expected<Out, errc> outcome(Fn&& f) {
    try {
        return f();
    } catch (const inside_error& e) {
        return std::unexpected{e.Code};
    }
}
template <class Out, policy_flag F, class A, class B>
void expect_mul_div_agree() {
    for (const A a : values<A>)
        for (const B b : values<B>) {
            const auto policy = make_policy<flags_of<Out> | F>();
            const auto m      = outcome<Out, F>([&] { return mul_into<Out, F>(a, b); });
            const auto mw     = outcome<Out, F>([&] { return detail::mul_into_wide<Out>(a, b, policy); });
            EXPECT_EQ(m, mw) << to_string(a) << " * " << to_string(b);
            if (b == 0)
                continue;
            const auto d  = outcome<Out, F>([&] {
                const auto r = div_into<Out, F>(a, b);
                if constexpr (detail::expected_like<std::remove_const_t<decltype(r)>>) {
                    if (!r)
                        throw inside_error(r.error());
                    return *r;
                } else
                    return r;
            });
            const auto dw = outcome<Out, F>([&] { return detail::div_into_wide<Out>(a, b, policy); });
            EXPECT_EQ(d, dw) << to_string(a) << " / " << to_string(b);
        }
}
using int_g  = inside<{-7, 9}>;
using quart  = inside<{{-3, 3}, per<4>}>;
using thirds = inside<{{rational{1, 3}, rational{13, 3}}, rational{1, 2}}>; // unanchored
using neg    = inside<{{-2, -1}, per<8>}>;
} // namespace

TEST(IntoTest, native_paths_agree_with_the_wide_path) {
    using round_out = inside<{{-50, 50}, per<3>}, round_nearest>;
    using floor_out = inside<{-40, 40}, round_floor>;
    using clamp_out = inside<{{-5, 5}, per<2>}, clamp | round_ceil>;
    using check_out = inside<{{-20, 20}, per<2>}>;
    using wrap_out  = inside<{{-3, 3}, per<4>}, wrap | round_half_even>;
    // Both native paths are taken: the product inside, and the rational
    // product where the store would reject the product type at compile time.
    static_assert(detail::native_product<round_out, checked, int_g, quart>);
    static_assert(detail::native_product<clamp_out, checked, int_g, int_g>);
    static_assert(!detail::native_product<check_out, checked, int_g, quart>);
    static_assert(detail::rational_operands<int_g, quart>);
    expect_mul_div_agree<round_out, checked, int_g, quart>();
    expect_mul_div_agree<round_out, checked, quart, thirds>();
    expect_mul_div_agree<floor_out, checked, thirds, int_g>();
    expect_mul_div_agree<floor_out, checked, neg, quart>();
    expect_mul_div_agree<clamp_out, checked, int_g, int_g>();
    expect_mul_div_agree<clamp_out, checked, quart, thirds>();
    expect_mul_div_agree<check_out, checked, int_g, quart>();
    expect_mul_div_agree<check_out, checked, thirds, thirds>();
    expect_mul_div_agree<check_out, round_nearest, quart, int_g>();
    expect_mul_div_agree<int_g, round_floor, quart, quart>();
    // div_into's unreduced quotient (Q-format targets): wrap, and a floor
    // target fed by unanchored and negative operands.
    static_assert(detail::unreduced_store<wrap_out> && detail::unreduced_store<floor_out>);
    expect_mul_div_agree<wrap_out, checked, int_g, quart>();
    expect_mul_div_agree<wrap_out, checked, thirds, neg>();
    expect_mul_div_agree<floor_out, round_ceil, neg, thirds>();
}
} // namespace into_tests

// Bounded continuous grids: a continuous result whose denominators the grids
// bound (grid::MaxDenominator) holds every exact +, −, ×, ÷ result, so those
// ops return plain values instead of std::expected.
namespace bounded_rational_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
using num   = inside<{0, 100}>;
using den   = inside<{1, 7}>;
using quot  = decltype(num{} / den{});
using half  = inside<{{-6, 6}, per<2>}>;
using small = inside<{1, 5}>;
} // namespace

TEST(BoundedRationalTest, quotient_of_notched_grids_is_bounded_and_plain) {
    static_assert(grid_of<quot>.MaxDenominator == 7);
    static_assert(rational_storage<quot>);
    static_assert(!is_expected_v<quot>);
    static_assert(!is_expected_v<decltype(quot{} * 9_ins / 5_ins + 32_ins)>);
    static_assert(!is_expected_v<decltype(quot{} + quot{} - quot{} * quot{})>);
    static_assert(!is_expected_v<decltype(quot{} / den{})>);

    const auto q = num{50} / den{3};
    EXPECT_EQ(rational{q}, (rational{50, 3}));
    EXPECT_EQ(rational{q * 9_ins / 5_ins + 32_ins}, 62);
    EXPECT_EQ(rational{q + q - q * q}, (rational{-2200, 9}));
    EXPECT_EQ(rational{q / den{7}}, (rational{50, 21}));
}

TEST(BoundedRationalTest, declared_bound_makes_arithmetic_plain) {
    using thousandths = inside<{interval{0, 1}, max_denominator{1000}}>;
    static_assert(!is_expected_v<decltype(thousandths{} + thousandths{})>);
    static_assert(!is_expected_v<decltype(thousandths{} * thousandths{})>);
    const thousandths a{rational{1, 1000}}, b{rational{999, 1000}};
    EXPECT_EQ(rational{a * b}, (rational{999, 1'000'000}));
    EXPECT_EQ(rational{a + b}, 1);
}

TEST(BoundedRationalTest, bounds_past_storage_report_in_cxx23_and_fit_in_cxx26) {
    using billionths = inside<{interval{0, 1}, max_denominator{1'000'000'000}}>;
    using b2         = decltype(billionths{} * billionths{});
    static_assert(!is_expected_v<b2>); // 10^18 fits the 64-bit rational
    using b3 = decltype(b2{} * billionths{});
    static_assert(!is_expected_v<b3>); // a fraction sized for 10^27
    static_assert(fraction_storage<b3>);
    // (A value whose cube still fits 64 bits, so both modes have one.)
    const billionths x{rational{1, 1000}};
    const auto       x3 = lift_unwrap(x * x * x);
    EXPECT_TRUE(x3 == rational(1, 1'000'000'000));
    EXPECT_TRUE(x3 == lift_unwrap(x * (x * x)));
}

// Every value of small grids against exact arithmetic: the unchecked 64-bit
// kernels the bound selects give the exact result.
TEST(BoundedRationalTest, sweep_matches_checked_rational) {
    using q_t = decltype(half{} / small{});
    static_assert(grid_of<q_t>.MaxDenominator == 10 && !is_expected_v<q_t>);
    for (auto x : values<half>)
        for (auto y : values<small>) {
            const q_t   q  = x / y;
            const exact qr = exact{x} / exact{y};
            ASSERT_EQ(exact{q}, qr);
            ASSERT_EQ(exact{q + q}, qr + qr);
            ASSERT_EQ(exact{q - x}, qr - exact{x});
            ASSERT_EQ(exact{q * q}, qr * qr);
            ASSERT_EQ(exact{q / y}, qr / exact{y});
            ASSERT_EQ(exact{(q + q) * q - q / y}, (qr + qr) * qr - qr / exact{y});
        }
}

// A bounded grid holds only values within its bound: a denominator past it
// is off the grid, like a value off a notched grid's lattice.
TEST(BoundedRationalTest, store_past_the_bound_reports_rounding_error) {
    using thousandths = inside<{interval{0, 1}, max_denominator{1000}}>;
    EXPECT_TRUE(thousandths::try_make(rational{7, 1000}).has_value());
    EXPECT_EQ(thousandths::try_make(rational{1, 1001}).error(), errc::rounding_error);
    EXPECT_EQ(parse<thousandths>("0.0001").error(), errc::rounding_error);
    EXPECT_THROW((thousandths{rational{1, 1001}}), inside_error);
    static_assert(!thousandths::try_make(rational{1, 1001}).has_value()); // also at compile time

    // Compound assignment stores back through the same check.
    thousandths x{rational{1, 10}};
    x *= thousandths{rational{1, 10}}; // 1/100: within the bound
    EXPECT_TRUE(x == rational(1, 100));
    EXPECT_THROW((x *= thousandths{rational{1, 100}}), inside_error); // 1/10000

    // A quotient type's bound is what the grids allow, so its own results fit.
    auto q = num{1} / den{7};
    q      = num{3} / den{7};
    EXPECT_TRUE(q == rational(3, 7));

    // unsafe: nothing reported; its rounding truncates onto the bound, as it
    // truncates onto a notch.
    inside<{interval{0, 1}, max_denominator{1000}}, unsafe> u{rational{1, 1001}};
    EXPECT_TRUE(u == 0);
}

// The same check on a bounded grid stored as a wide fraction.
TEST(BoundedRationalTest, wide_bounded_store_checks_the_bound) {
    constexpr grid e18{interval{0, 1}, max_denominator{1'000'000'000'000'000'000}};
    using wide = inside<e18 * e18>; // denominators ≤ 10^36
    static_assert(fraction_storage<wide>);
    const auto ok = parse<wide>("0.000000000000000000000000000000000001"); // 10^-36
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(parse<wide>("0.0000000000000000000000000000000000001").error(), errc::rounding_error); // 10^-37
    // A rounding policy rounds onto the bound instead: 10^-37 floors to 0, ceils to 10^-36.
    EXPECT_TRUE((parse<wide, round_floor>("0.0000000000000000000000000000000000001").value() == 0));
    EXPECT_TRUE((parse<wide, round_ceil>("0.0000000000000000000000000000000000001").value() == *ok));
}

// Exact math outputs on continuous inputs keep the bound.
TEST(BoundedRationalTest, abs_copysign_fmod_keep_the_bound) {
    using signed_quot = decltype(inside<{-100, 100}>{} / den{});
    const signed_quot q{rational{-50, 3}};
    const auto        a = math::abs(q);
    static_assert(grid_of<std::remove_cvref_t<decltype(a)>>.MaxDenominator == 7);
    EXPECT_TRUE(a == rational(50, 3));
    const auto c = math::copysign(q, 1_ins);
    static_assert(grid_of<std::remove_cvref_t<decltype(c)>>.MaxDenominator == 7);
    EXPECT_TRUE(c == rational(50, 3));
    const auto r = math::fmod(q, inside<{1, 4}>{3}); // −50/3 + 5·3 = −5/3
    static_assert(grid_of<std::remove_cvref_t<decltype(r)>>.MaxDenominator == 7);
    EXPECT_TRUE(lift_unwrap(r) == rational(-5, 3));
}

// A bounded continuous result holds every exact sum and product: where the
// 64-bit kernels' intermediates pass 64 bits (each operand over D_L·D_R
// here), the op computes exactly in wide integers. No check, no expected.
TEST(BoundedRationalTest, bounded_results_are_total_past_64_bit_intermediates) {
    using L = inside<{{1ull << 40, (1ull << 40) + 1}, max_denominator{4096}}>;
    using R = inside<{{-((1ll << 40) + 1), -(1ll << 40)}, max_denominator{4096}}>;
    constexpr L    a{(rational{1ull << 40} + rational{1, 3})};
    constexpr R    b{-(1ll << 40)};
    constexpr auto s = a + b;
    static_assert(insidable<decltype(s)>); // a plain value
    static_assert(s == rational{1, 3});
    EXPECT_EQ(static_cast<rational>(a - (-b)), (rational{1, 3}));
}

// Fractions past 1024 bits (detail::max_fraction_bits) have no type: the operator
// is deleted, so a probe answers, and mul_into rounds the product instead.
template <class A, class B>
constexpr bool can_mul = requires(A a, B b) { a * b; };
template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };

TEST(BoundedRationalTest, results_past_the_fraction_cap_have_no_type) {
    using billionths = inside<{interval{0, 1}, max_denominator{1'000'000'000}}>;
    using b2         = decltype(billionths{} * billionths{});
    using b4         = decltype(b2{} * b2{});
    using b8         = decltype(b4{} * b4{});
    using b16        = decltype(b8{} * b8{}); // denominators to 10^144
    static_assert(can_mul<b8, b8> && can_add<b16, billionths>);
    static_assert(!can_mul<b16, b16> && !can_add<b16, b16>); // 10^288: past 1024 bits
    using cent      = inside<{{0, 1}, per<100>}, round_nearest>;
    const b16 tenth = rational{1, 10};
    EXPECT_EQ(mul_into<cent>(tenth, tenth), rational(1, 100));
}
} // namespace bounded_rational_tests

// Pinned defects: each case failed before its fix.
namespace pinned_tests {
using namespace beman::inside;
using namespace beman::inside::detail;

template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };
template <class A, class B>
constexpr bool can_mul = requires(A a, B b) { a * b; };
template <class A, class B>
constexpr bool can_div = requires(A a, B b) { a / b; };
template <class A, class B>
constexpr bool can_mod = requires(A a, B b) { a % b; };
template <class A, class B>
constexpr bool can_divmod = requires(A a, B b) { divmod(a, b); };
template <class T, class S>
constexpr bool can_make = requires(S s) { T::try_make(s); };
template <class T, class S>
constexpr bool can_add_assign = requires(T t, S s) { t += s; };
template <class T, class S>
constexpr bool can_div_assign = requires(T t, S s) { t /= s; };
using element                 = inside<{0, 100}>;
struct zero_end { // the end of a range: the element 0
    friend bool operator==(std::vector<element>::const_iterator i, zero_end) { return *i == 0; }
};

// An unanchored sum counted in half units passes 63 bits: the wrapping work
// type then divided a wrapped value (−1 came out as INT64_MAX).
TEST(ArithmeticTest, unanchored_sum_past_63_bits) {
    static_assert([] {
        constexpr std::int64_t p = std::int64_t{1} << 61;
        constexpr rational     hi{2 * p - 1, 2}, lo{-2 * p - 1, 2};
        using L = inside<{{lo, hi}, 1}>;
        return L{-0.5} + L{-0.5} == -1 && L{hi} + L{hi} == 2 * p - 1 && L{lo} + L{lo} == -2 * p - 1 &&
               L{hi} - L{lo} == 2 * p;
    }());
}

// Sums counted in a unit finer than the result's notch (an unanchored
// result): the offset from Lower in that unit passed int64.
TEST(ArithmeticTest, unanchored_sum_near_int64) {
    using halves = inside<{{0.5, 10.5}, 1}>; // Lower off the notch's multiples
    using thirds = inside<{{-(3ll << 60), 3ll << 60}, 3}>;
    const auto s = halves{rational{1, 2}} + thirds{3ll << 60};
    EXPECT_EQ(to_string(s), "3458764513820540928.5");
    EXPECT_EQ(to_string(halves{rational{1, 2}} - thirds{-(3ll << 60)}), "3458764513820540928.5");
}

TEST(ArithmeticTest, operations_are_constraint_friendly) {
    constexpr std::int64_t i64_min = INT64_MIN;
    constexpr std::int64_t i64_max = INT64_MAX;
    using full                     = inside<{i64_min, i64_max}>;
    using digit                    = inside<{0, 9}>;
    using reals                    = inside<{{-2, 2}, max_denominator{1000}}>;
    using fourths                  = inside<{{-2, 3}, per<4>}>;
    static_assert(can_div<digit, digit> && can_div<reals, digit>);
    static_assert(can_div<digit, reals>);        // a bounded divisor stays 1/1000 from 0
    static_assert(!can_div<digit, inside<{0}>>); // the point 0
    static_assert(can_mod<digit, digit> && can_mod<fourths, digit> && !can_mod<reals, digit>); // a lattice each
    static_assert(can_divmod<digit, inside<{1, 3}>> && can_divmod<fourths, digit> && !can_divmod<reals, digit>);
    static_assert(can_add<digit, fourths> && can_mul<fourths, fourths>);
    static_assert(can_add<uint64, uint64> && can_mul<full, full>);
    static_assert(can_make<digit, int> && can_make<digit, inside<{5, 20}>> && !can_make<digit, inside<{20, 30}>>);
    static_assert(!can_make<digit, fourths> && can_make<inside<{0, 9}, round_toward_zero>, fourths>);
    static_assert(can_add_assign<digit, digit> && can_add_assign<digit, inside<{10}>> &&
                  !can_add_assign<digit, fourths>);
    static_assert(!can_div_assign<digit, inside<{0}>> && can_div_assign<inside<{0, 9}, round_toward_zero>, digit>);
}

// Two integer raws whose common unit (the gcd of a 2/3 and a 2^-62 notch) is
// past the 64-bit rational: comparing them did not compile (the unit's
// expected was read in a constant expression); they compare by value.
TEST(ArithmeticTest, compare_without_a_common_unit) {
    using thirds = inside<{{rational{1, 3}, rational{31, 3}}, rational{2, 3}}, round_nearest>;
    using fine   = inside<{{0, 1}, per<(1ll << 62)>}, round_nearest>;
    static_assert(thirds{rational{1, 3}} < fine{rational{1, 2}});
    static_assert(thirds{1} > fine{rational{1, 2}});
    static_assert(thirds{1} == fine{1});
    EXPECT_EQ((thirds{rational{1, 3}} <=> fine{rational{1, 2}}), std::strong_ordering::less);
}

// The sum of two grids whose value units have no 64-bit gcd (1/(3·2^60)
// against 1/(7·2^58) here: offset by 1/3 and 2/3) failed to compile although
// the sum grid fits.
TEST(ArithmeticTest, sum_with_units_past_the_64_bit_gcd) {
    using L      = inside<{{rational{1, 3}, rational{4, 3}}, rational{1, 1ll << 60}}>;
    using R      = inside<{{rational{2, 3}, rational{5, 3}}, rational{1, 7ll << 58}}>;
    const auto s = L{rational{1, 3}} + R{rational{2, 3}};
    EXPECT_EQ(s, 1);
    const auto t = L::from_raw(12345) + R::from_raw(999);
    EXPECT_EQ(to_string(t), "8070450532248019243/8070450532247928832");
}

TEST(ArithmeticTest, sum_over_a_sentinel_range) {
    const std::vector<element> v{element{1}, element{2}, element{3}, element{0}};
    const auto                 total = sum<inside<{0, 10000}>>(std::ranges::subrange(v.cbegin(), zero_end{}));
    EXPECT_EQ(total, 6);
}

// A cross-grid comparison of an unanchored grid overflowed int64 forming
// raw·scale.
TEST(ArithmeticTest, unanchored_cross_grid_comparison) {
    using I64         = std::int64_t;
    constexpr I64 P61 = I64{1} << 61;
    using X           = inside<grid{{rational{-2 * P61 - 1, 2}, rational{2 * P61 - 1, 2}}, 1}>;
    using Y           = inside<grid{{0, 1}, rational{1, 2}}>;
    constexpr X cx    = rational{2 * P61 - 1, 2};
    constexpr Y cy    = 0;
    static_assert(cx > cy);
    static_assert(X{rational{-2 * P61 - 1, 2}} < cy);
}

//---------------------------------------------------------------------------
// addition<L, R>::add (addition.hpp)
//
// The rational-mixed `add` branch stores `((sum - lower_of<result>) / notch_of<result>)`
// directly into `res.Raw`. That's the L-offset, but when the result type is
// value_storage<result> the Raw must hold the *value*. Same encoding-
// mismatch class as the previously-fixed assignment paths.
//---------------------------------------------------------------------------
TEST(ArithmeticTest, rational_mixed_add_into_a_value_raw) {
    using L = inside<{-5, 5}>;                            // signed value-storage
    using R = inside<{{-10, 10}, max_denominator{1000}}>; // rational raw

    constexpr L l{2};
    constexpr R r{1_r};
    // Result grid: {-15, 15}, notch 1 → signed value-storage.
    static_assert(l + r == 3);
}

//---------------------------------------------------------------------------
// multiplication<L, R>::mul (multiplication.hpp)
//
// The third-quadrant case (lower_of<result> == upper_of<L> * lower_of<R>) computes
// `negRaw = max_index_of<L> - lhs.Raw`. That formula treats lhs.Raw as a
// notch-offset, which is correct for offset-encoded raws but wrong for
// value-storage signed raws (where Raw is the value).
//
// The integer_lattice fast path of multiplication<L, R>::mul catches the
// all-integer case, so the bug only surfaces when one operand has a
// fractional notch (which forces the result to be non-integer-aligned and
// skips the fast path). L keeps value storage (Notch_L = 1, signed lower).
//---------------------------------------------------------------------------
TEST(ArithmeticTest, signed_multiplication_in_the_third_quadrant) {
    using L = inside<{-5, 5}>;                      // signed value-storage, integer-aligned
    using R = inside<{{-10, 10}, rational{1u, 2}}>; // notch 1/2, not value storage, not integer-aligned

    // lower_of<result> = upper_of<L> * lower_of<R> = 5 * -10 = -50 → third quadrant.
    // Without the fix, L{2} * R{1} produces value -3 instead of 2.
    static_assert(L{2} * R{rational{1u}} == rational{2u});
    static_assert(L{3} * R{rational{2u}} == rational{6u});
    static_assert(L{0} * R{rational{5u}} == rational{0u});
}

//---------------------------------------------------------------------------
// arithmetic past double's 53-bit significand.
//
// A `×` whose product grid outgrows 2^53 (notch = N_L·N_R) keeps every
// bit: the result equals the exact rational product.
//---------------------------------------------------------------------------
TEST(ArithmeticTest, product_stays_exact_past_2_53) {
    using U = inside<{{0, 4}, per<(1u << 26)>}>; // f=26

    const U        a  = 4.0 - std::ldexp(1.0, -26); // index 2^28-1, exact
    auto           p  = a * a;                      // product grid f=52 > 53 bits
    const rational ar = static_cast<rational>(a);
    ASSERT_TRUE(exact{p} == exact{ar} * exact{ar});
}

//---------------------------------------------------------------------------
// Comparing integer storage with a floating or wide unsigned scalar must not
// truncate or wrap the scalar.
//---------------------------------------------------------------------------
TEST(ArithmeticTest, scalar_comparison_does_not_truncate_the_scalar) {
    using namespace beman::inside;
    inside<{-10, 10}> one{1}, minus_one{-1};
    static_assert(detail::integer_value_storage<decltype(one)>);
    EXPECT_FALSE(one == 1.5_r);
    EXPECT_TRUE(one < 1.5_r);
    EXPECT_TRUE(minus_one > -1.5_r);
    EXPECT_TRUE(one == 1.0_r);
    EXPECT_TRUE(one < std::numeric_limits<std::uint64_t>::max());
}

// ++ moves one notch (1/256 here); += point adds the value.
TEST(ArithmeticTest, increment_moves_one_notch) {
    using namespace beman::inside;
    using rl = inside<{{-4, 4}, per<256>}, round_nearest>;
    rl x{rational{3, 2}};
    ++x;
    EXPECT_EQ(rational{x}, (rational{385, 256}));
    x += 1_ins;
    EXPECT_EQ(rational{x}, (rational{641, 256}));
    --x;
    EXPECT_EQ(rational{x}, (rational{5, 2}));
}

//---------------------------------------------------------------------------
// lift (inside/detail/lift.hpp) — the binary expected-lift short-circuits on a RIGHT-side
// error (the left-error short-circuit was already covered).
//---------------------------------------------------------------------------
// expected-lift propagates a right-hand-side error
TEST(ArithmeticTest, expected_lift_propagates_a_right_hand_side_error) {
    using num_t = inside<{0, 100}>;
    std::expected<num_t, errc> left{num_t{5}}; // valid
    std::expected<num_t, errc> right{std::unexpected{errc::overflow}};

    auto both = left + right; // lhs OK, rhs error -> rhs.error() wins
    ASSERT_FALSE(both.has_value());
    ASSERT_EQ(both.error(), errc::overflow);
}

//---------------------------------------------------------------------------
// Point-operand multiply — `x * just<c>` scales x's lattice (notch N·|c|), so the
// product keeps integer storage and its offset is x's offset (mirrored for c < 0).
//---------------------------------------------------------------------------
TEST(ArithmeticTest, multiply_by_point_keeps_integer_storage) {
    using U  = inside<{0, 200}>;
    using R3 = decltype(U{} * just<3>);
    static_assert(!rational_storage<R3>);
    static_assert(notch_of<R3> == 3 && lower_of<R3> == 0 && upper_of<R3> == 600);
    using RH = decltype(midpoint(U{}, U{}));
    static_assert(!rational_storage<RH>);
    static_assert(notch_of<RH> == rational{1, 2});
}

TEST(ArithmeticTest, multiply_by_point_matches_exact_product) {
    for (int v : {-100, -7, 0, 1, 99, 100}) {
        inside<{-100, 100}> x{v};
        EXPECT_EQ(rational{x * just<3>}, rational{3 * v});
        EXPECT_EQ(rational{just<3> * x}, rational{3 * v});
        EXPECT_EQ(rational{x * just<-2>}, rational{-2 * v});
        EXPECT_EQ((exact{x * just<-1, 4>}), (exact{v} * exact{rational{-1, 4}}));
    }
    inside<{0, 200}> a{3}, b{4};
    EXPECT_EQ(rational{midpoint(a, b)}, (rational{7, 2}));
}

// sum<Target> of continuous elements added 64-bit rationals: a partial sum
// whose denominator passed 64 bits reported overflow, however small the
// total. It adds exactly over lcm(1..D) now; n copies of a point are n·value.
TEST(ArithmeticTest, continuous_sum_is_exact) {
    using fine = inside<{{0, 1}, max_denominator{2000}}>;
    // Six primes: the reduced sum's denominator, their product, passes 2^64.
    const std::array<fine, 6> r{fine{rational{1, 1999}},
                                fine{rational{1, 1997}},
                                fine{rational{1, 1993}},
                                fine{rational{1, 1987}},
                                fine{rational{1, 1979}},
                                fine{rational{1, 1973}}};
    using total  = inside<{{0, 6}, per<(1ll << 40)>}, round_nearest>;
    const auto s = beman::inside::sum<total>(r); // Σ 1/p, rounded once
    EXPECT_EQ(s.raw(), 3318520424);              // round(2^40 · Σ 1/p)
    using sixths = inside<{{-1, 1}, max_denominator{6}}>;
    const std::array<sixths, 4> h{
        sixths{rational{1, 2}}, sixths{rational{1, 3}}, sixths{rational{1, 6}}, sixths{rational{-5, 6}}};
    using sixths_total = inside<{{-4, 4}, max_denominator{6}}>;
    EXPECT_EQ(rational{beman::inside::sum<sixths_total>(h)}, (rational{1, 6}));
    const std::array<std::remove_cvref_t<decltype(just<1, 3>)>, 5> p{}; // a point grid: 5 copies of 1/3
    using thirds_total = inside<{{0, 2}, max_denominator{3}}>;
    EXPECT_EQ(rational{beman::inside::sum<thirds_total>(p)}, (rational{5, 3}));
}

} // namespace pinned_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace beman::inside;
using detail::rational;

// Disjoint operand ranges decide min and max at compile time, either order.
TEST(ArithmeticArmTest, min_and_max_of_disjoint_ranges) {
    const inside<{0, 5}>   lo{3};
    const inside<{10, 20}> hi{15};
    EXPECT_TRUE(min(lo, hi) == 3 && min(hi, lo) == 3);
    EXPECT_TRUE(max(lo, hi) == 15 && max(hi, lo) == 15);
}

// div_into on rational raws: when the unreduced quotient passes 64 bits, the
// cross-reduced one (2^59/1023 ÷ 2^59/1021 = 1021/1023) or else the exact
// wide one is stored, as the exact quotient rounds.
TEST(ArithmeticArmTest, div_into_past_the_unreduced_quotient) {
    using C   = inside<{{1, 1125899906842624_g}, max_denominator{1024}}>; // [1, 2^50]
    using Out = inside<{{0, 2}, per<1024>}, round_nearest>;
    static_assert(std::is_same_v<detail::raw_t<C>, rational>);
    const C x{rational{std::uint64_t{1} << 59, 1023}}, y{rational{std::uint64_t{1} << 59, 1021}};
    const C u{rational{(std::uint64_t{1} << 59) + 1, 1023}}, v{rational{(std::uint64_t{1} << 59) + 3, 1021}};
    EXPECT_TRUE(div_into<Out>(x, y) == Out{x / y});
    EXPECT_TRUE(div_into<Out>(u, v) == Out{u / v});
}

// sum of continuous elements re-reduces a raw built unreduced (2/8), whose
// denominator does not divide lcm(1..D).
TEST(ArithmeticArmTest, sum_reduces_an_unreduced_raw) {
    using T = inside<{{0, 1}, max_denominator{4}}>;
    rational quarter;
    quarter.Num = 2;
    quarter.Den = 8;
    const std::vector<T> v{T::from_raw(quarter), T{rational{1, 4}}};
    using total = inside<{{0, 2}, max_denominator{4}}>;
    EXPECT_EQ(rational{beman::inside::sum<total>(v)}, (rational{1, 2}));
}
} // namespace reached_arms_tests
