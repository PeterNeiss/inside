// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// The integer fast paths against the exact paths they shortcut: every store,
// conversion and quotient below is computed once by the library's public
// operation (which takes the fast path) and once by the exact wide store or
// exact arithmetic (detail::assign_exact, exact_of), over every value of a
// small grid or a sweep past its limits, under every policy.
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/mdspan.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <limits>
#include <mdspan>
#include <random>
#include <string>
#include <vector>

namespace fast_path_tests {
using namespace ins;
namespace bd = ins::detail;

// The exact store of v into L under flags F: the value, or the error.
template <class L, policy_flag F, class V>
std::expected<L, error> exact_store(const V& v) {
    error ec{};
    L     l{};
    if constexpr (std::floating_point<V>)
        bd::assign_exact<V>(l, bd::exact_of_double(v), make_policy<F>(ec), bd::no_action{});
    else
        bd::assign_exact<V>(l, bd::exact_of(v), make_policy<F>(ec), bd::no_action{});
    if (ec != error{})
        return std::unexpected{ec};
    return l;
}
// The store under test: try_make<F> (checked) or the flag constructor.
template <class L, policy_flag F, class V>
std::expected<L, error> fast_store(const V& v) {
    return L::template try_make<F>(v);
}
template <class V>
std::string text_of(const V& v) {
    if constexpr (std::floating_point<V>) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.17g", v);
        return buf;
    } else
        return to_string(bd::as_rational(v));
}
template <class L, policy_flag F, class V>
void expect_same(const V& v, const char* what) {
    const auto f = fast_store<L, F>(v);
    const auto e = exact_store<L, F>(v);
    ASSERT_EQ(f.has_value(), e.has_value()) << what << " v=" << text_of(v);
    if (f) {
        EXPECT_EQ(f->raw(), e->raw()) << what << " v=" << text_of(v);
    } else {
        EXPECT_EQ(f.error(), e.error()) << what;
    }
}

template <policy_flag F>
void integer_sweep() {
    using n60   = id<"[-600..600] 60">;
    using n72   = id<"[-7..14] 7/2">;
    using un    = id<"[1..601] 60">;
    using half  = id<"[1/2..201/2] 1">;
    using q88   = id<"[0..255] 1/256">;
    using n53   = id<"[-5..5] 5/3">;                      // period·K odd: the parity carries
    using nhalf = id<"[-7/2..9/2] 1">;                    // unanchored below 0
    for (std::int64_t n = -1000; n <= 1000; ++n) {
        expect_same<n60, F>(n, "n60");
        expect_same<n72, F>(n, "n72");
        expect_same<un, F>(n, "un");
        expect_same<half, F>(n, "half");
        expect_same<q88, F>(n, "q88");
        expect_same<n53, F>(n, "n53");
        expect_same<nhalf, F>(n, "nhalf");
    }
}

TEST(FastPathTest, integer_stores_match_the_exact_store) {
    integer_sweep<checked>();
    integer_sweep<round_floor>();
    integer_sweep<round_ceil>();
    integer_sweep<round_toward_zero>();
    integer_sweep<round_half_away>();
    integer_sweep<round_half_even>();
    integer_sweep<clamp>();
    integer_sweep<clamp | round_floor>();
    integer_sweep<wrap>();
    integer_sweep<wrap | round_half_even>();
    integer_sweep<wrap | round_half_away>();
    integer_sweep<wrap | round_toward_zero>();
    integer_sweep<wrap | round_floor>();
    integer_sweep<wrap | round_ceil>();
}

template <class L, class R, policy_flag F>
void inside_sweep(const char* what) {
    for (const R r : values<R>)
        expect_same<L, F>(r, what);
}
template <policy_flag F>
void cross_grid() {
    using dec2  = id<"[-50..50] 1/100">;
    using q88   = id<"[0..20] 1/256">;
    using n72   = id<"[-7..14] 7/2">;
    using s32   = id<"[-2000..2000]">;
    using n60   = id<"[-600..600] 60">;
    using dec3  = id<"[-5..5] 1/1000">;
    using q114  = id<"[-1..1] 1/16384">;
    using small = id<"[-10..10]">;
    inside_sweep<q88, dec2, F>("dec2->q88");
    inside_sweep<n72, q88, F>("q88->n72");
    inside_sweep<n60, s32, F>("s32->n60");
    inside_sweep<dec3, dec2, F>("dec2->dec3");
    inside_sweep<small, s32, F>("s32->small");
    inside_sweep<q114, q88, F>("q88->q114");
    // unanchored targets: Lower off the notch's multiples, above and below 0
    using un    = id<"[1..601] 60">;
    using uneg  = id<"[-599..601] 60">;
    using half  = id<"[1/2..201/2] 1">;
    using nhalf = id<"[-7/2..9/2] 1">;
    inside_sweep<un, n60, F>("n60->un");
    inside_sweep<un, s32, F>("s32->un");
    inside_sweep<uneg, s32, F>("s32->uneg");
    inside_sweep<half, s32, F>("s32->half");
    inside_sweep<half, dec2, F>("dec2->half");
    inside_sweep<nhalf, dec2, F>("dec2->nhalf");
    inside_sweep<nhalf, small, F>("small->nhalf");
    inside_sweep<nhalf, half, F>("half->nhalf");
}

TEST(FastPathTest, cross_grid_stores_match_the_exact_store) {
    cross_grid<round_floor>();
    cross_grid<round_ceil>();
    cross_grid<round_half_away>();
    cross_grid<round_half_even>();
    cross_grid<clamp | round_floor>();
    cross_grid<clamp | round_half_even>();
    cross_grid<wrap | round_half_even>();
    cross_grid<wrap | round_toward_zero>();
    cross_grid<round_toward_zero>();
    cross_grid<clamp | round_half_away>();
    cross_grid<wrap | round_half_away>();
    cross_grid<wrap | round_floor>();
}

// Products stored back onto their factors' grid: a Q16.16 product's
// numerator spans most of int64.
template <policy_flag F>
void products_stored_back() {
    using q1616                   = id<"[-30000..30000] 1/65536">; // raw: the index from Lower, 0 … 3932160000
    using half                    = id<"[1/2..201/2] 1">;
    constexpr std::int64_t raws[] = {0,
                                     1,
                                     65535,
                                     65537,
                                     123456789,
                                     1966014464,
                                     1966080000,
                                     1966080001,
                                     1966145536,
                                     2000000000,
                                     3932159999,
                                     3932160000};
    for (const std::int64_t a : raws)
        for (const std::int64_t b : raws)
            expect_same<q1616, F>(q1616::from_raw(a, unsafe) * q1616::from_raw(b, unsafe), "q1616*q1616");
    for (const auto a : values<half>)
        for (const auto b : {half{0.5_q}, half{1.5_q}, half{50.5_q}, half{100.5_q}})
            expect_same<half, F>(a * b, "half*half");
}

TEST(FastPathTest, products_stored_back_match_the_exact_store) {
    products_stored_back<round_half_even>();
    products_stored_back<round_floor>();
    products_stored_back<round_toward_zero>();
    products_stored_back<clamp | round_floor>();
    products_stored_back<clamp | round_half_away>();
    products_stored_back<wrap | round_half_even>();
    products_stored_back<wrap | round_toward_zero>();
    products_stored_back<unsafe | round_half_even>();
}

template <policy_flag F>
void double_sweep() {
    using dec2         = id<"[-1000..1000] 1/100">;
    using dec3         = id<"[-50..50] 1/1000">;
    using n60          = id<"[-600..600] 60">;
    using n72          = id<"[-7..14] 7/2">;
    using un           = id<"[1..601] 60">;
    using half         = id<"[1/2..201/2] 1">;
    using s64          = id<"[-4000000000000000000..4000000000000000000]">;
    std::uint64_t x    = 0x9e3779b97f4a7c15u;
    auto          next = [&] { return x = x * 6364136223846793005u + 1442695040888963407u; };
    auto          each = [&](double v) {
        expect_same<dec2, F>(v, "dec2");
        expect_same<dec3, F>(v, "dec3");
        expect_same<n60, F>(v, "n60");
        expect_same<n72, F>(v, "n72");
        expect_same<un, F>(v, "un");
        expect_same<half, F>(v, "half");
        expect_same<s64, F>(v, "s64");
    };
    for (int k = -2000; k <= 2000; k += 7) {
        // grid points, ties and their neighbours in double
        for (const double base : {k / 100.0,
                                  (k + 0.5) / 100.0,
                                  k / 1000.0,
                                  (k + 0.5) / 1000.0,
                                  k * 0.5,
                                  k * 7.0 / 4.0,
                                  k * 30.0,
                                  k * 60.0})
            for (const double v : {base, std::nextafter(base, -1e300), std::nextafter(base, 1e300), -base})
                each(v);
    }
    for (int i = 0; i < 4000; ++i) {
        const double u = static_cast<double>(next() >> 11) * 0x1p-53;
        each((u - 0.5) * 2400.0);
        each((u - 0.5) * 9.0e18);
    }
}

TEST(FastPathTest, double_stores_match_the_exact_store) {
    double_sweep<round_floor>();
    double_sweep<round_ceil>();
    double_sweep<round_toward_zero>();
    double_sweep<round_half_away>();
    double_sweep<round_half_even>();
    double_sweep<checked>();
    double_sweep<clamp | round_half_even>();
}

// to<float>'s tie test reads float's dropped bits off the double: against
// the float round trip (which decides ties at every scale) on random doubles
// and on constructed ties and their neighbours. It may send a tiny value or
// one past float's range to the exact path, but never misses a tie.
TEST(FastPathTest, the_float_tie_test_matches_the_round_trip) {
    auto round_trip = [](double d) {
        const float f = static_cast<float>(d);
        if (!std::isfinite(f))
            return true;
        const double g = static_cast<double>(f) + 2 * (d - static_cast<double>(f));
        return g != static_cast<double>(f) && static_cast<double>(static_cast<float>(g)) == g;
    };
    auto check = [&](double d) {
        if (!std::isfinite(d))
            return;
        const bool bits = bd::narrow_midpoint<float>(d), tie = round_trip(d);
        if (tie) {
            EXPECT_TRUE(bits) << d;
        } else if (std::fabs(d) >= 0x1p-126 && std::fabs(d) <= std::numeric_limits<float>::max()) {
            EXPECT_FALSE(bits) << d;
        }
    };
    std::mt19937_64 g{7};
    for (int i = 0; i < 200000; ++i) {
        check(std::bit_cast<double>(g()));
        const float f = std::bit_cast<float>(static_cast<std::uint32_t>(g()));
        if (std::isfinite(f) && f != 0) {
            const double h = (static_cast<double>(std::nextafter(f, std::numeric_limits<float>::infinity())) +
                              static_cast<double>(f)) /
                             2;
            for (const double d : {h, -h, std::nextafter(h, 0.0), std::nextafter(h, 1e300)})
                check(d);
        }
    }
    for (const double d : {0.0, -0.0, 0x1p-126, 0x1p-127, 0x1.000001p-126, 0x1.fffffep127, 0x1.ffffffp127, 5e-324})
        check(d);
}

// to_string of a binary notch prints from the raw: against the general
// fraction printer over every value.
template <class B>
void binary_text() {
    for (const B b : values<B>) {
        const bd::rational r = bd::as_rational(b);
        EXPECT_EQ(to_string(b), bd::fraction_to_string_u64(r.Den < 0, r.Num, bd::abs_den(r.Den), 0));
    }
}
// A binary notch is read within 64 bits: 1/2^63 is one, and a notch past 64 bits (1/2^70) is not (it compiles,
// rather than failing the checked conversion of its denominator), nor is a 1/3 notch.
static_assert(bd::binary_notch<id<key{grid{{0, 1}, per<(std::uint64_t{1} << 63)>}}>>);
static_assert(!bd::binary_notch<id<"[0..1] 0x1p-70">>);
static_assert(!bd::binary_notch<id<"[0..1] 1/3">>);

TEST(FastPathTest, binary_notches_print_as_the_fraction_printer) {
    binary_text<id<"[0..255] 1/256">>();
    binary_text<id<"[-2..2] 1/16384">>();
    binary_text<id<"[-1..1] 1/524288">>();
    binary_text<id<"[-3..5] 1/2">>();
    using q1616 = id<"[-100000..100000] 1/65536">; // a sample
    for (const std::int64_t raw : {std::int64_t{0},
                                   std::int64_t{1},
                                   std::int64_t{65535},
                                   std::int64_t{65536},
                                   std::int64_t{6553600000} - 1,
                                   std::int64_t{6553600000},
                                   std::int64_t{6553600001},
                                   std::int64_t{13107200000} - 3,
                                   std::int64_t{13107200000}}) {
        const q1616        b = q1616::from_raw(raw, unsafe);
        const bd::rational r = bd::as_rational(b);
        EXPECT_EQ(to_string(b), bd::fraction_to_string_u64(r.Den < 0, r.Num, bd::abs_den(r.Den), 0));
    }
}

// dot<Out> sums the products in int64 and rounds once: against the exact
// total stored by the exact store, totals past Out's range included.
template <class Out, policy_flag F>
void dot_sweep(std::mt19937_64& g) {
    using q = id<"[-2..2] 1/16384">;
    for (int trial = 0; trial < 2000; ++trial) {
        std::array<q, 8> a{}, b{};
        for (std::size_t i = 0; i < 8; ++i) {
            a[i] = q::from_raw(static_cast<std::uint32_t>(g() % 65537), unsafe);
            b[i] = q::from_raw(static_cast<std::uint32_t>(g() % 65537), unsafe);
        }
        const auto d     = dot<Out>(std::mdspan{a.data(), std::extents<std::size_t, 8>{}},
                                    std::mdspan{b.data(), std::extents<std::size_t, 8>{}});
        auto       exact = a[0] * b[0];
        using E          = id<key{grid{{-32, 32}, per<(std::int64_t{1} << 28)>}, unsafe}>; // the exact total: |Σ| ≤ 32
        E total          = E{exact};
        for (std::size_t i = 1; i < 8; ++i)
            total = E{total + a[i] * b[i]};
        const auto e = exact_store<Out, F>(total);
        if constexpr (requires { d.has_value(); }) {
            ASSERT_EQ(d.has_value(), e.has_value());
            if (d) {
                EXPECT_EQ(d->raw(), e->raw());
            } else {
                EXPECT_EQ(d.error(), e.error());
            }
        } else {
            EXPECT_EQ(d.raw(), e->raw());
        }
    }
}
TEST(FastPathTest, dot_products_round_once_as_the_exact_total) {
    std::mt19937_64 g{11};
    dot_sweep<id<"[-8..8] 1/16384 clamp|round_half_away">, clamp | round_half_away>(g);
    dot_sweep<id<"[-32..32] 1/1000 round_half_even">, round_half_even>(g);
    dot_sweep<id<"[-3..3] 1/100 round_floor">, round_floor>(g); // past the range: overflow
}

TEST(FastPathTest, a_full_int64_source_clamps_and_wraps) {
    using full  = id<key{grid{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}}>;
    using small = id<"[-10..10] clamp">;
    using ring  = id<"[-10..10] wrap">;
    for (const std::int64_t v : {std::numeric_limits<std::int64_t>::min(),
                                 std::int64_t{-11},
                                 std::int64_t{-10},
                                 std::int64_t{0},
                                 std::int64_t{10},
                                 std::int64_t{11},
                                 std::int64_t{12345},
                                 std::numeric_limits<std::int64_t>::max()}) {
        const full f = full::from_raw(v, unsafe);
        EXPECT_EQ((small{f}.raw()), (exact_store<small, clamp>(f)->raw())) << v;
        EXPECT_EQ((ring{f}.raw()), (exact_store<ring, wrap>(f)->raw())) << v;
    }
}

TEST(FastPathTest, the_clamp_hook_hears_the_exact_overshoot) {
    using T = id<"[0..100] 1/100 clamp">;
    for (const auto a : values<id<"[0..100] 1/4">>)
        for (const auto b : values<id<"[0..100] 1/4">>) {
            T                  x{a, clamp};
            const bd::rational sum   = *bd::try_add(bd::as_rational(a), bd::as_rational(b));
            bool               fired = false;
            x.on_clamp([&](auto&, const auto& over) {
                fired = true;
                EXPECT_EQ(to<double>(over), static_cast<double>(*bd::try_sub(sum, bd::rational{100}))); // exact: quarters
            }) += T{b, clamp};
            EXPECT_EQ(fired, sum > bd::rational{100});
        }
}

TEST(FastPathTest, a_wide_product_clamps_and_wraps) {
    using s64 = id<"[-4000000000000000000..4000000000000000000]">;
    using c64 = id<"[-4000000000000000000..4000000000000000000] clamp">;
    using w64 = id<"[-4000000000000000000..4000000000000000000] wrap">;
    for (const std::int64_t a : {std::int64_t{-3000000000},
                                 std::int64_t{-7},
                                 std::int64_t{0},
                                 std::int64_t{9},
                                 std::int64_t{2000000000},
                                 std::int64_t{3999999999}})
        for (const std::int64_t b :
             {std::int64_t{-4000000000}, std::int64_t{-1}, std::int64_t{3}, std::int64_t{2000000001}}) {
            c64 x = c64::from_raw(a, unsafe);
            x *= c64::from_raw(b, unsafe);
            const auto exact = exact_store<c64, clamp>(s64::from_raw(a, unsafe) * s64::from_raw(b, unsafe));
            EXPECT_EQ(x.raw(), exact->raw()) << a << " * " << b;
            w64 y = w64::from_raw(a, unsafe);
            y *= w64::from_raw(b, unsafe);
            const auto folded = exact_store<w64, wrap>(s64::from_raw(a, unsafe) * s64::from_raw(b, unsafe));
            EXPECT_EQ(y.raw(), folded->raw()) << a << " * " << b << " (wrap)";
        }
}

template <class X>
long long plain(const X& x) {
    if constexpr (requires { *x; })
        return *x;
    else
        return x;
}
template <class T>
void steps_and_values() {
    for (std::int64_t i = -3; i <= static_cast<std::int64_t>(bd::max_index_of<T>) + 3; ++i) {
        const auto f = T::from_step(i);
        if (i < 0 || i > static_cast<std::int64_t>(bd::max_index_of<T>)) {
            EXPECT_EQ(f.error(), error::overflow);
            continue;
        }
        ASSERT_TRUE(f) << i;
        EXPECT_EQ(static_cast<std::int64_t>(f->step()), i);
        const bd::rational v = bd::as_rational(*f);
        EXPECT_EQ(to<double>(*f), static_cast<double>(v));
        EXPECT_EQ(f->template to<float>(), static_cast<float>(bd::nearest_float<double>(bd::exact_of(*f))));
        EXPECT_EQ(bd::rational(bd::to_value(f->numerator()), bd::to_value(f->denominator())), v);
        EXPECT_EQ((plain(to<long long, round_floor>(*f))), bd::floor(v));
        EXPECT_EQ((plain(to<long long, round_ceil>(*f))), bd::ceil(v));
        EXPECT_EQ((plain(to<long long, round_half_away>(*f))), bd::round(v));
        EXPECT_EQ((plain(to<long long, round_toward_zero>(*f))), bd::trunc(v));
    }
}

TEST(FastPathTest, steps_and_conversions_out_of_lattices) {
    steps_and_values<id<"[-7..14] 7/2">>();
    steps_and_values<id<"[1/2..201/2] 1">>();
    steps_and_values<id<"[1..601] 60">>();
    steps_and_values<id<"[-2..2] 1/1000">>();
    steps_and_values<id<"[0..3] 1/65536">>();
}

template <class A, class B, class Out = id<"[-1000..1000] 1/100 round_half_even">>
void divmod_and_onto() {
    for (const A a : values<A>)
        for (const B b : values<B>) {
            const bd::rational ra = bd::as_rational(a), rb = bd::as_rational(b);
            const auto [q, r]     = divmod(a, b, round_floor);
            const std::int64_t qe = bd::floor(*bd::try_div(ra, rb));
            EXPECT_EQ(bd::to_value(q), qe);
            EXPECT_EQ(bd::as_rational(r), *bd::try_sub(ra, *bd::try_mul(bd::rational{qe}, rb)));
            const std::expected<Out, error> p  = mul<Out>(a, b);
            const auto                      pe = exact_store<Out, round_half_even>(a * b);
            ASSERT_EQ(bool(p), bool(pe));
            if (p) {
                EXPECT_EQ(p->raw(), pe->raw());
            }
            const std::expected<Out, error> d  = div<Out>(a, b);
            const auto                      de = exact_store<Out, round_half_even>(a / b);
            ASSERT_EQ(bool(d), bool(de));
            if (d) {
                EXPECT_EQ(d->raw(), de->raw());
            }
        }
}

// div(a, b, mode) off the native paths: the exact quotient rounded once
// onto the multiples of a's notch, as the exact store rounds a / b.
template <class X>
const auto& value_of(const X& x) {
    if constexpr (requires { x.has_value(); })
        return *x;
    else
        return x;
}
template <class A, class B, policy_flag F>
void div_onto_dividend_notch(const char* what) {
    std::vector<B> divisors;
    if constexpr (notch_of<B> != 0)
        divisors.assign(values<B>.begin(), values<B>.end());
    else // a continuous divisor: a few values
        divisors = {B{1_q}, B{"13/12"_q}, B{"7/3"_q}, B{2.75_q}, B{4_q}};
    for (const A a : values<A>)
        for (const B b : divisors) {
            if (b == 0)
                continue;
            const auto d = div(a, b, make_policy<F>());
            using Q      = std::remove_cvref_t<decltype(value_of(d))>;
            static_assert(notch_of<Q> == notch_of<A>, "the dividend's notch");
            const auto e = exact_store<Q, F>(value_of(a / b));
            ASSERT_TRUE(e.has_value()) << what;
            EXPECT_EQ(value_of(d).raw(), e->raw()) << what << " " << text_of(a) << " / " << text_of(b);
        }
}
template <policy_flag F>
void div_modes() {
    using dec2 = id<"[-3..3] 1/100">;
    using q114 = id<"[-1..1] 1/16384">;
    using n60  = id<"[-600..600] 60">;
    using udec = id<"[1/200..401/200] 1/100">; // unanchored
    using s8   = id<"[-9..9]">;
    using nz   = id<"[1..13]">;
    using d3   = id<"[-2..2] 1/1000">;
    using cont = id<"[1..4] <12>">;
    div_onto_dividend_notch<dec2, s8, F>("dec2/s8");
    div_onto_dividend_notch<dec2, nz, F>("dec2/nz");
    div_onto_dividend_notch<q114, s8, F>("q114/s8");
    div_onto_dividend_notch<n60, d3, F>("n60/d3");
    div_onto_dividend_notch<udec, nz, F>("udec/nz");
    div_onto_dividend_notch<udec, s8, F>("udec/s8");
    div_onto_dividend_notch<dec2, cont, F>("dec2/cont");
}

// x /= b on a type that rounds, off the native paths: the exact quotient
// stored once onto x's own lattice (an unanchored one too), not rounded
// onto multiples of its notch first.
TEST(FastPathTest, a_rounding_type_divides_once_onto_its_lattice) {
    using udec = id<"[1/200..401/200] 1/100 clamp|round_half_even">;
    using nz   = id<"[1..13]">;
    for (const udec a : values<udec>)
        for (const nz b : values<nz>) {
            udec x = a;
            x /= b;
            EXPECT_EQ(x.raw(), (exact_store<udec, clamp | round_half_even>(a / b)->raw()))
                << text_of(a) << " / " << text_of(b);
        }
}

TEST(FastPathTest, div_with_a_mode_rounds_onto_the_dividend_notch) {
    div_modes<round_floor>();
    div_modes<round_ceil>();
    div_modes<round_toward_zero>();
    div_modes<round_half_away>();
    div_modes<round_half_even>();
}

TEST(FastPathTest, divmod_and_rounded_quotients_on_lattices) {
    divmod_and_onto<id<"[-3..3] 1/16">, id<"[1/4..2] 1/4">>();
    divmod_and_onto<id<"[-1..1] 1/100">, id<"[1..9]">>();
    divmod_and_onto<id<"[-70..70] 7">, id<"[1/2..3] 1/2">>();
    // an Out most products and quotients overflow: the error, as the exact store's
    using narrow = id<"[-1..1] 1/100 round_half_even">;
    divmod_and_onto<id<"[-3..3] 1/16">, id<"[1/4..2] 1/4">, narrow>();
    divmod_and_onto<id<"[-70..70] 7">, id<"[1/2..3] 1/2">, narrow>();
}

TEST(FastPathTest, exact_cast_and_the_predicates_match_the_exact_store) {
    using n72 = id<"[-7..14] 7/2">;
    using q88 = id<"[0..255] 1/256">;
    for (std::int64_t n = -300; n <= 300; ++n) {
        const auto e = exact_store<n72, checked>(n);
        const auto c = exact_cast<n72>(n);
        ASSERT_EQ(bool(c), bool(e)) << n;
        if (!c) {
            EXPECT_EQ(c.error(), e.error()) << n;
        }
        EXPECT_EQ(conversion_overflows<n72>(n), !e && e.error() == error::overflow) << n;
        EXPECT_EQ(conversion_rounds<n72>(n), !e && e.error() == error::rounding_error) << n;
        EXPECT_EQ(conversion_overflows<q88>(n), n < 0 || n > 255) << n;
        EXPECT_FALSE(conversion_rounds<q88>(n)) << n;
    }
}
} // namespace fast_path_tests
