// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP
#define BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_int.hpp>

#include <compare>
#include <expected>
#include <utility>

//---------------------------------------------------------------------------
// wide_value — exact values of insides the 64-bit paths cannot hold: a wide
// index raw (more than 2^64 slots), or grid numbers past 64 bits.
//
// On a valid grid Lower/Notch is an integer m (slot_base), so a value is its
// value index J = m + raw times Notch, exactly. exact_frac<K> carries a value
// as an unreduced fraction of K-limb integers; comparisons cross-multiply, and
// a store divides by the target notch and rounds by the policy's mode.
//
// K is sized from the grids involved (exact_limbs): every exact computation is
// at most a product of two values over a third notch, so three times the
// widest value suffices. Binary operations widen to the wider operand.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
inline constexpr std::size_t exact_min_limbs = 8; // scalars, 64-bit rationals

template <std::size_t K>
struct exact_frac {
    wide_sint<K> Num;
    wide_sint<K> Den; // > 0; not reduced

    constexpr exact_frac() = default;
    constexpr exact_frac(wide_sint<K> n, wide_sint<K> d) noexcept : Num{n}, Den{d} {}
    template <std::size_t M>
        requires(M < K)
    constexpr exact_frac(const exact_frac<M>& o) noexcept : Num{o.Num}, Den{o.Den} {} // widens

    constexpr explicit operator double() const noexcept { return static_cast<double>(Num) / static_cast<double>(Den); }
};

template <std::size_t A, std::size_t B>
inline constexpr std::size_t exact_max = A > B ? A : B;

template <std::size_t A, std::size_t B>
constexpr std::strong_ordering operator<=>(const exact_frac<A>& a, const exact_frac<B>& b) noexcept {
    using F = exact_frac<exact_max<A, B>>;
    const F x{a}, y{b};
    return x.Num * y.Den <=> y.Num * x.Den;
}
template <std::size_t A, std::size_t B>
constexpr bool operator==(const exact_frac<A>& a, const exact_frac<B>& b) noexcept {
    return (a <=> b) == 0;
}
template <std::size_t A, std::size_t B>
constexpr auto operator+(const exact_frac<A>& a, const exact_frac<B>& b) noexcept {
    using F = exact_frac<exact_max<A, B>>;
    const F x{a}, y{b};
    return F{x.Num * y.Den + y.Num * x.Den, x.Den * y.Den};
}
template <std::size_t A, std::size_t B>
constexpr auto operator*(const exact_frac<A>& a, const exact_frac<B>& b) noexcept {
    using F = exact_frac<exact_max<A, B>>;
    const F x{a}, y{b};
    return F{x.Num * y.Num, x.Den * y.Den};
}
// Pre: b != 0.
template <std::size_t A, std::size_t B>
constexpr auto operator/(const exact_frac<A>& a, const exact_frac<B>& b) noexcept {
    using F = exact_frac<exact_max<A, B>>;
    const F x{a}, y{b};
    const F q{x.Num * y.Den, x.Den * y.Num};
    return q.Den.negative() ? F{-q.Num, -q.Den} : q;
}
template <std::size_t K>
constexpr exact_frac<K> operator-(const exact_frac<K>& a) noexcept {
    return {-a.Num, a.Den};
}

template <std::size_t K = exact_min_limbs>
constexpr exact_frac<K> exact_of(const rational& r) noexcept {
    return {static_cast<wide_sint<K>>(wide_numerator(r)), static_cast<wide_sint<K>>(wide_denominator(r))};
}

template <std::size_t K = exact_min_limbs, std::integral T>
constexpr exact_frac<K> exact_of(T v) noexcept {
    return {wide_sint<K>{v}, wide_sint<K>{1}};
}

// A grid number (a limit or notch, of any size) as an exact value.
template <std::size_t K>
constexpr exact_frac<K> exact_of_grid(const grid_rational& r) noexcept {
    return {static_cast<wide_sint<K>>(wide_numerator(r)), static_cast<wide_sint<K>>(wide_denominator(r))};
}

// m = Lower/Notch, the value index of slot 0 (0 for a continuous grid).
template <insidable B>
inline constexpr grid_wide slot_base = [] {
    if constexpr (notch_of<B> == 0)
        return grid_wide{0};
    else
        return wide_numerator(lower_of<B>) * wide_denominator(notch_of<B>) /
               (wide_denominator(lower_of<B>) * wide_numerator(notch_of<B>));
}();

// Bits that bound every value of B's grid: |value| < 2^grid_magnitude_bits.
template <insidable B>
inline constexpr int grid_magnitude_bits = [] {
    auto bits = [](const grid_rational& r) {
        const grid_wide n = wide_numerator(r);
        return bit_width_of(n.negative() ? -n : n) - (bit_width_of(wide_denominator(r)) - 1) + 1;
    };
    const int a = bits(lower_of<B>), b = bits(upper_of<B>);
    return a > b ? a : b;
}();

// Bits of B's values as fractions J·n/d: numerator and denominator together.
template <insidable B>
inline constexpr int exact_value_bits = [] {
    if constexpr (!exact_valued<B>)
        return 128; // a 64-bit rational
    else {
        auto bits = [](const grid_wide& v) { return bit_width_of(v.negative() ? -v : v); };
        return grid_magnitude_bits<B> + bits(wide_denominator(notch_of<B>)) + bits(wide_numerator(notch_of<B>));
    }
}();

// Limbs for exact computations on values of the given insides.
template <insidable... Bs>
inline constexpr std::size_t exact_limbs = [] {
    int bits = 0;
    ((bits = exact_value_bits<Bs> > bits ? exact_value_bits<Bs> : bits), ...);
    const std::size_t k = limbs_for_bits(3 * bits + 8);
    return k > exact_min_limbs ? k : exact_min_limbs;
}();

template <insidable B>
constexpr auto exact_of(const B& b) {
    constexpr std::size_t K = exact_limbs<B>;
    using I                 = wide_sint<K>;
    if constexpr (exact_valued<B>) {
        const I j = static_cast<I>(slot_base<B>) + I{b.raw()};
        return exact_frac<K>{j * static_cast<I>(wide_numerator(notch_of<B>)),
                             static_cast<I>(wide_denominator(notch_of<B>))};
    } else
        return exact_of<K>(as_rational(b));
}

// A double whose magnitude is at least 2^64 (so an integer), as an exact
// value for a store into L. Past L's magnitude only its side matters, so a
// value just beyond the grid stands in.
template <insidable L>
constexpr exact_frac<exact_limbs<L>> exact_of_large(double d) noexcept {
    using I           = wide_sint<exact_limbs<L>>;
    constexpr int k   = grid_magnitude_bits<L> > 64 ? grid_magnitude_bits<L> : 64;
    const bool    neg = d < 0;
    const double  m   = neg ? -d : d;
    if (!(m < ldexp(1.0, k)))
        return {neg ? -(I{1} << (k + 1)) : I{1} << (k + 1), I{1}};
    int          e = 0;
    const double f = frexp(m, &e);                                   // m = f·2^e, f in [0.5, 1)
    const I      v = I{static_cast<umax>(ldexp(f, 53))} << (e - 53); // e ≥ 65
    return {neg ? -v : v, I{1}};
}

// Truncation toward zero, as an integer.
template <std::size_t K>
constexpr wide_sint<K> trunc(const exact_frac<K>& f) noexcept {
    return f.Num / f.Den;
}

// The reduced value as a 64-bit rational, or overflow when it does not fit.
template <std::size_t K>
constexpr std::expected<rational, errc> try_rational(const exact_frac<K>& f) noexcept {
    using I        = wide_sint<K>;
    const bool neg = f.Num.negative();
    I          a = neg ? -f.Num : f.Num, b = f.Den;
    I          x = a, y = b;
    while (!y.is_zero()) {
        const I t = x % y;
        x         = y;
        y         = t;
    }
    if (!x.is_zero()) {
        a /= x;
        b /= x;
    }
    if (a > I{std::numeric_limits<umax>::max()} || b > I{std::numeric_limits<imax>::max()})
        return std::unexpected{errc::overflow};
    const imax den = static_cast<imax>(b);
    return rational{static_cast<umax>(a), neg ? -den : den};
}

// Text → exact value, for wide grids whose values outgrow the 64-bit
// rational parse (from_chars falls back here on errc::overflow). Accepts
// [+-]digits[.digits][e[+-]digits] and two of those joined by '/'.
// Anything K limbs cannot hold reports overflow.
template <std::size_t K>
constexpr std::expected<exact_frac<K>, errc> parse_exact(const char* first, const char* last) noexcept {
    using I = wide_sint<K>;
    constexpr I ten{10};
    constexpr I limit = std::numeric_limits<I>::max() / I{100};
    auto        one   = [&](const char* f, const char* l) -> std::expected<exact_frac<K>, errc> {
        bool neg = false;
        if (f != l && (*f == '+' || *f == '-')) {
            neg = (*f == '-');
            ++f;
        }
        I    num{0}, den{1};
        bool digits = false, point = false;
        for (; f != l && ((*f >= '0' && *f <= '9') || (*f == '.' && !point)); ++f) {
            if (*f == '.') {
                point = true;
                continue;
            }
            if (num > limit || den > limit)
                return std::unexpected{errc::overflow};
            num = num * ten + I{*f - '0'};
            if (point)
                den = den * ten;
            digits = true;
        }
        if (!digits)
            return std::unexpected{errc::invalid_format};
        if (f != l && (*f == 'e' || *f == 'E')) {
            ++f;
            bool eneg = false;
            if (f != l && (*f == '+' || *f == '-')) {
                eneg = (*f == '-');
                ++f;
            }
            if (f == l)
                return std::unexpected{errc::invalid_format};
            int e = 0;
            for (; f != l && *f >= '0' && *f <= '9'; ++f)
                if ((e = e * 10 + (*f - '0')) > 64 * static_cast<int>(K))
                    return std::unexpected{errc::overflow};
            for (; e > 0; --e) {
                I& t = eneg ? den : num;
                if (t > limit)
                    return std::unexpected{errc::overflow};
                t = t * ten;
            }
        }
        if (f != l)
            return std::unexpected{errc::invalid_format};
        return exact_frac<K>{neg ? -num : num, den};
    };
    const char* slash = first;
    while (slash != last && *slash != '/')
        ++slash;
    auto v = one(first, slash);
    if (v && slash != last) {
        const auto d = one(slash + 1, last);
        if (!d)
            return d;
        if (d->Num.is_zero())
            return std::unexpected{errc::division_by_zero};
        v = *v / *d;
    }
    return v;
}

// n / d (d != 0) rounded to an integer by M; the sign rules of div_rounded.
template <round_mode M, std::size_t K>
constexpr wide_sint<K> rounded_div(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    using I     = wide_sint<K>;
    auto [q, r] = I::divmod(n, d); // toward zero
    if (r.is_zero())
        return q;
    const bool neg = n.negative() != d.negative();
    const I    ar = r.negative() ? -r : r, ad = d.negative() ? -d : d;
    const I    away = neg ? q - I{1} : q + I{1};
    const I    r2   = ar * I{2};
    if constexpr (M == round_mode::floor) {
        if (neg)
            q = away;
    } else if constexpr (M == round_mode::ceil) {
        if (!neg)
            q = away;
    } else if constexpr (M == round_mode::nearest) {
        if (r2 >= ad)
            q = away;
    } else if constexpr (M == round_mode::half_even) {
        if (r2 > ad || (r2 == ad && (q.Word[0] & 1u) != 0))
            q = away;
    }
    return q;
}

// The value index of f on L's lattice (f / Notch) rounded by M, minus the
// slot base: L's slot offset. Exact is false when f lies between notches.
template <std::size_t K>
struct exact_index_result {
    wide_sint<K> Index;
    bool         Exact;
};

template <insidable L, round_mode M, std::size_t K>
constexpr auto exact_index(const exact_frac<K>& f) noexcept {
    constexpr std::size_t KK = exact_max<K, exact_limbs<L>>;
    using I                  = wide_sint<KK>;
    const exact_frac<KK> g{f};
    const I              n     = g.Num * static_cast<I>(wide_denominator(notch_of<L>));
    const I              d     = g.Den * static_cast<I>(wide_numerator(notch_of<L>)); // > 0
    const bool           exact = (n % d).is_zero();
    return exact_index_result<KK>{rounded_div<M>(n, d) - static_cast<I>(slot_base<L>), exact};
}

// The raw of slot offset `index` (0 .. slot count) in L's encoding.
template <insidable L, std::size_t K>
constexpr raw_t<L> raw_of_index(const wide_sint<K>& index) noexcept {
    if constexpr (point_raw<L>)
        return raw_t<L>{};
    else if constexpr (index_raw<L>)
        return static_cast<raw_t<L>>(index);
    else // value raw: raw == J
        return static_cast<raw_t<L>>(index + static_cast<wide_sint<K>>(slot_base<L>));
}

//---------------------------------------------------------------------------
// Wrapping value-index arithmetic — the integer path of + and ×.
//
// wrap_work_t<Result> is unsigned and as wide as Result's raw (at least 64
// bits). + − × wrap modulo 2^bits, so any expression whose true value is a
// Result raw comes out exact, however far its intermediates wrapped. Each
// operand enters as its value index J = value/Notch (an index raw plus
// Lower/Notch, a value raw as is — value storage has notch 1).
//---------------------------------------------------------------------------
template <insidable Result>
using wrap_work_t = std::conditional_t<wide_raw<Result>, raw_t<Result>, umax>;

// An operand's value-index range in `Unit`s: Lower/Unit .. Upper/Unit.
template <insidable X, grid_rational Unit>
inline constexpr grid_wide units_lo = exact_quotient(lower_of<X>, Unit);
template <insidable X, grid_rational Unit>
inline constexpr grid_wide units_hi = exact_quotient(upper_of<X>, Unit);

// The work type of an integer + or ×: signed imax when every value index
// involved (each operand in its unit, the result in its notch, and the
// result's slot offsets) provably fits — then nothing wraps, and the
// compiler keeps the value ranges, as the builtin paths always did — else
// the wrapping type.
template <insidable Result, insidable L, grid_rational UL, insidable R, grid_rational UR>
using index_work_t = std::conditional_t<signed_value_bits_of({units_lo<L, UL>,
                                                              units_hi<L, UL>,
                                                              units_lo<R, UR>,
                                                              units_hi<R, UR>,
                                                              units_lo<Result, notch_of<Result>>,
                                                              units_hi<Result, notch_of<Result>>,
                                                              grid_of<Result>.slot_count()}) <= 63,
                                        imax,
                                        wrap_work_t<Result>>;

// Integer raws: neither fp nor rational (a point's empty raw counts).
template <insidable B>
inline constexpr bool integer_raw = !fp_raw<B> && !rational_raw<B>;

// a / b for grid numbers, known at compile time to be an integer.
constexpr grid_wide exact_quotient(const grid_rational& a, const grid_rational& b) noexcept {
    return wide_numerator(a) * wide_denominator(b) / (wide_denominator(a) * wide_numerator(b));
}

template <typename W, insidable X>
constexpr W value_index(const X& x) noexcept {
    if constexpr (index_raw<X>)
        return static_cast<W>(slot_base<X>) + static_cast<W>(x.raw());
    else
        return static_cast<W>(x.raw());
}

// x's value in units of the notch `unit` (an integer: the unit divides
// x's notch, or x's value for a point).
template <typename W, grid_rational Unit, insidable X>
constexpr W value_in_units(const X& x) noexcept {
    // A point (Lower == Upper) holds its value in the type — even under a
    // width flag, whose raw stores it again.
    if constexpr (lower_of<X> == upper_of<X>) {
        constexpr grid_wide q = exact_quotient(lower_of<X>, Unit);
        return static_cast<W>(q);
    } else {
        constexpr grid_wide scale = exact_quotient(notch_of<X>, Unit);
        if constexpr (scale == grid_wide{1})
            return value_index<W>(x);
        else
            return value_index<W>(x) * static_cast<W>(scale);
    }
}

// The Result whose value index is j (taken modulo 2^bits).
template <insidable Result, typename W>
constexpr Result from_value_index(const W& j) noexcept {
    if constexpr (point_raw<Result>)
        return Result::from_raw(raw_t<Result>{});
    else if constexpr (index_raw<Result>)
        return Result::from_raw(static_cast<raw_t<Result>>(j - static_cast<W>(slot_base<Result>)));
    else
        return Result::from_raw(static_cast<raw_t<Result>>(j));
}

// Exact result of grid arithmetic: the value is on the result lattice and
// inside its interval by construction, so it maps straight to a raw.
template <insidable Result, std::size_t K>
constexpr Result exact_result(const exact_frac<K>& v) noexcept {
    return Result::from_raw(raw_of_index<Result>(exact_index<Result, round_mode::trunc>(v).Index));
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP
