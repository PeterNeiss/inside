// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Defects from the 2026-10-12 audit. The constexpr cases fail to compile on
// the unfixed build: constant evaluation rejects the signed overflow (UB).

#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <compare>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

using namespace beman::inside;

namespace {
constexpr std::int64_t i64_min = INT64_MIN;
constexpr std::int64_t i64_max = INT64_MAX;
constexpr std::int64_t base    = std::int64_t{1} << 48;
} // namespace

// Integer raw mapping into a full-width int64 target: the old map_raw added
// trunc(Offset) (up to 2^63) before raw_lo<L>, overflowing on the way.
TEST(Audit20261012, raw_map_into_full_int64) {
    using full = inside<{i64_min, i64_max}>;
    static_assert([] {
        int64 a = inside<{0, 100}>{5};
        full  b = inside<{0, 100}>{7};
        full  c = inside<{i64_min, 0}>{i64_min};
        return a == 5 && b == 7 && c == i64_min;
    }());
    static_assert([] {
        inside<{-10, 10}, clamp> lo = full{i64_min};
        inside<{-10, 10}, clamp> hi = full{i64_max};
        return lo == -10 && hi == 10;
    }());
}

// Scalar stores into a full-width int64 grid: the Q-format guard took the
// ceiling of a 2^63 bound in imax, and the range check negated −2^63.
TEST(Audit20261012, scalar_store_into_full_int64) {
    using full = inside<{i64_min, i64_max}>;
    static_assert([] {
        int64 a{2.0};
        int64 b{rational{-7}};
        full  c{rational{-3}};
        c += rational{1};
        full d{-9.0e18};
        return a == 2 && b == -7 && c == -2 && d == -9'000'000'000'000'000'000;
    }());
}

// A Q-format grid far from zero: Lower·2^frac passes imax, so the integer
// codec does not apply (it overflowed in decode; the type did not compile).
TEST(Audit20261012, far_qformat_grid) {
    using Q = inside<{{base, base + 1}, per<65536>}>;
    static_assert([] {
        Q x{base};
        Q y{rational{base * 2 + 1, 2}};
        return rational(x) == rational{base} && rational(y) == rational{base * 2 + 1, 2} &&
               y.to<imax, round_toward_zero>() == base && y.to<imax>().error() == errc::rounding_error;
    }());
}

// `%` under rounding: INT64_MIN % -1 divided natively (UB), and a quotient
// rounded away from zero made q·b pass imax. The remainder is now adjusted
// from C++'s `%` instead of formed as a − q·b.
TEST(Audit20261012, modulo_at_int64_limits) {
    static_assert([] {
        inside<{i64_min, i64_max}> a{i64_min};
        inside<{-3, -1}>           b{-1};
        return a % b == 0;
    }());
    static_assert([] {
        inside<{1, 2}> two{2};
        return inside<{0, i64_max}, round_ceil>{i64_max} % two == -1 &&
               inside<{0, i64_max}, round_nearest>{i64_max} % two == -1;
    }());
    static_assert([] {
        inside<{1, 3}> two{2};
        return inside<{-7, 7}, round_floor>{-7} % two == 1 && inside<{-7, 7}, round_ceil>{7} % two == -1;
    }());
}

// Quotient grid of a divisor spanning 0: the smallest divisor magnitude was
// taken as the notch, but an unanchored lattice comes closer to 0
// ({{-1.5, 1.5}, 1} holds ±0.5), so 10 / 0.5 = 20 escaped the [-10, 10] grid.
TEST(Audit20261012, quotient_grid_of_unanchored_divisor) {
    static_assert([] {
        auto q  = *(inside<{0, 10}>{10} / inside<{{-1.5, 1.5}, 1}>{0.5});
        using Q = decltype(q);
        return q == 20 && upper_of<Q> >= rational{20} && lower_of<Q> <= rational{-20};
    }());
    static_assert(!(grid{0, 10} / grid{{0, 1}, 0}).has_value()); // unbounded continuous divisor
}

// rational{unsigned, unsigned} with a den past imax: the cast to the signed
// denominator flipped the sign silently.
TEST(Audit20261012, two_unsigned_rational_rejects_large_den) {
    EXPECT_THROW((void)(rational{1u, 0x8000'0000'0000'0001ull}), inside_error);
    static_assert(rational{1u, 0x7FFF'FFFF'FFFF'FFFFull} > rational{0});
}

// A double finer than 2^-62 lost its low bits (truncated toward zero) on the
// way to rational: ceil stored 0, a strict grid stored 0 without
// rounding_error, and comparisons saw the truncated value.
TEST(Audit20261012, tiny_double_is_exact) {
    using cent_ceil = inside<{{0, 1}, per<100>}, round_ceil>;
    static_assert(cent_ceil{1e-20} == rational{1, 100});
    static_assert(inside<{{0, 1}, per<100000>}, round_ceil>{1e-5} == rational{2, 100000}); // 1e-5 is above 10^-5
    static_assert(inside<{{0, 1}, per<100>}, round_floor>{1e-20} == 0);
    using cent = inside<{{0, 1}, per<100>}>;
    EXPECT_FALSE(cent::try_make(1e-20).has_value());
    EXPECT_FALSE((inside<{{0, 1}, per<100>}, round_floor>::try_make(-1e-20).has_value())); // floors to -0.01
    EXPECT_EQ((inside<{{0, 1}, per<100>}, round_ceil>{-1e-20}), 0);
    EXPECT_FALSE((inside<grid{interval{0, 1}, 0}>::try_make(1e-20).has_value())); // no 64-bit rational holds it
    constexpr inside<grid{interval{0, 1}, 0}> zero{0};
    static_assert(zero < 1e-20 && zero != 1e-20);
}

TEST(Audit20261012, tiny_double_predicates) {
    using unit = inside<{{0, 1}, per<100>}>;
    static_assert(conversion_rounds<unit>(1e-20) && !conversion_overflows<unit>(1e-20));
    static_assert(conversion_overflows<unit>(-1e-20) && !conversion_rounds<unit>(-1e-20));
    static_assert(conversion_rounds<inside<{{0, 1}, per<100>}, round_nearest>>(-1e-20));
}

// An unanchored sum counted in half units passes 63 bits: the wrapping work
// type then divided a wrapped value (−1 came out as INT64_MAX).
TEST(Audit20261012, unanchored_sum_past_63_bits) {
    static_assert([] {
        constexpr std::int64_t p = std::int64_t{1} << 61;
        constexpr rational     hi{2 * p - 1, 2}, lo{-2 * p - 1, 2};
        using L = inside<{{lo, hi}, 1}>;
        return L{-0.5} + L{-0.5} == -1 && L{hi} + L{hi} == 2 * p - 1 && L{lo} + L{lo} == -2 * p - 1 &&
               L{hi} - L{lo} == 2 * p;
    }());
}

// Fixed-point division on a 32-bit boundary: ja·K needs 32 value bits plus
// the sign, but the work type was int32.
TEST(Audit20261012, fixed_division_work_type) {
    using L = inside<{{0, rational{2147483647, 2}}, per<2>}, round_nearest>;
    using R = inside<{{rational{1, 2}, 1}, per<2>}>;
    static_assert(L{rational{2147483647, 2}} / R{rational{1, 2}} == 2147483647);
}

// Checked rational / and + reported overflow for results that fit.
TEST(Audit20261012, rational_ops_reduce_before_overflow) {
    constexpr umax big = (umax{1} << 63) + 2;
    static_assert(*(rational{big} / rational{big}) == rational{1});
    static_assert(*(rational{big} / rational{big, 3u}) == rational{3});
    static_assert(*detail::try_add(rational{(umax{1} << 63) + 1, 2u}, rational{1, 6}) ==
                  rational{3 * (umax{1} << 62) + 2, 3u});
    static_assert(*detail::try_add(rational{~umax{0}, 2u}, rational{1u, 2u}) == rational{umax{1} << 63});
    static_assert(!detail::try_add(rational{~umax{0}}, rational{1}).has_value());
}

// INT64_MIN / -1 reaches 2^63: the quotient grid's endpoint wrapped in imax.
TEST(Audit20261012, quotient_grid_reaches_2_pow_63) {
    static_assert([] {
        inside<{i64_min, 0}, snap> a{i64_min};
        inside<{-2, -1}>           b{-1};
        auto                       q = a / b;
        using Q                      = decltype(q);
        return rational(q) == rational{umax{1} << 63} && upper_of<Q> == rational{umax{1} << 63};
    }());
}

// Comparing with NaN threw (rational{NaN} raises not_finite) on grids
// without an integer or double fast path; it is unordered.
TEST(Audit20261012, nan_compares_unordered) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    using C          = inside<grid{interval{0, 1}, 0}>;
    using thirds     = inside<{{0, 1}, rational{1, 3}}>;
    const C      c{rational{1, 2}};
    const thirds t{rational{1, 3}};
    EXPECT_FALSE(c == nan);
    EXPECT_TRUE(c != nan);
    EXPECT_FALSE(c < nan);
    EXPECT_TRUE((c <=> nan) == std::partial_ordering::unordered);
    EXPECT_FALSE(t == nan);
    EXPECT_TRUE((t <=> nan) == std::partial_ordering::unordered);
    EXPECT_TRUE(c < std::numeric_limits<double>::infinity());
}

// An index grid reaching 2^63: Raw·Notch passes 64 bits on the way to a
// value that fits (the decode dereferenced a failed expected), the store's
// (v − Lower)/Notch did too, and the range check read 2^63 as INT64_MIN.
TEST(Audit20261012, index_grid_reaching_2_pow_63) {
    static_assert([] {
        constexpr rational lo{i64_min}, hi{umax{1} << 63};
        using G = inside<{{lo, hi}, 2}>;
        G x{hi};
        G y{lo};
        return rational(x) == hi && rational(y) == lo && x == hi && x.to<double>() == 0x1p63;
    }());
}

// to<T, round_toward_zero>() truncates first, then range-checks: 127.5 →
// int8 is 127, −0.5 → unsigned is 0. Without a rounding a fractional value
// is a rounding_error.
TEST(Audit20261012, to_truncates_before_the_range_check) {
    using H = inside<{{-0.5, 200}, 0.5}>;
    static_assert(*H{-0.5}.to<unsigned, round_toward_zero>() == 0u);
    static_assert(*H{127.5}.to<std::int8_t, round_toward_zero>() == 127);
    static_assert(H{127.5}.to<std::int8_t>().error() == errc::rounding_error);
    static_assert(*H{127.5}.to<int, round_nearest>() == 128 && *H{127.5}.to<int, round_floor>() == 127);
    static_assert(*inside<{{0, 10}, per<4>}, round_nearest>{2.75}.to<int>() == 3); // the type's own mode
    static_assert(H{128}.to<std::int8_t>().error() == errc::overflow);
    static_assert(inside<{-5, 5}>{-1}.to<unsigned>().error() == errc::domain_error);
}

// A bounded continuous grid rejects a denominator past its bound: so does
// conversion_rounds (it said false, and parse_exact relied on it).
TEST(Audit20261012, conversion_rounds_on_bounded_continuous) {
    using B = inside<grid{interval{0, 1}, max_denominator{2}}>;
    static_assert(conversion_rounds<B>(rational{1, 3}) && !conversion_rounds<B>(rational{1, 2}));
    static_assert(!conversion_rounds<B>(0.5) && conversion_rounds<B>(0.25));
    static_assert(!conversion_rounds<B>(rational{3}) && conversion_overflows<B>(rational{3}));
    EXPECT_FALSE(parse_exact<B>(std::string_view{"1/3"}).has_value());
}

// tan at a pole on a one-limb output: the "unresolved" error ~0 read as −1,
// inverted the interval, and decided 0.
TEST(Audit20261012, tan_near_pole_overflows) {
    using In  = inside<{{0, 2}, per<(std::uint64_t{1} << 40)>}, round_nearest>;
    using Out = inside<{{-4, 4}, per<8>}, round_nearest>;
    const In x{1.5707963267948966};
    for (int d = -2; d <= 2; ++d) {
        const auto r = math::tan_into<Out>(In::from_raw(x.raw() + d));
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error(), errc::overflow);
    }
}

// The range gates of the math kernels read a grid's largest magnitude: one
// reaching 2^63 wrapped to −1 and passed every gate.
TEST(Audit20261012, math_gates_fail_closed_on_huge_grids) {
    static_assert(math::detail::engine::in_max<uint64> > 1e18);
}

// pow of an exact rational result with a non-integer exponent sat on a
// rounding boundary and was guessed at the precision cap.
TEST(Audit20261012, pow_rational_exponent_is_exact) {
    using half   = inside<{{0, 2}, per<2>}>;
    using qtr    = inside<{{0, 4}, per<4>}>;
    using up4    = inside<{{0, 4}, per<4>}, round_ceil>;
    using down4  = inside<{{0, 4}, per<4>}, round_floor>;
    using up8    = inside<{{0, 4}, per<8>}, round_ceil>;
    using down16 = inside<{0, 16}, round_floor>;
    EXPECT_EQ(*math::pow_into<up4>(qtr{2.25}, half{0.5}), 1.5);
    EXPECT_EQ(*math::pow_into<down4>(qtr{2.25}, half{0.5}), 1.5);
    EXPECT_EQ(*math::pow_into<up8>(qtr{2.25}, half{1.5}), 3.375);
    EXPECT_EQ(*math::pow_into<down16>(inside<{0, 16}>{4}, half{0.5}), 2);
}

// A second exponent sign was accepted ("1e-+5" read as 1e5).
TEST(Audit20261012, one_exponent_sign) {
    using B = inside<{0, 1'000'000}>;
    EXPECT_EQ(parse<B>(std::string_view{"1e-+5"}).error(), errc::invalid_format);
    EXPECT_EQ(parse<B>(std::string_view{"1e+-5"}).error(), errc::invalid_format);
    EXPECT_EQ(*parse<B>(std::string_view{"1e+5"}), 100000);
}

// pow with a zero base: 0^y = 0 for y > 0, 0^0 = 1, a pole for y < 0.
TEST(Audit20261012, pow_zero_base) {
    using B   = inside<{{0, 4}, per<4>}>;
    using E   = inside<{{-2, 2}, per<2>}>;
    using Out = inside<{{0, 16}, per<16>}, round_nearest>;
    EXPECT_EQ(*math::pow_into<Out>(B{0}, E{1.5}), 0);
    EXPECT_EQ(*math::pow_into<Out>(B{0}, E{0}), 1);
    EXPECT_EQ(math::pow_into<Out>(B{0}, E{-1}).error(), errc::division_by_zero);
    EXPECT_EQ(*math::pow_into<Out>(B{2}, E{2}), 4);
    using SB = inside<{{-4, 4}, per<4>}>;
    EXPECT_EQ(math::pow_into<Out>(SB{-2}, E{2}).error(), errc::domain_error);
}

// A bounded continuous grid rounds an off-bound value onto its bound under
// a rounding policy (0.1 as a double is not 1/10); strict still rejects.
TEST(Audit20261012, bounded_continuous_rounds_onto_the_bound) {
    using N = inside<grid{interval{0, 1}, max_denominator{1000}}, round_nearest>;
    static_assert(N{0.1} == rational{1, 10});
    using F2 = inside<grid{interval{0, 1}, max_denominator{2}}, round_floor>;
    using C2 = inside<grid{interval{0, 1}, max_denominator{2}}, round_ceil>;
    static_assert(F2{rational{1, 3}} == 0 && C2{rational{1, 3}} == rational{1, 2});
    static_assert(inside<grid{interval{0, 1}, max_denominator{7}}, round_nearest>{rational{1000, 2001}} ==
                  rational{1, 2});
    static_assert(inside<grid{interval{-1, 1}, max_denominator{3}}, round_floor>{rational{-1, 4}} == rational{-1, 3});
    static_assert(inside<grid{interval{-1, 1}, max_denominator{3}}, round_nearest>{rational{-3, 10}} ==
                  rational{-1, 3});
    EXPECT_FALSE((inside<grid{interval{0, 1}, max_denominator{2}}>::try_make(rational{1, 3}).has_value()));
}

// Directed rounding of a result that nears a grid point beyond the precision
// cap: the deep step decides it (tanh(50) floor gave 1).
TEST(Audit20261012, math_deep_step_at_the_cap) {
    using F  = inside<{{-1, 1}, per<65536>}, round_floor>;
    using C  = inside<{{-1, 1}, per<65536>}, round_ceil>;
    using In = inside<{-100, 100}>;
    EXPECT_EQ(math::tanh_into<F>(In{50}), rational(65535, 65536));
    EXPECT_EQ(math::tanh_into<C>(In{-50}), rational(-65535, 65536));
    using EC = inside<{{0, 1}, per<1024>}, round_ceil>;
    EXPECT_EQ(math::exp_into<EC>(inside<{-100, 0}>{-60}), rational(1, 1024));
    using X  = inside<{{0, 1}, per<(std::uint64_t{1} << 40)>}>;
    using SF = inside<{{0, 1}, per<(std::uint64_t{1} << 40)>}, round_floor>;
    EXPECT_EQ(math::sin_into<SF>(X{rational{3u, std::uint64_t{1} << 40}}).raw(), 2u);
}

// Text past the 64-bit parse: the wide fallback reads the whole grammar
// (hex, binary, p exponents, separators), and a value too fine even for it
// stores by a same-sign stand-in below every grid step.
TEST(Audit20261012, text_past_the_64_bit_parse) {
    using H  = inside<{{0, 1}, per<100>}, round_nearest>;
    using HC = inside<{{0, 1}, per<100>}, round_ceil>;
    using S  = inside<{{0, 1}, per<100>}>;
    EXPECT_EQ(*parse<H>(std::string_view{"1e-200"}), 0);
    EXPECT_EQ(*parse<HC>(std::string_view{"1e-200"}), rational(1, 100));
    EXPECT_EQ(parse<S>(std::string_view{"1e-200"}).error(), errc::rounding_error);
    EXPECT_EQ(*parse<HC>(std::string_view{"-1e-200"}), 0);
    EXPECT_EQ(*parse<HC>(std::string_view{"1e-5000"}), rational(1, 100)); // past the wide parse
    EXPECT_EQ(parse_exact<S>(std::string_view{"1e-5000"}).error(), errc::rounding_error);
    EXPECT_TRUE(*parse<uint64>(std::string_view{"0x1p63"}) == (umax{1} << 63));
    EXPECT_EQ(*parse<H>(std::string_view{"0x1p-70"}), 0);
    EXPECT_TRUE(*parse<uint64>(std::string_view{"18'446'744'073'709'551'615"}) == ~umax{0});
    const std::string third = "0." + std::string(299, '3') + "4";
    EXPECT_EQ(*parse<H>(std::string_view{third}), rational(33, 100));
    const std::string over_half = "0.5" + std::string(298, '0') + "1";
    EXPECT_EQ((*parse<inside<{0, 1}, round_nearest>>(std::string_view{over_half})), 1);
}
