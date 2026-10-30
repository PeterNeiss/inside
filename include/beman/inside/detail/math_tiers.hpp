// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_TIERS_HPP
#define BEMAN_INSIDE_DETAIL_MATH_TIERS_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/math_cores.hpp>
#include <beman/inside/detail/math_fp.hpp> // the double tier's kernels
#include <beman/inside/detail/math_dd.hpp> // the dd tier's kernels

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

//---------------------------------------------------------------------------
// The double and dd tiers of the math engine (detail/math_entry.hpp): which
// kernel applies to an Out and its inputs, the per-call error bound, and the
// decision that turns a kernel's value into a slot when that bound allows.
// Without an FPU (BEMAN_INSIDE_MATH_NO_FP) both tiers are off.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::engine {
//---------------------------------------------------------------------------
// The double tier. Where an FPU is present, the double kernels
// (detail/math_fp.hpp) give the value first, with an error bound computed
// per call: the kernel's own proved bound, plus, for an input that is not
// a double exactly, its rounding (2^-50 of |x|, covering the conversion's
// roundings) times the function's slope. Each kernel is sized to Out: its
// polynomials get the fewest terms whose truncation stays 10 bits below
// Out's notch. When the bound places the result in one slot, that slot is
// the correctly rounded result; otherwise the dd tier or the integer path
// decides, so a result never depends on which path ran. Runtime only, for
// outputs up to the kernel's limit (fp_limit).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_NO_FP
inline constexpr bool fp_tier_available = true;
#else
inline constexpr bool fp_tier_available = false;
#endif

// Every value of In is a double: a dyadic notch, and values of at most 53
// significant bits.
template <insidable In>
inline constexpr bool fp_exact_input = [] {
    if constexpr (wide_valued<In> || !notched<In>)
        return false;
    else {
        const grid_int q = grid_denominator(notch_of<In>), p = grid_numerator(notch_of<In>);
        return is_pow2(q) && grid_magnitude_bits<In> + grid_bits(q) + grid_bits(p) <= 53;
    }
}();

// Outputs the double tier decides nearly always (up to 2^-14 of results go
// further): below them no other fast tier runs.
inline constexpr int fp_only_bits = 36;

// Bits of Out's value indices: every |index| < 2^index_bits.
template <insidable Out>
inline constexpr int index_bits = [] {
    if constexpr (!notched<Out> || wide_valued<Out>)
        return 1024;
    else {
        const grid_int lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.step_count();
        return bit_width_of(std::max(-lo, hi));
    }
}();

// Bits of Out a kernel must resolve when its values stay below 2^Mag:
// |v|/notch < 2^fp_bits, from Out's indices or the function's magnitude
// over the notch (≥ 2^-(out_bits − 1)). A relative error of 2^-fp_bits is
// within one notch.
template <insidable Out, int Mag>
inline constexpr int fp_bits = std::min(index_bits<Out>, Mag + out_bits<Out> - 1);

// Value indices of Out within ±2^52, so its slot bounds are doubles exactly.
template <insidable Out>
inline constexpr bool fp_output = notched<Out> && !wide_valued<Out> && index_bits<Out> <= 52;

// The tier for kernel K: Out within its limit. Inputs the tier reads as
// doubles: anything within the 64-bit rationals.
template <insidable Out, typename K, insidable... Ins>
inline constexpr bool fp_tier =
    fp_tier_available && fp_output<Out> && fp_bits<Out, K::Mag> <= K::Limit && (!wide_valued<Ins> && ...);

// A grid's notch p/q as doubles (the nearest, so within 2^-53 relative of
// p and q, at any size), and whether it is a power of two, so that scaling
// by it is exact.
template <insidable Out>
inline constexpr double notch_p = grid_double(grid_numerator(notch_of<Out>));
template <insidable Out>
inline constexpr double notch_q = grid_double(grid_denominator(notch_of<Out>));
template <insidable Out>
inline constexpr bool dyadic_notch =
    is_pow2(grid_numerator(notch_of<Out>)) && is_pow2(grid_denominator(notch_of<Out>));

// Value indices of In below 2^53: exact as doubles.
template <insidable In>
inline constexpr bool small_index = !wide_valued<In> && notched<In> &&
                                    grid_magnitude_bits<In> + grid_bits(grid_denominator(notch_of<In>)) -
                                            grid_bits(grid_numerator(notch_of<In>)) + 1 <=
                                        53;

// An input's value as a double: exactly (its value index times the dyadic
// notch) when fp_exact_input; else its index times p over q, within 2^-51
// (four roundings of 2^-53: p, q, the product and the quotient); else the
// conversion's nearest double.
template <insidable In>
constexpr double input_double(const In& x) noexcept {
    if constexpr (fp_exact_input<In>)
        return static_cast<double>(value_index<imax>(x)) * (notch_p<In> / notch_q<In>);
    else if constexpr (small_index<In>)
        return static_cast<double>(value_index<imax>(x)) * notch_p<In> / notch_q<In>;
    else
        return as_double(x);
}

// The relative rounding of an input's double: 0 when exact.
template <insidable In>
inline constexpr double input_rel = fp_exact_input<In> ? 0.0 : 0x1p-50;

// An integer near t (the nearest in the default rounding mode); NaN stays
// NaN. The tests below only need J to be an integer, so no compiler
// rewrite can make them pass wrongly — unlike the add-and-subtract-2^52
// trick, which reassociation folds away.
inline double nearest_int(double t) noexcept { return __builtin_nearbyint(t); }

// The slot of a kernel value v within an absolute bound, decided in double
// arithmetic. t = v·q/p is v's value index (exact for a dyadic notch, else
// within |t|·2^-52) and bt the bound in index units, with a margin of 1.5
// for the roundings of the bound's arithmetic. Every point of
// [t − bt, t + bt] must round to the same index J: for the nearest modes
// |t − J| < ½ − bt, for the directed ones a floor F with F < t − bt and
// t + bt < F + 1. Exact ties and integers fail the strict tests and go to
// the integer path. NaN and infinities fail every comparison, and the range
// check runs in double before J becomes an integer, so no finite check is
// needed. A decided slot inside Out's range is stored in out.
template <insidable Out>
inline bool fp_decide(double v, double bound, Out& out) noexcept {
    constexpr round_mode M  = out_rounding<Out>;
    constexpr double     s  = notch_q<Out> / notch_p<Out>;
    constexpr double     lo = static_cast<double>(static_cast<imax>(slot_base<Out>));
    constexpr double     hi = lo + static_cast<double>(static_cast<imax>(grid_of<Out>.step_count()));
    const double         t  = v * s;
    double               bt = bound * (1.5 * s);
    if constexpr (!dyadic_notch<Out>)
        bt += __builtin_fabs(t) * 0x1p-51;
    double j = nearest_int(t);
    if constexpr (M == round_mode::nearest || M == round_mode::half_even) {
        if (!(__builtin_fabs(t - j) < 0.5 - bt))
            return false;
    } else {
        if (j > t)
            j -= 1; // floor(t)
        if (!(t - j > bt && j + 1 - t > bt))
            return false;
        if constexpr (M == round_mode::ceil)
            j += 1;
        if constexpr (M == round_mode::trunc) {
            if (j < 0)
                j += 1;
        }
    }
    if (!(j >= lo && j <= hi)) {
        // Decided past an end: a clamp Out takes the end here; any other
        // policy reports or wraps on the integer path.
        if constexpr (has_flag(flags_of<Out>, clamp)) {
            if (j < lo) {
                out = at_slot<Out>(0);
                return true;
            }
            if (j > hi) {
                out = at_slot<Out>(static_cast<umax>(static_cast<imax>(hi - lo)));
                return true;
            }
        }
        return false; // NaN too
    }
    out = at_slot<Out>(static_cast<umax>(static_cast<imax>(j - lo)));
    return true;
}

// The kernels' safe argument ranges (compile-time, from In's grid).
template <insidable In>
inline constexpr double in_max = static_cast<double>(max_abs_int<In>);

// What follows a fast tier or a lean kernel: rarely taken, so out of line,
// which keeps its frame off the fast path.
template <std::invocable<> F>
[[gnu::cold]] [[gnu::noinline]] constexpr auto cold_call(const F& f) {
    return f();
}

#ifndef BEMAN_INSIDE_MATH_NO_FP
namespace fpk = ::beman::inside::math::detail::fp;

//---------------------------------------------------------------------------
// The dd tier: outputs past 36 bits whose value indices stay below 2^62,
// after the double tier where both apply. The double-double kernels (detail/math_dd.hpp) are within about
// 2^-97 of their values (measured against the integer path at 150 bits:
// 2^-97 relative at worst, 2^-99 absolute); the bound is 2^-88 of the
// result plus 2^-92·max(1, |x|), the same structure as the double tier's
// with a margin of 2^9, and inputs that are not doubles add 2^-100 of |x|
// times the slope. Undecided results go to the integer path as before.
//---------------------------------------------------------------------------
namespace ddk = ::beman::inside::math::detail::dd;

inline constexpr double dd_rel = 0x1p-88, dd_abs = 0x1p-92;

constexpr double dd_eval_bound(double x, double v) noexcept {
    return dd_rel * __builtin_fabs(v) + dd_abs * std::max(1.0, __builtin_fabs(x));
}

inline constexpr double two53 = 0x1p53;

// Value indices of Out of at most 2^62 in magnitude, and a notch whose p and q
// are doubles exactly.
template <insidable Out>
inline constexpr bool dd_output = [] {
    if constexpr (!notched<Out> || wide_valued<Out> || out_bits<Out> + mag_bits<Out> <= fp_only_bits)
        return false;
    else {
        if (!(notch_p<Out> < two53 && notch_q<Out> < two53))
            return false;
        const grid_int lim = grid_int{1} << 62;
        const grid_int lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.step_count();
        return !(lo < -lim) && !(lim < hi);
    }
}();

// Inputs the tier reads exactly (doubles) or within 2^-100 (index·p/q, or
// a rational raw's numerator over its denominator).
template <insidable In>
inline constexpr bool dd_input =
    !wide_valued<In> && !point_storage<In> &&
    (fp_exact_input<In> || rational_storage<In> || (small_index<In> && notch_p<In> < two53 && notch_q<In> < two53));

template <insidable Out, insidable... Ins>
inline constexpr bool dd_tier = fp_tier_available && dd_output<Out> && (dd_input<Ins> && ...);

template <insidable In>
inline constexpr double dd_input_rel = fp_exact_input<In> ? 0.0 : 0x1p-100;

template <insidable In>
inline ddk::dd dd_read(const In& x) noexcept {
    if constexpr (fp_exact_input<In>)
        return {input_double(x), 0};
    else if constexpr (rational_storage<In>) {
        const rational r     = x.raw();     // ±Num/|Den|
        auto           exact = [](umax n) { // two 32-bit halves, each a double exactly
            return ddk::fast_two_sum(static_cast<double>(n & ~umax{0xFFFFFFFF}),
                                     static_cast<double>(n & umax{0xFFFFFFFF}));
        };
        const ddk::dd q = ddk::div(exact(r.Num), exact(abs_den(r.Den)));
        return r.Den < 0 ? ddk::neg(q) : q;
    } else {
        const ddk::dd n = ddk::two_prod(static_cast<double>(value_index<imax>(x)), notch_p<In>);
        return ddk::div(n, ddk::dd{notch_q<In>, 0});
    }
}

// The slot of a dd value v within an absolute bound: the value index
// t = v·q/p as a dd (within 2^-104 of t; exact for a dyadic notch), split
// into J1 = nearbyint(t.Hi) and the rest u = (t.Hi − J1) + t.Lo, which
// rounds to J2 with remainder d. The tests on d are fp_decide's, with the
// bound widened by u's rounding.
template <insidable Out>
inline bool dd_decide(ddk::dd v, double bound, Out& out) noexcept {
    constexpr round_mode M     = out_rounding<Out>;
    constexpr ddk::dd    s     = ddk::div(ddk::dd{notch_q<Out>, 0}, ddk::dd{notch_p<Out>, 0});
    constexpr imax       first = static_cast<imax>(slot_base<Out>); // |index| ≤ 2^62
    constexpr imax       last  = static_cast<imax>(slot_base<Out> + grid_of<Out>.step_count());
    constexpr double     lo = static_cast<double>(first) - 1024, hi = static_cast<double>(last) + 1024;
    // t = J + u with J = j1 + j2: u is t.Hi's distance to its nearest
    // integer plus t.Lo, and may pass ½ (t.Hi on a half, or t.Lo carrying
    // it over), so it is rounded again rather than rejected.
    ddk::dd t;
    if constexpr (dyadic_notch<Out>)
        t = {v.Hi * s.Hi, v.Lo * s.Hi}; // s a power of two
    else
        t = ddk::mul(v, s);
    const double j1 = nearest_int(t.Hi);
    if (!(j1 >= lo && j1 <= hi))
        return false; // NaN and infinities too
    const double u  = (t.Hi - j1) + t.Lo;
    double       j2 = nearest_int(u);
    double       d  = u - j2;
    const double bt = bound * (1.5 * s.Hi) + __builtin_fabs(t.Hi) * 0x1p-100 + (__builtin_fabs(u) + 1) * 0x1p-52;
    if constexpr (M == round_mode::nearest || M == round_mode::half_even) {
        if (!(__builtin_fabs(d) < 0.5 - bt))
            return false;
    } else {
        if (d < 0) {
            d += 1;
            j2 -= 1;
        } // j1 + j2 = floor(t)
        if (!(d > bt && 1 - d > bt))
            return false;
        if constexpr (M == round_mode::ceil)
            j2 += 1;
    }
    imax j = static_cast<imax>(j1) + static_cast<imax>(j2);
    if constexpr (M == round_mode::trunc) {
        if (j < 0)
            j += 1;
    }
    if (j < first || j > last) {
        if constexpr (has_flag(flags_of<Out>, clamp)) { // as fp_decide
            out = at_slot<Out>(j < first ? umax{0} : static_cast<umax>(last) - static_cast<umax>(first));
            return true;
        }
        return false;
    }
    out = at_slot<Out>(static_cast<umax>(j) - static_cast<umax>(first));
    return true;
}

// The double kernels' bounds hold for IEEE arithmetic in the default
// rounding mode, with or without FMA contraction; fp_decide adds a 1.5
// margin for the roundings of the bound arithmetic. The bits a kernel's
// full-size bound e < 1 resolves: 2^-n ≤ e < 2^-(n−1) gives n − 4, where
// the bound spans at most 1/8 of a notch at Out's largest values, so 3 of 4
// results decide there and nearly all of the smaller ones.
consteval int fp_limit(double e) {
    int x = 0; // e = m·2^x, m in [½, 1): n = 1 − x
    ::beman::inside::detail::frexp(e, &x);
    return -x - 3;
}

// The kernel's target for Out: 10 bits past fp_bits, so the truncation
// moves at most 2^-9 of a notch.
template <insidable Out, typename K>
inline constexpr int fp_target = std::min(fp_bits<Out, K::Mag> + 10, fpk::full_bits);

// One kernel per function: its value and proved bound at a target T, the
// bits of its values (Mag, 1024 for unbounded), its limit, and its slope
// |f′(x)| (for the input's rounding).
inline constexpr int unbounded = 1024;
struct fp_plain {
    static double dd_eval(double x, double v) { return dd_eval_bound(x, v); }
};
    #define BEMAN_INSIDE_ENGINE_KERNEL_ON(base, fn, kernel, call, mag, limit, slope_expr)                    \
        struct fp_##fn : base {                                                                              \
            static constexpr int Mag   = mag;                                                                \
            static constexpr int Limit = limit;                                                              \
            template <int T>                                                                                 \
            static double value(double x, double& bound) {                                                   \
                return fpk::kernel<T>::call(x, bound);                                                       \
            }                                                                                                \
            template <typename D>                                                                            \
            static D dd_value(D x) {                                                                         \
                return ddk::fn(x);                                                                           \
            }                                                                                                \
            static double slope([[maybe_unused]] double x, [[maybe_unused]] double v) { return slope_expr; } \
        };
    #define BEMAN_INSIDE_ENGINE_KERNEL(...) BEMAN_INSIDE_ENGINE_KERNEL_ON(fp_plain, __VA_ARGS__)
using fp_full_trig = fpk::trig_k<fpk::full_bits>;
using fp_full_exp  = fpk::exp_k<fpk::full_bits>;
using fp_full_log  = fpk::log_k<fpk::full_bits>;
using fp_full_atan = fpk::atan_k<fpk::full_bits>;
BEMAN_INSIDE_ENGINE_KERNEL(sin, trig_k, sin, 1, fp_limit(fp_full_trig::Rel), 1.0)
BEMAN_INSIDE_ENGINE_KERNEL(cos, trig_k, cos, 1, fp_limit(fp_full_trig::Rel), 1.0)
BEMAN_INSIDE_ENGINE_KERNEL(exp, exp_k, exp, unbounded, fp_limit(fp_full_exp::Rel), __builtin_fabs(v))
BEMAN_INSIDE_ENGINE_KERNEL(exp2, exp_k, exp2, unbounded, fp_limit(fp_full_exp::Rel2), __builtin_fabs(v))
BEMAN_INSIDE_ENGINE_KERNEL(sinh, exp_k, sinh, unbounded, fp_limit(fp_full_exp::SinhRel), __builtin_fabs(v) + 1)
BEMAN_INSIDE_ENGINE_KERNEL(cosh, exp_k, cosh, unbounded, fp_limit(fp_full_exp::CoshRel), __builtin_fabs(v) + 1)
BEMAN_INSIDE_ENGINE_KERNEL(tanh, exp_k, tanh, 1, fp_limit(fp_full_exp::TanhRel + fp_full_exp::TanhAbs), 1.0)
BEMAN_INSIDE_ENGINE_KERNEL(atan, atan_k, atan, 1, fp_limit(fp_full_atan::Rel + fp_full_atan::Abs), 1.0)
BEMAN_INSIDE_ENGINE_KERNEL(asin,
                           atan_k,
                           asin,
                           1,
                           fp_limit(fp_full_atan::AsinRel + fp_full_atan::AsinAbs),
                           1.0 / fpk::fp_sqrt((1.0 - x) * (1.0 + x)))
BEMAN_INSIDE_ENGINE_KERNEL(acos,
                           atan_k,
                           acos,
                           2,
                           fp_limit(4 * fp_full_atan::AcosRel + fp_full_atan::AcosAbs),
                           1.0 / fpk::fp_sqrt((1.0 - x) * (1.0 + x)))
BEMAN_INSIDE_ENGINE_KERNEL(log, log_k, log, unbounded, fp_limit(fp_full_log::Rel), 1.0 / __builtin_fabs(x))
BEMAN_INSIDE_ENGINE_KERNEL(log2, log_k, log2, unbounded, fp_limit(fp_full_log::Rel2), 1.5 / __builtin_fabs(x))
BEMAN_INSIDE_ENGINE_KERNEL(log10, log_k, log10, unbounded, fp_limit(fp_full_log::Rel10), 1.0 / __builtin_fabs(x))
BEMAN_INSIDE_ENGINE_KERNEL(asinh, log_k, asinh, unbounded, fp_limit(fp_full_log::AsinhRel), 1.0)
BEMAN_INSIDE_ENGINE_KERNEL(
    atanh, log_k, atanh, unbounded, fp_limit(fp_full_log::Rel + fp_full_log::AtanhAbs), 1.0 / ((1.0 - x) * (1.0 + x)))
// cbrt = e^(ln|x|/3): the log's error grows with |ln|x||, up to 2^4.
BEMAN_INSIDE_ENGINE_KERNEL(cbrt,
                           pow_k,
                           cbrt,
                           unbounded,
                           fp_limit(fp_full_exp::Rel + 16 * fpk::pow_k<fpk::full_bits>::YRel),
                           x == 0 ? 0.0 : __builtin_fabs(v / x))

// acosh: the dd kernel's bound grows near 1 as 2^-92/√(1 − 1/x²).
struct fp_acosh_eval {
    static double dd_eval(double x, double v) {
        return dd_abs / fpk::fp_sqrt(1.0 - 1.0 / (x * x)) + dd_eval_bound(x, v);
    }
};
BEMAN_INSIDE_ENGINE_KERNEL_ON(fp_acosh_eval,
                              acosh,
                              log_k,
                              acosh,
                              unbounded,
                              fp_limit(fp_full_log::AcoshRel),
                              1.0 / fpk::fp_sqrt((x - 1.0) * (x + 1.0)))
    #undef BEMAN_INSIDE_ENGINE_KERNEL_ON
    #undef BEMAN_INSIDE_ENGINE_KERNEL

// sqrt: correctly rounded.
struct fp_sqrt : fp_plain {
    static constexpr int Mag   = unbounded;
    static constexpr int Limit = fp_limit(fpk::sqrt_rel);
    template <int T>
    static double value(double x, double& bound) {
        const double v = fpk::fp_sqrt(x);
        bound          = fpk::sqrt_rel * v;
        return v;
    }
    template <typename D>
    static D dd_value(D x) {
        return ddk::sqrt(x);
    }
    static double slope(double x, double v) { return x == 0 ? 0.0 : __builtin_fabs(v / x); }
};

// The kernels of the two-input functions and of tan, pow and Base^x: their
// bits and limits.
struct fp_tan {
    static constexpr int Mag   = unbounded;
    static constexpr int Limit = fp_limit(fp_full_trig::TanRel);
};
struct fp_atan2 {
    static constexpr int Mag   = 2;
    static constexpr int Limit = fp_limit(4 * fp_full_atan::Atan2Rel + fp_full_atan::Atan2Abs);
};
struct fp_hypot {
    static constexpr int Mag   = unbounded;
    static constexpr int Limit = fp_limit(fpk::hypot_rel);
};
// pow's bound grows with |e·ln b|: the limit holds up to 2^4.
struct fp_pow {
    static constexpr int Mag   = unbounded;
    static constexpr int Limit = fp_limit(fp_full_exp::Rel + 16 * fpk::pow_k<fpk::full_bits>::YRel);
};
struct fp_pow_base {
    static constexpr int Mag   = unbounded;
    static constexpr int Limit = fp_limit(fp_full_exp::RelLo);
};

// The tier's attempt for a one-input kernel K at input x (as read from In).
template <insidable Out, typename K, insidable In>
inline bool fp_attempt(const In& in, Out& out) {
    const double x = input_double(in);
    double       bound;
    const double v = K::template value<fp_target<Out, K>>(x, bound);
    if constexpr (!fp_exact_input<In>)
        bound += input_rel<In> * __builtin_fabs(x) * K::slope(x, v);
    return fp_decide(v, bound, out);
}

// atan2: each input's rounding moves the angle by at most its relative
// rounding (|∂/∂y|·|y| = |x·y|/r² ≤ ½).
template <insidable Out, insidable InY, insidable InX>
inline bool fp_attempt_atan2(const InY& yi, const InX& xi, Out& out) {
    const double y = input_double(yi), x = input_double(xi);
    double       bound;
    const double v = fpk::atan_k<fp_target<Out, fp_atan2>>::atan2(y, x, bound);
    bound += input_rel<InY> + input_rel<InX>;
    return fp_decide(v, bound, out);
}

template <insidable Out, insidable InX, insidable InY>
inline bool fp_attempt_hypot(const InX& xi, const InY& yi, Out& out) {
    const double x = input_double(xi), y = input_double(yi);
    const double v = fpk::fp_hypot(x, y);
    const double bound =
        fpk::hypot_rel * v + fpk::tiny + input_rel<InX> * __builtin_fabs(x) + input_rel<InY> * __builtin_fabs(y);
    return fp_decide(v, bound, out);
}

// tan: the input's rounding grows by sec² = 1 + t².
template <insidable Out, insidable In>
inline bool fp_attempt_tan(const In& in, Out& out) {
    const double x = input_double(in);
    double       t = 0, bound;
    if (!fpk::trig_k<fp_target<Out, fp_tan>>::tan(x, t, bound))
        return false;
    if constexpr (!fp_exact_input<In>)
        bound += input_rel<In> * __builtin_fabs(x) * (1 + t * t);
    return fp_decide(t, bound, out);
}

// pow = e^(e·ln b): the inputs' roundings move it by |e|·rel(b) and
// |e·ln b|·rel(e), relatively.
template <insidable Out, insidable InB, insidable InE>
inline bool fp_attempt_pow(const InB& bi, const InE& ei, Out& out) {
    const double b = input_double(bi), e = input_double(ei);
    if (!(b > 0))
        return false; // the integer path reports the domain
    double v, bound, y;
    if (!fpk::pow_k<fp_target<Out, fp_pow>>::pow(b, e, v, bound, y))
        return false;
    bound += v * (input_rel<InB> * __builtin_fabs(e) + input_rel<InE> * __builtin_fabs(y));
    return fp_decide(v, bound, out);
}

// ln Base at compile time, from the integer log, as a double-double.
template <imax Base>
inline constexpr ddk::dd ln_base = [] {
    const ddk::fixed a = ddk::fixed{Base} << ddk::scale_bits;
    return ddk::of_fixed(log_fixed<ddk::scale_bits>(a, 0).Value, ddk::scale_bits);
}();

// Base^x = e^(x·ln Base).
template <insidable Out, imax Base, insidable In>
inline bool fp_attempt_pow_base(const In& xi, Out& out) {
    const double x = input_double(xi);
    double       bound, y;
    const double v = fpk::pow_k<fp_target<Out, fp_pow_base>>::pow_ln(x, ln_base<Base>.Hi, ln_base<Base>.Lo, bound, y);
    if constexpr (!fp_exact_input<In>)
        bound += v * input_rel<In> * __builtin_fabs(y);
    return fp_decide(v, bound, out);
}
// The lean dd kernel of K, where it has one (detail/math_dd.hpp): its value
// at x and its proved bound.
template <typename K>
struct dd_lean {};
    #define BEMAN_INSIDE_ENGINE_LEAN(fn)         \
        template <>                              \
        struct dd_lean<fp_##fn> {                \
            template <typename D>                \
            static D value(D x, double& bound) { \
                return ddk::fn##_lean(x, bound); \
            }                                    \
        };
BEMAN_INSIDE_ENGINE_LEAN(sin)
BEMAN_INSIDE_ENGINE_LEAN(cos)
BEMAN_INSIDE_ENGINE_LEAN(exp)
BEMAN_INSIDE_ENGINE_LEAN(exp2)
BEMAN_INSIDE_ENGINE_LEAN(log)
BEMAN_INSIDE_ENGINE_LEAN(log2)
BEMAN_INSIDE_ENGINE_LEAN(log10)
BEMAN_INSIDE_ENGINE_LEAN(sinh)
BEMAN_INSIDE_ENGINE_LEAN(cosh)
BEMAN_INSIDE_ENGINE_LEAN(tanh)
BEMAN_INSIDE_ENGINE_LEAN(asinh)
BEMAN_INSIDE_ENGINE_LEAN(acosh)
BEMAN_INSIDE_ENGINE_LEAN(atanh)
BEMAN_INSIDE_ENGINE_LEAN(atan)
BEMAN_INSIDE_ENGINE_LEAN(asin)
BEMAN_INSIDE_ENGINE_LEAN(acos)
    #undef BEMAN_INSIDE_ENGINE_LEAN
template <typename K>
concept has_dd_lean = requires(ddk::dd x, double& b) { dd_lean<K>::template value<ddk::dd>(x, b); };

// The full dd kernel at x (read from In).
template <insidable Out, typename K, insidable In>
inline bool dd_full_attempt(const ddk::dd& x, Out& out) {
    const ddk::dd v     = K::dd_value(x);
    double        bound = K::dd_eval(x.Hi, v.Hi);
    if constexpr (dd_input_rel<In> != 0)
        bound += dd_input_rel<In> * __builtin_fabs(x.Hi) * K::slope(x.Hi, v.Hi);
    return dd_decide(v, bound, out);
}

// A lean attempt (its value, setting the bound) first, then the full one.
template <insidable Out, typename Lean, typename Full>
[[gnu::always_inline]] inline bool dd_lean_first(const Lean& lean, const Full& full, Out& out) {
    double        bound;
    const ddk::dd v = lean(bound);
    if (dd_decide(v, bound, out))
        return true;
    return cold_call(full);
}

// The lean kernel first where K has one, then the full one.
template <insidable Out, typename K, insidable In>
inline bool dd_attempt(const In& in, Out& out) {
    const ddk::dd x = dd_read(in);
    if constexpr (has_dd_lean<K>)
        return dd_lean_first(
            [&](double& bound) {
                const ddk::dd v = dd_lean<K>::value(x, bound);
                if constexpr (dd_input_rel<In> != 0)
                    bound += dd_input_rel<In> * __builtin_fabs(x.Hi) * K::slope(x.Hi, v.Hi);
                return v;
            },
            [&] { return dd_full_attempt<Out, K, In>(x, out); },
            out);
    else
        return dd_full_attempt<Out, K, In>(x, out);
}

template <insidable Out, insidable InY, insidable InX>
inline bool dd_attempt_atan2(const InY& yi, const InX& xi, Out& out) {
    const ddk::dd    y = dd_read(yi), x = dd_read(xi);
    constexpr double input = dd_input_rel<InY> + dd_input_rel<InX>;
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd v = ddk::atan2_lean(y, x, bound);
            bound += input;
            return v;
        },
        [&] {
            const ddk::dd v = ddk::atan2(y, x);
            return dd_decide(v, dd_eval_bound(1.0, v.Hi) + input, out);
        },
        out);
}

template <insidable Out, insidable InX, insidable InY>
inline bool dd_attempt_hypot(const InX& xi, const InY& yi, Out& out) {
    const ddk::dd x = dd_read(xi), y = dd_read(yi);
    const ddk::dd v  = ddk::hypot(x, y);
    const double  ax = __builtin_fabs(x.Hi), ay = __builtin_fabs(y.Hi);
    const double  bound = dd_eval_bound(std::max(ay, ax), v.Hi) + dd_input_rel<InX> * ax + dd_input_rel<InY> * ay;
    return dd_decide(v, bound, out);
}

// tan: the input's rounding grows by sec² = 1 + t².
template <insidable Out, insidable In>
inline bool dd_attempt_tan(const In& in, Out& out) {
    const ddk::dd x     = dd_read(in);
    auto          input = [&](double t) { return dd_input_rel<In> * __builtin_fabs(x.Hi) * (1 + t * t); };
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd t = ddk::tan_lean(x, bound);
            bound += input(t.Hi);
            return t;
        },
        [&] {
            const ddk::dd t    = ddk::tan(x);
            const double  sec2 = 1 + t.Hi * t.Hi, mx = std::max(1.0, __builtin_fabs(x.Hi));
            return dd_decide(t, dd_rel * sec2 * mx + dd_abs * mx + input(t.Hi), out);
        },
        out);
}

// pow = e^(e·ln b), as in the double tier; e·ln b within the exp range.
// The lean form: ln b within the lean log's bound Bl moves y = e·ln b by
// |e|·Bl, the product rounds within 4U² of |y|, and e^y adds its own Rel,
// all relative to v. The inputs' roundings add |e|·rel(b) and |y|·rel(e).
template <insidable Out, insidable InB, insidable InE>
inline bool dd_attempt_pow(const InB& bi, const InE& ei, Out& out) {
    const ddk::dd b = dd_read(bi), e = dd_read(ei);
    if (!(b.Hi > 0))
        return false;
    auto input = [&](double L) { return dd_input_rel<InB> * __builtin_fabs(e.Hi) + dd_input_rel<InE> * L; };
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd y = ddk::mul(ddk::log_lean(b, bound), e);
            const double  L = __builtin_fabs(y.Hi);
            if (!(L <= 700)) {
                bound = __builtin_inf();
                return ddk::dd{0, 0};
            }
            const ddk::dd v = ddk::exp_lean_value(y);
            bound = __builtin_fabs(v.Hi) * (ddk::lean_exp::Rel + __builtin_fabs(e.Hi) * bound * (1 + 0x1p-40) +
                                            4 * fpk::unit_roundoff2 * L + input(L)) +
                    fpk::tiny;
            return v;
        },
        [&] {
            const ddk::dd y = ddk::mul(ddk::log(b), e);
            const double  L = __builtin_fabs(y.Hi);
            if (!(L <= 700))
                return false;
            const ddk::dd v = ddk::exp(y);
            return dd_decide(v, __builtin_fabs(v.Hi) * (dd_rel * (1 + L) + input(L)) + dd_abs, out);
        },
        out);
}

// Base^x = e^(x·ln Base): ln Base within 2^-104 and the product's 4U².
// x·ln Base within the exp range (else the integer path decides).
template <insidable Out, imax Base, insidable In>
inline bool dd_attempt_pow_base(const In& xi, Out& out) {
    const ddk::dd x = dd_read(xi);
    const ddk::dd y = ddk::mul(x, ln_base<Base>);
    const double  L = __builtin_fabs(y.Hi);
    if (!(L <= 700))
        return false;
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd v = ddk::exp_lean_value(y);
            bound = __builtin_fabs(v.Hi) *
                        (ddk::lean_exp::Rel + (4 * fpk::unit_roundoff2 + ddk::table_err + dd_input_rel<In>)*L) +
                    fpk::tiny;
            return v;
        },
        [&] {
            const ddk::dd v = ddk::exp(y);
            return dd_decide(v, __builtin_fabs(v.Hi) * (dd_rel * (1 + L) + dd_input_rel<In> * L) + dd_abs, out);
        },
        out);
}
#else
template <insidable Out, insidable... Ins>
inline constexpr bool dd_tier = false;
#endif

} // namespace beman::inside::math::detail::engine

#endif // BEMAN_INSIDE_DETAIL_MATH_TIERS_HPP
