// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_CORES_HPP
#define BEMAN_INSIDE_DETAIL_MATH_CORES_HPP

#include <beman/inside/detail/math_adaptive.hpp>

#include <cstddef>
#include <expected>
#include <optional>

//---------------------------------------------------------------------------
// The integer path of the math engine: the exact inputs, the exact-value
// helpers, and one core per function (exact square and cube roots, series in
// wide fixed point), which decides every result and is the only tier at
// compile time and without an FPU. The tiers and the entry points are in
// detail/math_entry.hpp; the decision step and the driver in
// detail/math_adaptive.hpp.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::ax {
//---------------------------------------------------------------------------
// Inputs as exact values, in as few limbs as their grid needs.
//---------------------------------------------------------------------------
constexpr int grid_bits(const grid_wide& v) {
#if BEMAN_INSIDE_BIG_GRIDS
    return v.bit_width();
#else
    return bit_width_of(v.negative() ? -v : v);
#endif
}

// A grid integer as a double (a notch's p or q, which may pass imax).
constexpr double grid_double(const grid_wide& v) {
#if BEMAN_INSIDE_BIG_GRIDS
    return static_cast<double>(big_rational{v, big_int{1}});
#else
    return static_cast<double>(v);
#endif
}

// A value of In is n/d with d dividing the notch's denominator and
// |n| < 2^magnitude·d: its bits, plus a sign. A continuous grid takes its
// raw's: the 64-bit rational's, or a wide fraction's.
template <insidable In>
inline constexpr int input_bits = [] {
    if constexpr (fraction_storage<In>)
        return decltype(raw_t<In>::Num)::bits;
    else if constexpr (!notched<In>)
        return 130;
    else
        return grid_magnitude_bits<In> + grid_bits(wide_denominator(notch_of<In>)) + 2;
}();

template <insidable In>
inline constexpr std::size_t input_limbs = limbs_for_bits(input_bits<In>);

template <insidable In>
constexpr exact_frac<input_limbs<In>> exact_input(const In& x) {
    using I = wide_sint<input_limbs<In>>;
    if constexpr (wide_valued<In>) {
        const auto v = exact_of(x);
        return {static_cast<I>(v.Num), static_cast<I>(v.Den)};
    } else {
        const auto v = exact_of<2>(as_rational(x));
        return {static_cast<I>(v.Num), static_cast<I>(v.Den)};
    }
}

// |x| < 2^in_mag for every value of In.
template <insidable In>
inline constexpr int in_mag = grid_magnitude_bits<In> > 1 ? grid_magnitude_bits<In> : 1;

// Bits of an exact input's numerator or denominator.
template <std::size_t E>
inline constexpr int frac_bits = 64 * static_cast<int>(E);

// Result magnitude bound: values of Out lie below 2^out_kmax.
template <insidable Out>
inline constexpr int out_kmax = mag_bits<Out> + 1;

// The start precision for Out: its notch's bits plus guard bits.
template <insidable Out>
inline constexpr int start_bits = out_bits<Out> + 8 > 16 ? out_bits<Out> + 8 : 16;

template <std::size_t K>
constexpr exact_frac<K> exact_one() noexcept {
    return {wide_sint<K>{1}, wide_sint<K>{1}};
}
template <std::size_t K>
constexpr exact_frac<K> exact_int(imax v) noexcept {
    return {wide_sint<K>{v}, wide_sint<K>{1}};
}

//---------------------------------------------------------------------------
// Exact-value helpers at a compile-time scale S.
//---------------------------------------------------------------------------
// ⌊√f·2^S⌋ for f ≥ 0 (within 1 unit).
// Bits bounds the numerator's and denominator's bit widths.
template <int S, std::size_t K, int Bits, std::size_t E>
constexpr wide_sint<K> sqrt_exact_q(const exact_frac<E>& f) noexcept {
    using I   = wide_sint<limbs_for_bits(2 * Bits + 2 * S + 2)>;
    const I n = (I{f.Num} * I{f.Den}) << (2 * S);
    return static_cast<wide_sint<K>>(isqrt(n) / I{f.Den});
}

// log x for x > 0 at scale S: x = m·2^e exactly, m in [0.7, 1.42] rounded to
// scale S, log x = log m + e·ln 2.
template <int S, std::size_t K, int Bits, std::size_t E>
constexpr fx<K> log_exact(const exact_frac<E>& x) noexcept {
    using I                  = wide_sint<K>;
    constexpr std::size_t KI = limbs_for_bits(2 * Bits + S + 2);
    int                   e  = floor_log2(x);
    I                     m  = to_q_at<K, KI>(x, S - e);
    if (mul_q(m, m, S) > (one_q<K>(S) << 1)) {
        ++e;
        m = to_q_at<K, KI>(x, S - e);
    }
    const fx<K> l   = log_series<S>(m, 1);
    const I     ln2 = static_cast<I>(ln2_q<S>);
    return {l.Value + I{e} * ln2, l.Error + static_cast<umax>(e < 0 ? -e : e) + 1};
}

// An approx moved to scale A ≤ its own, rounding (error shrinks, plus ½).
template <std::size_t K>
constexpr fx<K> rescale(const approx<K>& a, int A) noexcept {
    const int sh = a.Scale - A;
    if (sh <= 0) // a bound past 64 bits saturates (never wraps small)
        return {a.Value << (-sh), -sh < 64 && a.Error <= (~umax{0} >> -sh) ? a.Error << (-sh) : ~umax{0}};
    return {round_shift(a.Value, sh), shr_bound(a.Error, sh) + 1};
}

// asin x for |x| ≤ 1/2 (exact): atan(x/√(1−x²)), the square root as
// x·(1/√(1−x²)) with 1 − x² ≥ 3/4 in fixed point.
template <int S, std::size_t K, int Bits, std::size_t E>
constexpr fx<K> asin_small(const exact_frac<E>& x) noexcept {
    using I    = wide_sint<K>;
    const I xq = to_q<S, K>(x);                  // within ½
    const I c  = one_q<K>(S) - mul_q(xq, xq, S); // within 1.5, ≥ 3/4
    const I t  = mul_q(xq, rsqrt_q<S>(c), S);    // |t| ≤ 0.58, within 6
    return atan_fixed<S>(t, 6);
}

// asin √y for 0 ≤ y ≤ 1/4 (exact): u = √y exactly rounded (within 1), then
// atan(u·(1/√(1−u²))) with 1 − u² ≥ 3/4.
template <int S, std::size_t K, int Bits, std::size_t E>
constexpr fx<K> asin_sqrt(const exact_frac<E>& y) noexcept {
    using I   = wide_sint<K>;
    const I u = sqrt_exact_q<S, K, Bits + 2>(y);
    const I w = one_q<K>(S) - mul_q(u, u, S); // within 2, ≥ 3/4
    const I t = mul_q(u, rsqrt_q<S>(w), S);   // within 7
    return atan_fixed<S>(t, 7);
}

//---------------------------------------------------------------------------
// The cores: one per function. Each holds its exact inputs and computes the
// result within its error bound at a precision W (run<W>), plus the exact
// rational results (exact()), which are the only values a rounding boundary
// can hold.
//---------------------------------------------------------------------------
template <std::size_t E>
using maybe_exact = std::optional<exact_frac<E>>;

// deep_extra helpers (bits, saturating at the driver's deep_limit): ⌈2^Mag ·
// milli / 1000⌉ bits for a result that nears a grid point like 2^(−c·|x|),
// |x| < 2^Mag; and the larger of two bounds.
constexpr int deep_scaled(int mag, int milli) noexcept {
    return mag >= 20 ? deep_limit : static_cast<int>(((imax{1} << mag) * milli + 999) / 1000);
}
constexpr int deep_max(int a, int b) noexcept { return a > b ? a : b; }

// exp x. Exact only at 0.
template <std::size_t E, int Mag, int KMax>
struct exp_core {
    exact_frac<E> X;
    // e^−|x| near 0 (1.443·|x| bits), e^x = 1 + x + x²/2 near 1 + x.
    static constexpr int     deep_extra = deep_max(deep_scaled(Mag, 1443), 2 * frac_bits<E>);
    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{exact_one<E>()} : std::nullopt; }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 8 + KMax;
        using I                 = fixed_t<S + Mag + 2>; // |x|·log2(e) < 2^(S+Mag+1)
        constexpr std::size_t K = limbs_of<I>;
        return exp_fixed<S>(to_q<S, K>(X), 1, KMax);
    }
};

// 2^x = 2^k·e^(f·ln 2), x = k + f. Exact at integers.
template <std::size_t E, int Mag, int KMax>
struct exp2_core {
    exact_frac<E> X;
    // 2^−|x| near 0 (|x| bits), 2^x near 1 for a small x.
    static constexpr int deep_extra = deep_max(deep_scaled(Mag, 1000), 2 * frac_bits<E>);
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 8 + KMax;
        using I                 = fixed_t<S + 8>;
        using J                 = wide_sint<E>;
        constexpr std::size_t K = limbs_of<I>;
        const J               k = rounded_div<round_mode::floor>(X.Num, X.Den);
        if (J{KMax} < k)
            return approx<K>{I{1}, -(KMax + 2), 0};
        if (k < J{-(S + 2)})
            return approx<K>{I{1}, S + 2, 1};
        int                 kk = static_cast<int>(static_cast<imax>(k));
        const exact_frac<E> f{X.Num - k * X.Den, X.Den}; // in [0, 1)
        if (is_zero(f))
            return approx<K>{I{1}, -kk, 0};
        const I ln2 = static_cast<I>(ln2_q<S>);
        I       r   = mul_q(to_q<S, K>(f), ln2, S); // within 2
        if (r > (ln2 >> 1)) {
            r -= ln2;
            ++kk;
        }
        const fx<K> e = exp_series<S>(r, 3);
        return approx<K>{e.Value, S - kk, e.Error};
    }
};

// log x, x > 0. Exact at 1.
template <std::size_t E, int Bits = frac_bits<E>>
struct log_core {
    exact_frac<E>            X;
    constexpr maybe_exact<E> exact() const {
        return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 8 + std::bit_width(static_cast<unsigned>(Bits));
        using I                 = fixed_t<S + std::bit_width(static_cast<unsigned>(Bits)) + 8>;
        constexpr std::size_t K = limbs_of<I>;
        const fx<K>           l = log_exact<S, K, Bits>(X);
        return approx<K>{l.Value, S, l.Error};
    }
};

// The integer k with x = B^k (B = 2 or 10), if any, without reducing x:
// with the factors 2 (and 5) of n and d stripped, n/d = 2^a·5^c·n'/d' is a
// power of B exactly when n' = d' and a (= c, for 10) is that power.
template <std::size_t E>
constexpr std::optional<imax> exact_log(const exact_frac<E>& x, int B) noexcept {
    using I = wide_sint<E>;
    if (x.Num.negative() || x.Num.is_zero())
        return std::nullopt;
    I    n = x.Num, d = x.Den;
    auto strip2 = [](I& v) {
        imax k = 0;
        while (v.Word[0] == 0) {
            v = v >> 64;
            k += 64;
        }
        const int z = std::countr_zero(static_cast<umax>(v.Word[0]));
        v           = v >> z;
        return k + z;
    };
    auto strip5 = [](I& v) {
        imax k = 0;
        for (;;) {
            const auto qr = divmod_small(wide_uint<E>{v}, 5);
            if (qr.Remainder != 0)
                return k;
            v = I{qr.Quotient};
            ++k;
        }
    };
    const imax a = strip2(n) - strip2(d);
    if (B == 10 && strip5(n) - strip5(d) != a)
        return std::nullopt;
    if (!(n == d))
        return std::nullopt;
    return a;
}

// log2 x and log10 x: log x / ln B. Exact at powers of B.
template <std::size_t E, int B, int Bits = frac_bits<E>>
struct logb_core {
    exact_frac<E>            X;
    constexpr maybe_exact<E> exact() const {
        if (const auto k = exact_log(X, B))
            return exact_int<E>(*k);
        return std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 10 + std::bit_width(static_cast<unsigned>(Bits));
        using I                 = fixed_t<S + std::bit_width(static_cast<unsigned>(Bits)) + 8>;
        constexpr std::size_t K = limbs_of<I>;
        const fx<K>           l = log_exact<S, K, Bits>(X);
        // log x · (1/ln B): within 1.45·error + 2 (the constant is within 1).
        const I inv = B == 2 ? static_cast<I>(log2e_q<S>) : static_cast<I>(log10e_q<S>);
        return approx<K>{mul_q(l.Value, inv, S), S, 2 * l.Error + 2 + static_cast<umax>(Bits)};
    }
};

// Residues of squares mod 64, as a bit set: 12 of the 64.
inline constexpr umax square_mod64 = [] {
    umax m = 0;
    for (umax i = 0; i < 64; ++i)
        m |= umax{1} << (i * i % 64);
    return m;
}();

// √x, x ≥ 0: ⌊√x·2^P⌋ exactly. Exact when x is a square of a rational.
template <std::size_t E, int Bits = frac_bits<E>>
struct sqrt_core {
    exact_frac<E> X;
    // n/d is the square of a rational exactly when n·d is a perfect square,
    // and then √(n/d) = √(n·d)/d. Most non-squares fail the test mod 64.
    constexpr maybe_exact<E> exact() const {
        using J   = wide_sint<2 * E>;
        const J m = J{X.Num} * J{X.Den};
        if (!((square_mod64 >> (m.Word[0] & 63)) & 1))
            return std::nullopt;
        const J s = isqrt(m);
        if (!(s * s == m))
            return std::nullopt;
        return exact_frac<E>{static_cast<wide_sint<E>>(s), X.Den};
    }
    template <int W>
    constexpr auto run() const {
        constexpr int P         = W + 2;
        using I                 = fixed_t<Bits + P + 4>;
        constexpr std::size_t K = limbs_of<I>;
        return approx<K>{sqrt_exact_q<P, K, Bits>(X), P, 1};
    }
};

// Residues of cubes mod 63, as a bit set: 9 of the 63.
inline constexpr umax cube_mod63 = [] {
    umax m = 0;
    for (umax i = 0; i < 63; ++i)
        m |= umax{1} << (i * i * i % 63);
    return m;
}();

// ∛x: ⌊∛(n·d²·2^(3P))⌋/d exactly. Exact when x is a cube of a rational.
template <std::size_t E, int Bits = frac_bits<E>>
struct cbrt_core {
    exact_frac<E> X;
    // |n|/d is the cube of a rational exactly when |n|·d² is a perfect cube,
    // and then ∛(|n|/d) = ∛(|n|·d²)/d. Most non-cubes fail the test mod 63.
    constexpr maybe_exact<E> exact() const {
        using J   = wide_sint<3 * E>;
        const J m = J{abs(X).Num} * J{X.Den} * J{X.Den};
        if (!((cube_mod63 >> static_cast<umax>(m % J{63})) & 1))
            return std::nullopt;
        const J c = icbrt(m);
        if (!(c * c * c == m))
            return std::nullopt;
        const wide_sint<E> r = static_cast<wide_sint<E>>(c);
        return exact_frac<E>{X.Num.negative() ? -r : r, X.Den};
    }
    template <int W>
    constexpr auto run() const {
        constexpr int P         = W + 2;
        using I                 = fixed_t<Bits + P + 4>;
        constexpr std::size_t K = limbs_of<I>;
        using J                 = wide_sint<limbs_for_bits(3 * Bits + 3 * P + 4)>;
        const exact_frac<E> a   = abs(X);
        const J             n   = (J{a.Num} * J{a.Den} * J{a.Den}) << (3 * P);
        const I             y   = static_cast<I>(icbrt(n) / J{a.Den});
        return approx<K>{X.Num.negative() ? -y : y, P, 1};
    }
};

// sin, cos and tan: x = k·π/2 + r, |r| ≤ π/4, then the series and the
// quadrant. Exact at 0.
enum class trig { sin, cos, tan };

template <std::size_t E, int Mag, trig Fn, int KMax>
struct trig_core {
    exact_frac<E> X;
    // sin x = x − x³/6 near x, cos x = 1 − x²/2 near 1 (tan: none).
    static constexpr int     deep_extra = Fn == trig::sin ? 3 * frac_bits<E> : Fn == trig::cos ? 2 * frac_bits<E> : 0;
    constexpr maybe_exact<E> exact() const {
        if (!is_zero(X))
            return std::nullopt;
        return Fn == trig::cos ? exact_one<E>() : exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}};
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S = W + 10 + (Fn == trig::tan ? KMax + 4 : 0);
        constexpr int T = S + Mag + 4;                                       // reduce with Mag more bits
        using I         = fixed_t<(Fn == trig::tan ? S + KMax + 4 : S + 2)>; // |sin|, |cos| ≤ 1; |tan| ≤ 2^(KMax+3)
        using R         = fixed_t<T + Mag + 8>;
        constexpr std::size_t K  = limbs_of<I>;
        const R               xq = to_q<T, limbs_of<R>>(X);
        const R               hp = static_cast<R>(pi_q<T - 1>);                                    // π/2 within 1
        const R               k  = round_shift(mul_q(xq, static_cast<R>(two_over_pi_q<T>), T), T); // round(x·2/π)
        const I               r  = static_cast<I>(round_shift(xq - k * hp, T - S)); // |r| ≤ π/4 + a hair, within 2
        const unsigned        q  = static_cast<unsigned>(k.Word[0] & 3u);
        // sin x and cos x by quadrant: (s, c), (c, −s), (−s, −c), (−c, s).
        auto neg   = [](fx<K> f) { return fx<K>{-f.Value, f.Error}; };
        auto sin_x = [&] {
            return (q == 0)   ? sin_series<S>(r, 2)
                   : (q == 1) ? cos_series<S>(r, 2)
                   : (q == 2) ? neg(sin_series<S>(r, 2))
                              : neg(cos_series<S>(r, 2));
        };
        auto cos_x = [&] {
            return (q == 0)   ? cos_series<S>(r, 2)
                   : (q == 1) ? neg(sin_series<S>(r, 2))
                   : (q == 2) ? neg(cos_series<S>(r, 2))
                              : sin_series<S>(r, 2);
        };
        if constexpr (Fn == trig::sin) {
            const fx<K> sn = sin_x();
            return approx<K>{sn.Value, S, sn.Error};
        } else if constexpr (Fn == trig::cos) {
            const fx<K> cs = cos_x();
            return approx<K>{cs.Value, S, cs.Error};
        } else {
            const fx<K> sn = sin_x(), cs = cos_x();
            // tan = sin/cos. The error bound, (δs + |t|·δc)/(|c| − δc) + 2 units,
            // and the test for a result past 2^KMax only need to be upper and
            // lower bounds: computed in doubles, widened by a hair.
            const double cd = static_cast<double>(cs.Value), sd = static_cast<double>(sn.Value);
            const double ac = cd < 0 ? -cd : cd, as = sd < 0 ? -sd : sd;
            const double ec = static_cast<double>(cs.Error), es = static_cast<double>(sn.Error);
            if (!(2 * ec < ac))
                return approx<K>{I{0}, S, ~umax{0}}; // cos unresolved
            const double unit = ::beman::inside::detail::ldexp(1.0, S);
            if ((as - es) / (ac + ec) * (1 - 0x1p-40) > ::beman::inside::detail::ldexp(1.0, KMax))
                return approx<K>{(sn.Value.negative() != cs.Value.negative()) ? I{-1} : I{1}, -(KMax + 2), 0};
            const I      t   = div_q(sn.Value, cs.Value, S);
            const double at  = (as + es) / (ac - ec);
            const double err = ((es + at * ec) / (ac - ec) * unit + 2) * (1 + 0x1p-40) + 1;
            const umax   e   = err < 0x1p62 ? static_cast<umax>(err) : ~umax{0};
            return approx<K>{t, S, e};
        }
    }
};

// atan x: |x| ≤ 1 directly, else ±π/2 − atan(1/x). Exact at 0.
template <int S, std::size_t K, std::size_t E>
constexpr fx<K> atan_exact(const exact_frac<E>& x) noexcept {
    using I = wide_sint<K>;
    if (!(exact_one<E>() < abs(x)))
        return atan_fixed<S>(to_q<S, K>(x), 1);
    const fx<K> a  = atan_fixed<S>(to_q<S, K>(inverse(x)), 1);
    const I     hp = static_cast<I>(pi_q<S - 1>);
    return {(x.Num.negative() ? -hp : hp) - a.Value, a.Error + 1};
}

template <std::size_t E>
struct atan_core {
    exact_frac<E>            X;
    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 10;
        using I                 = fixed_t<S + 3>; // |atan| < 2, 1 + t·c ≤ 2
        constexpr std::size_t K = limbs_of<I>;
        const fx<K>           a = atan_exact<S, K>(X);
        return approx<K>{a.Value, S, a.Error};
    }
};

// atan2(y, x) in [−π, π]; 0 for y = 0, x ≥ 0 (and the (0, 0) convention).
template <std::size_t E>
struct atan2_core {
    exact_frac<E>            Y, X;
    constexpr maybe_exact<E> exact() const {
        if (is_zero(Y) && !X.Num.negative())
            return exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}};
        return std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 10;
        using I                 = fixed_t<S + 8>;
        constexpr std::size_t K = limbs_of<I>;
        using F                 = exact_frac<2 * E + 1>;
        const F y{Y}, x{X};
        const I pi = static_cast<I>(pi_q<S>);
        if (!(abs(x) < abs(y))) {
            const fx<K> a = atan_exact<S, K>(y / x);
            if (!x.Num.negative())
                return approx<K>{a.Value, S, a.Error};
            return approx<K>{a.Value + (y.Num.negative() ? -pi : pi), S, a.Error + 1};
        }
        const fx<K> a  = atan_exact<S, K>(x / y);
        const I     hp = static_cast<I>(pi_q<S - 1>);
        return approx<K>{(y.Num.negative() ? -hp : hp) - a.Value, S, a.Error + 1};
    }
};

// asin x, |x| ≤ 1: small arguments directly; past 1/2, the half-angle form
// ±(π/2 − 2·asin √((1−|x|)/2)), which stays accurate up to ±1.
template <std::size_t E, int Bits = frac_bits<E>>
struct asin_core {
    exact_frac<E>            X;
    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 12;
        using I                 = fixed_t<S + 8>;
        constexpr std::size_t K = limbs_of<I>;
        const exact_frac<E>   half{wide_sint<E>{1}, wide_sint<E>{2}};
        if (!(half < abs(X))) {
            const fx<K> a = asin_small<S, K, Bits>(X);
            return approx<K>{a.Value, S, a.Error};
        }
        using F         = exact_frac<E + 1>;
        const F     one = exact_one<E + 1>();
        const F     y   = (one + F{-abs(X)}) * F{half}; // (1 − |x|)/2
        const fx<K> a   = asin_sqrt<S, K, Bits + 1>(y);
        const I     v   = static_cast<I>(pi_q<S - 1>) - (a.Value << 1);
        return approx<K>{X.Num.negative() ? -v : v, S, 2 * a.Error + 1};
    }
};

// acos x = π/2 − asin x for |x| ≤ 1/2; 2·asin √((1−x)/2) above;
// π − 2·asin √((1+x)/2) below. Exact at 1.
template <std::size_t E, int Bits = frac_bits<E>>
struct acos_core {
    exact_frac<E>            X;
    constexpr maybe_exact<E> exact() const {
        return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 12;
        using I                 = fixed_t<S + 8>;
        constexpr std::size_t K = limbs_of<I>;
        const exact_frac<E>   half{wide_sint<E>{1}, wide_sint<E>{2}};
        if (!(half < abs(X))) {
            const fx<K> a = asin_small<S, K, Bits>(X);
            return approx<K>{static_cast<I>(pi_q<S - 1>) - a.Value, S, a.Error + 1};
        }
        using F         = exact_frac<E + 1>;
        const F     one = exact_one<E + 1>();
        const F     x{X};
        const bool  neg = X.Num.negative();
        const F     y   = (neg ? one + x : one + F{-x}) * F{half};
        const fx<K> a   = asin_sqrt<S, K, Bits + 1>(y);
        if (!neg)
            return approx<K>{a.Value << 1, S, 2 * a.Error};
        return approx<K>{static_cast<I>(pi_q<S>) - (a.Value << 1), S, 2 * a.Error + 1};
    }
};

// sinh, cosh and tanh from e^|x| and e^−|x| at absolute scale A. Exact at 0
// (sinh, tanh: 0; cosh: 1).
enum class hyp { sinh, cosh, tanh };

template <std::size_t E, int Mag, hyp Fn, int KMax>
struct hyp_core {
    exact_frac<E> X;
    // tanh |x| = 1 − 2e^−2|x| near 1 (2.886·|x| bits) and x − x³/3 near x;
    // sinh near x by x³/6, cosh near 1 by x²/2.
    static constexpr int     deep_extra = Fn == hyp::tanh   ? deep_max(deep_scaled(Mag, 2886), 3 * frac_bits<E>)
                                          : Fn == hyp::sinh ? 3 * frac_bits<E>
                                                            : 2 * frac_bits<E>;
    constexpr maybe_exact<E> exact() const {
        if (!is_zero(X))
            return std::nullopt;
        return Fn == hyp::cosh ? exact_one<E>() : X;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int A           = W + 10;
        constexpr int KM          = Fn == hyp::tanh ? 1 : KMax + 1;
        constexpr int S           = A + KM + 4;
        using I                   = fixed_t<S + Mag + 3>; // 2|x|·log2(e) < 2^(S+Mag+2)
        constexpr std::size_t K   = limbs_of<I>;
        const bool            neg = X.Num.negative();
        const I               ax  = to_q<S, K>(abs(X)); // within ½
        if constexpr (Fn == hyp::tanh) {
            // tanh |x| = (1 − g)/(1 + g), g = e^(−2|x|) ≤ 1.
            const fx<K> g   = rescale(exp_fixed<S>(-(ax << 1), 1, KM), A);
            const I     one = one_q<K>(A);
            const I     t   = div_q(one - g.Value, one + g.Value, A);
            return approx<K>{neg ? -t : t, A, 2 * g.Error + 2};
        } else {
            // One reduction |x| = k·ln 2 + r: e^r = E + O and e^−r = E − O from
            // the even and odd halves of the series.
            const I k = round_shift(mul_q(ax, static_cast<I>(log2e_q<S>), S), S);
            if (I{KM} < k) // e^|x| past 2^KM: past Out
                return approx<K>{(Fn == hyp::sinh && neg) ? I{-1} : I{1}, -(KMax + 2), 0};
            const int  kk = static_cast<int>(static_cast<imax>(k));
            const I    r  = ax - k * static_cast<I>(ln2_q<S>); // within ½ + k
            const I    z  = mul_q(r, r, S);
            const I    ev = horner<series::cosh, S, false>(z);
            const I    od = mul_q(r, horner<series::sinh, S, false>(z), S);
            const umax e  = 24 + 2 * (static_cast<umax>(kk) + 2);
            const I    P  = round_shift(ev + od, S - kk - A); // e^|x| at scale A
            const I    M  = round_shift(ev - od, S + kk - A); // e^−|x| at scale A
            const umax ep = shr_bound(e, S - kk - A) + 1, em = shr_bound(e, S + kk - A) + 1;
            if constexpr (Fn == hyp::cosh)
                return approx<K>{(P + M) >> 1, A, (ep + em) / 2 + 1};
            else {
                const I sv = (P - M) >> 1;
                return approx<K>{neg ? -sv : sv, A, (ep + em) / 2 + 1};
            }
        }
    }
};

// asinh x = ±log(|x| + √(x²+1)); acosh x = log(x + √((x−1)(x+1))), x ≥ 1;
// atanh x = ±½·log((1+|x|)/(1−|x|)), |x| < 1.
enum class ahyp { asinh, acosh, atanh };

template <std::size_t E, int Mag, ahyp Fn, int Bits = frac_bits<E>>
struct ahyp_core {
    exact_frac<E>            X;
    constexpr maybe_exact<E> exact() const {
        if (Fn == ahyp::acosh)
            return is_one(X) ? maybe_exact<E>{exact_frac<E>{wide_sint<E>{0}, wide_sint<E>{1}}} : std::nullopt;
        return is_zero(X) ? maybe_exact<E>{X} : std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S = W + 12 + std::bit_width(static_cast<unsigned>(2 * Bits + Mag));
        // asinh forms x² + 1 in the word: twice the magnitude bits.
        using I                 = fixed_t<S + (Fn == ahyp::asinh ? 2 * Mag : Mag) + 8>;
        constexpr std::size_t K = limbs_of<I>;
        using F                 = exact_frac<2 * E + 1>;
        const bool neg          = X.Num.negative();
        const F    a            = abs(F{X});
        if constexpr (Fn == ahyp::atanh) {
            // (1 + |x|)/(1 − |x|) = (d + |n|)/(d − |n|), d − |n| > 0.
            const F     q{a.Num + a.Den, a.Den - a.Num};
            const fx<K> l = log_exact<S, K, 2 * Bits + 2>(q);
            return approx<K>{neg ? -(l.Value >> 1) : (l.Value >> 1), S, l.Error / 2 + 1};
        } else {
            if constexpr (Fn == ahyp::asinh) {
                // c = x² + 1 = m·4^h, m in [1, 4): √c = √m·2^h with √m = m·(1/√m);
                // log of v/2^h = (|x| + √c)/2^h, plus h·ln 2.
                const I     aq = to_q<S, K>(a);                  // within ½
                const I     c  = mul_q(aq, aq, S) + one_q<K>(S); // within |x| + 1
                const int   h  = (bit_width_of(c) - 1 - S) / 2;
                const I     m  = c >> (2 * h);                           // within 3
                const I     v  = (aq >> h) + mul_q(m, rsqrt_q<S>(m), S); // in [1, 3), within 30
                const fx<K> l  = log_fixed<S>(v, 30);
                const I     lv = l.Value + I{h} * static_cast<I>(ln2_q<S>);
                return approx<K>{neg ? -lv : lv, S, l.Error + static_cast<umax>(h) + 1};
            } else {
                const F     r{(a.Num - a.Den) * (a.Num + a.Den), a.Den * a.Den};     // x² − 1 = (n − d)(n + d)/d²
                const I     v = to_q<S, K>(a) + sqrt_exact_q<S, K, 2 * Bits + 2>(r); // ≥ 1, within 2
                const fx<K> l = log_fixed<S>(v, 2);
                return approx<K>{l.Value, S, l.Error};
            }
        }
    }
};

// b^e for b > 0: e^(e·log b). Exact for e = 0, b = 1, and e = p/q (q ≤ 64)
// when b's numerator and denominator are perfect q-th powers (4^0.5 = 2,
// 2.25^1.5 = 3.375) and the power fits PowLimbs: such a result can sit on a
// rounding boundary, where no approximation decides.
inline constexpr std::size_t pow_limbs = 32; // exact integer powers up to 2048 bits

// The q-th root of n ≥ 0 when n is a perfect q-th power: bit by bit, the
// largest r with r^q ≤ n, then r^q == n.
constexpr std::optional<wide_sint<pow_limbs>> exact_root(const wide_sint<pow_limbs>& n, int q) noexcept {
    using P = wide_sint<pow_limbs>;
    if (q == 1 || n.is_zero() || n == P{1})
        return n;
    auto at_most = [&](const P& c) { // c^q ≤ n, stopping once past n
        P p{1};
        for (int j = 0; j < q; ++j)
            if (n < (p = p * c))
                return false;
        return true;
    };
    P r{0};
    for (int i = (bit_width_of(n) + q - 1) / q; i-- > 0;)
        if (const P c = r + (P{1} << i); at_most(c))
            r = c;
    P p{1};
    for (int j = 0; j < q; ++j)
        p = p * r;
    return p == n ? std::optional<P>{r} : std::nullopt;
}

template <std::size_t EB, std::size_t EE>
constexpr std::optional<exact_frac<pow_limbs>> exact_pow(const exact_frac<EB>& b, const exact_frac<EE>& e) noexcept {
    using P = wide_sint<pow_limbs>;
    if (is_zero(e) || is_one(b))
        return exact_frac<pow_limbs>{P{1}, P{1}};
    const exact_frac<EE> re = reduced(e); // p/q, q > 0
    if (bit_width_of(re.Den) > 7 || 64 < static_cast<imax>(re.Den))
        return std::nullopt;
    const int            q  = static_cast<int>(static_cast<imax>(re.Den));
    const auto           k  = re.Num;
    const auto           ak = k.negative() ? -k : k;
    const exact_frac<EB> r  = reduced(b);
    // b^(1/q) first: both parts perfect q-th powers, else irrational.
    const auto rn = exact_root(P{r.Num}, q), rd = exact_root(P{r.Den}, q);
    if (!rn || !rd)
        return std::nullopt;
    const int bits = bit_width_of(*rn) + bit_width_of(*rd);
    if (bit_width_of(ak) > 12 || static_cast<imax>(ak) * bits > 64 * static_cast<imax>(pow_limbs) - 8)
        return std::nullopt;
    P n{1}, d{1};
    for (imax i = 0; i < static_cast<imax>(ak); ++i) {
        n = n * *rn;
        d = d * *rd;
    }
    return k.negative() ? inverse(exact_frac<pow_limbs>{n, d}) : exact_frac<pow_limbs>{n, d};
}

// ConstBase, when nonzero, is the base known at compile time (pow_base): its
// log is then a constant of each precision.
template <std::size_t EB,
          std::size_t EE,
          int         MagE,
          int         KMax,
          int         BitsB     = frac_bits<EB>,
          int         BitsE     = frac_bits<EE>,
          imax        ConstBase = 0>
struct pow_core {
    exact_frac<EB>                                 B;
    exact_frac<EE>                                 Exp;
    constexpr std::optional<exact_frac<pow_limbs>> exact() const { return exact_pow(B, Exp); }
    template <int W>
    constexpr auto run() const {
        // log b at MagE more bits, so e·log b (|e| < 2^MagE) is within 2 units
        // at scale S.
        constexpr int S         = W + 12 + KMax;
        constexpr int SL        = S + MagE + std::bit_width(static_cast<unsigned>(BitsB));
        using I                 = fixed_t<SL + MagE + std::bit_width(static_cast<unsigned>(BitsB)) + BitsE + 8>;
        constexpr std::size_t K = limbs_of<I>;
        const fx<K>           l = [&] {
            if constexpr (ConstBase != 0) {
                constexpr fx<K> c = log_exact<SL, K, BitsB>(exact_int<EB>(ConstBase));
                return c;
            } else
                return log_exact<SL, K, BitsB>(B);
        }();
        const I prod = mul_q(l.Value, static_cast<I>(Exp.Num), 0);
        const I q =
            bit_width_of(Exp.Den) <= 64 ? div_small(prod, static_cast<umax>(Exp.Den)) : prod / static_cast<I>(Exp.Den);
        const I    t  = round_shift(q, SL - S);
        const umax dt = (l.Error >> (SL - S - MagE)) + 2;
        return exp_fixed<S>(t, dt, KMax);
    }
};

//---------------------------------------------------------------------------
// Auto output grids. An endpoint of a deduced output is a core's result at
// an endpoint of the input, rounded outward to the input's notch: down for
// a lower end, up for an upper end. The rounding runs on the core's error
// interval, so the bound always covers the true value; one escalation makes
// it tight except within 2^-2W of a lattice point.
//---------------------------------------------------------------------------
template <grid_rational Notch>
inline constexpr std::size_t notch_limbs =
    limbs_for_bits(grid_bits(wide_numerator(Notch)) + grid_bits(wide_denominator(Notch)) + 2);

// k·Notch as a grid number.
template <grid_rational Notch, std::size_t K>
constexpr grid_rational lattice_point(const wide_sint<K>& k) {
#if BEMAN_INSIDE_BIG_GRIDS
    return grid_rational{big_int{k}} * Notch;
#else
    // |k| may pass 2^63 (π on a 2^-62 notch): a rational's numerator holds
    // up to 2^64 − 1, past it the point has no 64-bit grid number.
    const wide_sint<K> a = k.negative() ? -k : k;
    if (bit_width_of(a) > 64)
        constexpr_error<"beman::inside::math: a deduced output limit is past the 64-bit grid numbers">();
    const grid_rational m = rational{static_cast<umax>(a)} * Notch;
    return k.negative() ? -m : m;
#endif
}

// ⌊v/Notch⌋ (Up false) or ⌈v/Notch⌉ (Up true) for v = n/d, d > 0.
template <grid_rational Notch, bool Up, std::size_t K>
constexpr wide_sint<K + notch_limbs<Notch>> lattice_index(const wide_sint<K>& n, const wide_sint<K>& d) {
    using J   = wide_sint<K + notch_limbs<Notch>>;
    const J p = static_cast<J>(wide_numerator(Notch)), q = static_cast<J>(wide_denominator(Notch));
    return rounded_div<Up ? round_mode::ceil : round_mode::floor>(J{n} * q, J{d} * p);
}

template <grid_rational Notch, bool Up, int W, int Cap, typename Core>
constexpr grid_rational lattice_bound_from(const Core& core) {
    const auto            a = core.template run<W>();
    constexpr std::size_t K = limbs_of<decltype(a.Value)> + 1;
    using I                 = wide_sint<K>;
    const I y{a.Value}, e{a.Error};
    // ⌊v/Notch⌋ of v = y/2^Scale through a shift: a tiny value (exp(−58)
    // ≈ 2^−84) has a 2^Scale past I, and a nested floor (ceil) by positive
    // divisors is the floor (ceil) of the whole quotient.
    auto at = [&](const I& v) {
        if (a.Scale < 0)
            return lattice_index<Notch, Up>(v << (-a.Scale), I{1});
        const auto j = lattice_index<Notch, Up>(v, I{1});
        return Up ? -(-j >> a.Scale) : j >> a.Scale;
    };
    const auto lo = at(y - e), hi = at(y + e);
    if constexpr (2 * W <= Cap)
        if (!(lo == hi))
            return lattice_bound_from<Notch, Up, 2 * W, Cap>(core);
    return lattice_point<Notch>(Up ? hi : lo);
}

template <grid_rational Notch, bool Up, typename Core>
constexpr grid_rational lattice_bound(const Core& core) {
    if constexpr (requires { core.exact(); })
        if (const auto v = core.exact()) {
            return lattice_point<Notch>(lattice_index<Notch, Up>(v->Num, v->Den));
        }
    constexpr int W0 = static_cast<int>(64 * notch_limbs<Notch>) + 16;
    return lattice_bound_from<Notch, Up, W0, 2 * W0>(core);
}

// An In endpoint as an exact input.
template <insidable In>
constexpr exact_frac<input_limbs<In>> grid_input(const grid_rational& r) {
    return exact_of_grid<input_limbs<In>>(r);
}

// ⌈|r|⌉ as an int (deduction bounds), saturated at imax: a grid reaching
// 2^63 must fail every range gate, not wrap past them.
constexpr imax ceil_abs(const grid_rational& r) {
    const grid_wide n = wide_numerator(r), d = wide_denominator(r);
    const grid_wide a = n.negative() ? -n : n;
    const grid_wide c = (a + d - grid_wide{1}) / d;
    constexpr imax  M = std::numeric_limits<imax>::max();
    return grid_wide{M} < c ? M : static_cast<imax>(c);
}

template <insidable In>
inline constexpr imax max_abs_int = ceil_abs(lower_of<In>) > ceil_abs(upper_of<In>) ? ceil_abs(lower_of<In>)
                                                                                    : ceil_abs(upper_of<In>);

// Policy of a deduced output: the input's (no cursor), rounding to nearest.
template <insidable In>
inline constexpr policy_flag auto_policy = (policy_of<In> & ~cursor_marker) | round_nearest;

template <insidable In, grid_rational Lo, grid_rational Hi>
using auto_grid_t = inside<{{Lo, Hi}, notch_of<In>}, auto_policy<In>>;

// The bound of Core's result at In's lower or upper end.
template <insidable In, typename Core, bool AtUpper, bool Up>
inline constexpr grid_rational bound_at =
    lattice_bound<notch_of<In>, Up>(Core{grid_input<In>(AtUpper ? upper_of<In> : lower_of<In>)});

template <insidable In, typename Core>
using increasing_t = auto_grid_t<In, bound_at<In, Core, false, false>, bound_at<In, Core, true, true>>;
template <insidable In, typename Core>
using decreasing_t = auto_grid_t<In, bound_at<In, Core, true, false>, bound_at<In, Core, false, true>>;

// Growth bounds for the deduction: |result| < 2^kmax over In.
template <insidable In>
inline constexpr int exp_kmax = [] {
    static_assert(max_abs_int<In> <= 4096,
                  "beman::inside::math: the deduced output of exp, exp2, sinh and cosh needs |x| <= 4096 - "
                  "name an output grid with fn_into<Out> instead");
    return static_cast<int>(max_abs_int<In> * 3 / 2) + 2;
}();

// Bits of the larger of |b| and 1/|b| over In (b > 0), for pow's growth.
template <insidable In>
inline constexpr int log2_span = [] {
    const imax hi  = ceil_abs(upper_of<In>);
    const imax inv = ceil_abs(grid_rational{1} / lower_of<In>);
    const imax m   = hi > inv ? hi : inv;
    return std::bit_width(static_cast<umax>(m)) + 1;
}();

template <insidable InB, insidable InE>
inline constexpr int pow_kmax = [] {
    static_assert(max_abs_int<InE> * log2_span<InB> <= 1 << 16,
                  "beman::inside::math::pow: the deduced output would pass 2^65536 - name an output grid "
                  "with pow_into<Out> instead");
    return static_cast<int>(max_abs_int<InE>) * log2_span<InB> + 2;
}();

// The magnitude bound of a growing core onto Out: 2^out_kmax, past which
// the core returns a stand-in that every policy but wrap treats alike. A
// wrap Out folds the true value back, so the bound must cover the result
// over the whole input: Grow::bits, read only then (it may not exist for
// inputs that other outputs take, such as a base reaching 0).
template <insidable Out, typename Grow>
inline constexpr int grow_kmax = [] {
    if constexpr (has_flag(policy_of<Out>, wrap)) {
        constexpr int g = Grow::bits;
        return g > out_kmax<Out> ? g : out_kmax<Out>;
    } else
        return out_kmax<Out>;
}();
template <insidable In>
struct exp_grow {
    static constexpr int bits = exp_kmax<In>;
};
template <insidable InB, insidable InE>
struct pow_grow {
    // A base reaching 0 grows only through a negative exponent.
    static_assert(lower_of<InB> > 0 || !(lower_of<InE> < 0),
                  "beman::inside::math::pow: a wrap output needs the whole result, which has no bound for a "
                  "base reaching 0 and a negative exponent");
    static constexpr int bits = [] {
        if constexpr (lower_of<InB> > 0)
            return pow_kmax<InB, InE>;
        else {
            static_assert(max_abs_int<InE> * (std::bit_width(static_cast<umax>(ceil_abs(upper_of<InB>))) + 1) <=
                              1 << 16,
                          "beman::inside::math::pow: a wrap output needs the whole result, which would pass "
                          "2^65536");
            return static_cast<int>(max_abs_int<InE>) *
                       (std::bit_width(static_cast<umax>(ceil_abs(upper_of<InB>))) + 1) +
                   2;
        }
    }();
};
template <insidable In, imax Base>
struct pow_base_grow {
    static_assert(max_abs_int<In> * std::bit_width(static_cast<umax>(Base)) <= 1 << 16,
                  "beman::inside::math::pow_base: a wrap output needs the whole result, which would pass "
                  "2^65536");
    static constexpr int bits = static_cast<int>(max_abs_int<In>) * std::bit_width(static_cast<umax>(Base)) + 2;
};
template <insidable Out, insidable In>
inline constexpr int exp_out_kmax = grow_kmax<Out, exp_grow<In>>;
template <insidable Out, insidable InB, insidable InE>
inline constexpr int pow_out_kmax = grow_kmax<Out, pow_grow<InB, InE>>;
template <insidable Out, insidable In, imax Base>
inline constexpr int pow_base_out_kmax = grow_kmax<Out, pow_base_grow<In, Base>>;

// Bits of x² + y² as one fraction: each square doubles its input's bits,
// and the sum over two denominators adds them.
template <insidable InX, insidable InY>
inline constexpr int hypot_bits = 2 * (input_bits<InX> + input_bits<InY>)+2;

// Whether gcd_notch is a grid number, as a constraint of the deduced
// two-input functions: with 64-bit grid numbers a 2/3 notch against a
// 2^-62 one has no gcd (1/(3·2^62)).
template <insidable A, insidable B>
inline constexpr bool gcd_notch_fits =
    !notched<A> || !notched<B> || grid_gcd_fits(grid_of<A>.value_unit(), grid_of<B>.value_unit());

// gcd of two value units (0 when either is continuous), for two-input
// outputs: the notches, or finer where a lattice does not pass through 0.
// Every value of both inputs is a multiple of it.
template <insidable A, insidable B>
    requires gcd_notch_fits<A, B>
inline constexpr grid_rational gcd_notch =
    !notched<A> || !notched<B> ? grid_rational{0} : grid_gcd_of(grid_of<A>.value_unit(), grid_of<B>.value_unit());
} // namespace beman::inside::math::detail::ax

#endif // BEMAN_INSIDE_DETAIL_MATH_CORES_HPP
