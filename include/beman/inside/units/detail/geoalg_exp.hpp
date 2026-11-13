// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// math::exp(x) of a multivector (or a quantity of a blade): the closed form,
// each part of the output (the CoDomain) correctly rounded. Internal —
// include <beman/inside/units/math.hpp>.
//
// x = a + y, a the scalar part. When y·y is a scalar for every value of
// Domain (every two of y's parts anticommute), with z = -y·y,
//   exp(x) = e^a · (C + S·y),   C = cos √z,  S = sin √z / √z
// (for z < 0 these are cosh √-z and sinh √-z / √-z; for z = 0, C = S = 1).
// e^a, C and S are computed once per call and each part is decided onto its
// own grid from its error bound, as the scalar functions are: in double
// (parts of up to 47 bits), else in double-double, and what neither decides
// on the integer path (ga_exp_core), which also runs at compile time and
// without an FPU. So every part is the correctly rounded value, whichever
// path ran.
//
// Speed: the first fast tier runs inline (flattened), everything else out
// of line from x alone; one part of y needs no root or quotient (sin y
// itself); the bounds wait for no value (|sin|, |cos| ≤ 1, |S| ≤ 1/λ, ...);
// z is exact from the value indices on a 2^-k grid; and each part's range
// is proved at compile time where it can be, so the decides skip their
// range checks.
#ifndef BEMAN_INSIDE_UNITS_DETAIL_GEOALG_EXP_HPP
#define BEMAN_INSIDE_UNITS_DETAIL_GEOALG_EXP_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/math.hpp>
#include <beman/inside/units/geoalg.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

namespace beman::inside::units::detail::gaexp {
namespace eng = ::beman::inside::math::detail::engine;
using ::beman::inside::detail::wide_rational;
using ::beman::inside::detail::wide_sint;

template <class P>
using num_t = typename part_number<P>::type;

// f.template operator()<P>() for P = 0 … N-1.
template <std::size_t N, class Fn>
[[gnu::always_inline]] constexpr void for_index(Fn&& f) {
    [&]<std::size_t... P>(std::index_sequence<P...>) {
        (f.template operator()<P>(), ...);
    }(std::make_index_sequence<N>{});
}

// The sign of a part's unit against its canonical blade (j := e₃e₁ is -1).
template <class P>
consteval int part_sign() {
    constexpr grid_rational f = blade_factor(unit_of_part<P>());
    static_assert(f == grid_rational{1} || f == grid_rational{-1},
                  "math::exp: a part's unit scales its blade (a prefix, a scaled unit) - use the blade's own unit");
    return f == grid_rational{1} ? 1 : -1;
}

template <class P>
consteval bool plain_part() { // dimensionless (rad allowed) and free of π
    return same_outside_blade(part_dim<P>(), dim_one) && magnitude_of(unit_of_part<P>()).Pi == 0;
}

// |v| ≤ bound for every value of a part's number, as a double.
template <class N>
inline constexpr double max_abs = [] {
    const double lo = static_cast<double>(lower_of<N>), hi = static_cast<double>(upper_of<N>);
    return std::max(lo < 0 ? -lo : lo, hi < 0 ? -hi : hi);
}();

// An upper bound of e^x for 0 ≤ x ≤ 709, at compile time: x halved k times
// to at most ½, e^x there by its Taylor sum (positive terms: within 21U,
// widened by 2^-40), then squared k times, each square widened by 2^-50 so
// it stays above. Over by at most 2^-29.
consteval double exp_upper(double x) {
    int k = 0;
    while (x > 0.5) {
        x *= 0.5;
        ++k;
    }
    double t = 1, s = 1;
    for (int n = 1; n <= 20; ++n) {
        t *= x / n;
        s += t;
    }
    s *= 1 + 0x1p-40;
    for (int i = 0; i < k; ++i)
        s = s * s * (1 + 0x1p-50);
    return s;
}

// Everything known at compile time about Domain: its scalar part and the
// parts of y, their squares and signs, and bounds on each part of the result.
template <class Domain>
struct domain_facts {
    using DP                            = parts_t<Domain>;
    static constexpr std::size_t ND     = std::tuple_size_v<DP>;
    static constexpr auto        ddims  = dims_of<DP>();
    static constexpr int         scalar = [] {
        for (std::size_t i = 0; i < ND; ++i)
            if (!has_blade(ddims[i]))
                return static_cast<int>(i);
        return -1;
    }();
    static constexpr std::size_t N    = ND - (scalar >= 0 ? 1 : 0);
    static constexpr auto        yidx = [] {
        std::array<int, N> r{};
        std::size_t        j = 0;
        for (std::size_t i = 0; i < ND; ++i)
            if (static_cast<int>(i) != scalar)
                r[j++] = static_cast<int>(i);
        return r;
    }();
    template <std::size_t J>
    using ypart              = std::tuple_element_t<static_cast<std::size_t>(yidx[J]), DP>;
    static constexpr auto sq = [] {
        std::array<signed char, N> r{};
        for (std::size_t j = 0; j < N; ++j)
            r[j] = static_cast<signed char>(blade_square(ddims[static_cast<std::size_t>(yidx[j])]));
        return r;
    }();
    static constexpr bool closed = [] {
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = i + 1; j < N; ++j) {
                const dimension& a = ddims[static_cast<std::size_t>(yidx[i])];
                const dimension& b = ddims[static_cast<std::size_t>(yidx[j])];
                if (blade_sign(a, b) + blade_sign(b, a) != 0)
                    return false;
            }
        return true;
    }();
    static constexpr bool has_neg = [] { // a part squaring to -1: z can be > 0 (trig)
        for (auto s : sq)
            if (s < 0)
                return true;
        return false;
    }();
    static constexpr bool has_pos = [] { // a part squaring to +1: z can be < 0 (hyperbolic)
        for (auto s : sq)
            if (s > 0)
                return true;
        return false;
    }();
    // λ = √|z| bounds: over the parts squaring to -1 (z > 0), to +1 (z < 0).
    template <int Sign>
    static consteval double lam_bound() {
        return []<std::size_t... J>(std::index_sequence<J...>) {
            double s = 0;
            ((s += sq[J] == Sign ? max_abs<num_t<ypart<J>>> * max_abs<num_t<ypart<J>>> : 0.0), ...);
            return __builtin_sqrt(s);
        }(std::make_index_sequence<N>{});
    }
    static constexpr double lam_neg = lam_bound<-1>(), lam_pos = lam_bound<1>();
    // The signs of y's parts against their blades.
    static constexpr auto dsign = []<std::size_t... J>(std::index_sequence<J...>) {
        return std::array<int, N>{part_sign<ypart<J>>()...};
    }(std::make_index_sequence<N>{});
    // Bounds on the result's parts: |e^a·C| ≤ e^amax·cosh λ₊, and |e^a·S·yⱼ|
    // ≤ e^amax when every square is -1 (|yⱼ| ≤ λ), else e^amax·cosh λ₊·max|yⱼ|
    // (|S| ≤ max(1, sinh λ₊/λ₊) ≤ cosh λ₊). Each is above by at most 2^-29.
    // (e^amax: amax ≤ 0 counts as 0.)
    static constexpr double ea = [] {
        if constexpr (scalar >= 0) {
            const double hi =
                static_cast<double>(upper_of<num_t<std::tuple_element_t<static_cast<std::size_t>(scalar), DP>>>);
            return exp_upper(hi > 0 ? hi : 0.0);
        } else
            return exp_upper(0.0);
    }();
    static constexpr double ch      = has_pos ? (exp_upper(lam_pos) + 1) * 0.5 * (1 + 0x1p-50) : 1.0;
    static constexpr bool   all_neg = [] {
        for (auto s : sq)
            if (s >= 0)
                return false;
        return true;
    }();
    static constexpr double scalar_bound = ea * ch;
    static constexpr auto   part_bound   = []<std::size_t... J>(std::index_sequence<J...>) {
        return std::array<double, N>{(all_neg ? ea : ea * ch * max_abs<num_t<ypart<J>>>)...};
    }(std::make_index_sequence<N>{});
};

// What is known about a call onto CoDomain besides: for each part of
// CoDomain, where its value comes from (-2 zero, -1 the scalar part e^a·C,
// j ≥ 0 e^a·S·yⱼ) and its sign.
template <class Domain, class CoDomain>
struct facts : domain_facts<Domain> {
    using B = domain_facts<Domain>;
    using B::ddims, B::N, B::scalar, B::yidx;
    using typename B::DP;
    using CP                           = parts_t<CoDomain>;
    static constexpr std::size_t NC    = std::tuple_size_v<CP>;
    static constexpr auto        cdims = dims_of<CP>();
    static constexpr auto        src   = [] {
        std::array<int, NC> r{};
        for (std::size_t p = 0; p < NC; ++p) {
            r[p] = -2;
            if (!has_blade(cdims[p]))
                r[p] = -1;
            else
                for (std::size_t j = 0; j < N; ++j)
                    if (blade_equal(cdims[p], ddims[static_cast<std::size_t>(yidx[j])]))
                        r[p] = static_cast<int>(j);
        }
        return r;
    }();
    // The signs of CoDomain's parts against their blades.
    static constexpr auto osign = []<std::size_t... P>(std::index_sequence<P...>) {
        return std::array<int, NC>{part_sign<std::tuple_element_t<P, CP>>()...};
    }(std::make_index_sequence<NC>{});
    // Whether each part of CoDomain holds every value the part takes, so the
    // decides skip their range checks.
    static constexpr auto in_range = []<std::size_t... P>(std::index_sequence<P...>) {
        const auto fits = []<std::size_t Q>() {
            using O = num_t<std::tuple_element_t<Q, CP>>;
            if (src[Q] == -2)
                return true;
            const double m  = src[Q] == -1 ? B::scalar_bound : B::part_bound[static_cast<std::size_t>(src[Q])];
            const double lo = static_cast<double>(lower_of<O>), hi = static_cast<double>(upper_of<O>);
            return m * (1 + 0x1p-30) <= std::min(-lo, hi);
        };
        return std::array<bool, NC>{fits.template operator()<P>()...};
    }(std::make_index_sequence<NC>{});
    static constexpr bool covers = [] { // CoDomain holds the scalar part and every blade of y
        bool scalar_seen = false;
        for (std::size_t p = 0; p < NC; ++p)
            scalar_seen = scalar_seen || src[p] == -1;
        for (std::size_t j = 0; j < N; ++j) {
            bool seen = false;
            for (std::size_t p = 0; p < NC; ++p)
                seen = seen || src[p] == static_cast<int>(j);
            if (!seen)
                return false;
        }
        return scalar_seen;
    }();
};

template <class Domain>
consteval bool domain_closed() {
    return domain_facts<Domain>::closed;
}

//---------------------------------------------------------------------------
// The integer path: one part of the result at precision W. All values are
// fixed point at scale S, with |e^a| < 2^KE, |C|, |S| < 2^H and |yⱼ| < 2^MagY
// bounding the products, so an absolute error of a few units at S is far
// below 2^-(W + G).
//---------------------------------------------------------------------------
template <std::size_t                E,
          std::size_t                N,
          std::array<signed char, N> Sq,
          bool                       HasA,
          int                        MagA,
          int                        MagY,
          int                        MagL,
          int                        KE,
          int                        H>
struct ga_exp_core {
    static constexpr std::size_t    EZ = 2 * N * E + 1;
    wide_rational<E>                A;
    std::array<wide_rational<E>, N> Y;
    int                             Part; // -1: the scalar part, else j
    int                             Sign; // ±1 of CoDomain's part against the canonical blade

    // e^a·C near 1 (|x| tiny), e^a near 0 (a very negative), cos √z near 0.
    static constexpr int deep_extra = std::max(eng::deep_scaled(MagA, 1443), 4 * eng::fraction_bits<E>);

    // z = Σ -sqⱼ·yⱼ², exactly.
    constexpr wide_rational<EZ> z_exact() const noexcept {
        using ZI = wide_sint<EZ>;
        ZI num{0}, den{1};
        for (std::size_t j = 0; j < N; ++j) {
            const ZI n{Y[j].Num}, d{Y[j].Den};
            const ZI n2 = n * n, d2 = d * d;
            num = num * d2 + ZI{-Sq[j]} * n2 * den;
            den = den * d2;
        }
        return {num, den};
    }

    // The rational results: a part whose coefficient is 0, and every part
    // when a = 0 and z = 0 (1 and y). Elsewhere e^a·C and e^a·S·yⱼ are
    // transcendental (Lindemann–Weierstrass: e^(a ± i√z) and 1 are linearly
    // independent over the algebraic numbers unless a = z = 0).
    constexpr eng::maybe_exact<E> exact() const {
        if (Part >= 0 && eng::is_zero(Y[static_cast<std::size_t>(Part)]))
            return wide_rational<E>{wide_sint<E>{0}, wide_sint<E>{1}};
        if ((!HasA || eng::is_zero(A)) && eng::is_zero(z_exact())) {
            if (Part < 0)
                return wide_rational<E>{wide_sint<E>{1}, wide_sint<E>{1}};
            const wide_rational<E>& y = Y[static_cast<std::size_t>(Part)];
            return wide_rational<E>{Sign < 0 ? -y.Num : y.Num, y.Den};
        }
        return std::nullopt;
    }

    template <int W>
    constexpr auto run() const {
        constexpr int G            = 12;
        constexpr int Head         = KE + H + MagY + 2;
        constexpr int S            = W + G + Head;
        constexpr int T            = S + MagL + 4;
        using I                    = eng::fixed_t<T + std::max(Head, 2 * MagY + 2 * MagL + 8) + 8>;
        constexpr std::size_t K    = eng::limbs_of<I>;
        const auto            zr   = z_exact();
        const I               zq   = eng::to_q<S, K>(zr); // within ½
        const I               half = eng::one_q<K>(S - 1);
        I                     cq, sq;
        double                ec = 0, es = 0;
        if (!(zq > half) && !(zq < -half)) { // |z| ≤ ½: cos √z and sin √z / √z as series in z, both signs
            cq = eng::horner<eng::series::cos, S, false>(zq);
            sq = eng::horner<eng::series::sin, S, false>(zq);
            ec = 9;
            es = 9;
        } else if (zq > half) { // λ = √z ≥ 0.7: sin and cos of λ, reduced by quarter turns
            const I    lam  = eng::sqrt_exact_q<T, K, 64 * static_cast<int>(EZ)>(zr);
            const I    hp   = static_cast<I>(eng::pi_q<T - 1>);
            const I    k    = eng::round_shift(eng::mul_q(lam, static_cast<I>(eng::two_over_pi_q<T>), T), T);
            const I    r    = eng::round_shift(lam - k * hp, T - S); // |r| ≤ π/4 + a hair, within 2
            const auto qd   = static_cast<unsigned>(k.Word[0] & 3u);
            auto       quad = [&](unsigned p) {
                const eng::fixed_approx<K> f = p & 1u ? eng::cos_series<S>(r, 2) : eng::sin_series<S>(r, 2);
                return p & 2u ? eng::fixed_approx<K>{-f.Value, f.Error} : f;
            };
            const eng::fixed_approx<K> s = quad(qd), c = quad(qd + 1);
            const I                    lams = eng::round_shift(lam, T - S); // within 1
            cq                              = c.Value;
            ec                              = static_cast<double>(c.Error);
            sq                              = eng::div_q(s.Value, lams, S);
            es                              = 2 * static_cast<double>(s.Error) + 6;
        } else { // λ = √-z ≥ 0.7: cosh and sinh from e^λ
            const wide_rational<EZ> nz{-zr.Num, zr.Den};
            const I                 lam = eng::sqrt_exact_q<S, K, 64 * static_cast<int>(EZ)>(nz); // within 1
            const eng::approx<K>    e   = eng::exp_fixed<S>(lam, 1, H + 1);
            const int               kk  = S - e.Scale; // ≥ 0: e^λ ≥ 2
            const I                 ev  = e.Value << kk;
            const double            eev = static_cast<double>(e.Error) * ::beman::inside::detail::ldexp(1.0, kk);
            const I                 inv = eng::div_q(eng::one_q<K>(S), ev, S); // within eev/e² + 1
            const double            ei  = eev / 2.7 + 1;
            cq                          = eng::round_shift(ev + inv, 1);
            const I sh                  = eng::round_shift(ev - inv, 1);
            ec                          = (eev + ei) / 2 + 1;
            sq                          = eng::div_q(sh, lam, S);
            es = 1.5 * ((eev + ei) / 2 + 1) + 3 * ::beman::inside::detail::ldexp(1.0, H + 1) + 2;
        }
        // e^a at scale S: exp_fixed gives e^r·2^k at scale S - k (approx::below
        // is 1 at scale S + 2, which rescales to 0 within 1).
        I      eq = eng::one_q<K>(S);
        double ee = 0;
        if constexpr (HasA) {
            const I              aq = eng::to_q<S, K>(A);
            const eng::approx<K> e  = eng::exp_fixed<S>(aq, 1, KE);
            const int            kk = S - e.Scale;
            if (kk >= 0) {
                eq = e.Value << kk;
                ee = static_cast<double>(e.Error) * ::beman::inside::detail::ldexp(1.0, kk);
            } else {
                eq = eng::round_shift(e.Value, -kk);
                ee = static_cast<double>(eng::shr_bound(e.Error, -kk)) + 1;
            }
        }
        const double topE = ::beman::inside::detail::ldexp(1.0, KE), topC = ::beman::inside::detail::ldexp(1.0, H),
                     topY = ::beman::inside::detail::ldexp(1.0, MagY);
        I            v;
        double       err;
        if (Part < 0) {
            v   = eng::mul_q(eq, cq, S);
            err = ee * topC + ec * topE + 2;
        } else {
            const I      p  = eng::mul_q(eq, sq, S);
            const double ep = ee * topC + es * topE + 2;
            const I      yq = eng::to_q<S, K>(Y[static_cast<std::size_t>(Part)]); // within ½
            v               = eng::mul_q(p, yq, S);
            err             = ep * topY + topE * topC + 2;
        }
        if (Sign < 0)
            v = -v;
        err *= 1.01;
        int sh = 0;
        while (!(err < 0x1p60)) {
            err *= 0.5;
            ++sh;
        }
        if (sh == 0)
            return eng::approx<K>{v, S, static_cast<std::uint64_t>(err) + 1};
        return eng::approx<K>{eng::round_shift(v, sh), S - sh, static_cast<std::uint64_t>(err) + 2};
    }
};

//---------------------------------------------------------------------------
// The double tier: e^a, C and S once, each part decided from its bound.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_NO_FP
namespace fpk = ::beman::inside::math::detail::fp;

// The most bits (notch and magnitude) a part may need for the double tier
// to go first: the composite's bound is a few times 2^-50 relative. At 47
// bits a part near its largest magnitude fails over to the dd tier often
// enough to cost 5% (complex onto [-64, 64] at 2^-40), while typical values
// decide in half the dd tier's time; at 48 bits that worst case costs 50%.
inline constexpr int ga_fp_limit = 47;

template <class N>
inline constexpr int part_fp_bits = eng::fp_bits<N, eng::mag_bits<N> + 1>;

template <class N>
inline constexpr bool part_fp = eng::fp_output<N> && part_fp_bits<N> <= ga_fp_limit;

// Terms of cos √z and sin √z / √z as series in z for |z| ≤ z_small.
inline constexpr double z_small = 0.26;
template <int T>
inline constexpr int c_terms =
    fpk::terms_for(T, 14, [](int n) { return fpk::pow_n(z_small, n) / fpk::factorial(2 * n) * 1.01; });
template <int T>
inline constexpr int s_terms =
    fpk::terms_for(T, 14, [](int n) { return fpk::pow_n(z_small, n) / fpk::factorial(2 * n + 1) * 1.01; });
template <int T>
inline constexpr double c_small_err = [] {
    constexpr int N = c_terms<T>;
    return fpk::up(fpk::horner_error(fpk::cos_c<N>, z_small, 0).Err +
                   fpk::pow_n(z_small, N) / fpk::factorial(2 * N) * 1.01);
}();
template <int T>
inline constexpr double s_small_err = [] {
    constexpr int N = s_terms<T>;
    return fpk::up(fpk::horner_error(fpk::sin_c<N>, z_small, 0).Err +
                   fpk::pow_n(z_small, N) / fpk::factorial(2 * N + 1) * 1.01);
}();

// sin(r + q·π/2) and cos(r + q·π/2) from s = sin r and c = cos r, without a
// branch: sin and cos of q·π/2 are 0 or ±1, so both are exact.
struct sin_cos {
    double S, C;
};
inline constexpr double               quarter_sin[4] = {0, 1, 0, -1}, quarter_cos[4] = {1, 0, -1, 0};
[[gnu::always_inline]] inline sin_cos turn(long q, double s, double c) {
    const double qs = quarter_sin[q], qc = quarter_cos[q];
    return {std::fma(s, qc, c * qs), std::fma(c, qc, -(s * qs))};
}

// sin x and cos x for |x| ≤ 2^20 to T bits from one reduction, each within
// trig_k's Rel·|v| + AbsX·max(1, |x|).
template <int T>
[[gnu::always_inline]] inline sin_cos sincos_k(double x) {
    using tk       = fpk::trig_k<T>;
    long         q = 0;
    double       k = 0;
    const double r = fpk::reduce_quadrant(x, q, k);
    return turn(q, fpk::sin_poly<tk::NS>(r), fpk::cos_poly<tk::NC>(r));
}

// The shared values of the double tier and their absolute bounds. With one
// part of y, S·y itself (SY): sin y, sinh y or y, with no root or quotient.
// HasA: a scalar part (else E = 1 exactly); ExactInputs: every input a double
// exactly (no input terms in the bounds).
struct shared_fp {
    double E, bE, C, bC, S, bS, SY, bSY;
    double Cmax, Smax; // ≥ |C|, |S| (|SY|), known before C and S: the bounds wait for no value
};

template <int                        T,
          bool                       HasA,
          bool                       ExactInputs,
          bool                       HasNeg,
          bool                       HasPos,
          std::size_t                N,
          std::array<signed char, N> Sq,
          bool                       ZExact>
[[gnu::always_inline]] inline shared_fp
shared_values(double a, double ra, const std::array<double, N>& y, const std::array<double, N>& ry, double z_exact) {
    constexpr double U = fpk::unit_roundoff;
    shared_fp        v{1, 0, 1, 0, 1, 0, 0, 0, 1, 1};
    if constexpr (HasA) {
        v.E = fpk::exp_k<T>::exp(a, v.bE);
        if constexpr (!ExactInputs)
            v.bE += v.E * __builtin_fabs(a) * ra;
        v.bE = fpk::up(v.bE);
    }
    if constexpr (N == 1) {
        const double y0 = y[0], ay = __builtin_fabs(y0);
        double       by = 0; // y's input bound
        if constexpr (!ExactInputs)
            by = ay * ry[0];
        if (!(HasNeg || HasPos) || !(ay > 0.5)) { // z = y² ≤ ¼: the series
            constexpr int NC = c_terms<T>, NS = s_terms<T>;
            const double  z  = static_cast<double>(-Sq[0]) * y0 * y0;
            const double  bz = fpk::up(U * ay * ay + 2 * by * ay) + fpk::tiny;
            v.C              = fpk::horner(z, fpk::cos_c<NC>.C);
            v.S              = fpk::horner(z, fpk::sin_c<NS>.C);
            v.bC             = fpk::up(c_small_err<T> + 0.53 * bz);
            v.bS             = fpk::up(s_small_err<T> + 0.17 * bz);
            v.SY             = v.S * y0;
            v.Cmax           = 1.13; // ≥ cosh ½
            v.Smax           = 1.13 * ay;
            v.bSY            = fpk::up(v.bS * ay + 1.05 * by + U * v.Smax);
        } else if constexpr (HasNeg) { // cos y, sin y: |sin|, |cos| ≤ 1
            using tk           = fpk::trig_k<T>;
            const sin_cos sc   = sincos_k<T>(y0);
            const double  absx = tk::AbsX * std::max(1.0, ay) + fpk::tiny + by;
            v.C                = sc.C;
            v.bC               = fpk::up(tk::Rel + absx);
            v.SY               = sc.S;
            v.bSY              = v.bC;
        } else { // cosh y, sinh y; both move by at most cosh y per unit of y
            using ek         = fpk::exp_k<T>;
            const double e   = ek::value(ay);
            const double inv = 1.0 / e;
            const double rel = ek::Rel + U;
            v.C              = (e + inv) * 0.5;
            const double sh  = (e - inv) * 0.5;
            v.bC             = fpk::up(v.C * (rel + 2 * U + by));
            v.SY             = y0 < 0 ? -sh : sh;
            v.bSY            = fpk::up(v.C * (rel + by + U));
            v.Cmax = v.Smax = v.C * (1 + 4 * U);
        }
        return v;
    } else {
        // z = Σ -sqⱼ·yⱼ²: exact from the indices, else by fma with
        // ss = Σ yⱼ² bounding it.
        double z = z_exact, bz = fpk::tiny;
        if constexpr (!ZExact) {
            double ss = 0, sr = 0;
            z = 0;
            for_index<N>([&]<std::size_t J>() {
                if constexpr (Sq[J] != 0) {
                    ss = std::fma(y[J], y[J], ss);
                    z  = Sq[J] < 0 ? std::fma(y[J], y[J], z) : std::fma(-y[J], y[J], z);
                    if constexpr (!ExactInputs)
                        sr = std::fma(ry[J] * y[J], y[J], sr);
                }
            });
            bz = fpk::up((static_cast<double>(N) + 2) * U * ss + 2 * sr) + fpk::tiny;
        }
        const double az = __builtin_fabs(z);
        if (!(az > 0.25)) { // |C|, |S| ≤ cosh ½
            constexpr int NC = c_terms<T>, NS = s_terms<T>;
            v.C    = fpk::horner(z, fpk::cos_c<NC>.C);
            v.S    = fpk::horner(z, fpk::sin_c<NS>.C);
            v.bC   = fpk::up(c_small_err<T> + 0.53 * bz);
            v.bS   = fpk::up(s_small_err<T> + 0.17 * bz);
            v.Cmax = v.Smax = 1.13;
        } else if (HasNeg && z > 0) { // |sin|, |cos| ≤ 1, |S| ≤ 1/λ
            using tk           = fpk::trig_k<T>;
            const double  lam  = std::sqrt(z);
            const double  il   = 1.0 / lam; // now, so the quotient overlaps the reduction
            const double  blam = 0.5 * bz * il * (1 + 0x1p-50) + U * lam; // λ·(½·bz/z + U)
            const sin_cos sc   = sincos_k<T>(lam);
            const double  absx = tk::AbsX * std::max(1.0, lam) + fpk::tiny + blam;
            v.C                = sc.C;
            v.bC               = fpk::up(tk::Rel + absx);
            v.S                = sc.S * il; // within 2U of s/λ
            v.Smax             = il * (1 + 2 * U);
            v.bS               = fpk::up((tk::Rel + absx + blam * il) * il * (1 + U) + 2 * U * v.Smax);
        } else if (HasPos) {
            using ek          = fpk::exp_k<T>;
            const double lam  = std::sqrt(-z);
            const double il   = 1.0 / lam;
            const double blam = 0.5 * bz * il * (1 + 0x1p-50) + U * lam;
            const double e    = ek::value(lam);
            const double inv  = 1.0 / e;
            const double rel  = ek::Rel + blam + U;
            v.C               = (e + inv) * 0.5;
            const double sh   = (e - inv) * 0.5;
            v.Cmax            = v.C * (1 + 4 * U); // ≥ |C|, |sinh|
            v.bC              = fpk::up(v.C * (rel + 2 * U));
            const double bsh  = v.Cmax * (rel + U);
            v.S               = sh * il;
            v.Smax            = v.Cmax * il * (1 + 2 * U);
            v.bS              = fpk::up((bsh + v.Cmax * blam * il) * il * (1 + U) + 2 * U * v.Smax);
        } else { // unreachable for the domain's signs; force the integer path
            v.bC = v.bS = 1e300;
        }
        return v;
    }
}

//---------------------------------------------------------------------------
// The dd tier, for parts past 36 bits the double tier leaves: e^a, C and
// W = sin λ (sinh λ) once in double-double, λ = √|z|, and each blade part as
// (e^a·W·(1/λ))·yⱼ. With every square of one sign |yⱼ| ≤ λ, so W's absolute
// bound is not magnified as λ → 0; in a mixed signature near z = 0 the
// bound grows and the integer path decides. Error terms of the dd
// operations as in dd.hpp: a product within 4U², a sum 4U² of |a| + |b|, a
// quotient 32U², sqrt 7U² plus half its argument's (relative); the lean
// kernels' own bounds; the total doubled.
//---------------------------------------------------------------------------
namespace ddk = ::beman::inside::math::detail::dd;

template <class N>
inline constexpr bool part_dd = eng::dd_output<N>;

struct shared_dd {
    ddk::dd E, C, W, IL;    // IL = 1/λ
    double  bE, bC, bW, rL; // absolute bounds; rL relative, of 1/λ
    double  Cmax, Wmax;     // ≥ |C|, |W|, known before C and W
    bool    Ok;
};

// √z and 1/√z for z > 0 in dd from one root and one quotient: y = RN(√zh)
// and inv = RN(1/y), whose residuals zh − y² and 1 − y·inv are exact (fma).
// λ = y + (zh − y² + zl)·(inv/2): the Newton step's truncation
// (zh − y² + zl)²/(8y³) ≤ 1.2U²·λ, the sum's and products' roundings
// 4.5U²·λ: within 6U² (ddk::sqrt's 7U² stands). 1/λ = inv/(1 − r),
// r = 1 − y·inv − l·inv: inv + inv·r, within r²·inv ≤ 4.5U², the roundings
// of l·inv and inv·r 3.5U²: within 8U² (a quotient's 32U² stands). The root
// and the quotient overlap, and neither waits for a dd sum.
struct root_and_inverse {
    ddk::dd L, IL;
};
[[gnu::always_inline]] inline root_and_inverse sqrt_inv(ddk::dd z) noexcept {
    const double y   = fpk::fp_sqrt(z.Hi);
    const double inv = 1.0 / y;
    const double e   = std::fma(-y, y, z.Hi) + z.Lo;
    const double l   = e * (0.5 * inv);
    const double r   = std::fma(-y, inv, 1.0) - l * inv;
    return {fpk::fast_two_sum(y, l), fpk::fast_two_sum(inv, r * inv)};
}

template <bool                       HasA,
          bool                       ExactInputs,
          bool                       HasNeg,
          bool                       HasPos,
          std::size_t                N,
          std::array<signed char, N> Sq,
          bool                       ZExact>
[[gnu::always_inline]] inline shared_dd shared_dd_values(
    ddk::dd a, double ra, const std::array<ddk::dd, N>& y, const std::array<double, N>& ry, double z_exact) {
    constexpr double U2 = fpk::unit_roundoff2;
    shared_dd        v{{1, 0}, {1, 0}, {1, 0}, {1, 0}, 0, 0, 0, 0, 1, 1, true};
    if constexpr (HasA) {
        v.E = ddk::exp_lean(a, v.bE);
        if constexpr (!ExactInputs)
            v.bE += v.E.Hi * __builtin_fabs(a.Hi) * ra;
    }
    if constexpr (N == 1) { // W = sin y (sinh y, y) itself, λ = 1: no root, no quotient
        const double ay = __builtin_fabs(y[0].Hi);
        double       by = 0;
        if constexpr (!ExactInputs)
            by = ay * ry[0];
        if constexpr (HasNeg) {
            const ddk::sincos_t sc = ddk::sincos_lean(y[0]);
            v.C                    = sc.Cos;
            v.W                    = sc.Sin;
            v.bC = v.bW = ddk::lean_trig_bound(y[0].Hi) + by;
        } else if constexpr (HasPos) {
            const ddk::dd e   = ddk::exp_lean_value(ddk::dd{ay, y[0].Hi < 0 ? -y[0].Lo : y[0].Lo});
            const ddk::dd inv = ddk::div(ddk::dd{1, 0}, e);
            v.C               = ddk::scale(ddk::add(e, inv), -1);
            const ddk::dd sh  = ddk::scale(ddk::sub(e, inv), -1);
            v.W               = y[0].Hi < 0 ? ddk::neg(sh) : sh;
            v.Cmax = v.Wmax = v.C.Hi * (1 + 0x1p-50);
            v.bC = v.bW = (ddk::lean_exp::Rel + 20 * U2) * v.C.Hi * (1 + 0x1p-40) + v.C.Hi * by + fpk::tiny;
        } else {
            v.W    = y[0];
            v.bW   = by;
            v.Wmax = ay * (1 + 0x1p-50);
        }
    } else if constexpr (HasNeg || HasPos) {
        // z = Σ -sqⱼ·yⱼ²: the squares' high parts summed exactly (two_sum),
        // their low parts and the sums' errors in double, then one two_sum;
        // within (N + 2)·U²·Σ yⱼ² plus each square's own 4U² (sqr).
        ddk::dd z{z_exact, 0};
        double  bz = fpk::tiny;
        if constexpr (!ZExact) {
            double hi = 0, lo = 0, ss = 0, sr = 0;
            bool   first = true;
            for_index<N>([&]<std::size_t J>() {
                if constexpr (Sq[J] != 0) {
                    ddk::dd y2;
                    if constexpr (ExactInputs)
                        y2 = ddk::two_prod(y[J].Hi, y[J].Hi);
                    else {
                        y2 = ddk::sqr(y[J]);
                        sr = std::fma(ry[J], y2.Hi, sr);
                    }
                    if constexpr (Sq[J] > 0)
                        y2 = ddk::neg(y2);
                    ss += __builtin_fabs(y2.Hi);
                    if (first) {
                        hi    = y2.Hi;
                        lo    = y2.Lo;
                        first = false;
                    } else {
                        const ddk::dd t = fpk::two_sum(hi, y2.Hi);
                        hi              = t.Hi;
                        lo += t.Lo + y2.Lo;
                    }
                }
            });
            z  = fpk::two_sum(hi, lo);
            bz = fpk::up((4 * static_cast<double>(N) + 4) * U2 * ss + 2 * sr) + fpk::tiny;
        }
        const double az = __builtin_fabs(z.Hi);
        if (!(az > 2 * bz)) { // z's sign unknown, or λ too small for W/λ
            v.Ok = false;
            return v;
        }
        const bool             trig = z.Hi > 0;
        const root_and_inverse ri   = sqrt_inv(trig ? z : ddk::neg(z));
        const ddk::dd          lam  = ri.L;
        const double           rl   = 7 * U2 + 0.5 * bz / (az - bz); // λ's relative bound
        const double           dl   = lam.Hi * rl * (1 + 0x1p-40);   // and absolute
        v.IL                        = ri.IL;
        v.rL                        = rl * (1 + 0x1p-40) + 32 * U2;
        if (HasNeg && trig) {
            const ddk::sincos_t sc = ddk::sincos_lean(lam);
            const double        bt = ddk::lean_trig_bound(lam.Hi);
            v.C                    = sc.Cos;
            v.W                    = sc.Sin;
            v.bC = v.bW = bt + dl;
        } else if (HasPos && !trig) {
            const ddk::dd e   = ddk::exp_lean_value(lam);
            const ddk::dd inv = ddk::div(ddk::dd{1, 0}, e);
            v.C               = ddk::scale(ddk::add(e, inv), -1);
            v.W               = ddk::scale(ddk::sub(e, inv), -1);
            v.Cmax = v.Wmax = v.C.Hi * (1 + 0x1p-50);
            v.bC = v.bW = (ddk::lean_exp::Rel + 20 * U2) * v.C.Hi * (1 + 0x1p-40) + v.C.Hi * dl + fpk::tiny;
        } else
            v.Ok = false;
    }
    return v;
}
#endif

//---------------------------------------------------------------------------
// The call.
//---------------------------------------------------------------------------
template <std::size_t E, class Num>
constexpr wide_rational<E> exact_widened(const Num& n, int sign) {
    const auto       r = eng::exact_input(n);
    wide_rational<E> w{static_cast<wide_sint<E>>(r.Num), static_cast<wide_sint<E>>(r.Den)};
    if (sign < 0)
        w.Num = -w.Num;
    return w;
}

// x's parts: a multivector's own tuple (no copy: a local copy would sit in
// memory and bring the stack protector into the hot path), else a one-part
// tuple.
template <class X>
[[gnu::always_inline]] constexpr decltype(auto) parts_view(const X& x) {
    if constexpr (is_multivector_v<X>)
        return x.parts();
    else
        return parts_of(x);
}

// The number of y's J-th part, and of the scalar part.
template <class F, std::size_t J>
[[gnu::always_inline]] constexpr const auto& ynum(const typename F::DP& px) {
    return number_of_part(std::get<static_cast<std::size_t>(F::yidx[J])>(px));
}
template <class F>
[[gnu::always_inline]] constexpr const auto& anum(const typename F::DP& px) {
    return number_of_part(std::get<static_cast<std::size_t>(F::scalar)>(px));
}

// CoDomain's part numbers so far and which of them are decided.
template <class F>
struct state {
    decltype([]<std::size_t... P>(std::index_sequence<P...>) {
        return std::tuple<num_t<std::tuple_element_t<P, typename F::CP>>...>{};
    }(std::make_index_sequence<F::NC>{})) Out;
    std::array<bool, F::NC>               Done;
    // A fold over constant indices, so Done stays in registers.
    [[gnu::always_inline]] constexpr bool rest() const {
        return [&]<std::size_t... P>(std::index_sequence<P...>) {
            return (!Done[P] || ...);
        }(std::make_index_sequence<F::NC>{});
    }
};

// Compile-time facts of the tiers.
template <class F>
struct tier_facts {
    static constexpr bool a_fp = [] {
        if constexpr (F::scalar >= 0)
            return max_abs<num_t<std::tuple_element_t<static_cast<std::size_t>(F::scalar), typename F::DP>>> <= 700;
        else
            return true;
    }();
    static constexpr bool lam_fp = F::lam_neg <= 0x1p20 && F::lam_pos <= 700;
#ifndef BEMAN_INSIDE_MATH_NO_FP
    static constexpr bool any_fp = []<std::size_t... P>(std::index_sequence<P...>) {
        return ((part_fp<num_t<std::tuple_element_t<P, typename F::CP>>> && F::src[P] != -2) || ...);
    }(std::make_index_sequence<F::NC>{});
    static constexpr bool inputs_fp = []<std::size_t... I>(std::index_sequence<I...>) {
        return (!::beman::inside::detail::wide_valued<num_t<std::tuple_element_t<I, typename F::DP>>> && ...);
    }(std::make_index_sequence<F::ND>{});
    static constexpr bool fp_ok  = eng::fp_tier_available && any_fp && inputs_fp && a_fp && lam_fp;
    static constexpr bool any_dd = []<std::size_t... P>(std::index_sequence<P...>) {
        return ((part_dd<num_t<std::tuple_element_t<P, typename F::CP>>> && F::src[P] != -2) || ...);
    }(std::make_index_sequence<F::NC>{});
    static constexpr bool inputs_dd = []<std::size_t... I>(std::index_sequence<I...>) {
        return (eng::dd_input<num_t<std::tuple_element_t<I, typename F::DP>>> && ...);
    }(std::make_index_sequence<F::ND>{});
    static constexpr bool dd_ok = eng::fp_tier_available && any_dd && inputs_dd && a_fp && lam_fp;
    // Every input a double exactly: no input terms in the bounds.
    static constexpr bool exact_in = []<std::size_t... I>(std::index_sequence<I...>) {
        return (eng::fp_exact_input<num_t<std::tuple_element_t<I, typename F::DP>>> && ...);
    }(std::make_index_sequence<F::ND>{});
    // z exactly from the value indices: every part of y on one notch 2^-k
    // and N·max|index|² ≤ 2^53, so Σ -sqⱼ·indexⱼ² is an exact int64 and
    // double, and z = that·2^-2k exactly (no bound, no dd sum).
    template <std::size_t J>
    using ypart                 = num_t<std::tuple_element_t<static_cast<std::size_t>(F::yidx[J]), typename F::DP>>;
    static constexpr bool int_z = [] {
        if constexpr (F::N < 2)
            return false;
        else
            return []<std::size_t... J>(std::index_sequence<J...>) {
                using Y0 = ypart<0>;
                if constexpr (!((eng::fp_exact_input<ypart<J>> && eng::small_index<ypart<J>>) && ...))
                    return false;
                else {
                    const bool same = ((notch_of<ypart<J>> == notch_of<Y0>) && ...) && eng::notch_p<Y0> == 1;
                    const int  bits = std::max({eng::index_bits<ypart<J>>...});
                    return same && 2 * bits + static_cast<int>(std::bit_width(F::N)) <= 53;
                }
            }(std::make_index_sequence<F::N>{});
    }();
    static constexpr double z_scale = [] {
        if constexpr (int_z)
            return 1 / (eng::notch_q<ypart<0>> * eng::notch_q<ypart<0>>);
        else
            return 0.0;
    }();
#else
    static constexpr bool fp_ok = false, dd_ok = false;
#endif
};

#ifndef BEMAN_INSIDE_MATH_NO_FP
// z = Σ -sqⱼ·yⱼ² exactly from the value indices (tier_facts::int_z), else 0.
template <class F>
[[gnu::always_inline]] inline double exact_z(const typename F::DP& px) {
    if constexpr (tier_facts<F>::int_z) {
        std::int64_t zi = 0;
        for_index<F::N>([&]<std::size_t J>() {
            if constexpr (F::sq[J] != 0) {
                const auto i = ::beman::inside::detail::value_index<std::int64_t>(ynum<F, J>(px));
                zi += F::sq[J] < 0 ? i * i : -(i * i);
            }
        });
        return static_cast<double>(zi) * tier_facts<F>::z_scale;
    } else
        return 0.0;
}

// The double tier: e^a, C and S once, then each part it may decide.
template <class F>
[[gnu::always_inline]] inline void fp_pass(const typename F::DP& px, state<F>& st) {
    using CP                = typename F::CP;
    constexpr std::size_t N = F::N;
    constexpr bool        A = F::scalar >= 0, X = tier_facts<F>::exact_in;
    constexpr int         T = []<std::size_t... P>(std::index_sequence<P...>) {
        const int b = std::max({0, part_fp_bits<num_t<std::tuple_element_t<P, CP>>>...});
        return std::min(std::max(b + 10, 24), fpk::full_bits);
    }(std::make_index_sequence<F::NC>{});
    double a = 0, ra = 0;
    if constexpr (A) {
        a  = eng::input_double(anum<F>(px));
        ra = eng::input_rel<std::remove_cvref_t<decltype(anum<F>(px))>>;
    }
    std::array<double, N> y{}, ry{};
    for_index<N>([&]<std::size_t J>() {
        const double r = eng::input_double(ynum<F, J>(px));
        y[J]           = F::dsign[J] < 0 ? -r : r;
        ry[J]          = eng::input_rel<std::remove_cvref_t<decltype(ynum<F, J>(px))>>;
    });
    const shared_fp v =
        shared_values<T, A, X, F::has_neg, F::has_pos, N, F::sq, tier_facts<F>::int_z>(a, ra, y, ry, exact_z<F>(px));
    constexpr double U = fpk::unit_roundoff;
    // The bounds take |C|, |S| from Cmax, Smax: none waits for a value.
    const double aE = __builtin_fabs(v.E);
    // e^a·S (N > 1), its bound, and ≥ its magnitude.
    double p = v.S, bp = v.bS, pmax = v.Smax;
    if constexpr (A && N > 1) {
        p    = v.E * v.S;
        pmax = aE * v.Smax * (1 + U);
        bp   = fpk::up(aE * v.bS + v.Smax * v.bE + v.bE * v.bS + U * pmax);
    }
    // E·w and its bound, w within bw and |w| ≤ wmax (E·C, E·SY).
    const auto times_e = [&](double w, double bw, double wmax, double& b) {
        if constexpr (A) {
            b = aE * bw + wmax * v.bE + v.bE * bw + U * aE * wmax * (1 + U);
            return v.E * w;
        } else {
            b = bw;
            return w;
        }
    };
    for_index<F::NC>([&]<std::size_t Q>() {
        using O = num_t<std::tuple_element_t<Q, CP>>;
        if constexpr (part_fp<O> && F::src[Q] != -2) {
            if (st.Done[Q])
                return;
            double val, b;
            if constexpr (F::src[Q] == -1)
                val = times_e(v.C, v.bC, v.Cmax, b);
            else if constexpr (N == 1) {
                val = times_e(v.SY, v.bSY, v.Smax, b);
                if constexpr (F::osign[Q] < 0)
                    val = -val;
            } else {
                constexpr std::size_t j  = static_cast<std::size_t>(F::src[Q]);
                const double          ay = __builtin_fabs(y[j]);
                val                      = p * y[j];
                if constexpr (X)
                    b = ay * (bp + U * pmax * (1 + U));
                else
                    b = ay * (bp + (U + ry[j]) * pmax * (1 + U));
                if constexpr (F::osign[Q] < 0)
                    val = -val;
            }
            if (eng::fp_decide<O, F::in_range[Q]>(val, fpk::up(b) + fpk::tiny, std::get<Q>(st.Out)))
                st.Done[Q] = true;
        }
    });
}

// The dd tier on the parts it may decide that are left.
template <class F>
[[gnu::always_inline]] inline void dd_pass(const typename F::DP& px, state<F>& st) {
    using CP                = typename F::CP;
    constexpr std::size_t N = F::N;
    constexpr bool        A = F::scalar >= 0, X = tier_facts<F>::exact_in;
    ddk::dd               a{0, 0};
    double                ra = 0;
    if constexpr (A) {
        a  = eng::dd_read(anum<F>(px));
        ra = eng::dd_input_rel<std::remove_cvref_t<decltype(anum<F>(px))>>;
    }
    std::array<ddk::dd, N> y{};
    std::array<double, N>  ry{};
    for_index<N>([&]<std::size_t J>() {
        const ddk::dd r = eng::dd_read(ynum<F, J>(px));
        y[J]            = F::dsign[J] < 0 ? ddk::neg(r) : r;
        ry[J]           = eng::dd_input_rel<std::remove_cvref_t<decltype(ynum<F, J>(px))>>;
    });
    const shared_dd v =
        shared_dd_values<A, X, F::has_neg, F::has_pos, N, F::sq, tier_facts<F>::int_z>(a, ra, y, ry, exact_z<F>(px));
    if (!v.Ok)
        return;
    constexpr double U2 = fpk::unit_roundoff2;
    // The bounds take |C|, |W| from Cmax, Wmax: none waits for a value.
    const double aE = __builtin_fabs(v.E.Hi) * (1 + 0x1p-50);
    // E·w and its bound, w within bw and |w| ≤ wmax.
    const auto times_e = [&](ddk::dd w, double bw, double wmax, double& b) {
        if constexpr (A) {
            b = aE * bw + wmax * v.bE + v.bE * bw + 4 * U2 * aE * wmax * (1 + 0x1p-50);
            return ddk::mul(v.E, w);
        } else {
            b = bw;
            return w;
        }
    };
    double        bp;
    const ddk::dd p    = times_e(v.W, v.bW, v.Wmax, bp);
    double        pmax = A ? aE * v.Wmax * (1 + 0x1p-50) : v.Wmax;
    ddk::dd       w    = p; // e^a·W/λ (N > 1)
    double        bw = bp, wmax = pmax;
    if constexpr (N > 1) {
        const double ail = __builtin_fabs(v.IL.Hi) * (1 + 0x1p-50);
        w                = ddk::mul(p, v.IL);
        wmax             = pmax * ail * (1 + 0x1p-50);
        bw               = (bp + pmax * v.rL) * ail * (1 + 0x1p-40) + 4 * U2 * wmax;
    }
    for_index<F::NC>([&]<std::size_t Q>() {
        using O = num_t<std::tuple_element_t<Q, CP>>;
        if constexpr (part_dd<O> && F::src[Q] != -2) {
            if (st.Done[Q])
                return;
            ddk::dd val;
            double  b;
            if constexpr (F::src[Q] == -1)
                val = times_e(v.C, v.bC, v.Cmax, b);
            else {
                if constexpr (N == 1) { // y's input bound is in bW
                    val = p;
                    b   = bp;
                } else {
                    constexpr std::size_t j  = static_cast<std::size_t>(F::src[Q]);
                    const double          ay = __builtin_fabs(y[j].Hi) * (1 + 0x1p-50);
                    if constexpr (X) {
                        val = ddk::mul(w, y[j].Hi);
                        b   = ay * (bw + 4 * U2 * wmax);
                    } else {
                        val = ddk::mul(w, y[j]);
                        b   = ay * (bw + (ry[j] + 4 * U2) * wmax * (1 + 0x1p-50));
                    }
                }
                if constexpr (F::osign[Q] < 0)
                    val = ddk::neg(val);
            }
            if (eng::dd_decide<O, F::in_range[Q]>(val, 2 * b * (1 + 0x1p-40) + fpk::tiny, std::get<Q>(st.Out)))
                st.Done[Q] = true;
        }
    });
}
#endif

// The integer path for every part left.
template <class F>
constexpr void integer_pass(const typename F::DP& px, state<F>& st) {
    using DP                = typename F::DP;
    using CP                = typename F::CP;
    constexpr std::size_t N = F::N;
    constexpr std::size_t E = []<std::size_t... I>(std::index_sequence<I...>) {
        return std::max({std::size_t{1}, eng::input_limbs<num_t<std::tuple_element_t<I, DP>>>...});
    }(std::make_index_sequence<F::ND>{});
    constexpr bool HasA = F::scalar >= 0;
    constexpr int  MagA = [] {
        if constexpr (HasA)
            return eng::in_mag<num_t<std::tuple_element_t<static_cast<std::size_t>(F::scalar), DP>>>;
        else
            return 1;
    }();
    constexpr int MagY = []<std::size_t... J>(std::index_sequence<J...>) {
        return std::max({1, eng::in_mag<num_t<std::tuple_element_t<static_cast<std::size_t>(F::yidx[J]), DP>>>...});
    }(std::make_index_sequence<N>{});
    constexpr int MagL = MagY + static_cast<int>(std::bit_width(N)) + 1;
    constexpr int KE   = [] {
        if constexpr (HasA) {
            using A         = num_t<std::tuple_element_t<static_cast<std::size_t>(F::scalar), DP>>;
            const double hi = static_cast<double>(upper_of<A>);
            return std::max(0, static_cast<int>(hi * 1.4427) + 1) + 2;
        } else
            return 2;
    }();
    constexpr int H = F::has_pos ? static_cast<int>(F::lam_pos * 1.4427) + 3 : 2;
    using core      = ga_exp_core<E, N, F::sq, HasA, MagA, MagY, MagL, KE, H>;
    core c{};
    if constexpr (HasA)
        c.A = exact_widened<E>(anum<F>(px), 1);
    else
        c.A = wide_rational<E>{wide_sint<E>{0}, wide_sint<E>{1}};
    for_index<N>([&]<std::size_t J>() { c.Y[J] = exact_widened<E>(ynum<F, J>(px), F::dsign[J]); });
    for_index<F::NC>([&]<std::size_t Q>() {
        using O = num_t<std::tuple_element_t<Q, CP>>;
        if constexpr (F::src[Q] != -2) {
            if (!st.Done[Q]) {
                core cq = c;
                cq.Part = F::src[Q];
                cq.Sign = F::osign[Q];
                eng::note<eng::probe::integer_run>();
                std::get<Q>(st.Out) = eng::evaluate<O, eng::start_bits<O>>(cq);
                st.Done[Q]          = true;
            }
        }
    });
}

template <class Domain, class CoDomain>
CoDomain run_cold(const Domain& x);

// The call. Hot (Cold false): the exact parts, then the first fast tier
// inline; anything left redoes the call out of line from x alone, so the
// result goes straight to the caller and only x's address (the caller's
// object: Domain holds a std::tuple, passed by reference anyway) leaves the
// frame. Cold:
// the fast tiers again, the dd tier after the double tier, then the
// integer path.
template <class Domain, class CoDomain, bool Cold>
[[gnu::always_inline]] constexpr CoDomain body(const Domain& x) {
    using F                   = facts<Domain, CoDomain>;
    using TF [[maybe_unused]] = tier_facts<F>; // unused without the FP tiers
    using DP                  = typename F::DP;
    using CP                  = typename F::CP;
    constexpr std::size_t N   = F::N;
    constexpr std::size_t NC  = F::NC;
    static_assert(F::covers, "math::exp: CoDomain must hold the scalar part and every blade of the argument");
    []<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((plain_part<std::tuple_element_t<I, DP>>() && ...),
                      "math::exp: the argument's parts are numbers times blades (rad allowed), with no other unit "
                      "and no factor of pi");
    }(std::make_index_sequence<F::ND>{});
    []<std::size_t... P>(std::index_sequence<P...>) {
        static_assert((plain_part<std::tuple_element_t<P, CP>>() && ...),
                      "math::exp: CoDomain's parts are numbers times blades, with no other unit");
        static_assert((::beman::inside::detail::anchored<num_t<std::tuple_element_t<P, CP>>> && ...),
                      "math::exp: each CoDomain part's grid must pass through 0");
        (::beman::inside::math::detail::entry::require_rounding<num_t<std::tuple_element_t<P, CP>>>(), ...);
    }(std::make_index_sequence<NC>{});

    const DP& px = parts_view(x);
    state<F>  st{};

    // CoDomain's blades the argument lacks: 0.
    for_index<NC>([&]<std::size_t P>() {
        if constexpr (F::src[P] == -2) {
            std::get<P>(st.Out) = num_t<std::tuple_element_t<P, CP>>(just<0>);
            st.Done[P]          = true;
        }
    });
    // The exact parts of a value: coefficients 0, and with y = 0 the scalar
    // part e^a by the scalar engine (1 without a scalar part). Not on the
    // hot path: there a 0 comes out of the fast tiers exactly, and decides
    // unless a directed mode puts it on a rounding boundary - then the cold
    // call finds it here.
    bool       all_zero    = false;
    const auto exact_parts = [&] {
        all_zero = true;
        for_index<N>([&]<std::size_t J>() { all_zero = all_zero && ynum<F, J>(px) == just<0>; });
        for_index<NC>([&]<std::size_t P>() {
            using O = num_t<std::tuple_element_t<P, CP>>;
            if constexpr (F::src[P] >= 0) {
                if (ynum<F, static_cast<std::size_t>(F::src[P])>(px) == just<0>) {
                    std::get<P>(st.Out) = O(just<0>);
                    st.Done[P]          = true;
                }
            } else if constexpr (F::src[P] == -1) {
                if (all_zero) {
                    if constexpr (F::scalar >= 0) {
                        const auto& a       = anum<F>(px);
                        std::get<P>(st.Out) = ::beman::inside::math::detail::entry::exp_fn::eval<O>(a);
                    } else
                        std::get<P>(st.Out) = O(just<1>);
                    st.Done[P] = true;
                }
            }
        });
    };
    if consteval {
        exact_parts();
    } else {
        if constexpr (Cold)
            exact_parts();
    }

    if !consteval {
#ifndef BEMAN_INSIDE_MATH_NO_FP
        // The double tier where a part allows it, else the dd tier; cold,
        // the dd tier after the double tier.
        if constexpr (TF::fp_ok || TF::dd_ok)
            if (!eng::integer_only() && !all_zero) {
                if constexpr (TF::fp_ok)
                    fp_pass<F>(px, st);
                else
                    dd_pass<F>(px, st);
                if constexpr (Cold && TF::fp_ok && TF::dd_ok)
                    if (st.rest())
                        dd_pass<F>(px, st);
            }
#endif
        if (st.rest()) {
            if constexpr (Cold)
                integer_pass<F>(px, st);
            else
                return eng::cold_call([&x] { return run_cold<Domain, CoDomain>(x); });
        }
    } else {
        if (st.rest())
            integer_pass<F>(px, st);
    }
    return [&]<std::size_t... P>(std::index_sequence<P...>) {
        return CoDomain{units::detail::part_of_number<std::tuple_element_t<P, CP>>(std::get<P>(st.Out))...};
    }(std::make_index_sequence<NC>{});
}

// flatten: the kernels' helpers (Horner's lambdas, the dd operations) inline
// into the hot path too; cold_call stays out of line (noinline).
template <class Domain, class CoDomain>
[[gnu::always_inline]] [[gnu::flatten]] constexpr CoDomain run(const Domain& x) {
    return body<Domain, CoDomain, false>(x);
}
template <class Domain, class CoDomain>
CoDomain run_cold(const Domain& x) {
    return body<Domain, CoDomain, true>(x);
}
} // namespace beman::inside::units::detail::gaexp

namespace beman::inside::units::detail::gaexp {
// The output of exp over Domain: the scalar part, then each blade of y in
// Domain's unit, on the number N (the deduced output's, or one named).
template <class Domain, template <std::size_t> class N>
struct output_parts {
    using D    = domain_facts<Domain>;
    using type = decltype([]<std::size_t... J>(std::index_sequence<J...>) {
        return std::type_identity<multivector<N<D::N>, quantity<D::template ypart<J>::reference, N<J>>...>>{};
    }(std::make_index_sequence<D::N>{}))::type;
};

// The deduced output: each part's number on [-L, L], L its bound (above by
// a 2^-30 margin, so its decide skips the range check) rounded up onto the
// notch N: the parts' own (their gcd) unless S names one; the parts' policy
// (rounding to nearest unless it names a mode) under S's flags.
template <class Domain>
inline constexpr grid_rational parts_notch = []<std::size_t... P>(std::index_sequence<P...>) {
    using DP = parts_t<Domain>;
    grid_rational g{0};
    for (const grid_rational& n : {notch_of<num_t<std::tuple_element_t<P, DP>>>...}) {
        if (n == 0)
            return grid_rational{0};
        g = g == 0 ? n : ::beman::inside::detail::gcd(g, n);
    }
    return g;
}(std::make_index_sequence<std::tuple_size_v<parts_t<Domain>>>{});

template <class Domain, ::beman::inside::math::detail::out_spec S>
struct deduced {
    using D                              = domain_facts<Domain>;
    static constexpr grid_rational notch = S.HasNotch ? S.Notch : parts_notch<Domain>;
    static_assert(notch != 0,
                  "math::exp: a deduced output takes the argument's notch - its parts need one, or name it: "
                  "exp<per<K>>(x)");
    static constexpr policy_flag flags = ::beman::inside::math::detail::spec_flags(
        ::beman::inside::detail::with_default_rounding(flags_of<num_t<std::tuple_element_t<0, typename D::DP>>> &
                                                       ~::beman::inside::detail::cursor_marker),
        S);
    static consteval grid_rational limit(double m) {
        const grid_rational v{m * (1 + 0x1p-30)};
        return ::beman::inside::detail::grid_round<::beman::inside::detail::round_mode::ceil>(v / notch) * notch;
    }
    // Part J of y, or J = N for the scalar part.
    template <std::size_t J>
    using number = inside<grid{{-limit(J == D::N ? D::scalar_bound : D::part_bound[J % D::N]),
                                limit(J == D::N ? D::scalar_bound : D::part_bound[J % D::N])},
                               notch},
                          flags>;
    using type   = typename output_parts<Domain, number>::type;
};

// A named output: a multivector (or a quantity, its one part), or the number
// of every part.
template <class Domain, class C>
struct named {
    template <std::size_t>
    using number = C;
    static consteval auto pick() {
        if constexpr (is_multivector_v<C> || quantity_like<C>)
            return std::type_identity<C>{};
        else {
            static_assert(insidable<C>,
                          "math::exp<C>(x) of a multivector - C is a multivector, or the number of "
                          "every part");
            return std::type_identity<typename output_parts<Domain, number>::type>{};
        }
    }
    using type = typename decltype(pick())::type;
};
} // namespace beman::inside::units::detail::gaexp

namespace beman::inside::math::detail {
// A multivector's rank: 3 (a quantity of a blade has it too: units/math.hpp).
template <class T>
    requires units::detail::is_multivector_v<T>
inline constexpr int kind_rank<T> = 3;

// exp of a multivector, or of a quantity of a blade: the closed form, every
// part of the output correctly rounded.
template <class Domain>
struct kind<entry::exp_fn, 3, Domain> {
    static constexpr bool takes = units::detail::gaexp::domain_closed<Domain>();

    template <class S>
    struct out;
    template <out_spec S>
    struct out<specs<S>> {
        using type = typename units::detail::gaexp::deduced<Domain, S>::type;
    };
    template <class C>
    struct out<named<C>> {
        using type = typename units::detail::gaexp::named<Domain, C>::type;
    };
    template <class S>
    using output = typename out<S>::type;

    template <class O>
    [[nodiscard]] [[gnu::always_inline]] static constexpr O eval(const Domain& x) {
        return units::detail::gaexp::run<Domain, O>(x);
    }
};
} // namespace beman::inside::math::detail

namespace beman::inside::math {
// Two parts of the argument that commute: no closed form.
#define BEMAN_INSIDE_WHY_not_closed \
    "math::exp: no closed form - two parts of the argument commute (e12 and e34 in 4-D), so its square is no scalar"
template <auto... Spec, class D>
    requires(detail::rank_of<D> == 3 && !units::detail::gaexp::domain_closed<D>())
void exp(const D&) = delete (BEMAN_INSIDE_WHY_not_closed);
template <class C, class D>
    requires(detail::rank_of<D> == 3 && !units::detail::gaexp::domain_closed<D>())
void exp(const D&) = delete (BEMAN_INSIDE_WHY_not_closed);
#undef BEMAN_INSIDE_WHY_not_closed
} // namespace beman::inside::math

#endif // BEMAN_INSIDE_UNITS_DETAIL_GEOALG_EXP_HPP
