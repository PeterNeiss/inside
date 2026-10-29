// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_CORES_HPP
#define BEMAN_INSIDE_DETAIL_MATH_CORES_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/math_adaptive.hpp>

#include <algorithm>
#include <cstddef>
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
// A grid integer as a double (a notch's p or q, which may pass imax).
constexpr double grid_double(const grid_int& v) { return static_cast<double>(big_rational{v, big_int{1}}); }

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
constexpr wide_rational<input_limbs<In>> exact_input(const In& x) {
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
inline constexpr int in_mag = std::max(grid_magnitude_bits<In>, 1);

// Bits of an exact input's numerator or denominator.
template <std::size_t E>
inline constexpr int fraction_bits = 64 * static_cast<int>(E);

// Result magnitude bound: values of Out lie below 2^out_kmax.
template <insidable Out>
inline constexpr int out_kmax = mag_bits<Out> + 1;

// The start precision for Out: its notch's bits plus guard bits.
template <insidable Out>
inline constexpr int start_bits = std::max(out_bits<Out> + 8, 16);

//---------------------------------------------------------------------------
// Exact-value helpers at a compile-time scale S.
//---------------------------------------------------------------------------
// ⌊√f·2^S⌋ for f ≥ 0 (within 1 unit).
// Bits bounds the numerator's and denominator's bit widths.
template <int S, std::size_t K, int Bits, std::size_t E>
constexpr wide_sint<K> sqrt_exact_q(const wide_rational<E>& f) noexcept {
    using I   = wide_sint<limbs_for_bits(2 * Bits + 2 * S + 2)>;
    const I n = (I{f.Num} * I{f.Den}) << (2 * S);
    return static_cast<wide_sint<K>>(isqrt(n) / I{f.Den});
}

// log x for x > 0 at scale S: x = m·2^e exactly, m in [0.7, 1.42] rounded to
// scale S, log x = log m + e·ln 2.
template <int S, std::size_t K, int Bits, std::size_t E>
constexpr fx<K> log_exact(const wide_rational<E>& x) noexcept {
    using I                  = wide_sint<K>;
    constexpr std::size_t KI = limbs_for_bits(2 * Bits + S + 2);
    int                   e  = floor_log2(x);
    I                     m  = to_q_at<K, KI>(x, S - e);
    if (m > static_cast<I>(sqrt2_q<S>)) {
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

// asin u for |u| ≤ 1/2 at scale S: atan(u·(1/√(1−u²))) with 1 − u² ≥ 3/4
// in fixed point. The quotient is within dt units (|t| ≤ 0.58): 6 for a u
// within ½ (1 − u² then within 1.5), 7 for one within 1 (within 2).
template <int S, std::size_t K>
constexpr fx<K> asin_q(const wide_sint<K>& u, umax dt) noexcept {
    return atan_fixed<S>(mul_q(u, rsqrt_q<S>(one_q<K>(S) - mul_q(u, u, S)), S), dt);
}

//---------------------------------------------------------------------------
// The cores: one per function. Each holds its exact inputs and computes the
// result within its error bound at a precision W (run<W>), plus the exact
// rational results (exact()), which are the only values a rounding boundary
// can hold. Each core's comment names the theorem that makes its list
// complete for rational inputs; every other result is irrational, so the
// driver decides it at some precision (tests/beman/inside/math_exact).
//---------------------------------------------------------------------------
template <std::size_t E>
using maybe_exact = std::optional<wide_rational<E>>;

// deep_extra bits (saturating at the driver's deep_limit): ⌈2^Mag ·
// milli / 1000⌉ for a result that nears a grid point like 2^(−c·|x|),
// |x| < 2^Mag.
constexpr int deep_scaled(int mag, int milli) noexcept {
    return mag >= 20 ? deep_limit : static_cast<int>(((imax{1} << mag) * milli + 999) / 1000);
}

// exp x. Exact only at 0 (Lindemann–Weierstrass: e^x is transcendental for
// a rational x ≠ 0).
template <std::size_t E, int Mag, int KMax>
struct exp_core {
    wide_rational<E> X;
    // e^−|x| near 0 (1.443·|x| bits), e^x = 1 + x + x²/2 near 1 + x.
    static constexpr int     deep_extra = std::max(deep_scaled(Mag, 1443), 2 * fraction_bits<E>);
    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{exact_of<E>(1)} : std::nullopt; }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 8 + KMax;
        using I                 = fixed_t<S + Mag + 2>; // |x|·log2(e) < 2^(S+Mag+1)
        constexpr std::size_t K = limbs_of<I>;
        return exp_fixed<S>(to_q<S, K>(X), 1, KMax);
    }
};

// 2^x = 2^k·e^(f·ln 2), x = k + f. Exact at integers, where run() has no
// error; 2^(p/q) is irrational for q ∤ p.
template <std::size_t E, int Mag, int KMax>
struct exp2_core {
    wide_rational<E> X;
    // 2^−|x| near 0 (|x| bits), 2^x near 1 for a small x.
    static constexpr int deep_extra = std::max(deep_scaled(Mag, 1000), 2 * fraction_bits<E>);
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 8 + KMax;
        using I                 = fixed_t<S + 8>;
        using J                 = wide_sint<E>;
        constexpr std::size_t K = limbs_of<I>;
        const J               k = rounded_div<round_mode::floor>(X.Num, X.Den);
        if (J{KMax} < k)
            return approx<K>::beyond(false, KMax);
        if (k < J{-(S + 2)})
            return approx<K>::below(S);
        int                    kk = static_cast<int>(static_cast<imax>(k));
        const wide_rational<E> f{X.Num - k * X.Den, X.Den}; // in [0, 1)
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

// log x, x > 0. Exact at 1 (Lindemann–Weierstrass).
template <std::size_t E, int Bits = fraction_bits<E>>
struct log_core {
    wide_rational<E>         X;
    constexpr maybe_exact<E> exact() const { return is_one(X) ? maybe_exact<E>{exact_of<E>(0)} : std::nullopt; }
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
constexpr std::optional<imax> exact_log(const wide_rational<E>& x, int B) noexcept {
    using I = wide_sint<E>;
    if (x.Num.negative() || x.Num.is_zero())
        return std::nullopt;
    I          n = x.Num, d = x.Den;
    const imax a = strip_factor(n, 2) - strip_factor(d, 2);
    if (B == 10 && strip_factor(n, 5) - strip_factor(d, 5) != a)
        return std::nullopt;
    if (!(n == d))
        return std::nullopt;
    return a;
}

// log2 x and log10 x: log x / ln B. Exact at the integer powers of B: B is
// not a perfect power, so B^(p/q) is irrational for q ∤ p.
template <std::size_t E, int B, int Bits = fraction_bits<E>>
struct logb_core {
    wide_rational<E>         X;
    constexpr maybe_exact<E> exact() const {
        if (const auto k = exact_log(X, B))
            return exact_of<E>(*k);
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

// √x, x ≥ 0: ⌊√x·2^P⌋ exactly. Exact when x is a square of a rational (else
// irrational, by the rational root theorem).
template <std::size_t E, int Bits = fraction_bits<E>>
struct sqrt_core {
    wide_rational<E> X;
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
        return wide_rational<E>{static_cast<wide_sint<E>>(s), X.Den};
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

// ∛x: ⌊∛(n·d²·2^(3P))⌋/d exactly. Exact when x is a cube of a rational (else
// irrational, by the rational root theorem).
template <std::size_t E, int Bits = fraction_bits<E>>
struct cbrt_core {
    wide_rational<E> X;
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
        return wide_rational<E>{X.Num.negative() ? -r : r, X.Den};
    }
    template <int W>
    constexpr auto run() const {
        constexpr int P          = W + 2;
        using I                  = fixed_t<Bits + P + 4>;
        constexpr std::size_t K  = limbs_of<I>;
        using J                  = wide_sint<limbs_for_bits(3 * Bits + 3 * P + 4)>;
        const wide_rational<E> a = abs(X);
        const J                n = (J{a.Num} * J{a.Den} * J{a.Den}) << (3 * P);
        const I                y = static_cast<I>(icbrt(n) / J{a.Den});
        return approx<K>{X.Num.negative() ? -y : y, P, 1};
    }
};

// sin, cos and tan: x = k·π/2 + r, |r| ≤ π/4, then the series and the
// quadrant. Exact at 0 (Lindemann–Weierstrass); the angles in degrees and
// turns, exact at Niven's values, are units_math.hpp's.
enum class trig { sin, cos, tan };

template <std::size_t E, int Mag, trig Fn, int KMax>
struct trig_core {
    wide_rational<E> X;
    // sin x = x − x³/6 near x, cos x = 1 − x²/2 near 1 (tan: none).
    static constexpr int     deep_extra = Fn == trig::sin   ? 3 * fraction_bits<E>
                                          : Fn == trig::cos ? 2 * fraction_bits<E>
                                                            : 0;
    constexpr maybe_exact<E> exact() const {
        if (!is_zero(X))
            return std::nullopt;
        return exact_of<E>(Fn == trig::cos ? 1 : 0);
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
        // cos is sin one quadrant on.
        auto quad = [&](unsigned p) {
            const fx<K> f = p & 1u ? cos_series<S>(r, 2) : sin_series<S>(r, 2);
            return p & 2u ? fx<K>{-f.Value, f.Error} : f;
        };
        if constexpr (Fn != trig::tan) {
            const fx<K> v = quad(Fn == trig::sin ? q : q + 1);
            return approx<K>{v.Value, S, v.Error};
        } else {
            const fx<K> sn = quad(q), cs = quad(q + 1);
            // tan = sin/cos. The error bound, (δs + |t|·δc)/(|c| − δc) + 2 units,
            // and the test for a result past 2^KMax only need to be upper and
            // lower bounds: computed in doubles, widened by a hair.
            const double cd = static_cast<double>(cs.Value), sd = static_cast<double>(sn.Value);
            const double ac = cd < 0 ? -cd : cd, as = sd < 0 ? -sd : sd;
            const double ec = static_cast<double>(cs.Error), es = static_cast<double>(sn.Error);
            if (!(2 * ec < ac))
                return approx<K>::unresolved(S); // cos unresolved
            const double unit = ::beman::inside::detail::ldexp(1.0, S);
            if ((as - es) / (ac + ec) * (1 - 0x1p-40) > ::beman::inside::detail::ldexp(1.0, KMax))
                return approx<K>::beyond(sn.Value.negative() != cs.Value.negative(), KMax);
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
constexpr fx<K> atan_exact(const wide_rational<E>& x) noexcept {
    using I = wide_sint<K>;
    if (!(exact_of<E>(1) < abs(x)))
        return atan_fixed<S>(to_q<S, K>(x), 1);
    const fx<K> a  = atan_fixed<S>(to_q<S, K>(inverse(x)), 1);
    const I     hp = static_cast<I>(pi_q<S - 1>);
    return {(x.Num.negative() ? -hp : hp) - a.Value, a.Error + 1};
}

template <std::size_t E>
struct atan_core {
    wide_rational<E>         X;
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
// Every other result is irrational (Lindemann–Weierstrass), ±π and ±π/2 too.
template <std::size_t E>
struct atan2_core {
    wide_rational<E>         Y, X;
    constexpr maybe_exact<E> exact() const {
        if (is_zero(Y) && !X.Num.negative())
            return exact_of<E>(0);
        return std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S         = W + 10;
        using I                 = fixed_t<S + 8>;
        constexpr std::size_t K = limbs_of<I>;
        using F                 = wide_rational<2 * E + 1>;
        const F y{Y}, x{X};
        const I pi_fixed = static_cast<I>(pi_q<S>);
        if (!(abs(x) < abs(y))) {
            const fx<K> a = atan_exact<S, K>(y / x);
            if (!x.Num.negative())
                return approx<K>{a.Value, S, a.Error};
            return approx<K>{a.Value + (y.Num.negative() ? -pi_fixed : pi_fixed), S, a.Error + 1};
        }
        const fx<K> a  = atan_exact<S, K>(x / y);
        const I     hp = static_cast<I>(pi_q<S - 1>);
        return approx<K>{(y.Num.negative() ? -hp : hp) - a.Value, S, a.Error + 1};
    }
};

// asin x (Acos false) or acos x (true), |x| ≤ 1. For |x| ≤ 1/2 asin
// directly, acos = π/2 − asin x. Past 1/2 the half-angle forms, accurate up
// to ±1, both from a = asin √((1−|x|)/2): asin = ±(π/2 − 2a); acos = 2a above
// 0, π − 2a below. asin is exact at 0, acos at 1 (Lindemann–Weierstrass).
template <std::size_t E, int Bits, bool Acos>
struct arcsin_core {
    wide_rational<E>         X;
    constexpr maybe_exact<E> exact() const {
        if constexpr (Acos)
            return is_one(X) ? maybe_exact<E>{exact_of<E>(0)} : std::nullopt;
        else
            return is_zero(X) ? maybe_exact<E>{X} : std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S           = W + 12;
        using I                   = fixed_t<S + 8>;
        constexpr std::size_t  K  = limbs_of<I>;
        const I                hp = static_cast<I>(pi_q<S - 1>); // π/2
        const wide_rational<E> half{wide_sint<E>{1}, wide_sint<E>{2}};
        if (!(half < abs(X))) {
            const fx<K> a = asin_q<S>(to_q<S, K>(X), 6);
            if constexpr (Acos)
                return approx<K>{hp - a.Value, S, a.Error + 1};
            else
                return approx<K>{a.Value, S, a.Error};
        }
        using F         = wide_rational<E + 1>;
        const F     y   = (exact_of<E + 1>(1) + F{-abs(X)}) * F{half}; // (1 − |x|)/2
        const fx<K> a   = asin_q<S>(sqrt_exact_q<S, K, Bits + 3>(y), 7);
        const bool  neg = X.Num.negative();
        if constexpr (Acos) {
            if (!neg)
                return approx<K>{a.Value << 1, S, 2 * a.Error};
            return approx<K>{static_cast<I>(pi_q<S>) - (a.Value << 1), S, 2 * a.Error + 1};
        } else {
            const I v = hp - (a.Value << 1);
            return approx<K>{neg ? -v : v, S, 2 * a.Error + 1};
        }
    }
};
template <std::size_t E, int Bits = fraction_bits<E>>
using asin_core = arcsin_core<E, Bits, false>;
template <std::size_t E, int Bits = fraction_bits<E>>
using acos_core = arcsin_core<E, Bits, true>;

// sinh, cosh and tanh from e^|x| and e^−|x| at absolute scale A. Exact at 0
// (sinh, tanh: 0; cosh: 1); e^x is transcendental elsewhere.
enum class hyp { sinh, cosh, tanh };

template <std::size_t E, int Mag, hyp Fn, int KMax>
struct hyp_core {
    wide_rational<E> X;
    // tanh |x| = 1 − 2e^−2|x| near 1 (2.886·|x| bits) and x − x³/3 near x;
    // sinh near x by x³/6, cosh near 1 by x²/2.
    static constexpr int     deep_extra = Fn == hyp::tanh   ? std::max(deep_scaled(Mag, 2886), 3 * fraction_bits<E>)
                                          : Fn == hyp::sinh ? 3 * fraction_bits<E>
                                                            : 2 * fraction_bits<E>;
    constexpr maybe_exact<E> exact() const {
        if (!is_zero(X))
            return std::nullopt;
        return Fn == hyp::cosh ? exact_of<E>(1) : X;
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
                return approx<K>::beyond(Fn == hyp::sinh && neg, KMax);
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
// atanh x = ±½·log((1+|x|)/(1−|x|)), |x| < 1. Exact at 0 (acosh: 1): each
// is a log of an algebraic number ≠ 1 elsewhere (Lindemann–Weierstrass).
enum class ahyp { asinh, acosh, atanh };

template <std::size_t E, int Mag, ahyp Fn, int Bits = fraction_bits<E>>
struct ahyp_core {
    wide_rational<E>         X;
    constexpr maybe_exact<E> exact() const {
        if (Fn == ahyp::acosh)
            return is_one(X) ? maybe_exact<E>{exact_of<E>(0)} : std::nullopt;
        return is_zero(X) ? maybe_exact<E>{X} : std::nullopt;
    }
    template <int W>
    constexpr auto run() const {
        constexpr int S = W + 12 + std::bit_width(static_cast<unsigned>(2 * Bits + Mag));
        // asinh forms x² + 1 in the word: twice the magnitude bits.
        using I                 = fixed_t<S + (Fn == ahyp::asinh ? 2 * Mag : Mag) + 8>;
        constexpr std::size_t K = limbs_of<I>;
        using F                 = wide_rational<2 * E + 1>;
        const bool neg          = X.Num.negative();
        const F    a            = abs(F{X});
        if constexpr (Fn == ahyp::atanh) {
            // (1 + |x|)/(1 − |x|) = (d + |n|)/(d − |n|), d − |n| > 0.
            const F     q{a.Num + a.Den, a.Den - a.Num};
            const fx<K> l = log_exact<S, K, 2 * Bits + 2>(q);
            return approx<K>{neg ? -(l.Value >> 1) : (l.Value >> 1), S, l.Error / 2 + 1};
        } else if constexpr (Fn == ahyp::asinh) {
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
};

// b^e for b > 0: e^(e·log b). Exact for e = 0, b = 1, and e = p/q (q ≤ 64)
// when b's numerator and denominator are perfect q-th powers (4^0.5 = 2,
// 2.25^1.5 = 3.375) and the power fits pow_limbs: such a result can sit on a
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
    const P p = pow_by_squaring(r, static_cast<unsigned>(q), [](const P& x, const P& y) { return x * y; }, P{1});
    return p == n ? std::optional<P>{r} : std::nullopt;
}

// b^(p/q), q > 0 and p/q reduced, is rational exactly when b's numerator and
// denominator are perfect q-th powers (the rational root theorem). The size
// limits leave out only results no 64-bit grid can hold as a boundary: q > 64
// needs a part of b past 2^64, and a power past 2040 bits is far beyond any
// such grid's range or notch.
template <std::size_t EB, std::size_t EE>
constexpr std::optional<wide_rational<pow_limbs>> exact_pow(const wide_rational<EB>& b,
                                                            const wide_rational<EE>& e) noexcept {
    using P = wide_sint<pow_limbs>;
    if (is_zero(e) || is_one(b))
        return wide_rational<pow_limbs>{P{1}, P{1}};
    const wide_rational<EE> re = reduced(e); // p/q, q > 0
    if (bit_width_of(re.Den) > 7 || 64 < static_cast<imax>(re.Den))
        return std::nullopt;
    const int               q  = static_cast<int>(static_cast<imax>(re.Den));
    const auto              k  = re.Num;
    const auto              ak = abs(k);
    const wide_rational<EB> r  = reduced(b);
    // b^(1/q) first: both parts perfect q-th powers, else irrational.
    const auto rn = exact_root(P{r.Num}, q), rd = exact_root(P{r.Den}, q);
    if (!rn || !rd)
        return std::nullopt;
    const int bits = bit_width_of(*rn) + bit_width_of(*rd);
    if (bit_width_of(ak) > 12 || static_cast<imax>(ak) * bits > 64 * static_cast<imax>(pow_limbs) - 8)
        return std::nullopt;
    const auto     mul   = [](const P& x, const P& y) { return x * y; };
    const unsigned power = static_cast<unsigned>(static_cast<imax>(ak));
    const P        n = pow_by_squaring(*rn, power, mul, P{1}), d = pow_by_squaring(*rd, power, mul, P{1});
    return k.negative() ? inverse(wide_rational<pow_limbs>{n, d}) : wide_rational<pow_limbs>{n, d};
}

// ConstBase, when nonzero, is the base known at compile time (pow_base): its
// log is then a constant of each precision.
template <std::size_t EB,
          std::size_t EE,
          int         MagE,
          int         KMax,
          int         BitsB     = fraction_bits<EB>,
          int         BitsE     = fraction_bits<EE>,
          imax        ConstBase = 0>
struct pow_core {
    wide_rational<EB>                                 B;
    wide_rational<EE>                                 Exp;
    constexpr std::optional<wide_rational<pow_limbs>> exact() const { return exact_pow(B, Exp); }
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
                constexpr fx<K> c = log_exact<SL, K, BitsB>(exact_of<EB>(ConstBase));
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
    return grid_rational{big_int{k}} * Notch;
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
constexpr wide_rational<input_limbs<In>> grid_input(const grid_rational& r) {
    return exact_of_grid<input_limbs<In>>(r);
}

// ⌈|r|⌉ as an int (deduction bounds), saturated at imax: a grid reaching
// 2^63 must fail every range gate, not wrap past them.
constexpr imax ceil_abs(const grid_rational& r) {
    const grid_int n = wide_numerator(r), d = wide_denominator(r);
    const grid_int a = abs(n);
    const grid_int c = (a + d - grid_int{1}) / d;
    constexpr imax M = std::numeric_limits<imax>::max();
    return grid_int{M} < c ? M : static_cast<imax>(c);
}

template <insidable In>
inline constexpr imax max_abs_int = std::max(ceil_abs(lower_of<In>), ceil_abs(upper_of<In>));

// Policy of a deduced output: the input's (no cursor), rounding to nearest.
template <insidable In>
inline constexpr policy_flag result_policy = (flags_of<In> & ~cursor_marker) | round_nearest;

template <insidable In, grid_rational Lo, grid_rational Hi>
using result_grid_t = inside<{{Lo, Hi}, notch_of<In>}, result_policy<In>>;

// The bound of Core's result at In's lower or upper end.
template <insidable In, typename Core, bool AtUpper, bool Up>
inline constexpr grid_rational bound_at =
    lattice_bound<notch_of<In>, Up>(Core{grid_input<In>(AtUpper ? upper_of<In> : lower_of<In>)});

template <insidable In, typename Core>
using increasing_t = result_grid_t<In, bound_at<In, Core, false, false>, bound_at<In, Core, true, true>>;
template <insidable In, typename Core>
using decreasing_t = result_grid_t<In, bound_at<In, Core, true, false>, bound_at<In, Core, false, true>>;

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
    const imax m = std::max(ceil_abs(upper_of<In>), ceil_abs(grid_rational{1} / lower_of<In>));
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
    if constexpr (has_flag(flags_of<Out>, wrap))
        return std::max(Grow::bits, out_kmax<Out>);
    else
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
            constexpr int b = std::bit_width(static_cast<umax>(ceil_abs(upper_of<InB>))) + 1;
            static_assert(max_abs_int<InE> * b <= 1 << 16,
                          "beman::inside::math::pow: a wrap output needs the whole result, which would pass "
                          "2^65536");
            return static_cast<int>(max_abs_int<InE>) * b + 2;
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

// gcd of two value units (0 when either is continuous), for two-input
// outputs: the notches, or finer where a lattice does not pass through 0.
// Every value of both inputs is a multiple of it.
template <insidable A, insidable B>
inline constexpr grid_rational gcd_notch =
    !notched<A> || !notched<B> ? grid_rational{0} : gcd(grid_of<A>.value_unit(), grid_of<B>.value_unit());
} // namespace beman::inside::math::detail::ax

#endif // BEMAN_INSIDE_DETAIL_MATH_CORES_HPP
