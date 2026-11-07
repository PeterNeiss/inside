// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_MATH_INTEGER_HPP
#define BEMAN_INSIDE_MATH_INTEGER_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <iterator>
#include <numeric>

//---------------------------------------------------------------------------
// Counting and number theory on integer grids — exact, no rounding mode:
//   factorial(n)              n!
//   binomial(n, k)            C(n, k): 0 for k > n
//   falling_factorial(n, k)   n·(n−1)···(n−k+1), the k-permutations: 0 for k > n
//   rising_factorial(n, k)    n·(n+1)···(n+k−1): 1 for k = 0
//   multinomial(k1, …, km)    (k1 + … + km)! / (k1!···km!)
//   gcd(a, b), lcm(a, b)      as std::gcd and std::lcm: ≥ 0, gcd(0, 0) = 0,
//                             lcm(0, b) = 0
// Each result type holds exactly the values its inputs can produce, worked
// out at compile time: factorial of inside<{0, 20}> is inside<{1, 20!}>, of
// inside<{0, 100}> a wide raw. fn_into<Out>(…) stores the result into Out by
// Out's policy. A count (n, k) is an inside on the integers from 0, of any
// size; gcd and lcm take integer grids of any sign and size. A result whose
// limit would pass max_count_bits has no type: its grid number is computed
// while compiling.
//---------------------------------------------------------------------------
namespace beman::inside::math {
namespace detail {
using namespace beman::inside::detail;

// The bits a counting result's limit may have (16384: 1754!, C(16000, 8000)).
inline constexpr int max_count_bits = 16384;

// A count: an inside on the integers from 0, of any size.
template <class N>
concept count = insidable<N> && integer_lattice<N> && lower_of<N> >= 0;

template <insidable N>
inline constexpr grid_int count_lower = grid_numerator(lower_of<N>);
template <insidable N>
inline constexpr grid_int count_upper = grid_numerator(upper_of<N>);
// A count's values at run time: a builtin up to 64 bits, else a wide integer.
template <int Bits>
using count_work_t = std::conditional_t<(Bits <= 64), std::uint64_t, wide_uint<limbs_for_bits(Bits)>>;
template <insidable N>
using count_t = count_work_t<bit_width_of(count_upper<N>)>;

// The value of an integer-lattice inside as the integer type W (a builtin
// or a wide_int wide enough for it).
template <class W, insidable X>
constexpr W integer_value(const X& x) {
    if constexpr (values_fit_int64<X>)
        return static_cast<W>(to_value(x));
    else {
        const auto e = exact_of(x); // an integer: the denominator divides
        return static_cast<W>(e.Num / e.Den);
    }
}

template <insidable N>
constexpr count_t<N> count_value(const N& n) {
    if constexpr (values_fit_int64<N>)
        return static_cast<std::uint64_t>(to_value(n));
    else
        return static_cast<count_t<N>>(integer_value<wide_sint<limbs_for_bits(bit_width_of(count_upper<N>) + 1)>>(n));
}

//---------------------------------------------------------------------------
// Compile time: the counts of the result limits, as magnitudes. Each product
// stops once it passes max_count_bits (Fits false), so a grid far too large
// fails fast instead of running the compiler out of time.
//---------------------------------------------------------------------------
struct counted {
    big::mag Value{std::uint64_t{1}};
    bool     Fits = true;
};

constexpr void check_fits(counted& c) {
    if (big::bit_width(c.Value) > max_count_bits)
        c.Fits = false;
}
// The counts are magnitudes of any size; a one-limb factor takes the short
// multiply.
constexpr void times(counted& c, const big::mag& q) {
    if (q.empty() || c.Value.empty()) {
        c.Value.clear();
        return;
    }
    if (q.size() == 1) {
        if (const std::uint64_t carry = limb::mul_1(c.Value.data(), c.Value.data(), c.Value.size(), q[0]))
            c.Value.push_back(carry);
    } else
        c.Value = big::mul(c.Value, q);
    check_fits(c);
}
constexpr void divided_by(counted& c, std::uint64_t q) {
    limb::div_1(c.Value.data(), c.Value.data(), c.Value.size(), q);
    big::trim(c.Value);
}
constexpr big::mag mag_of(std::uint64_t v) { return v == 0 ? big::mag{} : big::mag{v}; }
constexpr big::mag plus(const big::mag& a, std::uint64_t b) { return big::add(a, mag_of(b)); }
constexpr bool     less(const big::mag& a, const big::mag& b) { return big::compare(a, b) < 0; }

// lo · (lo + 1) ··· (lo + count − 1), 1 for count 0 (lo ≥ 1). It stops once
// past max_count_bits, after at most that many factors.
constexpr counted product(const big::mag& lo, const big::mag& count) {
    counted c;
    for (std::uint64_t i = 0; less(mag_of(i), count) && c.Fits; ++i)
        times(c, plus(lo, i));
    return c;
}
constexpr counted factorial_count(const big::mag& n) {
    return less(n, big::mag{2}) ? counted{} : product(big::mag{2}, big::sub(n, big::mag{1}));
}
constexpr counted falling_count(const big::mag& n, const big::mag& k) {
    if (less(n, k))
        return counted{big::mag{}, true};
    return product(plus(big::sub(n, k), 1), k);
}
constexpr counted rising_count(const big::mag& n, const big::mag& k) {
    if (k.empty())
        return counted{};
    if (n.empty())
        return counted{big::mag{}, true};
    return product(n, k);
}
// r · C(s, k), by the multiplicative rule over t = min(k, s − k) steps: each
// intermediate r·C(s − t + i − 1, i − 1)·(s − t + i) is a multiple of i. Past
// max_count_bits it stops: the intermediates grow toward C(s, t).
constexpr void times_binomial(counted& r, const big::mag& s, const big::mag& k) {
    const big::mag rest = big::sub(s, k), t = less(rest, k) ? rest : k, base = big::sub(s, t);
    for (std::uint64_t i = 1; !less(t, mag_of(i)) && r.Fits; ++i) {
        times(r, plus(base, i));
        divided_by(r, i);
    }
}
constexpr counted binomial_count(const big::mag& n, const big::mag& k) {
    if (less(n, k))
        return counted{big::mag{}, true};
    counted c;
    times_binomial(c, n, k);
    return c;
}
template <std::size_t M>
constexpr counted multinomial_count(const std::array<big::mag, M>& ks) {
    counted  r;
    big::mag s;
    for (const big::mag& k : ks) {
        s = big::add(s, k);
        times_binomial(r, s, k);
    }
    return r;
}

// The integer grid [lo, hi] (a point when they are equal); Fits false when
// hi passes max_count_bits.
struct count_grid {
    grid Grid{grid_rational{0}};
    bool Fits = true;
};
consteval count_grid make_count_grid(const counted& lo, const counted& hi) {
    if (!lo.Fits || !hi.Fits)
        return {grid{grid_rational{0}}, false};
    const grid_rational l{big_int::from_mag(lo.Value, false)};
    const grid_rational h{big_int::from_mag(hi.Value, false)};
    if (l == h)
        return {grid{l}, true};
    return {grid{interval{l, h}, grid_rational{1}}, true};
}

// factorial is increasing in n.
template <count N>
inline constexpr count_grid factorial_grid =
    make_count_grid(factorial_count(count_lower<N>.magnitude()), factorial_count(count_upper<N>.magnitude()));

// falling_factorial is increasing in n and, while k ≤ n, in k; the corner
// (lowest n, highest k) is 0 when k can pass n there.
// (consteval functions, not immediately invoked lambdas: such a lambda
// becomes immediate, and GCC 16 then takes the address of its invoker.)
consteval count_grid falling_grid_of(const grid_int& a, const grid_int& b, const grid_int& c, const grid_int& d) {
    const counted hi = b < c ? counted{big::mag{}, true} : falling_count(b.magnitude(), std::min(d, b).magnitude());
    const counted lo = a < d ? counted{big::mag{}, true} : falling_count(a.magnitude(), c.magnitude());
    return make_count_grid(lo, hi);
}
template <count N, count K>
inline constexpr count_grid falling_grid =
    falling_grid_of(count_lower<N>, count_upper<N>, count_lower<K>, count_upper<K>);

// rising_factorial is increasing in both for n ≥ 1; rising(0, k ≥ 1) is 0.
consteval count_grid rising_grid_of(const grid_int& a, const grid_int& b, const grid_int& c, const grid_int& d) {
    const grid_int zero{0};
    const counted  lo = a == zero && zero < d ? counted{big::mag{}, true} : rising_count(a.magnitude(), c.magnitude());
    const counted  hi = zero < b    ? rising_count(b.magnitude(), d.magnitude())
                        : c == zero ? counted{}
                                    : counted{big::mag{}, true};
    return make_count_grid(lo, hi);
}
template <count N, count K>
inline constexpr count_grid rising_grid =
    rising_grid_of(count_lower<N>, count_upper<N>, count_lower<K>, count_upper<K>);

// binomial is increasing in n for k ≤ n, and in k unimodal with its peak at
// n/2: the largest at the highest n and the k nearest it, the smallest at
// the lowest n and an end of k's range (0 when k can pass n).
consteval count_grid binomial_grid_of(const grid_int& a, const grid_int& b, const grid_int& c, const grid_int& d) {
    counted hi{big::mag{}, true};
    if (c <= b)
        hi = binomial_count(b.magnitude(), std::clamp(b / grid_int{2}, c, std::min(d, b)).magnitude());
    counted lo{big::mag{}, true};
    if (d <= a) {
        const counted x = binomial_count(a.magnitude(), c.magnitude()),
                      y = binomial_count(a.magnitude(), d.magnitude());
        lo              = big::compare(x.Value, y.Value) <= 0 ? x : y;
    }
    return make_count_grid(lo, hi);
}
template <count N, count K>
inline constexpr count_grid binomial_grid =
    binomial_grid_of(count_lower<N>, count_upper<N>, count_lower<K>, count_upper<K>);

// multinomial is increasing in each count.
template <count... Ks>
inline constexpr count_grid multinomial_grid =
    make_count_grid(multinomial_count(std::array<big::mag, sizeof...(Ks)>{count_lower<Ks>.magnitude()...}),
                    multinomial_count(std::array<big::mag, sizeof...(Ks)>{count_upper<Ks>.magnitude()...}));
// The sum of the counts' limits: the run-time sum's width.
template <count... Ks>
inline constexpr int sum_bits = bit_width_of((count_upper<Ks> + ...));

//---------------------------------------------------------------------------
// gcd and lcm: the result lattices. Every value of X is a multiple of its
// unit (gcd of Lower and Notch, |Lower| for a point), so every gcd is a
// multiple of g = gcd(u_a, u_b) and every lcm of l = lcm(u_a, u_b).
//---------------------------------------------------------------------------
template <insidable X>
consteval grid_rational unit_of_values() {
    if (notch_of<X> == 0)
        return abs(lower_of<X>);
    return grid_of<X>.value_unit();
}
template <insidable X>
inline constexpr bool holds_zero = grid_of<X>.representable(grid_rational{0});
template <insidable X>
consteval grid_rational largest_magnitude() {
    return abs(lower_of<X>) < abs(upper_of<X>) ? abs(upper_of<X>) : abs(lower_of<X>);
}
// The least |v| over X's values.
template <insidable X>
consteval grid_rational least_magnitude() {
    const grid_rational lo = lower_of<X>, hi = upper_of<X>;
    if (lo >= 0)
        return lo;
    if (hi <= 0)
        return -hi;
    const grid_rational n  = notch_of<X>;
    const grid_rational up = lo + grid_round<round_mode::ceil>(-lo / n) * n; // the first value ≥ 0
    const grid_rational dn = up - n;                                         // the last ≤ 0
    return up < -dn ? up : -dn;
}
consteval grid lattice_grid(const grid_rational& lo, const grid_rational& hi, const grid_rational& unit) {
    if (lo == hi)
        return grid{lo};
    return grid{interval{lo, hi}, unit};
}
template <insidable A, insidable B>
consteval grid gcd_grid_of() {
    const grid_rational g = gcd(unit_of_values<A>(), unit_of_values<B>());
    if (point_grid<A> && point_grid<B>) // the gcd itself
        return grid{g};
    if (g == 0) // both the point 0
        return grid{grid_rational{0}};
    const grid_rational ma = largest_magnitude<A>(), mb = largest_magnitude<B>();
    grid_rational       hi = ma < mb ? ma : mb;
    if (holds_zero<A> && hi < mb)
        hi = mb;
    if (holds_zero<B> && hi < ma)
        hi = ma;
    hi                     = grid_round<round_mode::floor>(hi / g) * g;
    const grid_rational lo = holds_zero<A> && holds_zero<B> ? grid_rational{0} : g;
    return lattice_grid(lo, hi, g);
}
template <insidable A, insidable B>
inline constexpr grid gcd_grid = gcd_grid_of<A, B>();
// A lattice (a notch) or a point: what gcd and lcm take. Every value is a
// whole number of gcd_unit, the gcd of the two value units (1 or more on
// integer grids, a fraction on fractional ones).
template <insidable X>
inline constexpr bool has_lattice = notched<X> || point_grid<X>;
template <insidable A, insidable B>
inline constexpr grid_rational gcd_unit = gcd(unit_of_values<A>(), unit_of_values<B>());
template <insidable A, insidable B>
inline constexpr bool integer_operands = integer_lattice<A> && integer_lattice<B>;

// lcm(x, y) = |x·y| / gcd(x, y), and gcd(x, y) is a positive multiple of
// gcd_unit: at most |x·y| on integers, |x·y| / gcd_unit on fractions.
template <insidable A, insidable B>
inline constexpr grid_rational lcm_upper =
    integer_operands<A, B> ? largest_magnitude<A>() * largest_magnitude<B>()
                           : largest_magnitude<A>() * largest_magnitude<B>() / gcd_unit<A, B>;
template <insidable A, insidable B>
consteval grid lcm_grid_of() {
    const grid_rational ua = unit_of_values<A>(), ub = unit_of_values<B>();
    if (ua == 0 || ub == 0) // a point 0: every lcm is 0
        return grid{grid_rational{0}};
    const grid_rational l = ua / gcd(ua, ub) * ub;
    if (point_grid<A> && point_grid<B>) // the lcm itself
        return grid{l};
    const grid_rational hi = lcm_upper<A, B>;
    if (holds_zero<A> || holds_zero<B>)
        return lattice_grid(grid_rational{0}, hi, l);
    const grid_rational la = least_magnitude<A>(), lb = least_magnitude<B>();
    const grid_rational lo = grid_round<round_mode::ceil>((la < lb ? lb : la) / l) * l;
    return lattice_grid(lo, hi, l);
}
template <insidable A, insidable B>
inline constexpr grid lcm_grid = lcm_grid_of<A, B>();
template <insidable A, insidable B>
inline constexpr bool lcm_fits = bit_width_of(grid_numerator(lcm_upper<A, B>)) <= max_count_bits;

//---------------------------------------------------------------------------
// Run time: the counts in a work integer W, sized at compile time from the
// result's limit (plus a factor's bits where a step multiplies first). A
// count C (count_t) may be wide; the number of factors never is: a result
// within max_count_bits has at most that many factors of 2 or more.
//---------------------------------------------------------------------------
template <class W, class Q>
constexpr void times(W& w, const Q& q) noexcept {
    if constexpr (wide_integer<W> && std::integral<Q>)
        limb::mul_1(w.Word, w.Word, std::size(w.Word), q);
    else if constexpr (wide_integer<W>)
        w = w * static_cast<W>(q);
    else
        w *= static_cast<std::uint64_t>(q); // a factor of a 64-bit result fits 64 bits
}
template <class W>
constexpr void divided_by(W& w, std::uint64_t q) noexcept {
    if constexpr (wide_integer<W>)
        limb::div_1(w.Word, w.Word, std::size(w.Word), q);
    else
        w /= q;
}
template <class W, class C>
constexpr W product(const C& lo, std::uint64_t count) noexcept {
    W w{1};
    for (std::uint64_t i = 0; i < count; ++i)
        times(w, lo + static_cast<C>(i));
    return w;
}
template <class W, class C>
constexpr void times_binomial(W& r, const C& s, const C& k) noexcept {
    const std::uint64_t t = static_cast<std::uint64_t>(std::min(k, s - k));
    for (std::uint64_t i = 1; i <= t; ++i) {
        times(r, s - static_cast<C>(t) + static_cast<C>(i));
        divided_by(r, i);
    }
}

// The inside of R (an integer lattice) holding the integer v, which the
// caller proves lies in R. (Every caller returns a point result itself.)
template <insidable R, class W>
constexpr R lattice_inside(const W& v) noexcept {
    static_assert(!point_storage<R>);
    if constexpr (notch_of<R> == 1)
        return integer_inside<R>(v);
    else if constexpr (wide_integer<W>) {
        constexpr W lo = static_cast<W>(grid_numerator(lower_of<R>));
        constexpr W n  = static_cast<W>(grid_numerator(notch_of<R>));
        return R::from_raw(raw_cast<R>(W::divmod(v - lo, n).Quotient));
    } else
        return R::from_raw(raw_cast<R>((static_cast<std::uint64_t>(v) - static_cast<std::uint64_t>(lower_int64<R>)) /
                                       static_cast<std::uint64_t>(notch64<R>.Num)));
}

template <insidable R>
inline constexpr int result_bits = bit_width_of(grid_numerator(upper_of<R>));

// i! for i ≤ Hi, when Hi! fits 64 bits: one load.
template <std::uint64_t Hi>
inline constexpr auto factorial_table = [] {
    std::array<std::uint64_t, Hi + 1> t{};
    std::uint64_t                     f = 1;
    for (std::uint64_t i = 0; i <= Hi; ++i) {
        f *= i < 2 ? 1 : i;
        t[i] = f;
    }
    return t;
}();

#define BEMAN_INSIDE_WHY_count(fn) \
    "beman::inside::math::" #fn ": the counts are insides on the integers from 0 (inside<{0, 20}>)"
#define BEMAN_INSIDE_WHY_too_large(fn)                                                                            \
    "beman::inside::math::" #fn ": the result would pass 16384 bits (math::detail::max_count_bits) - narrow the " \
    "input grids"
} // namespace detail

//---------------------------------------------------------------------------
// factorial
//---------------------------------------------------------------------------
template <insidable N>
    requires detail::count<N> && (detail::factorial_grid<N>.Fits)
[[nodiscard]] constexpr auto factorial(N n) noexcept {
    using R = inside<detail::factorial_grid<N>.Grid>;
    if constexpr (detail::point_grid<R>) {
        (void)n;
        return R::from_raw({});
    } else {
        // A result within max_count_bits has n ≤ 1754: a builtin count.
        const std::uint64_t v = static_cast<std::uint64_t>(detail::count_value(n));
        if constexpr (detail::result_bits<R> <= 64)
            return detail::lattice_inside<R>(
                detail::factorial_table<static_cast<std::uint64_t>(detail::count_upper<N>)>[v]);
        else {
            using W = detail::count_work_t<detail::result_bits<R>>;
            return detail::lattice_inside<R>(v < 2 ? W{1} : detail::product<W>(std::uint64_t{2}, v - 1));
        }
    }
}
template <insidable N>
    requires(!detail::count<N>)
auto factorial(N) = delete (BEMAN_INSIDE_WHY_count(factorial));
template <insidable N>
    requires detail::count<N> && (!detail::factorial_grid<N>.Fits)
auto factorial(N) = delete (BEMAN_INSIDE_WHY_too_large(factorial));

template <insidable Out, insidable N>
    requires requires(N n) { factorial(n); }
[[nodiscard]] constexpr Out factorial_into(N n) {
    return Out{factorial(n)};
}

//---------------------------------------------------------------------------
// falling_factorial, rising_factorial
//---------------------------------------------------------------------------
template <insidable N, insidable K>
    requires detail::count<N> && detail::count<K> && (detail::falling_grid<N, K>.Fits)
[[nodiscard]] constexpr auto falling_factorial(N n, K k) noexcept {
    using R = inside<detail::falling_grid<N, K>.Grid>;
    if constexpr (detail::point_grid<R>) {
        (void)n, (void)k;
        return R::from_raw({});
    } else {
        using W = detail::count_work_t<detail::result_bits<R>>;
        using C =
            detail::count_work_t<std::max(bit_width_of(detail::count_upper<N>), bit_width_of(detail::count_upper<K>))>;
        const C a{detail::count_value(n)}, b{detail::count_value(k)};
        // b ≤ a: a nonzero result within max_count_bits has few factors.
        return detail::lattice_inside<R>(b > a ? W{0}
                                               : detail::product<W>(a - b + C{1}, static_cast<std::uint64_t>(b)));
    }
}
template <insidable N, insidable K>
    requires(!detail::count<N> || !detail::count<K>)
auto falling_factorial(N, K) = delete (BEMAN_INSIDE_WHY_count(falling_factorial));
template <insidable N, insidable K>
    requires detail::count<N> && detail::count<K> && (!detail::falling_grid<N, K>.Fits)
auto falling_factorial(N, K) = delete (BEMAN_INSIDE_WHY_too_large(falling_factorial));

template <insidable Out, insidable N, insidable K>
    requires requires(N n, K k) { falling_factorial(n, k); }
[[nodiscard]] constexpr Out falling_factorial_into(N n, K k) {
    return Out{falling_factorial(n, k)};
}

template <insidable N, insidable K>
    requires detail::count<N> && detail::count<K> && (detail::rising_grid<N, K>.Fits)
[[nodiscard]] constexpr auto rising_factorial(N n, K k) noexcept {
    using R = inside<detail::rising_grid<N, K>.Grid>;
    if constexpr (detail::point_grid<R>) {
        (void)n, (void)k;
        return R::from_raw({});
    } else {
        using W = detail::count_work_t<detail::result_bits<R>>;
        using C = detail::count_t<N>;
        const C    a{detail::count_value(n)};
        const auto b = detail::count_value(k);
        // a ≥ 1 and b ≥ 1: a result within max_count_bits has few factors.
        return detail::lattice_inside<R>(b == decltype(b){0} ? W{1}
                                         : a == C{0}         ? W{0}
                                                             : detail::product<W>(a, static_cast<std::uint64_t>(b)));
    }
}
template <insidable N, insidable K>
    requires(!detail::count<N> || !detail::count<K>)
auto rising_factorial(N, K) = delete (BEMAN_INSIDE_WHY_count(rising_factorial));
template <insidable N, insidable K>
    requires detail::count<N> && detail::count<K> && (!detail::rising_grid<N, K>.Fits)
auto rising_factorial(N, K) = delete (BEMAN_INSIDE_WHY_too_large(rising_factorial));

template <insidable Out, insidable N, insidable K>
    requires requires(N n, K k) { rising_factorial(n, k); }
[[nodiscard]] constexpr Out rising_factorial_into(N n, K k) {
    return Out{rising_factorial(n, k)};
}

//---------------------------------------------------------------------------
// binomial, multinomial
//---------------------------------------------------------------------------
template <insidable N, insidable K>
    requires detail::count<N> && detail::count<K> && (detail::binomial_grid<N, K>.Fits)
[[nodiscard]] constexpr auto binomial(N n, K k) noexcept {
    using R = inside<detail::binomial_grid<N, K>.Grid>;
    if constexpr (detail::point_grid<R>) {
        (void)n, (void)k;
        return R::from_raw({});
    } else {
        // c·(n − t + i) before the division by i: the limit times a factor.
        using W = detail::count_work_t<detail::result_bits<R> + bit_width_of(detail::count_upper<N>)>;
        using C =
            detail::count_work_t<std::max(bit_width_of(detail::count_upper<N>), bit_width_of(detail::count_upper<K>))>;
        const C a{detail::count_value(n)}, b{detail::count_value(k)};
        W       c{static_cast<std::uint64_t>(b > a ? 0 : 1)};
        if (b <= a)
            detail::times_binomial(c, a, b);
        if constexpr (detail::wide_integer<W> && detail::result_bits<R> <= 64)
            return detail::lattice_inside<R>(static_cast<std::uint64_t>(c));
        else
            return detail::lattice_inside<R>(c);
    }
}
template <insidable N, insidable K>
    requires(!detail::count<N> || !detail::count<K>)
auto binomial(N, K) = delete (BEMAN_INSIDE_WHY_count(binomial));
template <insidable N, insidable K>
    requires detail::count<N> && detail::count<K> && (!detail::binomial_grid<N, K>.Fits)
auto binomial(N, K) = delete (BEMAN_INSIDE_WHY_too_large(binomial));

template <insidable Out, insidable N, insidable K>
    requires requires(N n, K k) { binomial(n, k); }
[[nodiscard]] constexpr Out binomial_into(N n, K k) {
    return Out{binomial(n, k)};
}

template <insidable... Ks>
    requires(sizeof...(Ks) >= 1) && (detail::count<Ks> && ...) && (detail::multinomial_grid<Ks...>.Fits)
[[nodiscard]] constexpr auto multinomial(Ks... ks) noexcept {
    using R = inside<detail::multinomial_grid<Ks...>.Grid>;
    if constexpr (detail::point_grid<R>) {
        ((void)ks, ...);
        return R::from_raw({});
    } else {
        // r·(s − t + i) before the division by i: the limit times a partial sum.
        using W = detail::count_work_t<detail::result_bits<R> + detail::sum_bits<Ks...>>;
        using C = detail::count_work_t<detail::sum_bits<Ks...>>;
        W r{1};
        C s{0};
        for (const C k : {static_cast<C>(detail::count_value(ks))...}) {
            s = s + k;
            detail::times_binomial(r, s, k);
        }
        if constexpr (detail::wide_integer<W> && detail::result_bits<R> <= 64)
            return detail::lattice_inside<R>(static_cast<std::uint64_t>(r));
        else
            return detail::lattice_inside<R>(r);
    }
}
template <insidable... Ks>
    requires(sizeof...(Ks) >= 1) && (!(detail::count<Ks> && ...))
auto multinomial(Ks...) = delete (BEMAN_INSIDE_WHY_count(multinomial));
template <insidable... Ks>
    requires(sizeof...(Ks) >= 1) && (detail::count<Ks> && ...) && (!detail::multinomial_grid<Ks...>.Fits)
auto multinomial(Ks...) = delete (BEMAN_INSIDE_WHY_too_large(multinomial));

template <insidable Out, insidable... Ks>
    requires requires(Ks... ks) { multinomial(ks...); }
[[nodiscard]] constexpr Out multinomial_into(Ks... ks) {
    return Out{multinomial(ks...)};
}

//---------------------------------------------------------------------------
// gcd, lcm
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_WHY_integers(fn) \
    "beman::inside::math::" #fn ": the operands need a lattice (a notch or a point); a continuous grid has none"

namespace detail {
// For fractional lattices (gcd and lcm compute integers directly): x's
// value in units of gcd_unit, as a magnitude in an unsigned type that holds
// both operands', and the R of n such units.
template <insidable A, insidable B, int Bits, class X>
constexpr auto magnitude_in_units(const X& x) {
    using S   = int_for_bits_t<Bits, true>;
    const S v = value_in_units<S, gcd_unit<A, B>>(x);
    return static_cast<count_work_t<Bits>>(v < S{0} ? -v : v);
}
template <insidable R, insidable A, insidable B, class W>
constexpr R from_units(const W& n) {
    return from_value_in_units<R, gcd_unit<A, B>>(n);
}
} // namespace detail

// gcd and lcm of two lattices: of integers as std::gcd and std::lcm; of
// fractions x = a·u, y = b·u (u the gcd of the value units) u·gcd(a, b) and
// u·lcm(a, b), the largest common divisor and the least common multiple on
// the lattice uℤ.
template <insidable A, insidable B>
    requires detail::has_lattice<A> && detail::has_lattice<B>
[[nodiscard]] constexpr auto gcd(A a, B b) noexcept {
    using R = inside<detail::gcd_grid<A, B>>;
    if constexpr (detail::point_grid<R>) {
        (void)a, (void)b;
        return R::from_raw({});
    } else if constexpr (!detail::integer_operands<A, B>) {
        constexpr detail::grid_rational u = detail::gcd_unit<A, B>;
        constexpr int bits                = std::max(bit_width_of(grid_numerator(detail::largest_magnitude<A>() / u)),
                                                     bit_width_of(grid_numerator(detail::largest_magnitude<B>() / u)));
        const auto    ux = detail::magnitude_in_units<A, B, bits>(a), uy = detail::magnitude_in_units<A, B, bits>(b);
        if constexpr (detail::wide_integer<decltype(ux)>)
            return detail::from_units<R, A, B>(detail::wide_gcd(ux, uy));
        else
            return detail::from_units<R, A, B>(std::gcd(ux, uy));
    } else {
        // Magnitudes in one unsigned type that holds both.
        constexpr int bits = std::max(bit_width_of(grid_numerator(detail::largest_magnitude<A>())),
                                      bit_width_of(grid_numerator(detail::largest_magnitude<B>())));
        using S            = detail::int_for_bits_t<bits, true>;
        using U            = detail::count_work_t<bits>;
        const S x = detail::integer_value<S>(a), y = detail::integer_value<S>(b);
        const U ux = static_cast<U>(x < S{0} ? -x : x), uy = static_cast<U>(y < S{0} ? -y : y);
        if constexpr (detail::wide_integer<U>)
            return detail::lattice_inside<R>(detail::wide_gcd(ux, uy));
        else
            return detail::lattice_inside<R>(std::gcd(ux, uy));
    }
}
template <insidable A, insidable B>
    requires(!detail::has_lattice<A> || !detail::has_lattice<B>)
auto gcd(A, B) = delete (BEMAN_INSIDE_WHY_integers(gcd));

template <insidable Out, insidable A, insidable B>
    requires requires(A a, B b) { gcd(a, b); }
[[nodiscard]] constexpr Out gcd_into(A a, B b) {
    return Out{gcd(a, b)};
}

template <insidable A, insidable B>
    requires detail::has_lattice<A> && detail::has_lattice<B> && detail::lcm_fits<A, B>
[[nodiscard]] constexpr auto lcm(A a, B b) noexcept {
    using R = inside<detail::lcm_grid<A, B>>;
    if constexpr (detail::point_grid<R>) {
        (void)a, (void)b;
        return R::from_raw({});
    } else if constexpr (!detail::integer_operands<A, B>) {
        // |a| / gcd · |b| in units of u, in a type that holds the limit's units.
        constexpr int bits = bit_width_of(grid_numerator(detail::lcm_upper<A, B> / detail::gcd_unit<A, B>));
        const auto    ux = detail::magnitude_in_units<A, B, bits>(a), uy = detail::magnitude_in_units<A, B, bits>(b);
        using U = decltype(ux);
        if (ux == U{0} || uy == U{0})
            return detail::from_units<R, A, B>(U{0});
        if constexpr (detail::wide_integer<U>)
            return detail::from_units<R, A, B>(ux / detail::wide_gcd(ux, uy) * uy);
        else
            return detail::from_units<R, A, B>(std::lcm(ux, uy));
    } else {
        // |a| / gcd · |b|, in a type that holds the product's limit.
        constexpr int bits = bit_width_of(grid_numerator(detail::lcm_upper<A, B>));
        using S            = detail::int_for_bits_t<bits, true>;
        using U            = detail::count_work_t<bits>;
        const S x = detail::integer_value<S>(a), y = detail::integer_value<S>(b);
        const U ux = static_cast<U>(x < S{0} ? -x : x), uy = static_cast<U>(y < S{0} ? -y : y);
        if (ux == U{0} || uy == U{0})
            return detail::lattice_inside<R>(U{0});
        if constexpr (detail::wide_integer<U>)
            return detail::lattice_inside<R>(ux / detail::wide_gcd(ux, uy) * uy);
        else
            return detail::lattice_inside<R>(std::lcm(ux, uy));
    }
}
template <insidable A, insidable B>
    requires(!detail::has_lattice<A> || !detail::has_lattice<B>)
auto lcm(A, B) = delete (BEMAN_INSIDE_WHY_integers(lcm));
template <insidable A, insidable B>
    requires detail::has_lattice<A> && detail::has_lattice<B> && (!detail::lcm_fits<A, B>)
auto lcm(A, B) = delete (BEMAN_INSIDE_WHY_too_large(lcm));

template <insidable Out, insidable A, insidable B>
    requires requires(A a, B b) { lcm(a, b); }
[[nodiscard]] constexpr Out lcm_into(A a, B b) {
    return Out{lcm(a, b)};
}

#undef BEMAN_INSIDE_WHY_count
#undef BEMAN_INSIDE_WHY_too_large
#undef BEMAN_INSIDE_WHY_integers
} // namespace beman::inside::math

#endif // BEMAN_INSIDE_MATH_INTEGER_HPP
