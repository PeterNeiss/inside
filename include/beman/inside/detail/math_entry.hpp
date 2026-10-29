// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_ENTRY_HPP
#define BEMAN_INSIDE_DETAIL_MATH_ENTRY_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/math_cores.hpp>
#include <beman/inside/detail/math_fp.hpp> // the double tier's kernels
#include <beman/inside/detail/math_dd.hpp> // the dd tier's kernels

#include <algorithm>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <optional>
#include <utility>

//---------------------------------------------------------------------------
// beman::inside::math::adaptive — the math engine behind cmath.hpp (which
// states the guarantee). The decision step and the driver are in
// detail/math_adaptive.hpp. Domains are the mathematical ones; a result past
// Out's range goes through Out's policy, as any assignment.
//
// Each call takes the first tier that applies: a compile-time table for
// small inputs; the double kernels (detail/math_fp.hpp), then the dd kernels
// (detail/math_dd.hpp), each returning a result only when its error bound
// decides the slot; and the integer path (detail/math_cores.hpp: exact square
// and cube roots, series in wide fixed point), which decides every result and is the only
// tier at compile time and without an FPU.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::ax {
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
        const grid_int q = wide_denominator(notch_of<In>), p = wide_numerator(notch_of<In>);
        const bool     dyadic = (grid_int{1} << (grid_bits(q) - 1)) == q;
        return dyadic && grid_magnitude_bits<In> + grid_bits(q) + grid_bits(p) <= 53;
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
        const grid_int lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.slot_count();
        const grid_int m = (-lo < hi) ? hi : -lo;
        return bit_width_of(m);
    }
}();

// Bits of Out a kernel must resolve when its values stay below 2^Mag:
// |v|/notch < 2^fp_bits, from Out's indices or the function's magnitude
// over the notch (≥ 2^-(out_bits − 1)). A relative error of 2^-fp_bits is
// within one notch.
template <insidable Out, int Mag>
inline constexpr int fp_bits = index_bits<Out> < Mag + out_bits<Out> - 1 ? index_bits<Out> : Mag + out_bits<Out> - 1;

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
inline constexpr double notch_p = grid_double(wide_numerator(notch_of<Out>));
template <insidable Out>
inline constexpr double notch_q = grid_double(wide_denominator(notch_of<Out>));
template <insidable Out>
inline constexpr bool dyadic_notch = [] {
    const grid_int p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
    return (grid_int{1} << (grid_bits(p) - 1)) == p && (grid_int{1} << (grid_bits(q) - 1)) == q;
}();

// Value indices of In below 2^53: exact as doubles.
template <insidable In>
inline constexpr bool small_index = !wide_valued<In> && notched<In> &&
                                    grid_magnitude_bits<In> + grid_bits(wide_denominator(notch_of<In>)) -
                                            grid_bits(wide_numerator(notch_of<In>)) + 1 <=
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

constexpr double fabs_d(double v) noexcept { return __builtin_fabs(v); }

// An integer near t (the nearest in the default rounding mode); NaN stays
// NaN. The tests below only need J to be an integer, so no compiler
// rewrite can make them pass wrongly — unlike the add-and-subtract-2^52
// trick, which reassociation folds away (and Clang's -fassociative-math
// does not announce).
inline double nearest_int(double t) noexcept { return __builtin_nearbyint(t); }

// Stores the grid point with slot offset k in out.
template <insidable Out>
[[gnu::always_inline]] inline void store_slot(umax k, Out& out) {
    out = Out::from_raw(raw_from_offset<Out>(k)); // a slot: a notched, integer raw
}

// The slot of a kernel value v within an absolute bound, decided in double
// arithmetic. t = v·q/p is v's value index (exact for a dyadic notch, else
// within |t|·2^-52) and bt the bound in index units; the bound's own 1.5
// margin covers the roundings of its arithmetic. Every point of
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
    constexpr double     hi = lo + static_cast<double>(static_cast<imax>(grid_of<Out>.slot_count()));
    const double         t  = v * s;
    double               bt = bound * s;
    if constexpr (!dyadic_notch<Out>)
        bt += fabs_d(t) * 0x1p-51;
    double j = nearest_int(t);
    if constexpr (M == round_mode::nearest || M == round_mode::half_even) {
        if (!(fabs_d(t - j) < 0.5 - bt))
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
                store_slot(0, out);
                return true;
            }
            if (j > hi) {
                store_slot(static_cast<umax>(static_cast<imax>(hi - lo)), out);
                return true;
            }
        }
        return false; // NaN too
    }
    store_slot(static_cast<umax>(static_cast<imax>(j - lo)), out);
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
    return dd_rel * fabs_d(v) + dd_abs * (fabs_d(x) > 1 ? fabs_d(x) : 1.0);
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
        const grid_int lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.slot_count();
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
        const rational r     = x.raw();     // ±Numerator/|Denominator|
        auto           exact = [](umax n) { // two 32-bit halves, each a double exactly
            return ddk::fast_two_sum(static_cast<double>(n & ~umax{0xFFFFFFFF}),
                                     static_cast<double>(n & umax{0xFFFFFFFF}));
        };
        const imax    d = r.Denominator;
        const ddk::dd q = ddk::div(exact(r.Numerator), exact(static_cast<umax>(d < 0 ? -d : d)));
        return d < 0 ? ddk::neg(q) : q;
    } else {
        const ddk::dd n = ddk::two_prod(static_cast<double>(value_index<imax>(x)), notch_p<In>);
        return ddk::div(n, ddk::dd{notch_q<In>, 0});
    }
}

// The slot of a dd value v within an absolute bound: the value index
// t = v·q/p as a dd (within 2^-104 of t), split into J1 = nearbyint(t.Hi)
// and the rest u = (t.Hi − J1) + t.Lo, which rounds to J2 with remainder d.
// The tests on d are fp_decide's, with the bound widened by u's rounding.
template <insidable Out>
inline bool dd_decide(ddk::dd v, double bound, Out& out) noexcept {
    constexpr round_mode M     = out_rounding<Out>;
    constexpr ddk::dd    s     = ddk::div(ddk::dd{notch_q<Out>, 0}, ddk::dd{notch_p<Out>, 0});
    constexpr imax       first = static_cast<imax>(slot_base<Out>); // |index| ≤ 2^62
    constexpr imax       last  = static_cast<imax>(slot_base<Out> + grid_of<Out>.slot_count());
    constexpr double     lo = static_cast<double>(first) - 1024, hi = static_cast<double>(last) + 1024;
    // t = J + u with J = j1 + j2: u is t.Hi's distance to its nearest
    // integer plus t.Lo, and may pass ½ (t.Hi on a half, or t.Lo carrying
    // it over), so it is rounded again rather than rejected.
    const ddk::dd t  = ddk::mul(v, s);
    const double  j1 = nearest_int(t.Hi);
    if (!(j1 >= lo && j1 <= hi))
        return false; // NaN and infinities too
    const double u  = (t.Hi - j1) + t.Lo;
    double       j2 = nearest_int(u);
    double       d  = u - j2;
    const double bt = bound * s.Hi + fabs_d(t.Hi) * 0x1p-100 + (fabs_d(u) + 1) * 0x1p-52;
    if constexpr (M == round_mode::nearest || M == round_mode::half_even) {
        if (!(fabs_d(d) < 0.5 - bt))
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
            store_slot(j < first ? umax{0} : static_cast<umax>(last) - static_cast<umax>(first), out);
            return true;
        }
        return false;
    }
    store_slot(static_cast<umax>(j) - static_cast<umax>(first), out);
    return true;
}

// The double kernels' bounds hold for IEEE arithmetic in the default
// rounding mode, with or without FMA contraction; fp_decide gets them with
// a 1.5 margin for the roundings of the bound arithmetic. The bits a
// kernel's full-size bound resolves: 2^-n ≤ e < 2^-(n−1) gives n − 4, where
// the bound spans at most 1/8 of a notch at Out's largest values, so 3 of 4
// results decide there and nearly all of the smaller ones.
consteval int fp_limit(double e) {
    int n = 0;
    while (e < 1) {
        e *= 2;
        ++n;
    }
    return n - 4;
}

// The kernel's target for Out: 10 bits past fp_bits, so the truncation
// moves at most 2^-9 of a notch.
template <insidable Out, typename K>
inline constexpr int fp_target =
    fp_bits<Out, K::Mag> + 10 < fpk::full_bits ? fp_bits<Out, K::Mag> + 10 : fpk::full_bits;

// One kernel per function: its value and proved bound at a target T, the
// bits of its values (Mag, 1024 for unbounded), its limit, and its slope
// |f′(x)| (for the input's rounding).
inline constexpr int unbounded = 1024;
struct fp_plain {
    static double dd_eval(double x, double v) { return dd_eval_bound(x, v); }
};
    #define BEMAN_INSIDE_AX_KERNEL_ON(base, fn, kernel, call, mag, limit, slope_expr)                        \
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
    #define BEMAN_INSIDE_AX_KERNEL(...) BEMAN_INSIDE_AX_KERNEL_ON(fp_plain, __VA_ARGS__)
using fp_full_trig = fpk::trig_k<fpk::full_bits>;
using fp_full_exp  = fpk::exp_k<fpk::full_bits>;
using fp_full_log  = fpk::log_k<fpk::full_bits>;
using fp_full_atan = fpk::atan_k<fpk::full_bits>;
BEMAN_INSIDE_AX_KERNEL(sin, trig_k, sin, 1, fp_limit(fp_full_trig::Rel), 1.0)
BEMAN_INSIDE_AX_KERNEL(cos, trig_k, cos, 1, fp_limit(fp_full_trig::Rel), 1.0)
BEMAN_INSIDE_AX_KERNEL(exp, exp_k, exp, unbounded, fp_limit(fp_full_exp::Rel), fabs_d(v))
BEMAN_INSIDE_AX_KERNEL(exp2, exp_k, exp2, unbounded, fp_limit(fp_full_exp::Rel2), fabs_d(v))
BEMAN_INSIDE_AX_KERNEL(sinh, exp_k, sinh, unbounded, fp_limit(fp_full_exp::SinhRel), fabs_d(v) + 1)
BEMAN_INSIDE_AX_KERNEL(cosh, exp_k, cosh, unbounded, fp_limit(fp_full_exp::CoshRel), fabs_d(v) + 1)
BEMAN_INSIDE_AX_KERNEL(tanh, exp_k, tanh, 1, fp_limit(fp_full_exp::TanhRel + fp_full_exp::TanhAbs), 1.0)
BEMAN_INSIDE_AX_KERNEL(atan, atan_k, atan, 1, fp_limit(fp_full_atan::Rel + fp_full_atan::Abs), 1.0)
BEMAN_INSIDE_AX_KERNEL(asin,
                       atan_k,
                       asin,
                       1,
                       fp_limit(fp_full_atan::AsinRel + fp_full_atan::AsinAbs),
                       1.0 / fpk::fp_sqrt((1.0 - x) * (1.0 + x)))
BEMAN_INSIDE_AX_KERNEL(acos,
                       atan_k,
                       acos,
                       2,
                       fp_limit(4 * fp_full_atan::AcosRel + fp_full_atan::AcosAbs),
                       1.0 / fpk::fp_sqrt((1.0 - x) * (1.0 + x)))
BEMAN_INSIDE_AX_KERNEL(log, log_k, log, unbounded, fp_limit(fp_full_log::Rel), 1.0 / fabs_d(x))
BEMAN_INSIDE_AX_KERNEL(log2, log_k, log2, unbounded, fp_limit(fp_full_log::Rel2), 1.5 / fabs_d(x))
BEMAN_INSIDE_AX_KERNEL(log10, log_k, log10, unbounded, fp_limit(fp_full_log::Rel10), 1.0 / fabs_d(x))
BEMAN_INSIDE_AX_KERNEL(asinh, log_k, asinh, unbounded, fp_limit(fp_full_log::AsinhRel), 1.0)
BEMAN_INSIDE_AX_KERNEL(
    atanh, log_k, atanh, unbounded, fp_limit(fp_full_log::Rel + fp_full_log::AtanhAbs), 1.0 / ((1.0 - x) * (1.0 + x)))
// cbrt = e^(ln|x|/3): the log's error grows with |ln|x||, up to 2^4.
BEMAN_INSIDE_AX_KERNEL(cbrt,
                       pow_k,
                       cbrt,
                       unbounded,
                       fp_limit(fp_full_exp::Rel + 16 * fpk::pow_k<fpk::full_bits>::YRel),
                       x == 0 ? 0.0 : fabs_d(v / x))

// acosh: the dd kernel's bound grows near 1 as 2^-92/√(1 − 1/x²).
struct fp_acosh_eval {
    static double dd_eval(double x, double v) {
        return dd_abs / fpk::fp_sqrt(1.0 - 1.0 / (x * x)) + dd_eval_bound(x, v);
    }
};
BEMAN_INSIDE_AX_KERNEL_ON(fp_acosh_eval,
                          acosh,
                          log_k,
                          acosh,
                          unbounded,
                          fp_limit(fp_full_log::AcoshRel),
                          1.0 / fpk::fp_sqrt((x - 1.0) * (x + 1.0)))
    #undef BEMAN_INSIDE_AX_KERNEL_ON
    #undef BEMAN_INSIDE_AX_KERNEL

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
    static double slope(double x, double v) { return x == 0 ? 0.0 : fabs_d(v / x); }
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
        bound += input_rel<In> * fabs_d(x) * K::slope(x, v);
    return fp_decide(v, bound * 1.5, out);
}

// atan2: each input's rounding moves the angle by at most its relative
// rounding (|∂/∂y|·|y| = |x·y|/r² ≤ ½).
template <insidable Out, insidable InY, insidable InX>
inline bool fp_attempt_atan2(const InY& yi, const InX& xi, Out& out) {
    const double y = input_double(yi), x = input_double(xi);
    double       bound;
    const double v = fpk::atan_k<fp_target<Out, fp_atan2>>::atan2(y, x, bound);
    bound += input_rel<InY> + input_rel<InX>;
    return fp_decide(v, bound * 1.5, out);
}

template <insidable Out, insidable InX, insidable InY>
inline bool fp_attempt_hypot(const InX& xi, const InY& yi, Out& out) {
    const double x = input_double(xi), y = input_double(yi);
    const double v     = fpk::fp_hypot(x, y);
    const double bound = fpk::hypot_rel * v + fpk::tiny + input_rel<InX> * fabs_d(x) + input_rel<InY> * fabs_d(y);
    return fp_decide(v, bound * 1.5, out);
}

// tan: the input's rounding grows by sec² = 1 + t².
template <insidable Out, insidable In>
inline bool fp_attempt_tan(const In& in, Out& out) {
    const double x = input_double(in);
    double       t = 0, bound;
    if (!fpk::trig_k<fp_target<Out, fp_tan>>::tan(x, t, bound))
        return false;
    if constexpr (!fp_exact_input<In>)
        bound += input_rel<In> * fabs_d(x) * (1 + t * t);
    return fp_decide(t, bound * 1.5, out);
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
    bound += v * (input_rel<InB> * fabs_d(e) + input_rel<InE> * fabs_d(y));
    return fp_decide(v, bound * 1.5, out);
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
        bound += v * input_rel<In> * fabs_d(y);
    return fp_decide(v, bound * 1.5, out);
}
// The lean dd kernel of K, where it has one (detail/math_dd.hpp): its value
// at x and its proved bound.
template <typename K>
struct dd_lean {};
    #define BEMAN_INSIDE_AX_LEAN(fn)             \
        template <>                              \
        struct dd_lean<fp_##fn> {                \
            template <typename D>                \
            static D value(D x, double& bound) { \
                return ddk::fn##_lean(x, bound); \
            }                                    \
        };
BEMAN_INSIDE_AX_LEAN(sin)
BEMAN_INSIDE_AX_LEAN(cos)
BEMAN_INSIDE_AX_LEAN(exp)
BEMAN_INSIDE_AX_LEAN(exp2)
BEMAN_INSIDE_AX_LEAN(log)
BEMAN_INSIDE_AX_LEAN(log2)
BEMAN_INSIDE_AX_LEAN(log10)
BEMAN_INSIDE_AX_LEAN(sinh)
BEMAN_INSIDE_AX_LEAN(cosh)
BEMAN_INSIDE_AX_LEAN(tanh)
BEMAN_INSIDE_AX_LEAN(asinh)
BEMAN_INSIDE_AX_LEAN(acosh)
BEMAN_INSIDE_AX_LEAN(atanh)
BEMAN_INSIDE_AX_LEAN(atan)
BEMAN_INSIDE_AX_LEAN(asin)
BEMAN_INSIDE_AX_LEAN(acos)
    #undef BEMAN_INSIDE_AX_LEAN
template <typename K>
concept has_dd_lean = requires(ddk::dd x, double& b) { dd_lean<K>::template value<ddk::dd>(x, b); };

// The full dd kernel at x (read from In).
template <insidable Out, typename K, insidable In>
inline bool dd_full_attempt(const ddk::dd& x, Out& out) {
    const ddk::dd v     = K::dd_value(x);
    double        bound = K::dd_eval(x.Hi, v.Hi);
    if constexpr (dd_input_rel<In> != 0)
        bound += dd_input_rel<In> * fabs_d(x.Hi) * K::slope(x.Hi, v.Hi);
    return dd_decide(v, bound * 1.5, out);
}

// A lean attempt (its value, setting the bound) first, then the full one.
template <insidable Out, typename Lean, typename Full>
[[gnu::always_inline]] inline bool dd_lean_first(const Lean& lean, const Full& full, Out& out) {
    double        bound;
    const ddk::dd v = lean(bound);
    if (dd_decide(v, bound * 1.5, out))
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
                    bound += dd_input_rel<In> * fabs_d(x.Hi) * K::slope(x.Hi, v.Hi);
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
            return dd_decide(v, (dd_eval_bound(1.0, v.Hi) + input) * 1.5, out);
        },
        out);
}

template <insidable Out, insidable InX, insidable InY>
inline bool dd_attempt_hypot(const InX& xi, const InY& yi, Out& out) {
    const ddk::dd x = dd_read(xi), y = dd_read(yi);
    const ddk::dd v  = ddk::hypot(x, y);
    const double  ax = fabs_d(x.Hi), ay = fabs_d(y.Hi);
    const double  bound = dd_eval_bound(ax > ay ? ax : ay, v.Hi) + dd_input_rel<InX> * ax + dd_input_rel<InY> * ay;
    return dd_decide(v, bound * 1.5, out);
}

// tan: the input's rounding grows by sec² = 1 + t².
template <insidable Out, insidable In>
inline bool dd_attempt_tan(const In& in, Out& out) {
    const ddk::dd x     = dd_read(in);
    auto          input = [&](double t) { return dd_input_rel<In> * fabs_d(x.Hi) * (1 + t * t); };
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd t = ddk::tan_lean(x, bound);
            bound += input(t.Hi);
            return t;
        },
        [&] {
            const ddk::dd t    = ddk::tan(x);
            const double  sec2 = 1 + t.Hi * t.Hi, mx = fabs_d(x.Hi) > 1 ? fabs_d(x.Hi) : 1.0;
            return dd_decide(t, (dd_rel * sec2 * mx + dd_abs * mx + input(t.Hi)) * 1.5, out);
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
    auto input = [&](double L) { return dd_input_rel<InB> * fabs_d(e.Hi) + dd_input_rel<InE> * L; };
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd y = ddk::mul(ddk::log_lean_value(b, bound), e);
            const double  L = fabs_d(y.Hi);
            if (!(L <= 700)) {
                bound = __builtin_inf();
                return ddk::dd{0, 0};
            }
            const ddk::dd v = ddk::exp_lean_value(y);
            bound           = fabs_d(v.Hi) * (ddk::lean_exp::Rel + fabs_d(e.Hi) * bound * (1 + 0x1p-40) +
                                              4 * fpk::unit_roundoff * fpk::unit_roundoff * L + input(L)) +
                              fpk::tiny;
            return v;
        },
        [&] {
            const ddk::dd y = ddk::mul(ddk::log(b), e);
            const double  L = fabs_d(y.Hi);
            if (!(L <= 700))
                return false;
            const ddk::dd v = ddk::exp(y);
            return dd_decide(v, (fabs_d(v.Hi) * (dd_rel * (1 + L) + input(L)) + dd_abs) * 1.5, out);
        },
        out);
}

// Base^x = e^(x·ln Base): ln Base within 2^-104 and the product's 4U².
// x·ln Base within the exp range (else the integer path decides).
template <insidable Out, imax Base, insidable In>
inline bool dd_attempt_pow_base(const In& xi, Out& out) {
    const ddk::dd x = dd_read(xi);
    const ddk::dd y = ddk::mul(x, ln_base<Base>);
    const double  L = fabs_d(y.Hi);
    if (!(L <= 700))
        return false;
    return dd_lean_first(
        [&](double& bound) {
            const ddk::dd v = ddk::exp_lean_value(y);
            bound           = fabs_d(v.Hi) * (ddk::lean_exp::Rel + (4 * fpk::unit_roundoff * fpk::unit_roundoff +
                                                                    ddk::table_err + dd_input_rel<In>)*L) +
                              fpk::tiny;
            return v;
        },
        [&] {
            const ddk::dd v = ddk::exp(y);
            return dd_decide(v, (fabs_d(v.Hi) * (dd_rel * (1 + L) + dd_input_rel<In> * L) + dd_abs) * 1.5, out);
        },
        out);
}
#else
template <insidable Out, insidable... Ins>
inline constexpr bool dd_tier = false;
#endif

} // namespace beman::inside::math::detail::ax

namespace beman::inside::math::adaptive {
namespace ax = ::beman::inside::math::detail::ax;
using ::beman::inside::detail::grid_rational;
using ::beman::inside::detail::wide_valued;

//---------------------------------------------------------------------------
// Unanchored grids. The engine counts results in Out's notch from 0 and
// inputs by their value index, so a lattice that does not pass through 0
// ({{0.5, 10.5}, 1}) is computed through anchored grids:
//   * an input is viewed exactly on its value unit gcd(Notch, Lower);
//   * an output is computed rounded down and up on the grid of every Out
//     point and every half point between two (unit gcd(Notch/2, Lower)),
//     one notch wider on each side. Equal, the value is that point; else it
//     lies strictly between two neighbours there, where no rounding
//     boundary of Out falls, so their midpoint rounds as the value does.
// wrap would fold the wider grid differently, so it needs an anchored Out.
//---------------------------------------------------------------------------
namespace anchoring {
using ::beman::inside::detail::anchored;

template <insidable In>
constexpr auto anchored_input(const In& x) {
    if constexpr (anchored<In>)
        return x;
    else
        return inside<grid{grid_of<In>.Interval, grid_of<In>.value_unit()},
                      flags_of<In> & ~::beman::inside::detail::cursor_marker>{x};
}

template <insidable Out>
inline constexpr grid_rational half_unit = ::beman::inside::detail::grid_gcd_of(
    ::beman::inside::detail::grid_div_of(notch_of<Out>, grid_rational{2}), lower_of<Out>);

template <insidable Out, policy_flag R, grid_rational Unit = half_unit<Out>>
using bracket_t = inside<grid{{::beman::inside::detail::grid_sub(lower_of<Out>, notch_of<Out>),
                               ::beman::inside::detail::grid_add(upper_of<Out>, notch_of<Out>)},
                              Unit},
                         R | (flags_of<Out> & clamp)>;

// The integer form: a bracket 1024 times finer than half_unit, counted in
// imax. Rounded down there, the value lies in [y, y + F); only when y is
// itself a rounding boundary of Out (a point or a half point — at most one
// bracket point in 1024) does rounding up decide whether the value is exactly y.
// Then Out's slot follows from the bracket index by one integer division.
template <insidable Out>
inline constexpr grid_rational fine_unit = ::beman::inside::detail::grid_div_of(half_unit<Out>, grid_rational{1024});

template <insidable Out>
inline constexpr bool integer_bracket = [] {
    using ::beman::inside::detail::signed_value_bits_of, ::beman::inside::detail::units_lo,
        ::beman::inside::detail::units_hi;
    if constexpr (wide_valued<Out>)
        return false;
    else {
        using B = bracket_t<Out, round_floor, fine_unit<Out>>;
        return !wide_valued<B> &&
               signed_value_bits_of({units_lo<B, fine_unit<Out>>, units_hi<B, fine_unit<Out>>}) < 60;
    }
}();

// Out's slot for a value known as `num/den` bracket units past Lower (in
// value space: `negative` is the value's sign), or nothing past Out's range.
template <insidable Out, imax den>
constexpr std::optional<imax> bracket_slot(imax num, bool negative) noexcept {
    using namespace ::beman::inside::detail;
    constexpr round_mode m = rounding_for<Out, policy<none>>;
    const auto [q, r]      = floor_divmod(num, den);
    const imax k           = q + rounds_up(m,
                                           negative,
                                           classify_remainder(m, static_cast<umax>(r), static_cast<umax>(den)),
                                           ((q & 1) != 0) != lower_index_odd<Out>);
    if (k < 0 || std::cmp_greater(k, max_index_of<Out>))
        return std::nullopt;
    return k;
}

// fn.operator()<O>(xs...) for an anchored O: Out's result, or its error.
template <insidable Out, typename Fn, insidable... Ins>
constexpr auto via_anchored(const Fn& fn, const Ins&... xs) {
    static_assert(anchored<Out> || !has_flag(flags_of<Out>, wrap),
                  "beman::inside::math: wrap onto a grid that does not pass through 0 is not supported - "
                  "use clamp, or an anchored output grid");
    using ::beman::inside::detail::expected_like;
    if constexpr (anchored<Out>)
        return fn.template operator()<Out>(anchored_input(xs)...);
    else if constexpr (integer_bracket<Out>) {
        using namespace ::beman::inside::detail;
        constexpr grid_rational F       = fine_unit<Out>;
        constexpr imax          c       = static_cast<imax>(exact_quotient(lower_of<Out>, F)); // Lower in F
        constexpr imax          s       = static_cast<imax>(exact_quotient(notch_of<Out>, F)); // Notch in F (even)
        using Lo                        = bracket_t<Out, round_floor, F>;
        using Hi                        = bracket_t<Out, round_ceil, F>;
        using R                         = decltype(fn.template operator()<Lo>(anchored_input(xs)...));
        constexpr bool checked          = expected_like<R>;
        using Res                       = std::conditional_t<checked, std::expected<Out, errc>, Out>;
        const R lo                      = fn.template operator()<Lo>(anchored_input(xs)...);
        const auto               unwrap = [](const auto& r) -> const auto& {
            if constexpr (checked)
                return *r;
            else
                return r;
        };
        if constexpr (checked)
            if (!lo)
                return Res{std::unexpected{lo.error()}};
        const imax j = value_in_units<imax, F>(unwrap(lo)); // y = j·F
        const imax t = j - c;                               // y − Lower, in F
        // The value lies inside (y, y + F): 2t + 1 half units of F.
        std::optional<imax> k;
        if (t % (s / 2) != 0) [[likely]]
            k = bracket_slot<Out, 2 * s>(2 * t + 1, j < 0);
        else { // y is a boundary of Out: is the value exactly y?
            const auto hi = fn.template operator()<Hi>(anchored_input(xs)...);
            if constexpr (checked)
                if (!hi)
                    return Res{std::unexpected{hi.error()}};
            k = value_in_units<imax, F>(unwrap(hi)) == j ? bracket_slot<Out, s>(t, j < 0)
                                                         : bracket_slot<Out, 2 * s>(2 * t + 1, j < 0);
        }
        if (k) [[likely]]
            return Res{Out::from_raw(raw_of_slot<Out>(*k))};
        // Past Out's range: the bracket value through Out's policy (it rounds
        // as the value does, and lies on the same side of the range).
        const auto v = exact_of(unwrap(lo)) +
                       wide_rational<exact_min_limbs>{wide_sint<exact_min_limbs>{1}, wide_sint<exact_min_limbs>{2}} *
                           exact_of_grid<exact_min_limbs>(F);
        if constexpr (checked) {
            errc      ec{};
            const Out out = ax::store_exact<Out>(v, make_policy<flags_of<Out>>(ec));
            return ec == errc{} ? Res{out} : Res{std::unexpected{ec}};
        } else
            return ax::store_exact<Out>(v, make_policy<flags_of<Out>>());
    } else {
        const auto lo                   = fn.template operator()<bracket_t<Out, round_floor>>(anchored_input(xs)...);
        const auto hi                   = fn.template operator()<bracket_t<Out, round_ceil>>(anchored_input(xs)...);
        auto                        mid = [](const auto& a, const auto& b) {
            using ::beman::inside::detail::exact_of;
            const auto s = exact_of(a) + exact_of(b);
            return decltype(s){s.Num, s.Den + s.Den};
        };
        if constexpr (expected_like<decltype(lo)>) {
            using R = std::expected<Out, errc>;
            if (!lo)
                return R{std::unexpected{lo.error()}};
            if (!hi)
                return R{std::unexpected{hi.error()}};
            errc      ec{};
            const Out out = ax::store_exact<Out>(mid(*lo, *hi), make_policy<flags_of<Out>>(ec));
            return ec == errc{} ? R{out} : R{std::unexpected{ec}};
        } else
            return ax::store_exact<Out>(mid(lo, hi), make_policy<flags_of<Out>>());
    }
}

// Whether a call needs via_anchored.
template <insidable Out, insidable... Ins>
inline constexpr bool unanchored_call = !anchored<Out> || (!anchored<Ins> || ...);
} // namespace anchoring

// The tiers in order at runtime: the double kernel (when FP), the dd
// kernel (when DD), the integer path. Each fast tier stores a decided
// result and returns true; whatever runs after the first one is cold.
// Constant evaluation takes the integer path, and so does every call under
// math::driver.IntegerOnly (BEMAN_INSIDE_MATH_INSTRUMENT).
template <insidable Out, bool FP, bool DD, typename F, typename D, typename I>
[[gnu::always_inline]] constexpr auto
tiers([[maybe_unused]] const F& fp, [[maybe_unused]] const D& dd, const I& integer_path) -> decltype(integer_path()) {
#if BEMAN_INSIDE_MATH_INSTRUMENT
    const auto integer = [&] {
        ax::note_integer_run();
        return integer_path();
    };
#else
    const I& integer = integer_path;
#endif
    if constexpr (FP || DD)
        if !consteval {
            if (!ax::integer_only()) {
                if constexpr (FP) {
                    if (Out r; fp(r))
                        return r;
                    return ax::cold_call([&]() -> decltype(integer()) {
                        if constexpr (DD)
                            if (Out r; dd(r))
                                return r;
                        return integer();
                    });
                } else {
                    if (Out r; dd(r))
                        return r;
                    return ax::cold_call(integer);
                }
            }
        }
    return integer();
}

// An _into form's body after the table: the tiers, with the double and
// dd attempts as expressions in r (nothing of them without an FPU). FP and
// DD in parentheses.
#ifndef BEMAN_INSIDE_MATH_NO_FP
    #define BEMAN_INSIDE_AX_TIERS(Out, FP, DD, fp_call, dd_call, ...)                                        \
        return ::beman::inside::math::adaptive::tiers<Out, FP, DD>([&](auto& r) -> bool { return fp_call; }, \
                                                                   [&](auto& r) -> bool { return dd_call; }, \
                                                                   [&] { return __VA_ARGS__; });
#else
    #define BEMAN_INSIDE_AX_TIERS(Out, FP, DD, fp_call, dd_call, ...) \
        ::beman::inside::math::detail::ax::note_integer_run();        \
        return __VA_ARGS__;
#endif

// The tiers of a one-input kernel fn, where ok holds.
#define BEMAN_INSIDE_AX_KERNEL_TIERS(Out, In, fn, x, ok, ...)          \
    BEMAN_INSIDE_AX_TIERS(Out,                                         \
                          (ax::fp_tier<Out, ax::fp_##fn, In> && (ok)), \
                          (ax::dd_tier<Out, In> && (ok)),              \
                          (ax::fp_attempt<Out, ax::fp_##fn>(x, r)),    \
                          (ax::dd_attempt<Out, ax::fp_##fn>(x, r)),    \
                          __VA_ARGS__)

// The table tier's lookup, inside an _into form: In has few slots and every
// result lies in Out's range.
#define BEMAN_INSIDE_AX_TABLE(Out, In, x)                                                                            \
    if constexpr (ax::table_input<In> && ax::table_output<Out>) {                                                    \
        using table = ax::result_table<Out, In, ax::start_bits<Out>, [](In v) { return core{ax::exact_input(v)}; }>; \
        if constexpr (table::Table.Valid)                                                                            \
            return Out::from_raw(table::Table.Raw[ax::offset_of(x)]);                                                \
    }

// Every transcendental rounds onto the output grid: Out's policy needs a
// rounding mode, like any assignment that may round.
template <insidable Out>
consteval void require_rounding() noexcept {
    static_assert(has_flag(flags_of<Out>, snap) || !ax::notched<Out>,
                  "beman::inside::math: the result is rounded onto Out's grid - Out must permit rounding "
                  "(declare it with round_nearest, round_floor, ...)");
}

// One-input functions: the domain (checked on In's grid, `true` for none),
// the double kernel's argument range (fp_ok), and the integer core.
#define BEMAN_INSIDE_AX_UNARY(fn, domain, msg, fp_ok, ...)                                                 \
    template <insidable In>                                                                                \
    inline constexpr bool fn##_domain = domain;                                                            \
    template <insidable Out, insidable In>                                                                 \
    [[nodiscard]] constexpr Out fn##_into(In x) {                                                          \
        static_assert(fn##_domain<In>, "beman::inside::math::" #fn ": " msg);                              \
        require_rounding<Out>();                                                                           \
        if constexpr (anchoring::unanchored_call<Out, In>)                                                 \
            return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return fn##_into<O>(v); }, x);   \
        else {                                                                                             \
            using core = __VA_ARGS__;                                                                      \
            BEMAN_INSIDE_AX_TABLE(Out, In, x)                                                              \
            BEMAN_INSIDE_AX_KERNEL_TIERS(                                                                  \
                Out, In, fn, x, (fp_ok), ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(x)})) \
        }                                                                                                  \
    }

BEMAN_INSIDE_AX_UNARY(
    exp, true, "", ax::in_max<In> <= 700, ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_AX_UNARY(exp2,
                      true,
                      "",
                      ax::in_max<In> <= 1000,
                      ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_AX_UNARY(
    sin, true, "", ax::in_max<In> <= 0x1p20, ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::sin, 1>)
BEMAN_INSIDE_AX_UNARY(
    cos, true, "", ax::in_max<In> <= 0x1p20, ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::cos, 1>)
BEMAN_INSIDE_AX_UNARY(atan, true, "", true, ax::atan_core<ax::input_limbs<In>>)
BEMAN_INSIDE_AX_UNARY(sinh,
                      true,
                      "",
                      ax::in_max<In> <= 700,
                      ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_AX_UNARY(cosh,
                      true,
                      "",
                      ax::in_max<In> <= 700,
                      ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::cosh, ax::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_AX_UNARY(
    tanh, true, "", ax::in_max<In> <= 300, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::tanh, 1>)
BEMAN_INSIDE_AX_UNARY(asinh,
                      true,
                      "",
                      ax::in_max<In> <= 0x1p500,
                      ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::asinh, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(cbrt, true, "", true, ax::cbrt_core<ax::input_limbs<In>, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(log,
                      (lower_of<In> > 0),
                      "input must be strictly positive",
                      true,
                      ax::log_core<ax::input_limbs<In>, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(log2,
                      (lower_of<In> > 0),
                      "input must be strictly positive",
                      true,
                      ax::logb_core<ax::input_limbs<In>, 2, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(log10,
                      (lower_of<In> > 0),
                      "input must be strictly positive",
                      true,
                      ax::logb_core<ax::input_limbs<In>, 10, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(asin,
                      (lower_of<In> >= -1 && upper_of<In> <= 1),
                      "input must be in [-1, 1]",
                      true,
                      ax::asin_core<ax::input_limbs<In>, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(acos,
                      (lower_of<In> >= -1 && upper_of<In> <= 1),
                      "input must be in [-1, 1]",
                      true,
                      ax::acos_core<ax::input_limbs<In>, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(acosh,
                      (lower_of<In> >= 1),
                      "input must be at least 1",
                      ax::in_max<In> <= 0x1p500,
                      ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::acosh, ax::input_bits<In>>)
BEMAN_INSIDE_AX_UNARY(atanh,
                      (lower_of<In> > -1 && upper_of<In> < 1),
                      "input must be in (-1, 1)",
                      true,
                      ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::atanh, ax::input_bits<In>>)
#undef BEMAN_INSIDE_AX_UNARY

// sqrt of a non-negative input.
template <insidable Out, insidable In>
    requires(lower_of<In> >= 0)
[[nodiscard]] constexpr Out sqrt_into(In x) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return sqrt_into<O>(v); }, x);
    else {
        BEMAN_INSIDE_AX_KERNEL_TIERS(Out,
                                     In,
                                     sqrt,
                                     x,
                                     true,
                                     ax::evaluate<Out, ax::start_bits<Out>>(
                                         ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{ax::exact_input(x)}))
    }
}

// sqrt of a mixed-sign input: domain_error on a negative value.
template <insidable Out, insidable In>
    requires(lower_of<In> < 0)
[[nodiscard]] constexpr std::expected<Out, errc> sqrt_into(In x) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return sqrt_into<O>(v); }, x);
    else {
        const auto v = ax::exact_input(x);
        if (v.Num.negative())
            return std::unexpected(errc::domain_error);
        return ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{v});
    }
}

// tan: overflow when the result leaves Out (without clamp).
template <insidable In>
inline constexpr bool tan_domain = true;
template <insidable Out, insidable In>
[[nodiscard]] constexpr std::expected<Out, errc> tan_into(In x) {
    require_rounding<Out>();
    static_assert(!has_flag(flags_of<Out>, wrap),
                  "beman::inside::math::tan: wrap is not supported - tan has no bound near its poles, so "
                  "the folded value cannot be computed; take an Out with clamp or the default policy");
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return tan_into<O>(v); }, x);
    else {
        using core = ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::tan, ax::out_kmax<Out>>;
        BEMAN_INSIDE_AX_TIERS(Out,
                              (ax::fp_tier<Out, ax::fp_tan, In> && ax::in_max<In> <= 0x1p20),
                              (ax::dd_tier<Out, In> && ax::in_max<In> <= 0x1p20),
                              (ax::fp_attempt_tan<Out>(x, r)),
                              (ax::dd_attempt_tan<Out>(x, r)),
                              ax::evaluate_checked<Out, ax::start_bits<Out>>(core{ax::exact_input(x)}))
    }
}

template <insidable Out, insidable InY, insidable InX>
[[nodiscard]] constexpr Out atan2_into(InY y, InX x) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InY, InX>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return atan2_into<O>(a, b); }, y, x);
    else {
        constexpr std::size_t E =
            ax::input_limbs<InY> > ax::input_limbs<InX> ? ax::input_limbs<InY> : ax::input_limbs<InX>;
        using F = ::beman::inside::detail::wide_rational<E>;
        BEMAN_INSIDE_AX_TIERS(
            Out,
            (ax::fp_tier<Out, ax::fp_atan2, InY, InX> && ax::in_max<InY> <= 0x1p500 && ax::in_max<InX> <= 0x1p500),
            (ax::dd_tier<Out, InY, InX>),
            (ax::fp_attempt_atan2<Out>(y, x, r)),
            (ax::dd_attempt_atan2<Out>(y, x, r)),
            ax::evaluate<Out, ax::start_bits<Out>>(ax::atan2_core<E>{F{ax::exact_input(y)}, F{ax::exact_input(x)}}))
    }
}

template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr Out hypot_into(InX x, InY y) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InX, InY>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return hypot_into<O>(a, b); }, x, y);
    else {
        constexpr int         Bits = ax::hypot_bits<InX, InY>;
        constexpr std::size_t E    = ::beman::inside::detail::limbs_for_bits(Bits);
        using F                    = ::beman::inside::detail::wide_rational<E>;
        BEMAN_INSIDE_AX_TIERS(
            Out,
            (ax::fp_tier<Out, ax::fp_hypot, InX, InY> && ax::in_max<InX> <= 0x1p500 && ax::in_max<InY> <= 0x1p500),
            (ax::dd_tier<Out, InX, InY>),
            (ax::fp_attempt_hypot<Out>(x, y, r)),
            (ax::dd_attempt_hypot<Out>(x, y, r)),
            ax::evaluate<Out, ax::start_bits<Out>>(ax::sqrt_core<E, Bits>{
                F{ax::exact_input(x)} * F{ax::exact_input(x)} + F{ax::exact_input(y)} * F{ax::exact_input(y)}}))
    }
}

// pow: 0^y is 0 for y > 0 and 1 for y = 0; division_by_zero for 0^y with
// y < 0 (a pole); domain_error for a negative base; overflow when the
// result leaves Out (without clamp).
template <insidable Out, insidable InB, insidable InE>
[[nodiscard]] constexpr std::expected<Out, errc> pow_into(InB base, InE exp) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InB, InE>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return pow_into<O>(a, b); }, base, exp);
    else {
        const auto integer = [&]() -> std::expected<Out, errc> {
            const auto b = ax::exact_input(base);
            if (b.Num.negative())
                return std::unexpected(errc::domain_error);
            if (b.Num.is_zero()) {
                const auto e = ax::exact_input(exp);
                if (e.Num.negative())
                    return std::unexpected(errc::division_by_zero);
                return Out::try_make(e.Num.is_zero() ? 1 : 0); // Out's policy: range, clamp
            }
            using core = ax::pow_core<ax::input_limbs<InB>,
                                      ax::input_limbs<InE>,
                                      ax::in_mag<InE>,
                                      ax::pow_out_kmax<Out, InB, InE>,
                                      ax::input_bits<InB>,
                                      ax::input_bits<InE>>;
            return ax::evaluate_checked<Out, ax::start_bits<Out>>(core{b, ax::exact_input(exp)});
        };
        BEMAN_INSIDE_AX_TIERS(Out,
                              (ax::fp_tier<Out, ax::fp_pow, InB, InE>),
                              (ax::dd_tier<Out, InB, InE>),
                              (ax::fp_attempt_pow<Out>(base, exp, r)),
                              (ax::dd_attempt_pow<Out>(base, exp, r)),
                              integer())
    }
}

// Base^x for a compile-time integer Base ≥ 2.
template <insidable Out, imax Base, insidable In>
[[nodiscard]] constexpr Out pow_base_into(In x) {
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return pow_base_into<O, Base>(v); }, x);
    else {
        using core = ax::pow_core<2,
                                  ax::input_limbs<In>,
                                  ax::in_mag<In>,
                                  ax::pow_base_out_kmax<Out, In, Base>,
                                  66,
                                  ax::input_bits<In>,
                                  Base>;
        BEMAN_INSIDE_AX_TIERS(Out,
                              (ax::fp_tier<Out, ax::fp_pow_base, In> && ax::in_max<In> <= 1000),
                              (ax::dd_tier<Out, In> && ax::in_max<In> <= 700),
                              (ax::fp_attempt_pow_base<Out, Base>(x, r)),
                              (ax::dd_attempt_pow_base<Out, Base>(x, r)),
                              ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_int<2>(Base), ax::exact_input(x)}))
    }
}

//---------------------------------------------------------------------------
// Auto forms: Out deduced from In — the range the function takes over In,
// rounded outward to In's notch, with In's notch and policy (rounding to
// nearest). sin and cos give [-1, 1], tan [-1024, 1024] (closer to a pole
// the expected reports overflow), atan2 [-π, π] on the gcd of both notches.
//---------------------------------------------------------------------------
template <insidable In>
consteval bool deducible() noexcept {
    static_assert(has_flag(flags_of<In>, snap),
                  "beman::inside::math: a deduced result is rounded onto the input's grid - its operand "
                  "must permit rounding (declare it with round_nearest, round_floor, ...)");
    static_assert(::beman::inside::detail::notched<In>,
                  "beman::inside::math: a deduced output takes the input's notch - the input needs one");
    return true;
}

namespace results {
template <insidable In>
using exp = ax::increasing_t<In, ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_kmax<In>>>;
template <insidable In>
using exp2 = ax::increasing_t<In, ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::exp_kmax<In>>>;
template <insidable In>
using log = ax::increasing_t<In, ax::log_core<ax::input_limbs<In>, ax::input_bits<In>>>;
template <insidable In>
using log2 = ax::increasing_t<In, ax::logb_core<ax::input_limbs<In>, 2, ax::input_bits<In>>>;
template <insidable In>
using log10 = ax::increasing_t<In, ax::logb_core<ax::input_limbs<In>, 10, ax::input_bits<In>>>;
template <insidable In>
using sqrt = ax::increasing_t<In, ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>>;
template <insidable In>
using cbrt = ax::increasing_t<In, ax::cbrt_core<ax::input_limbs<In>, ax::input_bits<In>>>;
template <insidable In>
using atan = ax::increasing_t<In, ax::atan_core<ax::input_limbs<In>>>;
template <insidable In>
using asin = ax::increasing_t<In, ax::asin_core<ax::input_limbs<In>, ax::input_bits<In>>>;
template <insidable In>
using acos = ax::decreasing_t<In, ax::acos_core<ax::input_limbs<In>, ax::input_bits<In>>>;
template <insidable In>
using sinh = ax::increasing_t<In, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::exp_kmax<In>>>;
template <insidable In>
using tanh = ax::increasing_t<In, ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::tanh, 1>>;
template <insidable In>
using asinh =
    ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::asinh, ax::input_bits<In>>>;
template <insidable In>
using acosh =
    ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::acosh, ax::input_bits<In>>>;
template <insidable In>
using atanh =
    ax::increasing_t<In, ax::ahyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::ahyp::atanh, ax::input_bits<In>>>;
template <insidable In>
using sin = inside<{{-1, 1}, notch_of<In>}, ax::result_policy<In>>;
template <insidable In>
using cos = sin<In>;
template <insidable In>
using tan = inside<{{-1024, 1024}, notch_of<In>}, ax::result_policy<In>>;

// cosh is even: its least value is 1 when In spans 0, else at the end
// nearer 0.
template <insidable In>
using cosh_core = ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::cosh, ax::exp_kmax<In>>;
template <insidable In>
inline constexpr grid_rational cosh_lo = (lower_of<In> <= 0 && upper_of<In> >= 0) ? grid_rational{1}
                                         : (lower_of<In> > 0) ? ax::bound_at<In, cosh_core<In>, false, false>
                                                              : ax::bound_at<In, cosh_core<In>, true, false>;
template <insidable In>
inline constexpr grid_rational cosh_hi =
    ax::bound_at<In, cosh_core<In>, false, true> > ax::bound_at<In, cosh_core<In>, true, true>
        ? ax::bound_at<In, cosh_core<In>, false, true>
        : ax::bound_at<In, cosh_core<In>, true, true>;
template <insidable In>
using cosh = ax::result_grid_t<In, cosh_lo<In>, cosh_hi<In>>;

// sqrt of a mixed-sign input: [0, √max(|Lower|, |Upper|)].
template <insidable In>
inline constexpr grid_rational max_abs = (-lower_of<In> > upper_of<In>) ? -lower_of<In> : upper_of<In>;
template <insidable In>
using sqrt_signed =
    ax::result_grid_t<In,
                      grid_rational{0},
                      ax::lattice_bound<notch_of<In>, true>(
                          ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{ax::grid_input<In>(max_abs<In>)})>;

// pow_base: pow_core with the base bound.
template <imax Base, std::size_t E, int Mag, int KMax>
struct pow_base_core : ax::pow_core<2, E, Mag, KMax> {
    constexpr pow_base_core(::beman::inside::detail::wide_rational<E> x)
        : ax::pow_core<2, E, Mag, KMax>{ax::exact_int<2>(Base), x} {}
};
template <imax Base, insidable In>
using pow_base_t = ax::increasing_t<
    In,
    pow_base_core<Base,
                  ax::input_limbs<In>,
                  ax::in_mag<In>,
                  static_cast<int>(ax::max_abs_int<In>) * std::bit_width(static_cast<umax>(Base)) + 2>>;

// atan2: [−π, π] rounded outward on the gcd notch (π = atan2(0, −1)).
template <insidable A, insidable B>
inline constexpr grid_rational pi_up =
    ax::lattice_bound<ax::gcd_notch<A, B>, true>(ax::atan2_core<1>{ax::exact_int<1>(0), ax::exact_int<1>(-1)});
template <insidable InY, insidable InX>
using atan2 = inside<{{-pi_up<InY, InX>, pi_up<InY, InX>}, ax::gcd_notch<InY, InX>}, ax::result_policy<InY>>;

// hypot: [0, hypot of the largest magnitudes] on the gcd notch.
template <insidable InX, insidable InY>
inline constexpr std::size_t hypot_limbs = ::beman::inside::detail::limbs_for_bits(ax::hypot_bits<InX, InY>);
template <insidable InX, insidable InY>
inline constexpr grid_rational hypot_hi = [] {
    using F   = ::beman::inside::detail::wide_rational<hypot_limbs<InX, InY>>;
    const F a = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InX>);
    const F b = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InY>);
    return ax::lattice_bound<ax::gcd_notch<InX, InY>, true>(
        ax::sqrt_core<hypot_limbs<InX, InY>, ax::hypot_bits<InX, InY>>{a * a + b * b});
}();
template <insidable InX, insidable InY>
using hypot = inside<{{0, hypot_hi<InX, InY>}, ax::gcd_notch<InX, InY>}, ax::result_policy<InX>>;

// pow: b^e is monotone in each argument for b > 0, so the extremes are at
// the corners of the input rectangle. Notch of the base.
template <insidable InB, insidable InE>
using pow_core = ax::pow_core<ax::input_limbs<InB>,
                              ax::input_limbs<InE>,
                              ax::in_mag<InE>,
                              ax::pow_kmax<InB, InE>,
                              ax::input_bits<InB>,
                              ax::input_bits<InE>>;
template <insidable InB, insidable InE, bool BUp, bool EUp, bool Up>
inline constexpr grid_rational pow_corner =
    ax::lattice_bound<notch_of<InB>, Up>(pow_core<InB, InE>{ax::grid_input<InB>(BUp ? upper_of<InB> : lower_of<InB>),
                                                            ax::grid_input<InE>(EUp ? upper_of<InE> : lower_of<InE>)});
// The least (Up: greatest) bound over the four corners.
template <insidable InB, insidable InE, bool Up>
inline constexpr grid_rational pow_extreme = [] {
    const std::initializer_list<grid_rational> c{pow_corner<InB, InE, false, false, Up>,
                                                 pow_corner<InB, InE, false, true, Up>,
                                                 pow_corner<InB, InE, true, false, Up>,
                                                 pow_corner<InB, InE, true, true, Up>};
    return Up ? std::max(c) : std::min(c);
}();
template <insidable InB, insidable InE>
using pow =
    inside<{{pow_extreme<InB, InE, false>, pow_extreme<InB, InE, true>}, notch_of<InB>}, ax::result_policy<InB>>;
} // namespace results

#define BEMAN_INSIDE_AX_AUTO(fn)                                                               \
    template <insidable In>                                                                    \
    [[nodiscard]] constexpr auto fn(In x) {                                                    \
        static_assert(deducible<In>());                                                        \
        if constexpr (fn##_domain<In>)                                                         \
            return fn##_into<results::fn<In>>(x);                                              \
        else                                                                                   \
            return fn##_into<inside<{0, 1}, round_nearest>>(x); /* the _into domain message */ \
    }

BEMAN_INSIDE_AX_AUTO(exp)
BEMAN_INSIDE_AX_AUTO(exp2)
BEMAN_INSIDE_AX_AUTO(sin)
BEMAN_INSIDE_AX_AUTO(cos)
BEMAN_INSIDE_AX_AUTO(tan)
BEMAN_INSIDE_AX_AUTO(atan)
BEMAN_INSIDE_AX_AUTO(sinh)
BEMAN_INSIDE_AX_AUTO(cosh)
BEMAN_INSIDE_AX_AUTO(tanh)
BEMAN_INSIDE_AX_AUTO(asinh)
BEMAN_INSIDE_AX_AUTO(cbrt)
BEMAN_INSIDE_AX_AUTO(log)
BEMAN_INSIDE_AX_AUTO(log2)
BEMAN_INSIDE_AX_AUTO(log10)
BEMAN_INSIDE_AX_AUTO(asin)
BEMAN_INSIDE_AX_AUTO(acos)
BEMAN_INSIDE_AX_AUTO(acosh)
BEMAN_INSIDE_AX_AUTO(atanh)
#undef BEMAN_INSIDE_AX_AUTO

template <insidable In>
    requires(lower_of<In> >= 0)
[[nodiscard]] constexpr auto sqrt(In x) {
    static_assert(deducible<In>());
    return sqrt_into<results::sqrt<In>>(x);
}

template <insidable In>
    requires(lower_of<In> < 0)
[[nodiscard]] constexpr auto sqrt(In x) {
    static_assert(deducible<In>());
    return sqrt_into<results::sqrt_signed<In>>(x);
}

template <imax Base, insidable In>
[[nodiscard]] constexpr auto pow_base(In x) {
    static_assert(deducible<In>());
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
    return pow_base_into<results::pow_base_t<Base, In>, Base>(x);
}

template <insidable InY, insidable InX>
    requires ax::gcd_notch_fits<InY, InX>
[[nodiscard]] constexpr auto atan2(InY y, InX x) {
    static_assert(deducible<InY>() && deducible<InX>());
    return atan2_into<results::atan2<InY, InX>>(y, x);
}

template <insidable InX, insidable InY>
    requires ax::gcd_notch_fits<InX, InY>
[[nodiscard]] constexpr auto hypot(InX x, InY y) {
    static_assert(deducible<InX>() && deducible<InY>());
    return hypot_into<results::hypot<InX, InY>>(x, y);
}

template <insidable InB, insidable InE>
    requires(lower_of<InB> > 0)
[[nodiscard]] constexpr auto pow(InB base, InE exp) {
    static_assert(deducible<InB>() && deducible<InE>());
    return pow_into<results::pow<InB, InE>>(base, exp);
}
} // namespace beman::inside::math::adaptive

#undef BEMAN_INSIDE_AX_TIERS
#undef BEMAN_INSIDE_AX_KERNEL_TIERS
#undef BEMAN_INSIDE_AX_TABLE

#endif // BEMAN_INSIDE_DETAIL_MATH_ENTRY_HPP
