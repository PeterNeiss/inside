// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Unanchored grids: a lattice that does not pass through 0, such as
// {{0.5, 10.5}, 1} (0.5, 1.5, …, 10.5). Every operation is checked against an
// exact rational oracle over every slot of small grids: decoding, rounding
// stores under each mode, conversions between grids, arithmetic, comparison,
// clamp and wrap, text, ranges and the math engine.
//
// Rounding is in value space, as on every grid: toward zero is down for a
// value ≥ 0 and up below 0, a tie of `nearest` goes away from zero (up at 0
// itself), and `half_even` picks the even lattice point, counted from the
// lattice's anchor.

#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/random.hpp>
#include <beman/inside/inside/range.hpp>

#include <gtest/gtest.h>

#include "support.hpp"

using beman::inside::test::exact;

#include <cmath>
#include <random>
#include <vector>

// Whether numeric_limits<T> has epsilon() / round_error(): only where 0 is a value.
template <class T>
concept has_epsilon = requires { std::numeric_limits<T>::epsilon(); };
template <class T>
concept has_round_error = requires { std::numeric_limits<T>::round_error(); };

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
// The grids under test (all values and rounding results are exact rationals).
constexpr grid half_off{{0.5_r, 10.5_r}, 1};                    // 0.5 … 10.5
constexpr grid quarter_off{{-2.75_r, 3.25_r}, 0.5_r};           // anchor 1/4, spans 0
constexpr grid tenth_off{{-1.1_r, 0.9_r}, 0.2_r};               // decimal, anchor 1/10
constexpr grid third_off{{rational{1, 3}, rational{31, 3}}, 1}; // not dyadic
constexpr grid sym_off{{-0.75_r, 0.75_r}, 0.5_r};               // ±0.25, ±0.75
constexpr grid neg_off{{-20.5_r, -10.5_r}, 1};                  // all negative
constexpr grid byte_off{{0.5_r, 255.5_r}, 1};                   // 256 slots: a byte index

// The exact rounding of v onto g's lattice by mode m (the oracle).
rational oracle_round(rational v, const grid& g, round_mode m) {
    const exact lo{to_rational(g.Interval.Lower)}, n{to_rational(g.Notch)};
    const exact q = (exact{v} - lo) / n;
    const exact k = floor(q), f = q - k;
    const auto  c0 = static_cast<rational>(lo + k * n), c1 = static_cast<rational>(lo + k * n + n);
    if (f == exact{0})
        return c0;
    const exact half{rational{1, 2}};
    // the anchor's parity: lattice index of c0 counted from the anchor
    const auto j0 = static_cast<imax>(static_cast<rational>(k + floor(lo / n)));
    switch (m) {
    case round_mode::floor:
        return c0;
    case round_mode::ceil:
        return c1;
    case round_mode::trunc:
        return v < 0 ? c1 : c0;
    case round_mode::nearest:
        return f < half ? c0 : f > half ? c1 : (v < 0 ? c0 : c1);
    case round_mode::half_even:
        return f < half ? c0 : f > half ? c1 : ((j0 & 1) == 0 ? c0 : c1);
    }
    return c0;
}

template <policy_flag F>
constexpr round_mode mode_of = F == round_floor       ? round_mode::floor
                               : F == round_ceil      ? round_mode::ceil
                               : F == round_nearest   ? round_mode::nearest
                               : F == round_half_even ? round_mode::half_even
                                                      : round_mode::trunc;

// Every multiple of notch/8 from one notch below Lower to one above Upper.
std::vector<rational> sweep(const grid& g) {
    std::vector<rational> v;
    const exact n{to_rational(g.Notch)}, lo{to_rational(g.Interval.Lower)}, hi{to_rational(g.Interval.Upper)};
    const exact step = n / exact{8};
    for (exact x = lo - n; x <= hi + n; x = x + step)
        v.push_back(static_cast<rational>(x));
    return v;
}

template <grid G>
std::vector<rational> slots() {
    std::vector<rational> v;
    for (umax k = 0; k <= detail::max_index(G); ++k)
        v.push_back(
            static_cast<rational>(exact{to_rational(G.Interval.Lower)} + exact{k} * exact{to_rational(G.Notch)}));
    return v;
}
} // namespace

//---------------------------------------------------------------------------
// Grid algebra
//---------------------------------------------------------------------------
TEST(UnanchoredGridTest, grid_shape) {
    static_assert(!half_off.anchored());
    static_assert(grid{{0, 10}, 1}.anchored());
    static_assert(half_off.value_unit() == 0.5_r);
    static_assert(tenth_off.value_unit() == 0.1_r);
    static_assert(half_off.step_count() == grid_int{10});
    static_assert(interval{0.5_r, 10.5_r}.divides_evenly(1));
    static_assert(!interval{0.5_r, 10.25_r}.divides_evenly(1)); // the notch must divide the span

    // + keeps the gcd notch, anchored at the sum of the Lowers
    static_assert(half_off + half_off == grid{{1, 21}, 1});
    static_assert((half_off + grid{{0, 10}, 1}) == grid{{0.5_r, 20.5_r}, 1});
    // × refines the notch by the offsets: (0.5 + i)(0.5 + j) = 0.25 + (i + j)/2 + ij
    static_assert((half_off * half_off).Notch == 0.5_r);
    static_assert((half_off * grid{{0, 10}, 1}).Notch == 0.5_r);
    static_assert((half_off * grid{{0, 10}, 2}).Notch == 1);
    // a point scales the lattice
    static_assert((half_off * grid{3}) == grid{{1.5_r, 31.5_r}, 3});
    static_assert((half_off / grid{2}).value() == grid{{0.25_r, 5.25_r}, 0.5_r});
    static_assert((half_off / grid{-2}).value() == grid{{-5.25_r, -0.25_r}, 0.5_r});
    static_assert((grid{{0, 10}, 0} / grid{2}).value().Notch == 0); // continuous stays continuous
    // hull of offset lattices refines to their common lattice
    static_assert(hull(grid{{0, 2}, 1}, grid{{0.5_r, 1.5_r}, 1}).Notch == 0.5_r);
    static_assert(hull(half_off, grid{{1.5_r, 3.5_r}, 1}).Notch == 1);
}

TEST(UnanchoredGridTest, storage) {
    using half = inside<half_off>;
    using byte = inside<byte_off>;
    static_assert(index_storage<half>);
    static_assert(std::is_same_v<raw_t<byte>, std::uint8_t>);
    static_assert(sizeof(byte) == 1);
    // notch 1 but non-integer values: never value storage
    static_assert(!integer_value_storage<inside<neg_off>>);
    static_assert(!unit_lattice(half_off));
    static_assert(index_storage<inside<half_off>>);
    static_assert(!double_exact<third_off>);
}

//---------------------------------------------------------------------------
// Decode
//---------------------------------------------------------------------------
template <grid G>
void check_decode() {
    using B = inside<G>;
    for (umax k = 0; k <= detail::max_index(G); ++k) {
        const auto want =
            static_cast<rational>(exact{to_rational(G.Interval.Lower)} + exact{k} * exact{to_rational(G.Notch)});
        const B b{want};
        EXPECT_EQ(static_cast<rational>(b), want) << to_string(want);
        EXPECT_EQ(b.raw(), static_cast<raw_t<B>>(k));
        EXPECT_EQ(b.numerator(), signed_numerator(want));
        EXPECT_EQ(b.denominator(), abs_den(want.Den));
        EXPECT_EQ((b.template as<imax, round_toward_zero>()), trunc(want));
        EXPECT_EQ(as_double(b), static_cast<double>(want));
        EXPECT_TRUE(b == want);
    }
}

TEST(UnanchoredGridTest, decode) {
    check_decode<half_off>();
    check_decode<quarter_off>();
    check_decode<tenth_off>();
    check_decode<third_off>();
    check_decode<sym_off>();
    check_decode<neg_off>();
    check_decode<byte_off>();
}

//---------------------------------------------------------------------------
// Stores: a rational, a double, an integer, rounded by every mode
//---------------------------------------------------------------------------
template <grid G, policy_flag F, policy_flag S = checked>
void check_store_rational() {
    using B = inside<G, F | S>;
    for (const rational v : sweep(G)) {
        SCOPED_TRACE(::testing::Message() << "v=" << to_string(v) << " grid=" << to_string(G));
        const rational want = oracle_round(v, G, mode_of<F>);
        if (!includes(G.Interval, want)) {
            EXPECT_THROW((void)B{v}, inside_error);
            continue;
        }
        EXPECT_EQ(static_cast<rational>(B{v}), want);
        if (static_cast<double>(v) == static_cast<double>(v) && rational{static_cast<double>(v)} == v) {
            EXPECT_EQ(static_cast<rational>(B{static_cast<double>(v)}), want);
        }
        if (abs_den(v.Den) == 1) {
            EXPECT_EQ(static_cast<rational>(B{static_cast<imax>(signed_numerator(v))}), want);
        }
    }
}

template <grid G>
void check_store_all_modes() {
    check_store_rational<G, round_nearest>();
    check_store_rational<G, round_floor>();
    check_store_rational<G, round_ceil>();
    check_store_rational<G, round_half_even>();
    check_store_rational<G, round_toward_zero>();
}

TEST(UnanchoredGridTest, store_rounds_in_value_space) {
    check_store_all_modes<half_off>();
    check_store_all_modes<quarter_off>();
    check_store_all_modes<tenth_off>();
    check_store_all_modes<third_off>();
    check_store_all_modes<sym_off>();
    check_store_all_modes<neg_off>();
}

TEST(UnanchoredGridTest, store_rounds_dyadic_offsets) {
    check_store_rational<quarter_off, round_nearest>();
    check_store_rational<quarter_off, round_floor>();
    check_store_rational<quarter_off, round_ceil>();
    check_store_rational<quarter_off, round_half_even>();
    check_store_rational<quarter_off, round_toward_zero>();
    check_store_rational<sym_off, round_nearest>();
    check_store_rational<sym_off, round_toward_zero>();
    check_store_rational<half_off, round_half_even>();
}

TEST(UnanchoredGridTest, store_rounds_with_exact_storage) {
    check_store_rational<quarter_off, round_nearest>();
    check_store_rational<sym_off, round_toward_zero>();
    check_store_rational<third_off, round_half_even>();
}

TEST(UnanchoredGridTest, off_lattice_store_is_an_error_without_rounding) {
    using half = inside<half_off>;
    EXPECT_THROW((void)half{1}, inside_error);
    EXPECT_THROW((void)half{rational{1}}, inside_error);
    EXPECT_EQ(static_cast<rational>(half{1.5}), 1.5_r);
    EXPECT_FALSE(half::try_make(2).has_value());
    EXPECT_EQ(half::try_make(2).error(), errc::rounding_error);
}

//---------------------------------------------------------------------------
// Conversions between grids
//---------------------------------------------------------------------------
template <grid To, policy_flag F, grid From>
void check_convert() {
    using T = inside<To, F>;
    using S = inside<From>;
    for (const rational v : slots<From>()) {
        SCOPED_TRACE(::testing::Message()
                     << "v=" << to_string(v) << " " << to_string(From) << " -> " << to_string(To));
        const rational want = oracle_round(v, To, mode_of<F>);
        if (!includes(To.Interval, want))
            continue;
        EXPECT_EQ(static_cast<rational>(T{S{v}}), want);
    }
}

template <grid To, grid From>
void check_convert_all_modes() {
    check_convert<To, round_nearest, From>();
    check_convert<To, round_floor, From>();
    check_convert<To, round_ceil, From>();
    check_convert<To, round_half_even, From>();
    check_convert<To, round_toward_zero, From>();
}

TEST(UnanchoredGridTest, convert) {
    constexpr grid fine{{-4, 12}, rational{1, 8}};
    constexpr grid ints{{-3, 12}, 1};
    check_convert_all_modes<half_off, fine>();
    check_convert_all_modes<half_off, ints>();
    check_convert_all_modes<ints, half_off>();
    check_convert_all_modes<quarter_off, half_off>();
    check_convert_all_modes<half_off, quarter_off>();
    check_convert_all_modes<sym_off, tenth_off>();
    check_convert_all_modes<tenth_off, sym_off>();
    check_convert_all_modes<third_off, half_off>();
    check_convert_all_modes<half_off, third_off>();
}

TEST(UnanchoredGridTest, assignability) {
    using half    = inside<half_off>;
    using ints    = inside<{0, 10}>;
    using on_half = inside<{{1.5_r, 3.5_r}, 1}>; // on half's lattice
    using fine    = inside<{{0, 10}, 0.5_r}>;
    static_assert(inside_assignable<half, on_half>);
    static_assert(inside_assignable<half, decltype(1.5_ins)>);
    static_assert(!inside_assignable<half, decltype(1_ins)>);
    static_assert(!inside_assignable<half, ints>); // different lattice
    static_assert(!inside_assignable<half, fine>); // finer
    static_assert(inside_assignable<fine, half>);  // half's lattice lies on fine's
    static_assert(!inside_assignable<ints, half>);
    static_assert(inside_assignable<half, ints, round_nearest>);
    EXPECT_EQ(static_cast<rational>(fine{half{2.5}}), 2.5_r);
    EXPECT_EQ(static_cast<rational>(half{on_half{3.5}}), 3.5_r);
}

//---------------------------------------------------------------------------
// Arithmetic and comparison against the oracle, every pair of slots
//---------------------------------------------------------------------------
template <grid GA, grid GB>
void check_pairs() {
    using A = inside<GA>;
    using B = inside<GB>;
    for (const rational x : slots<GA>())
        for (const rational y : slots<GB>()) {
            SCOPED_TRACE(::testing::Message() << to_string(x) << " , " << to_string(y));
            const A a{x};
            const B b{y};
            EXPECT_EQ(exact{a + b}, exact{x} + exact{y});
            EXPECT_EQ(exact{a - b}, exact{x} - exact{y});
            EXPECT_EQ(exact{a * b}, exact{x} * exact{y});
            if (y != 0) {
                const auto q = a / b;
                if constexpr (is_expected_v<decltype(q)>) {
                    EXPECT_EQ(exact{*q}, exact{x} / exact{y});
                } else {
                    EXPECT_EQ(exact{q}, exact{x} / exact{y});
                }
            }
            EXPECT_EQ((a <=> b) < 0, x < y);
            EXPECT_EQ(a == b, x == y);
            EXPECT_EQ(static_cast<rational>(-a), -x);
        }
}

TEST(UnanchoredGridTest, arithmetic_and_comparison) {
    check_pairs<half_off, half_off>();
    check_pairs<half_off, grid{{-3, 4}, 1}>();
    check_pairs<grid{{-3, 4}, 1}, half_off>();
    check_pairs<quarter_off, half_off>();
    check_pairs<quarter_off, sym_off>();
    check_pairs<tenth_off, sym_off>();
    check_pairs<third_off, quarter_off>();
    check_pairs<neg_off, half_off>();
    check_pairs<sym_off, grid{{-1, 1}, rational{1, 8}}>();
}

TEST(UnanchoredGridTest, point_operands) {
    using half = inside<half_off>;
    for (const rational x : slots<half_off>()) {
        const half a{x};
        EXPECT_EQ(exact{a + 2_ins}, exact{x} + exact{2});
        EXPECT_EQ(exact{a + 0.25_ins}, exact{x} + exact{0.25_r});
        EXPECT_EQ(exact{a * 3_ins}, exact{x} * exact{3});
        EXPECT_EQ(exact{a * -0.5_ins}, exact{x} * exact{-0.5_r});
        EXPECT_EQ(exact{1_ins - a}, exact{1} - exact{x});
        EXPECT_EQ(a <=> 3, x <=> rational{3});
        EXPECT_EQ(a == 2.5_r, x == 2.5_r);
        EXPECT_EQ(a < 4.75_r, x < 4.75_r);
    }
}

TEST(UnanchoredGridTest, compound_assignment) {
    using half = inside<half_off>;
    using ints = inside<{-3, 4}>;
    for (const rational x : slots<half_off>())
        for (const rational y : slots<grid{{-3, 4}, 1}>()) {
            SCOPED_TRACE(::testing::Message() << to_string(x) << " , " << to_string(y));
            const auto s = static_cast<rational>(exact{x} + exact{y});
            half       a{x};
            if (includes(half_off.Interval, s)) {
                a += ints{y};
                EXPECT_EQ(static_cast<rational>(a), s);
            } else
                EXPECT_THROW(a += ints{y}, inside_error);
            half       b{x};
            const auto d = static_cast<rational>(exact{x} - exact{y});
            if (includes(half_off.Interval, d)) {
                b -= ints{y};
                EXPECT_EQ(static_cast<rational>(b), d);
            } else
                EXPECT_THROW(b -= ints{y}, inside_error);
        }
    // a sum on another lattice rounds back by the policy
    inside<half_off, round_floor> h{2.5};
    h += inside<half_off>{0.5};
    EXPECT_EQ(static_cast<rational>(h), 2.5_r); // 3 floors to 2.5
    inside<half_off, round_ceil> c{2.5};
    c *= inside<{2, 2}>{2};
    EXPECT_EQ(static_cast<rational>(c), 5.5_r); // 5 rounds up to 5.5
    half i{4.5};
    ++i;
    EXPECT_EQ(static_cast<rational>(i), 5.5_r);
    --i;
    --i;
    EXPECT_EQ(static_cast<rational>(i), 3.5_r);
}

//---------------------------------------------------------------------------
// clamp and wrap
//---------------------------------------------------------------------------
TEST(UnanchoredGridTest, clamp_and_wrap) {
    using clamped = inside<half_off, clamp | round_nearest>;
    using wrapped = inside<half_off, wrap | round_nearest>;
    EXPECT_EQ(static_cast<rational>(clamped{-3}), 0.5_r);
    EXPECT_EQ(static_cast<rational>(clamped{100}), 10.5_r);
    EXPECT_EQ(static_cast<rational>(clamped{0.6}), 0.5_r);
    // wrap: modulo the 11 slots of [0.5, 10.5]
    EXPECT_EQ(static_cast<rational>(wrapped{11.5}), 0.5_r);
    EXPECT_EQ(static_cast<rational>(wrapped{12.5}), 1.5_r);
    EXPECT_EQ(static_cast<rational>(wrapped{-0.5}), 10.5_r);
    EXPECT_EQ(static_cast<rational>(wrapped{12}), 1.5_r); // 12 → 12.5 (nearest, up) → 1.5
    wrapped w{10.5};
    ++w;
    EXPECT_EQ(static_cast<rational>(w), 0.5_r);
    w += inside<{-30, 30}>{-23};
    EXPECT_EQ(static_cast<rational>(w), 10.5_r); // 0.5 − 23 = −22.5 ≡ 10.5 (mod 11)
    inside<half_off, clamp> k{9.5};
    k += inside<{0, 5}>{5};
    EXPECT_EQ(static_cast<rational>(k), 10.5_r);
}

//---------------------------------------------------------------------------
// Text
//---------------------------------------------------------------------------
template <grid G>
void check_text() {
    using B = inside<G>;
    for (const rational v : slots<G>()) {
        const B           b{v};
        const std::string s = to_string(b);
        const auto        r = parse<B>(s);
        ASSERT_TRUE(r.has_value()) << s;
        EXPECT_EQ(*r, b) << s;
    }
}

TEST(UnanchoredGridTest, text) {
    check_text<half_off>();
    check_text<quarter_off>();
    check_text<tenth_off>();
    check_text<third_off>();
    // a decimal lattice prints its unit's decimals: 0.05 needs two digits
    EXPECT_EQ(to_string(inside<{{0.05_r, 0.95_r}, 0.1_r}>{0.15_r}), "0.15");
    EXPECT_EQ(to_string(inside<tenth_off>{-0.3_r}), "-0.3");
    EXPECT_EQ(to_string(inside<half_off>{2.5}), "2.5");
    EXPECT_EQ(to_string(inside<third_off>{rational{4, 3}}), "4/3");
    EXPECT_FALSE(parse<inside<half_off>>("2").has_value());
    EXPECT_EQ(static_cast<rational>(*parse<inside<half_off>, round_nearest>("2.1")), 2.5_r);
}

//---------------------------------------------------------------------------
// Ranges, limits, random
//---------------------------------------------------------------------------
TEST(UnanchoredGridTest, range_limits_random) {
    std::vector<rational> got;
    for (auto x : inside_range<quarter_off>{})
        got.push_back(static_cast<rational>(x));
    EXPECT_EQ(got, slots<quarter_off>());

    std::vector<rational> from_mid;
    for (auto x : inside_range<half_off>{inside<half_off>{8.5}})
        from_mid.push_back(static_cast<rational>(x));
    ASSERT_EQ(from_mid.size(), 11u);
    EXPECT_EQ(from_mid.front(), 8.5_r);
    EXPECT_EQ(from_mid[3], 0.5_r);

    using lim = std::numeric_limits<inside<half_off>>;
    EXPECT_EQ(static_cast<rational>(lim::min()), 0.5_r);
    EXPECT_EQ(static_cast<rational>(lim::lowest()), 0.5_r);
    EXPECT_EQ(static_cast<rational>(lim::max()), 10.5_r);
    // Lattices that span 0 but miss it: 0 is no value, so there is no epsilon.
    static_assert(!has_epsilon<inside<quarter_off>>);
    static_assert(!has_epsilon<inside<tenth_off>>);
    EXPECT_FALSE(std::numeric_limits<inside<half_off>>::is_integer);
    static_assert(!std::is_convertible_v<inside<half_off>, imax>); // no silent truncation
    static_assert(!std::is_convertible_v<inside<{0, 10}>, imax>);
    // as<imax>() rounds by the policy's mode
    EXPECT_EQ((inside<half_off, round_nearest>{2.5}.as<imax>()), 3);
    EXPECT_EQ((inside<half_off, round_floor>{2.5}.as<imax>()), 2);
    EXPECT_EQ((inside<half_off, round_toward_zero>{2.5}.as<imax>()), 2);
    EXPECT_EQ((inside<quarter_off, round_toward_zero>{-1.75}.as<imax>()), -1);
    EXPECT_EQ((inside<half_off, round_half_even>{2.5}.as<imax>()), 2);
    EXPECT_EQ((inside<half_off, round_half_even>{3.5}.as<imax>()), 4);
    const imax i = inside<{{0, 4}, per<4>}, round_ceil>{1.25}.as<imax>();
    EXPECT_EQ(i, 2);

    std::mt19937_64 rng{42};
    for (int k = 0; k < 200; ++k) {
        const rational v = static_cast<rational>(uniform<inside<tenth_off>>(rng));
        EXPECT_TRUE(inside<tenth_off>::try_make(v).has_value()) << to_string(v);
    }
}

//---------------------------------------------------------------------------
// sum<>
//---------------------------------------------------------------------------
TEST(UnanchoredGridTest, sum) {
    std::vector<inside<half_off>>    halves;
    std::vector<inside<tenth_off>>   tenths;
    std::vector<inside<quarter_off>> quarters;
    exact                            h{0}, t{0}, q{0};
    for (const rational v : slots<half_off>()) {
        halves.emplace_back(v);
        h = h + exact{v};
    }
    for (const rational v : slots<tenth_off>()) {
        tenths.emplace_back(v);
        t = t + exact{v};
    }
    for (const rational v : slots<quarter_off>()) {
        quarters.emplace_back(v);
        q = q + exact{v};
    }
    using half_total    = inside<{{0, 1000}, 0.5_r}>;
    using tenth_total   = inside<{{-100, 100}, 0.1_r}>;
    using quarter_total = inside<{{-100, 100}, 0.25_r}>;
    EXPECT_EQ(exact{sum<half_total>(halves)}, h);
    EXPECT_EQ(exact{sum<tenth_total>(tenths)}, t);
    EXPECT_EQ(exact{sum<quarter_total>(quarters)}, q);
}

//---------------------------------------------------------------------------
// Math: correctly rounded onto an unanchored output, from an unanchored
// input. The oracle rounds the double result exactly by the mode, unless the
// double lies too close to a rounding boundary of Out to decide.
//---------------------------------------------------------------------------
namespace {
template <insidable Out>
void expect_rounded(rational got, double value, round_mode m) {
    constexpr grid g = grid_of<Out>;
    const double   pos =
        2 * (value - static_cast<double>(to_rational(g.Interval.Lower))) / static_cast<double>(to_rational(g.Notch));
    const double f = pos - std::floor(pos);
    if (f < 1e-7 || f > 1 - 1e-7)
        return; // a lattice point or half point: double cannot decide
    // Far from every boundary, 2^-30 precision decides the same.
    const rational v{static_cast<imax>(std::round(std::ldexp(value, 30))), imax{1} << 30};
    const rational want = oracle_round(v, g, m);
    if (!includes(g.Interval, want))
        return;
    EXPECT_EQ(got, want) << "value " << value;
}

template <policy_flag F>
void check_math_mode() {
    using in_t             = inside<{{-2.75_r, 2.25_r}, 0.5_r}, round_nearest>;
    using tan_t            = inside<{{-1.25_r, 1.25_r}, 0.5_r}, round_nearest>; // in_t's values on tan's branch
    using pos_t            = inside<{{0.3_r, 9.9_r}, 0.4_r}, round_nearest>;    // 0.3, 0.7, …
    using out_t            = inside<{{-20.0625_r, 20.0625_r}, rational{1, 8}}, F>;
    using dec_t            = inside<{{-4.05_r, 4.05_r}, 0.1_r}, F>;
    constexpr round_mode m = mode_of<F>;
    for (const rational r : slots<grid_of<in_t>>()) {
        SCOPED_TRACE(to_string(r));
        const in_t   x{r};
        const double d = static_cast<double>(r);
        expect_rounded<out_t>(static_cast<rational>(math::exp_into<out_t>(x)), std::exp(d), m);
        expect_rounded<out_t>(static_cast<rational>(math::sin_into<out_t>(x)), std::sin(d), m);
        expect_rounded<dec_t>(static_cast<rational>(math::cos_into<dec_t>(x)), std::cos(d), m);
        expect_rounded<dec_t>(static_cast<rational>(math::atan_into<dec_t>(x)), std::atan(d), m);
        expect_rounded<dec_t>(static_cast<rational>(math::tanh_into<dec_t>(x)), std::tanh(d), m);
        expect_rounded<dec_t>(static_cast<rational>(math::cbrt_into<dec_t>(x)), std::cbrt(d), m);
        if (includes(grid_of<tan_t>.Interval, r)) // tan on one branch
            expect_rounded<out_t>(static_cast<rational>(math::tan_into<out_t>(tan_t{r})), std::tan(d), m);
        for (const rational s : slots<grid_of<pos_t>>()) {
            const pos_t  y{s};
            const double e = static_cast<double>(s);
            expect_rounded<out_t>(static_cast<rational>(math::atan2_into<out_t>(x, y)), std::atan2(d, e), m);
            expect_rounded<out_t>(static_cast<rational>(math::hypot_into<out_t>(x, y)), std::hypot(d, e), m);
            try {
                expect_rounded<out_t>(static_cast<rational>(math::pow_into<out_t>(y, x)), std::pow(e, d), m);
            } catch (const inside_error& err) { // past Out: its store reports overflow
                EXPECT_EQ(err.Code, errc::overflow);
                EXPECT_GT(std::pow(e, d), 20.0);
            }
        }
    }
    for (const rational s : slots<grid_of<pos_t>>()) {
        const pos_t  y{s};
        const double e = static_cast<double>(s);
        expect_rounded<dec_t>(static_cast<rational>(math::log_into<dec_t>(y)), std::log(e), m);
        expect_rounded<dec_t>(static_cast<rational>(math::sqrt_into<dec_t>(y)), std::sqrt(e), m);
    }
}
} // namespace

TEST(UnanchoredGridTest, math_rounds_correctly) {
    check_math_mode<round_nearest>();
    check_math_mode<round_floor>();
    check_math_mode<round_ceil>();
    check_math_mode<round_half_even>();
    check_math_mode<round_toward_zero>();
}

TEST(UnanchoredGridTest, math) {
    using in_t  = inside<{{-2.75_r, 2.25_r}, 0.5_r}, round_nearest>;          // −2.75 … 2.25
    using ref_t = inside<{{-2.75_r, 2.25_r}, rational{1, 4}}, round_nearest>; // anchored, same values
    for (const rational x : slots<grid_of<in_t>>()) {
        SCOPED_TRACE(to_string(x));
        const in_t  a{x};
        const ref_t r{x};
        // an unanchored input gives the same result as the same value anchored
        using fixed = inside<{{0, 17}, rational{1, 64}}, round_nearest>;
        using unit  = inside<{{-1, 1}, rational{1, 1024}}, round_nearest>;
        EXPECT_EQ(math::exp_into<fixed>(a), math::exp_into<fixed>(r));
        EXPECT_EQ(math::sin_into<unit>(a), math::sin_into<unit>(r));
        EXPECT_EQ(math::floor(a), math::floor(r));
        EXPECT_EQ(math::round(a), math::round(r));
        EXPECT_EQ(static_cast<rational>(math::abs(a)), static_cast<rational>(math::abs(r)));
        EXPECT_EQ(static_cast<rational>(math::fmod(a, inside<half_off>{1.5})),
                  static_cast<rational>(math::fmod(r, inside<half_off>{1.5})));
        EXPECT_EQ(static_cast<rational>(math::copysign(a, inside<{-1, 1}>{-1})), -abs(x));
    }
    // abs of an offset lattice: |x| for x on ±0.25 + k/2
    static_assert(grid_of<decltype(math::abs(inside<sym_off, round_nearest>{}))>.Notch == 0.5_r);
    // sqrt of an exact square lands exactly on the offset lattice
    using sq_t     = inside<{{0.5_r, 4.5_r}, 1}, round_nearest>;
    using sq_floor = inside<{{0.5_r, 4.5_r}, 1}, round_floor>;
    using quarters = inside<{{0, 64}, rational{1, 4}}>;
    using whole    = inside<{0, 64}>;
    EXPECT_EQ(static_cast<rational>(math::sqrt_into<sq_t>(quarters{6.25})), 2.5_r);
    // a tie of `nearest` on the offset lattice: sqrt(4) = 2 between 1.5 and 2.5 → 2.5
    EXPECT_EQ(static_cast<rational>(math::sqrt_into<sq_t>(whole{4})), 2.5_r);
    EXPECT_EQ(static_cast<rational>(math::sqrt_into<sq_floor>(whole{4})), 1.5_r);
    // out of range through the policy: a checked output reports
    using small = inside<{{0.5_r, 2.5_r}, 1}, round_nearest>;
    EXPECT_THROW((void)math::exp_into<small>(whole{3}), inside_error);
    using clamped = inside<{{0.5_r, 2.5_r}, 1}, round_nearest | clamp>;
    EXPECT_EQ(static_cast<rational>(math::exp_into<clamped>(whole{3})), 2.5_r);
}
