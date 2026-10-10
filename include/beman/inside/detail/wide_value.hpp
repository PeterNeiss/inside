// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP
#define BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_int.hpp>

#include <compare>
#include <expected>
#include <optional>
#include <utility>

//---------------------------------------------------------------------------
// wide_value — exact values of insides the 64-bit paths cannot hold: a wide
// index raw (more than 2^64 slots), or grid numbers past 64 bits.
//
// On an anchored grid Lower/Notch is an integer m (slot_base), so a value is
// its value index J = m + raw times Notch, exactly; an unanchored grid counts
// in its value unit gcd(Notch, Lower) instead. exact_frac<K> carries a value
// as an unreduced fraction of K-limb integers; comparisons cross-multiply, and
// a store divides by the target notch and rounds by the policy's mode.
//
// K is sized from the grids involved (exact_limbs): every exact computation is
// at most a product of two values over a third notch, so three times the
// widest value suffices. Binary operations widen to the wider operand.
//---------------------------------------------------------------------------
namespace beman::inside::detail {

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

// m = Lower/Notch, the value index of slot 0 (0 for a continuous grid). An
// integer only on an anchored grid: the paths that use it require one.
template <insidable B>
inline constexpr grid_wide slot_base = [] {
    if constexpr (!notched<B>)
        return grid_wide{0};
    else
        return wide_numerator(lower_of<B>) * wide_denominator(notch_of<B>) /
               (wide_denominator(lower_of<B>) * wide_numerator(notch_of<B>));
}();

// ⌊a / b⌋ for grid numbers (b > 0).
constexpr grid_wide floor_quotient(const grid_rational& a, const grid_rational& b) noexcept {
    const grid_wide n = wide_numerator(a) * wide_denominator(b), d = wide_denominator(a) * wide_numerator(b);
    const grid_wide q = n / d;
    return (n % d != grid_wide{0} && n.negative()) ? q - grid_wide{1} : q;
}

// The lattice index of Lower, ⌊Lower/Notch⌋ — slot_base on an anchored grid.
// Its parity plus an offset's says which lattice points are even.
template <insidable B>
inline constexpr grid_wide lower_index_wide = notched<B> ? floor_quotient(lower_of<B>, notch_of<B>) : grid_wide{0};

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
    if constexpr (point_storage<B>) {
        auto bits = [](const grid_wide& v) { return bit_width_of(v.negative() ? -v : v); };
        return bits(wide_numerator(lower_of<B>)) + bits(wide_denominator(lower_of<B>));
    } else if constexpr (fraction_storage<B>)
        return 2 * decltype(raw_t<B>::Num)::bits;
    else if constexpr (!wide_valued<B>)
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

template <typename W, integer_storage X>
constexpr W value_index(const X& x) noexcept;

template <insidable B>
constexpr auto exact_of(const B& b) {
    constexpr std::size_t K = exact_limbs<B>;
    using I                 = wide_sint<K>;
    if constexpr (point_storage<B>)
        return exact_of_grid<K>(lower_of<B>);
    else if constexpr (rational_storage<B>)
        return exact_of<K>(b.raw());
    else if constexpr (fraction_storage<B>)
        return exact_frac<K>{b.raw()};
    else if constexpr (anchored<B>) // an integer raw: its value index J times the notch
        return exact_frac<K>{value_index<I>(b) * static_cast<I>(wide_numerator(notch_of<B>)),
                             static_cast<I>(wide_denominator(notch_of<B>))};
    else // an unanchored index raw: Lower + raw·Notch over their common denominator
        return exact_of_grid<K>(lower_of<B>) +
               exact_frac<K>{static_cast<I>(b.raw()) * static_cast<I>(wide_numerator(notch_of<B>)),
                             static_cast<I>(wide_denominator(notch_of<B>))};
}

// f in lowest terms.
template <std::size_t K>
constexpr exact_frac<K> reduced(const exact_frac<K>& f) noexcept {
    using I = wide_sint<K>;
    I a = f.Num.negative() ? -f.Num : f.Num, b = f.Den;
    while (!b.is_zero()) {
        const I t = a % b;
        a         = b;
        b         = t;
    }
    if (a == I{1})
        return f;
    return {f.Num / a, f.Den / a};
}

// v in lowest terms as the fraction raw F, or nothing when it needs more
// limbs than F has.
template <typename F, std::size_t K>
constexpr std::optional<F> frac_raw_of(const exact_frac<K>& v) noexcept {
    const auto r      = reduced(v);
    using I           = decltype(F::Num);
    constexpr int cap = I::bits - 1; // magnitude bits
    if (bit_width_of(r.Num.negative() ? -r.Num : r.Num) > cap || bit_width_of(r.Den) > cap)
        return std::nullopt;
    return F{static_cast<I>(r.Num), static_cast<I>(r.Den)};
}

// b's value in lowest terms, in the fewest limbs that hold every value of B.
template <insidable B>
constexpr auto reduced_exact(const B& b) {
    constexpr std::size_t K = limbs_for_bits(exact_value_bits<B> + 1);
    const auto            r = reduced(exact_of(b));
    return exact_frac<K>{static_cast<wide_sint<K>>(r.Num), static_cast<wide_sint<K>>(r.Den)};
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

// A finite double's exact value: m·2^e, with 2^1024 and 2^-1074 in reach.
constexpr exact_frac<18> exact_of_double(double d) noexcept {
    using I         = wide_sint<18>;
    int        e    = 0;
    const umax mant = static_cast<umax>(ldexp(frexp(d < 0 ? -d : d, &e), 53)); // |d| = mant·2^(e−53)
    e -= 53;
    const I n = e >= 0 ? I{mant} << e : I{mant};
    return {d < 0 ? -n : n, e >= 0 ? I{1} : I{1} << -e};
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
    const auto r   = reduced(f);
    const bool neg = r.Num.negative();
    const I    a   = neg ? -r.Num : r.Num;
    if (a > I{std::numeric_limits<umax>::max()} || r.Den > I{std::numeric_limits<imax>::max()})
        return std::unexpected{errc::overflow};
    const imax den = static_cast<imax>(r.Den);
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
    using I        = wide_sint<K>;
    auto [q, r]    = I::divmod(n, d); // toward zero
    const bool neg = n.negative() != d.negative();
    const I    ar = r.negative() ? -r : r, ad = d.negative() ? -d : d;
    if (!rounds_away(M, neg, classify_remainder(M, ar, ad), (q.Word[0] & 1u) != 0))
        return q;
    return neg ? q - I{1} : q + I{1};
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
    if constexpr (anchored<L>) {
        const I    n     = g.Num * static_cast<I>(wide_denominator(notch_of<L>));
        const I    d     = g.Den * static_cast<I>(wide_numerator(notch_of<L>)); // > 0
        const bool exact = (n % d).is_zero();
        return exact_index_result<KK>{rounded_div<M>(n, d) - static_cast<I>(slot_base<L>), exact};
    } else {
        // The offset (f − Lower)/Notch rounded in value space (rounds_up).
        const exact_frac<KK> o = g + -exact_of_grid<KK>(lower_of<L>);
        const I              n = o.Num * static_cast<I>(wide_denominator(notch_of<L>));
        const I              d = o.Den * static_cast<I>(wide_numerator(notch_of<L>)); // > 0 (Den > 0)
        const auto [q, r]      = floor_divmod(n, d);
        const bool up          = rounds_up(M,
                                           g.Num.negative(),
                                           classify_remainder(M, r, d),
                                           ((q + static_cast<I>(lower_index_wide<L>)).Word[0] & 1u) != 0);
        return exact_index_result<KK>{up ? q + I{1} : q, r.is_zero()};
    }
}

// The raw of slot offset `offset` (0 .. slot count) in L's storage. W is
// any signed integer holding the offset and the value index J = offset +
// Lower/Notch: imax, a wide_int, a wide_sint.
template <insidable L, typename W>
constexpr raw_t<L> raw_of_slot(const W& offset) noexcept {
    if constexpr (point_storage<L>)
        return raw_t<L>{};
    else if constexpr (index_storage<L>)
        return static_cast<raw_t<L>>(offset);
    else { // integer value raw on a whole-number grid: raw == J (Notch 1)
        static_assert(integer_value_storage<L>, "raw_of_slot: a slot needs a notched grid");
        return static_cast<raw_t<L>>(offset + static_cast<W>(slot_base<L>));
    }
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
using wrap_work_t = std::conditional_t<wide_index_storage<Result>, raw_t<Result>, umax>;

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
// URes: the unit the result is counted in (its notch, or a finer unit
// that also divides its Lower when the grids do not pass through 0).
template <insidable     Result,
          insidable     L,
          grid_rational UL,
          insidable     R,
          grid_rational UR,
          grid_rational URes = notch_of<Result>>
using index_work_t = std::conditional_t<signed_value_bits_of({units_lo<L, UL>,
                                                              units_hi<L, UL>,
                                                              units_lo<R, UR>,
                                                              units_hi<R, UR>,
                                                              units_lo<Result, URes>,
                                                              units_hi<Result, URes>,
                                                              grid_of<Result>.slot_count()}) <= 63,
                                        imax,
                                        wrap_work_t<Result>>;

// a / b for grid numbers, known at compile time to be an integer.
constexpr grid_wide exact_quotient(const grid_rational& a, const grid_rational& b) noexcept {
    return wide_numerator(a) * wide_denominator(b) / (wide_denominator(a) * wide_numerator(b));
}

template <typename W, integer_storage X>
constexpr W value_index(const X& x) noexcept {
    if constexpr (index_storage<X>)
        return static_cast<W>(slot_base<X>) + static_cast<W>(x.raw());
    else
        return static_cast<W>(x.raw());
}

// The unit an integer path counts X's values in: its value unit (the notch
// on an anchored grid), or |c| for a point c.
template <insidable X>
inline constexpr grid_rational unit_of = point_grid<X> ? abs(lower_of<X>) : grid_of<X>.value_unit();

// Two integer raws whose values, counted in the gcd of their value units,
// fit imax.
template <insidable L, insidable R>
inline constexpr bool units_cmp_fits = [] {
    if constexpr (!integer_storage<L> || !integer_storage<R>)
        return false;
    else {
        constexpr grid_rational U = grid_gcd_of(unit_of<L>, unit_of<R>);
        return signed_value_bits_of({units_lo<L, U>, units_hi<L, U>, units_lo<R, U>, units_hi<R, U>}) <= 63;
    }
}();

// x's value in units of `Unit` (an integer: the unit divides x's notch and
// Lower, or x's value for a point).
template <typename W, grid_rational Unit, insidable X>
constexpr W value_in_units(const X& x) noexcept {
    // A point (Lower == Upper) holds its value in the type.
    if constexpr (point_grid<X>) {
        constexpr grid_wide q = exact_quotient(lower_of<X>, Unit);
        return static_cast<W>(q);
    } else if constexpr (!anchored<X>) {
        // An index raw (integer value storage has integer values): Lower in
        // units, plus raw notches of `scale` units each.
        constexpr grid_wide base = exact_quotient(lower_of<X>, Unit), scale = exact_quotient(notch_of<X>, Unit);
        return static_cast<W>(base) + static_cast<W>(x.raw()) * static_cast<W>(scale);
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
    if constexpr (point_storage<Result>)
        return Result::from_raw(raw_t<Result>{});
    else if constexpr (index_storage<Result>)
        return Result::from_raw(static_cast<raw_t<Result>>(j - static_cast<W>(slot_base<Result>)));
    else
        return Result::from_raw(static_cast<raw_t<Result>>(j));
}

// The Result whose value is j `Unit`s (on its lattice by construction; an
// exact division when Unit is finer than its notch).
template <insidable Result, grid_rational Unit, typename W>
constexpr Result from_value_in_units(const W& j) noexcept {
    if constexpr (point_storage<Result> || (anchored<Result> && Unit == notch_of<Result>))
        return from_value_index<Result>(j);
    else if constexpr (index_storage<Result>) {
        constexpr grid_wide base  = exact_quotient(lower_of<Result>, Unit),
                            scale = exact_quotient(notch_of<Result>, Unit);
        return Result::from_raw(static_cast<raw_t<Result>>((j - static_cast<W>(base)) / static_cast<W>(scale)));
    } else // integer values counted in Unit = 1/k
        return Result::from_raw(
            static_cast<raw_t<Result>>(j / static_cast<W>(exact_quotient(grid_rational{1}, Unit))));
}

// Exact result of grid arithmetic: the value is on the result lattice and
// inside its interval by construction, so it maps straight to a raw.
template <insidable Result, std::size_t K>
constexpr Result exact_result(const exact_frac<K>& v) noexcept {
    return Result::from_raw(raw_of_slot<Result>(exact_index<Result, round_mode::trunc>(v).Index));
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP
