// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The double kernels of the math engine's double tier (cmath_adaptive.hpp):
// a small libm in `double` — Taylor polynomials in Horner form with explicit
// std::fma, Cody-Waite range reduction, and only std::fma / sqrt / nearbyint
// from <cmath>.
//
// A kernel takes a target T in bits: its polynomials get the fewest terms
// whose truncation stays below 2^-T relative, and its error bound for that
// size is proved at compile time from the coefficients and argument range —
// the first omitted term, each Horner step's rounding, the constants' and the
// reduction's roundings. These are first-order bounds of IEEE round-to-nearest
// (each operation within kU = 2^-53, relatively); up() widens each by 2^-30 of
// itself for the second-order terms and the bound arithmetic's roundings.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_MATH_FP_HPP
#define BEMAN_INSIDE_DETAIL_MATH_FP_HPP

#include <beman/inside/math.hpp>   // beman::inside::detail::ldexp (constexpr, reproducible)
#include <beman/inside/inside.hpp> // complete inside/rational + has_flag/policy_of (store<>)

// BEMAN_INSIDE_MATH_NO_FP is resolved in policy_flag.hpp (included via inside.hpp).

#ifndef BEMAN_INSIDE_MATH_NO_FP // ===== FP engine present (needs <cmath> + an FPU) =====

    #include <array>
    #include <cmath> // std::fma, std::sqrt, std::nearbyint ONLY
    #include <cstddef>
    #include <utility>

namespace beman::inside::math::detail::fp {
using std::fma;

// c0·z^n + c1·z^(n-1) + … + cn as an fma chain from the highest coefficient
// down (Horner) — the same operation order as writing the chain out by hand.
template <std::floating_point T, typename... C>
[[gnu::always_inline]] inline T horner(T z, T c0, C... cs) {
    T p = c0;
    ((p = fma(p, z, static_cast<T>(cs))), ...);
    return p;
}

// The same over a coefficient array, highest degree first.
template <std::size_t N>
[[gnu::always_inline]] inline double horner(double z, const std::array<double, N>& c) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        double p = c[0];
        ((p = fma(p, z, c[I + 1])), ...);
        return p;
    }(std::make_index_sequence<N - 1>{});
}

inline constexpr double kHalfPiHi  = 0x1.921fb54442d18p+0;  // π/2  high
inline constexpr double kHalfPiLo  = 0x1.1a62633145c07p-54; // π/2  low
inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;  // 2/π
inline constexpr double kLn2Hi     = 0x1.62e42fee00000p-1;  // ln2  high (33 bits)
inline constexpr double kLn2Lo     = 0x1.a39ef35793c76p-33; // ln2  low
inline constexpr double kLog2e     = 0x1.71547652b82fep+0;  // 1/ln2
inline constexpr double kLn2Full   = 0x1.62e42fefa39efp-1;  // ln2
inline constexpr double kLog10e    = 0x1.bcb7b1526e50ep-2;  // 1/ln10
inline constexpr double kSqrtHalf  = 0x1.6a09e667f3bcdp-1;  // √½
inline constexpr double kPi        = 0x1.921fb54442d18p+1;  // π
inline constexpr double kPiHalf    = 0x1.921fb54442d18p+0;  // π/2
inline constexpr double kPiSixth   = 0x1.0c152382d7366p-1;  // π/6
inline constexpr double kInvSqrt3  = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
inline constexpr double kTanPi12   = 0x1.126145e9ecd56p-2;  // tan(π/12) ≈ 0.2679
inline constexpr double kThird     = 1.0 / 3.0;
inline constexpr double kThirdLo   = 0x1.5555555555555p-56; // 1/3 − kThird = 2^-54/3

// Under -ffp-contract=fast (GCC's default; Clang contracts only within one
// expression) a product feeding a later sum may be fused into an fma, which
// adds the exact product where an error-free transformation expects the
// rounded one. fenced() keeps an operand of an error-free sum out of that, at
// no runtime cost.
constexpr double fenced(double x) noexcept {
    #if defined(__has_builtin)
        #if __has_builtin(__builtin_assoc_barrier)
    return __builtin_assoc_barrier(x);
        #elif __has_builtin(__arithmetic_fence)
    return __arithmetic_fence(x);
        #else
    return x;
        #endif
    #else
    return x;
    #endif
}

// The product rounded once, which the compiler cannot fuse further:
// fma(a, b, +0) (+0 keeps it from folding back to a·b), one instruction with
// hardware FMA; without it nothing contracts.
inline double rounded_product(double a, double b) noexcept {
    #if defined(__FMA__) || defined(__ARM_FEATURE_FMA)
    return std::fma(a, b, 0.0);
    #else
    return a * b;
    #endif
}

// A value as the unevaluated sum Hi + Lo of two doubles, and the error-free
// sums that make one (the dd tier's arithmetic builds on them).
struct hilo {
    double Hi;
    double Lo;
};

constexpr hilo two_sum(double a, double b) noexcept {
    a              = fenced(a);
    b              = fenced(b);
    const double s = a + b, bb = s - a;
    return {s, (a - (s - bb)) + (b - bb)};
}

// |a| ≥ |b| (or a == 0).
constexpr hilo fast_two_sum(double a, double b) noexcept {
    a              = fenced(a);
    b              = fenced(b);
    const double s = a + b;
    return {s, b - (s - a)};
}

//---------------------------------------------------------------------------
// The bound arithmetic.
//---------------------------------------------------------------------------
inline constexpr double kU = 0x1p-53; // unit roundoff
// Absolute slack for results near the bottom of the double range (a
// subnormal or underflowing ldexp, a square below 2^-1022): far below
// every grid the tier serves.
inline constexpr double kTiny = 0x1p-500;

// How far each constant is from its value (computed offline at 300 bits,
// rounded up).
inline constexpr double kHalfPiRes   = 0x1p-109;       // |π/2 − Hi − Lo|      (2^-109.04)
inline constexpr double kLn2Res      = 0x1p-86;        // |ln2 − Hi − Lo|      (2^-86.15)
inline constexpr double kLn2FullErr  = 1.25 * 0x1p-55; // relative          (2^-54.73)
inline constexpr double kLog2eErr    = 1.02 * 0x1p-56; // relative          (2^-55.98)
inline constexpr double kLog10eErr   = 1.0 * 0x1p-55;  // relative          (2^-55.13)
inline constexpr double kPiErr       = 1.11 * 0x1p-53; // absolute          (2^-52.86)
inline constexpr double kPiHalfErr   = 1.11 * 0x1p-54; // absolute          (2^-53.86)
inline constexpr double kPiSixthErr  = 1.0 * 0x1p-54;  // absolute          (2^-54.05)
inline constexpr double kInvSqrt3Err = 1.21 * 0x1p-55; // absolute          (2^-54.73)

constexpr double up(double b) { return b * (1 + 0x1p-30); }
constexpr double pow_n(double x, int n) {
    double r = 1;
    for (int i = 0; i < n; ++i)
        r *= x;
    return r;
}
constexpr double factorial(int n) {
    double r = 1;
    for (int i = 2; i <= n; ++i)
        r *= i;
    return r;
}
constexpr double max_d(double a, double b) { return a > b ? a : b; }

// A polynomial's coefficients, highest degree first, each with its
// relative distance from the exact rational it stands for.
template <std::size_t N>
struct poly {
    std::array<double, N> C;
    std::array<double, N> Err;
};

// Σ_{k<N} sign(k)/den(k)·z^k: each coefficient one correctly rounded
// division of integers below 2^53, so within kU of its value (exact when
// den is a power of two).
template <std::size_t N, typename F>
consteval poly<N> series(F term) {
    poly<N> p{};
    for (std::size_t k = 0; k < N; ++k) {
        const auto [sign, den] = term(static_cast<int>(k));
        if (!(den < 0x1p53))
            std::unreachable(); // a denominator must be a double exactly
        int        e     = 0;
        const bool pow2  = ::beman::inside::detail::frexp(den, &e) == 0.5;
        p.C[N - 1 - k]   = sign / den;
        p.Err[N - 1 - k] = pow2 ? 0.0 : kU;
    }
    return p;
}

struct term {
    double Sign, Den;
};

// Horner's evaluation of p (one fma per step) for |z| ≤ Z, where the z it
// is given lies within relative Dz of the exact argument: Mag bounds |p|
// and Err the distance of the computed value from p at the exact argument.
// Step k's value lies within Err of p_k = p_(k−1)·z + c_k; the fma's
// rounding adds kU of it.
struct horner_bound {
    double Mag, Err;
};

template <std::size_t N>
consteval horner_bound horner_error(const poly<N>& p, double Z, double Dz) {
    auto   abs = [](double v) { return v < 0 ? -v : v; };
    double P = abs(p.C[0]), E = abs(p.C[0]) * p.Err[0];
    for (std::size_t k = 1; k < N; ++k) {
        E = E * Z * (1 + Dz) + P * Z * Dz + abs(p.C[k]) * p.Err[k];
        P = P * Z + abs(p.C[k]);
        E += kU * (P + E);
    }
    return {P, E};
}

// The fewest terms n ≤ Max whose truncation trunc(n) is at most 2^-T.
template <typename F>
consteval int terms_for(int T, int Max, F trunc) {
    double target = 1;
    for (int i = 0; i < T; ++i)
        target *= 0.5;
    for (int n = 1; n < Max; ++n)
        if (trunc(n) <= target)
            return n;
    return Max;
}

// The target of the full kernels: truncation below 2^-58, a thirty-second
// of their rounding error.
inline constexpr int kFullBits = 58;

// sinh, asinh and acosh have a sharp form, which reaches 47 bits but costs
// 15–40% more, and a plain one, which reaches 46: targets up to kPlainBits
// (fp_target = bits + 10) take the plain one.
inline constexpr int kPlainBits = 56;

//---------------------------------------------------------------------------
// sin and cos on the reduced argument r, |r| ≤ kRTrig. The reduction of
// |x| ≤ 2^20 leaves |r| ≤ π/4 + 2^-32 (x·2/π is within 2^-32.6 of exact).
//---------------------------------------------------------------------------
inline constexpr double kRTrig  = 0x1.9220p-1; // 0.78540039 ≥ π/4 + 2^-30
inline constexpr double kZTrig  = kRTrig * kRTrig;
inline constexpr double kSinMin = 1 - kZTrig / 6; // ≤ sin r / r
inline constexpr double kCosMin = 1 - kZTrig / 2 + kZTrig * kZTrig / 24 - kZTrig * kZTrig * kZTrig / 720; // ≤ cos r

// sin r = r·Σ (−1)^k z^k/(2k+1)!; cos r = Σ (−1)^k z^k/(2k)!, z = r². Both
// series alternate with falling terms, so the first omitted term bounds
// the tail.
constexpr double sin_trunc(int n) { return pow_n(kZTrig, n) / factorial(2 * n + 1) / kSinMin; }
constexpr double cos_trunc(int n) { return pow_n(kZTrig, n) / factorial(2 * n) / kCosMin; }
template <int T>
inline constexpr int sin_terms = terms_for(T, 9, sin_trunc);
template <int T>
inline constexpr int cos_terms = terms_for(T, 10, cos_trunc);

template <int N>
inline constexpr poly<N> sin_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, factorial(2 * k + 1)}; });
template <int N>
inline constexpr poly<N> cos_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, factorial(2 * k)}; });

// Relative errors of sin_poly and cos_poly at the r they are given: the
// Horner error and the truncation over the least |sin r / r| or cos r,
// and for sin the final product's rounding.
template <int N>
inline constexpr double sin_poly_rel = [] {
    const horner_bound h = horner_error(sin_c<N>, kZTrig, kU);
    return up((h.Err + sin_trunc(N) * kSinMin) / kSinMin + kU);
}();
template <int N>
inline constexpr double cos_poly_rel = [] {
    const horner_bound h = horner_error(cos_c<N>, kZTrig, kU);
    return up((h.Err + cos_trunc(N) * kCosMin) / kCosMin);
}();

template <int N>
[[gnu::always_inline]] inline double sin_poly(double r) {
    const double z = r * r;
    return r * horner(z, sin_c<N>.C);
}

template <int N>
[[gnu::always_inline]] inline double cos_poly(double r) {
    return horner(r * r, cos_c<N>.C);
}

// Quadrant reduction: x → r = x − k·π/2 and q = k mod 4. r1 = x − k·Hi is
// exact: k ≠ 0 needs |x| > ½, so both terms are multiples of 2^-53, and
// |r1| < 1. The second fma rounds once, and Hi + Lo misses π/2 by
// kHalfPiRes: r is within kU·|r| + |k|·kHalfPiRes of x − k·π/2.
inline double reduce_quadrant(double x, long& q, double& k) {
    k        = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r        = fma(-k, kHalfPiLo, r);
    q        = static_cast<long>(k) & 3;
    return r;
}

// sin, cos and tan of |x| ≤ 2^20, sized to T bits. The reduction's kU·|r|
// moves sin r by kU·|r| ≤ kU·|sin r|/kSinMin and cos r by kU·r² ≤
// kU·kZTrig/cos r, relatively; its |k|·kHalfPiRes is absolute, with
// |k| ≤ 1.2·max(1, |x|). So each value is within Rel·|v| + AbsX·max(1, |x|).
template <int T>
struct trig_k {
    static constexpr int    NS = sin_terms<T>, NC = cos_terms<T>;
    static constexpr double Rel = up(max_d(sin_poly_rel<NS> + kU / kSinMin, cos_poly_rel<NC> + kU * kZTrig / kCosMin));
    static constexpr double AbsX = up(1.2 * kHalfPiRes);
    // tan = s/c or −c/s: both relative errors and the division's; the
    // reduction's kU·|r| moves tan r by 2kU·|r|/|sin 2r| ≤ 1.571kU of it,
    // and |k|·kHalfPiRes by sec² = 1 + t² times itself.
    static constexpr double TanRel = up(sin_poly_rel<NS> + cos_poly_rel<NC> + kU + 1.5710 * kU);

    // sin x is sin, cos, −sin, −cos of r in quadrants 0…3; cos x is sin x one
    // quadrant on.
    static double quadrant(double x, long shift, double& bound) {
        long         q;
        double       k;
        const double r = reduce_quadrant(x, q, k);
        q              = (q + shift) & 3;
        const double p = q & 1 ? cos_poly<NC>(r) : sin_poly<NS>(r);
        const double v = q & 2 ? -p : p;
        bound          = Rel * __builtin_fabs(v) + AbsX * (__builtin_fabs(x) > 1 ? __builtin_fabs(x) : 1.0) + kTiny;
        return v;
    }
    static double sin(double x, double& bound) { return quadrant(x, 0, bound); }
    static double cos(double x, double& bound) { return quadrant(x, 1, bound); }

    // False on a pole (odd quadrant with s == 0).
    static bool tan(double x, double& t, double& bound) {
        long         q;
        double       k;
        const double r = reduce_quadrant(x, q, k);
        const double s = sin_poly<NS>(r), c = cos_poly<NC>(r);
        if (q & 1) {
            if (s == 0.0)
                return false;
            t = -c / s;
        } else
            t = s / c;
        const double mx = __builtin_fabs(x) > 1 ? __builtin_fabs(x) : 1.0;
        bound           = TanRel * __builtin_fabs(t) + (1 + t * t) * AbsX * mx + kTiny;
        return true;
    }
};

//---------------------------------------------------------------------------
// e^r on |r| ≤ kRExp: Σ r^k/k!, whose tail is at most r^n/n!·e^|r|.
//---------------------------------------------------------------------------
inline constexpr double kRExp   = 0x1.63p-2; // 0.34668 ≥ ln2/2 + 2^-40
inline constexpr double kExpMax = 1.4144;    // ≥ e^kRExp
constexpr double        exp_trunc(int n) { return pow_n(kRExp, n) / factorial(n) * kExpMax * kExpMax; }
template <int T>
inline constexpr int exp_terms = terms_for(T, 15, exp_trunc);

template <int N>
inline constexpr poly<N> exp_c = series<N>([](int k) { return term{1.0, factorial(k)}; });

// Relative error of exp_poly at the r it is given (e^r ≥ 1/kExpMax).
template <int N>
inline constexpr double exp_poly_rel = [] {
    const horner_bound h = horner_error(exp_c<N>, kRExp, 0);
    return up(h.Err * kExpMax + exp_trunc(N));
}();

template <int N>
[[gnu::always_inline]] inline double exp_poly(double r) {
    return horner(r, exp_c<N>.C);
}

// sinh a = a + a·z·P(z) for 0 ≤ a < ln 2, z = a² ≤ kZSinh, P = Σ z^k/(2k+3)!.
// Relative to sinh a ≥ a: z·P's truncation (the first omitted term,
// widened by 1% for the rest), z and a·z rounding (2kU of z·P), P's
// Horner error at z, and the fma's rounding.
inline constexpr double kZSinh = 0.4805; // ≥ (ln 2)² = 0.480453
constexpr double        sinh_trunc(int n) { return pow_n(kZSinh, n + 1) / factorial(2 * n + 3) * 1.01; }
template <int T>
inline constexpr int sinh_terms = terms_for(T, 9, sinh_trunc);

template <int N>
inline constexpr poly<N> sinh_c = series<N>([](int k) { return term{1.0, factorial(2 * k + 3)}; });

template <int N>
inline constexpr double sinh_poly_rel = [] {
    const horner_bound h = horner_error(sinh_c<N>, kZSinh, kU);
    return up(kZSinh * (2 * kU * h.Mag + h.Err) + sinh_trunc(N) + kU);
}();

// e^x = 2^k·e^r, x = k·ln2 + r, for |x| ≤ 745 (|k| ≤ 1100) — larger |x|
// overflow to infinity or underflow to 0, both within the bound's kTiny
// or failing every test downstream. r1 = x − k·Hi is exact (Hi has 33
// bits, k at most 11, and x − k·Hi is a multiple of 2^-54 below ½); the
// second fma rounds once (kU·|r|, moving e^r by kU·kRExp relatively) and
// Hi + Lo misses ln 2 by kLn2Res (|k|·kLn2Res). An optional low part of x
// adds one more rounding of r.
template <int T>
struct exp_k {
    static constexpr int    N     = exp_terms<T>;
    static constexpr double Rel   = up(exp_poly_rel<N> + kU * kRExp + 1100 * kLn2Res);
    static constexpr double RelLo = up(Rel + kU * kRExp);

    [[gnu::always_inline]] static double value(double x) {
        const double k = std::nearbyint(x * kLog2e);
        double       r = fma(-k, kLn2Hi, x);
        r              = fma(-k, kLn2Lo, r);
        return ::beman::inside::detail::ldexp(exp_poly<N>(r), static_cast<int>(k));
    }
    [[gnu::always_inline]] static double value(double x, double lo) {
        const double k = std::nearbyint(x * kLog2e);
        double       r = fma(-k, kLn2Hi, x);
        r              = fma(-k, kLn2Lo, r) + lo;
        return ::beman::inside::detail::ldexp(exp_poly<N>(r), static_cast<int>(k));
    }
    static double exp(double x, double& bound) {
        const double v = value(x);
        bound          = Rel * v + kTiny;
        return v;
    }

    // 2^x = 2^k·e^((x − k)·ln 2): x − k is exact (|x| < 2^52), and its
    // product with ln 2 rounds once on top of the constant's 2^-54.5.
    static constexpr double Rel2 = up(exp_poly_rel<N> + (kU + kLn2FullErr) * kRExp);
    static double           exp2(double x, double& bound) {
        const double k = std::nearbyint(x);
        const double r = (x - k) * kLn2Full;
        const double v = ::beman::inside::detail::ldexp(exp_poly<N>(r), static_cast<int>(k));
        bound          = Rel2 * v + kTiny;
        return v;
    }

    // The hyperbolics from e = e^x within Rel: cosh's two positive terms
    // keep it relative (1/e adds one rounding, the sum another);
    // tanh = (e^2x − 1)/(e^2x + 1) is within Rel + |t|·(Rel + 3kU). sinh of
    // a = |x| ≥ ln 2 (kLn2Full is above it) takes (e − 1/e)/2 with e ≥ 2:
    // e's Rel and 1/e's Rel + kU over the difference are at most
    // Rel·coth a + kU/(e² − 1) ≤ Rel·5/3 + kU/3, and the difference rounds
    // once; below ln 2 the odd series, relative (sinh_poly_rel). The
    // plain form takes the difference throughout: within (Rel + kU)·cosh x
    // + kU·|sinh x|, and cosh ≤ |sinh| + 1.
    static constexpr int    NSh          = sinh_terms<T>;
    static constexpr double SinhRel      = up(max_d(sinh_poly_rel<NSh>, Rel * 5 / 3 + kU / 3 + kU));
    static constexpr double SinhPlainRel = up(Rel + 2 * kU), SinhPlainAbs = up(Rel + kU);
    static constexpr double CoshRel = up(Rel + 2 * kU);
    static constexpr double TanhRel = up(Rel + 3 * kU), TanhAbs = up(Rel);

    static double sinh(double x, double& bound) {
        if constexpr (T <= kPlainBits) {
            const double e = value(x);
            const double v = (e - 1.0 / e) * 0.5;
            bound          = SinhPlainRel * __builtin_fabs(v) + SinhPlainAbs + kTiny;
            return v;
        }
        const double a = x < 0 ? -x : x;
        double       m;
        if (a < kLn2Full) {
            const double z = a * a;
            m              = fma(a * z, horner(z, sinh_c<NSh>.C), a);
        } else {
            const double e = value(a);
            m              = (e - 1.0 / e) * 0.5;
        }
        bound = SinhRel * m + kTiny;
        return x < 0 ? -m : m;
    }
    static double cosh(double x, double& bound) {
        const double e = value(x);
        const double v = (e + 1.0 / e) * 0.5;
        bound          = CoshRel * v + kTiny;
        return v;
    }
    static double tanh(double x, double& bound) {
        const double e = value(x + x);
        const double v = (e - 1.0) / (e + 1.0);
        bound          = TanhRel * __builtin_fabs(v) + TanhAbs + kTiny;
        return v;
    }
};

//---------------------------------------------------------------------------
// ln x for x > 0: frexp to m ∈ [√½, √2), ln x = e·ln2 + 2·atanh f with
// f = (m − 1)/(m + 1), |f| ≤ kFMax, and 2·atanh f = 2f·Σ f^2k/(2k+1),
// whose tail is at most f^2n/((2n+1)(1 − f²)) of the sum (which is ≥ 1).
//---------------------------------------------------------------------------
inline constexpr double kFMax = 0.1716; // ≥ (√2 − 1)/(√2 + 1) = 0.171573
inline constexpr double kZLog = kFMax * kFMax;
constexpr double        log_trunc(int n) { return pow_n(kZLog, n) / ((2 * n + 1) * (1 - kZLog)); }
template <int T>
inline constexpr int log_terms = terms_for(T, 11, log_trunc);

template <int N>
inline constexpr poly<N> log_c = series<N>([](int k) { return term{1.0, 2.0 * k + 1}; });

// m − 1 is exact (Sterbenz), m + 1 and the quotient round: f within 2kU,
// which moves atanh f by 2kU/(1 − f²) of itself. The Horner error and
// the truncation are relative (the sum is ≥ 1); 2f·p rounds once. Then
// e·Hi is exact and the two fmas round once each, relative to |v|: for
// e ≠ 0, |v| ≥ ln2/2 ≥ |2 atanh f|, and |e|·kLn2Res ≤ 3·kLn2Res·|v|;
// for e = 0 both fmas return 2f·p exactly.
template <int T>
struct log_k {
    static constexpr int    N    = log_terms<T>;
    static constexpr double MRel = [] {
        const horner_bound h = horner_error(log_c<N>, kZLog, kU);
        return h.Err + log_trunc(N) + kU + 2 * kU / (1 - kZLog);
    }();
    static constexpr double Rel   = up(MRel + 2 * kU + 3 * kLn2Res);
    static constexpr double Rel2  = up(Rel + kU + kLog2eErr);
    static constexpr double Rel10 = up(Rel + kU + kLog10eErr);

    [[gnu::always_inline]] static double value(double x) {
        int    e;
        double m = ::beman::inside::detail::frexp(x, &e);
        if (m < kSqrtHalf) {
            m += m;
            --e;
        }
        const double f    = (m - 1.0) / (m + 1.0);
        const double logm = 2.0 * f * horner(f * f, log_c<N>.C);
        const double r    = fma(static_cast<double>(e), kLn2Hi, logm);
        return fma(static_cast<double>(e), kLn2Lo, r);
    }
    // ln x as Hi + Lo, for pow and cbrt, whose y = c·ln x multiplies the
    // log's error by |y|. With b + Bl = m + 1 exactly (both differences
    // exact by Sterbenz) and r = a − f·b exact (f is the rounded quotient),
    // Fl = (r − f·Bl)/b puts f + Fl within 4kU² of a/(m + 1), and |Fl| ≤
    // 2kU·|f|. Then ln m = 2f + T, T = 2Fl + 2f·z·q(z), q = Σ z^k/(2k+3),
    // and 2f joins e·Hi in an exact two-sum; only T and the low parts
    // round. Relative to |ln x|, the error is HlRel:
    // - e = 0 (|ln x| ≥ |2f|), in units of |2f|: q evaluated at f, not f +
    //   Fl (the slope of 2·atanh f − 2f is 2z/(1 − z): 2kU·kZLog/(1 −
    //   kZLog)); Fl's own error; Horner and the truncation; the product
    //   2f·z and T's fma (kU each);
    // - e ≠ 0 (|ln x| ≥ 0.3466·|e|, |2f| ≤ 0.3432): that, plus the two
    //   additions of the low parts, e·Lo's rounding and kLn2Res, per |e|.
    template <int>
    struct q_series {
        static constexpr poly<N - 1> C = [] {
            poly<N - 1> q{};
            for (std::size_t i = 0; i + 1 < N; ++i) {
                q.C[i]   = log_c<N>.C[i];
                q.Err[i] = log_c<N>.Err[i];
            }
            return q;
        }();
    };
    static constexpr double HlRel = [] {
        const horner_bound h     = horner_error(q_series<0>::C, kZLog, kU);
        const double       t     = kZLog * h.Mag + 2 * kU; // |T|/|2f|
        const double       a2f   = 2 * kU * kZLog / (1 - kZLog) + 4 * kU * kU / (1 - kZLog) + log_trunc(N) +
                                   kZLog * (h.Err + kU * h.Mag) + kU * t;
        const double       per_e = 2 * kU * (0.7 * kU + kLn2Lo) + kU * kLn2Lo + kLn2Res;
        const double       rest  = 2 * kU * (0.35 * kU + 0.3432 * t);
        return up(max_d(a2f, (0.3432 * a2f + rest + per_e) / 0.3466));
    }();
    [[gnu::always_inline]] static double value_hl(double x, double& lo) {
        int    e;
        double m = ::beman::inside::detail::frexp(x, &e);
        if (m < kSqrtHalf) {
            m += m;
            --e;
        }
        const double a = m - 1.0, b = m + 1.0;
        const double bl = m - (b - 1.0);
        const double f  = a / b;
        const double fl = fma(-f, bl, fma(-f, b, a)) / b;
        const double z  = f * f;
        const double f2 = f + f;
        const double t  = fma(fenced(f2 * z), horner(z, q_series<0>::C.C), fl + fl);
        const double ed = static_cast<double>(e);
        const hilo   h  = two_sum(ed * kLn2Hi, f2);
        const hilo   v  = fast_two_sum(h.Hi, (h.Lo + ed * kLn2Lo) + t);
        lo              = v.Lo;
        return v.Hi;
    }

    static double log(double x, double& bound) {
        const double v = value(x);
        bound          = Rel * __builtin_fabs(v) + kTiny;
        return v;
    }
    static double log2(double x, double& bound) {
        const double v = value(x) * kLog2e;
        bound          = Rel2 * __builtin_fabs(v) + kTiny;
        return v;
    }
    static double log10(double x, double& bound) {
        const double v = value(x) * kLog10e;
        bound          = Rel10 * __builtin_fabs(v) + kTiny;
        return v;
    }

    // asinh and acosh as A = ln(1 + u), u ≥ 0 given as uh + ul with
    // |ul| ≤ kU·uh, through ln's Hi + Lo: 1 + uh = wh + e1 exactly,
    // wl = e1 + ul rounds once, and A = ln wh + ln(1 + q), q = wl/wh. If
    // wh = 1, q = wl and only wl's rounding is left; below 2, |wl| ≤ kU·wh
    // and A ≥ kU, so |q| ≤ A·(1 + 2^-50); from 2 on, |q| ≤ 2kU ≤ 3kU·A. Relative to A: ln wh within HlRel of
    // |ln wh| ≤ 2A; wl's rounding, the quotient's, the sum with ln wh's Lo
    // and the final sum (kU each, as |q| ≤ A); q − ln(1 + q) ≤ q²/2
    // (½kU). Log1pRel collects these; the callers add u's own error.
    static constexpr double              Log1pRel = up(2 * HlRel + 4 * kU * (1 + 0x1p-50) + 0.5 * kU);
    [[gnu::always_inline]] static double log1p_hl(double uh, double ul) {
        const hilo   w  = two_sum(1.0, uh);
        const double wl = w.Lo + ul;
        double       lo;
        const double v = value_hl(w.Hi, lo);
        return v + (lo + wl / w.Hi);
    }
    // asinh a, a = |x| ≤ 2^500: u = a + t, t = a²/(1 + √(1 + a²)), the sum
    // exact. t is within 4.5kU (a², the fma's ½kU through the root, the
    // root, 1 + √ and the quotient), which moves A by 4.5kU·t/(1 + u)
    // absolutely; with a = sinh A, t = cosh A − 1, so relative to A that
    // is 4.5kU·(1 − e^-A)²/(2A) ≤ 4.5kU·0.2037 (the maximum, at A ≈ 1.26).
    static constexpr double AsinhRel = up(Log1pRel + 4.5 * kU * 0.2037);
    // The plain form: below 1, ln(a + √(a² + 1)), whose argument is within
    // 2.5kU (fma, √, sum); above, ln a + ln(1 + √(1 + 1/a²)), the second
    // argument within 2kU and the sum one more rounding. Both: within
    // (Rel + kU)·|v| + 2.5kU.
    static constexpr double AhRel = up(Rel + kU), AsinhAbs = up(2.5 * kU);
    static double           asinh(double x, double& bound) {
        const double a = x < 0 ? -x : x;
        if constexpr (T <= kPlainBits) {
            const double m = a <= 1.0 ? value(a + std::sqrt(fma(a, a, 1.0)))
                                      : value(a) + value(1.0 + std::sqrt(1.0 + 1.0 / (a * a)));
            bound          = AhRel * m + AsinhAbs + kTiny;
            return x < 0 ? -m : m;
        }
        const double t = (a * a) / (1.0 + std::sqrt(fma(a, a, 1.0)));
        const hilo   u = two_sum(a, t);
        const double m = log1p_hl(u.Hi, u.Lo);
        bound          = AsinhRel * m + kTiny;
        return x < 0 ? -m : m;
    }

    // acosh x, 1 ≤ x ≤ 2^500: u = y + s, y = x − 1, s = √(y² + 2y) (one
    // fma), the sum exact. y is exact below 2^53; above, its rounding moves
    // A by at most kU·y/(1 + u) ≤ kU, and A ≥ 37. s is within 1.5kU, which
    // moves A by 1.5kU·s/(1 + u); with s = sinh A, 1 + u = e^A, relative
    // to A that is 1.5kU·(1 − e^-2A)/(2A) ≤ 1.5kU.
    static constexpr double AcoshRel = up(Log1pRel + 1.5 * kU + kU / 37);
    // The plain form: ln x + ln(1 + s), s = √d, d = 1 − 1/x². 1/x² is
    // within 2kU, and d (exact by Sterbenz, else one rounding of at most
    // kU·d) within 2kU of 1 − 1/x², which moves √d by at most 2kU/s; the
    // root, the sum and the logs add (Rel + kU)·|v| + 2kU. At x = 1, s = 0
    // makes the bound infinite.
    static double acosh(double x, double& bound) {
        if constexpr (T <= kPlainBits) {
            const double s = std::sqrt(1.0 - 1.0 / (x * x));
            const double v = value(x) + value(1.0 + s);
            bound          = AhRel * v + up(2 * kU) + up(2 * kU * (1 + 0x1p-50)) / s + kTiny;
            return v;
        }
        const double y = x - 1.0;
        const double s = std::sqrt(fma(y, y, y + y));
        const hilo   u = two_sum(y, s);
        const double v = log1p_hl(u.Hi, u.Lo);
        bound          = AcoshRel * v + kTiny;
        return v;
    }

    // atanh |x| < 1: ½·ln((1 + a)/(1 − a)), the quotient within 3kU.
    static constexpr double AtanhAbs = up(1.5 * kU);
    static double           atanh(double x, double& bound) {
        const double a = x < 0 ? -x : x;
        const double m = 0.5 * value((1.0 + a) / (1.0 - a));
        bound          = Rel * m + AtanhAbs + kTiny;
        return x < 0 ? -m : m;
    }
};

//---------------------------------------------------------------------------
// atan: |x| > 1 through π/2 − atan(1/x); then a > tan(π/12) through the
// π/6 addition formula, so |t| ≤ kTMax; atan t = t·Σ (−1)^k t^2k/(2k+1),
// alternating with falling terms.
//---------------------------------------------------------------------------
inline constexpr double kTMax    = 0.2681; // ≥ tan(π/12) = 0.267949
inline constexpr double kZAtan   = kTMax * kTMax;
inline constexpr double kAtanMin = 1 - kZAtan / 3; // ≤ atan t / t
constexpr double        atan_trunc(int n) { return pow_n(kZAtan, n) / (2 * n + 1) / kAtanMin; }
template <int T>
inline constexpr int atan_terms = terms_for(T, 15, atan_trunc);

template <int N>
inline constexpr poly<N> atan_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, 2.0 * k + 1}; });

// Unreduced (|x| ≤ tan(π/12), the only case where |v| can be small):
// the Horner error and truncation over atan t / t, and fma(t, p, 0)'s
// rounding — relative. Every other path adds absolute errors, in units of
// kU: t's three roundings (a − c, the fma, the quotient) move atan by
// 3·kTMax; the constants c and π/6 by 0.23 and 0.5 (atan(c̃) is within
// |c̃ − c|·¾ of π/6); fma(t, p, π/6) rounds by π/4; 1/a moves atan by ½;
// π/2 − r by 0.56 (the constant) and π/2 (the rounding); and t·(Horner +
// truncation) of the polynomial itself.
template <int T>
struct atan_k {
    static constexpr int    N       = atan_terms<T>;
    static constexpr double PolyAbs = [] {
        const horner_bound h = horner_error(atan_c<N>, kZAtan, kU);
        return h.Err + atan_trunc(N) * kAtanMin;
    }();
    static constexpr double Rel = up(PolyAbs / kAtanMin + kU);
    static constexpr double Abs = up(kTMax * PolyAbs + 3 * kTMax * kU + kInvSqrt3Err * 0.75 + kPiSixthErr +
                                     kRTrig * kU + 0.5 * kU + kPiHalfErr + 1.5708 * kU);

    [[gnu::always_inline]] static double value(double x) {
        const bool neg = x < 0;
        double     a   = neg ? -x : x;
        const bool inv = a > 1.0;
        if (inv)
            a = 1.0 / a;
        double off = 0.0;
        if (a > kTanPi12) {
            a   = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0);
            off = kPiSixth;
        }
        double r = fma(a, horner(a * a, atan_c<N>.C), off);
        if (inv)
            r = kPiHalf - r;
        return neg ? -r : r;
    }
    static double atan(double x, double& bound) {
        const double v = value(x);
        bound          = Rel * __builtin_fabs(v) + Abs + kTiny;
        return v;
    }

    // atan2: y/x rounds (½kU on the angle); x < 0 adds ±π (its constant
    // and one rounding).
    static constexpr double Atan2Rel = up(Rel + kU), Atan2Abs = up(Abs + 0.5 * kU + kPiErr);
    static double           atan2(double y, double x, double& bound) {
        double v;
        if (x > 0.0)
            v = value(y / x);
        else if (x < 0.0)
            v = value(y / x) + (y >= 0.0 ? kPi : -kPi);
        else
            v = y > 0.0 ? kPiHalf : y < 0.0 ? -kPiHalf : 0.0;
        bound = Atan2Rel * __builtin_fabs(v) + Atan2Abs + kTiny;
        return v;
    }

    // asin = atan(x/√((1 − x)(1 + x))): the quotient within 3.5kU, which
    // moves atan by 3.5kU of itself at most. acos = π/2 − asin: absolute.
    static constexpr double              AsinRel = up(Rel + 3.5 * kU), AsinAbs = Abs;
    static constexpr double              AcosRel = kU, AcosAbs = up(AsinRel * 1.5708 + AsinAbs + kPiHalfErr);
    [[gnu::always_inline]] static double asin_value(double x) { return value(x / std::sqrt((1.0 - x) * (1.0 + x))); }
    static double                        asin(double x, double& bound) {
        const double v = asin_value(x);
        bound          = AsinRel * __builtin_fabs(v) + AsinAbs + kTiny;
        return v;
    }
    static double acos(double x, double& bound) {
        const double v = kPiHalf - asin_value(x);
        bound          = AcosRel * __builtin_fabs(v) + AcosAbs + kTiny;
        return v;
    }
};

//---------------------------------------------------------------------------
// Compositions.
//---------------------------------------------------------------------------
// e^(y), y = ln a / 3 (or e·ln b): ln's relative error Rel_log and the
// product's roundings move y by |y|·(Rel_log + 2kU), and e^y by that much
// relatively, on top of Rel_exp. T counts the bits of the result; the
// log's error is multiplied by |y| (below 2^10), so it gets 10 more. Past
// T = 52 (outputs past 42 bits, where that would not decide at |y| = 16)
// y comes from the log's Hi + Lo instead: within |y|·(HlRel + 2^-103)
// (the low parts' roundings), and e^y sees one more rounding (RelLo).
template <int T>
struct pow_k {
    using L                      = log_k<T + 10 < kFullBits ? T + 10 : kFullBits>;
    using E                      = exp_k<T>;
    static constexpr bool   Hl   = T > 52;
    static constexpr double YRel = Hl ? up(L::HlRel + 0x1p-103) : up(L::Rel + 2 * kU);
    static constexpr double ERel = Hl ? E::RelLo : E::Rel;

    static double cbrt(double x, double& bound) {
        if (x == 0.0) {
            bound = 0;
            return 0.0;
        }
        const double a = x < 0 ? -x : x;
        double       y, m;
        if constexpr (Hl) {
            double       lo;
            const double h = L::value_hl(a, lo);
            y              = rounded_product(h, kThird);
            m              = E::value(y, fma(h, kThird, -y) + (h * kThirdLo + lo * kThird));
        } else {
            y = L::value(a) * kThird;
            m = E::value(y);
        }
        bound = m * (ERel + __builtin_fabs(y) * YRel) + kTiny;
        return x < 0 ? -m : m;
    }

    // b^e for b > 0, |e·ln b| ≤ 745 (else false).
    static bool pow(double b, double e, double& v, double& bound, double& y) {
        double ylo = 0;
        if constexpr (Hl) {
            double       lo;
            const double h = L::value_hl(b, lo);
            y              = rounded_product(e, h);
            ylo            = fma(e, h, -y) + e * lo;
        } else
            y = e * L::value(b);
        if (!(__builtin_fabs(y) <= 745))
            return false;
        v     = Hl ? E::value(y, ylo) : E::value(y);
        bound = v * (ERel + __builtin_fabs(y) * YRel) + kTiny;
        return true;
    }

    // Base^x with ln Base as a double-double (Hi, Lo within 2^-104 of it
    // relatively): y = x·ln Base as an exact product plus x·Lo, so e^y sees
    // only the low part's extra rounding and 2^-100·|y|.
    static double pow_ln(double x, double lnHi, double lnLo, double& bound, double& y) {
        y                = x * lnHi;
        const double ylo = fma(x, lnHi, -y) + x * lnLo;
        const double v   = E::value(y, ylo);
        bound            = v * (E::RelLo + __builtin_fabs(y) * 0x1p-100) + kTiny;
        return v;
    }
};

// √ (correctly rounded) and √(x² + y²): the squares and their sum within
// 2kU, the root halves that and rounds once more. |x|, |y| ≤ 2^500 keep
// the squares finite; squares below 2^-1022 lose at most 2^-1074, which
// moves the root by far less than kTiny.
inline double           fp_sqrt(double x) { return std::sqrt(x); }
inline constexpr double kSqrtRel = up(kU), kHypotRel = up(2 * kU);
inline double           fp_hypot(double x, double y) { return std::sqrt(fma(x, x, y * y)); }

// Seeds of the dd tier's Newton steps: one step squares a seed's error,
// so 2^-48 is enough (the dd kernels' test checks the result).
inline constexpr int kSeedBits = 48;
inline double        seed_cbrt(double x) {
    if (x == 0.0)
        return 0.0;
    const double m = exp_k<kSeedBits>::value(log_k<kSeedBits>::value(x < 0 ? -x : x) * kThird);
    return x < 0 ? -m : m;
}

} // namespace beman::inside::math::detail::fp

#endif // !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_DETAIL_MATH_FP_HPP
