// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// beman.inside 0.5.0 — single-header amalgamation
//
//   *** GENERATED FILE — DO NOT EDIT BY HAND ***
//
// Regenerate with:  cmake --build <build-dir> --target beman.inside.amalgamate
// Source of truth:  include/beman/inside/*.hpp, include/beman/inside/detail/*.hpp
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_SINGLE_HEADER_HPP
#define BEMAN_INSIDE_SINGLE_HEADER_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <meta>
#include <numeric>
#include <optional>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

// ======================================================================
//  beman/inside/inside.hpp
// ======================================================================
// Public umbrella header. Include this to get the full `beman::inside::inside` API:
// the core type (core.hpp) and its comparisons (detail/compare.hpp), plus the
// free-function layers that depend on the complete type — casts, arithmetic
// operators, inside_range and cursor. Those follow core.hpp because they
// need `inside<G, P>` fully defined.
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/detail/config.hpp
// ======================================================================

//---------------------------------------------------------------------------
// config — the build options and the platform-dependent macros in one place.
// The other headers test these, never a platform macro (__SIZEOF_INT128__,
// __FMA__, …) themselves. Every header includes this one first.
//
//   Build options       BEMAN_INSIDE_MATH_NO_FP, BEMAN_INSIDE_MATH_INSTRUMENT
//   Features            BEMAN_INSIDE_HAS_EXCEPTIONS, _HAS_INT128, _HAS_FMA
//
// The library is C++26 with static reflection (GCC 16, -freflection), written
// in the GNU dialect: [[gnu::cold]], __builtin_fabs, __builtin_*_overflow.
//---------------------------------------------------------------------------


#if !defined(__cpp_impl_reflection) || !__has_include(<meta>)
    #error "beman::inside needs C++26 static reflection: compile with g++-16 -std=c++26 -freflection"
#endif

//---------------------------------------------------------------------------
// Build options
//---------------------------------------------------------------------------
// BEMAN_INSIDE_MATH_NO_FP — no hardware floating point anywhere: the math
// engine's double and dd tiers compile out (its integer path computes every
// result; results do not change). Define it to force the FP-free build; it is
// auto-enabled on freestanding targets (__STDC_HOSTED__ == 0). Public API and
// grid deduction are unchanged.
#if !defined(BEMAN_INSIDE_MATH_NO_FP)
    #if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
        #define BEMAN_INSIDE_MATH_NO_FP
    #endif
#endif

// BEMAN_INSIDE_MATH_INSTRUMENT — 1 adds math::driver (detail/math_adaptive.hpp):
// run-time counts of the math engine's integer path, its deep steps and its
// guesses (results still undecided at the cap), and a switch that sends every
// call to the integer path. The certify tool (tests/beman/inside/certify) is
// built on it. 0, the default, compiles all of it out.
#if !defined(BEMAN_INSIDE_MATH_INSTRUMENT)
    #define BEMAN_INSIDE_MATH_INSTRUMENT 0
#endif

// -ffast-math is not supported. The library's results are exact or correctly
// rounded, and that rests on IEEE arithmetic as written: double sources are
// screened for infinities and NaN, and the math engine's error bounds and
// error-free sums count every rounding in program order. Fast-math lets the
// compiler assume no NaN or infinity and reassociate, which can change
// results silently, so a build that announces it stops here.
#if defined(__FAST_MATH__) || defined(__ASSOCIATIVE_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
    #error \
        "beman::inside does not support -ffast-math, -fassociative-math or -ffinite-math-only: its results rely on IEEE floating point as written"
#endif

// Excess precision is not supported either: the x87 unit (32-bit x86 without
// SSE2) evaluates double expressions at 80 bits and rounds them when it
// spills, so the same source rounds differently by register pressure.
#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ != 0
    #error \
        "beman::inside needs floating point evaluated in its own type (FLT_EVAL_METHOD 0): on 32-bit x86 compile for SSE2 (-msse2 -mfpmath=sse)"
#endif

//---------------------------------------------------------------------------
// Features
//---------------------------------------------------------------------------
#if defined(__cpp_exceptions)
    #define BEMAN_INSIDE_HAS_EXCEPTIONS 1
#else
    #define BEMAN_INSIDE_HAS_EXCEPTIONS 0
#endif

// unsigned __int128: the 64×64 → 128 products and divisions in one step; else
// (32-bit targets) a portable 32-bit split.
#if defined(__SIZEOF_INT128__)
    #define BEMAN_INSIDE_HAS_INT128 1
#else
    #define BEMAN_INSIDE_HAS_INT128 0
#endif

// A hardware fused multiply-add the compiler targets (math_fp.hpp).
#if defined(__FMA__) || defined(__ARM_FEATURE_FMA)
    #define BEMAN_INSIDE_HAS_FMA 1
#else
    #define BEMAN_INSIDE_HAS_FMA 0
#endif



// ======================================================================
//  beman/inside/core.hpp
// ======================================================================
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
// The core type beman::inside::inside<G, P>: construction and try_make,
// compound assignment, ++/--, the conversions (imax, rational, double,
// to<T>/as<T>), numerator()/denominator(), raw()/from_raw, the per-store
// overrides (with_clamp, with_snap, policy<F>) and hooks (on_wrap, on_clamp,
// on_error, on_overflow), parse(first, last), just<…> and the _ins literal.
// The umbrella adds the casts, arithmetic, range and cursor layers, which
// need this complete type.
//---------------------------------------------------------------------------



// ======================================================================
//  beman/inside/generic.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/introspection.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/debug.hpp
// ======================================================================


// <stdexcept> backs the *default* throwing handler only; it is the single heavy
// error include and is pulled in solely when exceptions are available. A
// freestanding / -fno-exceptions build never sees it and traps instead. Error
// reporting otherwise stays on static const-char* messages — no <string>, no
// <string_view>, no <system_error>. (General-purpose stringification lives in
// the opt-in "beman/inside/io.hpp".)
#if BEMAN_INSIDE_HAS_EXCEPTIONS // (config.hpp)
    #include <stdexcept>
#endif

//---------------------------------------------------------------------------
// debug — error codes, the replaceable failure handler, and diagnostic helpers.
// `errc` enumerates the failure modes. Every runtime failure funnels through
// `detail::raise`, which calls the installed `error_handler` — by default one
// that throws `beman::inside::inside_error` (hosted) or traps (freestanding / no
// exceptions). `set_error_handler` lets a bare-metal target redirect failures
// to a reset/log without depending on <system_error> or the C++ exception ABI.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// error codes
//---------------------------------------------------------------------------
enum class errc {
    domain_error = 1, // argument outside a function's mathematical domain
    division_by_zero, // divisor is zero
    overflow,         // value does not fit its destination's range (an inside's
                      // interval, a native type, a rational's 64-bit fields)
    rounding_error,   // value is not on the grid (between notches)
    not_finite,       // non-finite double input (NaN/Inf)
    invalid_format,   // text that is not a number (parse, operator>>)
};

// Static, allocation-free message per code. The single source of truth for
// every error path; returns a null-terminated string literal so it doubles as
// the default exception's what() and as an on_error message.
constexpr const char* errc_message(errc e) noexcept {
    switch (e) {
    case errc::domain_error:
        return "argument outside the function's domain";
    case errc::division_by_zero:
        return "division by zero";
    case errc::overflow:
        return "value does not fit its range";
    case errc::rounding_error:
        return "value is not on the grid";
    case errc::not_finite:
        return "non-finite floating-point value";
    case errc::invalid_format:
        return "malformed number";
    }
    return "unknown inside error";
}

#if BEMAN_INSIDE_HAS_EXCEPTIONS
//---------------------------------------------------------------------------
// inside_error — the exception thrown by the default handler. Derives from
// std::runtime_error (the library's only <stdexcept> use) and carries the
// originating `errc`: `catch (inside_error& e) { e.Code; }`.
//---------------------------------------------------------------------------
struct inside_error : std::runtime_error {
    errc Code;
    explicit inside_error(errc c) : std::runtime_error(errc_message(c)), Code(c) {}
    inside_error(errc c, const char* what) : std::runtime_error(what ? what : errc_message(c)), Code(c) {}
};
#endif

//---------------------------------------------------------------------------
// Replaceable failure handler. Contract: it must NOT return (throw / longjmp /
// abort / reset). If it does return, `raise` traps to honour [[noreturn]].
//---------------------------------------------------------------------------
using error_handler_t = void (*)(errc code, const char* what);

namespace detail {
[[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void default_error_handler(errc code, const char* what) {
#if BEMAN_INSIDE_HAS_EXCEPTIONS
    throw inside_error(code, what);
#else
    (void)code;
    (void)what;
    __builtin_trap();
#endif
}

// Header-only global: an inline variable, one per program. Mutating it is a
// runtime-only act (constant evaluation never reads it), so constexpr paths
// are unaffected.
inline error_handler_t g_error_handler = &default_error_handler;
} // namespace detail

// Install a handler; returns the previous one. A null argument restores the
// default. Never throws.
inline error_handler_t set_error_handler(error_handler_t h) noexcept {
    error_handler_t prev    = detail::g_error_handler;
    detail::g_error_handler = h ? h : &detail::default_error_handler;
    return prev;
}

inline error_handler_t get_error_handler() noexcept { return detail::g_error_handler; }

namespace detail {
//-----------------------------------------------------------------------
// Outlined failure funnel. Cold + non-inline so the (rare) machinery is
// emitted once, off the hot path. Not constexpr: calling it during constant
// evaluation is ill-formed, which is exactly how the checked compile-time
// paths hard-fail the build.
//-----------------------------------------------------------------------
[[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void raise(errc code, const char* what = nullptr) {
    g_error_handler(code, what ? what : errc_message(code));
    __builtin_trap(); // handler must not return; trap if it did
}

//-----------------------------------------------------------------------
// Compile-time-only diagnostic. A fixed_string NTTP carries the message
// into the type, so a malformed literal / overflow aborts constant
// evaluation with the text in the instantiation — no `throw` token, so it
// also compiles under -fno-exceptions. Never reached at runtime (all call
// sites are guarded by std::is_constant_evaluated or are consteval).
//-----------------------------------------------------------------------
template <unsigned N>
struct fixed_string {
    char Data[N]{};
    constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, Data); }
};

template <fixed_string Msg>
[[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void constexpr_error() {
    raise(errc::overflow, Msg.Data);
}

// A checked path's hard failure: Msg as the compile-time diagnostic during
// constant evaluation, else raised with code.
template <fixed_string Msg>
[[noreturn]] constexpr void fail_hard(errc code) {
    if consteval {
        constexpr_error<Msg>();
    }
    raise(code, Msg.Data);
}
} // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/grid.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/rational.hpp
// ======================================================================



// ======================================================================
//  beman/inside/numbers.hpp
// ======================================================================



//---------------------------------------------------------------------------
// numbers — primitive numeric utilities: umax/imax, the plain_number /
// fractional concepts and safe_abs. The floating-point bit helpers are in
// detail/float_bits.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
using umax = std::uint64_t;
using imax = std::int64_t;

namespace detail {
struct rational;

// A plain number a grid-less value can be: integral, floating point, or the
// library's exact rational. Internal; the public operand concept is `numeric`.
template <typename T>
concept plain_number = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;

// Subset of arithmetic excluding integrals — the rhs types that need the
// rational-arithmetic assignment path.
template <typename T>
concept fractional = std::floating_point<T> || std::same_as<rational, T>;

template <std::signed_integral V>
[[nodiscard]] constexpr umax safe_abs(V value) noexcept {
    return (value >= 0) ? static_cast<umax>(value) : umax{0} - static_cast<umax>(value);
}
} // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/float_bits.hpp
// ======================================================================




//---------------------------------------------------------------------------
// float_bits — floating-point values read and built from their IEEE-754
// bits, with no <cmath>: constexpr frexp/ldexp, scale_pow2 for the kernels,
// finiteness, abs_fraction (the double → rational engine behind
// rational(double)), double_parts and narrow_midpoint.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// The binary64 fields: the sign bit, the 52 stored significand bits, and the
// 11-bit biased exponent (0: zero or subnormal, 0x7FF: infinity or NaN).
inline constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
inline constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
constexpr int biased_exponent(std::uint64_t bits) noexcept { return static_cast<int>((bits >> 52) & 0x7FF); }

// A finite |value| as Significand·2^Exp2, exactly: the stored bits with the
// implicit 1 of a normal double (a subnormal has none).
struct float_fields {
    umax Significand;
    int  Exp2;
};
constexpr float_fields abs_fields(double value) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const int  e    = biased_exponent(bits);
    const umax m    = bits & mantissa_mask;
    return e == 0 ? float_fields{m, 1 - 1023 - 52} : float_fields{m | umax{1} << 52, e - 1023 - 52};
}

inline constexpr double frexp(double value, int* exp) noexcept {
    if (value == 0.0) {
        *exp = 0;
        return value;
    }

    auto bits = std::bit_cast<std::uint64_t>(value);
    auto e    = biased_exponent(bits);

    if (e == 0x7FF) {
        *exp = 0;
        return value;
    }

    if (e == 0) {
        // subnormal: scale up, recurse
        double scaled = value * 0x1p53;
        double result = frexp(scaled, exp);
        *exp -= 53;
        return result;
    }

    *exp = e - 0x3FE;
    bits = (bits & (sign_mask | mantissa_mask)) | (std::uint64_t{0x3FE} << 52);
    return std::bit_cast<double>(bits);
}

// value·2^exp for any exp, rounded to nearest even. Past ±2200 every finite
// double overflows or underflows alike, so exp is clamped there (an exp near
// INT_MAX would overflow the exponent sum below).
constexpr double ldexp(double value, int exp) noexcept {
    exp = exp > 2200 ? 2200 : exp < -2200 ? -2200 : exp;
    if (value == 0.0 || exp == 0)
        return value;

    auto bits = std::bit_cast<std::uint64_t>(value);
    auto e    = biased_exponent(bits);

    if (e == 0x7FF)
        return value; // inf or NaN

    // Normalize subnormals
    int extra = 0;
    if (e == 0) {
        bits  = std::bit_cast<std::uint64_t>(value * 0x1p53);
        e     = biased_exponent(bits);
        extra = -53;
    }

    int new_exp = e + exp + extra;

    if (new_exp >= 0x7FF) {
        // overflow → ±inf
        return (bits & sign_mask) ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }

    if (new_exp > 0) {
        // normal result
        bits = (bits & (sign_mask | mantissa_mask)) | (static_cast<std::uint64_t>(new_exp) << 52);
        return std::bit_cast<double>(bits);
    }

    // Subnormal or underflow
    auto mantissa = (bits & mantissa_mask) | (std::uint64_t{1} << 52);
    int  shift    = 1 - new_exp;

    if (shift > 53)
        return std::bit_cast<double>(bits & sign_mask); // ±0

    // Round-to-nearest-even
    std::uint64_t dropped = mantissa & ((std::uint64_t{1} << shift) - 1);
    mantissa >>= shift;
    std::uint64_t halfway = std::uint64_t{1} << (shift - 1);
    if (dropped > halfway || (dropped == halfway && (mantissa & 1)))
        ++mantissa;

    bits = (bits & sign_mask) | mantissa;
    return std::bit_cast<double>(bits);
}

// value·2^k where 2^k is a normal double (−1022 ≤ k ≤ 1023): one multiply,
// which rounds once like ldexp's round-to-nearest-even; else ldexp. For the
// run-time kernels (the exp family).
constexpr double scale_pow2(double value, int k) noexcept {
    if (k >= -1022 && k <= 1023) [[likely]]
        return value * std::bit_cast<double>(static_cast<std::uint64_t>(k + 1023) << 52);
    return ldexp(value, k);
}

// Freestanding finite check: `v - v` is 0 for every finite v, NaN otherwise
// (and inf - inf is NaN). Avoids <cmath>/std::isfinite so the core stays
// bare-metal clean; the same idiom is used in the store paths.
template <std::floating_point T>
constexpr bool is_finite(T v) noexcept {
    return v - v == 0;
}

// |v| ≥ 2^64 or NaN: past every 64-bit integer and rational (a grid within
// them is decided by the sign alone).
template <std::floating_point T>
constexpr bool past_64_bits(T v) noexcept {
    return !(v < static_cast<T>(0x1p64) && v > static_cast<T>(-0x1p64));
}

// Whether abs_fraction keeps a finite double exactly below 2^64: its reduced
// denominator is at most 2^62 (the bits past that are dropped). Every |value|
// ≥ 2^-10 qualifies; smaller ones need enough trailing zero bits.
constexpr bool abs_fraction_exact(double value) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(value) & ~sign_mask;
    if (biased_exponent(bits) >= 1023 - 10 || bits == 0)
        return true;
    const auto [significand, exp2] = abs_fields(value);
    return exp2 + std::countr_zero(significand) >= -62;
}

// Exact conversion of a finite double to num/den (den a power of two), the
// engine behind rational(double). A finite double is exactly
// `significand · 2^exp2` (a 53-bit significand), both read straight from the
// IEEE-754 bits — no <cmath>, no FPU rounding, bit-identical across platforms.
constexpr std::pair<umax, umax> abs_fraction(double value, bool& exact) {
    if (not is_finite(value))
        raise(errc::not_finite, "beman::inside::detail::abs_fraction: non-finite double");

    if (value == 0.0)
        return {0, 1};
    auto [significand, exp2] = abs_fields(value); // |value|; the sign is the caller's job

    // value == significand * 2^exp2. Re-express as num/den with den = 2^k.
    if (exp2 >= 0) // integer-valued: scale up, den = 1
    {
        // |value| ≥ 2^64 has no 64-bit numerator: fail rather than wrap the shift.
        if (exp2 > 64 - std::bit_width(significand)) {
            if consteval {
                // Clang prints only the first ~34 characters: lead with the cause.
                constexpr_error<"no 64-bit rational for a double of magnitude 2^64 or more">();
            }
            raise(errc::overflow, "beman::inside::detail::abs_fraction: |double| >= 2^64");
        }
        return {significand << exp2, 1};
    }

    // exp2 < 0 → den = 2^(-exp2), capped at 2^62 (den is stored signed). Beyond
    // the cap the value is too small to keep: drop the significand's low bits
    // (shift ≥ 64 folds to zero). Reachable for every subnormal and normals
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalizes to 0/1).
    int           den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow) {
        const int drop = den_pow - max_pow;
        exact          = drop <= std::countr_zero(significand); // only zero bits dropped
        significand    = (drop >= 64) ? 0 : (significand >> drop);
        // Return canonical {0,1}: callers take this verbatim, so a non-canonical
        // {0, 2^62} would break the structural rational::operator==.
        if (significand == 0)
            return {0, 1};
        den_pow = max_pow;
    }
    umax den = umax{1} << den_pow;

    // den is a power of two, so the whole reduction is one shift by the
    // shared factor count (bounded by den's exponent). Plain ternary — no
    // <algorithm> in this core header.
    if (significand != 0) {
        const int trailing = std::countr_zero(significand);
        const int shift    = trailing < den_pow ? trailing : den_pow;
        significand >>= shift;
        den >>= shift;
    }
    return {significand, den};
}
constexpr std::pair<umax, umax> abs_fraction(double value) {
    bool exact = true;
    return abs_fraction(value, exact);
}

// A floating point wider than double (long double) whose value is a double.
template <std::floating_point T>
[[nodiscard]] constexpr bool is_double_value(T v) noexcept {
    return static_cast<T>(static_cast<double>(v)) == v; // NaN: false
}

// A floating point wider than double: long double on x87 and IEEE quad, whose
// bits past a double's every exact path keeps.
template <typename T>
concept wide_float = std::floating_point<T> && (sizeof(T) > sizeof(double));

// Whether a finite double d lies halfway between two adjacent values of a
// narrower T (float): there static_cast<T>(d) breaks a tie that d's source
// value may not have, so a value rounded to d first must round again from its
// exact value. f and the other neighbour f + 2(d − f) are exact in double.
template <std::floating_point T>
[[nodiscard]] constexpr bool narrow_midpoint(double d) noexcept {
    const T f = static_cast<T>(d);
    if (!is_finite(f))
        return true; // past T's largest finite: decide exactly
    const double g = static_cast<double>(f) + 2 * (d - static_cast<double>(f));
    return g != static_cast<double>(f) && static_cast<double>(static_cast<T>(g)) == g;
}

// A finite double as |d| = Mant·2^Exp, Mant of 53 bits ({0, 0} for 0).
struct double_parts_t {
    umax Mant;
    int  Exp;
};
[[nodiscard]] constexpr double_parts_t double_parts(double d) noexcept {
    const auto [m, exp2] = abs_fields(d);
    if (m == 0)
        return {0, 0};
    const int shift = 53 - std::bit_width(m); // a subnormal's leading 1 moves up
    return {m << shift, exp2 - shift};
}

// The doubles a floating point splits into, each the rounding error of the
// last: f(part) for each, until a part is 0, f returns false, or 8 parts.
// Returns what is left (0 when every part was taken).
template <std::floating_point T, typename F>
constexpr T for_each_double_part(T v, F f) {
    T rem = v;
    for (int i = 0; i < 8 && rem != 0; ++i) {
        const double d = static_cast<double>(rem);
        if (d == 0 || !f(d))
            break;
        rem -= static_cast<T>(d);
    }
    return rem;
}

} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/lift.hpp
// ======================================================================




//---------------------------------------------------------------------------
// lift — monadic composition for `std::expected<T, errc>`. `lift(op, args...)`
// unwraps each expected arg, calls `op`, and re-wraps; the first arg (left to
// right) holding an error short-circuits with that error. An `op` already
// returning expected<R> is forwarded as-is, so the inner operation reports
// its own cause. Internal: it backs grid division and the expected-lift
// operators; user code chains through those.
//
// std::expected is larger than its value (flag + errc), so it only ever
// travels as a parameter or return value — never as stored state.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <class T>
struct is_expected : std::false_type {};
template <class T, class E>
struct is_expected<std::expected<T, E>> : std::true_type {};

template <class T>
inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

// Any std::expected specialization (cv/ref-stripped).
template <typename T>
concept expected_like = is_expected_v<T>;

// strip expected<X, E> down to X, leave non-expected unchanged
template <class T>
struct unwrap {
    using type = T;
};
template <class T, class E>
struct unwrap<std::expected<T, E>> {
    using type = T;
};
template <class T>
using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

template <class T>
constexpr decltype(auto) lift_unwrap(T&& v) {
    if constexpr (expected_like<T>)
        return *std::forward<T>(v);
    else
        return std::forward<T>(v);
}

// Copy an arg's error into `e`; true if the arg holds one.
template <class T>
constexpr bool lift_take_error([[maybe_unused]] const T& v, [[maybe_unused]] errc& e) {
    if constexpr (expected_like<T>)
        if (!v.has_value()) {
            e = v.error();
            return true;
        }
    return false;
}

template <class Op, class... Args>
[[nodiscard]] constexpr auto lift(Op op, Args&&... args) {
    using R   = std::remove_cvref_t<decltype(op(lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<is_expected_v<R>, R, std::expected<R, errc>>;

    errc e{};
    if ((lift_take_error(args, e) || ...))
        return Ret{std::unexpected{e}};

    if constexpr (expected_like<R>)
        return op(lift_unwrap(std::forward<Args>(args))...);
    else
        return Ret{op(lift_unwrap(std::forward<Args>(args))...)};
}
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/detail/overflow.hpp
// ======================================================================
// This is derived from Peter Sommerlads odins.h, allowed by MIT license
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// overflow — overflow-detecting add/sub/mul for integers: the GCC/Clang
// __builtin_*_overflow intrinsics. Return true on overflow; *result holds the
// wrapped value either way. Used by rational::*_impl and every checked path.
//---------------------------------------------------------------------------
namespace beman::inside {
template <std::integral T>
[[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept {
    return __builtin_add_overflow(l, r, result);
}

template <std::integral T>
[[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept {
    return __builtin_sub_overflow(l, r, result);
}

template <std::integral T>
[[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept {
    return __builtin_mul_overflow(l, r, result);
}
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/wide_int.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/limb_algorithms.hpp
// ======================================================================
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.




//---------------------------------------------------------------------------
// The library's own integer algorithms: the limb primitives and array
// kernels shared by wide_int, big_rational's magnitudes, the adaptive math
// tier's fixed-point arrays and the text output; and three value algorithms
// for any of its integer types (Euclid's gcd, stripping a factor, a power by
// squaring).
//---------------------------------------------------------------------------
namespace beman::inside::detail {
namespace limb {
template <std::unsigned_integral L>
inline constexpr int bits = std::numeric_limits<L>::digits;

template <std::unsigned_integral L>
struct pair {
    L Hi;
    L Lo;
};

// a + b + carry; carry is 0 or 1 in and out.
template <std::unsigned_integral L>
constexpr L add_carry(L a, L b, L& carry) noexcept {
    const L s  = static_cast<L>(a + b);
    const L s2 = static_cast<L>(s + carry);
    carry      = static_cast<L>((s < a) + (s2 < s));
    return s2;
}

// a − b − borrow; borrow is 0 or 1 in and out.
template <std::unsigned_integral L>
constexpr L sub_borrow(L a, L b, L& borrow) noexcept {
    const L d  = static_cast<L>(a - b);
    const L d2 = static_cast<L>(d - borrow);
    borrow     = static_cast<L>((a < b) + (d < borrow));
    return d2;
}

// Full product a·b as {hi, lo}. Native where the target has unsigned
// __int128, else a schoolbook 32-bit split.
template <std::unsigned_integral L>
constexpr pair<L> mul(L a, L b) noexcept {
    if constexpr (bits<L> <= 32) {
        const std::uint64_t p = std::uint64_t{a} * b;
        return {static_cast<L>(p >> bits<L>), static_cast<L>(p)};
    } else {
        static_assert(bits<L> == 64, "wide_int: limbs are at most 64 bits");
#if BEMAN_INSIDE_HAS_INT128
        const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
        return {static_cast<L>(p >> 64), static_cast<L>(p)};
#else
        const L al = a & 0xffffffffu, ah = a >> 32;
        const L bl = b & 0xffffffffu, bh = b >> 32;
        const L ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
        const L mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
        return {hh + (lh >> 32) + (hl >> 32) + (mid >> 32), (ll & 0xffffffffu) | (mid << 32)};
#endif
    }
}

// a·b <=> c·d, exactly (the native 128-bit compare where available).
constexpr std::strong_ordering mul_compare(umax a, umax b, umax c, umax d) noexcept {
#if BEMAN_INSIDE_HAS_INT128
    return static_cast<unsigned __int128>(a) * b <=> static_cast<unsigned __int128>(c) * d;
#else
    const pair<umax> x = mul(a, b), y = mul(c, d);
    return x.Hi != y.Hi ? x.Hi <=> y.Hi : x.Lo <=> y.Lo;
#endif
}

// {hi, lo} / d and the remainder. Requires hi < d, so the quotient fits L.
template <std::unsigned_integral L>
constexpr pair<L> div(L hi, L lo, L d) noexcept // {quotient, remainder}
{
    if constexpr (bits<L> <= 32) {
        const std::uint64_t n = (std::uint64_t{hi} << bits<L>) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
    } else {
        if (hi == 0)
            return {static_cast<L>(lo / d), static_cast<L>(lo % d)}; // one machine division
#if BEMAN_INSIDE_HAS_INT128
        const unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
#else
        // Restoring shift-subtract; `top` keeps the bit shifted out of r.
        L r = hi, q = 0;
        for (int i = 63; i >= 0; --i) {
            const bool top = (r >> 63) != 0;
            r              = (r << 1) | ((lo >> i) & 1u);
            q <<= 1;
            if (top || r >= d) {
                r -= d;
                q |= 1u;
            }
        }
        return {q, r};
#endif
    }
}

//---------------------------------------------------------------------------
// Array kernels over (pointer, length), least significant limb first, in
// the style of GMP's mpn layer. An output may alias an input of the same
// position where noted.
//---------------------------------------------------------------------------
// r[0, n) = a + b (r may alias a or b); the carry out.
template <std::unsigned_integral L>
constexpr L add_n(L* r, const L* a, const L* b, std::size_t n) noexcept {
    L c = 0;
    for (std::size_t i = 0; i < n; ++i)
        r[i] = add_carry(a[i], b[i], c);
    return c;
}
// r[0, n) = a − b (r may alias a or b); the borrow out.
template <std::unsigned_integral L>
constexpr L sub_n(L* r, const L* a, const L* b, std::size_t n) noexcept {
    L br = 0;
    for (std::size_t i = 0; i < n; ++i)
        r[i] = sub_borrow(a[i], b[i], br);
    return br;
}
// r[0, an) = a + b[0, bn) for an ≥ bn; the carry out.
template <std::unsigned_integral L>
constexpr L add(L* r, const L* a, std::size_t an, const L* b, std::size_t bn) noexcept {
    L c = add_n(r, a, b, bn);
    for (std::size_t i = bn; i < an; ++i)
        r[i] = add_carry(a[i], L{0}, c);
    return c;
}
// r[0, an) = a − b[0, bn) for an ≥ bn; the borrow out.
template <std::unsigned_integral L>
constexpr L sub(L* r, const L* a, std::size_t an, const L* b, std::size_t bn) noexcept {
    L br = sub_n(r, a, b, bn);
    for (std::size_t i = bn; i < an; ++i)
        r[i] = sub_borrow(a[i], L{0}, br);
    return br;
}
// r[0, n) = a·q (r may alias a); the top limb.
template <std::unsigned_integral L>
constexpr L mul_1(L* r, const L* a, std::size_t n, L q) noexcept {
    L carry = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const pair<L> t = mul(a[i], q);
        L             c = 0;
        r[i]            = add_carry(t.Lo, carry, c);
        carry           = static_cast<L>(t.Hi + c);
    }
    return carry;
}
// r[0, n) += a·q; the carry out of the top limb.
template <std::unsigned_integral L>
constexpr L addmul_1(L* r, const L* a, std::size_t n, L q) noexcept {
    L carry = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const pair<L> p  = mul(q, a[i]);
        L             c1 = 0, c2 = 0;
        L             s = add_carry(r[i], p.Lo, c1);
        s               = add_carry(s, carry, c2);
        r[i]            = s;
        carry           = static_cast<L>(p.Hi + c1 + c2); // fits: (b−1)² + 2(b−1) < b²
    }
    return carry;
}
// r[0, an + bn) = a·b (schoolbook; r aliases neither).
template <std::unsigned_integral L>
constexpr void mul_basecase(L* r, const L* a, std::size_t an, const L* b, std::size_t bn) noexcept {
    for (std::size_t i = 0; i < an + bn; ++i)
        r[i] = 0;
    for (std::size_t i = 0; i < an; ++i)
        r[i + bn] = addmul_1(r + i, b, bn, a[i]);
}
// q[0, n) = a / d, top down (q may alias a); a mod d.
template <std::unsigned_integral L>
constexpr L div_1(L* q, const L* a, std::size_t n, L d) noexcept {
    L rem = 0;
    for (std::size_t i = n; i-- > 0;) {
        const pair<L> t = div(rem, a[i], d);
        q[i]            = t.Hi;
        rem             = t.Lo;
    }
    return rem;
}
// r[0, rn) = a[0, an) >> s, the limbs past an read as fill (0, or a sign).
template <std::unsigned_integral L>
constexpr void shift_right(L* r, std::size_t rn, const L* a, std::size_t an, int s, L fill = 0) noexcept {
    constexpr int     B  = bits<L>;
    const std::size_t ws = static_cast<std::size_t>(s / B);
    const int         bs = s % B;
    for (std::size_t i = 0; i < rn; ++i) {
        const std::size_t k  = i + ws;
        const L           lo = k < an ? a[k] : fill;
        if (bs == 0)
            r[i] = lo;
        else {
            const L hi = k + 1 < an ? a[k + 1] : fill;
            r[i]       = static_cast<L>(static_cast<L>(lo >> bs) | static_cast<L>(hi << (B - bs)));
        }
    }
}
// r[0, rn) = a[0, an) << s, the bits past rn dropped. r starts at 0 (its
// limbs below the shift are not written) and aliases nothing.
template <std::unsigned_integral L>
constexpr void shift_left(L* r, std::size_t rn, const L* a, std::size_t an, int s) noexcept {
    constexpr int     B  = bits<L>;
    const std::size_t ws = static_cast<std::size_t>(s / B);
    const int         bs = s % B;
    for (std::size_t i = ws; i < rn; ++i) {
        const std::size_t k  = i - ws;
        const L           lo = k < an ? a[k] : L{0};
        if (bs == 0 || k == 0)
            r[i] = static_cast<L>(lo << bs);
        else {
            const L below = k - 1 < an ? a[k - 1] : L{0};
            r[i]          = static_cast<L>(static_cast<L>(lo << bs) | static_cast<L>(below >> (B - bs)));
        }
    }
}
// The significant length of a[0, n): n without its top zero limbs.
template <std::unsigned_integral L>
constexpr std::size_t top_size(const L* a, std::size_t n) noexcept {
    while (n > 0 && a[n - 1] == 0)
        --n;
    return n;
}

// Knuth's algorithm D (TAOCP 4.3.1). u has m limbs, v has n limbs with
// v[n−1] != 0 and m >= n. Writes the m−n+1 quotient limbs to q and the n
// remainder limbs to r. The caller provides scratch: un (m+1) and vn (n).
template <std::unsigned_integral L>
constexpr void divmod(const L* u, std::size_t m, const L* v, std::size_t n, L* q, L* r, L* un, L* vn) noexcept {
    constexpr int B = bits<L>;
    if (n == 1) { // (written out: through div_1, GCC loses that this returns for n == 1)
        L rem = 0;
        for (std::size_t i = m; i-- > 0;) {
            const pair<L> d = div(rem, u[i], v[0]);
            q[i]            = d.Hi;
            rem             = d.Lo;
        }
        r[0] = rem;
        return;
    }
    // Normalise: shift so the divisor's top limb has its high bit set.
    const int  s   = std::countl_zero(v[n - 1]);
    const auto shl = [&](L hi, L lo) -> L {
        return s == 0 ? hi : static_cast<L>(static_cast<L>(hi << s) | static_cast<L>(lo >> (B - s)));
    };
    for (std::size_t i = n - 1; i > 0; --i)
        vn[i] = shl(v[i], v[i - 1]);
    vn[0] = static_cast<L>(v[0] << s);
    un[m] = (s == 0) ? L{0} : static_cast<L>(u[m - 1] >> (B - s));
    for (std::size_t i = m - 1; i > 0; --i)
        un[i] = shl(u[i], u[i - 1]);
    un[0] = static_cast<L>(u[0] << s);

    for (std::size_t j = m - n + 1; j-- > 0;) {
        // Estimate q̂ from the top two dividend limbs, then correct it with the
        // next limb (at most two steps).
        L    qhat, rhat;
        bool rhat_big = false; // r̂ ≥ base: skip the test
        if (un[j + n] == vn[n - 1]) {
            qhat     = static_cast<L>(~L{0});
            rhat     = static_cast<L>(un[j + n - 1] + vn[n - 1]);
            rhat_big = rhat < vn[n - 1];
        } else {
            const pair<L> d = div(un[j + n], un[j + n - 1], vn[n - 1]);
            qhat            = d.Hi;
            rhat            = d.Lo;
        }
        while (!rhat_big) {
            const pair<L> p = mul(qhat, vn[n - 2]);
            if (p.Hi < rhat || (p.Hi == rhat && p.Lo <= un[j + n - 2]))
                break;
            --qhat;
            const L old = rhat;
            rhat        = static_cast<L>(rhat + vn[n - 1]);
            rhat_big    = rhat < old;
        }
        // Multiply and subtract q̂·vn from un[j .. j+n].
        L carry = 0, borrow = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const pair<L> p  = mul(qhat, vn[i]);
            L             c  = 0;
            const L       lo = add_carry(p.Lo, carry, c);
            carry            = static_cast<L>(p.Hi + c);
            un[i + j]        = sub_borrow(un[i + j], lo, borrow);
        }
        un[j + n] = sub_borrow(un[j + n], carry, borrow);
        if (borrow != 0) // q̂ was one too large: add back
        {
            --qhat;
            L c = 0;
            for (std::size_t i = 0; i < n; ++i)
                un[i + j] = add_carry(un[i + j], vn[i], c);
            un[j + n] = static_cast<L>(un[j + n] + c);
        }
        q[j] = qhat;
    }
    // Denormalise the remainder.
    for (std::size_t i = 0; i + 1 < n; ++i)
        r[i] = (s == 0) ? un[i] : static_cast<L>(static_cast<L>(un[i] >> s) | static_cast<L>(un[i + 1] << (B - s)));
    r[n - 1] = static_cast<L>(un[n - 1] >> s);
}
} // namespace limb

// gcd(a, b) of non-negative a and b by Euclid (0 when both are), with
// rem(a, b) the remainder.
inline constexpr auto remainder_of = [](const auto& a, const auto& b) { return a % b; };
template <class I, class Rem = decltype(remainder_of)>
constexpr I euclid_gcd(I a, I b, Rem rem = remainder_of) {
    const auto zero = [](const I& v) {
        if constexpr (requires { v.is_zero(); })
            return v.is_zero();
        else
            return v.empty();
    };
    while (!zero(b)) {
        I t = rem(a, b);
        a   = std::move(b);
        b   = std::move(t);
    }
    return a;
}

// x divided by p as often as p divides it (x ≠ 0); how often. A wide x
// (one with Word limbs) divides limb-wise; any other integer type by its
// own % and /.
template <class I>
constexpr imax strip_factor(I& x, umax p) {
    imax k = 0;
    if constexpr (requires { x.Word; }) {
        constexpr std::size_t N = sizeof(x.Word) / sizeof(x.Word[0]);
        if (p == 2) {
            std::size_t w = 0;
            while (w < N && x.Word[w] == 0)
                ++w;
            const int s = static_cast<int>(w) * 64 + (w < N ? std::countr_zero(x.Word[w]) : 0);
            x           = x >> s;
            return s;
        }
        for (;;) {
            umax       q[N];
            const umax r = limb::div_1(q, x.Word, N, p);
            if (r != 0)
                return k;
            std::copy_n(q, N, x.Word);
            ++k;
        }
    } else {
        const I d{p};
        for (; x % d == I{0}; x = x / d)
            ++k;
        return k;
    }
}

// x^n for n ≥ 0 by squaring, with mul(a, b) the product and one the unit.
// For exact multiplication only: a rounded mul would round differently
// from the repeated product.
template <class T, class Mul>
constexpr T pow_by_squaring(T x, unsigned n, Mul mul, T one) {
    T r = std::move(one);
    while (n != 0) {
        if (n & 1u)
            r = mul(r, x);
        n >>= 1;
        if (n != 0)
            x = mul(x, x);
    }
    return r;
}
} // namespace beman::inside::detail



//---------------------------------------------------------------------------
// wide_int — a fixed-width two's-complement integer of N limbs, for raws and
// intermediates wider than 64 bits. It behaves like a builtin integer of
// N·limb_bits bits: + − × wrap, / and % truncate toward zero, >> is arithmetic
// when signed, and conversions to and from builtin integers truncate or
// sign-extend as the builtin conversions do. Unlike a builtin, overflow and
// out-of-range shifts are defined (wrap, and 0 or the sign fill).
//
// Structural (a public C array) and trivially copyable, so it can be an NTTP
// member and an inside raw. Every operation is constexpr and allocation-free.
//
// The limb type is a template parameter so tests can run wide_int over uint8_t
// limbs exhaustively against builtin oracles; the library uses umax limbs.
// The limb kernels below are also used by big_rational.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// The builtin word a conversion between wide_int and T goes through: 128 bits
// for a T wider than 64 where the target has them.
#if BEMAN_INSIDE_HAS_INT128
template <class T>
using conversion_word_t = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
template <class T>
using conversion_word_t = std::uint64_t;
#endif

template <std::size_t N, bool Signed, std::unsigned_integral L = umax>
struct wide_int {
    static_assert(N >= 1, "wide_int: at least one limb");
    static constexpr int  limb_bits = limb::bits<L>;
    static constexpr int  bits      = static_cast<int>(N) * limb_bits;
    static constexpr bool is_signed = Signed;

    L Word[N]; // little-endian limbs

    wide_int() = default;

    // From a builtin integer: sign-extends a negative signed value, as the
    // builtin conversions do.
    template <std::integral T>
    constexpr wide_int(T v) noexcept {
        using W             = conversion_word_t<T>;
        using S             = std::make_signed_t<W>;
        constexpr int wbits = std::numeric_limits<W>::digits;
        const W       w     = std::is_signed_v<T> ? static_cast<W>(static_cast<S>(v)) : static_cast<W>(v);
        const L       fill  = (std::is_signed_v<T> && v < 0) ? static_cast<L>(~L{0}) : L{0};
        for (std::size_t i = 0; i < N; ++i)
            Word[i] = (static_cast<int>(i) * limb_bits < wbits)
                          ? static_cast<L>(w >> (static_cast<int>(i) * limb_bits))
                          : fill;
    }

    // Between widths: sign-extends a signed source, truncates a wider one.
    // Implicit only when it widens.
    template <std::size_t M, bool S2>
        requires(M != N || S2 != Signed)
    constexpr explicit(M >= N) wide_int(const wide_int<M, S2, L>& o) noexcept {
        // (One loop, not copy_n + fill: that costs the math rows an instruction.)
        const L fill = o.negative() ? static_cast<L>(~L{0}) : L{0};
        for (std::size_t i = 0; i < N; ++i)
            Word[i] = (i < M) ? o.Word[i] : fill;
    }

    [[nodiscard]] constexpr bool negative() const noexcept { return Signed && (Word[N - 1] >> (limb_bits - 1)) != 0; }

    [[nodiscard]] constexpr bool is_zero() const noexcept {
        for (std::size_t i = 0; i < N; ++i) // (not ranges::all_of: slower to constant-evaluate)
            if (Word[i] != 0)
                return false;
        return true;
    }

    constexpr explicit operator bool() const noexcept { return !is_zero(); }

    // To a builtin integer: keeps the low bits (two's complement), as the
    // builtin narrowing conversions do.
    template <std::integral T>
    constexpr explicit operator T() const noexcept {
        using W             = conversion_word_t<T>;
        constexpr int wbits = std::numeric_limits<W>::digits;
        W             acc   = 0;
        for (std::size_t i = 0; i < N && static_cast<int>(i) * limb_bits < wbits; ++i)
            acc |= static_cast<W>(Word[i]) << (static_cast<int>(i) * limb_bits);
        if constexpr (bits < wbits)
            if (negative())
                acc |= ~W{0} << bits;
        return static_cast<T>(acc);
    }

    // Nearest double (ties to even).
    constexpr explicit operator double() const noexcept {
        if (negative())
            return -static_cast<double>(wide_int<N, false, L>(-*this)); // −min reads as 2^(bits−1)
        const int bw = bit_width_of(*this);
        if (bw <= 64)
            return static_cast<double>(static_cast<std::uint64_t>(*this));
        // Keep the top 64 bits and fold the rest into a sticky bit: 64 > 53 + 2,
        // so the one uint64 → double rounding is the correct one.
        const int     sh  = bw - 64;
        std::uint64_t top = static_cast<std::uint64_t>(lshr(*this, sh));
        if (!(lshr(shl(*this, bits - sh), bits - sh)).is_zero())
            top |= 1u;
        return ldexp(static_cast<double>(top), sh);
    }

    // ---- arithmetic (wraps modulo 2^bits) -------------------------------
    // + − × stay written out: through limb::add_n / sub_n and a truncated
    // addmul_1 product the math tier's rows lose an instruction each (the
    // ins/op bench).
    friend constexpr wide_int operator+(wide_int a, const wide_int& b) noexcept {
        L c = 0;
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] = limb::add_carry(a.Word[i], b.Word[i], c);
        return a;
    }
    friend constexpr wide_int operator-(wide_int a, const wide_int& b) noexcept {
        L br = 0;
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] = limb::sub_borrow(a.Word[i], b.Word[i], br);
        return a;
    }
    friend constexpr wide_int operator-(const wide_int& a) noexcept { return wide_int{0} - a; }
    friend constexpr wide_int operator+(const wide_int& a) noexcept { return a; }

    friend constexpr wide_int operator*(const wide_int& a, const wide_int& b) noexcept {
        wide_int r{0};
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (N > 2) // a small value's high limbs: nothing to add
                if (a.Word[i] == 0)
                    continue;
            L carry = 0;
            for (std::size_t j = 0; i + j < N; ++j) {
                const limb::pair<L> p  = limb::mul(a.Word[i], b.Word[j]);
                L                   c1 = 0, c2 = 0;
                L                   s = limb::add_carry(r.Word[i + j], p.Lo, c1);
                s                     = limb::add_carry(s, carry, c2);
                r.Word[i + j]         = s;
                carry                 = static_cast<L>(p.Hi + c1 + c2); // fits: (b−1)² + 2(b−1) < b²
            }
        }
        return r;
    }

    struct divmod_result;
    // Truncating division, as the builtin operators. Division by zero fails
    // the build in constant evaluation and reports errc::division_by_zero at
    // runtime.
    [[nodiscard]] static constexpr divmod_result divmod(const wide_int& a, const wide_int& b) noexcept;

    friend constexpr wide_int operator/(const wide_int& a, const wide_int& b) noexcept {
        return divmod(a, b).Quotient;
    }
    friend constexpr wide_int operator%(const wide_int& a, const wide_int& b) noexcept {
        return divmod(a, b).Remainder;
    }

    // ---- bitwise ---------------------------------------------------------
    friend constexpr wide_int operator~(wide_int a) noexcept {
        std::ranges::transform(a.Word, a.Word, [](L w) { return static_cast<L>(~w); });
        return a;
    }
    friend constexpr wide_int operator&(wide_int a, const wide_int& b) noexcept {
        std::ranges::transform(a.Word, b.Word, a.Word, [](L x, L y) { return static_cast<L>(x & y); });
        return a;
    }
    friend constexpr wide_int operator|(wide_int a, const wide_int& b) noexcept {
        std::ranges::transform(a.Word, b.Word, a.Word, [](L x, L y) { return static_cast<L>(x | y); });
        return a;
    }
    friend constexpr wide_int operator^(wide_int a, const wide_int& b) noexcept {
        std::ranges::transform(a.Word, b.Word, a.Word, [](L x, L y) { return static_cast<L>(x ^ y); });
        return a;
    }

    // Shifts by s >= bits give 0 (<<) or the sign fill (>>); negative s is a
    // precondition violation, as for builtins.
    friend constexpr wide_int operator<<(const wide_int& a, int s) noexcept { return shl(a, s); }
    friend constexpr wide_int operator>>(const wide_int& a, int s) noexcept {
        const L fill = a.negative() ? static_cast<L>(~L{0}) : L{0};
        return shr(a, s, fill);
    }

    // ---- comparison --------------------------------------------------------
    // Written out: GCC 15/16 miscompare a defaulted == over an array member in
    // constant evaluation (`a == x && b == y` can come out true with b != y).
    friend constexpr bool operator==(const wide_int& a, const wide_int& b) noexcept {
        for (std::size_t i = 0; i < N; ++i) // (not ranges::equal: that calls memcmp)
            if (a.Word[i] != b.Word[i])
                return false;
        return true;
    }
    // (A loop, not lexicographical_compare_three_way on reverse iterators,
    // which moves the ins/op bench both ways.)
    friend constexpr std::strong_ordering operator<=>(const wide_int& a, const wide_int& b) noexcept {
        if (a.negative() != b.negative())
            return a.negative() ? std::strong_ordering::less : std::strong_ordering::greater;
        for (std::size_t i = N; i-- > 0;)
            if (a.Word[i] != b.Word[i])
                return a.Word[i] <=> b.Word[i];
        return std::strong_ordering::equal;
    }

    // ---- compound forms ----------------------------------------------------
    constexpr wide_int& operator+=(const wide_int& b) noexcept { return *this = *this + b; }
    constexpr wide_int& operator-=(const wide_int& b) noexcept { return *this = *this - b; }
    constexpr wide_int& operator*=(const wide_int& b) noexcept { return *this = *this * b; }
    constexpr wide_int& operator/=(const wide_int& b) noexcept { return *this = *this / b; }
    constexpr wide_int& operator%=(const wide_int& b) noexcept { return *this = *this % b; }
    constexpr wide_int& operator&=(const wide_int& b) noexcept { return *this = *this & b; }
    constexpr wide_int& operator|=(const wide_int& b) noexcept { return *this = *this | b; }
    constexpr wide_int& operator^=(const wide_int& b) noexcept { return *this = *this ^ b; }
    constexpr wide_int& operator<<=(int s) noexcept { return *this = *this << s; }
    constexpr wide_int& operator>>=(int s) noexcept { return *this = *this >> s; }
    constexpr wide_int& operator++() noexcept { return *this += wide_int{1}; }
    constexpr wide_int& operator--() noexcept { return *this -= wide_int{1}; }
    constexpr wide_int  operator++(int) noexcept {
        wide_int t = *this;
        ++*this;
        return t;
    }
    constexpr wide_int operator--(int) noexcept {
        wide_int t = *this;
        --*this;
        return t;
    }

    // |a| (a signed minimum stays itself, as the builtin negation wraps).
    [[nodiscard]] friend constexpr wide_int abs(const wide_int& a) noexcept { return a.negative() ? -a : a; }

    // Number of significant bits of a non-negative value (0 for 0).
    [[nodiscard]] friend constexpr int bit_width_of(const wide_int& a) noexcept {
        const std::size_t n = limb::top_size(a.Word, N);
        return n == 0 ? 0 : static_cast<int>(n - 1) * limb_bits + static_cast<int>(std::bit_width(a.Word[n - 1]));
    }

  private:
    static constexpr wide_int shl(const wide_int& a, int s) noexcept {
        wide_int r{0};
        if (s < bits)
            limb::shift_left(r.Word, N, a.Word, N, s);
        return r;
    }
    static constexpr wide_int shr(const wide_int& a, int s, L fill) noexcept {
        wide_int r;
        if (s >= bits)
            for (std::size_t i = 0; i < N; ++i) // (not ranges::fill: that calls memset)
                r.Word[i] = fill;
        else
            limb::shift_right(r.Word, N, a.Word, N, s, fill);
        return r;
    }
    static constexpr wide_int lshr(const wide_int& a, int s) noexcept { return shr(a, s, L{0}); }
};

template <std::size_t N, bool Signed, std::unsigned_integral L>
struct wide_int<N, Signed, L>::divmod_result {
    wide_int Quotient;
    wide_int Remainder;
};

template <std::size_t N, bool Signed, std::unsigned_integral L>
constexpr auto wide_int<N, Signed, L>::divmod(const wide_int& a, const wide_int& b) noexcept -> divmod_result {
    if (b.is_zero())
        fail_hard<"wide_int: division by zero">(errc::division_by_zero);
    // Divide the magnitudes; −min reads correctly as an unsigned magnitude.
    const bool        na = a.negative(), nb = b.negative();
    const wide_int    ua = na ? -a : a, ub = nb ? -b : b;
    const std::size_t m = limb::top_size(ua.Word, N);
    std::size_t       n = N; // b ≠ 0: no n > 0 test, which GCC's bounds warnings would need
    while (ub.Word[n - 1] == 0)
        --n;
    divmod_result res{wide_int{0}, wide_int{0}};
    if (m < n)
        res.Remainder = ua;
    else {
        L un[N + 1]{}, vn[N]{};
        limb::divmod(ua.Word, m, ub.Word, n, res.Quotient.Word, res.Remainder.Word, un, vn);
    }
    if (na != nb)
        res.Quotient = -res.Quotient;
    if (na)
        res.Remainder = -res.Remainder;
    return res;
}

template <std::size_t N, std::unsigned_integral L = umax>
using wide_uint = wide_int<N, false, L>;
template <std::size_t N, std::unsigned_integral L = umax>
using wide_sint = wide_int<N, true, L>;

// n / d for a one-limb d: the quotient and the remainder.
template <std::size_t K>
struct small_divmod {
    wide_uint<K> Quotient;
    umax         Remainder;
};

template <std::size_t K>
constexpr small_divmod<K> divmod_small(const wide_uint<K>& n, umax d) noexcept {
    small_divmod<K> r{wide_uint<K>{0}, 0};
    for (std::size_t i = K; i-- > 0;) { // (not limb::div_1: slower to constant-evaluate)
        const limb::pair<umax> qr = limb::div(r.Remainder, n.Word[i], d);
        r.Quotient.Word[i]        = qr.Hi;
        r.Remainder               = qr.Lo;
    }
    return r;
}

template <typename T>
inline constexpr bool is_wide_int_v = false;
template <std::size_t N, bool S, std::unsigned_integral L>
inline constexpr bool is_wide_int_v<wide_int<N, S, L>> = true;

// A wide_int of any width and signedness.
template <typename T>
concept wide_integer = is_wide_int_v<std::remove_cvref_t<T>>;

// An integer the library may hold in a raw or intermediate: a builtin
// integer or a wide_int. (std::integral cannot be extended to wide_int.)
template <typename T>
concept raw_integer = std::integral<T> || wide_integer<T>;

// gcd of two non-negative wide integers (Euclid; 0 when both are).
template <wide_integer I>
constexpr I wide_gcd(I a, I b) noexcept {
    return euclid_gcd(a, b);
}

//---------------------------------------------------------------------------
// wide_rational — an exact value as a fraction of K-limb integers: the exact
// paths' intermediate (wide_value.hpp), and, reduced, the raw of a continuous
// grid whose limits pass 64 bits. Structural, so it can be a raw.
//---------------------------------------------------------------------------
inline constexpr std::size_t exact_min_limbs = 8; // scalars, 64-bit rationals

template <std::size_t K>
struct wide_rational {
    wide_sint<K> Num{0};
    wide_sint<K> Den{1}; // > 0; not reduced

    constexpr wide_rational() = default;
    constexpr wide_rational(wide_sint<K> n, wide_sint<K> d) noexcept : Num{n}, Den{d} {}
    template <std::size_t M>
        requires(M < K)
    constexpr wide_rational(const wide_rational<M>& o) noexcept : Num{o.Num}, Den{o.Den} {} // widens

    constexpr explicit operator double() const noexcept { return static_cast<double>(Num) / static_cast<double>(Den); }
};

template <typename T>
inline constexpr bool is_wide_rational_v = false;
template <std::size_t K>
inline constexpr bool is_wide_rational_v<wide_rational<K>> = true;

// A wide_rational of any width.
template <typename T>
concept wide_rational_like = is_wide_rational_v<std::remove_cvref_t<T>>;
} // namespace beman::inside::detail

template <std::size_t N, bool Signed, std::unsigned_integral L>
struct std::numeric_limits<beman::inside::detail::wide_int<N, Signed, L>> {
  private:
    using W = beman::inside::detail::wide_int<N, Signed, L>;
    static constexpr W top_bit() noexcept { return W{1} << (W::bits - 1); }

  public:
    static constexpr bool is_specialized    = true;
    static constexpr bool is_signed         = Signed;
    static constexpr bool is_integer        = true;
    static constexpr bool is_exact          = true;
    static constexpr bool has_infinity      = false;
    static constexpr bool has_quiet_NaN     = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool is_bounded        = true;
    static constexpr bool is_modulo         = !Signed;
    static constexpr int  radix             = 2;
    static constexpr int  digits            = W::bits - (Signed ? 1 : 0);
    static constexpr int  digits10          = digits * 30103 / 100000; // ⌊digits·log10 2⌋
    static constexpr W    min() noexcept { return Signed ? top_bit() : W{0}; }
    static constexpr W    lowest() noexcept { return min(); }
    static constexpr W    max() noexcept { return Signed ? ~top_bit() : ~W{0}; }
};


// ======================================================================
//  beman/inside/detail/rounding.hpp
// ======================================================================


//---------------------------------------------------------------------------
// The rounding decision every integer rounding path shares. A path
// truncates toward zero, classifies the dropped remainder, and asks
// rounds_away whether the mode moves the quotient one unit away from zero.
// The arithmetic (builtin, wide, rational, double) stays with the caller;
// only the decision lives here. With a constant mode it folds to the one
// comparison that mode needs.
//---------------------------------------------------------------------------
namespace beman::inside::detail {

// Ties of `nearest` go half away from zero. rounding_of (policy_flag.hpp)
// maps a flag set to its mode.
enum class round_mode { trunc, nearest, floor, ceil, half_even };

// Where a truncation's dropped remainder sits against half a unit. Only the
// nearest modes read the half; for the others every nonzero remainder
// classifies as below_half (they need only "inexact").
enum class remainder_class : unsigned char { zero, below_half, half, above_half };

// The remainder magnitude r of a divisor magnitude d (0 ≤ r < d), classified
// for m. Compares r with d − r, so 2·r never has to fit T.
template <typename T>
[[nodiscard]] constexpr remainder_class classify_remainder(round_mode m, const T& r, const T& d) noexcept {
    if (r == T{0})
        return remainder_class::zero;
    if (m != round_mode::nearest && m != round_mode::half_even)
        return remainder_class::below_half;
    const T rest = d - r;
    return r < rest ? remainder_class::below_half : rest < r ? remainder_class::above_half : remainder_class::half;
}

// Whether rounding by m moves a truncated quotient one unit away from zero.
// `negative`: the exact value's sign; `odd`: the truncated quotient's parity.
[[nodiscard]] constexpr bool rounds_away(round_mode m, bool negative, remainder_class r, bool odd) noexcept {
    if (r == remainder_class::zero)
        return false;
    switch (m) {
    case round_mode::floor:
        return negative;
    case round_mode::ceil:
        return !negative;
    case round_mode::nearest:
        return r != remainder_class::below_half;
    case round_mode::half_even:
        return r == remainder_class::above_half || (r == remainder_class::half && odd);
    default:
        return false;
    }
}
// The same decision for a quotient rounded down (toward −∞) instead of
// toward zero: whether rounding by m moves it one unit up. `negative`: the
// rounded VALUE is below zero, which on an unanchored grid need not be the
// quotient's sign (toward zero is down for a value ≥ 0, up below 0, and a
// tie of `nearest` goes away from zero — up at 0 itself); `odd`: the floor's
// parity as a lattice index.
[[nodiscard]] constexpr bool rounds_up(round_mode m, bool negative, remainder_class r, bool odd) noexcept {
    if (r == remainder_class::zero)
        return false;
    switch (m) {
    case round_mode::floor:
        return false;
    case round_mode::ceil:
        return true;
    case round_mode::nearest:
        return r == remainder_class::above_half || (r == remainder_class::half && !negative);
    case round_mode::half_even:
        return r == remainder_class::above_half || (r == remainder_class::half && odd);
    default: // toward zero
        return negative;
    }
}
} // namespace beman::inside::detail




namespace beman::inside::detail {
// |d| of a signed denominator (any imax, INT64_MIN too).
[[nodiscard]] constexpr umax abs_den(imax d) noexcept { return safe_abs(d); }

//---------------------------------------------------------------------------
// trim
//---------------------------------------------------------------------------
inline constexpr void trim(umax& numerator, imax& denominator) {
    umax ad = abs_den(denominator);
    if (ad <= 1)
        return;
    if (std::has_single_bit(ad)) {
        // A power-of-two denominator: gcd(n, 2^k) = 2^ctz(n | 2^k), no loop.
        const int s = std::countr_zero(numerator | ad);
        numerator >>= s;
        ad >>= s;
        denominator = (denominator < 0) ? -static_cast<imax>(ad) : static_cast<imax>(ad);
        return;
    }
    auto g = std::gcd(numerator, ad);
    if (g <= 1)
        return;
    numerator /= g;
    ad /= g;
    denominator = static_cast<imax>((denominator < 0) ? umax{0} - ad : ad);
}

inline constexpr void trim(umax& a, umax& b) {
    if (a <= 1 || b <= 1)
        return;
    auto g = std::gcd(a, b);
    if (g <= 1)
        return;
    a /= g;
    b /= g;
}

[[nodiscard]] constexpr std::expected<rational, errc> operator+(const rational&, const rational&);
[[nodiscard]] constexpr std::expected<rational, errc> operator/(const rational&, const rational&);
[[nodiscard]] constexpr std::expected<rational, errc> operator-(const rational&, const rational&);

[[nodiscard]] constexpr std::expected<rational, errc> operator*(const rational&, const rational&);
[[nodiscard]] constexpr auto                          operator<=>(rational, rational) -> std::strong_ordering;

//---------------------------------------------------------------------------
// rational — structural type for NTTP (public members only). Sign is encoded
// in the denominator (negative = negative rational); numerator is unsigned to
// represent e.g. umax itself.
//---------------------------------------------------------------------------
struct rational {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    // Exact for every double of magnitude 2^-10 or more; one finer than
    // 2^-62 keeps only its bits down to 2^-62 (truncated toward zero), as the
    // denominator has 63 bits. |value| ≥ 2^64 and non-finite values raise.
    constexpr rational(std::floating_point auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1) : Numerator{num}, Denominator{den} {
        canonicalize(Numerator, Denominator);
    }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. A den past imax has no signed denominator: overflow.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den) : Numerator{num}, Denominator{static_cast<imax>(den)} {
        if (!std::in_range<imax>(den))
            raise(errc::overflow, "rational: denominator past imax");
        canonicalize(Numerator, Denominator);
    }

    // Implicit unwrap of a checked result, so coefficient expressions read as
    // plain arithmetic (`rational two_pi = 2 * pi;`); an error (overflow) is a
    // compile error in constant evaluation, a throw at runtime. same_as-constrained
    // (not a plain `rational(expected<rational, errc>)`) because expected's own
    // converting ctor is gated on `is_constructible_v<rational, U>` — a
    // non-template overload would make that trait depend on itself.
    template <class O>
        requires std::same_as<std::remove_cvref_t<O>, std::expected<rational, errc>>
    constexpr rational(O&& o) : rational(o.value()) {}

    // operator== by default for structural type
    [[nodiscard]] constexpr bool operator==(const rational&) const = default;
    // Exact against a double too (rational{value} truncates one finer than
    // 2^-62): defined after compare_double.
    template <plain_number T>
    [[nodiscard]] constexpr bool operator==(T value) const;

    [[nodiscard]] constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr std::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T() const {
        if (Denominator < 0)
            raise(errc::domain_error, "cannot convert negative rational to unsigned");
        return static_cast<T>(Numerator / abs_den(Denominator));
    }

    template <std::signed_integral T>
    explicit constexpr operator T() const {
        const umax q = Numerator / abs_den(Denominator);
        return static_cast<T>((Denominator < 0) ? umax{0} - q : q);
    }

    template <std::floating_point T>
    explicit constexpr operator T() const {
        T q = static_cast<T>(Numerator) / static_cast<T>(abs_den(Denominator));
        return (Denominator < 0) ? -q : q;
    }

    // allow unary+ for generic programming
    [[nodiscard]] constexpr rational operator+() const { return *this; }

    // Compound-assign: forward to the checked binary op and unwrap via .value()
    // — overflow surfaces as std::bad_expected_access (no error channel here).
    constexpr rational& operator+=(const rational& rhs);
    constexpr rational& operator-=(const rational& rhs);
    constexpr rational& operator*=(const rational& rhs);
    constexpr rational& operator/=(const rational& rhs);

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero operand for div_unchecked / inv_unchecked).
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);
    static constexpr rational inv_unchecked(rational);

    static constexpr std::expected<rational, errc> add(rational a, rational b) { return a + b; }
    static constexpr std::expected<rational, errc> inv(rational);

    // Shared algorithm bodies. Checked=true returns expected<rational, errc>,
    // reporting overflow / division_by_zero (a compile error at compile time,
    // std::unexpected at runtime); Checked=false
    // silently overflows — the caller must guarantee its absence.
    template <bool Checked, bool Loud = true>
    static constexpr auto add_impl(const rational&, const rational&);
    template <bool Checked, bool Loud = true>
    static constexpr auto mul_impl(const rational&, const rational&);
    template <bool Checked, bool Loud = true>
    static constexpr auto div_impl(const rational&, const rational&);
    template <bool Checked, bool Loud = true>
    static constexpr auto inv_impl(const rational&);

  private:
    // The two domain errors of a denominator: 0 (undefined) and imax_min
    // (cannot be negated without UB, which every sign-flip in the file
    // assumes is well-defined).
    static constexpr void check_den(imax den) {
        if (den == 0)
            raise(errc::domain_error, "Denominator of Zero is invalid");
        if (den == std::numeric_limits<imax>::min())
            raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
    }

    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    static constexpr void canonicalize(umax& num, imax& den) {
        check_den(den);
        if (num == 0)
            den = 1;
        trim(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB.
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den) {
        check_den(den);
        return (num < 0) ? -den : den;
    }
};

[[nodiscard]] constexpr std::expected<rational, errc> gcd(const rational&, const rational&);
[[nodiscard]] constexpr rational                      abs(rational);

[[nodiscard]] constexpr bool divides_evenly(const rational&, const rational&);

//---------------------------------------------------------------------------
// sign / named integer reductions — free functions over the public
// numerator/denominator (structural type), siblings of abs/gcd. Reductions
// are explicit, lossy alternatives to `static_cast`:
// trunc → 0; floor → -inf; ceil → +inf; round → half-away-from-zero.
//---------------------------------------------------------------------------
// -1 / 0 / +1 — single source of truth for the sign convention (sign lives in
// Denominator; canonical zero is {0, 1}).
[[nodiscard]] constexpr int sign(rational v) noexcept {
    if (v.Numerator == 0)
        return 0;
    return (v.Denominator < 0) ? -1 : 1;
}

// A rational from already-canonical parts (magnitude, signed denominator):
// no gcd, no domain checks.
[[nodiscard]] constexpr rational make_raw(umax num, imax den) noexcept {
    rational r;
    r.Numerator   = num;
    r.Denominator = den;
    return r;
}

// A checked op failed: during constant evaluation that is a compile error
// naming the cause; at runtime it is an error value.
// Loud == false is the quiet form behind try_add & co.: an error value even
// during constant evaluation, for code that must test whether an op fits.
template <fixed_string Msg, bool Loud = true>
constexpr std::unexpected<errc> fail(errc code) {
    if constexpr (Loud)
        if consteval {
            constexpr_error<Msg>();
        }
    return std::unexpected{code};
}

// The numerator with the value's sign, as imax (callers ensure it fits).
// Negated in umax, so −2^63 (numerator 2^63) comes out as INT64_MIN.
[[nodiscard]] constexpr imax signed_numerator(rational v) noexcept {
    return static_cast<imax>((v.Denominator < 0) ? umax{0} - v.Numerator : v.Numerator);
}

// v rounded to an integer by m (nearest: half away from zero), as a sign
// and a umax magnitude: q + 1 cannot overflow, since a nonzero remainder
// needs a denominator ≥ 2, so q ≤ umax/2.
struct rounded_integer {
    umax Magnitude;
    bool Negative;
};
[[nodiscard]] constexpr rounded_integer round_magnitude(rational v, round_mode m) {
    const umax ad  = abs_den(v.Denominator);
    const umax q   = v.Numerator / ad;
    const bool neg = v.Denominator < 0;
    return {q + rounds_away(m, neg, classify_remainder(m, v.Numerator % ad, ad), (q & 1) != 0), neg};
}

// Narrowed to imax (callers keep |v| within it).
[[nodiscard]] constexpr imax round_to_int(rational v, round_mode m) {
    const auto [mag, neg] = round_magnitude(v, m);
    return static_cast<imax>(neg ? umax{0} - mag : mag);
}

// Kept exact as a rational, for indices that may pass imax.
[[nodiscard]] constexpr rational round_to_integral(rational v, round_mode m) {
    const auto [mag, neg] = round_magnitude(v, m);
    return make_raw(mag, (neg && mag != 0) ? imax{-1} : imax{1});
}

[[nodiscard]] constexpr imax trunc(rational v) { return round_to_int(v, round_mode::trunc); }
[[nodiscard]] constexpr imax floor(rational v) { return round_to_int(v, round_mode::floor); }
[[nodiscard]] constexpr imax ceil(rational v) { return round_to_int(v, round_mode::ceil); }
[[nodiscard]] constexpr imax round(rational v) { return round_to_int(v, round_mode::nearest); }

//---------------------------------------------------------------------------
// abs
//---------------------------------------------------------------------------
[[nodiscard]] constexpr rational abs(rational v) {
    if (v.Denominator < 0)
        v.Denominator = -v.Denominator;
    return v;
}

//---------------------------------------------------------------------------
// gcd
//---------------------------------------------------------------------------
// Returns errc::overflow if the combined denominator lcm = (a/gcd)·b would exceed
// imax_max (sign-bit reservation) — traps the mul_overflow then range-checks.
//---------------------------------------------------------------------------
[[nodiscard]] constexpr std::expected<rational, errc> gcd(const rational& lhs, const rational& rhs) {
    umax a = abs_den(lhs.Denominator);
    umax b = abs_den(rhs.Denominator);
    umax g = std::gcd(a, b);

    umax denominator;
    if (mul_overflow(a / g, b, &denominator))
        return std::unexpected{errc::overflow};
    if (!std::in_range<imax>(denominator))
        return std::unexpected{errc::overflow};

    auto numerator = std::gcd(lhs.Numerator, rhs.Numerator);
    return rational{numerator, denominator};
}

//---------------------------------------------------------------------------
// Floating points wider than double (long double), exactly: the sum of the
// doubles they split into, each the rounding error of the last.
//---------------------------------------------------------------------------

// |v|·2^wide_float_shift for a finite 2^-64 ≤ |v| < 2^64, truncated to an
// integer (< 2^244), and the sign of what the truncation dropped (0: exact,
// as for every long double of at most 116 significant bits).
inline constexpr int wide_float_shift = 180;
struct scaled_float {
    wide_uint<4> Magnitude;
    int          Tail;
};
template <std::floating_point T>
[[nodiscard]] constexpr scaled_float scaled_magnitude(T v) noexcept {
    using W = wide_sint<5>;
    W       acc{0};
    int     tail = 0;
    const T rem  = for_each_double_part(v < 0 ? -v : v, [&](double d) {
        const auto [mant, e] = double_parts(d);
        const int s          = e + wide_float_shift;
        if (s < 0) { // below 2^-180: the parts after it are smaller still
            tail = d < 0 ? -1 : 1;
            return false;
        }
        acc += d < 0 ? -(W{mant} << s) : W{mant} << s;
        return true;
    });
    if (tail == 0 && rem != 0)
        tail = rem < 0 ? -1 : 1;
    return {static_cast<wide_uint<4>>(acc), tail};
}

// rational{v} for a finite v wider than a double: exact when it fits, else
// (bits finer than 2^-62) truncated toward zero, as for a double; |v| ≥ 2^64
// raises.
template <std::floating_point T>
constexpr rational rational_of_wide_float(T v, bool& exact) {
    const T a = v < 0 ? -v : v;
    exact     = true;
    if (!(a < static_cast<T>(0x1p64)))
        raise(errc::overflow, "beman::inside::detail::rational: |long double| >= 2^64");
    if (a < static_cast<T>(0x1p-62)) {
        exact = a == 0;
        return make_raw(0, 1);
    }
    const auto [m, tail] = scaled_magnitude(a);
    constexpr int drop   = wide_float_shift - 62; // keep bits down to 2^-62
    exact                = tail == 0 && (m << (wide_uint<4>::bits - drop)).is_zero();
    const auto q         = m >> drop; // ≥ 1 here
    const int  tz        = std::countr_zero(static_cast<umax>(q)) < 62 ? std::countr_zero(static_cast<umax>(q)) : 62;
    const auto num       = q >> tz;
    if (bit_width_of(num) > 64)
        raise(errc::overflow, "beman::inside::detail::rational: long double past the 64-bit rational");
    const imax den = imax{1} << (62 - tz);
    return make_raw(static_cast<umax>(num), v < 0 ? -den : den);
}

//---------------------------------------------------------------------------
// rational::rational
//---------------------------------------------------------------------------
constexpr rational::rational(std::signed_integral auto num, imax den)
    : Numerator{safe_abs(num)}, Denominator{signed_den_from(num, den)} {
    canonicalize(Numerator, Denominator);
}

// rational{value}, and whether it is exact: a finite |value| < 2^64 finer
// than 2^-62 loses its low bits (truncated toward zero). abs_fraction's
// fraction is already reduced.
constexpr rational rational_of_double(double value, bool& exact) {
    exact = true;
    if (value == 0.0)
        return make_raw(0, 1);
    const auto [num, den] = abs_fraction(value, exact);
    return make_raw(num, value < 0 ? -static_cast<imax>(den) : static_cast<imax>(den));
}

constexpr rational::rational(std::floating_point auto value) {
    if (!is_finite(value))
        raise(errc::not_finite, "non-finite double");
    bool exact;
    if constexpr (wide_float<decltype(value)>)
        if (!is_double_value(value)) {
            *this = rational_of_wide_float(value, exact);
            return;
        }
    *this = rational_of_double(static_cast<double>(value), exact);
}

//---------------------------------------------------------------------------
// to
//---------------------------------------------------------------------------
template <std::unsigned_integral T>
constexpr std::expected<T, errc> rational::to() const {
    if (Denominator < 0)
        return std::unexpected{errc::domain_error};
    const umax q = Numerator / abs_den(Denominator);
    if (!std::in_range<T>(q))
        return std::unexpected{errc::overflow};
    return static_cast<T>(q);
}

// a + b exactly, for the checked add once a 64-bit step overflowed: the
// numerator and lcm in 3 limbs, reduced, then checked to fit (a sum can
// reduce back into range: (2^63+1)/2 + 1/6 = (3·2^62+2)/3).
template <bool Loud>
[[gnu::cold]] [[gnu::noinline]] constexpr std::expected<rational, errc> add_wide(rational a, rational b) {
    using W       = wide_sint<3>;
    const umax ad = abs_den(a.Denominator), bd = abs_den(b.Denominator), g = std::gcd(ad, bd);
    const W    an  = a.Denominator < 0 ? -W{a.Numerator} : W{a.Numerator};
    const W    bn  = b.Denominator < 0 ? -W{b.Numerator} : W{b.Numerator};
    W          num = an * W{bd / g} + bn * W{ad / g};
    W          den = W{ad} * W{bd / g};
    if (num.is_zero())
        return rational{0};
    const W x      = wide_gcd(abs(num), den);
    num            = num / x;
    den            = den / x;
    const bool neg = num.negative();
    const W    mag = neg ? -num : num;
    if (W{std::numeric_limits<umax>::max()} < mag || W{std::numeric_limits<imax>::max()} < den)
        return fail<"rational +: result out of the 64-bit range", Loud>(errc::overflow);
    const imax d = static_cast<imax>(static_cast<umax>(den));
    return make_raw(static_cast<umax>(mag), neg ? -d : d);
}

// Reduce n/d, the sum of two reduced fractions over d = lcm of their
// denominators, whose gcd is g: gcd(n, d) = gcd(n, g) (Henrici), so a sum
// over coprime denominators is already reduced.
constexpr void reduce_sum(umax& n, umax& d, umax g) noexcept {
    if (g == 1)
        return;
    // A power-of-two g: the gcd is the lowest set bit of n | g.
    const umax h = std::has_single_bit(g) ? (n | g) & (umax{0} - (n | g)) : std::gcd(n, g);
    n /= h;
    d /= h;
}

//---------------------------------------------------------------------------
// add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
//---------------------------------------------------------------------------
template <bool Checked, bool Loud>
inline constexpr auto rational::add_impl(const rational& a, const rational& b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    // (a == −b needs no test: equal denominators, opposite signs → num == 0 below.)
    if (a.Numerator == 0)
        return ret_t{b};
    if (b.Numerator == 0)
        return ret_t{a};

    bool a_neg = a.Denominator < 0;
    bool b_neg = b.Denominator < 0;
    umax a_ad  = abs_den(a.Denominator);
    umax b_ad  = abs_den(b.Denominator);

    if (a_ad == b_ad) {
        if (a_neg == b_neg) {
            umax numerator;
            if constexpr (Checked) {
                if (add_overflow(a.Numerator, b.Numerator, &numerator)) {
                    return ret_t{fail<"rational +: numerator overflow (same denominator)", Loud>(errc::overflow)};
                }
            } else
                numerator = a.Numerator + b.Numerator;

            rational r;
            r.Numerator   = numerator;
            r.Denominator = a.Denominator;
            trim(r.Numerator, r.Denominator);
            return ret_t{r};
        }

        umax num   = (a.Numerator > b.Numerator) ? (a.Numerator - b.Numerator) : (b.Numerator - a.Numerator);
        bool r_neg = a_neg ? (a.Numerator > b.Numerator) : (b.Numerator > a.Numerator);
        if (num == 0)
            return ret_t{rational{0}};
        rational r;
        r.Numerator   = num;
        r.Denominator = static_cast<imax>(r_neg ? umax{0} - a_ad : a_ad);
        trim(r.Numerator, r.Denominator);
        return ret_t{r};
    }

    // Common denominator = lcm(a_ad, b_ad) = a_ad·(b_ad/g), not a_ad·b_ad: the
    // reduced cofactors (g = gcd) overflow far less often than the raw product.
    umax g      = std::gcd(a_ad, b_ad);
    umax a_ad_r = a_ad / g; // = a_ad / gcd; coprime with b_ad_r
    umax b_ad_r = b_ad / g;

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked) {
        if (mul_overflow(a_ad, b_ad_r, &denominator) || // = lcm(a_ad, b_ad)
            !std::in_range<imax>(denominator)) {
            return ret_t{fail<"rational +: denominator overflow", Loud>(errc::overflow)};
        }
        if (mul_overflow(a.Numerator, b_ad_r, &A) || mul_overflow(b.Numerator, a_ad_r, &B)) {
            // Mixed signs: |A − B| can fit umax even when a cross-product alone
            // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
            // subtraction brings it back in range). Retry the difference in 128-bit before giving up.
            if (a_neg != b_neg) {
                const limb::pair<umax> A2       = limb::mul(a.Numerator, b_ad_r);
                const limb::pair<umax> B2       = limb::mul(b.Numerator, a_ad_r);
                const bool             a_bigger = A2.Hi != B2.Hi ? A2.Hi > B2.Hi : A2.Lo > B2.Lo;
                const limb::pair<umax> big = a_bigger ? A2 : B2, small = a_bigger ? B2 : A2;
                if (big.Hi - small.Hi - (big.Lo < small.Lo ? 1u : 0u) == 0) {
                    rational r;
                    r.Numerator = big.Lo - small.Lo;
                    r.Denominator =
                        static_cast<imax>((a_neg ? a_bigger : !a_bigger) ? umax{0} - denominator : denominator);
                    trim(r.Numerator, r.Denominator);
                    return ret_t{r};
                }
            }
            return ret_t{fail<"rational +: cross-multiplication overflow", Loud>(errc::overflow)};
        }
    } else {
        denominator = a_ad * b_ad_r;
        A           = a.Numerator * b_ad_r;
        B           = b.Numerator * a_ad_r;
    }

    if (a_neg == b_neg) {
        umax numerator;
        if constexpr (Checked) {
            if (add_overflow(A, B, &numerator)) {
                return ret_t{fail<"rational +: numerator sum overflow", Loud>(errc::overflow)};
            }
        } else
            numerator = A + B;

        // num, den both > 0 here, so the ctor's domain/zero checks are dead;
        // assemble directly and reduce.
        reduce_sum(numerator, denominator, g);
        return ret_t{make_raw(numerator, a_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator))};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which for canonical inputs has equal denominators and
    // took the equal-denominator branch above.
    umax       numerator = (A > B) ? (A - B) : (B - A);
    const bool r_neg     = a_neg ? (A > B) : (B > A);
    reduce_sum(numerator, denominator, g);
    return ret_t{make_raw(numerator, r_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator))};
}

template <bool Checked, bool Loud>
inline constexpr auto rational::mul_impl(const rational& a_in, const rational& b_in) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0)
        return ret_t{rational{0}};

    bool r_neg = (a.Denominator < 0) != (b.Denominator < 0);
    umax a_ad  = abs_den(a.Denominator);
    umax b_ad  = abs_den(b.Denominator);

    if (a_ad == 1 && b_ad == 1) {
        umax numerator;
        if constexpr (Checked) {
            if (mul_overflow(a.Numerator, b.Numerator, &numerator)) {
                return ret_t{fail<"rational *: numerator overflow", Loud>(errc::overflow)};
            }
        } else
            numerator = a.Numerator * b.Numerator;

        return ret_t{make_raw(numerator, r_neg ? imax{-1} : imax{1})};
    }

    trim(a.Numerator, b_ad);
    trim(b.Numerator, a_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked) {
        if (mul_overflow(a.Numerator, b.Numerator, &numerator) || mul_overflow(a_ad, b_ad, &denominator) ||
            !std::in_range<imax>(denominator)) {
            return ret_t{fail<"rational *: numerator or denominator overflow", Loud>(errc::overflow)};
        }
    } else {
        numerator   = a.Numerator * b.Numerator;
        denominator = a_ad * b_ad;
    }

    // The cross-trims above guarantee gcd(numerator, denominator) == 1, so
    // bypass rational(num, den) and skip its redundant trim.
    return ret_t{make_raw(numerator, static_cast<imax>(r_neg ? umax{0} - denominator : denominator))};
}

//---------------------------------------------------------------------------
// inv_impl — multiplicative inverse (1 / a)
//---------------------------------------------------------------------------
// a is already trimmed (Numerator and |Denominator| coprime), so the swapped
// pair is also trimmed. Sign lives in the denominator and 1/(-x) has the same
// sign as -x, so the sign bit moves with the (now) denominator unchanged.
template <bool Checked, bool Loud>
inline constexpr auto rational::inv_impl(const rational& a) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked) {
        // a.Numerator goes into the result's Denominator slot, so it must fit in
        // imax (else the umax→imax conversion wraps and a later -Denominator is UB).
        if (a.Numerator == 0) {
            return ret_t{fail<"rational inv: division by zero", Loud>(errc::division_by_zero)};
        }
        if (!std::in_range<imax>(a.Numerator)) {
            return ret_t{fail<"rational inv: numerator out of denominator range", Loud>(errc::overflow)};
        }
    }

    return ret_t{make_raw(abs_den(a.Denominator), signed_numerator(a))};
}

// div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
// the b.Numerator-fits-in-imax check (added there) propagates here too;
// the unchecked path skips it (caller's contract).
template <bool Checked, bool Loud>
inline constexpr auto rational::div_impl(const rational& a, const rational& b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked) {
        // Cross-reduce a.N/b.N and b.D/a.D first: b.N need not fit the
        // denominator alone (x/x with x > 2^63 is 1).
        if (b.Numerator == 0)
            return ret_t{fail<"rational /: division by zero", Loud>(errc::division_by_zero)};
        if (a.Numerator == 0)
            return ret_t{rational{0}};
        const umax g1 = std::gcd(a.Numerator, b.Numerator);
        const umax g2 = std::gcd(abs_den(a.Denominator), abs_den(b.Denominator));
        umax       num, den;
        if (mul_overflow(a.Numerator / g1, abs_den(b.Denominator) / g2, &num) ||
            mul_overflow(abs_den(a.Denominator) / g2, b.Numerator / g1, &den) || !std::in_range<imax>(den))
            return ret_t{fail<"rational /: numerator or denominator overflow", Loud>(errc::overflow)};
        const bool neg = (a.Denominator < 0) != (b.Denominator < 0);
        return ret_t{make_raw(num, neg ? -static_cast<imax>(den) : static_cast<imax>(den))};
    } else
        return mul_impl<false>(a, inv_impl<false>(b));
}

//---------------------------------------------------------------------------
// unchecked rational arithmetic — caller guarantees: no umax overflow on the
// products, no zero divisor/numerator, and the result Denominator fits in imax.
// The checked variants enforce all three; unchecked skips them.
//---------------------------------------------------------------------------
inline constexpr rational rational::add_unchecked(rational a, rational b) { return add_impl<false>(a, b); }

inline constexpr rational rational::mul_unchecked(rational a, rational b) { return mul_impl<false>(a, b); }

inline constexpr rational rational::div_unchecked(rational a, rational b) { return div_impl<false>(a, b); }

inline constexpr rational rational::inv_unchecked(rational a) { return inv_impl<false>(a); }

inline constexpr std::expected<rational, errc> rational::inv(rational a) { return inv_impl<true>(a); }

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr rational rational::operator-() const {
    if (Numerator == 0)
        return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    return make_raw(Numerator, -Denominator);
}

//---------------------------------------------------------------------------
// operator<=>
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering {
    int lhs_sign = sign(lhs);
    int rhs_sign = sign(rhs);

    if (lhs_sign != rhs_sign)
        return lhs_sign <=> rhs_sign;

    if (lhs_sign == 0)
        return std::strong_ordering::equal;

    // signs are equal here (the `lhs_sign != rhs_sign` branch returned above)
    bool lhs_neg = lhs_sign < 0;

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    // integer comparison: skip cross-multiply entirely
    if (lhs_ad == 1 && rhs_ad == 1) {
        if (lhs_neg)
            return rhs.Numerator <=> lhs.Numerator;
        else
            return lhs.Numerator <=> rhs.Numerator;
    }

    // One side is an integer: compare via divmod instead of cross-multiply.
    // Cross-multiplying would otherwise overflow when the non-integer side
    // has a huge denominator (e.g. doubles like 19.99 stored as N/2^48).
    if (rhs_ad == 1) {
        umax q   = lhs.Numerator / lhs_ad;
        umax r   = lhs.Numerator % lhs_ad;
        auto cmp = (q == rhs.Numerator) ? (r == 0 ? std::strong_ordering::equal : std::strong_ordering::greater)
                                        : (q <=> rhs.Numerator);
        return lhs_neg ? (0 <=> cmp) : cmp;
    }
    if (lhs_ad == 1) {
        umax q   = rhs.Numerator / rhs_ad;
        umax r   = rhs.Numerator % rhs_ad;
        auto cmp = (q == lhs.Numerator) ? (r == 0 ? std::strong_ordering::equal : std::strong_ordering::less)
                                        : (lhs.Numerator <=> q);
        return lhs_neg ? (0 <=> cmp) : cmp;
    }

    // Cross-multiply in 128-bit: |numerator| and |denominator| are each ≤ 2^64−1,
    // so the products fit exactly in 128 bits — the comparison can never overflow,
    // so no trap is needed.
    return lhs_neg ? limb::mul_compare(rhs.Numerator, lhs_ad, lhs.Numerator, rhs_ad)
                   : limb::mul_compare(lhs.Numerator, rhs_ad, rhs.Numerator, lhs_ad);
}

template <plain_number T>
[[nodiscard]] inline constexpr auto operator<=>(const std::expected<T, errc>& lhs, const rational& rhs) {
    return rational{lhs.value()} <=> rhs;
}

template <plain_number T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, const std::expected<T, errc>& rhs) {
    return lhs <=> rational{rhs.value()};
}

// lhs against a finite double finer than 2^-62 (which rational{rhs}
// truncates): |lhs| = N/D against |rhs| = s·2^-k (k > 62) as N·2^k against
// s·D, which stays within two 64-bit factors. Cold and out of line: the
// stores compare doubles with their limits.
[[nodiscard]] [[gnu::cold]] [[gnu::noinline]] constexpr std::strong_ordering compare_fine_double(const rational& lhs,
                                                                                                 double          rhs) {
    const int ls = sign(lhs), rs = rhs < 0 ? -1 : 1;
    if (ls != rs)
        return ls <=> rs;
    const auto [s, exp2] = abs_fields(rhs);
    const int  k         = -exp2; // rhs = s·2^-k, k > 62
    const umax n = lhs.Numerator, d = abs_den(lhs.Denominator);
    // s·D < 2^117: past that width N·2^k is larger.
    const auto mag = std::bit_width(n) + k > 117
                         ? std::strong_ordering::greater
                         : limb::mul_compare(n << (k - 63), umax{1} << 63, s, d); // N·2^(k−63) < 2^54
    return ls < 0 ? (0 <=> mag) : mag;
}

// lhs against a double, exactly (non-finite raises, as rational{rhs}). Every
// rational lies strictly inside ±2^64, so a larger double is decided by its sign.
[[nodiscard]] inline constexpr std::strong_ordering compare_double(const rational& lhs, double rhs) {
    if (is_finite(rhs) && past_64_bits(rhs)) [[unlikely]]
        return rhs < 0 ? std::strong_ordering::greater : std::strong_ordering::less;
    if (abs_fraction_exact(rhs) || not is_finite(rhs)) [[likely]]
        return lhs <=> rational{rhs};
    return compare_fine_double(lhs, rhs);
}

// lhs against a floating point of any width, exactly: one wider than double
// (long double) with bits past the double's compares as N·2^180 against its
// scaled magnitude times D.
template <std::floating_point T>
[[nodiscard]] constexpr std::strong_ordering compare_float(const rational& lhs, T rhs) {
    if constexpr (wide_float<T>)
        if (is_finite(rhs) && !is_double_value(rhs)) [[unlikely]] {
            const int ls = sign(lhs), rs = rhs < 0 ? -1 : 1;
            if (ls != rs)
                return ls <=> rs;
            const T              a   = rhs < 0 ? -rhs : rhs;
            std::strong_ordering mag = std::strong_ordering::equal;
            if (!(a < static_cast<T>(0x1p64))) // past every rational
                mag = std::strong_ordering::less;
            else if (a < static_cast<T>(0x1p-64)) // below every nonzero rational
                mag = std::strong_ordering::greater;
            else {
                using W              = wide_uint<5>;
                const auto [m, tail] = scaled_magnitude(a);
                mag                  = (W{lhs.Numerator} << wide_float_shift) <=> W{m} * W{abs_den(lhs.Denominator)};
                if (mag == 0)
                    mag = 0 <=> tail;
            }
            return ls < 0 ? (0 <=> mag) : mag;
        }
    return compare_double(lhs, static_cast<double>(rhs));
}

template <plain_number T>
[[nodiscard]] constexpr bool rational::operator==(T value) const {
    if constexpr (std::floating_point<T>)
        return compare_float(*this, value) == 0;
    else
        return operator==(rational{value});
}

template <plain_number T>
[[nodiscard]] inline constexpr auto operator<=>(T lhs, const rational& rhs) {
    if constexpr (std::floating_point<T>)
        return 0 <=> compare_float(rhs, lhs);
    else
        return rational{lhs} <=> rhs;
}

template <plain_number T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, T rhs) {
    if constexpr (std::floating_point<T>)
        return compare_float(lhs, rhs);
    else
        return lhs <=> rational{rhs};
}

//---------------------------------------------------------------------------
// Expected-lifting operators — one generic overload per arithmetic operator
// that engages when an operand is a std::expected, both unwrap to arithmetic,
// and at least one to rational. Gating on `plain_number` (not `insidable`, which
// isn't visible this low) excludes inside operands, so inside-involving expected
// expressions partition cleanly to arithmetic.hpp's generic instead.
//---------------------------------------------------------------------------
template <class L, class R>
concept rational_lift_operands =
    (expected_like<L> || expected_like<R>) && plain_number<unwrap_t<L>> && plain_number<unwrap_t<R>> &&
    (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

//---------------------------------------------------------------------------
// Binary operators: the checked rational ⋈ rational cores, then per operator
// the arithmetic-operand forms (direct construction, no lift overhead), the
// expected-operand form (propagates via lift), and the compound assignments.
// Compound assignments unwrap with .value() — std::bad_expected_access on
// overflow; callers needing a non-throwing path use the binary operators.
//---------------------------------------------------------------------------
// add_impl fails on a 64-bit step's overflow; the sum may still fit once
// reduced, which add_wide (cold) settles.
[[nodiscard]] inline constexpr std::expected<rational, errc> operator+(const rational& lhs, const rational& rhs) {
    const auto r = rational::add_impl<true, false>(lhs, rhs);
    if (!r) [[unlikely]]
        return add_wide<true>(lhs, rhs);
    return r;
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator-(const rational& lhs, const rational& rhs) {
    return operator+(lhs, -rhs);
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator*(const rational& lhs, const rational& rhs) {
    return rational::mul_impl<true>(lhs, rhs);
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator/(const rational& lhs, const rational& rhs) {
    return rational::div_impl<true>(lhs, rhs);
}

// Quiet checked ops: like the operators, but an overflow is an error value
// even in constant evaluation (the operators make it a compile error there).
// For compile-time code that asks whether a result fits.
[[nodiscard]] inline constexpr std::expected<rational, errc> try_add(const rational& a, const rational& b) {
    const auto r = rational::add_impl<true, false>(a, b);
    if (!r) [[unlikely]]
        return add_wide<false>(a, b);
    return r;
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_sub(const rational& a, const rational& b) {
    return try_add(a, -b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_mul(const rational& a, const rational& b) {
    return rational::mul_impl<true, false>(a, b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_div(const rational& a, const rational& b) {
    return rational::div_impl<true, false>(a, b);
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator-(const std::expected<rational, errc>& v) {
    return lift([](rational r) { return -r; }, v);
}

#define BEMAN_INSIDE_RATIONAL_OP(op)                                                                        \
    template <plain_number T>                                                                               \
    [[nodiscard]] inline constexpr auto operator op(T lhs, rational const& rhs) {                           \
        return rational{lhs} op rhs;                                                                        \
    }                                                                                                       \
    template <plain_number T>                                                                               \
    [[nodiscard]] inline constexpr auto operator op(rational const& lhs, T rhs) {                           \
        return lhs op rational{rhs};                                                                        \
    }                                                                                                       \
    template <class L, class R>                                                                             \
        requires rational_lift_operands<L, R>                                                               \
    [[nodiscard]] inline constexpr auto operator op(L const& lhs, R const& rhs) {                           \
        return lift([](auto const& a, auto const& b) { return a op b; }, lhs, rhs);                         \
    }                                                                                                       \
    inline constexpr rational& rational::operator op## = (rational const& rhs) {                            \
        *this = (*this op rhs).value();                                                                     \
        return *this;                                                                                       \
    }                                                                                                       \
    template <plain_number T>                                                                               \
    inline constexpr rational& operator op## = (rational & lhs, T rhs) {                                    \
        return lhs op## = rational{rhs};                                                                    \
    }                                                                                                       \
    inline constexpr rational& operator op## = (rational & lhs, std::expected<rational, errc> const& rhs) { \
        return lhs op## = rhs.value();                                                                      \
    }

BEMAN_INSIDE_RATIONAL_OP(+)
BEMAN_INSIDE_RATIONAL_OP(-)
BEMAN_INSIDE_RATIONAL_OP(*)
BEMAN_INSIDE_RATIONAL_OP(/)
#undef BEMAN_INSIDE_RATIONAL_OP

//---------------------------------------------------------------------------
// divides_evenly
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr bool divides_evenly(const rational& dividend, const rational& divisor) {
    if (divisor == 0)
        return true; // convention: everything divides 0 evenly
    if (dividend.Numerator == 0)
        return true; // 0 / anything is the integer 0

    // dividend/divisor ∈ ℤ without forming the (possibly umax-overflowing)
    // quotient numerator. In lowest terms dividend = p/q, divisor = r/s; the
    // quotient p·s/(q·r) is integral iff q | s and r | p (rationals are
    // canonicalized, so gcd(p,q)=gcd(r,s)=1). All checks are on single fields.
    const umax p = dividend.Numerator, q = abs_den(dividend.Denominator);
    const umax r = divisor.Numerator, s = abs_den(divisor.Denominator);
    return (s % q == 0) && (p % r == 0);
}

// Checked builder for per<D> below: a static_assert here fires at the user's
// spelling (also through just<N, D>, which is N · per<D>).
template <umax D>
consteval rational make_per() {
    static_assert(D >= 1, "per<D> is the positive step 1/D; a continuous grid is spelled 0");
    static_assert(std::in_range<imax>(D), "per<D>: denominator too large");
    return rational{umax{1}, D};
}
} // namespace beman::inside::detail

namespace beman::inside {
// `rational` — an exact 64-bit fraction: the value of a literal (`0.1_r`), a
// continuous inside's raw, and a runtime exact value to compute with.
using detail::rational;

// The grid-building helpers:
//   per<D>       the step 1/D — the common notch (per<256> is Q·8);
// any other exact ratio is the point just<N, D> (core.hpp): a limit like
// just<-6, 5> for -1.2, or another step like just<360, 4096>
// (the _r literal, an exact decimal / hex value — 0.1_r is exactly 1/10 —
// is in detail/number_text.hpp.)
// Integer steps are plain integers ({{0, 100}, 5}); grid::validate rejects a
// negative notch however it is spelled.
template <umax D>
inline constexpr detail::rational per = detail::make_per<D>();

} // namespace beman::inside


// ======================================================================
//  beman/inside/interval.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/grid_rational.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/number_text.hpp
// ======================================================================




namespace beman::inside::detail {
//---------------------------------------------------------------------------
// _ins / _r literal parser — shared between core.hpp's `_ins` and `_r` below.
// Accepts:
//   integer:           5, 1'000
//   decimal:           1.25, .5
//   decimal scientific 1.5e2, 2.5e-1
//   hex integer:       0xff
//   binary integer:    0b1010
//   hex float (Q-fmt): 0x1p15, 0x1p-15, 0x1.8p3
// Exact (no double round-trip). A malformed or overflowing literal fails
// constant evaluation via `constexpr_error<Msg>()`; parse_text (below) is the
// runtime form behind parse.
//---------------------------------------------------------------------------
constexpr int parse_digit(char c, int base) {
    if (c >= '0' && c <= '9') {
        int d = c - '0';
        return d < base ? d : -1;
    }
    if (base == 16) {
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
    }
    return -1;
}

// Failure kinds of the number parser. The literals turn each into a named
// compile-time diagnostic; runtime parsing maps them onto errc.
enum class parse_fail : unsigned char {
    none,
    empty,
    invalid_exponent,
    multiple_dots,
    dot_in_binary,
    invalid_digit,
    numerator_overflow,
    denominator_overflow,
    hex_fraction_too_long,
    p_exponent_too_large,
    p_exponent_denominator_overflow,
    e_exponent_numerator_overflow,
    e_exponent_denominator_overflow,
};

struct parsed_number {
    rational   Value;
    parse_fail Fail;
};

// An unsigned number in the grammar of the _ins / _r literals, scanned in
// one pass: the radix and the exponent come back, and each mantissa digit
// goes to on_digit(d, in_frac, base) (false stops the scan: the builder's
// overflow). parse_number (64-bit), parse_exact (wide, wide_value.hpp) and
// stand_in build from it, so all read one grammar.
struct scanned_number {
    int        Base    = 10;
    int        Exp     = 0; // capped at 100000: past it any value overflows
    bool       ExpNeg  = false;
    bool       HasP    = false; // 2^Exp (hex floats)
    bool       HasE    = false; // 10^Exp (decimal scientific)
    bool       Stopped = false; // on_digit returned false
    parse_fail Fail    = parse_fail::none;
};

template <typename OnDigit>
[[gnu::always_inline]] constexpr scanned_number scan_number(const char* first, const char* last, OnDigit&& on_digit) {
    scanned_number r;
    auto           fail = [&](parse_fail f) {
        r.Fail = f;
        return r;
    };
    if (last - first >= 2 && first[0] == '0') { // radix prefix
        if (first[1] == 'x' || first[1] == 'X') {
            r.Base = 16;
            first += 2;
        } else if (first[1] == 'b' || first[1] == 'B') {
            r.Base = 2;
            first += 2;
        }
    }
    bool in_frac = false, seen_digit = false;
    for (; first != last; ++first) {
        const char c = *first;
        if (c == '\'')
            continue;
        if (c == '.') {
            if (in_frac)
                return fail(parse_fail::multiple_dots);
            if (r.Base == 2)
                return fail(parse_fail::dot_in_binary);
            in_frac = true;
            continue;
        }
        if (((c == 'p' || c == 'P') && r.Base == 16) || ((c == 'e' || c == 'E') && r.Base == 10))
            break;
        const int d = parse_digit(c, r.Base);
        if (d < 0)
            return fail(parse_fail::invalid_digit);
        seen_digit = true;
        if (!r.Stopped && !on_digit(d, in_frac, r.Base))
            r.Stopped = true; // keep scanning: malformed text still reports invalid_format
    }
    if (!seen_digit)
        return fail(parse_fail::empty);
    if (first == last)
        return r;
    (r.Base == 16 ? r.HasP : r.HasE) = true;
    bool exp_seen_digit = false, exp_seen_sign = false; // one sign, right after e/p
    for (++first; first != last; ++first) {
        const char c = *first;
        if (c == '\'')
            continue;
        if (!exp_seen_digit && !exp_seen_sign && (c == '+' || c == '-')) {
            r.ExpNeg      = (c == '-');
            exp_seen_sign = true;
            continue;
        }
        if (c < '0' || c > '9')
            return fail(parse_fail::invalid_exponent);
        if (r.Exp < 100000)
            r.Exp = r.Exp * 10 + (c - '0');
        exp_seen_digit = true;
    }
    if (!exp_seen_digit)
        return fail(parse_fail::invalid_exponent);
    return r;
}

// Unsigned number in [first, last): the grammar of the _ins / _r literals.
constexpr parsed_number parse_number(const char* first, const char* last) {
    auto fail       = [](parse_fail f) { return parsed_number{rational{}, f}; };
    umax num        = 0;
    int  frac_len   = 0;
    int  frac_zeros = 0; // deferred fractional zeros
    // A fractional zero only matters if a non-zero digit follows: defer it,
    // so trailing zeros ("1.000…0") cannot overflow num / den.
    const scanned_number sc = scan_number(first, last, [&](int d, bool in_frac, int base) {
        const umax base_u = static_cast<umax>(base);
        if (in_frac && d == 0) {
            ++frac_zeros;
            return true;
        }
        for (; frac_zeros > 0; --frac_zeros, ++frac_len) {
            if (num > (~umax{0}) / base_u)
                return false;
            num *= base_u;
        }
        if (num > (~umax{0} - static_cast<umax>(d)) / base_u)
            return false;
        num = num * base_u + static_cast<umax>(d);
        if (in_frac)
            ++frac_len;
        return true;
    });
    if (sc.Fail != parse_fail::none)
        return fail(sc.Fail);
    if (sc.Stopped)
        return fail(parse_fail::numerator_overflow);
    const int  base      = sc.Base;
    const int  exp       = sc.Exp;
    const bool exp_neg   = sc.ExpNeg;
    const bool has_p_exp = sc.HasP;
    const bool has_e_exp = sc.HasE;

    // Build denominator from fractional part.
    // For decimal: den = 10^frac_len. For hex: den = 2^(4*frac_len).
    umax den = 1;
    if (base == 10) {
        for (int k = 0; k < frac_len; ++k) {
            if (den > (~umax{0}) / 10u)
                return fail(parse_fail::denominator_overflow);
            den *= 10u;
        }
    } else if (base == 16) {
        const int shift = 4 * frac_len;
        if (shift >= 64)
            return fail(parse_fail::hex_fraction_too_long);
        den <<= shift;
    }

    // Zero stays zero whatever the exponent: skip the scaling (which could
    // only overflow).
    if (num == 0)
        return parsed_number{rational{umax{0}}, parse_fail::none};

    // Apply binary exponent (hex floats, `p`).
    // The factors of two cancel first (0x0.1p4 is 1, 0x10p-66 is 2^-62), so
    // only a value past the 64-bit rationals fails; 2^63 itself fits.
    if (has_p_exp) {
        umax&     other  = exp_neg ? num : den; // the side the shift cancels against
        const int tz     = std::countr_zero(other);
        const int cancel = tz < exp ? tz : exp;
        other >>= cancel;
        const int e = exp - cancel;
        if (e >= 64)
            return fail(parse_fail::p_exponent_too_large);
        if (!exp_neg) {
            if (num > (~umax{0}) >> e)
                return fail(parse_fail::numerator_overflow);
            num <<= e;
        } else {
            if (den > (~umax{0}) >> e)
                return fail(parse_fail::p_exponent_denominator_overflow);
            den <<= e;
        }
    }

    // Apply decimal exponent (decimal scientific, `e`).
    if (has_e_exp) {
        for (int k = 0; k < exp; ++k) {
            if (!exp_neg) {
                if (num > (~umax{0}) / 10u)
                    return fail(parse_fail::e_exponent_numerator_overflow);
                num *= 10u;
            } else {
                if (den > (~umax{0}) / 10u)
                    return fail(parse_fail::e_exponent_denominator_overflow);
                den *= 10u;
            }
        }
    }

    // In lowest terms before the signed-denominator check (0.5e-18 is
    // 1/(2·10^18), though 10^19 itself is past imax).
    const umax g = std::gcd(num, den);
    num /= g;
    den /= g;
    if (!std::in_range<imax>(den))
        return fail(parse_fail::denominator_overflow);
    return parsed_number{rational{num, den}, parse_fail::none};
}

// Whether a numeric literal's characters are an octal integer to C++: a
// leading 0, then only digits (and ' separators). The literal grammar reads
// decimal, so 010 would silently be ten, not eight: such literals are refused.
template <char... Chars>
consteval bool octal_literal() {
    constexpr char text[] = {Chars..., '\0'};
    if (sizeof...(Chars) < 2 || text[0] != '0')
        return false;
    return std::all_of(text + 1, text + sizeof...(Chars), [](char c) { return c == '\'' || (c >= '0' && c <= '9'); });
}

template <char... Chars>
consteval rational parse_ins_literal() {
    constexpr char          src[] = {Chars..., '\0'};
    constexpr parsed_number r     = parse_number(src, src + sizeof...(Chars));
    if constexpr (octal_literal<Chars...>())
        constexpr_error<"_ins/_r literal: a leading 0 makes an octal literal; write the number without it">();
    else if constexpr (r.Fail == parse_fail::invalid_exponent)
        constexpr_error<"_ins/_r literal: invalid char in exponent">();
    else if constexpr (r.Fail == parse_fail::multiple_dots)
        constexpr_error<"_ins/_r literal: multiple '.'">();
    else if constexpr (r.Fail == parse_fail::dot_in_binary)
        constexpr_error<"_ins/_r literal: '.' not allowed in binary">();
    else if constexpr (r.Fail == parse_fail::invalid_digit || r.Fail == parse_fail::empty)
        constexpr_error<"_ins/_r literal: invalid digit for radix">();
    else if constexpr (r.Fail == parse_fail::numerator_overflow)
        constexpr_error<"_ins/_r literal: numerator overflow">();
    else if constexpr (r.Fail == parse_fail::denominator_overflow)
        constexpr_error<"_ins/_r literal: denominator overflow">();
    else if constexpr (r.Fail == parse_fail::hex_fraction_too_long)
        constexpr_error<"_ins/_r literal: hex fraction too long">();
    else if constexpr (r.Fail == parse_fail::p_exponent_too_large)
        constexpr_error<"_ins/_r literal: p exponent too large">();
    else if constexpr (r.Fail == parse_fail::p_exponent_denominator_overflow)
        constexpr_error<"_ins/_r literal: p exponent denominator overflow">();
    else if constexpr (r.Fail == parse_fail::e_exponent_numerator_overflow)
        constexpr_error<"_ins/_r literal: e exponent numerator overflow">();
    else if constexpr (r.Fail == parse_fail::e_exponent_denominator_overflow)
        constexpr_error<"_ins/_r literal: e exponent denominator overflow">();
    return r.Value;
}

// Runtime text → exact value: an optional sign, then a number in the literal
// grammar, or a fraction `N/D` of two such numbers (the form to_string prints
// for a non-terminating value). Malformed text is errc::invalid_format; a value
// past the 64-bit fields is errc::overflow; `N/0` is errc::division_by_zero.
// Text as an optional sign and the number's [First, Slash), with the
// denominator after Slash when Slash != last.
struct signed_fraction_text {
    bool        Neg;
    const char* First;
    const char* Slash;
};
constexpr signed_fraction_text split_fraction(const char* first, const char* last) noexcept {
    const bool neg = first != last && *first == '-';
    if (first != last && (*first == '+' || *first == '-'))
        ++first;
    const char* slash = first;
    while (slash != last && *slash != '/')
        ++slash;
    return {neg, first, slash};
}

// The sign and N/D around `one` (text → an expected value): N/0 is
// division_by_zero.
template <class One, class IsZero>
constexpr auto parse_fraction(const char* first, const char* last, One one, IsZero is_zero) {
    const auto [neg, f, slash] = split_fraction(first, last);
    auto v                     = one(f, slash);
    if (v && slash != last) {
        const auto d = one(slash + 1, last);
        if (!d)
            return d;
        if (is_zero(*d))
            return decltype(v){std::unexpect, errc::division_by_zero};
        v = *v / *d;
    }
    if (v && neg)
        v = -*v;
    return v;
}

constexpr std::expected<rational, errc> parse_text(const char* first, const char* last) {
    auto one = [](const char* f, const char* l) -> std::expected<rational, errc> {
        const parsed_number r = parse_number(f, l);
        switch (r.Fail) {
        case parse_fail::none:
            return r.Value;
        case parse_fail::numerator_overflow:
        case parse_fail::denominator_overflow:
        case parse_fail::hex_fraction_too_long:
        case parse_fail::p_exponent_too_large:
        case parse_fail::p_exponent_denominator_overflow:
        case parse_fail::e_exponent_numerator_overflow:
        case parse_fail::e_exponent_denominator_overflow:
            return std::unexpected{errc::overflow};
        default:
            return std::unexpected{errc::invalid_format};
        }
    };
    return parse_fraction(first, last, one, [](const rational& d) { return d.Numerator == 0; });
}

template <char... Chars>
constexpr rational operator""_r() {
    return parse_ins_literal<Chars...>();
}
} // namespace beman::inside::detail

namespace beman::inside {
//   _r literal   an exact decimal / hex value, e.g. 0.1_r is exactly 1/10.
using detail::operator""_r;
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/big_rational.hpp
// ======================================================================



//---------------------------------------------------------------------------
// big_int / big_rational — grid numbers of any size (static reflection).
//
// A grid is a template argument, so its numbers must be structural values.
// A value of at most one limb is held inline (Small); a larger magnitude is
// interned in static storage with std::define_static_array and held by
// pointer. Equal arrays intern to the same object, and every value has one
// canonical form (reduced fraction, no leading zero limbs, inline when it
// fits), so equal values are the same template argument.
//
// Interning is consteval: a value past 64 bits exists only at compile time.
// Small values compute without allocation and work at runtime as well (for
// grid::try_make); a runtime operation whose result would need more than one
// limb reports errc::overflow.
//
// Arithmetic on large values runs on transient std::vector<umax> magnitudes
// with the wide_int limb kernels, during constant evaluation only.
//---------------------------------------------------------------------------

namespace beman::inside::detail {
namespace big {
using mag = std::vector<umax>; // little-endian, no top zeros

constexpr void trim(mag& m) { m.resize(limb::top_size(m.data(), m.size())); }

constexpr std::strong_ordering compare(const mag& a, const mag& b) {
    if (a.size() != b.size())
        return a.size() <=> b.size();
    return std::lexicographical_compare_three_way(a.rbegin(), a.rend(), b.rbegin(), b.rend());
}

constexpr mag add(const mag& a, const mag& b) {
    const mag& x = a.size() >= b.size() ? a : b; // the longer
    const mag& y = a.size() >= b.size() ? b : a;
    mag        r(x.size() + 1, 0);
    r.back() = limb::add(r.data(), x.data(), x.size(), y.data(), y.size());
    trim(r);
    return r;
}

// a − b for a ≥ b.
constexpr mag sub(const mag& a, const mag& b) {
    mag r(a.size(), 0);
    limb::sub(r.data(), a.data(), a.size(), b.data(), b.size());
    trim(r);
    return r;
}

constexpr mag mul(const mag& a, const mag& b) {
    if (a.empty() || b.empty())
        return {};
    mag r(a.size() + b.size(), 0);
    limb::mul_basecase(r.data(), a.data(), a.size(), b.data(), b.size());
    trim(r);
    return r;
}

// {a / b, a % b} for b ≠ 0.
struct divmod_result {
    mag Quotient;
    mag Remainder;
};
constexpr divmod_result divmod(const mag& a, const mag& b) {
    if (compare(a, b) < 0)
        return {{}, a};
    mag q(a.size() - b.size() + 1, 0), r(b.size(), 0), un(a.size() + 1, 0), vn(b.size(), 0);
    limb::divmod(a.data(), a.size(), b.data(), b.size(), q.data(), r.data(), un.data(), vn.data());
    trim(q);
    trim(r);
    return {q, r};
}

constexpr mag gcd(mag a, mag b) {
    return euclid_gcd(std::move(a), std::move(b), [](const mag& x, const mag& y) { return divmod(x, y).Remainder; });
}

constexpr int bit_width(const mag& m) {
    return m.empty() ? 0 : static_cast<int>(m.size() - 1) * 64 + (64 - std::countl_zero(m.back()));
}

// m · 2^k (k ≥ 0).
constexpr mag shl(const mag& m, int k) {
    mag r(m.size() + static_cast<std::size_t>(k / 64) + 1, 0);
    limb::shift_left(r.data(), r.size(), m.data(), m.size(), k);
    trim(r);
    return r;
}

// A fraction of transient magnitudes, unreduced: nothing is interned, so it
// computes at runtime too (a big grid's predicates against a runtime value).
struct fraction {
    mag  Num;          // |numerator|
    mag  Den{umax{1}}; // > 0
    bool Negative = false;
};

constexpr std::strong_ordering compare(const fraction& a, const fraction& b) {
    const int sa = a.Num.empty() ? 0 : a.Negative ? -1 : 1, sb = b.Num.empty() ? 0 : b.Negative ? -1 : 1;
    if (sa != sb)
        return sa <=> sb;
    const std::strong_ordering m = compare(mul(a.Num, b.Den), mul(b.Num, a.Den));
    return sa < 0 ? 0 <=> m : m;
}

constexpr fraction operator+(const fraction& a, const fraction& b) {
    const mag x = mul(a.Num, b.Den), y = mul(b.Num, a.Den);
    fraction  r;
    r.Den = mul(a.Den, b.Den);
    if (a.Negative == b.Negative) {
        r.Num      = add(x, y);
        r.Negative = a.Negative;
    } else if (compare(x, y) >= 0) {
        r.Num      = sub(x, y);
        r.Negative = a.Negative;
    } else {
        r.Num      = sub(y, x);
        r.Negative = b.Negative;
    }
    r.Negative = r.Negative && !r.Num.empty();
    return r;
}

// (a − b) / n is an integer (n ≠ 0).
constexpr bool same_lattice(const fraction& a, fraction b, const fraction& n) {
    b.Negative       = !b.Negative && !b.Num.empty();
    const fraction d = a + b;
    return divmod(mul(d.Num, n.Den), mul(d.Den, n.Num)).Remainder.empty();
}

// A floating-point value within the double range exactly, in lowest terms:
// a wider one (long double) as the sum of the doubles it splits into.
template <std::floating_point T>
constexpr fraction fraction_of_float(T v) {
    if constexpr (wide_float<T>) {
        if (static_cast<T>(static_cast<double>(v)) != v) {
            fraction s;
            for_each_double_part(v, [&](double p) {
                s = s + fraction_of_float(p);
                return true;
            });
            const mag g = gcd(s.Num, s.Den);
            return {divmod(s.Num, g).Quotient, divmod(s.Den, g).Quotient, s.Negative};
        }
    }
    const double d = static_cast<double>(v);
    if (d == 0)
        return {};
    auto [mant, e] = double_parts(d);
    if (e < 0) { // in lowest terms: the twos the denominator shares
        const int z = std::min(std::countr_zero(mant), -e);
        mant >>= z;
        e += z;
    }
    return e >= 0 ? fraction{shl(mag{mant}, e), mag{umax{1}}, d < 0}
                  : fraction{mag{mant}, shl(mag{umax{1}}, -e), d < 0};
}
} // namespace big

//---------------------------------------------------------------------------
// big_int — a signed integer of any size; structural.
//---------------------------------------------------------------------------
struct big_int {
    umax        Small = 0;       // the magnitude when Size == 0
    const umax* Limbs = nullptr; // interned magnitude, Size ≥ 2 limbs
    std::size_t Size  = 0;       // tested instead of Limbs: under -fno-delete-null-pointer-checks
                                 // (implied by -fsanitize=null) GCC cannot compare an interned
                                 // pointer with nullptr in a constant expression
    bool Negative = false;       // never set for zero

    constexpr big_int() = default;

    template <std::integral T>
    constexpr big_int(T v) noexcept
        : Small{std::is_signed_v<T> && v < 0 ? umax{0} - static_cast<umax>(v) : static_cast<umax>(v)},
          Negative{std::is_signed_v<T> && v < 0} {}

    // From a wide_int (sign-extending a signed one).
    template <std::size_t N, bool S, std::unsigned_integral L>
    constexpr big_int(const wide_int<N, S, L>& w) {
        static_assert(std::numeric_limits<L>::digits == 64, "big_int: umax limbs only");
        const bool                  neg = w.negative();
        const wide_int<N, false, L> m(neg ? -w : w);
        *this = from_mag(big::mag(std::begin(m.Word), std::end(m.Word)), neg);
    }

    // By value, not by pointer (see Size): equal magnitudes intern to the
    // same array, so this matches the defaulted comparison.
    friend constexpr bool operator==(const big_int& a, const big_int& b) noexcept {
        return a.Size == b.Size && a.Small == b.Small && a.Negative == b.Negative &&
               std::equal(a.Limbs, a.Limbs + a.Size, b.Limbs);
    }

    [[nodiscard]] constexpr bool is_zero() const noexcept { return Size == 0 && Small == 0; }
    [[nodiscard]] constexpr bool negative() const noexcept { return Negative; }
    [[nodiscard]] constexpr bool fits_limb() const noexcept { return Size == 0; }

    [[nodiscard]] constexpr big::mag magnitude() const {
        if (Size != 0)
            return big::mag(Limbs, Limbs + Size);
        if (Small)
            return big::mag{Small};
        return {};
    }

    // The canonical big_int of a magnitude and sign: inline when it fits one
    // limb, else interned (constant evaluation only).
    static constexpr big_int from_mag(big::mag m, bool neg) {
        big::trim(m);
        big_int r;
        if (m.size() <= 1) {
            r.Small    = m.empty() ? umax{0} : m[0];
            r.Negative = neg && r.Small != 0;
            return r;
        }
        if consteval {
            const auto s = std::define_static_array(m);
            r.Limbs      = s.data();
            r.Size       = s.size();
            r.Negative   = neg;
            return r;
        } else {
            raise(errc::overflow, "big_int: a value past 64 bits exists only at compile time");
        }
    }

    [[nodiscard]] constexpr int bit_width() const {
        return Size != 0 ? big::bit_width(magnitude()) : (Small ? 64 - std::countl_zero(Small) : 0);
    }

    // Truncating conversion to a builtin integer or wide_int (two's complement).
    template <typename T>
        requires raw_integer<T>
    constexpr explicit operator T() const {
        if constexpr (std::integral<T>) {
            const umax low = Size != 0 ? Limbs[0] : Small;
            return static_cast<T>(Negative ? umax{0} - low : low);
        } else {
            T w{0};
            for (std::size_t i = 0; i < sizeof(w.Word) / sizeof(w.Word[0]); ++i)
                w.Word[i] = Size != 0 ? (i < Size ? Limbs[i] : 0) : (i == 0 ? Small : 0);
            return Negative ? -w : w;
        }
    }

    friend constexpr std::strong_ordering operator<=>(const big_int& a, const big_int& b) {
        if (a.Negative != b.Negative)
            return a.Negative ? std::strong_ordering::less : std::strong_ordering::greater;
        const std::strong_ordering m =
            (a.Size != 0 || b.Size != 0) ? big::compare(a.magnitude(), b.magnitude()) : a.Small <=> b.Small;
        return a.Negative ? 0 <=> m : m;
    }

    friend constexpr big_int operator-(big_int a) {
        a.Negative = !a.Negative && !a.is_zero();
        return a;
    }

    friend constexpr big_int operator+(const big_int& a, const big_int& b) {
        if (a.Size == 0 && b.Size == 0) {
            if (a.Negative == b.Negative) {
                umax       c = 0;
                const umax s = limb::add_carry(a.Small, b.Small, c);
                if (c == 0) {
                    big_int r;
                    r.Small    = s;
                    r.Negative = a.Negative && s != 0;
                    return r;
                }
            } else {
                const bool a_big = a.Small >= b.Small;
                big_int    r;
                r.Small    = a_big ? a.Small - b.Small : b.Small - a.Small;
                r.Negative = (a_big ? a.Negative : b.Negative) && r.Small != 0;
                return r;
            }
        }
        const big::mag ma = a.magnitude(), mb = b.magnitude();
        if (a.Negative == b.Negative)
            return from_mag(big::add(ma, mb), a.Negative);
        return big::compare(ma, mb) >= 0 ? from_mag(big::sub(ma, mb), a.Negative)
                                         : from_mag(big::sub(mb, ma), b.Negative);
    }
    friend constexpr big_int operator-(const big_int& a, const big_int& b) { return a + (-b); }

    friend constexpr big_int operator*(const big_int& a, const big_int& b) {
        if (a.Size == 0 && b.Size == 0) {
            const limb::pair<umax> p = limb::mul(a.Small, b.Small);
            if (p.Hi == 0) {
                big_int r;
                r.Small    = p.Lo;
                r.Negative = (a.Negative != b.Negative) && p.Lo != 0;
                return r;
            }
        }
        return from_mag(big::mul(a.magnitude(), b.magnitude()), a.Negative != b.Negative);
    }

    // Truncating division, as the builtin operators. Pre: b ≠ 0.
    struct divmod_result;
    static constexpr divmod_result divmod(const big_int& a, const big_int& b);
    friend constexpr big_int       operator/(const big_int& a, const big_int& b);
    friend constexpr big_int       operator%(const big_int& a, const big_int& b);

    // a · 2^k (k ≥ 0).
    friend constexpr big_int operator<<(const big_int& a, int k) {
        if (a.Size == 0 && k < 64 && (k == 0 || (a.Small >> (64 - k)) == 0)) {
            big_int r = a;
            r.Small   = a.Small << k;
            return r;
        }
        return from_mag(big::shl(a.magnitude(), k), a.Negative);
    }
};

struct big_int::divmod_result {
    big_int Quotient;
    big_int Remainder;
};

// (Free functions, not hidden friends: callers spell detail::abs / gcd.)
constexpr big_int abs(big_int a) {
    a.Negative = false;
    return a;
}

constexpr auto big_int::divmod(const big_int& a, const big_int& b) -> divmod_result {
    if (b.is_zero())
        fail_hard<"big_int: division by zero">(errc::division_by_zero);
    if (a.Size == 0 && b.Size == 0) {
        big_int q, r;
        q.Small    = a.Small / b.Small;
        q.Negative = (a.Negative != b.Negative) && q.Small != 0;
        r.Small    = a.Small % b.Small;
        r.Negative = a.Negative && r.Small != 0;
        return {q, r};
    }
    big::divmod_result m = big::divmod(a.magnitude(), b.magnitude());
    return {from_mag(std::move(m.Quotient), a.Negative != b.Negative), from_mag(std::move(m.Remainder), a.Negative)};
}
constexpr big_int operator/(const big_int& a, const big_int& b) { return big_int::divmod(a, b).Quotient; }
constexpr big_int operator%(const big_int& a, const big_int& b) { return big_int::divmod(a, b).Remainder; }

constexpr big_int gcd(const big_int& a, const big_int& b) { return euclid_gcd(abs(a), abs(b)); }

constexpr int bit_width_of(const big_int& v) { return v.bit_width(); }

//---------------------------------------------------------------------------
// big_rational — an exact fraction of any size; structural and canonical
// (reduced, positive denominator).
//---------------------------------------------------------------------------
struct big_rational {
    big_int Num{};
    big_int Den{1};

    constexpr big_rational() = default;
    template <std::integral T>
    constexpr big_rational(T v) noexcept : Num{v} {}
    constexpr big_rational(big_int n, big_int d = big_int{1}) : Num{n}, Den{d} { normalize(); }
    constexpr big_rational(const rational& r) : Num{r.Numerator}, Den{abs_den(r.Denominator)} {
        if (r.Denominator < 0)
            Num = -Num;
    }
    // Anything else that converts to `rational` (an inside: just<-6, 5>)
    // takes that one conversion, so it can be a grid limit or notch.
    template <class T>
        requires(std::is_class_v<T> && !std::same_as<T, rational> && std::convertible_to<const T&, rational>)
    constexpr big_rational(const T& v) : big_rational(rational(v)) {}
    constexpr big_rational(std::floating_point auto d) // exact binary value
    {
        if (d != d || d - d != 0)
            fail_hard<"big_rational: not a finite number">(errc::not_finite);
        if constexpr (wide_float<decltype(d)>)
            if (!is_double_value(d) && !is_finite(static_cast<double>(d)))
                fail_hard<"big_rational: a long double past the double range">(errc::overflow);
        // In lowest terms from the start (a long double as the sum of the
        // doubles it splits into).
        const big::fraction f = big::fraction_of_float(d);
        Num                   = big_int::from_mag(f.Num, f.Negative);
        Den                   = big_int::from_mag(f.Den, false);
    }

    friend constexpr bool operator==(const big_rational&, const big_rational&) = default;

    constexpr void normalize() {
        if (Den.is_zero())
            fail_hard<"big_rational: zero denominator">(errc::division_by_zero);
        if (Den.negative()) {
            Num = -Num;
            Den = -Den;
        }
        if (Num.is_zero()) {
            Den = big_int{1};
            return;
        }
        const big_int g = gcd(Num, Den);
        if (!(g == big_int{1})) {
            Num = Num / g;
            Den = Den / g;
        }
    }

    // The 64-bit rational, when the value fits it.
    [[nodiscard]] constexpr bool fits_rational() const noexcept {
        return Num.fits_limb() && Den.fits_limb() && std::in_range<imax>(Den.Small);
    }

    // Implicit: lets every 64-bit path keep working on a small grid. A value
    // past 64 bits fails the build when such a path meets it.
    constexpr operator rational() const {
        if (!fits_rational()) {
            if consteval {
                constexpr_error<"grid number past 64 bits on a path that needs a 64-bit rational">();
            }
            raise(errc::overflow, "big_rational: value past 64 bits");
        }
        const imax d = static_cast<imax>(Den.Small);
        return rational{Num.Small, Num.negative() ? -d : d};
    }

    friend constexpr std::strong_ordering operator<=>(const big_rational& a, const big_rational& b) {
        // One-limb parts compare by a 128-bit cross product, forming no big
        // value: runtime comparisons (a store's range check) stay allocation-
        // and error-free.
        if (a.Num.fits_limb() && a.Den.fits_limb() && b.Num.fits_limb() && b.Den.fits_limb()) {
            if (a.Num.negative() != b.Num.negative())
                return a.Num.negative() ? std::strong_ordering::less : std::strong_ordering::greater;
            const std::strong_ordering m = limb::mul_compare(a.Num.Small, b.Den.Small, b.Num.Small, a.Den.Small);
            return a.Num.negative() ? 0 <=> m : m;
        }
        return big::compare(big::fraction{a.Num.magnitude(), a.Den.magnitude(), a.Num.negative()},
                            big::fraction{b.Num.magnitude(), b.Den.magnitude(), b.Num.negative()});
    }

    friend constexpr big_rational operator-(big_rational a) {
        a.Num = -a.Num;
        return a;
    }
    friend constexpr big_rational operator+(const big_rational& a, const big_rational& b) {
        return {a.Num * b.Den + b.Num * a.Den, a.Den * b.Den};
    }
    friend constexpr big_rational operator-(const big_rational& a, const big_rational& b) { return a + (-b); }
    friend constexpr big_rational operator*(const big_rational& a, const big_rational& b) {
        return {a.Num * b.Num, a.Den * b.Den};
    }
    // Pre: b ≠ 0.
    friend constexpr big_rational operator/(const big_rational& a, const big_rational& b) {
        return {a.Num * b.Den, a.Den * b.Num};
    }

    [[nodiscard]] constexpr bool is_integer() const noexcept { return Den == big_int{1}; }

    // With an integer: exact, and unambiguous (an int converts to both).
    template <std::integral T>
    friend constexpr bool operator==(const big_rational& a, T b) {
        return a.Den == big_int{1} && a.Num == big_int{b};
    }
    template <std::integral T>
    friend constexpr std::strong_ordering operator<=>(const big_rational& a, T b) {
        return a <=> big_rational{b};
    }
    template <std::integral T>
    friend constexpr big_rational operator+(const big_rational& a, T b) {
        return a + big_rational{b};
    }
    template <std::integral T>
    friend constexpr big_rational operator+(T a, const big_rational& b) {
        return big_rational{a} + b;
    }
    template <std::integral T>
    friend constexpr big_rational operator-(const big_rational& a, T b) {
        return a - big_rational{b};
    }
    template <std::integral T>
    friend constexpr big_rational operator-(T a, const big_rational& b) {
        return big_rational{a} - b;
    }
    template <std::integral T>
    friend constexpr big_rational operator*(const big_rational& a, T b) {
        return a * big_rational{b};
    }
    template <std::integral T>
    friend constexpr big_rational operator*(T a, const big_rational& b) {
        return big_rational{a} * b;
    }
    template <std::integral T>
    friend constexpr big_rational operator/(const big_rational& a, T b) {
        return a / big_rational{b};
    }
    template <std::integral T>
    friend constexpr big_rational operator/(T a, const big_rational& b) {
        return big_rational{a} / b;
    }

    // The nearest double, by way of the top 64 bits of each part.
    constexpr explicit operator double() const {
        // |v| as top·2^sh, top below 2^64 (with a sticky bit for the bits
        // below): the quotient of the tops is then finite, and the shift is
        // applied once after it, so a numerator and a denominator both past
        // 2^1024 still give their finite ratio.
        auto split = [](const big_int& v, int& sh) {
            const big::mag m  = v.magnitude();
            const int      bw = big::bit_width(m);
            sh                = 0;
            if (bw <= 64)
                return static_cast<double>(m.empty() ? umax{0} : m[0]);
            sh                    = bw - 64;
            const std::size_t w   = static_cast<std::size_t>(sh / 64);
            const int         b   = sh % 64;
            umax              top = m[w] >> b;
            if (b != 0 && w + 1 < m.size())
                top |= m[w + 1] << (64 - b);
            const bool sticky =
                (b != 0 && (m[w] << (64 - b)) != 0) ||
                std::any_of(m.begin(), m.begin() + static_cast<std::ptrdiff_t>(w), [](umax x) { return x != 0; });
            return static_cast<double>(top | (sticky ? 1u : 0u));
        };
        int          sn = 0, sd = 0;
        const double n = split(Num, sn), d = split(Den, sd);
        const double q = ldexp(n / d, sn - sd);
        return Num.negative() ? -q : q;
    }
    constexpr explicit operator float() const { return static_cast<float>(static_cast<double>(*this)); }

    // Mixed with the 64-bit rational: each converts to the other, so these
    // exact overloads keep the operators unambiguous.
    friend constexpr bool operator==(const big_rational& a, const rational& b) { return a == big_rational{b}; }
    friend constexpr std::strong_ordering operator<=>(const big_rational& a, const rational& b) {
        return a <=> big_rational{b};
    }
    friend constexpr big_rational operator+(const big_rational& a, const rational& b) { return a + big_rational{b}; }
    friend constexpr big_rational operator+(const rational& a, const big_rational& b) { return big_rational{a} + b; }
    friend constexpr big_rational operator-(const big_rational& a, const rational& b) { return a - big_rational{b}; }
    friend constexpr big_rational operator-(const rational& a, const big_rational& b) { return big_rational{a} - b; }
    friend constexpr big_rational operator*(const big_rational& a, const rational& b) { return a * big_rational{b}; }
    friend constexpr big_rational operator*(const rational& a, const big_rational& b) { return big_rational{a} * b; }
    friend constexpr big_rational operator/(const big_rational& a, const rational& b) { return a / big_rational{b}; }
    friend constexpr big_rational operator/(const rational& a, const big_rational& b) { return big_rational{a} / b; }
};
constexpr big_rational abs(big_rational a) {
    a.Num = abs(a.Num);
    return a;
}

// gcd of two fractions: gcd of numerators over lcm of denominators.
constexpr big_rational gcd(const big_rational& a, const big_rational& b) {
    const big_int dg = gcd(a.Den, b.Den);
    return {gcd(a.Num, b.Num), a.Den / dg * b.Den};
}
} // namespace beman::inside::detail


//---------------------------------------------------------------------------
// The grid-number vocabulary. A grid's limits and notch (the NTTP substrate
// of `interval` and `grid`) are big_rationals, of any size: C++26 static
// reflection (std::define_static_array) interns their limbs in static
// storage, so equal values stay the same template argument.
//   grid_rational          the type of a grid's limits and notch
//   grid_int               exact integers for grid computations (slot counts,
//                          value indices): wide enough for every grid
//   wide_numerator(r) /    a grid number's signed numerator and positive
//   wide_denominator(r)    denominator as grid_int (also for a rational)
//   grid_divides_evenly    a / n is an integer (true for n == 0)
//   grid_same_lattice      (a − b) / n is an integer (true for n == 0)
//   fits_rational(r) /     whether, and as which, 64-bit rational a grid
//   to_rational(r)         number serves the 64-bit fast paths
// + − × / and gcd are big_rational's own.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// (a − b) / n is an integer (true for n == 0), for 64-bit rationals. A
// difference past the rational range is tested in four limbs:
// (a − b)/n = (a_n·b_d − b_n·a_d)·n_d / (a_d·b_d·n_n), whose products of
// three 64-bit magnitudes fit.
constexpr bool same_lattice64(const rational& a, const rational& b, const rational& n) {
    if (n == 0)
        return true;
    if (const auto d = try_sub(a, b)) [[likely]]
        return divides_evenly(*d, n);
    using W        = wide_sint<4>;
    auto    num_of = [](const rational& r) { return r.Denominator < 0 ? -W{r.Numerator} : W{r.Numerator}; };
    auto    den_of = [](const rational& r) { return W{abs_den(r.Denominator)}; };
    const W num    = (num_of(a) * den_of(b) - num_of(b) * den_of(a)) * den_of(n);
    const W den    = den_of(a) * den_of(b) * num_of(n);
    return num % den == W{0};
}

using grid_rational = big_rational;
using grid_int      = big_int;

constexpr grid_int wide_numerator(const big_rational& r) { return r.Num; }
constexpr grid_int wide_denominator(const big_rational& r) { return r.Den; }

// A grid number as a transient fraction, for tests that run at runtime too.
constexpr big::fraction fraction_of(const big_rational& r) {
    return {r.Num.magnitude(), r.Den.magnitude(), r.Num.negative()};
}

// (a − b) / n is an integer (true for n == 0): b and a share n's lattice.
// (No big value is formed, so the test runs at runtime too.)
constexpr bool grid_same_lattice(const big_rational& a, const big_rational& b, const big_rational& n) {
    if (n == 0)
        return true;
    if (a.fits_rational() && b.fits_rational() && n.fits_rational())
        return same_lattice64(a, b, n);
    return big::same_lattice(fraction_of(a), fraction_of(b), fraction_of(n));
}

// (Convention, as divides_evenly: everything divides 0 evenly.)
constexpr bool grid_divides_evenly(const big_rational& a, const big_rational& n) {
    return grid_same_lattice(a, big_rational{0}, n);
}

// The 64-bit rational of a grid number, for the 64-bit fast paths.
constexpr bool     fits_rational(const big_rational& r) { return r.fits_rational(); }
constexpr rational to_rational(const big_rational& r) { return r; }

constexpr grid_int wide_numerator(const rational& r) noexcept {
    const grid_int n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
}
constexpr grid_int wide_denominator(const rational& r) noexcept { return grid_int{abs_den(r.Denominator)}; }

// Value bits a signed integer needs to hold every value in [lo, hi].
constexpr int signed_value_bits(const grid_int& lo, const grid_int& hi) noexcept {
    auto      mag = [](const grid_int& v) { return bit_width_of(v.negative() ? -(v + grid_int{1}) : v); };
    const int a = mag(lo), b = mag(hi);
    return a > b ? a : b;
}

// r rounded to an integer by M (half away from zero for nearest).
template <round_mode M>
constexpr grid_rational grid_to_int(const grid_rational& r) {
    const grid_int n = wide_numerator(r), d = wide_denominator(r);
    grid_int       q   = n / d;
    const grid_int rem = n - q * d;
    const bool     neg = n.negative();
    const bool     odd = !(q / grid_int{2} * grid_int{2} == q);
    if (rounds_away(M, neg, classify_remainder(M, neg ? -rem : rem, d), odd))
        q = neg ? q - grid_int{1} : q + grid_int{1};
    return grid_rational{q};
}

//---------------------------------------------------------------------------
// parse_grid_literal — the _g literal: the decimal forms of the literal
// grammar (scan_number: digits with ' separators, an optional point, an
// optional e±n exponent), taken exactly, of any size.
//---------------------------------------------------------------------------
template <char... Chars>
consteval grid_rational parse_grid_literal() {
    if constexpr (octal_literal<Chars...>())
        constexpr_error<"_g literal: a leading 0 makes an octal literal; write the number without it">();
    constexpr char text[] = {Chars...};
    const grid_int ten{10};
    // The value is m·10^e10. Zero digits are held back (`zeros`) and only
    // multiplied in before a later nonzero digit, so trailing zeros
    // (1.000…) never grow m.
    grid_int             m{0};
    int                  e10 = 0, zeros = 0;
    const scanned_number sc = scan_number(text, text + sizeof(text), [&](int d, bool in_frac, int) {
        if (d == 0)
            ++zeros;
        else {
            for (; zeros > 0; --zeros)
                m = m * ten;
            m = m * ten + grid_int{d};
        }
        e10 -= in_frac;
        return true;
    });
    if (sc.Fail == parse_fail::empty)
        constexpr_error<"_g literal: expected decimal digits">();
    if (sc.Fail != parse_fail::none || sc.Base != 10)
        constexpr_error<"_g literal: decimal digits, a point and e±n only">();
    if (sc.Exp > 10000)
        constexpr_error<"_g literal: exponent too large">();
    if (m == grid_int{0})
        return big_rational{0};
    e10 += zeros + (sc.ExpNeg ? -sc.Exp : sc.Exp);
    const grid_int p = pow_by_squaring(
        ten,
        static_cast<unsigned>(e10 < 0 ? -e10 : e10),
        [](const grid_int& a, const grid_int& b) { return a * b; },
        grid_int{1});
    return e10 < 0 ? big_rational{m, p} : big_rational{m * p, grid_int{1}};
}
} // namespace beman::inside::detail

namespace beman::inside {
// A grid number: `inside<{0, 1267650600228229401496703205376_g}>`, or an
// exact decimal notch `{{0, 1}, 1e-30_g}`. Any size.
template <char... Chars>
consteval detail::grid_rational operator""_g() {
    return detail::parse_grid_literal<Chars...>();
}
} // namespace beman::inside


namespace beman::inside {
//---------------------------------------------------------------------------
// interval — structural NTTP type (public members only) with inclusive Lower
// and Upper bounds, and includes / excludes / overlaps. Like `grid`, its
// + - * / compute result intervals at compile time; division returns
// errc::division_by_zero when the divisor straddles zero (grid::operator/
// re-runs on the two zero-free halves and joins them).
//---------------------------------------------------------------------------
struct interval {
    detail::grid_rational Lower;
    detail::grid_rational Upper;

    interval() = default;

    constexpr interval(detail::grid_rational lower, detail::grid_rational upper) : Lower{lower}, Upper{upper} {}
    constexpr interval(detail::plain_number auto lower, detail::plain_number auto upper)
        : Lower{lower}, Upper{upper} {}

    template <auto I>
    static constexpr bool validate() {
        static_assert(I.Lower <= I.Upper);
        return true;
    }

    [[nodiscard]] constexpr bool     operator==(const interval& rhs) const = default;
    [[nodiscard]] constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    [[nodiscard]] constexpr bool divides_evenly(const detail::grid_rational& notch) const {
        return detail::grid_divides_evenly(Upper - Lower, notch);
    }

    // The span in units of `notch` (the slot count of a grid).
    [[nodiscard]] constexpr std::expected<detail::grid_rational, errc>
    operator/(const detail::grid_rational& notch) const {
        if (notch == 0)
            return std::unexpected{errc::division_by_zero};
        return (Upper - Lower) / notch;
    }
};

// Containment / disjointness — free functions over the public endpoints
// (siblings of the binary interval operators below).
[[nodiscard]] constexpr bool includes(const interval& iv, const interval& rhs) noexcept {
    return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper;
}

[[nodiscard]] constexpr bool includes(const interval& iv, const detail::rational& r) noexcept {
    return iv.Lower <= r && r <= iv.Upper;
}

[[nodiscard]] constexpr bool includes(const interval& iv, const detail::grid_rational& r) noexcept {
    return iv.Lower <= r && r <= iv.Upper;
}

// A floating-point value is compared exactly: against 64-bit limits through
// the rational comparison, against big limits as a transient fraction (a big
// grid number past 64 bits is never formed at runtime).
template <detail::plain_number A>
[[nodiscard]] constexpr bool includes(const interval& iv, A a) noexcept {
    if constexpr (std::floating_point<A>) {
        if (!detail::is_finite(static_cast<double>(a)))
            return false;
        if (detail::fits_rational(iv.Lower) && detail::fits_rational(iv.Upper)) {
            if (detail::past_64_bits(a))
                return false;
            return detail::to_rational(iv.Lower) <= a && a <= detail::to_rational(iv.Upper);
        }
        const auto f = detail::big::fraction_of_float(a);
        return detail::big::compare(detail::fraction_of(iv.Lower), f) <= 0 &&
               detail::big::compare(f, detail::fraction_of(iv.Upper)) <= 0;
    } else
        return includes(iv, detail::grid_rational{a});
}

// `excludes` means *strictly disjoint* — the intervals share no value.
// `!includes()` is weaker: it only rules out total containment, so two
// overlapping intervals are `!includes` AND `!excludes`.
[[nodiscard]] constexpr bool excludes(const interval& iv, const interval& rhs) noexcept {
    return rhs.Upper < iv.Lower || iv.Upper < rhs.Lower;
}

// The `includes(rhs, iv)` clause catches rhs wholly containing iv (where
// neither rhs endpoint lands in iv, so the other checks would miss it).
[[nodiscard]] constexpr bool overlaps(const interval& iv, const interval& rhs) noexcept {
    return includes(rhs, iv) || includes(iv, rhs.Lower) || includes(iv, rhs.Upper);
}

// The min/max hull of four endpoint combinations — the result interval of an
// interval product or quotient (interval arithmetic's four-corner rule).
namespace detail {
[[nodiscard]] constexpr interval
corner_hull(grid_rational a, grid_rational b, grid_rational c, grid_rational d) noexcept {
    const grid_rational lo1 = a < b ? a : b, hi1 = a < b ? b : a;
    const grid_rational lo2 = c < d ? c : d, hi2 = c < d ? d : c;
    return interval{lo1 < lo2 ? lo1 : lo2, hi1 < hi2 ? hi2 : hi1};
}

// The smallest interval holding both.
[[nodiscard]] constexpr interval interval_hull(const interval& a, const interval& b) noexcept {
    return interval{a.Lower < b.Lower ? a.Lower : b.Lower, a.Upper < b.Upper ? b.Upper : a.Upper};
}
} // namespace detail

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr interval operator+(const interval& lhs, const interval& rhs) {
    return interval{lhs.Lower + rhs.Lower, lhs.Upper + rhs.Upper};
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr interval operator-(const interval& lhs, const interval& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr interval operator*(const interval& lhs, const interval& rhs) {
    return detail::corner_hull(
        lhs.Lower * rhs.Lower, lhs.Lower * rhs.Upper, lhs.Upper * rhs.Lower, lhs.Upper * rhs.Upper);
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator/(const interval& lhs, const interval& rhs) {
    if (includes(rhs, 0))
        return std::unexpected{errc::division_by_zero};

    return detail::corner_hull(
        lhs.Lower / rhs.Lower, lhs.Lower / rhs.Upper, lhs.Upper / rhs.Lower, lhs.Upper / rhs.Upper);
}

//---------------------------------------------------------------------------
// operator<=>
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering {
    if (lhs.Upper < rhs.Lower)
        return std::partial_ordering::less;

    if (lhs.Lower > rhs.Upper)
        return std::partial_ordering::greater;

    if (lhs.Lower == rhs.Lower && lhs.Upper == rhs.Upper)
        return std::partial_ordering::equivalent;

    return std::partial_ordering::unordered;
}

} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/diag_text.hpp
// ======================================================================




//---------------------------------------------------------------------------
// Compile-time diagnostics with the values in them. A static_assert
// message can be any constant object with size() and data()
// (P2741), so an error can say "notch 3, Upper − Lower 10". diag_text is
// that object: text and grid numbers appended with <<, in a fixed buffer.
//
//   static_assert(cond, diag_text{} << "grid: the notch must …" << " (notch " << n << ")");
//---------------------------------------------------------------------------
namespace beman::inside::detail {
struct diag_text {
    static constexpr std::size_t capacity = 480;
    char                         Buf[capacity]{};
    std::size_t                  Size{0};

    [[nodiscard]] constexpr std::size_t size() const noexcept { return Size; }
    [[nodiscard]] constexpr const char* data() const noexcept { return Buf; }

    constexpr void push(char c) noexcept {
        if (Size + 1 < capacity)
            Buf[Size++] = c;
    }
    constexpr diag_text& operator<<(std::string_view s) noexcept {
        for (char c : s)
            push(c);
        return *this;
    }
    // A grid number: an integer, or N/D.
    constexpr diag_text& operator<<(const grid_rational& r) {
        append(wide_numerator(r));
        if (const grid_int d = wide_denominator(r); !(d == grid_int{1})) {
            push('/');
            append(d);
        }
        return *this;
    }

  private:
    constexpr void append(grid_int v) {
        if (v < grid_int{0}) {
            push('-');
            v = -v;
        }
        char        digits[160]{};
        std::size_t n = 0;
        do {
            const grid_int q = v / grid_int{10};
            digits[n++]      = static_cast<char>('0' + static_cast<int>(v - q * grid_int{10}));
            v                = q;
        } while (!(v == grid_int{0}) && n < sizeof digits);
        while (n > 0)
            push(digits[--n]);
    }
};
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/policy_flag.hpp
// ======================================================================




namespace beman::inside {
//---------------------------------------------------------------------------
// policy_flag — the policy bits of inside<G, P> (none, ignore_zero,
// ignore_range, snap and the rounding modes, clamp, wrap, checked, unsafe),
// has_flag / is_checked, the rounding precedence (rounding_of), the hook
// factories on_clamp / on_wrap / on_error / on_overflow with their
// is_*_action traits, and the flags each hook implies.
//---------------------------------------------------------------------------
using policy_flag = unsigned long long;

// Check model: compile-time checks always run. When success can't be proven
// statically, compilation fails unless the matching ignore flag is set; else a
// runtime check is inserted that throws (or reports through an errc& parameter).
// Binary operations OR the flags of both operands.
inline constexpr policy_flag none{0ull};
inline constexpr policy_flag ignore_zero{1ull << 1};
inline constexpr policy_flag ignore_range{1ull << 2};
// `snap` — permission: an off-notch value is rounded to fit the grid instead
// of rejected. Without it, an off-notch value is a compile/runtime error and
// div/mod fall through to exact-rational results. Every round_* mode carries
// it; on its own it rounds toward zero.
inline constexpr policy_flag snap{1ull << 4};
// `round_toward_zero` — the snap bit alone: truncate toward zero (IEEE 754
// roundTowardZero). Last in dispatch, after every explicit mode.
inline constexpr policy_flag round_toward_zero{snap};
inline constexpr policy_flag round_nearest{(1ull << 5) | snap};
// Rounding modes each pick a unique bit and OR in `snap`. Conceptually
// exclusive; combining two is allowed but dispatch (storage.hpp) picks the
// first match: floor → ceil → half_even → nearest → toward zero (rounding_of).
inline constexpr policy_flag round_floor{(1ull << 6) | snap};
inline constexpr policy_flag round_ceil{(1ull << 7) | snap};
inline constexpr policy_flag round_half_even{(1ull << 8) | snap};

// runtime checking — on unless the policy carries `unsafe` (see is_checked).
// Spelling `checked` re-enables the checks alongside `unsafe`.
inline constexpr policy_flag checked{1ull << 34}; // runtime range/notch/overflow checks

// unary — mutually exclusive
inline constexpr policy_flag clamp{1ull << 32}; // saturate to boundary
inline constexpr policy_flag wrap{1ull << 33};  // modular arithmetic

// No flag picks the raw: storage follows the grid alone (grid.hpp
// raw_for, docs/storage.md).

// opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
// out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
// check). Includes `snap` so notch-incompatible assigns compile.
namespace detail {
inline constexpr policy_flag unsafe_marker{1ull << 36};
// Marks a cursor (`cursor<T, Step>`): default construction starts at Lower,
// and `end(t)` names its past-the-end value. Results never carry it.
inline constexpr policy_flag cursor_marker{1ull << 50};
} // namespace detail
inline constexpr policy_flag unsafe{detail::unsafe_marker | ignore_range | snap | ignore_zero};

//---------------------------------------------------------------------------
// Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
// of `flag` is present in `set` — reads better than the raw `(set & flag) ==
// flag` and is correct for composite flags (e.g. `round_nearest` carries
// `snap`), where a bare `set & flag`
// truthy test would misfire. `has_any_flag` tests for any overlap.
//---------------------------------------------------------------------------
[[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept { return (set & flag) == flag; }

[[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept {
    return (set & flags) != none;
}

// Runtime checks run unless the policy opts out with `unsafe`; an explicit
// `checked` wins over `unsafe`. So `inside<G, round_nearest>` and
// `inside<G, clamp>` are checked, exactly like the default `inside<G>`.
[[nodiscard]] constexpr bool is_checked(policy_flag set) noexcept {
    return has_flag(set, checked) || !has_flag(set, detail::unsafe_marker);
}

// Whether an out-of-range value does anything under a flag set: a clamp or
// wrap bit stores it, a checked range reports it. False under `unsafe`.
[[nodiscard]] constexpr bool range_handled(policy_flag set) noexcept {
    return has_any_flag(set, clamp | wrap) || (is_checked(set) && !has_flag(set, ignore_range));
}

namespace detail {
// The rounding mode a flag set selects — the ONE precedence every rounding
// path uses (integer and rational storage, division, math stores).
// An explicit directional or half-even mode beats round_nearest; `snap` alone, or no
// rounding flag at all, truncates toward zero. Ties of `nearest` go half
// away from zero. round_mode and the shared decision: detail/rounding.hpp.

[[nodiscard]] constexpr round_mode rounding_of(policy_flag f) noexcept {
    if (has_flag(f, round_floor))
        return round_mode::floor;
    if (has_flag(f, round_ceil))
        return round_mode::ceil;
    if (has_flag(f, round_half_even))
        return round_mode::half_even;
    if (has_flag(f, round_nearest))
        return round_mode::nearest;
    return round_mode::trunc;
}
} // namespace detail

//---------------------------------------------------------------------------
// no_action — zero-overhead default for overflow callbacks
//---------------------------------------------------------------------------
struct no_action {};

//---------------------------------------------------------------------------
// tagged actions — opt-in callbacks for each failure path.
// The lambda receives the inside by mutable reference as its first argument,
// so the handler can override the value the policy was about to store.
//---------------------------------------------------------------------------
template <typename F>
struct on_clamp_t {
    [[no_unique_address]] F Fn;
};
template <typename F>
struct on_wrap_t {
    [[no_unique_address]] F Fn;
};
template <typename F>
struct on_error_t {
    [[no_unique_address]] F Fn;
};
template <typename F>
struct on_overflow_t {
    [[no_unique_address]] F Fn;
};

//---------------------------------------------------------------------------
// CTAD-style factories — drop the on_overflow_t{lambda} brace-init.
//---------------------------------------------------------------------------
template <typename F>
[[nodiscard]] constexpr auto on_clamp(F&& fn) {
    return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}
template <typename F>
[[nodiscard]] constexpr auto on_wrap(F&& fn) {
    return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}
template <typename F>
[[nodiscard]] constexpr auto on_error(F&& fn) {
    return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}
template <typename F>
[[nodiscard]] constexpr auto on_overflow(F&& fn) {
    return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}

namespace detail {
// Action detection: the `is_*_action` trait is the primary detector (the
// pack helpers below take it as a template-template parameter); the concept
// derives from it and strips cvref so the ref form matches the value form.
template <template <typename> class Action, typename T>
inline constexpr bool is_action = false;
template <template <typename> class Action, typename F>
inline constexpr bool is_action<Action, Action<F>> = true;

template <typename T>
using is_clamp_action = std::bool_constant<is_action<on_clamp_t, T>>;
template <typename T>
using is_wrap_action = std::bool_constant<is_action<on_wrap_t, T>>;
template <typename T>
using is_error_action = std::bool_constant<is_action<on_error_t, T>>;
template <typename T>
using is_overflow_action = std::bool_constant<is_action<on_overflow_t, T>>;

template <typename T>
concept clamp_action = is_clamp_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept wrap_action = is_wrap_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept error_action = is_error_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept overflow_action = is_overflow_action<std::remove_cvref_t<T>>::value;

//---------------------------------------------------------------------------
// implied_flags<A> — single source of truth for "this action requires these
// policy bits". Used by inside::on_* and the action-first free-fn overloads.
//---------------------------------------------------------------------------
template <typename T>
inline constexpr policy_flag implied_flags = none;
template <typename F>
inline constexpr policy_flag implied_flags<on_clamp_t<F>> = clamp;
template <typename F>
inline constexpr policy_flag implied_flags<on_wrap_t<F>> = wrap;
template <typename F>
inline constexpr policy_flag implied_flags<on_error_t<F>> = checked;
template <typename F>
inline constexpr policy_flag implied_flags<on_overflow_t<F>> = checked;

//---------------------------------------------------------------------------
// Pack helpers — let policy_ref/assignment/arithmetic accept Actions... packs.
// The `is_*_action` traits are passed as template-template parameters.
//---------------------------------------------------------------------------

// True if any element of the pack matches the trait.
template <template <typename> class Trait, typename... As>
inline constexpr bool has_action = (Trait<std::remove_cvref_t<As>>::value || ... || false);

// How many pack elements match.
template <template <typename> class Trait, typename... As>
inline constexpr unsigned count_action_matches = (0u + ... + (Trait<std::remove_cvref_t<As>>::value ? 1u : 0u));

// OR of implied_flags<plain_t<A>> across the pack.
template <typename... As>
inline constexpr policy_flag merged_implied_flags = (none | ... | implied_flags<std::remove_cvref_t<As>>);

// pick_action<Trait>(actions...) returns a reference to the first pack element
// matching the trait, or to `fallback_action` if none does. Conflict
// diagnostics elsewhere ensure at most one match.
inline no_action fallback_action;

template <template <typename> class Trait, typename... As>
constexpr auto& pick_action(As&... as) {
    constexpr std::size_t i = [] { // the first match, or sizeof...(As)
        std::size_t k = 0;
        (void)((Trait<std::remove_cvref_t<As>>::value || (++k, false)) || ...);
        return k;
    }();
    if constexpr (i < sizeof...(As))
        return as...[i];
    else
        return fallback_action;
}

// Same, but operating on a tuple.
template <template <typename> class Trait, typename Tuple>
constexpr auto& pick_action_in(Tuple& t) {
    auto& [... as] = t;
    return pick_action<Trait>(as...);
}

} // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/int_for_bits.hpp
// ======================================================================




//---------------------------------------------------------------------------
// int_for_bits — one rule for every integer the library stores or computes
// with: the smallest builtin integer of at least Bits value bits, else a
// wide_int of enough 64-bit limbs. Raws and intermediates both size by it, so
// a grid's bit count fully decides its integer types.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
constexpr std::size_t limbs_for_bits(int bits) noexcept { return static_cast<std::size_t>((bits + 63) / 64); }

template <std::size_t I, class... Ts>
using nth_type_t = Ts...[I];

// Bits counts value bits; a signed type spends one more on the sign.
template <int Bits, bool Signed>
struct int_for_bits {
    static constexpr int         total = Bits + (Signed ? 1 : 0);
    static constexpr std::size_t i     = total <= 8 ? 0 : total <= 16 ? 1 : total <= 32 ? 2 : total <= 64 ? 3 : 4;
    using wide                         = wide_int<limbs_for_bits(total), Signed>;
    using type = std::conditional_t<Signed,
                                    nth_type_t<i, std::int8_t, std::int16_t, std::int32_t, std::int64_t, wide>,
                                    nth_type_t<i, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, wide>>;
};

template <int Bits, bool Signed>
using int_for_bits_t = typename int_for_bits<Bits, Signed>::type;
} // namespace beman::inside::detail



namespace beman::inside {
//---------------------------------------------------------------------------
// grid — structural NTTP type (public members only): an interval split into
// notch-sized steps (the notch must divide the interval; Notch == 0 allows
// every rational in it), optionally with a MaxDenominator bound. Its + - * /
// are the engine of compile-time result-grid inference: every inside operator
// computes its result grid here, so the result holds every reachable value.
// Also here: bounds (closed, half_open, open), grid::try_make for runtime
// checks, max_denominator, and raw_for (the storage a grid deduces).
//---------------------------------------------------------------------------
// A grid corner: a number (int / float / rational / grid number), or
// anything that converts to the 64-bit rational, such as an inside (the grid
// number takes that one conversion).
template <typename T>
concept grid_number_like = std::convertible_to<T, detail::grid_rational>;

// Which limits of a grid are values. A grid is always stored closed; an open
// limit is a spelling, moved in by one notch at construction, so
// {0, 360, bounds::half_open} is the grid {0, 359} — the same type.
enum class bounds : unsigned char {
    closed,    // [Lower, Upper] — the default
    half_open, // [Lower, Upper) — the last value is Upper − Notch
    open,      // (Lower, Upper) — Lower + Notch .. Upper − Notch
};

// The largest maximum denominator a continuous grid can carry: past it a
// bound would size fractions without limit along an expression chain, so it
// is dropped (the grid is unbounded).
#ifndef BEMAN_INSIDE_MAX_FRACTION_BITS
    #define BEMAN_INSIDE_MAX_FRACTION_BITS 1024
#endif

// A continuous grid whose values have denominators of at most Value:
// inside<{{0, 1}, max_denominator{1000}}>. The bound makes exact arithmetic
// predictable — the result type knows how large every fraction can get.
struct max_denominator {
    detail::grid_rational Value;
    constexpr explicit max_denominator(grid_number_like auto v) : Value{detail::grid_rational{v}} {}
    constexpr explicit max_denominator(const detail::grid_rational& v) : Value{v} {}
};

struct grid {
    interval              Interval;
    detail::grid_rational Notch;
    // A continuous grid's bound on its values' denominators; 0 is unbounded
    // (any rational in range). Always 0 on a notched or point grid, whose
    // lattice already bounds them — so a value set has one grid.
    detail::grid_rational MaxDenominator{0};

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `inside` / `just<>` (via its implicit `operator rational()`), so an inside can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `inside`.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper, grid_number_like auto notch)
        : grid{interval{to_number(lower), to_number(upper)}, to_number(notch)} {}
    // Two limits: the notch is derived — gcd(1, Lower, Upper), the coarsest
    // step 1/k that keeps every integer and both limits on the lattice. Integer
    // limits give 1; {0.5, 10} gives 1/2; {just<-6, 5>, just<3, 5>} gives 1/5.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper)
        : grid{interval{to_number(lower), to_number(upper)}, derive_notch(lower, upper)} {}
    constexpr grid(grid_number_like auto lower)
        : grid{interval{to_number(lower), to_number(lower)}, detail::grid_rational{0}} {}
    constexpr grid(interval val, detail::grid_rational notch) : Interval{val}, Notch{notch} {}
    // The same with open limits: {{0, 64}, per<16>, bounds::half_open} is
    // {{0, 63.9375}, per<16>}, and {0, 360, bounds::half_open} — notch derived
    // from the limits as written — is {0, 359}.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper, grid_number_like auto notch, bounds b)
        : grid{interval{to_number(lower), to_number(upper)}, to_number(notch), b} {}
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper, bounds b)
        : grid{interval{to_number(lower), to_number(upper)}, derive_notch(lower, upper), b} {}
    constexpr grid(interval val, detail::grid_rational notch, bounds b)
        : Interval{close_bounds(val, notch, b)}, Notch{notch} {}
    // A continuous grid with bounded denominators: {{0, 1}, max_denominator{1000}}.
    constexpr grid(interval val, max_denominator d)
        : Interval{val}, Notch{0}, MaxDenominator{check_bound(val, d.Value)} {}

  private:
    // A bound is a positive integer at least the limits' denominators (each
    // limit is a value).
    static constexpr detail::grid_rational check_bound(const interval& iv, const detail::grid_rational& d) {
        using detail::wide_denominator;
        if (!(d > 0) || wide_denominator(d) != detail::grid_int{1})
            detail::constexpr_error<"max_denominator: the bound must be a positive integer">();
        if (detail::wide_numerator(d) < wide_denominator(iv.Lower) ||
            detail::wide_numerator(d) < wide_denominator(iv.Upper))
            detail::constexpr_error<"max_denominator: the bound is below a limit's denominator">();
        return d;
    }

    // Moves iv's open limits in by one notch; false when the notch is 0 (no
    // next point) or no grid point is left between them.
    static constexpr bool move_in(interval& iv, const detail::grid_rational& notch, bounds b) {
        if (b == bounds::closed)
            return true;
        if (notch == 0)
            return false;
        if (b == bounds::open)
            iv.Lower = iv.Lower + notch;
        iv.Upper = iv.Upper - notch;
        return !(iv.Lower > iv.Upper);
    }

    static constexpr interval close_bounds(interval iv, const detail::grid_rational& notch, bounds b) {
        if (b != bounds::closed && notch == 0)
            detail::constexpr_error<"bounds: an open limit needs a notch - a continuous grid has no next point">();
        if (!move_in(iv, notch, b))
            detail::constexpr_error<"bounds: no grid point lies between the limits">();
        return iv;
    }

    template <grid_number_like T>
    static constexpr detail::grid_rational to_number(const T& v) {
        return detail::grid_rational{v};
    }

    // The notch gcd(1, lower, upper).
    static constexpr detail::grid_rational derive_notch(grid_number_like auto lower, grid_number_like auto upper) {
        check_short_binary(lower);
        check_short_binary(upper);
        return gcd(gcd(detail::grid_rational{1}, to_number(lower)), to_number(upper));
    }

    // A floating-point limit is taken as its exact binary value, so 0.1 would
    // derive a 2^-55 notch. Past 1/1024 the literal almost surely meant a
    // decimal: reject it at compile time and point to the exact spellings.
    template <grid_number_like T>
    static constexpr void check_short_binary([[maybe_unused]] T v) {
        if constexpr (std::floating_point<T>)
            if (std::is_constant_evaluated() &&
                detail::grid_int{1024} < detail::wide_denominator(detail::grid_rational{v}))
                detail::constexpr_error<
                    // Clang prints only the first ~34 characters: lead with the fix.
                    "float limit: use _r literal (0.1_r) or give a notch {{lo, hi}, per<D>}; "
                    "grid{lo, hi} derives a notch from a floating-point limit only down to "
                    "1/1024 (0.1 is not 1/10 in binary)">();
    }

  public:
    template <auto G>
    static constexpr bool validate() {
        interval::validate<G.Interval>();
        // Decoding is Lower + raw·Notch: a negative notch would count downward.
        static_assert(G.Notch >= 0,
                      detail::diag_text{} << "grid: the notch must be non-negative (notch " << G.Notch << ")");
        // The values are Lower, Lower + Notch, …, Upper: the notch steps from
        // Lower to Upper. Lower need not be a multiple of the notch
        // ({{0.5, 10.5}, 1} holds 0.5, 1.5, …).
        static_assert(G.Interval.divides_evenly(G.Notch),
                      detail::diag_text{} << "grid: the notch must divide Upper − Lower evenly (notch " << G.Notch
                                          << ", Upper − Lower " << G.Interval.Upper - G.Interval.Lower << ")");
        static_assert(G.MaxDenominator == 0 || (G.Notch == 0 && G.Interval.Lower != G.Interval.Upper),
                      "grid: only a continuous grid carries a max_denominator");

        return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be an inside<G,P> template argument.
    // An open limit (`bounds`) moves in by one notch first, as in the ctors.
    [[nodiscard]] static constexpr std::expected<grid, errc>
    try_make(interval iv, detail::grid_rational notch, bounds b = bounds::closed) {
        if (iv.Lower > iv.Upper || notch < 0 || !move_in(iv, notch, b))
            return std::unexpected{errc::domain_error};
        if (!iv.divides_evenly(notch))
            return std::unexpected{errc::rounding_error};
        return grid{iv, notch};
    }

    // Exact slot count (Upper − Lower)/Notch, however large: with Upper = a/b,
    // Lower = c/d and Notch = e/f it is (a·d − c·b)·f / (b·d·e), exact on a
    // valid grid. 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_int slot_count() const noexcept {
        using detail::wide_numerator, detail::wide_denominator;
        if (Notch == 0)
            return detail::grid_int{0};
        const auto&            U = Interval.Upper;
        const auto&            L = Interval.Lower;
        const detail::grid_int num =
            (wide_numerator(U) * wide_denominator(L) - wide_numerator(L) * wide_denominator(U)) *
            wide_denominator(Notch);
        return num / (wide_denominator(U) * wide_denominator(L) * wide_numerator(Notch));
    }

    // Bits needed to hold every slot index 0..slot_count().
    [[nodiscard]] constexpr int slot_bits() const noexcept { return bit_width_of(slot_count()); }

    // True when the slot count fits umax (a builtin index). False ⇒ the grid is
    // still valid and stores a wide index.
    [[nodiscard]] constexpr bool max_index_representable() const {
        return !(detail::grid_int{std::numeric_limits<umax>::max()} < slot_count());
    }

    // Index-storage slot count (0 on overflow: such grids store a wide index).
    [[nodiscard]] constexpr umax max_index() const {
        return max_index_representable() ? static_cast<umax>(slot_count()) : umax{0};
    }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_ins`) regardless of whole-range mapping.
    [[nodiscard]] constexpr bool representable(detail::grid_rational v) const noexcept {
        if (!includes(Interval, v))
            return false;
        if (Notch == 0)
            return MaxDenominator == 0 || !(detail::wide_numerator(MaxDenominator) < detail::wide_denominator(v));
        // (Without forming (v − Lower)/Notch: on a fine notch it may leave
        // the 64-bit rational although v lies on the lattice.)
        return detail::grid_same_lattice(v, Interval.Lower, Notch);
    }
    // A double exactly: one finer than 2^-62 fits no 64-bit rational store;
    // a big grid number past 64 bits exists only at compile time.
    template <std::floating_point T>
    [[nodiscard]] constexpr bool representable(T d) const noexcept {
        if (!includes(Interval, d))
            return false;
        if (detail::fits_rational(Interval.Lower) && detail::fits_rational(Interval.Upper) &&
            detail::fits_rational(Notch)) {
            if constexpr (detail::wide_float<T>)
                if (!detail::is_double_value(d)) { // a long double with bits past the double's
                    bool       exact = false;
                    const auto r     = detail::rational_of_wide_float(d, exact);
                    return exact && representable(detail::grid_rational{r});
                }
            if (!detail::abs_fraction_exact(static_cast<double>(d)))
                return false;
            return representable(detail::grid_rational{d});
        }
        // Big grid numbers: d as a transient fraction, which (unlike a big
        // grid number) also exists at runtime.
        const auto f = detail::big::fraction_of_float(d);
        if (Notch == 0)
            return MaxDenominator == 0 || detail::big::compare(f.Den, MaxDenominator.Num.magnitude()) <= 0;
        return detail::big::same_lattice(f, detail::fraction_of(Interval.Lower), detail::fraction_of(Notch));
    }

    // Whether the lattice passes through 0: Lower is a multiple of the notch
    // (every continuous grid is). An unanchored grid such as {{0.5, 10.5}, 1}
    // has its values offset from the multiples of the notch.
    [[nodiscard]] constexpr bool anchored() const { return detail::grid_divides_evenly(Interval.Lower, Notch); }

    // The largest number every value is an integer multiple of: gcd(Notch,
    // Lower) — the notch on an anchored grid, finer on an unanchored one
    // ({{0.5, 10.5}, 1}: 1/2). 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_rational value_unit() const {
        if (Notch == 0 || anchored())
            return Notch;
        return detail::gcd(Notch, Interval.Lower);
    }

    // operator== be default for structural type
    [[nodiscard]] constexpr bool operator==(const grid& rhs) const = default;
    [[nodiscard]] constexpr grid operator-() const {
        grid g{-Interval, Notch};
        g.MaxDenominator = MaxDenominator;
        return g;
    }

    // (Raw → double decoding lives in `detail::as_double` (detail/storage.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // whole-number grid above 0 has an unsigned raw that IS the value.)
};

namespace detail {
//---------------------------------------------------------------------------
// Denominator bounds — every value of a grid is n/d with d ≤ den_bound and
// |n| ≤ num_bound. A lattice bounds d by its value unit's denominator (the
// lcm of Lower's and the notch's), a point by its own, a continuous grid by
// MaxDenominator; 0 is unbounded. The bounds hold for unreduced fractions
// too, so an exact result can be sized from its operands' bounds.
//---------------------------------------------------------------------------
constexpr bool continuous_grid(const grid& g) { return g.Notch == 0 && g.Interval.Lower != g.Interval.Upper; }

constexpr grid_int den_bound(const grid& g) {
    if (continuous_grid(g))
        return wide_numerator(g.MaxDenominator);
    if (g.Notch == 0)
        return wide_denominator(g.Interval.Lower);
    // The value unit gcd(Lower, Notch) has denominator lcm(den Lower, den Notch).
    const grid_int a = wide_denominator(g.Interval.Lower), b = wide_denominator(g.Notch);
    return a / gcd(a, b) * b;
}

// floor(max(|Lower|, |Upper|) · d): the largest numerator over denominator d.
constexpr grid_int num_bound(const interval& iv, const grid_int& d) {
    auto           mag = [&](const grid_rational& x) { return abs(wide_numerator(x)) * d / wide_denominator(x); };
    const grid_int a = mag(iv.Lower), b = mag(iv.Upper);
    return a < b ? b : a;
}
constexpr grid_int num_bound(const grid& g) { return num_bound(g.Interval, den_bound(g)); }

// The result grid of an exact op whose values are continuous on `iv` with
// denominators ≤ d (0: unbounded). The bound is kept while it can size a
// fraction within BEMAN_INSIDE_MAX_FRACTION_BITS — else the grid is
// unbounded.
constexpr grid bounded_continuous(const interval& iv, grid_int d) {
    grid g{iv, grid_rational{0}};
    if (d == grid_int{0} || iv.Lower == iv.Upper) // a point has its own denominator
        return g;
    // Each limit is a value of the result, but an operator's interval may be
    // wider than the values it reaches: cover the limits' denominators.
    d                = std::max({d, wide_denominator(iv.Lower), wide_denominator(iv.Upper)});
    const grid_int n = num_bound(iv, d);
    if (bit_width_of(n) + bit_width_of(d) > BEMAN_INSIDE_MAX_FRACTION_BITS)
        return g;
    g.MaxDenominator = grid_rational{d};
    return g;
}

// The value index v/Notch of a double v rounded onto G by mode M, the
// integer storage's rule (rounding_of; ties of `nearest` half away from
// zero). G is anchored with a power-of-two notch whose values double holds
// (double_exact), so v/Notch is exact and below 2^53 for every v in range.
// G and M are template parameters so each store compiles to its own
// branch-free rounding.
template <grid G, round_mode M>
[[nodiscard]] constexpr imax snap_double_index(double v) noexcept {
    constexpr double nd = static_cast<double>(G.Notch);
    const double     q  = v / nd;
    const imax       t  = static_cast<imax>(q);       // toward zero
    const double     f  = q - static_cast<double>(t); // exact, sign of q, |f| < 1
    if constexpr (M == round_mode::nearest)
        return t + (f >= 0.5) - (G.Interval.Lower < 0 && f <= -0.5);
    else if constexpr (M == round_mode::floor)
        return t - (f < 0);
    else if constexpr (M == round_mode::ceil)
        return t + (f > 0);
    else if constexpr (M == round_mode::half_even)
        return t + (f > 0.5 || (f == 0.5 && (t & 1))) - (f < -0.5 || (f == -0.5 && (t & 1)));
    else
        return t;
}

// Raw of a point grid (Lower == Upper): its value lives in the type, so the
// raw is empty. It acts as index slot 0 — constructible from any index,
// converting to integer 0 — so the index-storage decode (Lower + raw·Notch)
// yields the point's value without special cases. Declared
// [[no_unique_address]] in inside, a point member of another struct (also
// marked [[no_unique_address]]) takes no space.
struct point_slot {
    constexpr point_slot() = default;
    template <typename T>
        requires std::is_arithmetic_v<T>
    constexpr point_slot(T) noexcept {}                         // any index: the only slot
    constexpr point_slot(const rational&) noexcept {}           // any value: the type holds it
    constexpr      operator imax() const noexcept { return 0; } // reads as index 0
    constexpr bool operator==(const point_slot&) const  = default;
    constexpr auto operator<=>(const point_slot&) const = default;
};

// Notch 1 from an integer Lower: the values are integers, so a raw can hold
// the value itself (value storage). {{0.5, 10.5}, 1} has notch 1 but not
// integer values.
constexpr bool unit_lattice(const grid& g) noexcept {
    return g.Notch == 1 && wide_denominator(g.Interval.Lower) == grid_int{1};
}

// Both endpoints lie in imax — the signed value raw candidates (and every
// `trunc(endpoint)` constant) are only meaningful then.
constexpr bool fits_imax(const interval& iv) noexcept {
    return iv.Lower >= rational{std::numeric_limits<imax>::min()} &&
           iv.Upper <= rational{std::numeric_limits<imax>::max()};
}

// Storage is a function of the grid alone. Order: point → empty point_slot;
// notch zero → an exact fraction (no integer index space); more than 2^64
// slots → a wide_int index; a whole-number grid → its value where that costs
// no width (deduces_value); otherwise the unsigned 0-based index.

// Unsigned index raw for G's slots: a builtin up to 64 bits, else wide.
template <grid G>
using index_raw_for_t = int_for_bits_t<G.slot_bits(), false>;

// The value raw of a whole-number grid above 0: unsigned, holding Upper.
template <grid G>
using value_uint_for_t = int_for_bits_t<bit_width_of(wide_numerator(G.Interval.Upper)), false>;

// A whole-number grid stores the value itself where that is free: below
// zero a signed value (within int64); from 0 the index is the value; above
// 0 the value when its unsigned type is no wider than the index's ({5, 100}
// stores 5..100 in a uint8_t, {200, 300} the index 0..100, as 300 needs 16
// bits).
template <grid G>
inline constexpr bool deduces_value = [] {
    if constexpr (G.Interval.Lower == G.Interval.Upper || !unit_lattice(G) || !G.max_index_representable())
        return false;
    else if constexpr (G.Interval.Lower < 0)
        return fits_imax(G.Interval);
    else if constexpr (G.Interval.Lower == 0)
        return true;
    else if constexpr (!fits_imax(G.Interval))
        return false;
    else
        return sizeof(value_uint_for_t<G>) <= sizeof(index_raw_for_t<G>);
}();

// A continuous grid stores its value as an exact fraction: the 64-bit
// rational, or — past it — a reduced fraction of K-limb integers.
// A bounded grid sizes K from its bounds, so every value fits; an unbounded
// one from its limits, K holding twice the limits' bits, and a value that
// needs more reports overflow, as one past the 64-bit rational does.
template <grid G>
inline constexpr bool bound_fits_rational =
    G.MaxDenominator != 0 && !(grid_int{std::numeric_limits<imax>::max()} < den_bound(G)) &&
    !(grid_int{std::numeric_limits<umax>::max()} < num_bound(G));

template <grid G>
inline constexpr std::size_t continuous_limbs = [] {
    if constexpr (G.MaxDenominator != 0) {
        const int n = bit_width_of(num_bound(G)), d = bit_width_of(den_bound(G));
        return limbs_for_bits((n < d ? d : n) + 1);
    } else {
        auto bits = [](const grid_rational& r) {
            const grid_int n = wide_numerator(r);
            return bit_width_of(abs(n)) + bit_width_of(wide_denominator(r));
        };
        const int b =
            bits(G.Interval.Lower) > bits(G.Interval.Upper) ? bits(G.Interval.Lower) : bits(G.Interval.Upper);
        return limbs_for_bits(2 * b + 2);
    }
}();
template <grid G>
using continuous_raw_t =
    std::conditional_t<G.MaxDenominator != 0 ? bound_fits_rational<G>
                                             : fits_rational(G.Interval.Lower) && fits_rational(G.Interval.Upper),
                       detail::rational,
                       wide_rational<continuous_limbs<G>>>;

template <grid G>
constexpr auto raw_for() {
    if constexpr (G.Interval.Lower == G.Interval.Upper)
        return point_slot{};
    else if constexpr (G.Notch == 0)
        return continuous_raw_t<G>{};
    else if constexpr (!deduces_value<G>)
        return index_raw_for_t<G>{};
    else if constexpr (G.Interval.Lower < 0)
        return int_for_bits_t<signed_value_bits(wide_numerator(G.Interval.Lower), wide_numerator(G.Interval.Upper)),
                              true>{};
    else if constexpr (G.Interval.Lower == 0)
        return index_raw_for_t<G>{};
    else
        return value_uint_for_t<G>{};
}

// The raw type of inside<G, P> (the policy plays no part).
template <grid G>
using raw_for_t = decltype(raw_for<G>());

// A positive power of two (exact at any width).
constexpr bool is_pow2(const grid_int& v) noexcept {
    return grid_int{0} < v && v == (grid_int{1} << (bit_width_of(v) - 1));
}

// Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
// on-grid value is a binary fraction (a double when it fits double_exact).
template <grid G>
inline constexpr bool dyadic_grid =
    G.Notch != 0 && is_pow2(wide_denominator(G.Notch)) && is_pow2(wide_denominator(G.Interval.Lower));

// Bits of |r · 2^f| — an integer on a dyadic grid, where r's denominator is
// a power of two dividing 2^f (0 for r == 0).
constexpr int scaled_numerator_bits(const grid_rational& r, int f) noexcept {
    const grid_int n = wide_numerator(r);
    if (n == grid_int{0})
        return 0;
    return bit_width_of(abs(n)) + f - (bit_width_of(wide_denominator(r)) - 1);
}

// `fp`-exactness of a dyadic grid: the IEEE-754 path equals the exact grid
// arithmetic iff, at the coarsest-magnitude end, the value's ULP is no
// coarser than the notch. Writing v = N·2^(−f) with f = log2(den(Notch)),
// that is |N| < 2^Digits (the significand) AND f ≤ MaxF (notch ≥ the
// smallest normal, so no on-grid value is subnormal). The overflow ceiling
// is unreachable once |N| < 2^Digits.
template <grid G, int Digits, int MaxF>
constexpr bool compute_fp_exact() noexcept {
    if constexpr (!dyadic_grid<G>)
        return false;
    else {
        // f: the finest power of two among the values — the notch's, or
        // Lower's on an unanchored grid ({{0.25, 4.25}, 1}: 2^-2).
        constexpr int f = bit_width_of(wide_denominator(G.value_unit())) - 1;
        return f <= MaxF && scaled_numerator_bits(G.Interval.Lower, f) <= Digits &&
               scaled_numerator_bits(G.Interval.Upper, f) <= Digits;
    }
}

// double: 53-bit significand, notch at least 2^-1022 — every value of G is
// a double.
template <grid G>
inline constexpr bool double_exact = compute_fp_exact<G, 53, 1022>();

// The notch of a × b. A product (La + i·Na)(Lb + j·Nb) differs from La·Lb
// by multiples of Na·Nb, Na·Lb and Nb·La; on an anchored operand its term is
// already a multiple of Na·Nb, so only an unanchored operand adds one. A
// point c (notch 0) scales the other lattice: the notch becomes N·|c|.
constexpr grid_rational product_notch(const grid& a, const grid& b) {
    const bool    ap = a.Interval.Lower == a.Interval.Upper, bp = b.Interval.Lower == b.Interval.Upper;
    grid_rational n =
        ((ap && !bp) ? abs(a.Interval.Lower) : a.Notch) * ((bp && !ap) ? abs(b.Interval.Lower) : b.Notch);
    if (ap || bp)
        return n;
    if (!b.anchored())
        n = gcd(n, a.Notch * b.Interval.Lower);
    if (!a.anchored())
        n = gcd(n, b.Notch * a.Interval.Lower);
    return n;
}

// Whether a / b has a result grid: the divisor is not the point 0, and an
// unbounded continuous divisor does not reach 0.
constexpr bool grid_quotient_fits(const grid& b) {
    const interval& d = b.Interval;
    if (d.Lower == 0 && d.Upper == 0)
        return false;
    return !includes(d, 0) || b.Notch != 0 || b.MaxDenominator != 0;
}
} // namespace detail

//---------------------------------------------------------------------------
// operator+ - * / — templates on G = grid: no conversion reaches them, so an
// inside (which converts to its point grid) never lands here when its own
// operator is constrained away.
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr grid operator+(const G& lhs, const G& rhs) {
    // A continuous operand makes the sum continuous, with denominators at most
    // the product of the operands' bounds. (A point, also notch 0, shifts the
    // other lattice: gcd(0, n) = n is its notch.)
    if (detail::continuous_grid(lhs) || detail::continuous_grid(rhs))
        return detail::bounded_continuous(lhs.Interval + rhs.Interval,
                                          detail::den_bound(lhs) * detail::den_bound(rhs));
    return grid{lhs.Interval + rhs.Interval, gcd(lhs.Notch, rhs.Notch)};
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr grid operator-(const G& lhs, const G& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr grid operator*(const G& lhs, const G& rhs) {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous. A continuous product bounds its denominators by the
    // product of the operands' bounds.
    const interval              i = lhs.Interval * rhs.Interval;
    const detail::grid_rational n = detail::product_notch(lhs, rhs);
    if (n == 0 && i.Lower != i.Upper)
        return detail::bounded_continuous(i, detail::den_bound(lhs) * detail::den_bound(rhs));
    return grid{i, n};
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
template <std::same_as<grid> G>
[[nodiscard]] constexpr std::expected<grid, errc> operator/(const G& lhs, const G& rhs) {
    const interval& r = rhs.Interval;
    // A nonzero point divisor c scales the other lattice exactly, as in
    // operator*: its notch becomes N/|c|, so `x / just<c>` keeps integer
    // storage instead of turning continuous.
    if (r.Lower == r.Upper && r.Lower != 0 && lhs.Notch != 0)
        return grid{*(lhs.Interval / r), lhs.Notch / detail::abs(r.Lower)};
    // Any other quotient is continuous: x = p/a over y = r/b is p·b/(a·r), so
    // its denominators stay within den_bound(lhs)·num_bound(rhs).
    const detail::grid_int qd         = detail::den_bound(lhs) * detail::num_bound(rhs);
    auto                   continuous = [&](const interval& i) { return detail::bounded_continuous(i, qd); };
    if (!includes(r, 0))
        return continuous(*(lhs.Interval / r));
    if (r.Lower == 0 && r.Upper == 0)
        return std::unexpected{errc::division_by_zero};

    // The divisor interval holds 0: `step` bounds the nonzero divisor
    // magnitudes from below, splitting it into positive [step, Upper] and
    // negative [Lower, −step]; with both present the result is their union.
    // Every lattice value is a multiple of the value unit (the notch when
    // anchored, finer when not: {{-1.5, 1.5}, 1} holds ±0.5), and a bounded
    // continuous value p/q has q ≤ MaxDenominator. An unbounded continuous
    // divisor comes arbitrarily close to zero: no bounded quotient.
    if (rhs.Notch == 0 && rhs.MaxDenominator == 0)
        return std::unexpected{errc::overflow};
    const detail::grid_rational step = rhs.Notch != 0 ? rhs.value_unit() : 1 / rhs.MaxDenominator;
    const interval              pos{step, r.Upper}, neg{r.Lower, -step};
    auto over = [&](const interval& part) { return *(lhs.Interval / part); }; // zero-free: no error
    if (0 < r.Upper && 0 > r.Lower)
        return continuous(detail::interval_hull(over(pos), over(neg)));
    return continuous(over(0 < r.Upper ? pos : neg));
}

//---------------------------------------------------------------------------
// hull
//---------------------------------------------------------------------------
// The smallest grid that represents every value of both operands exactly:
// interval hull + notch gcd, refined by the offset between the two lattices
// when they do not line up ({{0, 2}, 1} and {{0.5, 1.5}, 1} hull to notch
// 1/2). A continuous operand (Notch 0) makes the hull continuous.
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr grid hull(const grid& lhs, const grid& rhs) {
    const interval iv = detail::interval_hull(lhs.Interval, rhs.Interval);
    // A continuous operand makes the hull continuous, bounded by the larger
    // bound (unbounded if either is); a point (also notch 0) joins the other
    // lattice through the offset between them.
    if (detail::continuous_grid(lhs) || detail::continuous_grid(rhs)) {
        const detail::grid_int a = detail::den_bound(lhs), b = detail::den_bound(rhs);
        return detail::bounded_continuous(iv,
                                          a == detail::grid_int{0} || b == detail::grid_int{0} ? a * b
                                          : a < b                                              ? b
                                                                                               : a);
    }
    detail::grid_rational n = gcd(lhs.Notch, rhs.Notch);
    if (n == 0 || !detail::grid_same_lattice(lhs.Interval.Lower, rhs.Interval.Lower, n))
        n = gcd(n, rhs.Interval.Lower - lhs.Interval.Lower);
    return grid{iv, n};
}
} // namespace beman::inside


//---------------------------------------------------------------------------
// introspection — an inside's template parameters as values (grid_of<B>,
// flags_of<B>, lower_of<B>, notch_of<B>, … and their 64-bit forms), the
// insidable / numeric concepts, and the basic plumbing every layer uses
// (plain_t, as_rational, raw_t). The storage shapes build
// on it in detail/storage.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
template <grid G = grid{{0, 0}, 0}, policy_flag P = checked>
struct inside;

template <class>
inline constexpr bool is_inside_v = false;
template <grid G, policy_flag P>
inline constexpr bool is_inside_v<inside<G, P>> = true;

template <typename B>
concept insidable = is_inside_v<std::remove_cvref_t<B>>;

//---------------------------------------------------------------------------
// Public grid/policy introspection — extract an inside's template parameters.
// These mirror std::numeric_limits: they report what the grid is, used
// opaquely (the rational return type is never named by callers).
//---------------------------------------------------------------------------
namespace detail {
template <typename B>
struct inside_params;
template <grid G, policy_flag P>
struct inside_params<inside<G, P>> {
    static constexpr grid grid_v = G;
};
} // namespace detail

template <insidable B>
inline constexpr grid grid_of = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

namespace detail {
template <typename T>
inline constexpr policy_flag flags_of_impl = none;
template <grid G, policy_flag P>
inline constexpr policy_flag flags_of_impl<inside<G, P>> = P;
} // namespace detail

// flags_of<X> — the policy flags of an inside, or of a one-shot `policy<F, E>`
// (policy.hpp); `none` for anything else, such as an action.
template <typename X>
inline constexpr policy_flag flags_of = detail::flags_of_impl<std::remove_cvref_t<X>>;

template <typename T>
inline constexpr interval interval_of = {0, 0};

template <insidable B>
inline constexpr interval interval_of<B> = grid_of<B>.Interval;

template <std::integral I>
inline constexpr interval interval_of<I> = {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

template <insidable B>
inline constexpr detail::grid_rational lower_of = grid_of<B>.Interval.Lower;
template <insidable B>
inline constexpr detail::grid_rational upper_of = grid_of<B>.Interval.Upper;
template <insidable B>
inline constexpr detail::grid_rational notch_of = grid_of<B>.Notch;

namespace detail {
// The same as 64-bit rationals, for the paths that work in 64 bits (the
// integer fast paths, the math engines). A grid number past 64 bits fails
// the build here, never truncates.
template <insidable B>
inline constexpr rational lower64 = to_rational(grid_of<B>.Interval.Lower);
template <insidable B>
inline constexpr rational upper64 = to_rational(grid_of<B>.Interval.Upper);
template <insidable B>
inline constexpr rational notch64 = to_rational(grid_of<B>.Notch);
} // namespace detail

template <typename N>
concept numeric = insidable<N> or detail::plain_number<N>;

//---------------------------------------------------------------------------
// Internal plumbing — storage shape, raw/value conversion, dispatch.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
using plain_t = std::remove_cvref_t<T>;

//-------------------------------------------------------------------------
// Conversion-helper legend — the value/raw plumbing reused across the
// engine. "value space" = the number an inside denotes; "raw space" = how it
// is stored (see §2 "Storage encoding" in docs/internals.md). Use this to
// tell the similarly-named helpers apart:
//
//   as_rational(x)         value → rational    exact view of a scalar or inside
//   as_double(b)           raw   → double       kind-aware decode; lossy off dyadic grids
//   to_value(b)            raw   → imax         the inside's integer value (decodes an index)
//   from_value(b, v)       imax  → raw          store integer value v into b (inverse of to_value)
//   raw_cast<B>(x)         x     → raw_t<B>     TYPE cast only — no value arithmetic
//   raw_imax(b)            raw   → imax         widen the raw bits (NOT the value for index storage)
//   raw_from_offset<B>(o)  index → raw_t<B>     adds raw_lo (Lower for a value raw, 0 for an index)
//   raw_of_slot<B>(o)      index → raw_t<B>     any storage, any width (detail/wide_value.hpp)
//-------------------------------------------------------------------------

// Uniform rational view of a scalar or inside (rational{v} / operator rational()).
template <numeric N>
[[nodiscard]] constexpr rational as_rational(N v) {
    if constexpr (plain_number<N>)
        return rational{v};
    else
        return v;
}

// Zero test for a rational divisor: rational stores zero as {0, 1}, so
// Numerator == 0 catches it.
[[nodiscard]] constexpr bool is_canonical_zero(const rational& v) { return v.Numerator == 0; }

// From the grid, not B::raw_type: the storage traits are read inside B's own
// definition, where B is incomplete.
template <insidable B>
using raw_t = raw_for_t<grid_of<B>>;
} // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/storage.hpp
// ======================================================================




//---------------------------------------------------------------------------
// storage — how an inside's value lives in its raw (the storage concepts),
// the raw ↔ value plumbing, the grid shape predicates, and the integer
// rounding of quotients the stores share.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
//-------------------------------------------------------------------------
// The storage is a function of the grid alone (grid.hpp raw_for). The
// leaves partition every inside; each names the raw type and what it means:
//
//   value_storage — the raw IS the value
//     rational_storage                         a 64-bit rational
//     fraction_storage                         a wide_rational (continuous, limits past 64 bits)
//     integer_value_storage                    a builtin integer
//   index_storage — the raw is a 0-based slot index; value = Lower + raw·Notch
//     point_storage                            empty (point_slot): one slot, the value is the type
//     integer_index_storage                    a builtin integer
//     wide_index_storage                       a wide_int (more than 2^64 slots)
//   integer_storage = index_storage | integer_value_storage   (an integer raw)
//
// Concepts, so the groups subsume their leaves and can constrain helpers.
//-------------------------------------------------------------------------
template <typename B>
concept rational_storage = insidable<B> && std::same_as<raw_t<B>, rational>;

// A continuous grid whose limits pass 64 bits: a reduced exact
// fraction of as many limbs as the limits need.
template <typename B>
concept fraction_storage = insidable<B> && wide_rational_like<raw_t<B>>;

// Whether an integer raw holds the value itself (else a slot index): the
// whole-number grids where that is free (grid.hpp deduces_value).
template <insidable B>
inline constexpr bool integer_raw_holds_value = deduces_value<grid_of<B>>;

template <typename B>
concept integer_value_storage = insidable<B> && std::integral<raw_t<B>> && integer_raw_holds_value<B>;

template <typename B>
concept integer_index_storage = insidable<B> && std::integral<raw_t<B>> && !integer_raw_holds_value<B>;

// A point's raw acts as index slot 0.
template <typename B>
concept point_storage = insidable<B> && std::same_as<raw_t<B>, point_slot>;

// Its values need the exact wide paths (detail/wide_value.hpp): a 64-bit
// rational or imax cannot hold them.
template <typename B>
concept wide_index_storage = insidable<B> && wide_integer<raw_t<B>>;

template <typename B>
concept value_storage = rational_storage<B> || fraction_storage<B> || integer_value_storage<B>;

template <typename B>
concept index_storage = point_storage<B> || integer_index_storage<B> || wide_index_storage<B>;

template <typename B>
concept integer_storage = index_storage<B> || integer_value_storage<B>;

// Same raw type AND same encoding (value vs index): only then does one
// inside's raw mean the same as another's. Two insides on one grid always
// share it; two grids may not ({5, 100} holds values, {5, 1000} an index).
template <insidable L, insidable R>
inline constexpr bool same_storage = std::is_same_v<raw_t<L>, raw_t<R>> && index_storage<L> == index_storage<R>;

//-------------------------------------------------------------------------
// Grid shape and magnitude.
//   notched     — Notch != 0: the values sit on a lattice (a point may still
//                 have Notch 0).
//   point_grid  — Lower == Upper: one value. Its raw is point_storage.
//   wide_valued — values past 64 bits: a wide raw, or grid numbers (a limit
//                 or the notch) past 64 bits even with few slots
//                 (wide_grid_numbers). They take the
//                 exact paths of detail/wide_value.hpp.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool notched = notch_of<B> != 0;

template <insidable B>
inline constexpr bool point_grid = lower_of<B> == upper_of<B>;

// The lattice passes through 0 (grid::anchored): every value is a whole
// number of notches, the value index J = value/Notch an integer. Unanchored
// grids ({{0.5, 10.5}, 1}) take the paths that work from Lower.
template <insidable B>
inline constexpr bool anchored = grid_of<B>.anchored();

template <insidable B>
inline constexpr bool wide_grid_numbers =
    !fits_rational(grid_of<B>.Interval.Lower) || !fits_rational(grid_of<B>.Interval.Upper) ||
    !fits_rational(grid_of<B>.Notch);

// A lattice whose values pass the 64-bit rational between limits that fit:
// on {{-1.2e19, 1.2e19}, 1.5} the half-integers need numerators to 2.4e19.
template <insidable B>
inline constexpr bool lattice_past_rational =
    grid_of<B>.Notch != 0 && (grid_int{std::numeric_limits<imax>::max()} < den_bound(grid_of<B>) ||
                              grid_int{std::numeric_limits<umax>::max()} < num_bound(grid_of<B>));

// Values past the 64-bit rational: a wide index, grid numbers past 64 bits,
// a lattice past it, or a fraction raw (a bounded continuous grid can need
// one).
template <insidable B>
inline constexpr bool wide_valued =
    wide_index_storage<B> || wide_grid_numbers<B> || lattice_past_rational<B> || fraction_storage<B>;

// Whether an inside converts to the 64-bit rational: a lattice past it still
// does, for its values that fit (the conversion reports the others).
template <insidable B>
inline constexpr bool rational_readable = !wide_valued<B> || (lattice_past_rational<B> && !wide_index_storage<B> &&
                                                              !wide_grid_numbers<B> && !fraction_storage<B>);

template <insidable B>
constexpr auto exact_of(const B& b); // wide_value.hpp

// The value indices Lower/Notch and Upper/Notch, as 64-bit rationals (for
// the 64-bit paths of a notched grid).
template <insidable B>
inline constexpr auto index_lo64 = lower64<B> / notch64<B>;
template <insidable B>
inline constexpr auto index_hi64 = upper64<B> / notch64<B>;

// Index raw → double in one IEEE operation. On an anchored grid with Notch
// p/q the value is J·p/q, J = raw + Lower/Notch. With |J·p| and q at most 2^53
// both are exact doubles, so double(J·p) / double(q) is the correctly rounded
// quotient: what the rational decode gives (a power-of-two q compiles to a
// multiply).
struct index_double_codec {
    bool   Ok;
    imax   LowerIndex; // Lower / Notch
    imax   P;          // Notch numerator
    double Q;          // Notch denominator
};
template <insidable B>
inline constexpr index_double_codec index_double = [] {
    if constexpr (!integer_index_storage<B> || !anchored<B> || wide_valued<B>)
        return index_double_codec{};
    else {
        constexpr umax k53 = umax{1} << 53;
        const rational n   = notch64<B>;
        const auto     lo = index_lo64<B>, hi = index_hi64<B>;
        if (abs_den(n.Denominator) > k53)
            return index_double_codec{};
        const umax m = lo->Numerator > hi->Numerator ? lo->Numerator : hi->Numerator; // max |J|
        if (m > k53 / n.Numerator)
            return index_double_codec{};
        return index_double_codec{
            true, signed_numerator(*lo), static_cast<imax>(n.Numerator), static_cast<double>(abs_den(n.Denominator))};
    }
}();

// Ungated double view of any inside (the public operator double() is gated
// on a rounding flag; this is always available). Everything but index
// storage holds the value verbatim; an index decodes through the grid.
template <insidable B>
[[nodiscard]] constexpr double as_double(const B& b) noexcept {
    if constexpr (wide_valued<B>)
        return static_cast<double>(exact_of(b));
    else if constexpr (point_storage<B>)
        return static_cast<double>(lower64<B>);
    else if constexpr (value_storage<B>)
        return static_cast<double>(b.raw());
    else if constexpr (constexpr auto c = index_double<B>; c.Ok)
        return static_cast<double>((static_cast<imax>(b.raw()) + c.LowerIndex) * c.P) / c.Q;
    else
        return static_cast<double>(as_rational(b)); // exact, even where Raw·Notch passes 64 bits
}

// True when R's interval cannot contain zero — so `a / b` can return a plain
// `inside` instead of `expected<inside, errc>` (see detail/division.hpp). A point
// grid at 0 is *not* excluded.
template <insidable R>
inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

// Storage-agnostic int truncation of interval endpoints — intent-revealing
// `static_cast<imax>(lower64<B>)`. Used by from_value, raw_lo, the fast paths.
template <insidable B>
inline constexpr imax lower_imax = trunc(lower64<B>);

template <insidable B>
inline constexpr imax upper_imax = trunc(upper64<B>);

// Slot count via grid::max_index (overflow-safe: 0 when it doesn't fit umax,
// for grids with a wide index, which take the exact wide paths).
template <insidable B>
inline constexpr umax max_index_of = grid_of<B>.max_index();

// Every value — and, for index storage, every slot — fits imax. Gates the
// integer fast paths that work in imax (raw_imax / to_value / raw_lo /
// raw_hi); a grid reaching past int64 (e.g. {0, 2^64−1} in a uint64) takes
// the exact rational / umax paths instead.
template <insidable B>
inline constexpr bool values_fit_imax =
    !wide_valued<B> && fits_imax(interval_of<B>) && (value_storage<B> || std::in_range<imax>(max_index_of<B>));

// Every value's reduced numerator and denominator fit imax too: a value
// within int64 can still have a numerator past it ((2^63 + 1)/2). An
// unbounded continuous grid bounds denominators by imax only, so its
// numerators fit when no value passes 1 in magnitude.
template <insidable B>
inline constexpr bool fraction_fits_imax = [] {
    if constexpr (!values_fit_imax<B>)
        return false;
    else {
        constexpr grid_int d = den_bound(grid_of<B>);
        if constexpr (d == grid_int{0})
            return !(grid_rational{1} < abs(lower_of<B>)) && !(grid_rational{1} < abs(upper_of<B>));
        else
            return !(grid_int{std::numeric_limits<imax>::max()} < num_bound(grid_of<B>));
    }
}();

// Notch a non-zero integer and Lower an integer — every value is an
// integer ({{0.5, 10.5}, 1} has an integer notch but not integer values).
// Gates the implicit imax/size_t conversions.
template <grid G>
inline constexpr bool integer_notch =
    wide_denominator(G.Notch) == grid_int{1} && G.Notch != 0 && wide_denominator(G.Interval.Lower) == grid_int{1};

// ONLY type conversion, NO value representation conversion calculation
template <insidable B>
[[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept {
    return static_cast<raw_t<B>>(value);
}

// Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
// grids where raw is an index rather than a value; the names keep the two
// intents apart although both are a `static_cast<imax>`.
template <insidable B>
    requires(!wide_index_storage<B>) // a wide raw does not fit imax: use the exact wide path
constexpr imax raw_imax(B b) noexcept {
    return static_cast<imax>(b.raw());
}

//-------------------------------------------------------------------------
// Q-format integer fast path: for grids with integer Lower, unit-numerator
// Notch 1/nd, and every value·nd within imax, value↔raw is pure integer
// arithmetic. Shared
// by operator rational(), from_value, and assignment::store.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool qformat_codec_fits = [] {
    if constexpr (abs_den(lower64<B>.Denominator) != 1 || notch64<B>.Numerator != 1 || !values_fit_imax<B>)
        return false;
    else { // every value·nd (so Lower·nd and raw + Lower·nd) in imax
        constexpr imax nd = abs_den(notch64<B>.Denominator);
        constexpr auto lo = try_mul(lower64<B>, rational{nd});
        constexpr auto hi = try_mul(upper64<B>, rational{nd});
        return lo.has_value() && hi.has_value() && *lo >= rational{std::numeric_limits<imax>::min()} &&
               *hi <= rational{std::numeric_limits<imax>::max()};
    }
}();

// value → raw, integer math only.
template <insidable B>
    requires qformat_codec_fits<B>
constexpr raw_t<B> q_format_encode(imax value) noexcept {
    constexpr imax nd = abs_den(notch64<B>.Denominator);
    return raw_cast<B>((value - lower_imax<B>)*nd);
}

// raw → rational, integer math only.
template <insidable B>
    requires qformat_codec_fits<B>
constexpr rational q_format_decode(B b) noexcept {
    constexpr imax nd = abs_den(notch64<B>.Denominator);
    const imax     n  = raw_imax(b) + lower_imax<B> * nd;
    if constexpr (std::has_single_bit(static_cast<umax>(nd))) {
        // A power-of-two denominator reduces by the common trailing zeros
        // (0 reduces to 0/1).
        const umax m = n < 0 ? umax{0} - static_cast<umax>(n) : static_cast<umax>(n);
        const int  s = std::countr_zero(m | static_cast<umax>(nd));
        rational   r;
        r.Numerator   = m >> s;
        r.Denominator = n < 0 ? -(nd >> s) : nd >> s;
        return r;
    } else
        return rational{n, nd};
}

// Library-internal extraction helper. Always succeeds (returns `imax`
// unconditionally) but does not check the value fits in any narrower
// target. User code should prefer `b.to<T>()`, which carries a typed
// overflow error.
template <insidable B>
[[nodiscard]] constexpr imax to_value(B b) noexcept {
    if constexpr (value_storage<B>)
        return raw_imax(b);
    else if constexpr (abs_den(notch64<B>.Denominator) == 1 && abs_den(lower64<B>.Denominator) == 1)
        // In umax: raw·Notch can pass imax although the value fits.
        return static_cast<imax>(static_cast<umax>(lower_imax<B>) +
                                 static_cast<umax>(raw_imax(b)) * notch64<B>.Numerator);
    else if constexpr (qformat_codec_fits<B>) {
        constexpr imax nd = abs_den(notch64<B>.Denominator);
        return (raw_imax(b) + lower_imax<B> * nd) / nd; // q_format_decode, truncated
    } else                                              // index storage, generic rational path
        return trunc(as_rational(b));
}

template <insidable B>
constexpr void from_value(B& b, imax val) {
    if constexpr (value_storage<B>)
        b = B::from_raw(raw_cast<B>(val));
    else if constexpr (abs_den(notch64<B>.Denominator) == 1 && abs_den(lower64<B>.Denominator) == 1)
        b = B::from_raw(
            raw_cast<B>((static_cast<umax>(val) - static_cast<umax>(lower_imax<B>)) / notch64<B>.Numerator));
    else if constexpr (qformat_codec_fits<B>)
        b = B::from_raw(q_format_encode<B>(val));
    else // index storage, generic rational path
    {
        auto offset = (rational{val} - lower64<B>) / notch64<B>;
        b           = B::from_raw(raw_cast<B>(offset.value().Numerator));
    }
}

//-------------------------------------------------------------------------
// raw_lo / raw_hi / raw_from_offset — an integer raw's endpoints. An index
// raw is 0-based (raw_lo == 0); a value raw IS the value (raw_lo == Lower),
// so an offset needs raw_lo<L> added back before storing.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr imax raw_lo = integer_value_storage<B> ? lower_imax<B> : 0;

template <insidable B>
inline constexpr imax raw_hi = integer_value_storage<B> ? upper_imax<B> : static_cast<imax>(max_index_of<B>);

// The exact raw range: 0 .. slot count for index storage, Lower .. Upper
// for value storage (integers there). Sizes the work types below.
template <insidable B>
inline constexpr grid_int raw_lo_exact = index_storage<B> ? grid_int{0} : wide_numerator(lower_of<B>);
template <insidable B>
inline constexpr grid_int raw_hi_exact = index_storage<B> ? grid_of<B>.slot_count() : wide_numerator(upper_of<B>);

// Value bits for every value in a list (their min .. max).
constexpr int signed_value_bits_of(std::initializer_list<grid_int> vals) noexcept {
    const auto [mn, mx] = std::minmax(vals);
    return signed_value_bits(mn, mx);
}

// Signed work type for an exact intermediate of Bits value bits: imax for
// everything within int64 (the builtin fast paths keep their codegen),
// a wide_int beyond.
template <int Bits>
using work_int_t = int_for_bits_t<(Bits < 63 ? 63 : Bits), true>;

// t = Quot·m + Rem with 0 ≤ Rem < m (m > 0): division rounded toward −∞,
// the fold of every wrap. A wide int divides once for both parts.
template <typename W>
struct floored {
    W Quot, Rem;
};
template <raw_integer W>
[[nodiscard]] constexpr floored<W> floor_divmod(const W& t, const W& m) noexcept {
    floored<W> f;
    if constexpr (wide_integer<W>) {
        auto [q, r] = W::divmod(t, m);
        f           = {q, r};
    } else
        f = {t / m, t % m};
    if (f.Rem < W{0}) {
        f.Rem += m;
        f.Quot -= W{1};
    }
    return f;
}

// Offset (umax or imax) → raw. Adds in umax: the bits are the same, but a
// value raw of a grid reaching past int64 (offset + Lower ≥ 2^63) must not
// overflow imax.
template <insidable L, std::integral W>
constexpr raw_t<L> raw_from_offset(W offset) noexcept {
    return raw_cast<L>(static_cast<umax>(offset) + static_cast<umax>(raw_lo<L>)); // raw_lo: 0 for an index raw
}

// Raw → offset (slot 0 at Lower), raw_from_offset's inverse, in umax.
template <insidable L, std::integral R>
constexpr umax raw_offset(R raw) noexcept {
    return static_cast<umax>(static_cast<imax>(raw)) - static_cast<umax>(raw_lo<L>);
}

// The raw of L's Lower (low) or Upper endpoint: the exact constant for a
// rational raw, slot 0 or max_index_of for an integer one.
template <insidable L>
constexpr raw_t<L> endpoint_raw(bool low) noexcept {
    if constexpr (rational_storage<L>)
        return low ? lower64<L> : upper64<L>;
    else
        return raw_from_offset<L>(low ? umax{0} : max_index_of<L>);
}

// The raw of a value v on L's lattice within [Lower, Upper], through its
// exact offset (v − Lower)/Notch.
template <insidable L>
constexpr raw_t<L> raw_of_lattice_value(rational v) {
    return raw_from_offset<L>(((v - lower64<L>).value() / notch64<L>).value().Numerator);
}

//-------------------------------------------------------------------------
// integer_limits vs integer_lattice — easy to confuse, both needed.
//   integer_limits<B>: Lower and Upper integer (Notch may be fractional,
//     e.g. inside<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
//   integer_lattice<B>: every value an integer — an integer Lower and an
//     integer non-zero Notch, or a point ⇒ integer_limits (not the converse).
//     A continuous grid (Notch 0) is not one, whatever its limits.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool integer_limits =
    wide_denominator(lower_of<B>) == grid_int{1} && wide_denominator(upper_of<B>) == grid_int{1};

template <insidable B>
inline constexpr bool integer_lattice =
    integer_notch<grid_of<B>> || (lower_of<B> == upper_of<B> && wide_denominator(lower_of<B>) == grid_int{1});

// Policy test: checks both type-level and per-operation policy.
// Composite flags (e.g. round_nearest = bit5 | snap) require all
// their bits set — having a subset like just `snap` does NOT match.
template <insidable B, typename P, policy_flag F>
inline constexpr bool has_policy = has_flag(flags_of<B>, F) || plain_t<P>::test(F);

// rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
template <insidable L, typename P>
inline constexpr round_mode rounding_for = has_policy<L, P, round_floor>       ? round_mode::floor
                                           : has_policy<L, P, round_ceil>      ? round_mode::ceil
                                           : has_policy<L, P, round_half_even> ? round_mode::half_even
                                           : has_policy<L, P, round_nearest>   ? round_mode::nearest
                                                                               : round_mode::trunc;

// ⌊a / b⌋ for grid numbers (b > 0).
constexpr grid_int floor_quotient(const grid_rational& a, const grid_rational& b) noexcept {
    const grid_int n = wide_numerator(a) * wide_denominator(b), d = wide_denominator(a) * wide_numerator(b);
    const grid_int q = n / d;
    return (n % d != grid_int{0} && n.negative()) ? q - grid_int{1} : q;
}

// The lattice index of Lower, ⌊Lower/Notch⌋ — slot_base on an anchored grid.
// Its parity plus an offset's says which lattice points are even.
template <insidable B>
inline constexpr grid_int lower_index_wide = notched<B> ? floor_quotient(lower_of<B>, notch_of<B>) : grid_int{0};
template <insidable B>
inline constexpr bool lower_index_odd = (static_cast<umax>(lower_index_wide<B>) & 1) != 0;

// The rounding step onto L's lattice by rounding_for<L, P>: the exact
// lattice offset q (v/Notch when anchored, (v − Lower)/Notch otherwise) as
// a base index and whether to step one unit up from it. `negative`: v < 0.
// A rational index never wraps, however far past imax it lies.
struct lattice_step {
    rational Base;
    bool     Step;
};
template <insidable L, typename P>
[[nodiscard]] constexpr lattice_step lattice_step_of(rational q, bool negative) {
    constexpr round_mode m = rounding_for<L, P>;
    if constexpr (anchored<L>)
        return {round_to_integral(q, m), false}; // the value index's own sign rules
    else {
        const rational k = round_to_integral(q, round_mode::floor);
        const rational f = (q - k).value(); // in [0, 1): no overflow
        return {k,
                rounds_up(m,
                          negative,
                          classify_remainder(m, f.Numerator, abs_den(f.Denominator)),
                          ((k.Numerator & 1) != 0) != lower_index_odd<L>)};
    }
}

// v rounded onto L's lattice {Lower + k·Notch} by rounding_for<L, P> (value
// space, ties half away from zero), for any v — not limited to [Lower,
// Upper], so wrap can round first and fold an on-lattice value after: a
// result or step past the rational range is errc::overflow.
template <insidable L, typename P>
[[nodiscard]] constexpr std::expected<rational, errc> try_round_to_lattice(rational v) {
    if constexpr (!notched<L>)
        return v;
    else {
        const auto q = anchored<L>
                           ? try_div(v, notch64<L>)
                           : try_sub(v, lower64<L>).and_then([](rational off) { return try_div(off, notch64<L>); });
        if (!q)
            return q;
        const auto [base, step] = lattice_step_of<L, P>(*q, v < 0);
        const auto j            = step ? try_add(base, rational{1}) : std::expected<rational, errc>{base};
        if (!j)
            return j;
        const auto rel = try_mul(*j, notch64<L>);
        return (rel && !anchored<L>) ? try_add(lower64<L>, *rel) : rel;
    }
}

// Round, then range-check. Lower and Upper are lattice points, so rounding
// an in-range value keeps it in range; only an out-of-range value can change
// outcome. When the policy may round (snap), rounds_into_range rounds such a
// value and reports whether it lands inside the interval (`out` = the
// rounded value). Only values within one notch of the interval can, which
// also keeps try_round_to_lattice's division bounded for huge sources. A
// bounded continuous grid rounds onto its bound the same way: its limits are
// values, and fractions k/D lie 1/D apart.
template <insidable L, typename P>
inline constexpr bool rounds_before_range_check =
    (notched<L> || grid_of<L>.MaxDenominator != 0) && has_policy<L, P, snap>;

// rounds_into_range for a v whose rounding passes the 64-bit rational on the
// way (v − Lower for a fine v's denominator), decided exactly (wide_value.hpp).
template <insidable L, typename P>
constexpr bool exact_rounds_into_range(const rational& v, rational& out);

template <insidable L, typename P>
[[nodiscard]] constexpr bool rounds_into_range(rational v, rational& out) {
    if constexpr (!rounds_before_range_check<L, P>)
        return false;
    else {
        // A bound past imax: 1/imax still widens the limits by at least a
        // step (only a prefilter: the exact rounding decides).
        constexpr rational step = [] {
            if constexpr (notched<L>)
                return notch64<L>;
            else {
                constexpr umax D = to_rational(grid_of<L>.MaxDenominator).Numerator;
                constexpr umax M = static_cast<umax>(std::numeric_limits<imax>::max());
                return rational{1, static_cast<imax>(D < M ? D : M)};
            }
        }();
        constexpr rational lo = try_sub(lower64<L>, step).value_or(lower64<L>);
        constexpr rational hi = try_add(upper64<L>, step).value_or(upper64<L>);
        if (v <= lo || v >= hi)
            return false;
        if constexpr (!notched<L>)
            return exact_rounds_into_range<L, P>(v, out);
        // Within one notch of the limits the rounded value fits; the steps
        // there may not, for a fine v.
        const auto r = try_round_to_lattice<L, P>(v);
        if (!r) [[unlikely]]
            return exact_rounds_into_range<L, P>(v, out);
        out = *r;
        return includes(interval_of<L>, out);
    }
}

// Store-side form for the assignment paths: when v rounds inside, the raw of
// the rounded lattice point (an exact in-range point: an index or rational
// raw, no further rounding). Cold and out of line, and it returns the
// raw in registers instead of writing through the caller's inside:
//   - a second call site of the large store functions stops GCC inlining
//     them into the hot path (~40 instructions per in-range store);
//   - an escaping `lhs` address turns on the stack protector there (~3).
template <insidable L>
struct rounded_raw {
    raw_t<L> Raw;
    bool     Ok;
};

template <insidable L, typename P>
[[gnu::cold]] [[gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v) {
    rational r;
    if (!rounds_into_range<L, P>(v, r))
        return {raw_t<L>{}, false};
    if constexpr (rational_storage<L>)
        return {r, true};
    else
        return {raw_of_lattice_value<L>(r), true};
}

// Rounds the split offset quotient q + r/den (r < den ≤ imax_max) per L's
// rounding policy — q/r form so no expression can overflow umax
// (num + den/2 could, for num near umax). round_quotient's offset rule.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_offset(umax q, umax r, umax den) noexcept {
    constexpr round_mode m = rounding_for<L, P>;
    return q + rounds_away(m, false, classify_remainder(m, r, den), (q & 1) != 0);
}

// The offset quotient num/den (den ≥ 1) of an unanchored grid rounded in
// value space: the value Lower + (num/den)·Notch is below zero exactly when
// num/den < −Lower/Notch, compared without overflow in 128 bits.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_offset_unanchored(umax num, umax den) noexcept {
    const umax           q = num / den, r = num % den;
    constexpr round_mode m        = rounding_for<L, P>;
    bool                 negative = false;
    if constexpr (lower64<L> < 0) {
        constexpr rational c = -*index_lo64<L>; // > 0
        using W              = wide_uint<2>;
        negative             = W{num} * W{abs_den(c.Denominator)} < W{c.Numerator} * W{den};
    }
    return q + rounds_up(m, negative, classify_remainder(m, r, den), ((q & 1) != 0) != lower_index_odd<L>);
}

// Round the non-negative offset quotient num/den (den >= 1) to an integer
// notch index per L's rounding policy.
//
// Tie/sign rules are in VALUE space, not offset space, so assigning a value
// rounds it the same way dividing down to it does (div_rounded is the
// reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
// we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
// it like div_rounded, and return the offset J - m. m is integral on every
// anchored grid; an unanchored one rounds the offset against the value's sign.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept {
    // Lower/Notch past 64 bits (a far-off grid with a fine notch) has no imax
    // value index: round the offset as for a non-integral m.
    constexpr auto zq   = (!notched<L>) ? std::expected<rational, errc>{rational{0}} : try_div(lower64<L>, notch64<L>);
    constexpr bool vidx = zq.has_value() && abs_den(zq->Denominator) == 1 &&
                          zq->Numerator <= static_cast<umax>(std::numeric_limits<imax>::max()) + 1;
    constexpr imax m    = vidx ? signed_numerator(*zq) : imax{0};

    if constexpr (!anchored<L>)
        return round_offset_unanchored<L, P>(num, den);
    else if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
    else {
        // Round the signed value-index NUM/di exactly like div_rounded.
        // A numerator or m·di beyond imax (fine-denominator sources on grids
        // with large |Lower·count|) cannot rebuild the signed index: round the
        // offset's floor q instead, in value space — the value is below zero
        // exactly when the floor's value index m + q is.
        const imax di = static_cast<imax>(den);
        imax       mdi, NUM;
        if (!std::in_range<imax>(num) || mul_overflow(m, di, &mdi) || add_overflow(mdi, static_cast<imax>(num), &NUM))
            [[unlikely]] {
            constexpr round_mode mode = rounding_for<L, P>;
            const umax           q = num / den, r = num % den;
            const bool           negative = m < 0 && q < umax{0} - static_cast<umax>(m);
            return q +
                   rounds_up(mode, negative, classify_remainder(mode, r, den), ((q ^ static_cast<umax>(m)) & 1) != 0);
        }
        const imax           t    = NUM / di; // C++ truncation toward zero
        const imax           rr   = NUM % di; // sign of NUM, |rr| < di
        const bool           neg  = NUM < 0;
        const umax           ar   = (rr < 0) ? ~static_cast<umax>(rr) + 1u : static_cast<umax>(rr);
        constexpr round_mode mode = rounding_for<L, P>;
        const imax J = rounds_away(mode, neg, classify_remainder(mode, ar, static_cast<umax>(di)), (t & 1) != 0)
                           ? (neg ? t - 1 : t + 1)
                           : t;
        return static_cast<umax>(J - m); // offset index k = J - m (>= 0)
    }
}

// Forward decl — defined in assignment.hpp
template <typename L, typename R>
struct assignment;

// A single-point source (Lower == Upper) carries one value, so its notch
// question is only whether that value lies on L's lattice — admitting
// `3_ins` into `{{0,9},3}` while rejecting `1_ins`. (Range is the interval
// check's job, so clamp/wrap still take an out-of-range point.) The raw
// Factor says nothing here: a point's notch is 0.
template <typename L, typename R>
inline constexpr bool point_on_lattice = grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);

// The notch half of inside_assignable for an insidable R.
template <typename L, typename R>
inline constexpr bool notches_compatible = [] {
    if constexpr (point_grid<R>)
        return point_on_lattice<L, R>;
    else if constexpr (!notched<L> || !notched<R>)
        // A continuous target holds every value; a continuous source is
        // checked when stored, like a rational scalar.
        return true;
    else if constexpr (wide_valued<L> || wide_valued<R>)
        // Every R value on L's lattice: R's notch a multiple of L's, and R's
        // lattice anchored on L's.
        return grid_divides_evenly(notch_of<R>, notch_of<L>) &&
               grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);
    else
        // R's notch a whole number of L's, and R's lattice on L's (a given
        // when both pass through 0).
        return abs_den(assignment<L, R>::Factor.Denominator) == 1 &&
               grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);
}();

// Tail of the policy cascade: checked reports.
// Returns true if a policy handled the failure (caller should return).
// Cheap default — reports through the static category message (no string).
template <typename P>
constexpr bool range_fail(P&& policy) {
    if (policy.range_check()) {
        policy.report(errc::overflow);
        return true;
    }
    return false;
}
} // namespace beman::inside::detail



//---------------------------------------------------------------------------
// generic — the type-level layer every other header builds on: the public
// introspection (detail/introspection.hpp), the storage shapes
// (detail/storage.hpp), and here the inside_assignable concept with its
// diagnostics.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// The two non-trivial clauses of `inside_assignable`, named so the concept and
// its `inside_assignable_why` diagnostic share one definition. Concepts (not
// bools) so `||` short-circuits *instantiation* (e.g. assignment<L,R>::Factor
// is never formed when R isn't insidable).
template <typename L, typename R, policy_flag P = checked>
concept assign_intervals_ok =
    (!insidable<R> && !std::integral<R>)
    // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
    // for them (the integral-rhs path already allows it — int's interval is unbounded).
    || ((flags_of<L> | P) & (wrap | clamp)) != 0 || not excludes(interval_of<L>, interval_of<R>);

template <typename L, typename R, policy_flag P>
concept assign_notch_ok = !insidable<R> || ((flags_of<L> | P) & snap) != 0 || notches_compatible<L, R>;

// R's notch for a diagnostic (0 for a non-inside, which never fails there).
template <typename R>
consteval grid_rational notch_for_diag() {
    if constexpr (insidable<R>)
        return notch_of<R>;
    else
        return grid_rational{0};
}
} // namespace detail

// Compile-time prerequisites for L = R, gating three failure modes at the call
// site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
// skipped for float/rational, which have no static interval); (3) integer
// notch ratio or snap set (else R's notch doesn't divide L's; opt into
// rounding). Named `inside_assignable` to avoid shadowing std::assignable_from.
template <typename L, typename R, policy_flag P = checked>
concept inside_assignable = numeric<R> && detail::assign_intervals_ok<L, R, P> && detail::assign_notch_ok<L, R, P>;

// Diagnostic helper: instantiating `inside_assignable_why<L,R,P>` fires a named
// static_assert per failed clause, so a developer can see which tripped. Backs
// both the default-build diagnostic fallbacks in `inside` (core.hpp, gated by
// `BEMAN_INSIDE_STRICT_SFINAE`) and the public `explain_assignable` probe below.
template <typename L, typename R, policy_flag P = checked>
struct inside_assignable_why {
    // Collapse each clause to a plain bool *before* the static_assert. Asserting on
    // a concept-id makes GCC dump the whole satisfaction tree ("constraints not
    // satisfied / no operand of the disjunction…") on top of the message; a bool
    // condition prints just the message. Each clause is self-guarding (the inner
    // disjunctions gate `assignment<L,R>::Factor` on `insidable<R>`), so evaluating
    // all three unconditionally is safe even when R is not numeric.
    static constexpr bool is_numeric   = numeric<R>;
    static constexpr bool intervals_ok = detail::assign_intervals_ok<L, R, P>;
    static constexpr bool notch_ok     = detail::assign_notch_ok<L, R, P>;
    static_assert(is_numeric, "inside_assignable: rhs is not numeric (must be an inside or arithmetic type)");
    static_assert(
        intervals_ok,
        detail::diag_text{} << "inside_assignable: rhs interval lies entirely outside lhs interval and the policy "
                               "(not wrap/clamp) cannot bring it into range — assignment can never succeed (lhs ["
                            << interval_of<L>.Lower << ", " << interval_of<L>.Upper << "], rhs ["
                            << interval_of<R>.Lower << ", " << interval_of<R>.Upper << "])");
    static_assert(notch_ok,
                  detail::diag_text{} << "inside_assignable: incompatible notches — use `with_snap()` or "
                                         "`policy<snap>()` to allow rounding (lhs notch "
                                      << notch_of<L> << ", rhs notch " << detail::notch_for_diag<R>() << ")");
    static constexpr bool value = inside_assignable<L, R, P>;
};

// Public manual probe: `static_assert(beman::inside::explain_assignable<DstInside, decltype(src)>);`
// emits the named per-clause reasons in any build — including a strict
// (`BEMAN_INSIDE_STRICT_SFINAE`) build where the automatic in-`inside` fallbacks are absent.
template <typename Dst, typename Src, policy_flag P = flags_of<Dst>>
inline constexpr bool explain_assignable = inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/wide_value.hpp
// ======================================================================




//---------------------------------------------------------------------------
// wide_value — exact values of insides the 64-bit paths cannot hold: a wide
// index raw (more than 2^64 slots), or grid numbers past 64 bits.
//
// On an anchored grid Lower/Notch is an integer m (slot_base), so a value is
// its value index J = m + raw times Notch, exactly; an unanchored grid counts
// in its value unit gcd(Notch, Lower) instead. wide_rational<K> carries a value
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
constexpr std::strong_ordering operator<=>(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<exact_max<A, B>>;
    const F x{a}, y{b};
    return x.Num * y.Den <=> y.Num * x.Den;
}
template <std::size_t A, std::size_t B>
constexpr bool operator==(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    return (a <=> b) == 0;
}
template <std::size_t A, std::size_t B>
constexpr auto operator+(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<exact_max<A, B>>;
    const F x{a}, y{b};
    return F{x.Num * y.Den + y.Num * x.Den, x.Den * y.Den};
}
template <std::size_t A, std::size_t B>
constexpr auto operator*(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<exact_max<A, B>>;
    const F x{a}, y{b};
    return F{x.Num * y.Num, x.Den * y.Den};
}
// Pre: b != 0.
template <std::size_t A, std::size_t B>
constexpr auto operator/(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<exact_max<A, B>>;
    const F x{a}, y{b};
    const F q{x.Num * y.Den, x.Den * y.Num};
    return q.Den.negative() ? F{-q.Num, -q.Den} : q;
}
template <std::size_t K>
constexpr wide_rational<K> operator-(const wide_rational<K>& a) noexcept {
    return {-a.Num, a.Den};
}

template <std::size_t K = exact_min_limbs>
constexpr wide_rational<K> exact_of(const rational& r) noexcept {
    return {static_cast<wide_sint<K>>(wide_numerator(r)), static_cast<wide_sint<K>>(wide_denominator(r))};
}

template <std::size_t K = exact_min_limbs, std::integral T>
constexpr wide_rational<K> exact_of(T v) noexcept {
    return {wide_sint<K>{v}, wide_sint<K>{1}};
}

// A grid number (a limit or notch, of any size) as an exact value.
template <std::size_t K>
constexpr wide_rational<K> exact_of_grid(const grid_rational& r) noexcept {
    return {static_cast<wide_sint<K>>(wide_numerator(r)), static_cast<wide_sint<K>>(wide_denominator(r))};
}

// a / b for grid numbers, known at compile time to be an integer.
constexpr grid_int exact_quotient(const grid_rational& a, const grid_rational& b) noexcept {
    return wide_numerator(a) * wide_denominator(b) / (wide_denominator(a) * wide_numerator(b));
}

// m = Lower/Notch, the value index of slot 0 (0 for a continuous grid). An
// integer only on an anchored grid: the paths that use it require one.
template <insidable B>
inline constexpr grid_int slot_base = notched<B> ? exact_quotient(lower_of<B>, notch_of<B>) : grid_int{0};

// Bits that bound every value of B's grid: |value| < 2^grid_magnitude_bits.
template <insidable B>
inline constexpr int grid_magnitude_bits = [] {
    auto bits = [](const grid_rational& r) {
        const grid_int n = wide_numerator(r);
        return bit_width_of(abs(n)) - (bit_width_of(wide_denominator(r)) - 1) + 1;
    };
    const int a = bits(lower_of<B>), b = bits(upper_of<B>);
    return a > b ? a : b;
}();

// Bits of |v| for a grid integer.
constexpr int grid_bits(const grid_int& v) { return bit_width_of(abs(v)); }

// Bits of B's values as fractions J·n/d: numerator and denominator together.
template <insidable B>
inline constexpr int exact_value_bits = [] {
    if constexpr (point_storage<B>) {
        return grid_bits(wide_numerator(lower_of<B>)) + grid_bits(wide_denominator(lower_of<B>));
    } else if constexpr (fraction_storage<B>)
        return 2 * decltype(raw_t<B>::Num)::bits;
    else if constexpr (!wide_valued<B>)
        return 128; // a 64-bit rational
    else {
        return grid_magnitude_bits<B> + grid_bits(wide_denominator(notch_of<B>)) +
               grid_bits(wide_numerator(notch_of<B>));
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
        return wide_rational<K>{b.raw()};
    else if constexpr (anchored<B>) // an integer raw: its value index J times the notch
        return wide_rational<K>{value_index<I>(b) * static_cast<I>(wide_numerator(notch_of<B>)),
                                static_cast<I>(wide_denominator(notch_of<B>))};
    else // an unanchored index raw: Lower + raw·Notch over their common denominator
        return exact_of_grid<K>(lower_of<B>) +
               wide_rational<K>{static_cast<I>(b.raw()) * static_cast<I>(wide_numerator(notch_of<B>)),
                                static_cast<I>(wide_denominator(notch_of<B>))};
}

// f in lowest terms.
template <std::size_t K>
constexpr wide_rational<K> reduced(const wide_rational<K>& f) noexcept {
    using I = wide_sint<K>;
    I a = abs(f.Num), b = f.Den;
    if (bit_width_of(a) <= 64 && bit_width_of(b) <= 64) { // the common case: one-limb Euclid
        const umax an = static_cast<umax>(a), bn = static_cast<umax>(b), g = std::gcd(an, bn);
        if (g == 1)
            return f;
        const I n{an / g};
        return {f.Num.negative() ? -n : n, I{bn / g}};
    }
    const I g = wide_gcd(a, b);
    if (g == I{1})
        return f;
    return {f.Num / g, f.Den / g};
}

// x rounded by M onto the fractions with denominator at most D (D ≥ 1):
// the two Farey neighbours l ≤ x ≤ u of order D, found by walking the
// Stern–Brocot tree in batched (semiconvergent) steps, then picked by M.
// floor / ceil / trunc take a side; nearest the closer one, a tie away from
// zero; half_even the closer one, a tie to the even numerator (else the
// smaller denominator). Products are exact in twice x's width.
template <round_mode M, std::size_t K>
constexpr wide_rational<K> round_to_denominator(const wide_rational<K>& x, const wide_sint<K>& D) noexcept {
    using I        = wide_sint<2 * K + 2>;
    const bool neg = x.Num.negative();
    const I    n{neg ? -x.Num : x.Num}, d{x.Den}, dmax{D};
    I          lp = n / d, lq{1}, up = lp + I{1}, uq{1}; // floor and ceil of |x|
    if (lp * d == n)
        return x; // an integer
    for (;;) {
        const I mp = lp + up, mq = lq + uq;
        if (dmax < mq)
            break;
        if (n * mq < mp * d) { // |x| below the mediant: bring u down
            const I a = up * d - n * uq, b = n * lq - lp * d;
            I       k = (a - I{1}) / b;
            if (const I kd = (dmax - uq) / lq; kd < k)
                k = kd;
            up = up + k * lp;
            uq = uq + k * lq;
        } else if (mp * d < n * mq) { // above: bring l up
            const I a = n * lq - lp * d, b = up * d - n * uq;
            I       k = (a - I{1}) / b;
            if (const I kd = (dmax - lq) / uq; kd < k)
                k = kd;
            lp = lp + k * up;
            lq = lq + k * uq;
        } else
            return x; // |x| is the mediant: on the bound already
    }
    // Pick on |x|: floor of a negative x is the ceil of |x|, and so on.
    bool upper;
    if constexpr (M == round_mode::floor)
        upper = neg;
    else if constexpr (M == round_mode::ceil)
        upper = !neg;
    else if constexpr (M == round_mode::trunc)
        upper = false;
    else {
        const I below = (n * lq - lp * d) * uq, above = (up * d - n * uq) * lq; // distances · d·lq·uq
        if (below != above)
            upper = above < below;
        else if constexpr (M == round_mode::nearest)
            upper = true; // away from zero
        else
            upper = (up.Word[0] & 1u) == 0 ? true : (lp.Word[0] & 1u) == 0 ? false : uq < lq;
    }
    const I p = upper ? up : lp, q = upper ? uq : lq;
    return {static_cast<wide_sint<K>>(neg ? -p : p), static_cast<wide_sint<K>>(q)};
}

// v in lowest terms as the fraction raw F, or nothing when it needs more
// limbs than F has.
template <typename F, std::size_t K>
constexpr std::optional<F> continuous_raw_of(const wide_rational<K>& v) noexcept {
    const auto r      = reduced(v);
    using I           = decltype(F::Num);
    constexpr int cap = I::bits - 1; // magnitude bits
    if (bit_width_of(abs(r.Num)) > cap || bit_width_of(r.Den) > cap)
        return std::nullopt;
    return F{static_cast<I>(r.Num), static_cast<I>(r.Den)};
}

// b's value in lowest terms, in the fewest limbs that hold every value of B.
template <insidable B>
constexpr auto reduced_exact(const B& b) {
    constexpr std::size_t K = limbs_for_bits(exact_value_bits<B> + 1);
    const auto            r = reduced(exact_of(b));
    return wide_rational<K>{static_cast<wide_sint<K>>(r.Num), static_cast<wide_sint<K>>(r.Den)};
}

// A double whose magnitude is at least 2^64 (so an integer), as an exact
// value for a store into L. Past L's magnitude only its side matters, so a
// value just beyond the grid stands in.
template <insidable L>
constexpr wide_rational<exact_limbs<L>> exact_of_large(double d) noexcept {
    using I           = wide_sint<exact_limbs<L>>;
    constexpr int k   = grid_magnitude_bits<L> > 64 ? grid_magnitude_bits<L> : 64;
    const bool    neg = d < 0;
    const double  m   = neg ? -d : d;
    if (!(m < ldexp(1.0, k)))
        return {neg ? -(I{1} << (k + 1)) : I{1} << (k + 1), I{1}};
    const auto [mant, e] = double_parts(m);
    const I v            = I{mant} << e; // e ≥ 12
    return {neg ? -v : v, I{1}};
}

// A finite double's exact value: m·2^e, with 2^1024 and 2^-1074 in reach.
constexpr wide_rational<18> exact_of_double(double d) noexcept {
    using I              = wide_sint<18>;
    const auto [mant, e] = double_parts(d);
    const I n            = e >= 0 ? I{mant} << e : I{mant};
    return {d < 0 ? -n : n, e >= 0 ? I{1} : I{1} << -e};
}

// A wider floating point's exact value (long double): the sum of the doubles
// it splits into, each the rounding error of the last, over 2^1130 (below
// the least subnormal's 2^-1074 · 2^-53 mantissa scale). Past the double
// range a stand-in: ±2^1100 beyond every limit, or ±2^-1130 for a part below
// the subnormals (below every notch, so it only rounds).
template <std::floating_point T>
constexpr wide_rational<36> exact_of_long_double(T v) noexcept {
    using I             = wide_sint<36>;
    constexpr int shift = 1130;
    I             num{0};
    bool          past = false;
    const T       rem  = for_each_double_part(v, [&](double d) {
        if (!is_finite(d))
            return !(past = true);
        const auto [mant, e] = double_parts(d);
        const I m            = I{mant} << (e + shift);
        num += d < 0 ? -m : m;
        return true;
    });
    if (past)
        return {rem < 0 ? -(I{1} << 1100) : I{1} << 1100, I{1}};
    if (rem != 0)
        num += rem < 0 ? I{-1} : I{1};
    return reduced(wide_rational<36>{num, I{1} << shift});
}

// The nearest T to an exact value (ties to even), rounded once from the
// exact quotient: its top digits + 2 bits and a sticky bit, then scaled by
// exact powers of two. For a T wider than double (long double), whose
// digits a double would drop. Past T's range ±infinity, below half its least
// subnormal ±0.
template <std::floating_point T, std::size_t K>
constexpr T nearest_float(const wide_rational<K>& f) noexcept {
    using U           = wide_uint<2 * K + 2>; // n·2^s with s up to digits + 2 + the denominator's bits
    using lim         = std::numeric_limits<T>;
    constexpr int p   = lim::digits;
    const bool    neg = f.Num.negative() != f.Den.negative();
    const U       n{static_cast<wide_uint<K>>(abs(f.Num))};
    const U       d{static_cast<wide_uint<K>>(abs(f.Den))};
    if (n.is_zero())
        return neg ? -T{0} : T{0};
    // q = ⌊n·2^s / d⌋ has p + 2 or p + 3 bits: |v| = (q + sticky)·2^-s.
    const int  s  = p + 2 - (bit_width_of(n) - bit_width_of(d));
    const U    nn = s >= 0 ? n << s : n, dd = s >= 0 ? d : d << -s;
    const U    q      = nn / dd;
    const bool sticky = !(nn - q * dd).is_zero();
    const int  bw     = bit_width_of(q);
    const int  e      = bw - 1 - s; // |v| in [2^e, 2^(e+1))
    if (e > lim::max_exponent - 1)
        return neg ? -lim::infinity() : lim::infinity();
    // Below the normal range a subnormal keeps fewer digits.
    const int keep = e >= lim::min_exponent - 1 ? p : p - (lim::min_exponent - 1 - e);
    if (keep < 0)
        return neg ? -T{0} : T{0};
    const int drop = bw - keep; // ≥ 2
    U         m    = q >> drop;
    const U   low = q - (m << drop), half = U{1} << (drop - 1);
    if (half < low || (low == half && (sticky || static_cast<umax>(m) % 2 != 0)))
        m += U{1};
    if (bit_width_of(m) - 1 - s + drop > lim::max_exponent - 1) // rounded up past the largest finite
        return neg ? -lim::infinity() : lim::infinity();
    // m ≤ 2^p is exact in T, limb by limb; the scaling by 2^(drop − s) in
    // steps of 2^±64 stays exact (every step lies between m and the result).
    T x = std::accumulate(std::rbegin(m.Word), std::rend(m.Word), T{0}, [](T acc, umax w) {
        return acc * static_cast<T>(0x1p64) + static_cast<T>(w);
    });
    for (int k = drop - s; k != 0;) {
        const int step = k > 64 ? 64 : k < -64 ? -64 : k;
        const T   t    = pow_by_squaring(
            step < 0 ? static_cast<T>(0.5) : static_cast<T>(2),
            static_cast<unsigned>(step < 0 ? -step : step),
            [](T a, T b) { return a * b; },
            T{1});
        x *= t;
        k -= step;
    }
    return neg ? -x : x;
}

// Truncation toward zero, as an integer.
template <std::size_t K>
constexpr wide_sint<K> trunc(const wide_rational<K>& f) noexcept {
    return f.Num / f.Den;
}

// The reduced value as a 64-bit rational, or overflow when it does not fit.
template <std::size_t K>
constexpr std::expected<rational, errc> try_rational(const wide_rational<K>& f) noexcept {
    using I        = wide_sint<K>;
    const auto r   = reduced(f);
    const bool neg = r.Num.negative();
    const I    a   = neg ? -r.Num : r.Num;
    if (a > I{std::numeric_limits<umax>::max()} || r.Den > I{std::numeric_limits<imax>::max()})
        return std::unexpected{errc::overflow};
    const imax den = static_cast<imax>(r.Den);
    return rational{static_cast<umax>(a), neg ? -den : den};
}

// Text → exact value, for values that outgrow the 64-bit rational parse
// (parse falls back here on errc::overflow): the grammar of parse_text
// (scan_number: radix prefixes, ' separators, '.', e / p exponents, N/D),
// built in K limbs. Anything K limbs cannot hold reports overflow.
template <std::size_t K>
constexpr std::expected<wide_rational<K>, errc> parse_exact(const char* first, const char* last) noexcept {
    using I           = wide_sint<K>;
    constexpr I limit = std::numeric_limits<I>::max() / I{32}; // room for ·16 + a digit
    const auto  big   = [&](const I& x) { return limit < x; };
    auto        one   = [&](const char* f, const char* l) -> std::expected<wide_rational<K>, errc> {
        I                    num{0}, den{1};
        int                  frac_zeros = 0; // deferred, as in parse_number: trailing zeros cost nothing
        const scanned_number sc         = scan_number(f, l, [&](int d, bool in_frac, int b) {
            const I base{b};
            if (in_frac && d == 0) {
                ++frac_zeros;
                return true;
            }
            for (; frac_zeros > 0; --frac_zeros) {
                if (big(num) || big(den))
                    return false;
                num = num * base;
                den = den * base;
            }
            if (big(num) || big(den))
                return false;
            num = num * base + I{d};
            if (in_frac)
                den = den * base;
            return true;
        });
        if (sc.Fail != parse_fail::none)
            return std::unexpected{errc::invalid_format};
        if (sc.Stopped)
            return std::unexpected{errc::overflow};
        if (num.is_zero())
            return wide_rational<K>{I{0}, I{1}};
        if (sc.HasP) { // ·2^±Exp
            I& t = sc.ExpNeg ? den : num;
            if (bit_width_of(t) + sc.Exp > I::bits - 2)
                return std::unexpected{errc::overflow};
            t = t << sc.Exp;
        } else if (sc.HasE) // ·10^±Exp
            for (int e = 0; e < sc.Exp; ++e) {
                I& t = sc.ExpNeg ? den : num;
                if (big(t))
                    return std::unexpected{errc::overflow};
                t = t * I{10};
            }
        return wide_rational<K>{num, den};
    };
    return parse_fraction(first, last, one, [](const wide_rational<K>& d) { return d.Num.is_zero(); });
}

// The text parse's width: wide enough for about 1200 decimal digits or a
// 10^±1200 exponent, past every grid's own values.
inline constexpr std::size_t parse_limbs = 64;

// Bits of B's finest denominator: every grid value and every midpoint
// between two lies 0 or at least 2^−(bits + 1) from 0. An unbounded
// continuous grid is bounded by its storage.
template <insidable B>
inline constexpr int finest_den_bits = [] {
    const grid_int d = den_bound(grid_of<B>);
    return d == grid_int{0} ? 64 * static_cast<int>(exact_limbs<B>) : bit_width_of(d);
}();

// Bounds on log2|v| of one number in the literal grammar (lo ≤ log2|v| < hi),
// from its digits and exponent alone; empty for malformed text or zero.
struct log2_bounds {
    long Lo, Hi;
};
constexpr std::optional<log2_bounds> number_log2_bounds(const char* first, const char* last) noexcept {
    long                 sig = 0, frac = 0; // significant digits, digits after the point
    const scanned_number sc = scan_number(first, last, [&](int d, bool in_frac, int) {
        if (sig > 0 || d != 0)
            ++sig;
        if (in_frac)
            ++frac;
        return true;
    });
    if (sc.Fail != parse_fail::none || sig == 0)
        return std::nullopt;
    // |v| = M·base^−frac·(10 or 2)^±Exp, base^(sig−1) ≤ M < base^sig.
    auto up = [&](long n) { return sc.Base == 10 ? (n * 3322 + 999) / 1000 : sc.Base == 16 ? 4 * n : n; };
    auto dn = [&](long n) { return sc.Base == 10 ? n * 3321 / 1000 : sc.Base == 16 ? 4 * n : n; };
    long lo = dn(sig - 1) - up(frac), hi = up(sig) - dn(frac);
    if (sc.HasP) {
        const long e = sc.ExpNeg ? -long{sc.Exp} : long{sc.Exp};
        lo += e;
        hi += e;
    } else if (sc.HasE) {
        const long e = long{sc.Exp};
        lo += sc.ExpNeg ? -(e * 3322 + 999) / 1000 : e * 3321 / 1000;
        hi += sc.ExpNeg ? -(e * 3321 / 1000) : (e * 3322 + 999) / 1000;
    }
    return log2_bounds{lo, hi};
}

// A number past the wide parse (1e-5000, 1e5000, either as N/D) still has
// a store. One nearer 0 than every value and midpoint of B rounds and
// range-checks like any other such value of its sign: ±2^−(finest_den_bits
// + 2) stands in. One past every limit of B overflows, or clamps onto the
// bound, like any other: ±2^(exact_value_bits + 2) stands in — unless Wrap,
// whose fold needs the exact value. Empty unless the text's magnitude,
// bounded from its digits and exponents, is that small or that large.
template <insidable B, std::size_t K, bool Wrap = false>
constexpr std::optional<wide_rational<K>> stand_in(const char* first, const char* last) noexcept {
    constexpr int G            = finest_den_bits<B> + 2;
    constexpr int H            = exact_value_bits<B> + 2;
    const auto [neg, f, slash] = split_fraction(first, last);
    auto b                     = number_log2_bounds(f, slash);
    if (!b)
        return std::nullopt;
    if (slash != last) {
        const auto d = number_log2_bounds(slash + 1, last);
        if (!d)
            return std::nullopt; // malformed, or N/0: the parse reports it
        b = log2_bounds{b->Lo - d->Hi, b->Hi - d->Lo};
    }
    using I = wide_sint<K>;
    if constexpr (G < 64 * static_cast<int>(K) - 2)
        if (b->Hi <= -G)
            return wide_rational<K>{neg ? I{-1} : I{1}, I{1} << G};
    if constexpr (!Wrap && H < 64 * static_cast<int>(K) - 2)
        if (b->Lo >= H)
            return wide_rational<K>{neg ? -(I{1} << H) : I{1} << H, I{1}};
    return std::nullopt;
}

// n / d (d != 0) rounded to an integer by M; the sign rules of div_rounded.
// Exact: d divides n (from the same division).
template <std::size_t K>
struct rounded_quotient {
    wide_sint<K> Quot;
    bool         Exact;
};
template <round_mode M, std::size_t K>
constexpr rounded_quotient<K> rounded_div_exact(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    using I        = wide_sint<K>;
    auto [q, r]    = I::divmod(n, d); // toward zero
    const bool neg = n.negative() != d.negative();
    const I    ar = abs(r), ad = abs(d);
    if (!rounds_away(M, neg, classify_remainder(M, ar, ad), (q.Word[0] & 1u) != 0))
        return {q, r.is_zero()};
    return {neg ? q - I{1} : q + I{1}, false};
}
template <round_mode M, std::size_t K>
constexpr wide_sint<K> rounded_div(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    return rounded_div_exact<M>(n, d).Quot;
}

// The value index of f on L's lattice (f / Notch) rounded by M, minus the
// slot base: L's slot offset. Exact is false when f lies between notches.
template <std::size_t K>
struct exact_index_result {
    wide_sint<K> Index;
    bool         Exact;
};

template <insidable L, round_mode M, std::size_t K>
constexpr auto exact_index(const wide_rational<K>& f) noexcept {
    constexpr std::size_t KK = exact_max<K, exact_limbs<L>>;
    using I                  = wide_sint<KK>;
    const wide_rational<KK> g{f};
    if constexpr (anchored<L>) {
        const I    n = g.Num * static_cast<I>(wide_denominator(notch_of<L>));
        const I    d = g.Den * static_cast<I>(wide_numerator(notch_of<L>)); // > 0
        const auto q = rounded_div_exact<M>(n, d);                          // one division for both
        return exact_index_result<KK>{q.Quot - static_cast<I>(slot_base<L>), q.Exact};
    } else {
        // The offset (f − Lower)/Notch rounded in value space (rounds_up).
        const wide_rational<KK> o = g + -exact_of_grid<KK>(lower_of<L>);
        const I                 n = o.Num * static_cast<I>(wide_denominator(notch_of<L>));
        const I                 d = o.Den * static_cast<I>(wide_numerator(notch_of<L>)); // > 0 (Den > 0)
        const auto [q, r]         = floor_divmod(n, d);
        const bool up             = rounds_up(M,
                                              g.Num.negative(),
                                              classify_remainder(M, r, d),
                                              ((q + static_cast<I>(lower_index_wide<L>)).Word[0] & 1u) != 0);
        return exact_index_result<KK>{up ? q + I{1} : q, r.is_zero()};
    }
}

// x rounded by P's rounding to a fraction of L's bound (a bounded continuous
// grid: denominators ≤ MaxDenominator), limits aside.
template <insidable L, typename P, std::size_t K>
constexpr wide_rational<K> round_to_bound(const wide_rational<K>& x) {
    return round_to_denominator<rounding_for<L, P>>(
        x, static_cast<wide_sint<K>>(wide_numerator(grid_of<L>.MaxDenominator)));
}

template <insidable L, typename P>
constexpr bool exact_rounds_into_range(const rational& v, rational& out) {
    if constexpr (!notched<L>) { // a bounded continuous grid: onto its bound
        // Near a limit it fits, unless the bound itself is past imax.
        const auto r = try_rational(round_to_bound<L, P>(exact_of(v)));
        if (!r)
            return false;
        out = *r;
        return includes(interval_of<L>, out);
    } else {
        const auto j = exact_index<L, rounding_for<L, P>>(exact_of(v)).Index; // L's slot offset
        using I      = std::remove_cvref_t<decltype(j)>;
        if (j.negative() || I{max_index_of<L>} < j)
            return false;
        out = (lower64<L> + (rational{static_cast<umax>(j)} * notch64<L>).value()).value(); // L's point
        return true;
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
inline constexpr grid_int units_lo = exact_quotient(lower_of<X>, Unit);
template <insidable X, grid_rational Unit>
inline constexpr grid_int units_hi = exact_quotient(upper_of<X>, Unit);

// Whether from_value_in_units<Result, Unit> divides: the result is counted
// in a unit finer than its notch (or off its lattice's multiples).
template <insidable Result, grid_rational Unit>
inline constexpr bool divides_units = !(point_storage<Result> || (anchored<Result> && Unit == notch_of<Result>));

// The work type of an integer + or ×: signed imax when every value index
// involved (each operand in its unit, the result in its notch, and the
// result's slot offsets) provably fits — then nothing wraps, and the
// compiler keeps the value ranges, as the builtin paths always did — else
// the wrapping type, or, when a division follows (which a wrapped value
// cannot take), an exact signed one.
// URes: the unit the result is counted in (its notch, or a finer unit
// that also divides its Lower when the grids do not pass through 0); its
// slot offsets in that unit are what from_value_in_units divides.
template <insidable     Result,
          insidable     L,
          grid_rational UL,
          insidable     R,
          grid_rational UR,
          grid_rational URes = notch_of<Result>>
inline constexpr int index_work_bits =
    signed_value_bits_of({units_lo<L, UL>,
                          units_hi<L, UL>,
                          units_lo<R, UR>,
                          units_hi<R, UR>,
                          units_lo<Result, URes>,
                          units_hi<Result, URes>,
                          grid_of<Result>.slot_count() * exact_quotient(notch_of<Result>, URes)});
template <insidable     Result,
          insidable     L,
          grid_rational UL,
          insidable     R,
          grid_rational UR,
          grid_rational URes = notch_of<Result>>
using index_work_t = std::conditional_t<(index_work_bits<Result, L, UL, R, UR, URes> <= 63),
                                        imax,
                                        std::conditional_t<divides_units<Result, URes>,
                                                           work_int_t<index_work_bits<Result, L, UL, R, UR, URes>>,
                                                           wrap_work_t<Result>>>;

template <typename W, integer_storage X>
constexpr W value_index(const X& x) noexcept {
    if constexpr (index_storage<X> && std::is_integral_v<W>) {
        // In W's unsigned twin: a raw past W's max (a slot count past 2^63
        // from a negative base) wraps back onto the index, which fits W.
        using U = std::make_unsigned_t<W>;
        return static_cast<W>(static_cast<U>(static_cast<W>(slot_base<X>)) + static_cast<U>(x.raw()));
    } else if constexpr (index_storage<X>)
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
        constexpr grid_rational U = gcd(unit_of<L>, unit_of<R>);
        return signed_value_bits_of({units_lo<L, U>, units_hi<L, U>, units_lo<R, U>, units_hi<R, U>}) <= 63;
    }
}();

// x's value in units of `Unit` (an integer: the unit divides x's notch and
// Lower, or x's value for a point).
template <typename W, grid_rational Unit, insidable X>
constexpr W value_in_units(const X& x) noexcept {
    // A point (Lower == Upper) holds its value in the type.
    if constexpr (point_grid<X>) {
        constexpr grid_int q = exact_quotient(lower_of<X>, Unit);
        return static_cast<W>(q);
    } else if constexpr (!anchored<X>) {
        // An index raw (integer value storage has integer values): Lower in
        // units, plus raw notches of `scale` units each.
        // raw·scale, the offset from Lower, can pass W while the value fits:
        // an integral W computes in its unsigned twin, as value_index does.
        constexpr grid_int base = exact_quotient(lower_of<X>, Unit), scale = exact_quotient(notch_of<X>, Unit);
        if constexpr (std::is_integral_v<W>) {
            using U = std::make_unsigned_t<W>;
            return static_cast<W>(static_cast<U>(static_cast<W>(base)) +
                                  static_cast<U>(x.raw()) * static_cast<U>(static_cast<W>(scale)));
        } else
            return static_cast<W>(base) + static_cast<W>(x.raw()) * static_cast<W>(scale);
    } else {
        constexpr grid_int scale = exact_quotient(notch_of<X>, Unit);
        if constexpr (scale == grid_int{1})
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
    if constexpr (!divides_units<Result, Unit>)
        return from_value_index<Result>(j);
    else if constexpr (index_storage<Result>) {
        constexpr grid_int base  = exact_quotient(lower_of<Result>, Unit),
                           scale = exact_quotient(notch_of<Result>, Unit);
        return Result::from_raw(static_cast<raw_t<Result>>((j - static_cast<W>(base)) / static_cast<W>(scale)));
    } else // integer values counted in Unit = 1/k
        return Result::from_raw(
            static_cast<raw_t<Result>>(j / static_cast<W>(exact_quotient(grid_rational{1}, Unit))));
}

// Exact result of grid arithmetic: the value is on the result lattice and
// inside its interval by construction, so it maps straight to a raw.
template <insidable Result, std::size_t K>
constexpr Result exact_result(const wide_rational<K>& v) noexcept {
    return Result::from_raw(raw_of_slot<Result>(exact_index<Result, round_mode::trunc>(v).Index));
}
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/policy.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/assignment.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/assign_exact.hpp
// ======================================================================




// policy.hpp includes this header: the recording policy of the fine-double
// store (an errc& one) is named here and completed there.
namespace beman::inside {
namespace detail {
struct error_ref;
} // namespace detail
template <policy_flag W, typename E>
struct policy;
} // namespace beman::inside

namespace beman::inside::detail {
//---------------------------------------------------------------------------
// The exact store (assign_exact) and what every assignment path shares: the
// out-of-range cascade, failure reporting, the on_wrap carry grid and the
// same-notch wide raw mapping. The source-type specializations are in
// assignment.hpp.
//---------------------------------------------------------------------------
// needs_runtime_range_check<L, P, A>: true iff any out-of-range handler would
// fire (an action, a clamp/wrap bit, or default-throw under checked).
// When false (typically `unsafe`, no action) the runtime range branch in
// `assign` is dead code and skipped, letting the autovectorizer kick in.
//---------------------------------------------------------------------------
template <insidable L, typename P, typename A>
inline constexpr bool needs_runtime_range_check =
    clamp_action<plain_t<A>> || wrap_action<plain_t<A>> || error_action<plain_t<A>> ||
    range_handled(flags_of<L> | plain_t<P>::Flags);

// Shared out-of-range policy cascade. Order: clamp/wrap *actions*, then
// clamp/wrap *policy* bits, then an on_error action, then `range_fail`. The
// callers say how clamp and wrap store; `Wrappable` is false on the
// fractional path (no wrap *action* branch). Returns true when a handler
// resolved the write.
template <bool Wrappable, insidable L, typename P, typename A, typename DoClamp, typename DoWrap>
constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action, DoClamp do_clamp, DoWrap do_wrap) {
    using PA = plain_t<A>;
    if constexpr (clamp_action<PA>) {
        do_clamp();
        return true;
    } else if constexpr (Wrappable && wrap_action<PA>) {
        do_wrap();
        return true;
    } else if constexpr (has_policy<L, P, clamp>) {
        do_clamp();
        return true;
    } else if constexpr (has_policy<L, P, wrap>) {
        do_wrap();
        return true;
    } else if constexpr (error_action<PA>) { // on_error replaces the failure, not a clamp or wrap
        action.Fn(lhs, errc::overflow, errc_message(errc::overflow));
        return true;
    } else
        return range_fail(policy);
}

// A failure goes to an on_error action when there is one, else the policy.
template <typename L, typename P, typename A>
constexpr void report_failure(L& lhs, P& policy, A& action, errc code) {
    if constexpr (error_action<plain_t<A>>)
        action.Fn(lhs, code, errc_message(code));
    else
        policy.report(code);
}

//---------------------------------------------------------------------------
// The on_wrap carry is always an inside whose grid holds every carry the
// source kind can produce: an inside source uses its own range, an
// integral source its type's limits, a fractional (double / rational) source the whole imax range. So
// `minutes += carry` compiles for every source, and a callback taking `imax`
// still binds through the implicit operator imax(). A bound whose exact
// computation leaves imax falls back to that side of the imax range.
//---------------------------------------------------------------------------
template <insidable L, typename R>
constexpr grid wrap_carry_grid() {
    constexpr imax imax_lo = std::numeric_limits<imax>::min();
    constexpr imax imax_hi = std::numeric_limits<imax>::max();
    // A range past the rational range (a grid spanning 2^64 or more) has
    // carries in {−1, 0, 1} at most: the full imax grid holds them.
    constexpr auto span_r = try_sub(upper64<L>, lower64<L>);
    if constexpr (!span_r || !try_add(*span_r, notch64<L>))
        return grid{imax_lo, imax_hi};
    else if constexpr (std::integral<R> || insidable<R>) {
        // An integral source spans its type's limits, an inside its interval.
        constexpr rational src_lo = [] {
            if constexpr (insidable<R>)
                return lower64<R>;
            else
                return rational{std::numeric_limits<R>::min()};
        }();
        constexpr rational src_hi = [] {
            if constexpr (insidable<R>)
                return upper64<R>;
            else
                return rational{std::numeric_limits<R>::max()};
        }();
        const rational range    = *try_add(*span_r, notch64<L>);
        auto           carry_of = [&](rational v, imax fallback) -> imax {
            const auto off = try_sub(v, lower64<L>);
            if (!off)
                return fallback;
            const auto q = try_div(*off, range);
            if (!q || *q < rational{imax_lo} || *q > rational{imax_hi})
                return fallback;
            return floor(*q);
        };
        return grid{carry_of(src_lo, imax_lo), carry_of(src_hi, imax_hi)};
    } else
        return grid{imax_lo, imax_hi};
}

template <insidable L, typename R>
constexpr auto make_wrap_carry(imax q) {
    beman::inside::inside<wrap_carry_grid<L, R>()> carry;
    from_value(carry, q); // in the carry grid by construction
    return carry;
}

// d saturated to imax: a clamp overshoot or a wrap carry, from a work type W
// (imax or a wide integer).
template <typename W>
constexpr imax saturate_imax(const W& d) noexcept {
    constexpr W lo = static_cast<W>(std::numeric_limits<imax>::min());
    constexpr W hi = static_cast<W>(std::numeric_limits<imax>::max());
    return static_cast<imax>(d < lo ? lo : hi < d ? hi : d);
}

//---------------------------------------------------------------------------
// unit_fold — clamp/wrap of an integer value v in [SrcLo, SrcHi] onto a
// unit-notch integer grid L, exactly. v − Lower and the range
// Upper − Lower + 1 may need 65 bits (a grid can span 2^64 values), so the
// arithmetic runs in a work type sized from all of them: imax when that
// suffices, else a wide_int.
//---------------------------------------------------------------------------
template <insidable L, grid_int SrcLo, grid_int SrcHi>
struct unit_fold {
    static constexpr grid_int lo = wide_numerator(lower_of<L>);
    static constexpr grid_int hi = wide_numerator(upper_of<L>);
    using W = work_int_t<signed_value_bits_of({SrcLo, SrcHi, lo, hi, SrcLo - hi, SrcHi - lo, hi - lo + grid_int{1}})>;

    static constexpr W lower = static_cast<W>(lo);
    static constexpr W upper = static_cast<W>(hi);
    static constexpr W span  = static_cast<W>(hi - lo);

    // v = Lower + carry·(span + 1) + offset with 0 ≤ offset ≤ span.
    struct folded {
        imax Carry;
        W    Offset;
    };
    static constexpr folded fold(const W& v) noexcept {
        const auto [q, w] = floor_divmod(v - lower, span + W{1});
        return {saturate_imax(q), w};
    }
};

// The integer range of a source: its type's limits, or ±2^64 (every
// integer a 64-bit rational or a grid value can be).
template <typename R>
inline constexpr grid_int source_lo = [] {
    if constexpr (std::integral<R>)
        return grid_int{std::numeric_limits<R>::min()};
    else
        return -(grid_int{1} << 64);
}();
template <typename R>
inline constexpr grid_int source_hi = [] {
    if constexpr (std::integral<R>)
        return grid_int{std::numeric_limits<R>::max()};
    else
        return grid_int{1} << 64;
}();

// A value off a bounded continuous grid (its reduced denominator past
// MaxDenominator) rounded onto it by P's rounding, kept within [Lower,
// Upper]: a limit itself off the bound moves to the nearest fraction inside.
// Empty when no fraction of the bound lies in range.
template <insidable L, typename P, std::size_t K>
constexpr std::optional<wide_rational<exact_max<K, exact_limbs<L>>>> onto_bound(const wide_rational<K>& v) noexcept {
    constexpr std::size_t KK = exact_max<K, exact_limbs<L>>;
    using F                  = wide_rational<KK>;
    using W                  = wide_sint<KK>;
    const W D                = static_cast<W>(wide_numerator(grid_of<L>.MaxDenominator));
    const F lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
    F       r = round_to_bound<L, P>(F{v});
    if (r < lo)
        r = round_to_denominator<round_mode::ceil>(lo, D);
    else if (hi < r)
        r = round_to_denominator<round_mode::floor>(hi, D);
    if (r < lo || hi < r)
        return std::nullopt;
    return r;
}

//---------------------------------------------------------------------------
// assign_exact — store an exact value into L: the path wherever neither imax
// nor the 64-bit rational holds every value (a wide raw on either side, fine
// doubles, mul_into/div_into, parse, sum, wrap_exact). Rounds first, then range-checks,
// like the builtin paths; out of range runs the usual policy cascade.
//---------------------------------------------------------------------------
// R is the source type: an on_clamp action gets the overshoot and an
// on_wrap action the carry in the same shape as on the builtin paths.
template <typename R, insidable L, typename P, typename A, std::size_t K>
constexpr L& assign_exact(L& lhs, const wide_rational<K>& v, P&& policy, A&& action) {
    auto fail = [&](errc code) { report_failure(lhs, policy, action, code); };
    if constexpr (fraction_storage<L>) {
        // A continuous grid past 64 bits: the value itself, reduced, when it
        // lies within the limits and fits the raw's limbs.
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(false, "on_clamp / on_wrap: not supported on a continuous grid past 64 bits");
        constexpr std::size_t   KK = exact_max<K, exact_limbs<L>>;
        const wide_rational<KK> lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
        auto                    store = [&](const wide_rational<KK>& x) {
            // A bounded grid: a denominator past its bound is off the grid —
            // rounded onto it under a rounding policy, else rounding_error.
            if constexpr (grid_of<L>.MaxDenominator != 0) {
                constexpr wide_rational<KK> bound = exact_of_grid<KK>(grid_of<L>.MaxDenominator);
                if (bound.Num < reduced(x).Den) [[unlikely]] {
                    if constexpr (has_policy<L, P, snap>) {
                        const auto o = onto_bound<L, plain_t<P>>(x);
                        if (!o)
                            return fail(errc::rounding_error);
                        if (const auto r = continuous_raw_of<raw_t<L>>(*o)) [[likely]]
                            lhs = L::from_raw(*r);
                        else
                            fail(errc::overflow);
                        return;
                    } else if (std::is_constant_evaluated() || is_checked(flags_of<L> | plain_t<P>::Flags))
                        return fail(errc::rounding_error);
                }
            }
            if (const auto r = continuous_raw_of<raw_t<L>>(x)) [[likely]]
                lhs = L::from_raw(*r);
            else
                fail(errc::overflow);
        };
        wide_rational<KK> x{v};
        // Round first, as the builtin stores do: a value just past a limit
        // may round onto it.
        if constexpr (grid_of<L>.MaxDenominator != 0 && has_policy<L, P, snap>)
            x = round_to_bound<L, plain_t<P>>(x);
        if (x < lo || hi < x) [[unlikely]] {
            dispatch_out_of_range<true>(
                lhs,
                policy,
                action,
                [&] { store(x < lo ? lo : hi); },
                [&] {
                    // x − q·span with q = ⌊(x − lo)/span⌋: into [lo, hi).
                    const wide_rational<KK> span = hi + -lo, t = (x + -lo) / span;
                    const auto              q = floor_divmod(t.Num, t.Den).Quot;
                    store(x + -(wide_rational<KK>{q, wide_sint<KK>{1}} * span));
                });
            return lhs;
        }
        store(x);
        return lhs;
    } else if constexpr (rational_storage<L>) {
        // L holds 64-bit values: narrow through the rational. A value that does
        // not fit (a double finer than 2^-62, an exact sum) may still lie
        // within the limits: a bounded grid rounds it onto its bound first,
        // clamp saturates it past a limit; else it is off a bounded grid
        // (rounding_error) or does not fit (overflow).
        auto r = try_rational(v);
        if (!r) [[unlikely]] {
            constexpr std::size_t   KK = exact_max<K, exact_limbs<L>>;
            const wide_rational<KK> x{v};
            if constexpr (grid_of<L>.MaxDenominator != 0 && has_policy<L, P, snap>)
                r = try_rational(round_to_bound<L, plain_t<P>>(x));
            if (!r) {
                const bool low = x < exact_of_grid<KK>(lower_of<L>);
                if (!low && !(exact_of_grid<KK>(upper_of<L>) < x)) {
                    fail(grid_of<L>.MaxDenominator != 0 ? errc::rounding_error : errc::overflow);
                    return lhs;
                }
                if constexpr (!has_policy<L, P, clamp>) {
                    fail(errc::overflow);
                    return lhs;
                } else
                    r = low ? lower64<L> : upper64<L>;
            }
        }
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(
                false,
                "on_clamp / on_wrap: a source past the 64-bit rational into a rational inside is not supported");
        return assignment<L, rational>::assign(lhs, *r, policy, std::forward<A>(action));
    } else {
        // (Plain variables, not a structured binding: Clang rejects a binding
        // captured by the lambdas below in constant evaluation.)
        const auto slot = [&] {
            if constexpr (point_storage<L>) {
                // One value and no lattice to divide by: the slot below it,
                // its own, or the one above (the slot count is 0).
                constexpr std::size_t KP = exact_max<K, exact_limbs<L>>;
                using W                  = wide_sint<KP>;
                const auto c             = wide_rational<KP>{v} <=> exact_of_grid<KP>(lower_of<L>);
                return exact_index_result<KP>{c < 0 ? W{-1} : W{c > 0 ? 1 : 0}, true};
            } else
                return exact_index<L, rounding_for<L, plain_t<P>>>(v);
        }();
        using I                       = decltype(slot.Index);
        constexpr std::size_t KK      = sizeof(I) / sizeof(umax);
        const I               index   = slot.Index;
        const bool            inexact = !slot.Exact && !has_policy<L, P, snap> && policy.round_check();
        constexpr I           count   = static_cast<I>(grid_of<L>.slot_count());
        // Without a rounding flag the range test reads the value itself,
        // before the notch, as the builtin stores do: an off-notch value past
        // a limit is out of range (it clamps, or reports overflow).
        bool low = index.negative(), out = low || index > count;
        if constexpr (!has_policy<L, P, snap>)
            if (!slot.Exact) {
                const wide_rational<KK> x{v};
                low = x < exact_of_grid<KK>(lower_of<L>);
                out = low || exact_of_grid<KK>(upper_of<L>) < x;
            }
        if (out) [[unlikely]] {
            if (dispatch_out_of_range<true>(
                    lhs,
                    policy,
                    action,
                    [&] {
                        lhs = L::from_raw(raw_of_slot<L>(low ? I{0} : count));
                        if constexpr (clamp_action<plain_t<A>>) {
                            // The overshoot rhs − bound, shaped like the builtin paths'.
                            const auto over = v + wide_rational<KK>{I{-1}, I{1}} *
                                                      exact_of_grid<KK>(low ? lower_of<L> : upper_of<L>);
                            if constexpr (insidable<R>) {
                                action.Fn(lhs, exact_result<beman::inside::inside<grid_of<R> - grid_of<L>>>(over));
                            } else if constexpr (std::integral<R>)
                                action.Fn(lhs, saturate_imax(trunc(over)));
                            else if constexpr (std::floating_point<R>)
                                action.Fn(lhs, static_cast<R>(static_cast<double>(over)));
                            else
                                action.Fn(lhs, try_rational(over).value_or(rational{0}));
                        }
                    },
                    [&] {
                        if constexpr (point_storage<L>)
                            fail(errc::overflow); // a point has no period to fold by
                        else if (inexact)
                            fail(errc::rounding_error); // folds off the notch too
                        else {
                            const auto [q, w] = floor_divmod(index, count + I{1});
                            lhs               = L::from_raw(raw_of_slot<L>(w));
                            if constexpr (wrap_action<plain_t<A>>)
                                action.Fn(lhs, make_wrap_carry<L, R>(saturate_imax(q)));
                        }
                    }))
                return lhs;
        }
        if (inexact) [[unlikely]] {
            fail(errc::rounding_error);
            return lhs;
        }
        lhs = L::from_raw(raw_of_slot<L>(index));
        return lhs;
    }
}

//---------------------------------------------------------------------------
// Same-notch raw mapping for wide assignments: R's raw plus a constant is L's
// raw (value index J = raw + slot base for an index raw, J = raw for a value
// raw).
//---------------------------------------------------------------------------
template <insidable L, insidable R>
inline constexpr bool same_notch_raws =
    notched<L> && notch_of<L> == notch_of<R> && !point_storage<L> && !point_storage<R> && // ⇒ integer raws
    grid_divides_evenly(lower_of<R> - lower_of<L>, notch_of<L>);                          // one lattice
// The shift is the exact offset between the Lowers in notches, plus the raws
// of the Lowers (0 for an index raw, Lower for a value raw): exact on
// unanchored lattices too.
template <insidable L, insidable R>
inline constexpr grid_int same_notch_shift =
    exact_quotient(lower_of<R> - lower_of<L>, notch_of<L>) + raw_lo_exact<L> - raw_lo_exact<R>;
template <insidable L, insidable R>
inline constexpr int same_notch_bits = signed_value_bits_of({raw_lo_exact<R>,
                                                             raw_hi_exact<R>,
                                                             raw_lo_exact<R> + same_notch_shift<L, R>,
                                                             raw_hi_exact<R> + same_notch_shift<L, R>,
                                                             raw_lo_exact<L>,
                                                             raw_hi_exact<L>});
} // namespace beman::inside::detail



namespace beman::inside::detail {
//---------------------------------------------------------------------------
// assignment — narrowing/coercion between bounded and arithmetic types. Three
// specializations dispatch on the source (integral / fractional / insidable);
// each stores an in-range value and hands an out-of-range one to the policy
// (`apply_clamp` / `apply_wrap`, or a report). The insidable path also exposes
// `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// assign(insidable, integral)
//---------------------------------------------------------------------------
template <insidable L, std::integral R>
struct assignment<L, R> {
  private:
    // Exact integer arithmetic for the bounds and the cold clamp/wrap paths:
    // an integer interval may reach past int64 on either side (e.g.
    // {0, 2^64−1}), and rhs − Lower may need 65 bits.
    using fold = unit_fold<L, source_lo<R>, source_hi<R>>;
    using W    = typename fold::W;

    // Hot-path range test. Bounds within imax compare as plain integers; a
    // 64-bit unsigned source can exceed imax and compares sign-safe instead.
    // (Not std::cmp_* throughout: GCC then misses folding `lo ≤ x ≤ hi` into
    // one unsigned compare.) Bounds past int64 compare exactly in the fold's
    // work type.
    static constexpr bool out_of_range(R rhs) noexcept {
        if constexpr (fits_imax(interval_of<L>)) {
            constexpr imax lower = lower_imax<L>, upper = upper_imax<L>;
            if constexpr (std::is_unsigned_v<R> && sizeof(R) >= sizeof(imax))
                return std::cmp_less(rhs, lower) || std::cmp_greater(rhs, upper);
            else
                return static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper;
        } else {
            const W v = static_cast<W>(rhs);
            return v < fold::lower || fold::upper < v;
        }
    }

    template <typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, A&& action) {
        // Pre: rhs is out of [Lower, Upper] (only called from handle_out_of_range),
        // so the two-way pick is the full clamp.
        const W    v   = static_cast<W>(rhs);
        const bool low = v < fold::lower;
        lhs            = L::from_raw(raw_of_slot<L>(low ? W{0} : fold::span));
        if constexpr (clamp_action<plain_t<A>>)
            action.Fn(lhs, saturate_imax(v - (low ? fold::lower : fold::upper)));
    }

    template <typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, A&& action) {
        // Modular wrap on the exact offset rhs − Lower into span + 1 slots. The
        // carry saturates at imax, like the carry grid (wrap_carry_grid).
        const auto [carry, w] = fold::fold(static_cast<W>(rhs));
        lhs                   = L::from_raw(raw_of_slot<L>(w));
        if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(carry));
    }

    template <typename P, typename A>
    static constexpr bool handle_out_of_range(L& lhs, R rhs, P&& policy, A&& action) {
        return dispatch_out_of_range<true>(
            lhs, policy, action, [&] { apply_clamp(lhs, rhs, action); }, [&] { apply_wrap(lhs, rhs, action); });
    }

    static constexpr void store(L& lhs, R rhs) {
        if constexpr (value_storage<L>)
            lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (point_grid<L>)
            lhs = L::from_raw(0); // a point grid: 0 is the only offset
        else if constexpr (qformat_codec_fits<L>)
            lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage on a notch 1/K grid: the offset is an exact integer
        {
            rational raw = ((rhs - lower64<L>) / notch64<L>).value();
            lhs          = L::from_raw(raw_cast<L>(raw.Numerator));
        }
    }

  public:
    // An integer lands on L's grid whenever the notch is 1/K over an integer
    // Lower (or the grid is continuous); otherwise it may fall between notches
    // and must round or report exactly like the same value given as a rational.
    static constexpr bool integers_on_grid =
        !notched<L> || (notch64<L>.Numerator == 1 && abs_den(lower64<L>.Denominator) == 1);

    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // wrap/clamp bring any value into range (matches assign_intervals_ok).
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                          not excludes(interval_of<L>, interval_of<R>),
                      "rhs type's range lies entirely outside lhs interval and the policy cannot bring it into range");

        if constexpr (wide_valued<L>)
            return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
        else if constexpr (!integers_on_grid)
            return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
        else {
            // The out-of-range check runs unconditionally — clamp/wrap
            // policies handle it via apply_*, which is constexpr-clean. Only the
            // unhandled-checked path winds up calling `policy.report`, which
            // contains its own `std::is_constant_evaluated()` guard.
            if constexpr (not includes(interval_of<L>, interval_of<R>)) {
                if constexpr (integer_limits<L>) {
                    // Skip the runtime range branch entirely when every handler would
                    // be dead anyway — the dead branch otherwise inhibits autovec.
                    if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>) {
                        if (out_of_range(rhs)) [[unlikely]] {
                            // The integer clamp/wrap formulas need consecutive integers to be
                            // adjacent grid points (notch 1); a finer notch wraps modulo
                            // span + notch on the rational path.
                            if constexpr (notch64<L> == 1) {
                                if (handle_out_of_range(lhs, rhs, policy, action))
                                    return lhs;
                            } else
                                return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                        }
                    }
                } else if (not includes(interval_of<L>, rhs)) {
                    // Non-integer L bounds: route through the rational path so fractional
                    // Lower/Upper drive clamp/error correctly.
                    return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                }
            }

            store(lhs, rhs);
            return lhs;
        }
    }
};

//---------------------------------------------------------------------------
// The integer store of an unanchored lattice: Notch = S/K and Lower = M/K
// over their common denominator K. MaxDen bounds a source denominator so
// that num·K, M·aden and aden·S stay in imax (|num| ≤ |value|·aden, and
// every in-range |value| is at most Mag); Ok when the grid allows any.
//---------------------------------------------------------------------------
struct unanchored_codec_t {
    imax K, S, M;
    umax MaxDen;
    bool Ok;
};
template <insidable L>
inline constexpr unanchored_codec_t unanchored_codec = [] {
    constexpr unanchored_codec_t no{0, 0, 0, 0, false};
    if constexpr (anchored<L> || point_storage<L> || !values_fit_imax<L>) // unanchored ⇒ index raw
        return no;
    else {
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        const rational n = notch64<L>, lo = lower64<L>, hi = upper64<L>;
        const umax     dn = abs_den(n.Denominator), dl = abs_den(lo.Denominator);
        umax           k;
        if (mul_overflow(dn / std::gcd(dn, dl), dl, &k) || k > cap)
            return no;
        umax s, mm;
        if (mul_overflow(n.Numerator, k / dn, &s) || s > cap || mul_overflow(lo.Numerator, k / dl, &mm) || mm > cap)
            return no;
        // Mag: a bound on every in-range |value|, plus one.
        const umax mag = static_cast<umax>(ceil(abs(lo) > abs(hi) ? abs(lo) : abs(hi))) + 1;
        umax       km, kms;
        if (mul_overflow(k, mag, &km) || mul_overflow(km, umax{4}, &kms))
            return no;
        const umax by_num = cap / kms; // num·K and M·aden each below cap/2
        const umax by_den = cap / s;   // aden·S
        return unanchored_codec_t{static_cast<imax>(k),
                                  static_cast<imax>(s),
                                  lo.Denominator < 0 ? -static_cast<imax>(mm) : static_cast<imax>(mm),
                                  by_num < by_den ? by_num : by_den,
                                  true};
    }
}();

//---------------------------------------------------------------------------
// assign(insidable, floating_point | rational)
//---------------------------------------------------------------------------
template <insidable L, typename R>
    requires fractional<R>
struct assignment<L, R> {
  private:
    // A floating source with |rhs| ≥ 2^64 lies outside every grid and has no
    // rational form (rational(double) would fail): decide such values by sign.
    static constexpr bool huge(const R& rhs) noexcept {
        if constexpr (std::floating_point<R>)
            return past_64_bits(rhs);
        else
            return false;
    }

    // (Out of line, by value, for a double: its exact comparison has a cold
    // tail whose call would otherwise cost the inlined store registers.)
    [[gnu::noinline]] static constexpr bool below_lower(R rhs)
        requires std::floating_point<R>
    {
        return huge(rhs) ? rhs < 0 : rhs < lower64<L>;
    }
    static constexpr bool below_lower(const R& rhs)
        requires(!std::floating_point<R>)
    {
        return rhs < lower64<L>;
    }

    template <typename P, typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action) {
        const bool low   = below_lower(rhs);
        R          bound = low ? static_cast<R>(lower64<L>) : static_cast<R>(upper64<L>);
        R          overshoot;
        if constexpr (std::same_as<R, rational>)
            overshoot = try_sub(rhs, bound).value_or(rational{0});
        else
            overshoot = rhs - bound;

        // The clamp target is an interval endpoint — a grid point, no rounding.
        lhs = L::from_raw(endpoint_raw<L>(low));

        if constexpr (clamp_action<plain_t<A>>)
            action.Fn(lhs, overshoot);
    }

  public:
    // Exposed (not private) so the insidable-rhs wrap path can reuse the
    // rational specialization's modular wrap on fractional/notch grids, and
    // so the wrap path can reuse store_checked after computing the wrapped
    // value.
    //
    // apply_wrap for a fractional R — rounding onto the lattice, then modular
    // reduction into [Lower, Lower + range), range = Upper - Lower + Notch.
    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action) {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        // A floating source of 2^63 or more has no rational form: it folds
        // exactly, like every value whose rational steps overflow below.
        if constexpr (std::floating_point<R>)
            if (!(rhs < 0x1p63 && rhs > -0x1p63)) [[unlikely]]
                return wrap_exact(lhs, exact_of_double(static_cast<double>(rhs)), policy, action);
        rational rhs_r{rhs};
        if constexpr (!notched<L> && grid_of<L>.MaxDenominator != 0 && has_policy<L, P, snap>) {
            // A bounded continuous grid rounds onto its bound first, too: a
            // fold keeps the denominator, so the folded value stays on it.
            const auto r = try_rational(round_to_bound<L, plain_t<P>>(exact_of(rhs_r)));
            if (!r) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            rhs_r = *r;
        } else if constexpr (has_policy<L, P, snap>) {
            const auto r = try_round_to_lattice<L, P>(rhs_r);
            if (!r) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            rhs_r = *r;
        }
        imax     q;
        rational folded;
        if (abs_den(rhs_r.Denominator) == 1 && notch64<L> == 1 && abs_den(lower64<L>.Denominator) == 1) {
            // Integer value on a unit lattice: fold exactly (the grid may span
            // 2^64 values, past the rational range).
            using fold         = unit_fold<L, source_lo<rational>, source_hi<rational>>;
            using W            = typename fold::W;
            const auto [qq, w] = fold::fold(static_cast<W>(wide_numerator(rhs_r)));
            const W v          = fold::lower + w;
            q                  = qq;
            folded             = v < W{0} ? -rational{static_cast<umax>(-v)} : rational{static_cast<umax>(v)};
        } else {
            // q = floor((rhs - lower) / range), wrapped = rhs - q * range. A step
            // past the 64-bit rational range, or a fold count past imax, folds
            // exactly instead.
            const auto span  = try_sub(upper64<L>, lower64<L>);
            const auto range = span ? try_add(*span, notch64<L>) : span;
            const auto off   = try_sub(rhs_r, lower64<L>);
            const auto quot  = (range && off) ? try_div(*off, *range) : off;
            if (!range || !quot || *quot < rational{std::numeric_limits<imax>::min()} ||
                *quot > rational{std::numeric_limits<imax>::max()}) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            q             = floor(*quot);
            const auto qr = try_mul(rational{q}, *range);
            const auto w  = qr ? try_sub(rhs_r, *qr) : qr;
            if (!w) [[unlikely]]
                return wrap_exact(lhs, exact_of(rhs_r), policy, action);
            folded = *w;
        }

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, folded, policy, action);

        if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(q));
    }

    // The fold done exactly when its rational steps overflow (a double past
    // 2^63, a far value on a fine grid): the exact store's wrap, which rounds
    // by the policy and folds the wide lattice index (a wrap action's carry
    // saturates, as on the wide paths). A continuous L has no lattice to
    // fold in wide integers, and a point no period: still overflow.
    template <typename P, typename A, std::size_t K>
    [[gnu::cold]] [[gnu::noinline]] static constexpr void
    wrap_exact(L& lhs, const wide_rational<K>& v, P&& policy, A&& action) {
        if constexpr (integer_storage<L> && !point_storage<L>)
            assign_exact<R>(lhs, v, policy, action);
        else
            report_failure(lhs, policy, action, errc::overflow);
    }

    // Cold and out of line: the exact wide slot for a quotient past the
    // 64-bit rational, kept off the hot store path. It returns 16 bytes, not
    // the 72-byte exact_index_result: GCC charges the copy of a large return
    // value against inlining the whole store (+17% instructions measured).
    struct wide_slot_result {
        umax Slot;
        bool Exact;
    };
    template <typename P>
    [[gnu::cold]] [[gnu::noinline]] static constexpr wide_slot_result wide_slot(const rational& v) {
        const exact_index_result r = exact_index<L, rounding_for<L, P>>(exact_of(v));
        return {static_cast<umax>(r.Index), r.Exact}; // in range: fits 64 bits
    }

    // The source as a rational, converted once. A double finer than 2^-62
    // has no exact 64-bit rational (rational{d} truncates it toward zero):
    // `exact` comes back false, and the store takes its exact value through
    // store_fine_double, so it rounds, or reports rounding_error, by its true
    // value.
    static constexpr rational source_of(const R& rhs, bool& exact) {
        if constexpr (std::floating_point<R>)
            return rational_of_double(static_cast<double>(rhs), exact);
        else
            return rational{rhs};
    }
    // fine_double_value's result, sized to return in registers (a larger
    // one goes through memory, and the stack protector into the hot path):
    // a rational raw marks a failure by a denominator no value has (0), with
    // the errc as numerator; an integer raw carries the errc beside it.
    struct fine_result {
        raw_t<L>       Raw;
        errc           Code;
        constexpr errc code() const { return Code; }
    };
    struct fine_rational_result {
        rational       Raw;
        constexpr errc code() const { return Raw.Denominator == 0 ? static_cast<errc>(Raw.Numerator) : errc{}; }
    };
    using fine_result_t = std::conditional_t<rational_storage<L>, fine_rational_result, fine_result>;

    // P's flags, recording its first failure into an errc instead.
    template <typename P>
    using recording_policy = ::beman::inside::policy<P::Flags, error_ref>;

    // Out of line, and by value (an empty action too): a reference to a
    // caller's local would make it escape the hot path's frame. lhs may be
    // uninitialized (a trivial default ctor), so the store runs on a copy of
    // the lower limit, and a failure is returned, not stored: a failed store
    // leaves lhs intact. The policy hears of it here; an on_error action,
    // which gets lhs, from store_fine_double.
    template <typename P, typename A>
    [[gnu::cold]] [[gnu::noinline]] static constexpr fine_result_t fine_double_value(double rhs, P policy, A action) {
        L r = [] {
            if constexpr (rational_storage<L>)
                return L::from_raw(lower64<L>);
            else if constexpr (point_storage<L>)
                return L::from_raw(raw_t<L>{});
            else
                return L::from_raw(static_cast<raw_t<L>>(raw_lo_exact<L>));
        }();
        errc code{};
        // A recording on_error stands in for one (dispatch_out_of_range ranks
        // it below clamp and wrap): the order holds.
        if constexpr (error_action<plain_t<A>>)
            assign_exact<R>(r, exact_of_double(rhs), recording_policy<P>{code}, on_error([&code](L&, errc c, auto&&) {
                                code = c;
                            }));
        else {
            assign_exact<R>(r, exact_of_double(rhs), recording_policy<P>{code}, std::forward<A>(action));
            if (code != errc{})
                policy.report(code);
        }
        if constexpr (rational_storage<L>) {
            if (code == errc{})
                return {r.raw()};
            rational failed{};
            failed.Numerator   = static_cast<umax>(code);
            failed.Denominator = 0;
            return {failed};
        } else
            return {r.raw(), code};
    }
    // Whether a failure returns (errc&, on_error): else it raised, and lhs
    // takes the result unconditionally — no branch on the hot path's tail.
    template <typename P, typename A>
    static constexpr bool fine_failure_returns = std::is_same_v<P, recording_policy<P>> || error_action<A>;
    template <typename P, typename A>
    static constexpr void store_fine_double(L& lhs, double rhs, P&& policy, A&& action) {
        using Act    = std::conditional_t<std::is_empty_v<plain_t<A>>, plain_t<A>, A&&>;
        const auto r = fine_double_value<plain_t<P>, Act>(rhs, policy, std::forward<A>(action));
        if constexpr (!fine_failure_returns<plain_t<P>, plain_t<A>>)
            lhs = L::from_raw(r.Raw);
        else if (r.code() == errc{}) [[likely]]
            lhs = L::from_raw(r.Raw);
        else if constexpr (error_action<plain_t<A>>)
            action.Fn(lhs, r.code(), errc_message(r.code()));
    }

    template <typename P, typename A = no_action>
    static constexpr void store_checked(L& lhs, R rhs, P&& policy, A&& action = {}) {
        if constexpr (rational_storage<L>) {
            bool           exact = true;
            const rational rv    = source_of(rhs, exact);
            if constexpr (std::floating_point<R>)
                if (!exact) [[unlikely]]
                    return store_fine_double(lhs, static_cast<double>(rhs), policy, action);
            // Continuous: store verbatim. A bounded grid's values have
            // denominators ≤ its bound: one past it is off the grid — rounded
            // onto it under a rounding policy, else reported (but unsafe).
            if constexpr (grid_of<L>.MaxDenominator != 0)
                if (abs_den(rv.Denominator) > to_rational(grid_of<L>.MaxDenominator).Numerator) [[unlikely]] {
                    if constexpr (has_policy<L, P, snap>) {
                        const auto o = onto_bound<L, plain_t<P>>(exact_of(rv));
                        const auto r = o ? try_rational(*o)
                                         : std::expected<rational, errc>{std::unexpected{errc::rounding_error}};
                        if (!r)
                            return report_failure(lhs, policy, action, errc::rounding_error);
                        lhs = L::from_raw(*r);
                        return;
                    } else if (std::is_constant_evaluated() || is_checked(flags_of<L> | plain_t<P>::Flags))
                        return report_failure(lhs, policy, action, errc::rounding_error);
                }
            lhs = L::from_raw(rv);
        } else if constexpr (point_grid<L>)
            lhs = L::from_raw({}); // a point: its value is the type
        else {
            // Store the k-th notch slot: raw_from_offset<L> covers index and
            // value raws.
            auto store_slot = [&](auto k) { lhs = L::from_raw(raw_from_offset<L>(k)); };

            constexpr bool has_round_flag = has_policy<L, P, snap>; // every round_* flag contains snap
            // The slot onum/den when it is exact or P rounds; false otherwise
            // (strict policy, off-notch: the rational path below reports it).
            auto try_slot = [&](umax onum, umax den) {
                if (onum % den == 0) {
                    store_slot(onum / den);
                    return true;
                }
                if constexpr (has_round_flag) {
                    store_slot(round_quotient<L, P>(onum, den));
                    return true;
                }
                return false;
            };

            // A floating source on an anchored grid with a power-of-two notch
            // whose values double holds: v/Notch is exact, so round it in
            // double (the integer storage's rule) and read the slot off the
            // value index. No rational round trip.
            if constexpr (std::floating_point<R> && anchored<L> && double_exact<grid_of<L>> &&
                          std::has_single_bit(notch64<L>.Numerator)) {
                constexpr imax lo = signed_numerator(*index_lo64<L>);
                const double   v  = static_cast<double>(rhs);
                const imax     j  = snap_double_index<grid_of<L>, rounding_for<L, P>>(v);
                if constexpr (!has_round_flag)
                    if (static_cast<double>(j) * static_cast<double>(notch64<L>) != v && policy.round_check())
                        [[unlikely]]
                        return report_failure(lhs, policy, action, errc::rounding_error);
                store_slot(static_cast<umax>(j - lo));
                return;
            }
            bool           exact = true;
            const rational rv    = source_of(rhs, exact);
            if constexpr (std::floating_point<R>)
                if (!exact) [[unlikely]]
                    return store_fine_double(lhs, static_cast<double>(rhs), policy, action);

            // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
            // (num − Lo·aden)·K / aden — integer ops instead of two rational ops.
            // round_quotient is invariant under reduction, so the slot is
            // bit-identical to the rational path. Oversized denominators
            // fall through (the max_den guard keeps every product inside imax).
            if constexpr (qformat_codec_fits<L> && notched<L>) {
                constexpr imax K      = abs_den(notch64<L>.Denominator);
                constexpr imax Lo     = lower_imax<L>;
                constexpr umax two_km = [] {
                    // 2 · K · M with saturation (M bounds |value| and the offset span)
                    // ceil in umax: a bound's magnitude may reach 2^63 (past imax)
                    constexpr umax umax_hi = std::numeric_limits<umax>::max();
                    const rational a       = abs(lower64<L>) > abs(upper64<L>) ? abs(lower64<L>) : abs(upper64<L>);
                    const umax     c       = round_magnitude(a, round_mode::ceil).Magnitude;
                    if (c > (umax_hi - 2) / 2)
                        return umax_hi;
                    umax k = static_cast<umax>(K);
                    umax m = c * 2 + 2;
                    if (k > umax_hi / m)
                        return umax_hi;
                    umax prod = k * m;
                    return prod > umax_hi / 2 ? umax_hi : prod * 2;
                }();
                constexpr umax max_den = static_cast<umax>(std::numeric_limits<imax>::max()) / two_km;

                const umax aden = abs_den(rv.Denominator);
                if (max_den != 0 && aden <= max_den) {
                    // The offset (num − Lo·aden)·K / aden, unreduced: round_quotient
                    // rounds it as the reduced fraction would, and max_den keeps the
                    // product in imax.
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>((num - Lo * static_cast<imax>(aden)) * K);
                    if (try_slot(onum, aden))
                        return;
                }
            }

            // The same shortcut for an unanchored lattice: with K the common
            // denominator of Notch and Lower, Notch = sN/K and Lower = m/K, the
            // offset is (num·K − m·aden)/(aden·sN).
            if constexpr (unanchored_codec<L>.Ok) {
                constexpr auto c    = unanchored_codec<L>;
                const umax     aden = abs_den(rv.Denominator);
                if (aden <= c.MaxDen) {
                    // Unreduced: MaxDen bounds these products already, and the
                    // unanchored rounding and the exactness test do not change
                    // under reduction.
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>(num * c.K - c.M * static_cast<imax>(aden));
                    const umax den2 = aden * static_cast<umax>(c.S);
                    if (try_slot(onum, den2))
                        return;
                }
            }

            // The exact quotient can overflow the 64-bit rational range (huge
            // source denominator × fine notch): take the slot from the exact wide
            // index instead (rhs is in range, so it fits L's raw). Rounding is
            // the value-space rule, as everywhere else.
            const auto rel      = try_sub(rv, lower64<L>);
            const auto quotient = rel ? try_div(*rel, notch64<L>) : rel;
            if (!quotient.has_value()) [[unlikely]] {
                const wide_slot_result slot = wide_slot<P>(rv);
                if constexpr (!has_round_flag)
                    if (!slot.Exact && policy.round_check()) [[unlikely]]
                        return report_failure(lhs, policy, action, errc::rounding_error);
                store_slot(slot.Slot);
                return;
            }
            rational raw = *quotient;
            umax     den = static_cast<umax>(raw.Denominator);
            if (den == 1) {
                store_slot(raw.Numerator);
                return;
            }

            if constexpr (!has_round_flag)
                if (policy.round_check()) [[unlikely]]
                    return report_failure(lhs, policy, action, errc::rounding_error);
            store_slot(round_quotient<L, P>(raw.Numerator, den));
        }
    }

  private:
    // Range test for the source value. A floating source compares in double when
    // both endpoints are exact doubles (then the comparison is exact), instead of
    // converting the value to a rational first.
    // (A limit near 2^64 rounds to a double of 2^64, which no rational holds:
    // it is no exact double.)
    static constexpr bool exact_double(const rational& r) {
        const double d = static_cast<double>(r);
        return !past_64_bits(d) && rational{d} == r;
    }
    static constexpr bool double_bounds_exact =
        std::floating_point<R> && exact_double(lower64<L>) && exact_double(upper64<L>);

    // A rational source on integer endpoints compares by multiplying the
    // endpoint by the denominator (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d
    // exceeds any n) — exact, and no division.
    static constexpr bool integer_bounds = std::same_as<R, rational> && abs_den(lower64<L>.Denominator) == 1 &&
                                           abs_den(upper64<L>.Denominator) == 1 && fits_imax(interval_of<L>);

    static constexpr bool out_of_interval(const R& rhs) {
        if (huge(rhs))
            return true;
        if constexpr (double_bounds_exact) {
            constexpr double lo = static_cast<double>(lower64<L>);
            constexpr double hi = static_cast<double>(upper64<L>);
            return rhs < lo || rhs > hi;
        } else if constexpr (integer_bounds) {
            constexpr imax lo = signed_numerator(lower64<L>);
            constexpr imax hi = signed_numerator(upper64<L>);
            const umax     n = rhs.Numerator, d = abs_den(rhs.Denominator);
            auto           le = [&](umax m) {
                umax p;
                return mul_overflow(m, d, &p) || n <= p;
            };
            auto ge = [&](umax m) {
                umax p;
                return !mul_overflow(m, d, &p) && n >= p;
            };
            if (rhs.Denominator < 0 && n != 0) // value −n/d < 0
            {
                bool in_lo, in_hi;
                if constexpr (lo >= 0)
                    in_lo = false;
                else
                    in_lo = le(safe_abs(lo));
                if constexpr (hi >= 0)
                    in_hi = true;
                else
                    in_hi = ge(safe_abs(hi));
                return !(in_lo && in_hi);
            }
            bool in_lo, in_hi; // value n/d ≥ 0
            if constexpr (lo <= 0)
                in_lo = true;
            else
                in_lo = ge(static_cast<umax>(lo));
            if constexpr (hi < 0)
                in_hi = false;
            else
                in_hi = le(static_cast<umax>(hi));
            return !(in_lo && in_hi);
        } else if constexpr (std::floating_point<R>) {
            // A limit no double holds (INT64_MAX): compare exactly, without
            // grid numbers, whose runtime ops stop at 64 bits.
            // Below 2^64 a rational holds the value unless it is finer than 2^-62.
            const double d = static_cast<double>(rhs);
            if (abs_fraction_exact(d)) [[likely]] {
                const rational r{d};
                return r < lower64<L> || upper64<L> < r;
            }
            const auto v = exact_of_double(d);
            return v < exact_of(lower64<L>) || exact_of(upper64<L>) < v;
        } else
            return not includes(interval_of<L>, rhs);
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // A long double that is a double stores as that double; one with bits
        // past it stores exactly (the double paths would drop them).
        if constexpr (wide_float<R>) {
            const double d = static_cast<double>(rhs);
            if (is_finite(rhs) && static_cast<R>(d) != rhs) [[unlikely]] {
                // Past the double range: a stand-in past every limit, which
                // clamps or overflows, but a fold needs the exact value.
                if constexpr (has_policy<L, P, wrap>)
                    if (!is_finite(d)) {
                        report_failure(lhs, policy, action, errc::overflow);
                        return lhs;
                    }
                return assign_exact<R>(lhs, exact_of_long_double(rhs), policy, std::forward<A>(action));
            }
            return assignment<L, double>::assign(lhs, d, policy, std::forward<A>(action));
        } else if constexpr (wide_valued<L>) {
            // NaN / ±inf first, as below; a finite |rhs| ≥ 2^64 is an integer,
            // taken exactly (or by its side, past the grid: exact_of_large).
            if constexpr (std::floating_point<R>)
                if (!is_finite(rhs) || huge(rhs)) [[unlikely]] {
                    if (rhs != rhs) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    if (!is_finite(rhs) && !has_policy<L, P, clamp>) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    return assign_exact<R>(
                        lhs, exact_of_large<L>(static_cast<double>(rhs)), policy, std::forward<A>(action));
                }
            if constexpr (std::floating_point<R>)
                if (!abs_fraction_exact(static_cast<double>(rhs))) [[unlikely]] // finer than 2^-62
                    return assign_exact<R>(
                        lhs, exact_of_double(static_cast<double>(rhs)), policy, std::forward<A>(action));
            return assign_exact<R>(lhs, exact_of(rational{rhs}), policy, std::forward<A>(action));
        } else
            return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
    }

  private:
    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action) {
        // NaN / ±inf: no rational value to round or range-check. clamp saturates
        // an infinity; everything else reports not_finite through the policy.
        if constexpr (std::floating_point<R>)
            if (!is_finite(rhs)) [[unlikely]] {
                if constexpr (has_policy<L, P, clamp>)
                    if (rhs == rhs) {
                        // Saturated like any value past a limit: an on_clamp
                        // hears of it, with an overshoot of ±inf.
                        apply_clamp(lhs, rhs, policy, action);
                        return lhs;
                    }
                report_failure(lhs, policy, action, errc::not_finite);
                return lhs;
            }

        if (out_of_interval(rhs)) [[unlikely]] {
            if constexpr (std::floating_point<R>)
                if (!abs_fraction_exact(static_cast<double>(rhs))) {
                    store_fine_double(lhs, static_cast<double>(rhs), policy, action);
                    return lhs;
                }
            // Round first: a value just outside may round onto an endpoint.
            if constexpr (rounds_before_range_check<L, plain_t<P>>)
                if (!huge(rhs))
                    if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(rational{rhs}); rr.Ok) {
                        lhs = L::from_raw(rr.Raw);
                        return lhs;
                    }
            // Fractional path has no wrap *action* branch (Wrappable = false).
            if (dispatch_out_of_range<false>(
                    lhs,
                    policy,
                    action,
                    [&] { apply_clamp(lhs, rhs, policy, action); },
                    [&] { apply_wrap(lhs, rhs, policy, action); }))
                return lhs;
        }

        store_checked(lhs, rhs, policy, action);
        return lhs;
    }
};

//---------------------------------------------------------------------------
// assign(insidable, insidable)
//---------------------------------------------------------------------------
template <insidable L, insidable R>
struct assignment<L, R> {
  private:
    // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`
    // between two integer raws. A continuous side stores through the rational
    // path instead (both 0, unused), and a point L holds one value (both 0).
    static constexpr bool integer_raws = integer_storage<L> && integer_storage<R>;

    static constexpr std::expected<rational, errc> calc_offset() {
        if constexpr (!integer_raws || !notched<L>)
            return rational{0};
        else
            return try_sub(lower64<R>, lower64<L>).and_then([](const rational& d) { return try_div(d, notch64<L>); });
    }

    static constexpr std::expected<rational, errc> calc_factor() {
        if constexpr (!integer_raws || !notched<L> || point_storage<R>)
            return rational{0}; // a point R's raw is always 0: the mapping is Offset alone
        else
            return try_div(notch64<R>, notch64<L>);
    }

    // A mapping past the 64-bit rational (a full-int64 source into a fine
    // grid) stores through the rational path, as a continuous side does.
    static constexpr bool maps_raws = integer_raws && calc_offset().has_value() && calc_factor().has_value();

  public:
    static constexpr rational Offset = calc_offset().value_or(rational{0});
    static constexpr rational Factor = calc_factor().value_or(rational{0});

    // Raw-space integer-only mapping — requires integer raw storage on both
    // sides (not rational).
    // It also needs every raw and every mapped raw in imax: map_raw's L-raw
    // range is [Offset, Offset + Factor·max_index<R>] (+ Lower for value storage).
    static constexpr bool is_integer_mapping = [] {
        if constexpr (!maps_raws || abs_den(Factor.Denominator) != 1 || abs_den(Offset.Denominator) != 1 ||
                      !values_fit_imax<L> || !values_fit_imax<R>)
            return false;
        else {
            const rational base = index_storage<L> ? rational{0} : lower64<L>;
            const auto     lo   = Offset + base;
            const auto     span = Factor * rational{max_index_of<R>};
            if (!lo || !span)
                return false;
            const auto hi = *lo + *span;
            return hi.has_value() && *lo >= rational{std::numeric_limits<imax>::min()} &&
                   *hi <= rational{std::numeric_limits<imax>::max()};
        }
    }();

    // Non-integer mapping folded to one integer multiply-add:
    //   Offset + Factor·raw = (o_s·f_d + raw·f_n·o_d) / (o_d·f_d)
    // with every coefficient compile-time. round_quotient is invariant under
    // fraction reduction, so rounding the unreduced pair is bit-identical to
    // reducing through the two rational ops first. ok gates on every product
    // (including the worst-case runtime numerator over R's raw range)
    // provably fitting imax; mul/add/den are zeroed when not ok.
    struct affine_map_t {
        imax Mul;
        imax Add;
        imax Den;
        bool Ok;
    };
    static constexpr affine_map_t affine_map = [] {
        constexpr affine_map_t no{0, 0, 0, false};
        if constexpr (!maps_raws || !notched<L> || is_integer_mapping || !values_fit_imax<L> || !values_fit_imax<R>)
            return no;
        else {
            if (!std::in_range<imax>(Factor.Numerator) || !std::in_range<imax>(Offset.Numerator))
                return no;
            const imax   f_n = static_cast<imax>(Factor.Numerator); // Factor > 0
            const imax   f_d = abs_den(Factor.Denominator);
            const imax   o_s = signed_numerator(Offset);
            const imax   o_d = abs_den(Offset.Denominator);
            affine_map_t m{0, 0, 0, true};
            if (mul_overflow(f_n, o_d, &m.Mul) || mul_overflow(o_s, f_d, &m.Add) || mul_overflow(o_d, f_d, &m.Den))
                return no;
            // worst-case |numerator| over R's offset range [0, max_index]
            if (!std::in_range<imax>(max_index_of<R>))
                return no;
            const imax rmax = static_cast<imax>(max_index_of<R>);
            imax       term, num;
            if (mul_overflow(rmax, m.Mul, &term) || add_overflow(term, m.Add < 0 ? -m.Add : m.Add, &num))
                return no;
            // round_quotient equivalence: rounding is reduction-invariant, but
            // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
            // imax — mirror those checks for the unreduced den so both forms
            // take the same branch (ties on negatives differ across branches).
            constexpr auto zl = try_div(lower64<L>, notch64<L>).value_or(rational{0});
            if (abs_den(zl.Denominator) == 1) {
                if (!std::in_range<imax>(zl.Numerator))
                    return no;
                const imax mbias = signed_numerator(zl);
                imax       mdi, total;
                if (mul_overflow(mbias, m.Den, &mdi) || add_overflow(mdi, num, &total))
                    return no;
            }
            return m;
        }
    }();

    // L's raw for R's offset 0: Offset + raw_lo<L>, which is_integer_mapping
    // proves fits imax (Offset alone may not, e.g. 2^63 into a full int64).
    static constexpr imax map_bias = [] {
        if constexpr (is_integer_mapping)
            return signed_numerator((Offset + (index_storage<L> ? rational{0} : lower64<L>)).value());
        else
            return imax{0};
    }();

    // Map rhs.Raw into L's raw space (requires is_integer_mapping): Factor
    // scales R's 0-based offset, map_bias adds L's raw for offset 0. Every
    // mapped raw fits imax, so the wrapping umax multiply-add is exact even
    // where an intermediate (the offset, or a signed partial sum) would not be.
    static constexpr imax map_raw(auto rhs_raw) {
        const umax rel = static_cast<umax>(static_cast<imax>(rhs_raw)) - static_cast<umax>(raw_lo<R>);
        return static_cast<imax>(static_cast<umax>(map_bias) + static_cast<umax>(Factor.Numerator) * rel);
    }

  private:
    template <typename A>
    static constexpr void apply_clamp(L& lhs, const R& rhs, A&& action) {
        lhs = L::from_raw(endpoint_raw<L>(as_rational(rhs) < lower64<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are insides, so the overshoot is too. It is always
        // in-grid and on-notch for grid_of<R> − grid_of<L>, so the construction is exact.
        if constexpr (clamp_action<plain_t<A>>) {
            constexpr grid            OG = grid_of<R> - grid_of<L>;
            beman::inside::inside<OG> overshoot{(as_rational(rhs) - as_rational(lhs)).value()};
            action.Fn(lhs, overshoot);
        }
    }

    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, const R& rhs, P&& policy, A&& action) {
        // The integer modular wrap (range = Upper - Lower + 1, integer values) is
        // only correct on a unit-integer grid — notch 1 with integer bounds, so
        // consecutive integers are adjacent grid points — and for a source whose
        // values are integers (no rounding to do). Anything else routes through
        // the rational modular wrap, which rounds by the policy first.
        if constexpr (unit_lattice(grid_of<L>) && integer_lattice<R>) {
            // Unit-integer fast path: modular wrap on the integer value, exact
            // (either grid may reach past int64; the span can be 2^64−1).
            using fold             = unit_fold<L, wide_numerator(lower_of<R>), wide_numerator(upper_of<R>)>;
            using W                = typename fold::W;
            const auto [excess, w] = fold::fold(static_cast<W>(wide_numerator(as_rational(rhs))));
            lhs                    = L::from_raw(raw_of_slot<L>(w));
            if constexpr (wrap_action<plain_t<A>>)
                action.Fn(lhs, make_wrap_carry<L, R>(excess)); // carry as an inside
        } else if constexpr (wrap_action<plain_t<A>>) {
            // Fractional destination with a wrap action: reuse the rational modular-wrap
            // path for the store/rounding, but wrap its imax carry `q` into an inside before
            // handing it to the user action.
            assignment<L, rational>::apply_wrap(
                lhs, as_rational(rhs), policy, beman::inside::on_wrap([&](auto& self, imax q) {
                    action.Fn(self, make_wrap_carry<L, R>(q));
                }));
        } else {
            // Fractional destination, no wrap action: delegate unchanged.
            assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
        }
    }

    template <typename P, typename A>
    static constexpr bool try_clamp_or_fail(L& lhs, const R& rhs, P&& policy, A&& action) {
        return dispatch_out_of_range<true>(
            lhs,
            policy,
            action,
            [&] { apply_clamp(lhs, rhs, action); },
            [&] { apply_wrap(lhs, rhs, policy, action); });
    }

    // R's value outside L's interval: for integer raws an integer compare of
    // counts in a common unit against L's endpoints, else the exact value.
    static constexpr bool out_of_interval(const R& rhs) {
        if constexpr (units_cmp_fits<L, R>) {
            constexpr grid_rational U  = gcd(unit_of<L>, unit_of<R>);
            constexpr imax          lo = static_cast<imax>(units_lo<L, U>), hi = static_cast<imax>(units_hi<L, U>);
            const imax              c = value_in_units<imax, U>(rhs);
            return c < lo || c > hi;
        } else
            return not includes(interval_of<L>, as_rational(rhs));
    }

    // (The value is in range here: of the actions only on_error can fire.)
    template <typename P, typename A>
    static constexpr void store(L& lhs, const R& rhs, P&& policy, A&& action) {
        if constexpr (!maps_raws)
            // A continuous side or no raw mapping: store the source's value through
            // the rational store, which rounds it by the policy or reports rounding_error.
            assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy, action);
        else if constexpr (is_integer_mapping) {
            // exact: Factor and Offset have integer denominators, no rounding ambiguity
            if constexpr (Offset == 0 && Factor == 1 && same_storage<L, R>)
                lhs = L::from_raw(raw_cast<L>(rhs.raw()));
            else
                lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
        } else if constexpr (affine_map.Ok) {
            // Folded non-integer mapping: one multiply-add, then the same
            // round_quotient (invariant under reduction — bit-identical to the
            // rational chain below).
            // Offset/Factor map R's 0-based offset: a value raw counts from Lower.
            const imax r_offset = static_cast<imax>(rhs.raw()) - raw_lo<R>; // 0 for an index raw
            const imax num      = affine_map.Add + r_offset * affine_map.Mul;
            const umax q =
                round_quotient<L, P>(static_cast<umax>(num < 0 ? -num : num), static_cast<umax>(affine_map.Den));
            lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        } else {
            // Offset/Factor map R's 0-based offset (a value raw counts from Lower).
            // A step past the 64-bit rational takes the exact wide store.
            const auto r_offset = [&] {
                if constexpr (point_storage<R>) // a point's raw holds nothing: the mapping is Offset alone
                    return std::expected<rational, errc>{rational{0}};
                else if constexpr (raw_lo<R> == 0)
                    return std::expected<rational, errc>{rational{rhs.raw()}};
                else
                    return try_sub(rational{rhs.raw()}, lower64<R>);
            }();
            const auto scaled = r_offset ? try_mul(Factor, *r_offset) : r_offset;
            const auto mapped = scaled ? try_add(Offset, *scaled) : scaled;
            if (!mapped) [[unlikely]] {
                assign_exact<R>(lhs, exact_of(rhs), policy, action);
                return;
            }
            const rational rat = *mapped;
            umax           ad  = static_cast<umax>(abs_den(rat.Denominator));
            // Round the L-offset to a notch index in VALUE space via round_quotient
            // (same as the scalar path), honouring every rounding mode.
            umax q = round_quotient<L, P>(rat.Numerator, ad);
            // rat is the L-offset; raw_from_offset<L> adds lower64<L> back for value storage.
            lhs =
                L::from_raw((rat.Denominator < 0) ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        }
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // A wide raw on either side: the exact wide path.
        if constexpr (wide_valued<L> || wide_valued<R>) {
            static_checks<P>();
            if constexpr (same_notch_raws<L, R>) {
                // Equal notches: the raw maps by a constant shift. Out of range
                // takes the exact path's policy cascade below.
                using W     = work_int_t<same_notch_bits<L, R>>;
                const W raw = static_cast<W>(rhs.raw()) + static_cast<W>(same_notch_shift<L, R>);
                if (raw >= static_cast<W>(raw_lo_exact<L>) && raw <= static_cast<W>(raw_hi_exact<L>)) [[likely]] {
                    lhs = L::from_raw(static_cast<raw_t<L>>(raw));
                    return lhs;
                }
            }
            return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
        } else
            return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
    }

  private:
    // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
    // for them (matches the integral-rhs path); only strict policies reject it.
    template <typename P>
    static constexpr void static_checks() {
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                          not excludes(interval_of<L>, interval_of<R>),
                      "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
                      "incompatible notches: use with_snap() or policy<snap>() to allow rounding");
    }

    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action) {
        static_checks<P>();

        if constexpr (not includes(interval_of<L>, interval_of<R>)) {
            if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>) {
                // (A point L maps every raw onto its one raw: only the value
                // tells whether it is in range.)
                if constexpr (is_integer_mapping && !point_storage<L>) {
                    if (imax mapped = map_raw(rhs.raw()); mapped < raw_lo<L> || mapped > raw_hi<L>)
                        if (try_clamp_or_fail(lhs, rhs, policy, action))
                            return lhs;
                } else if (out_of_interval(rhs)) {
                    // Round first: a value just outside may round onto an endpoint.
                    // (The integer mapping above lands on the lattice: nothing to round.)
                    if constexpr (rounds_before_range_check<L, plain_t<P>>)
                        if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(as_rational(rhs)); rr.Ok) {
                            lhs = L::from_raw(rr.Raw);
                            return lhs;
                        }
                    if (try_clamp_or_fail(lhs, rhs, policy, action))
                        return lhs;
                }
            }
        }

        store(lhs, rhs, policy, action);
        return lhs;
    }
};

//---------------------------------------------------------------------------
// assign_grid_number — store a grid number at run time (a constexpr
// variable, `N - 1_g`). Forming a value past 64 bits is compile-time only, but
// reading one is not: its limbs live in static storage. The exact value then
// stores like any exact value. A number too wide for L's exact type lies far
// outside L when it is an integer, so a stand-in just past the near limit
// takes the out-of-range path; a fraction that wide is overflow.
//---------------------------------------------------------------------------
template <insidable L, typename P>
constexpr L& assign_grid_number(L& lhs, const big_rational& v, P&& policy) {
    constexpr std::size_t K     = exact_limbs<L>;
    auto                  limbs = [](const big_int& x) { return x.Size != 0 ? x.Size : std::size_t{1}; };
    if (limbs(v.Num) < K && limbs(v.Den) < K) [[likely]]
        return assign_exact<rational>(lhs, exact_of_grid<K>(v), policy, no_action{});
    if (!v.is_integer()) {
        no_action none;
        report_failure(lhs, policy, none, errc::overflow);
        return lhs;
    }
    const wide_rational<K> one{wide_sint<K>{1}, wide_sint<K>{1}};
    const wide_rational<K> past =
        v.Num.negative() ? exact_of_grid<K>(lower_of<L>) + -one : exact_of_grid<K>(upper_of<L>) + one;
    return assign_exact<rational>(lhs, past, policy, no_action{});
}
} // namespace beman::inside::detail



//---------------------------------------------------------------------------
// policy — runtime policy carrier, plus `policy_ref` for per-operation dispatch.
//   policy<F, E>  — compile-time flags F plus an optional error_ref E holding a
//                   beman::inside::errc& (EBO so the no-error-code form is zero-sized).
//   make_policy<F>() and the named policies rounded_toward_zero, rounded_nearest,
//                   rounded_floor, rounded_ceil, rounded_half_even, clamped, wrapped.
//   policy_ref    — wraps an inside& with a policy<...> and a tuple of on_* actions;
//                   compound ops flow through it, routing each action to the right
//                   callback at the right pipeline stage.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// policy — derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
// `policy(ec)` makes E == error_ref carrying a `beman::inside::errc&`. No virtuals,
// resolved at compile time. (The error-code channel reports `errc` directly —
// there is no <system_error> dependency.)
//---------------------------------------------------------------------------
namespace detail {
struct empty_ref {};
struct error_ref {
    constexpr error_ref(errc& ec) : Code{ec} {}
    errc& Code;
};
} // namespace detail

template <policy_flag W = none, typename E = detail::empty_ref>
struct policy : E {
    constexpr policy() = default;
    constexpr policy(errc& ec)
        requires std::same_as<E, detail::error_ref>
        : E(ec) {}

    static constexpr policy_flag Flags = W;

    static constexpr bool test(policy_flag w) { return has_flag(W, w); }

    static constexpr bool range_check() {
        if consteval {
            return true;
        }
        return is_checked(W) && not test(ignore_range);
    }

    static constexpr bool round_check() {
        if consteval {
            return true;
        }
        return is_checked(W) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error) — also during constant evaluation, so
    // try_make / parse / tan_into return error values in constexpr code.
    // Throw mode funnels through the installed handler via an outlined cold
    // helper; at compile time it names a fixed-string diagnostic instead of
    // "non-constexpr function called".
    constexpr void report(errc code) const {
        if constexpr (std::is_same_v<E, detail::error_ref>)
            E::Code = E::Code != errc{} ? E::Code : code;
        else {
            if consteval {
                detail::constexpr_error<"inside: value out of range during constant evaluation "
                                        "(checked policy hit; choose clamp/wrap or widen the interval)">();
            }
            detail::raise(code);
        }
    }
};

policy(errc&) -> policy<none, detail::error_ref>;

//---------------------------------------------------------------------------
// is_policy_v — true for policy<F,E> specializations, false otherwise.
// Used to gate free-fn overloads so they don't accidentally bind P = action tag.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr bool is_policy_v = false;
template <policy_flag F, typename E>
inline constexpr bool is_policy_v<policy<F, E>> = true;

// Concept form of is_policy_v — pulls cvref off so the constraint matches
// forwarded `policy<F,E>` references in template parameters.
template <typename T>
concept policy_like = is_policy_v<std::remove_cvref_t<T>>;

// flags_of<policy<F, E>> is F. Lets the value+policy constructor and
// policy_ref's conversion fold the per-call flags into their
// `inside_assignable` check, so a one-shot clamp/round actually relaxes the
// constraint it enables.
template <policy_flag F, typename E>
inline constexpr policy_flag flags_of_impl<policy<F, E>> = F;

// True for policy specializations that carry a beman::inside::errc& reference.
// Free-fn arithmetic uses this to decide whether to call policy.report on
// failure (which sets ec) vs. returning a silent std::unexpected (no-arg form).
template <typename T>
inline constexpr bool uses_error_ref = false;
template <policy_flag F>
inline constexpr bool uses_error_ref<policy<F, error_ref>> = true;
} // namespace detail

//---------------------------------------------------------------------------
// make_policy
//---------------------------------------------------------------------------
template <policy_flag F = none>
[[nodiscard]] constexpr auto make_policy() {
    return policy<F, detail::empty_ref>{};
}

template <policy_flag F = none>
[[nodiscard]] constexpr auto make_policy(errc& ec) {
    return policy<F, detail::error_ref>{ec};
}

//---------------------------------------------------------------------------
// report_or_unexpected — uniform "rational arithmetic failed" handler shared
// by addition/multiplication/division/modulo. Three compile-time behaviors:
// overflow_action<A> → fire it on a default Result; uses_error_ref<P> →
// policy.report then std::unexpected{code}; plain throw-policy →
// std::unexpected{code}.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Result, typename A, typename P>
constexpr auto report_or_unexpected(A&& action, P&& policy, errc code)
    -> std::conditional_t<overflow_action<A>, Result, std::expected<Result, errc>> {
    if constexpr (overflow_action<A>) {
        Result res;
        action.Fn(res, code);
        return res;
    } else {
        if constexpr (uses_error_ref<std::remove_cvref_t<P>>)
            policy.report(code);
        return std::unexpected{code};
    }
}
} // namespace detail

//---------------------------------------------------------------------------
// Named convenience policies — let user code skip `make_policy<F>()` entirely
// for the per-call flag form on free arithmetic functions.
//---------------------------------------------------------------------------
// Each is named after its flag: rounded_* = round_*, clamped = clamp,
// wrapped = wrap.
inline constexpr auto rounded_toward_zero = make_policy<round_toward_zero>();
inline constexpr auto rounded_nearest     = make_policy<round_nearest>();
inline constexpr auto rounded_floor       = make_policy<round_floor>();
inline constexpr auto rounded_ceil        = make_policy<round_ceil>();
inline constexpr auto rounded_half_even   = make_policy<round_half_even>();
inline constexpr auto clamped             = make_policy<clamp>();
inline constexpr auto wrapped             = make_policy<wrap>();

//---------------------------------------------------------------------------
// policy_ref — variadic in actions (stores std::tuple<As...>). policy_ref
// pre-picks the matching action per call path, so assignment/arithmetic keep
// their single-A signatures. Payoff: the arithmetic and narrowing stages of a
// compound op can each fire a different action (e.g. on_overflow + on_clamp).
//---------------------------------------------------------------------------
namespace detail {
// The single assignment-time action among `actions` (at most one such tag is
// present), or no_action.
template <typename... As>
constexpr decltype(auto) assignment_action(std::tuple<As...>& actions) {
    if constexpr (has_action<is_clamp_action, As...>)
        return pick_action_in<is_clamp_action>(actions);
    else if constexpr (has_action<is_wrap_action, As...>)
        return pick_action_in<is_wrap_action>(actions);
    else if constexpr (has_action<is_error_action, As...>)
        return pick_action_in<is_error_action>(actions);
    else
        return no_action{};
}

// Shared assignment dispatch: store `src` into `dst` under `policy` + that
// action. Backs both policy_ref (dst = the wrapped inside) and policy_buffer
// (dst = a fresh target), so the conversion/assignment logic lives in exactly
// one place.
template <insidable Dst, numeric C, typename P, typename... As>
constexpr Dst& dispatch_assign(Dst& dst, const C& src, P& policy, std::tuple<As...>& actions) {
    return assignment<Dst, C>::assign(dst, src, policy, assignment_action(actions));
}

// The same into a fresh Target: the value read-out of policy_ref and
// policy_buffer. A failed store (errc&, on_error) leaves it value-initialized,
// not garbage.
template <insidable Target, numeric C, typename P, typename... As>
constexpr Target dispatch_new(const C& src, P& policy, std::tuple<As...>& actions) {
    Target r{};
    dispatch_assign(r, src, policy, actions);
    return r;
}

// policy_buffer — the rvalue-receiver sibling of policy_ref. `with_snap()` etc.
// on a *temporary* return this instead: it OWNS the inside by value (the temporary
// is moved in), so the snapped value can be returned/stored without dangling —
// `auto square(small n){ return (n*n).with_snap(); }` is safe. Value read-out only
// (no operator=: assigning into a throwaway is meaningless). Same constrained
// conversion as policy_ref, so it stays SFINAE-friendly.
template <insidable B, typename P, typename... As>
struct policy_buffer {
    B                                       Owned;
    P                                       Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    template <insidable Target>
        requires inside_assignable<Target, B, flags_of<Target> | flags_of<P>>
    constexpr operator Target() {
        return dispatch_new<Target>(Owned, Policy, Actions);
    }
};

template <insidable B, typename P, typename... As>
struct policy_ref {
  private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned ClampCount    = count_action_matches<is_clamp_action, As...>;
    static constexpr unsigned WrapCount     = count_action_matches<is_wrap_action, As...>;
    static constexpr unsigned ErrorCount    = count_action_matches<is_error_action, As...>;
    static constexpr unsigned OverflowCount = count_action_matches<is_overflow_action, As...>;

    static_assert(ClampCount + WrapCount + ErrorCount <= 1,
                  "on_clamp / on_wrap / on_error are mutually exclusive in a single policy_ref");
    static_assert(ClampCount <= 1, "duplicate on_clamp");
    static_assert(WrapCount <= 1, "duplicate on_wrap");
    static_assert(ErrorCount <= 1, "duplicate on_error");
    static_assert(OverflowCount <= 1, "duplicate on_overflow");

  public:
    B& Ref;
    P  Policy;
    // `[[no_unique_address]]` is load-bearing: each captureless action lambda
    // is an empty type, and without this attribute the tuple would pad each
    // one out to a byte. With it, `policy_ref<B, P>` carrying no actions has
    // the same size as `policy_ref<B, P, no_action>`.
    [[no_unique_address]] std::tuple<As...> Actions;

    // Stores go through dispatch_assign under this ref's Policy and actions:
    // into Ref for `operator=`, into a fresh target for the conversion below,
    // so a one-shot snap can be read out as a value (`(a * b).with_snap()`).
    template <numeric C>
    constexpr B& operator=(const C& other) {
        return dispatch_assign(Ref, other, Policy, Actions);
    }

    // Value read-out: a one-shot policy ref converts to any inside the assignment
    // could satisfy, applying the target's own policy (range) plus this ref's
    // carried flags (notch/rounding) via has_policy's merge. Makes
    // `Target t = (a * b).with_snap();` / `return (a * b).with_snap();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <insidable Target>
        requires inside_assignable<Target, B, flags_of<Target> | flags_of<P>>
    constexpr operator Target() {
        return dispatch_new<Target>(Ref, Policy, Actions);
    }

    // expected<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    // An error goes through this ref's policy (on_overflow / on_error, or the
    // errc& sink), as an arithmetic result's does.
    template <numeric C>
    constexpr B& operator=(const std::expected<C, errc>& other) {
        return finalize_arith(other, "policy_ref::operator= of an expected without a value");
    }

  private:
    // The value an op's result holds (an expected's value type) assigns back.
    template <typename R>
    static constexpr bool stores_back = inside_assignable<B, detail::unwrap_t<R>, flags_of<B> | flags_of<P>>;

  public:
    //-------------------------------------------------------------------------
    // insidable RHS overloads — route through the inside's arithmetic, then
    // assign via dispatch_assign so callbacks fire on the narrowing back to B.
    // An expected<inside> result carrying an error (rational-raw overflow,
    // division by zero) surfaces its errc through on_overflow if registered,
    // else report. Constrained on the free op they call and on storing its
    // result back under this policy, so a requires probe sees what compiles:
    // a 1/16 sum does not store into an integer grid without snap.
    //-------------------------------------------------------------------------
    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { add(b, c, p); } &&
                 stores_back<decltype(add(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator+=(const C& rhs) {
        return finalize_arith(add(Ref, rhs, Policy), "policy_ref::operator+= overflow");
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { sub(b, c, p); } &&
                 stores_back<decltype(sub(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator-=(const C& rhs) {
        return finalize_arith(sub(Ref, rhs, Policy), "policy_ref::operator-= overflow");
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mul(b, c, p); } &&
                 stores_back<decltype(mul(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator*=(const C& rhs) {
        return finalize_arith(mul(Ref, rhs, Policy), "policy_ref::operator*= overflow");
    }

    // A zero divisor is reported (on_error / ignore_zero) and leaves Ref
    // unchanged, like inside::operator/=: div/mod under ignore_zero skip their
    // own check, so dividing here would be a division by zero.
    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { div(b, c, p); } &&
                 stores_back<decltype(div(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator/=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (!has_flag(flags_of<C>, ignore_zero)) // either operand silences it
                report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return finalize_arith(div(Ref, rhs, Policy), "policy_ref::operator/= division/overflow");
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mod(b, c, p); } &&
                 stores_back<decltype(mod(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator%=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (!has_flag(flags_of<C>, ignore_zero)) // either operand silences it
                report_zero(errc::division_by_zero, "policy_ref::operator%= division by zero");
            return Ref;
        }
        return finalize_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow");
    }

    //-------------------------------------------------------------------------
    // rational RHS overloads — the only non-inside operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_ins` / `just<1>` / `inside<{lo,hi}>{n}`).
    // Ref as a rational, checked op, finalize_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(const C& rhs) {
        return finalize_arith(rational{Ref} + rhs, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(const C& rhs) {
        return finalize_arith(rational{Ref} - rhs, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(const C& rhs) {
        return finalize_arith(rational{Ref} * rhs, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(const C& rhs) {
        if (is_canonical_zero(rhs)) {
            report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return finalize_arith(rational{Ref} / rhs, "policy_ref::operator/= division/overflow");
    }

  private:
    constexpr void report_zero(errc code, const char* what) {
        if constexpr (has_action<is_error_action, As...>)
            pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
        else if constexpr (!has_policy<B, P, ignore_zero>)
            Policy.report(code);
    }

    template <typename R>
    constexpr B& finalize_arith(R&& result, [[maybe_unused]] const char* msg) {
        if constexpr (expected_like<R>) {
            if (!result.has_value()) [[unlikely]] {
                if (result.error() == errc::division_by_zero)
                    report_zero(errc::division_by_zero, msg); // on_error / ignore_zero, like rational /=
                else if constexpr (has_action<is_overflow_action, As...>)
                    pick_action_in<is_overflow_action>(Actions).Fn(Ref, result.error());
                else
                    Policy.report(result.error());
                return Ref;
            }
            return dispatch_assign(Ref, result.value(), Policy, Actions);
        } else
            return dispatch_assign(Ref, std::forward<R>(result), Policy, Actions);
    }
};
} // namespace detail

} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/addition.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/rep.hpp
// ======================================================================



namespace beman::inside::detail {
enum class exact_op : unsigned char { add, mul, div };

// An exact L op R into the continuous Result that the grids prove cannot
// overflow. Result carries a denominator bound (grid.hpp: den_bound), so its
// raw holds every value: a fraction raw is sized from the bound; a 64-bit
// rational needs the unchecked kernels' intermediates in range too —
//   +: each operand over the common denominator D_L·D_R fits umax;
//   ×: nothing more (numerator ≤ num_bound(Result), denominator ≤ D_L·D_R);
//   ÷: the divisor's numerator fits imax (1/r moves it to the denominator).
template <exact_op Op, insidable Result, insidable L, insidable R>
inline constexpr bool exact_fits = [] {
    if constexpr (grid_of<Result>.MaxDenominator == 0)
        return false;
    else if constexpr (fraction_storage<Result>)
        return true;
    else if constexpr (!rational_storage<Result>)
        return false;
    else {
        const grid_int umax_max{std::numeric_limits<umax>::max()}, imax_max{std::numeric_limits<imax>::max()};
        if constexpr (Op == exact_op::add) {
            const grid_int d = den_bound(grid_of<L>) * den_bound(grid_of<R>);
            return !(umax_max < num_bound(interval_of<L>, d)) && !(umax_max < num_bound(interval_of<R>, d));
        } else if constexpr (Op == exact_op::div)
            return !(imax_max < num_bound(grid_of<R>));
        else
            return true;
    }
}();

// Whether a + or × into Result runs its overflow check. The result grid
// holds every result, so only a continuous result can overflow, and not one
// whose denominator bound proves it fits (exact_fits): otherwise a fraction
// raw always may (its denominator grows); a rational raw under a checked
// policy (a continuous operand bounds no denominator).
template <exact_op Op, insidable Result, insidable L, insidable R, policy_flag F>
inline constexpr bool lattice_op_checked =
    !exact_fits<Op, Result, L, R> &&
    (fraction_storage<Result> ||
     (rational_storage<Result> && (has_any_flag(F, checked) || is_checked(flags_of<L>) || is_checked(flags_of<R>))));

// `x * just<c>` and `x / just<c>` (c != 0): the result lattice is x's lattice
// scaled by c or 1/c (grid operator* / operator/), so the result offset IS x's
// offset — counted from the far end when the scale is negative. No multiply
// or divide at all.
template <insidable Point, insidable X, insidable Result>
inline constexpr bool point_scale =
    point_grid<Point> && lower_of<Point> != 0 && notched<X> && !wide_valued<X> && !wide_valued<Result>;

template <insidable Result, bool Negate, insidable X>
constexpr Result scale_by_point(const X& x) noexcept {
    static_assert(max_index_of<Result> == max_index_of<X>);
    const umax off = raw_offset<X>(x.raw());
    return Result::from_raw(raw_from_offset<Result>(Negate ? max_index_of<X> - off : off));
}
} // namespace beman::inside::detail



//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = grid_of<L> + grid_of<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on continuous results.
// Shapes: a continuous result (exact fraction, or a rational raw), a point,
// or integer raws added as value counts in a common unit.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct addition {
    static constexpr grid result_grid = grid_of<L> + grid_of<R>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check = lattice_op_checked<exact_op::add, result, L, R, F>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> return_t<F, A> {
        result res;
        if constexpr (fraction_storage<result>) {
            const auto sum = continuous_raw_of<raw_t<result>>(exact_of(lhs) + exact_of(rhs));
            if constexpr (needs_overflow_check<F>)
                if (!sum) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow);
            res = result::from_raw(*sum);
        } else if constexpr (rational_storage<result>) {
            static_assert(!wide_valued<L> && !wide_valued<R>,
                          "addition: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<F>) {
                auto sum = rational::add(lhs, rhs);
                if (!sum) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow);
                res = result::from_raw(*sum);
            } else
                res = result::from_raw(rational::add_unchecked(lhs, rhs));
        } else if constexpr (point_storage<result>)
            res = result::from_raw(raw_t<result>{}); // point + point: a point
        else {
            // Integer raws (a notched result has no continuous operand): add the
            // values in a unit dividing both operands' (the result notch
            // gcd(N_L, N_R) on anchored grids), in imax or by wrapping arithmetic
            // (wide_value.hpp). Exact for every grid, at any width.
            static_assert(integer_storage<L> && integer_storage<R>);
            constexpr grid_rational U = gcd(unit_of<L>, unit_of<R>);
            using W                   = index_work_t<result, L, U, R, U, U>;
            res = from_value_in_units<result, U>(value_in_units<W, U>(lhs) + value_in_units<W, U>(rhs));
        }
        return res;
    }
};

// Whether `l + r` has a result, as a constraint of add / + / += (so a
// requires-expression or `if constexpr` probe gets the answer instead of a
// hard error): no wide-index operand with a continuous result. (F, the
// call's flags, is taken for symmetry with div_defined; a sum ignores it.)
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool add_defined =
    !rational_storage<typename addition<L, R>::result> || (!wide_valued<L> && !wide_valued<R>);

// The same for `l - r` (l + (−r)).
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool sub_defined = add_defined<L, decltype(-std::declval<const R&>())>;
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/detail/multiplication.hpp
// ======================================================================



//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> inside<grid_of<L> * grid_of<R>>`.
// Integer raws multiply their value indices (wide_value.hpp), in imax when the
// grids' bounds allow, else by wrapping arithmetic as wide as the result raw.
// Point scaling and rational results have their own branches.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable L, insidable R = L>
struct multiplication {
    static constexpr grid result_grid = grid_of<L> * grid_of<R>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check = lattice_op_checked<exact_op::mul, result, L, R, F>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> return_t<flags_of<P>, A> {
        if constexpr (point_scale<R, L, result>) // x * just<c>: x's offset (see rep.hpp)
            return scale_by_point<result, (lower_of<R> < 0)>(lhs);
        else if constexpr (point_scale<L, R, result>)
            return scale_by_point<result, (lower_of<L> < 0)>(rhs);
        else if constexpr (fraction_storage<result>) {
            const auto prod = continuous_raw_of<raw_t<result>>(exact_of(lhs) * exact_of(rhs));
            if constexpr (needs_overflow_check<flags_of<P>>)
                if (!prod) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow);
            return result::from_raw(*prod);
        } else if constexpr (rational_storage<result>) {
            static_assert(!wide_valued<L> && !wide_valued<R>,
                          "multiplication: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<flags_of<P>>) {
                auto prod = as_rational(lhs) * as_rational(rhs);
                if (!prod) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow);
                return result::from_raw(*prod);
            } else
                return result::from_raw(rational::mul_unchecked(as_rational(lhs), as_rational(rhs)));
        } else if constexpr (point_storage<result>)
            return result::from_raw(
                raw_t<result>{}); // a one-point result (point × point, or × 0): its value is the grid's
        else {
            // Integer raws (a notched result has no continuous operand): multiply the operands' values in their own
            // units (a notch or value unit, or |c| for a point c), in imax or by wrapping arithmetic (wide_value.hpp).
            // The product of the unit counts counts the product in the product of the units — on anchored grids the
            // result notch — exact for every grid and sign, at any width.
            static_assert(integer_storage<L> && integer_storage<R>);
            constexpr grid_rational U = unit_of<L> * unit_of<R>;
            using W                   = index_work_t<result, L, unit_of<L>, R, unit_of<R>, U>;
            return from_value_in_units<result, U>(value_in_units<W, unit_of<L>>(lhs) *
                                                  value_in_units<W, unit_of<R>>(rhs));
        }
    }
};

// Whether `l * r` has a result, as a constraint of mul / * / *=: no
// wide-index operand with a continuous result.
template <insidable L, insidable R, policy_flag = none>
inline constexpr bool mul_defined = [] {
    using result = typename multiplication<L, R>::result;
    return point_scale<R, L, result> || point_scale<L, R, result> || !rational_storage<result> ||
           (!wide_valued<L> && !wide_valued<R>);
}();
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/detail/division.hpp
// ======================================================================




//---------------------------------------------------------------------------
// division / modulo. `division::div` returns expected<result, errc> unless the
// divisor's grid excludes zero and the quotient provably fits (then a plain
// result). The paths, in order: Q-format (same notch + snap), native integer
// and wide integer (integer-aligned grids + snap), point scale (a nonzero
// point divisor keeps the dividend's lattice), exact wide fractions, an
// integer quotient that fits, and the rational paths (checked: may overflow).
// `modulo::mod` is integer-only — non-integer remainders aren't well-defined
// on fractional notches.
//---------------------------------------------------------------------------
namespace beman::inside {
// divmod's result: the rounded quotient and its remainder, for
// `auto [q, r] = divmod(a, b);`.
template <insidable Q, insidable R>
struct divmod_result {
    Q Quotient;
    R Remainder;
};
} // namespace beman::inside

namespace beman::inside::detail {
// Both operands are plain integer grids and the caller accepted integer
// truncation (snap) — the prerequisite for native integer div / mod.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_ops = ((F | flags_of<L> | flags_of<R>)&snap) && integer_lattice<L> && integer_lattice<R>;

// ...and every value fits imax, so the builtin integer division applies.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_native_ops = integer_ops<L, R, F> && values_fit_imax<L> && values_fit_imax<R>;

// Integer operands past imax (a uint64 grid, the 2^63 of INT64_MIN / -1, a
// big grid): the rounded quotient in exact wide integers, on a grid of the
// corner quotients rounded the same way.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_wide_ops = integer_ops<L, R, F> && !integer_native_ops<L, R, F>;

// Whether `l / r`, `l % r` and divmod have a result under flags F, as
// constraints of div / mod / divmod and their operators: the quotient grid
// exists (not a zero divisor grid), and the remainder needs integer-valued
// grids.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool div_defined =
    (integer_native_ops<L, R, F> || integer_wide_ops<L, R, F>) ? lower_of<R> != 0 || upper_of<R> != 0
                                                               : grid_quotient_fits(grid_of<R>);

// (A zero divisor grid has no remainder grid: |r| < |b| holds no value.)
template <insidable L, insidable R, policy_flag F>
inline constexpr bool mod_defined = integer_ops<L, R, F | snap> && (lower_of<R> != 0 || upper_of<R> != 0);

template <insidable L, insidable R, policy_flag F>
inline constexpr bool divmod_defined = integer_ops<L, R, F | snap> && div_defined<L, R, F | snap>;

//---------------------------------------------------------------------------
// Rounding mode for the native div & mod paths (fire when `snap` is set).
// Decided from the combined flags by rounding_of (policy_flag.hpp), the one
// precedence all rounding paths share; `snap` alone is truncate-toward-zero.
// The runtime quotient and the compile-time grid endpoints MUST agree on the
// mode (both read rounding_of), or a result could escape its own grid.
//---------------------------------------------------------------------------

// The signed exact quotient a/b (b != 0) rounded to an integer per `m`, and
// the remainder a − q·b consistent with it (computed without forming q·b,
// which can pass T when q rounds away from zero).
template <std::signed_integral T>
struct rounded_division {
    T Quotient, Remainder;
};

template <std::signed_integral T>
constexpr rounded_division<T> div_rem_rounded(T a, T b, round_mode m) noexcept {
    using U = std::make_unsigned_t<T>;
    if (b == -1) // exact; a / -1 and a % -1 are UB for T::min (−T::min wraps: no caller's grid holds it)
        return {T(U(0) - U(a)), T(0)};
    const T    t   = a / b;                        // C++ truncation toward zero
    const T    r   = a % b;                        // sign of a, |r| < |b|
    const bool neg = (a < 0) != (b < 0);           // exact quotient is negative
    const U    ar  = r < 0 ? U(~U(r) + 1u) : U(r); // |r|, |b| in U (safe for T::min)
    const U    ab  = b < 0 ? U(~U(b) + 1u) : U(b);
    if (!rounds_away(m, neg, classify_remainder(m, ar, ab), (t & 1) != 0))
        return {t, r};
    // One step away from zero; r and b differ in sign (neg) or agree, so the
    // adjusted remainder stays below |b| in magnitude.
    return neg ? rounded_division<T>{T(t - 1), T(r + b)} : rounded_division<T>{T(t + 1), T(r - b)};
}

template <std::signed_integral T>
constexpr T div_rounded(T a, T b, round_mode m) noexcept {
    return div_rem_rounded(a, b, m).Quotient;
}

// The native quotient q of a / b stored into Q. Q's grid reaches 2^63 when
// a can be INT64_MIN and b can be −1 — past from_value's imax, and where
// div_rem_rounded's q wraps — so such a Q stores through its exact index.
template <insidable Q, std::signed_integral T>
constexpr Q native_quotient(T a, T b, T q) noexcept {
    Q res;
    if constexpr (values_fit_imax<Q>)
        from_value(res, imax{q});
    else {
        constexpr std::size_t K = exact_limbs<Q>;
        using I                 = wide_sint<K>;
        const I v               = a == std::numeric_limits<T>::min() && b == T{-1} ? -I{a} : I{q};
        res                     = exact_result<Q>(wide_rational<K>{v, I{1}});
    }
    return res;
}

// The narrowest signed type in which native div/mod of L by R is exact: int32
// when both value ranges fit (excluding INT32_MIN, so a / -1 cannot overflow),
// else imax. A 32-bit divide is markedly cheaper than a 64-bit one.
template <insidable L, insidable R>
using native_div_t = std::conditional_t<(lower_imax<L> > std::numeric_limits<std::int32_t>::min() &&
                                         upper_imax<L> <= std::numeric_limits<std::int32_t>::max() &&
                                         lower_imax<R> > std::numeric_limits<std::int32_t>::min() &&
                                         upper_imax<R> <= std::numeric_limits<std::int32_t>::max()),
                                        std::int32_t,
                                        imax>;

// The zero-divisor check is skipped when R's grid excludes zero or
// `ignore_zero` is set (a zero divisor is then UB, matching the `/= 0` no-op).
template <insidable L, insidable R, policy_flag F, policy_flag G>
inline constexpr bool divisor_unchecked =
    divisor_excludes_zero<R> || ((G | F | flags_of<L> | flags_of<R>)&ignore_zero) != 0;

// Compile-time rounding of a quotient-interval endpoint to an integer index.
// Under half_even the endpoint is bracketed by [floor, ceil] (Upper: ceil)
// rather than reproducing the parity rule at compile time.
constexpr rational round_rat(rational q, round_mode m, bool upper) noexcept {
    if (m == round_mode::half_even)
        m = upper ? round_mode::ceil : round_mode::floor;
    return round_to_integral(q, m); // exact: INT64_MIN / -1 reaches 2^63
}

// Every value of a 64-bit grid with a nonzero notch (or a point grid) is
// n/d with d | Den and |n| ≤ Num: Den = lcm(den(lower), den(notch)) (the
// upper limit's denominator divides it too), Num = max(|lower|, |upper|)·Den.
// Ok is false when a bound passes 64 bits.
struct value_bounds {
    umax Num, Den;
    bool Ok;
};

template <insidable B>
constexpr value_bounds value_bounds_of() noexcept {
    const rational lo = lower64<B>, hi = upper64<B>, n = notch64<B>;
    const umax     dl = abs_den(lo.Denominator), dn = n.Numerator == 0 ? umax{1} : abs_den(n.Denominator);
    umax           den, nlo, nhi;
    if (mul_overflow(dl / std::gcd(dl, dn), dn, &den))
        return {0, 0, false};
    if (mul_overflow(lo.Numerator, den / dl, &nlo))
        return {0, 0, false};
    if (mul_overflow(hi.Numerator, den / abs_den(hi.Denominator), &nhi))
        return {0, 0, false};
    return {nlo > nhi ? nlo : nhi, den, true};
}

// The checked rational quotient (a/b)/(c/d) = (a·d)/(b·c) cannot overflow
// for any values of L and R: c, a·d and b·c stay within the rational's
// fields (rational::div_impl). Bounded only for 64-bit integer raws; a
// continuous operand gives false.
template <insidable L, insidable R>
constexpr bool quotient_fits_rational() noexcept {
    if constexpr (wide_valued<L> || wide_valued<R>)
        return false;
    else {
        if (!integer_storage<L> || !integer_storage<R>) // a continuous operand bounds no denominator
            return false;
        constexpr value_bounds l = value_bounds_of<L>(), r = value_bounds_of<R>();
        umax                   ad, bc;
        return l.Ok && r.Ok && std::in_range<imax>(r.Num) && !mul_overflow(l.Num, r.Den, &ad) &&
               !mul_overflow(l.Den, r.Num, &bc) && std::in_range<imax>(bc);
    }
}

template <insidable L, insidable R = L, policy_flag F = none>
struct division {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_fixed   — both on one notch 1/K (K > 1), anchored, any sign:
    //                        the quotient's index on that notch is
    //                        round(ja·K / jb) of the value indices (the native
    //                        `(a << log2 K)/b` idiom for a power-of-two K).
    // Otherwise the exact-rational path returns a continuous quotient.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    // (if constexpr: naming a 64-bit view instantiates it, even where && would
    // skip it — so an exact-valued operand returns before any is named.)
    static constexpr bool native_div_fixed = [] {
        if constexpr (wide_grid_numbers<L> || wide_grid_numbers<R> || !notched<L> || notch_of<L> != notch_of<R> ||
                      !anchored<L> || !anchored<R> || wide_numerator(notch_of<L>) != grid_int{1} ||
                      !(grid_int{1} < wide_denominator(notch_of<L>)) || !(grid_of<L> / grid_of<R>).has_value())
            return false;
        else
            return ((F | flags_of<L> | flags_of<R>)&snap) != 0;
    }();

    static constexpr bool wide_div_integer = integer_wide_ops<L, R, F>;

    static constexpr bool native_div = native_div_integer || native_div_fixed || wide_div_integer;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode = rounding_of(F | flags_of<L> | flags_of<R>);

    // A clear diagnostic when there is no result grid, instead of the
    // expected-deref below failing cryptically. The native integer path forms
    // no exact quotient grid (below).
    static_assert(div_defined<L, R, F>,
                  "division: no result grid - the divisor grid is the point 0, or a continuous divisor without "
                  "max_denominator reaches 0");

    // The native integer quotient's range: the corner quotients rounded by
    // the runtime's mode (half_even: the low end by floor, the high by ceil,
    // as round_rat). a/b is monotonic in each operand on either side of 0,
    // and so is its rounding, so these are the rounded endpoints of the exact
    // quotient grid — which is not formed: its endpoints can pass the 64-bit
    // rational (7/2^63 for a divisor reaching INT64_MIN). Rounded in umax,
    // sign apart, so limits past imax and INT64_MIN / −1 = 2^63 need no
    // special case: |a / b| ≤ |a| for an integer |b| ≥ 1. Grid numbers past
    // 64 bits (a big grid) round the same way in grid integers.
    static constexpr interval integer_quotient_range() {
        const round_mode lo_mode = rmode == round_mode::half_even ? round_mode::floor : rmode;
        const round_mode hi_mode = rmode == round_mode::half_even ? round_mode::ceil : rmode;
        constexpr bool   big     = wide_grid_numbers<L> || wide_grid_numbers<R>;
        using M                  = std::conditional_t<big, grid_int, umax>;
        using V                  = std::conditional_t<big, grid_rational, rational>;
        struct signed_mag {
            M    Mag;
            bool Neg;
        };
        auto of = [](const grid_rational& g) {
            if constexpr (big)
                return signed_mag{abs(wide_numerator(g)), wide_numerator(g).negative()};
            else {
                const rational r = to_rational(g);
                return signed_mag{r.Numerator, r.Denominator < 0 && r.Numerator != 0};
            }
        };
        auto quotient = [](const signed_mag& a, const signed_mag& b, round_mode m) {
            const M    q = a.Mag / b.Mag, r = a.Mag % b.Mag;
            const bool neg = a.Neg != b.Neg && a.Mag != M{0};
            const M    mag = rounds_away(m, neg, classify_remainder(m, r, b.Mag), q % M{2} != M{0}) ? q + M{1} : q;
            return neg ? -V{mag} : V{mag};
        };
        const signed_mag lo = of(lower_of<R>), hi = of(upper_of<R>);
        const M          step = of(grid_of<R>.value_unit()).Mag;
        signed_mag       divisors[4];
        int              n = 0;
        if ((!lo.Neg && lo.Mag != M{0}) || hi.Neg) { // zero-free: the limits
            divisors[n++] = lo;
            divisors[n++] = hi;
        } else {
            if (hi.Mag != M{0}) {
                divisors[n++] = signed_mag{step, false};
                divisors[n++] = hi;
            }
            if (lo.Neg) {
                divisors[n++] = lo;
                divisors[n++] = signed_mag{step, true};
            }
        }
        if (n == 0) // a zero divisor grid (the static_assert above)
            return interval{0, 0};
        const signed_mag a0 = of(lower_of<L>);
        V                ql = quotient(a0, divisors[0], lo_mode), qh = quotient(a0, divisors[0], hi_mode);
        for (const signed_mag& a : {a0, of(upper_of<L>)})
            for (int i = 0; i < n; ++i) {
                const V l = quotient(a, divisors[i], lo_mode), h = quotient(a, divisors[i], hi_mode);
                if (l < ql)
                    ql = l;
                if (qh < h)
                    qh = h;
            }
        return interval{ql, qh};
    }

    // The native paths' endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid: onto the integers,
    // or onto the operands' notch 1/K.
    static constexpr grid result_grid = [] {
        if constexpr (native_div_integer || wide_div_integer)
            return grid{integer_quotient_range(), rational{1}};
        else if constexpr (native_div_fixed) {
            constexpr interval q = (*(grid_of<L> / grid_of<R>)).Interval;
            // An endpoint on the notch stays; one off it rounds onto it.
            constexpr rational n = notch64<L>, k = (rational{1} / n).value();
            auto               on = [&](const grid_rational& x, bool up) {
                return grid_divides_evenly(x, notch_of<L>)
                           ? to_rational(x)
                           : (round_rat((to_rational(x) * k).value(), rmode, up) * n).value();
            };
            return grid{interval{on(q.Lower, false), on(q.Upper, true)}, n};
        } else
            return *(grid_of<L> / grid_of<R>);
    }();

    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    // The exact quotient of two anchored integer raws as one 64-bit fraction:
    // |J_L|·p_L·q_R and |J_R|·p_R·q_L bounded by 2^62 for every value.
    static constexpr bool int_quotient_fits = [] {
        if constexpr (!rational_storage<result> || !notched<L> || !notched<R> || !anchored<L> || !anchored<R> ||
                      wide_valued<L> || wide_valued<R>)
            return false;
        else {
            constexpr grid_int lim   = grid_int{1} << 62;
            auto               max_j = []<insidable B>() {
                const grid_int lo = exact_quotient(lower_of<B>, notch_of<B>),
                               hi = exact_quotient(upper_of<B>, notch_of<B>);
                const grid_int a = abs(lo), b = abs(hi);
                return a < b ? b : a;
            };
            const grid_int        a = wide_numerator(notch_of<L>) * wide_denominator(notch_of<R>);
            const grid_int        b = wide_numerator(notch_of<R>) * wide_denominator(notch_of<L>);
            return max_j.template operator()<L>() * a < lim && max_j.template operator()<R>() * b < lim;
        }
    }();

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F, checked) || is_checked(flags_of<L>) || is_checked(flags_of<R>);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>. The
    // operand grids may prove the quotient fits (quotient_fits_rational).
    // A wide-index operand's quotient may outgrow the 64-bit rational
    // whatever the policy, so that path always reports.
    static constexpr bool fits_rational = quotient_fits_rational<L, R>();

    // `x / just<c>` (c != 0) off the native paths: the result keeps x's
    // lattice scaled by 1/|c| (grid operator/), so it is x's offset (see
    // rep.hpp) — or, past 64 bits, the exact quotient on that lattice.
    // Exact and in range by construction: it cannot fail.
    static constexpr bool point_divisor = !native_div && point_grid<R> && lower_of<R> != 0 && notched<result>;

    // A continuous quotient whose denominator bound proves it fits (rep.hpp)
    // cannot overflow either.
    static constexpr bool bound_fits = exact_fits<exact_op::div, result, L, R>;

    static constexpr bool may_overflow_nonzero =
        !native_div && !point_divisor && !fits_rational && !bound_fits &&
        (needs_overflow_check<F> != 0 || wide_valued<L> || wide_valued<R> || wide_valued<result>);

    // Plain `result` when the op cannot fail (overflow-action, or the divisor
    // grid excludes zero with no rational overflow), else expected<result, errc>.
    template <typename A>
    using return_t =
        std::conditional_t<overflow_action<plain_t<A>> || (divisor_excludes_zero<R> && !may_overflow_nonzero),
                           result,
                           std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
};

//---------------------------------------------------------------------------
// div
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F>
template <policy_flag G, typename E, typename A>
constexpr auto division<L, R, F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> return_t<A> {
    // `fail` must stay well-formed even when return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    [[maybe_unused]] auto fail = [&]([[maybe_unused]] errc code) -> return_t<A> {
        if constexpr (overflow_action<plain_t<A>> || !divisor_excludes_zero<R> || may_overflow_nonzero)
            return report_or_unexpected<result>(action, policy, code); // result, or expected<result, errc>
        else
            return result{}; // unreachable: divisor excludes zero, op cannot fail
    };

    // The fail arms stay keyed on divisor_excludes_zero (which narrows the
    // return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;

    if constexpr (native_div_fixed) {
        // ja·K, jb and the quotient in the narrowest signed type that holds
        // them: a 32-bit divide where it fits (Q8.8, Q15, ...), else 64 bits,
        // else a wide_int. The quotient's magnitude is at most |ja·K|; −ja·K
        // is listed for the one it can reach past the type, min / −1.
        constexpr grid_int K     = wide_denominator(notch_of<L>);
        constexpr grid_int ja_lo = exact_quotient(lower_of<L>, notch_of<L>) * K;
        constexpr int      bits  = signed_value_bits_of({ja_lo,
                                                         -ja_lo,
                                                         exact_quotient(upper_of<L>, notch_of<L>) * K,
                                                         exact_quotient(lower_of<R>, notch_of<R>),
                                                         exact_quotient(upper_of<R>, notch_of<R>)});
        using W    = std::conditional_t<(bits <= 31), std::int32_t, work_int_t<bits>>; // bits: without the sign
        const W jb = value_index<W>(rhs);
        if constexpr (!zero_unchecked)
            if (jb == W{0})
                return fail(errc::division_by_zero);
        const W ja = value_index<W>(lhs) * static_cast<W>(K);
        if constexpr (std::is_integral_v<W>)
            return from_value_index<result>(div_rounded(ja, jb, rmode));
        else
            return from_value_index<result>(rounded_div<rmode>(ja, jb));
    } else if constexpr (native_div_integer) {
        using T         = native_div_t<L, R>;
        const T rhs_val = static_cast<T>(to_value(rhs));
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return fail(errc::division_by_zero);
        const T lhs_val = static_cast<T>(to_value(lhs));
        return native_quotient<result>(lhs_val, rhs_val, div_rounded(lhs_val, rhs_val, rmode));
    } else if constexpr (wide_div_integer) {
        // Integer values past imax: the rounded quotient in exact wide
        // integers, as modulo's remainder.
        constexpr std::size_t K = exact_limbs<L, R, result>;
        using I                 = wide_sint<K>;
        const I b               = value_in_units<I, grid_rational{1}>(rhs); // integer values: no division
        if constexpr (!zero_unchecked)
            if (b.is_zero())
                return fail(errc::division_by_zero);
        const I a = value_in_units<I, grid_rational{1}>(lhs);
        return exact_result<result>(wide_rational<K>{rounded_div<rmode>(a, b), I{1}});
    } else if constexpr (point_divisor) {
        if constexpr (point_scale<R, L, result>)
            return scale_by_point<result, (lower_of<R> < 0)>(lhs);
        else
            return exact_result<result>(exact_of(lhs) / exact_of(rhs));
    } else if constexpr (wide_valued<L> || wide_valued<R> || wide_valued<result>) {
        // A wide-index or big-grid operand: the exact quotient, in the result's raw.
        const auto d = exact_of(rhs);
        if constexpr (!zero_unchecked)
            if (d.Num.is_zero())
                return fail(errc::division_by_zero);
        if constexpr (fraction_storage<result>) {
            // Grids past 64 bits: the exact quotient in the result's wide fraction.
            const auto q = continuous_raw_of<raw_t<result>>(exact_of(lhs) / d);
            if constexpr (!bound_fits)
                if (!q) [[unlikely]]
                    return fail(errc::overflow);
            return result::from_raw(*q);
        } else {
            const auto q = try_rational(exact_of(lhs) / d);
            if constexpr (!bound_fits)
                if (!q) [[unlikely]]
                    return fail(errc::overflow);
            return result::from_raw(*q);
        }
    } else if constexpr (int_quotient_fits) {
        // Two integer raws: J_L·Notch_L / (J_R·Notch_R) as one fraction of
        // value indices, reduced once — no operand decodes, no checked divide.
        constexpr imax a =
            static_cast<imax>(notch64<L>.Numerator) * static_cast<imax>(abs_den(notch64<R>.Denominator));
        constexpr imax b =
            static_cast<imax>(notch64<R>.Numerator) * static_cast<imax>(abs_den(notch64<L>.Denominator));
        const imax d = value_index<imax>(rhs) * b;
        if constexpr (!zero_unchecked)
            if (d == 0)
                return fail(errc::division_by_zero);
        const imax n = value_index<imax>(lhs) * a;
        return result::from_raw(d < 0 ? rational{-n, -d} : rational{n, d});
    } else if constexpr (needs_overflow_check<G> && !fits_rational && !bound_fits) {
        rational rhs_r = rhs;
        if constexpr (!zero_unchecked)
            if (rhs_r.Numerator == 0)
                return fail(errc::division_by_zero);
        auto q = as_rational(lhs) / rhs_r;
        if (!q) [[unlikely]]
            return fail(errc::overflow);
        return result::from_raw(*q);
    } else {
        rational rhs_r = rhs;
        if constexpr (!zero_unchecked)
            if (rhs_r.Numerator == 0)
                return fail(errc::division_by_zero);
        return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
}
//---------------------------------------------------------------------------
// modulo (requires integer-valued grids)
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F = none>
struct modulo {
    // `a mod b` is the remainder of an integer quotient, so it always rounds:
    // truncation (C++ `%`) unless the policy names a mode — with or without
    // `snap`, which only decides whether `/` rounds.
    static constexpr policy_flag FS = F | snap;

    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned.
    static_assert(integer_ops<L, R, FS>, "modulo requires integer-valued grids");

    // Builtin division when every value fits imax; else exact wide integers.
    static constexpr bool native_mod = integer_native_ops<L, R, FS>;

    static constexpr grid_rational max_rem =
        (abs(lower_of<R>) > abs(upper_of<R>) ? abs(lower_of<R>) : abs(upper_of<R>)) - grid_rational{1};

    static constexpr round_mode rmode = rounding_of(F | flags_of<L> | flags_of<R>);

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr grid result_grid =
        (rmode == round_mode::trunc && lower_of<L> >= 0) ? grid{grid_rational{0}, max_rem} : grid{-max_rem, max_rem};

    using result = inside<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
    template <typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || divisor_excludes_zero<R>, result, std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
};

template <insidable L, insidable R, policy_flag F>
template <policy_flag G, typename E, typename A>
constexpr auto modulo<L, R, F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> return_t<A> {
    if constexpr (!native_mod) {
        // Integer values past imax: r = a − round(a/b)·b in exact wide integers.
        constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;
        using I                       = wide_sint<exact_limbs<L, R, result>>;
        const I b                     = value_in_units<I, grid_rational{1}>(rhs); // integer values: no division
        if constexpr (!zero_unchecked)
            if (b.is_zero())
                return report_or_unexpected<result>(action, policy, errc::division_by_zero);
        const I a = value_in_units<I, grid_rational{1}>(lhs);
        return exact_result<result>(wide_rational<exact_limbs<L, R, result>>{a - rounded_div<rmode>(a, b) * b, I{1}});
    } else {
        using T                       = native_div_t<L, R>;
        const T        rhs_val        = static_cast<T>(to_value(rhs));
        constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return report_or_unexpected<result>(action, policy, errc::division_by_zero);
        result res;
        // Remainder consistent with the rounded quotient (trunc → C++ `%`).
        const T lhs_val = static_cast<T>(to_value(lhs));
        from_value(res, imax{div_rem_rounded(lhs_val, rhs_val, rmode).Remainder});
        return res;
    }
}

//---------------------------------------------------------------------------
// divmod (what `%` requires): the rounded quotient and the remainder
// consistent with it, r = a − q·b, from one division (native within imax,
// else exact wide integers). The two halves have the grids of `a / b` and
// `a % b`.
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F = none>
struct quotient_remainder {
    // Rounds like `%`, so like `%` it needs no `snap`.
    static constexpr policy_flag FS = F | snap;

    static_assert(integer_ops<L, R, FS>, "divmod requires integer-valued grids");

    static constexpr round_mode rmode = rounding_of(FS | flags_of<L> | flags_of<R>);

    using result = divmod_result<typename division<L, R, FS>::result, typename modulo<L, R, FS>::result>;

    template <typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || divisor_excludes_zero<R>, result, std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> divmod(L lhs, R rhs, policy<G, E> policy = {}, A&& action = {}) {
        // (Generic: its body is formed only where a zero divisor is possible.)
        auto fail = [&](auto code) -> return_t<A> {
            if constexpr (overflow_action<plain_t<A>>) {
                result res{};
                action.Fn(res.Quotient, code);
                return res;
            } else {
                if constexpr (uses_error_ref<std::remove_cvref_t<decltype(policy)>>)
                    policy.report(code);
                return std::unexpected{code};
            }
        };
        if constexpr (!integer_native_ops<L, R, FS>) {
            // Integer values past imax: q and r = a − q·b in exact wide
            // integers, as `/` and `%` do.
            using Q                 = typename division<L, R, FS>::result;
            using M                 = typename modulo<L, R, FS>::result;
            constexpr std::size_t K = exact_limbs<L, R, Q, M>;
            using I                 = wide_sint<K>;
            const I b               = value_in_units<I, grid_rational{1}>(rhs); // integer values: no division
            if constexpr (!divisor_unchecked<L, R, F, G>)
                if (b.is_zero())
                    return fail(errc::division_by_zero);
            const I a = value_in_units<I, grid_rational{1}>(lhs);
            const I q = rounded_div<rmode>(a, b);
            return result{exact_result<Q>(wide_rational<K>{q, I{1}}),
                          exact_result<M>(wide_rational<K>{a - q * b, I{1}})};
        } else {
            using T         = native_div_t<L, R>;
            const T rhs_val = static_cast<T>(to_value(rhs));
            if constexpr (!divisor_unchecked<L, R, F, G>)
                if (rhs_val == 0)
                    return fail(errc::division_by_zero);
            const T    lhs_val = static_cast<T>(to_value(lhs));
            const auto qr      = div_rem_rounded(lhs_val, rhs_val, rmode);
            result     res{native_quotient<typename division<L, R, FS>::result>(lhs_val, rhs_val, qr.Quotient), {}};
            from_value(res.Remainder, imax{qr.Remainder});
            return res;
        }
    }
};

} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/predicates.hpp
// ======================================================================



//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   conversion_overflows<B>(v) — v falls outside B's interval.
//   conversion_rounds<B>(v)    — v is in-range but off-notch (would round).
//   conversion_loses<B>(v)     — either of the two.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// Whether B's limits and notch are 64-bit rationals: the predicates' 64-bit
// paths need them.
template <insidable B>
inline constexpr bool predicates_fit64 = fits_rational(grid_of<B>.Interval.Lower) &&
                                         fits_rational(grid_of<B>.Interval.Upper) && fits_rational(grid_of<B>.Notch);

// A double the 64-bit paths cannot take exactly: |d| ≥ 2^64, or finer than
// 2^-62 (as_rational would truncate it).
constexpr bool past_rational(double d) noexcept { return past_64_bits(d) || !abs_fraction_exact(d); }

// What converting the exact value x into B does: overflow (out of range,
// and B's rounding does not bring it in) and rounding (in range off the
// grid, or brought in by B's rounding). For values past the 64-bit paths.
struct lossy_result {
    bool Overflows;
    bool Rounds;
};
template <insidable B, std::size_t K>
constexpr lossy_result exact_lossy(const wide_rational<K>& x) noexcept {
    constexpr std::size_t   KK = exact_max<K, exact_limbs<B>>;
    const wide_rational<KK> v{x};
    const bool              in = !(v < exact_of_grid<KK>(lower_of<B>)) && !(exact_of_grid<KK>(upper_of<B>) < v);
    if constexpr (notched<B>) {
        const auto j = exact_index<B, rounding_for<B, policy<>>>(v);
        if (in)
            return {false, !j.Exact};
        if constexpr (!rounds_before_range_check<B, policy<>>)
            return {true, false};
        else {
            using I         = std::remove_cvref_t<decltype(j.Index)>;
            const bool fits = !j.Index.negative() && !(static_cast<I>(grid_of<B>.slot_count()) < j.Index);
            return {!fits, fits};
        }
    } else {
        constexpr bool bounded = grid_of<B>.MaxDenominator != 0;
        if (in) {
            if constexpr (bounded)
                return {false, static_cast<wide_sint<KK>>(wide_numerator(grid_of<B>.MaxDenominator)) < reduced(v).Den};
            else if constexpr (rational_storage<B>)
                return {!try_rational(v).has_value(), false}; // the store reports what its raw cannot hold
            else
                return {false, false};
        }
        if constexpr (!rounds_before_range_check<B, policy<>>)
            return {true, false};
        else {
            const wide_rational<KK> r = round_to_bound<B, policy<>>(v);
            const bool fits           = !(r < exact_of_grid<KK>(lower_of<B>)) && !(exact_of_grid<KK>(upper_of<B>) < r);
            return {!fits, fits};
        }
    }
}

// Whether a value of A always takes the exact path: B past the 64-bit
// paths, or an inside A whose values pass the 64-bit rational.
template <typename A>
concept wide_inside = insidable<A> && wide_valued<A>;
template <insidable B, numeric A>
inline constexpr bool predicates_exact_only = !predicates_fit64<B> || wide_inside<A>;

// exact_lossy for a value, when the 64-bit paths cannot take it.
template <insidable B, numeric A>
constexpr std::optional<lossy_result> exact_lossy_of(A value) noexcept {
    if constexpr (wide_float<A>) {
        // A long double that is no double has bits the 64-bit paths drop.
        const double d = static_cast<double>(value);
        if (static_cast<A>(d) != value)
            return exact_lossy<B>(exact_of_long_double(value));
        if (!predicates_fit64<B> || past_rational(d))
            return exact_lossy<B>(exact_of_double(d));
    } else if constexpr (std::floating_point<A>) {
        if (!predicates_fit64<B> || past_rational(static_cast<double>(value)))
            return exact_lossy<B>(exact_of_double(static_cast<double>(value)));
    } else if constexpr (wide_inside<A>)
        return exact_lossy<B>(exact_of(value));
    else if constexpr (!predicates_fit64<B>)
        return exact_lossy<B>(exact_of(as_rational(value)));
    return std::nullopt;
}
} // namespace detail

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_overflows(A value) noexcept {
    if constexpr (std::floating_point<A>)
        if (!detail::is_finite(value))
            return true; // NaN / ±inf fit no grid (and must not raise here)
    // The 64-bit paths below do not compile for a grid (or value) past them.
    if constexpr (detail::predicates_exact_only<B, A>)
        return detail::exact_lossy_of<B>(value)->Overflows;
    else {
        if (const auto e = detail::exact_lossy_of<B>(value)) [[unlikely]]
            return e->Overflows;
        const detail::rational r = detail::as_rational(value);
        if (includes(interval_of<B>, r))
            return false;
        // B's policy rounds before it range-checks: a value that rounds onto the
        // grid does not overflow.
        detail::rational rounded;
        return !detail::rounds_into_range<B, policy<>>(r, rounded);
    }
}

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_rounds(A value) noexcept {
    if constexpr (std::floating_point<A>)
        if (!detail::is_finite(value))
            return false; // non-finite — overflow, not rounding
    // The 64-bit paths below do not compile for a grid (or value) past them.
    if constexpr (detail::predicates_exact_only<B, A>)
        return detail::exact_lossy_of<B>(value)->Rounds;
    else {
        if (const auto e = detail::exact_lossy_of<B>(value)) [[unlikely]]
            return e->Rounds;
        const detail::rational r = detail::as_rational(value);
        if (!includes(interval_of<B>, r)) {
            // Out of range: a rounding policy that brings it onto the grid
            // rounds (a grid rounds before the range check); anything else is
            // overflow, not rounding.
            detail::rational rounded;
            return detail::rounds_into_range<B, policy<>>(r, rounded);
        }
        if constexpr (!detail::notched<B>) {
            // Continuous: nothing to miss, unless the grid bounds denominators
            // (an in-range value with a larger one is off the grid).
            if constexpr (grid_of<B>.MaxDenominator == 0)
                return false;
            else
                return detail::abs_den(r.Denominator) > detail::to_rational(grid_of<B>.MaxDenominator).Numerator;
        } else {
            // In range: rounding occurs iff (value − Lower) / Notch is not an
            // integer — exactly when the 64-bit steps overflow.
            if (const auto d = try_sub(r, detail::lower64<B>))
                if (const auto q = try_div(*d, detail::notch64<B>))
                    return detail::abs_den(q->Denominator) != 1;
            return !detail::exact_index<B, detail::round_mode::floor>(detail::exact_of(r)).Exact;
        }
    }
}

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_loses(A value) noexcept {
    return conversion_overflows<B>(value) || conversion_rounds<B>(value);
}
} // namespace beman::inside



// Forward-declare the `beman::inside::math` entry points used in-class, so the bodies
// pass `-Wtemplate-body` without pulling cmath.hpp in unconditionally (its
// definitions live there).
namespace beman::inside::math {
template <insidable Out, insidable In>
constexpr Out floor_into(In x);
template <insidable Out, insidable In>
constexpr Out ceil_into(In x);
template <insidable Out, insidable In>
constexpr Out round_into(In x);
template <insidable Out, insidable In>
constexpr Out trunc_into(In x);
namespace detail {
// abs_into's constraint (cmath.hpp): Out includes the smallest |x|.
template <class Out, class In>
consteval bool abs_into_ok();
} // namespace detail
template <insidable Out, insidable In>
    requires(detail::abs_into_ok<Out, In>())
constexpr Out abs_into(In x);
template <insidable In>
constexpr auto floor(In x);
template <insidable In>
constexpr auto ceil(In x);
template <insidable In>
constexpr auto round(In x);
template <insidable In>
constexpr auto trunc(In x);
template <insidable In>
constexpr auto abs(In x);
} // namespace beman::inside::math

//---------------------------------------------------------------------------
// inside — defines `inside<G, P>` and its member operators. Free-function
// arithmetic (arithmetic.hpp), casts (casts.hpp) and `inside_range` (range.hpp)
// follow in the umbrella. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap), generic.hpp
// (inside_assignable, over detail/introspection.hpp and detail/storage.hpp)
// and policy.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// What `*t` yields for a cursor on G: G without its end point, no cursor — a
// value of T, which assigns into T.
template <grid G, policy_flag P>
using cursor_value_t =
    inside<grid{interval{G.Interval.Lower, G.Interval.Upper - G.Notch}, G.Notch}, P & ~cursor_marker>;

// A store into a fresh B through store(b, policy) under B's flags F with an
// errc& policy: b, or the error it reported.
template <class B, policy_flag F, class Store>
constexpr std::expected<B, errc> try_store(Store store) {
    errc ec{};
    B    b{};
    store(b, make_policy<F>(ec));
    if (ec != errc{})
        return std::unexpected{ec};
    return b;
}
} // namespace detail

//---------------------------------------------------------------------------
// inside
//---------------------------------------------------------------------------
template <grid G, policy_flag P>
struct inside {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");

    using negated_type = inside<-G, P & ~detail::cursor_marker>; // −cursor is no cursor
    using raw_type     = detail::raw_for_t<G>;

  private:
    [[no_unique_address]] raw_type Raw; // empty for a point grid

  public:
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only an inside that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr const raw_type& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw() noexcept
        requires(has_flag(P, unsafe))
    {
        return Raw;
    }

    // Trivial default ctor — Raw is left uninitialized, like a built-in scalar: a
    // default-constructed inside has no value until assigned. (A zero raw would
    // decode to an out-of-range value on grids without 0.) Value-init `inside{}`
    // still zero-fills where a zero raw is genuinely wanted.
    constexpr inside()
        requires(!has_flag(P, detail::cursor_marker))
    = default;

    // A cursor starts at Lower: slot 0.
    constexpr inside()
        requires(has_flag(P, detail::cursor_marker))
        : Raw{detail::raw_from_offset<inside>(umax{0})} {}

  private:
    // The one store every constructor and assignment goes through — the same
    // for every raw kind.
    template <numeric A, typename Pol>
    constexpr void store_value(const A& value, Pol&& pol) {
        detail::assignment<inside, A>::assign(*this, value, pol);
    }

    template <numeric A>
    constexpr void store_value(const A& value) {
        store_value(value, make_policy<P>());
    }

  public:
    // (Raw starts zeroed: a store that rounds a fine double starts from the
    // destination's value, which a constant expression may not read
    // uninitialized. The optimizer drops the dead store.)
    template <numeric A>
        requires inside_assignable<inside, A, P>
    constexpr inside(A value) : Raw{} {
        store_value(value);
    }

    // One-shot policy: `pol`'s flags widen the assignable check (a clamp/round
    // relaxes the interval/notch clause, e.g. clamp_cast<B, round_nearest>(some_inside)) and
    // apply to this store. If it reports an error (ec mode), Raw is ill-defined.
    template <numeric A, typename Pol>
        requires inside_assignable<inside, A, P | flags_of<Pol>>
    constexpr inside(A value, Pol&& pol) : Raw{} {
        store_value(value, pol);
    }

    // No error-code constructor: construction that can fail is `try_make(value)`,
    // which returns expected<inside, errc>. This overload only turns
    // `inside x(value, ec)` into a readable error (an errc& would otherwise bind
    // the Pol&& constructor above).
    template <numeric A>
    inside(A, errc&) = delete ("inside(value, errc&) was removed: use `auto r = B::try_make(value);` "
                               "(expected<B, errc>), or `b.policy(ec) = value` to assign with an error code");

    // expected<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws
    // std::bad_expected_access on an error.
    template <numeric A>
        requires inside_assignable<inside, A, P>
    constexpr inside(const std::expected<A, errc>& value) : Raw{} {
        store_value(value.value());
    }

    // A grid number at run time (`residue r = N - 1_g;`, a constexpr big
    // variable): checked like any store, since its interval is not part of
    // its type. (Exactly a grid number: templates, so nothing converts to one.)
    template <std::same_as<detail::grid_rational> V>
    constexpr inside(const V& value) : Raw{} {
        detail::assign_grid_number(*this, value, make_policy<P>());
    }
    template <std::same_as<detail::grid_rational> V, detail::policy_like Pol>
    constexpr inside(const V& value, Pol&& pol) : Raw{} {
        detail::assign_grid_number(*this, value, pol);
    }
    template <std::same_as<detail::grid_rational> V>
    constexpr inside& operator=(const V& value) {
        detail::assign_grid_number(*this, value, make_policy<P>());
        return *this;
    }

    template <numeric B>
        requires inside_assignable<inside, B, P>
    constexpr inside& operator=(const B& other) {
        store_value(other);
        return *this;
    }

    template <numeric B>
        requires inside_assignable<inside, B, P>
    constexpr inside& operator=(const std::expected<B, errc>& other) {
        store_value(other.value());
        return *this;
    }

    // ---- Diagnostic fallbacks (default on; -DBEMAN_INSIDE_STRICT_SFINAE removes them) ----
    // When a source is numeric but NOT assignable to this inside, every constrained
    // sink above drops out of overload resolution and the compiler emits a bare
    // "could not convert" — losing the reason. These complementary overloads (enabled
    // exactly when `inside_assignable` is false) catch that case and turn it into the
    // named per-clause notes from `inside_assignable_why` (interval-excludes /
    // incompatible-notch). The trade: they make `inside` *appear* is_constructible /
    // assignable from incompatible types (the static_assert is in the body, not the
    // immediate context, so trait probes return true then hard-error only on real
    // use). Define BEMAN_INSIDE_STRICT_SFINAE to drop them and restore SFINAE-pure traits for
    // metaprogramming that probes convertibility (variant/expected/`if constexpr`).
#ifndef BEMAN_INSIDE_STRICT_SFINAE
    template <numeric A>
        requires(!inside_assignable<inside, A, P>)
    constexpr inside(A) {
        static_assert(inside_assignable_why<inside, A, P>::value,
                      "inside: cannot construct this inside from the value — see the per-clause notes above");
    }

    template <numeric A>
        requires(!inside_assignable<inside, A, P>)
    constexpr inside(const std::expected<A, errc>&) {
        static_assert(
            inside_assignable_why<inside, A, P>::value,
            "inside: cannot construct this inside from the expected's value — see the per-clause notes above");
    }

    template <numeric B>
        requires(!inside_assignable<inside, B, P>)
    constexpr inside& operator=(const B&) {
        static_assert(inside_assignable_why<inside, B, P>::value,
                      "inside: cannot assign this value to this inside — see the per-clause notes above");
        return *this;
    }

    template <numeric B>
        requires(!inside_assignable<inside, B, P>)
    constexpr inside& operator=(const std::expected<B, errc>&) {
        static_assert(inside_assignable_why<inside, B, P>::value,
                      "inside: cannot assign this expected's value to this inside — see the per-clause notes above");
        return *this;
    }
#endif

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr inside from_raw(raw_type r) noexcept {
        inside b;
        b.Raw = r;
        return b;
    }

    // Conversion summary:
    //   operator imax     — implicit, when the values are integers or the policy
    //                       may round (then rounded by its mode), and the interval
    //                       fits int64 (else use `to<imax>()`). Also the `vec[b]`
    //                       index path. No second implicit integer operator (would
    //                       make `imax_var += b` ambiguous).
    //   operator rational — implicit; lossless and exact.
    //   operator double   — explicit, and gated on a rounding flag (a double
    //                       may round the value).
    //                       A strict inside opts in via `to<double>().value()`.
    //   to<T>()           — typed-error narrowing/widening → `expected<T, errc>`
    //                       (overflow / domain_error).
    //   as<T>()           — non-expected sibling; throws on error. For known-
    //                       in-range sites (array indexing). FP shares the gate.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    constexpr operator imax() const
        requires((detail::integer_notch<G> || (has_flag(P, snap) && detail::rational_readable<inside>)) &&
                 detail::fits_imax(G.Interval))
    {
        if constexpr (detail::integer_notch<G>)
            return detail::to_value(*this);
        else
            return round_to_int(detail::as_rational(*this), detail::rounding_of(P));
    }

    constexpr explicit operator double() const
        requires(has_flag(P, snap))
    {
        return detail::as_double(*this);
    }

    // Unavailable on a wide-index grid: its values outgrow the 64-bit rational
    // (compare it, or read it with to<T>()).
    constexpr operator detail::rational() const
        requires(detail::rational_readable<inside>)
    {
        if constexpr (G.Interval.Lower == G.Interval.Upper)
            return G.Interval.Lower;
        else if constexpr (detail::value_storage<inside>)
            return Raw;
        // Q-format-with-integer-Lower fast path skips the generic path's three
        // rational ops. Falls through to the rational path when a value times
        // the notch's denominator could pass imax (e.g. a Q16.16 × Q16.16 result).
        else if constexpr (detail::qformat_codec_fits<inside>)
            return detail::q_format_decode(*this);
        else {
            // Raw·Notch can pass 64 bits on the way to a value that fits.
            const auto scaled = detail::try_mul(rational{Raw}, detail::notch64<inside>);
            const auto value  = scaled ? detail::try_add(*scaled, detail::lower64<inside>) : scaled;
            if (value) [[likely]]
                return *value;
            if (const auto exact = detail::try_rational(detail::exact_of(*this)))
                return *exact;
            // (Not reached for a rational_readable grid; reported, as any
            // overflow, rather than a bad_expected_access.)
            detail::raise(errc::overflow, "inside: value past the 64-bit rationals");
        }
    }

    // to<T>() — typed-error scalar extraction (mirrors rational::to<T>, extended
    // to signed and floating point). Returns `errc::overflow` (out of T's range;
    // `errc::domain_error` for a negative value into unsigned T); fractional
    // truncation is silent. Bounds the grid already satisfies are not checked.
    template <std::integral T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const {
        using lim               = std::numeric_limits<T>;
        constexpr bool check_lo = lower_of<inside> < detail::rational{lim::min()};
        constexpr bool check_hi = upper_of<inside> > detail::rational{lim::max()};
        if constexpr (!check_lo && !check_hi && detail::values_fit_imax<inside>)
            return static_cast<T>(detail::to_value(*this));
        else if constexpr (detail::wide_valued<inside>) { // truncate first: -0.5 → unsigned is 0
            const auto t = trunc(detail::exact_of(*this));
            if (check_lo && t < decltype(t){lim::min()})
                return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if (check_hi && t > decltype(t){lim::max()})
                return std::unexpected{errc::overflow};
            return static_cast<T>(t);
        } else {
            const auto t = detail::round_to_integral(detail::as_rational(*this), detail::round_mode::trunc);
            if (check_lo && t < detail::rational{lim::min()})
                return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if (check_hi && t > detail::rational{lim::max()})
                return std::unexpected{errc::overflow};
            return static_cast<T>(detail::signed_numerator(t));
        }
    }

    // Rounded once to T's digits: a floating point wider than double (long
    // double) from the exact value; a narrower one (float) through the double,
    // unless that double is a tie between two Ts.
    template <std::floating_point T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const {
        if constexpr (detail::wide_float<T>)
            return detail::nearest_float<T>(detail::exact_of(*this));
        else if constexpr (sizeof(T) < sizeof(double)) {
            const double d = detail::as_double(*this);
            if (detail::narrow_midpoint<T>(d)) [[unlikely]]
                return detail::nearest_float<T>(detail::exact_of(*this));
            return static_cast<T>(d);
        } else
            return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict inside rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
        requires(std::integral<T> || std::floating_point<T>) && (!std::floating_point<T> || has_flag(P, snap))
    {
        return to<T>().value();
    }

    // numerator() / denominator() — the exact value in lowest terms as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
    // imax when every value fits it, else a wide integer that holds every value
    // (a wide grid, a grid past int64).
    [[nodiscard]] constexpr auto numerator() const {
        if constexpr (detail::fraction_fits_imax<inside>)
            return fraction().first;
        else
            return detail::reduced_exact(*this).Num;
    }
    [[nodiscard]] constexpr auto denominator() const {
        if constexpr (detail::fraction_fits_imax<inside>)
            return fraction().second;
        else
            return detail::reduced_exact(*this).Den;
    }

  private:
    // The reduced exact value as {numerator, positive denominator}. Integer
    // grids need no division; dyadic Q-format grids reduce by shifting out
    // common factors of two instead of a gcd.
    constexpr std::pair<imax, imax> fraction() const {
        if constexpr (detail::integer_storage<inside> && detail::integer_lattice<inside>)
            return {detail::to_value(*this), 1};
        else if constexpr (detail::index_storage<inside> && detail::qformat_codec_fits<inside> &&
                           std::has_single_bit(detail::abs_den(detail::notch64<inside>.Denominator))) {
            constexpr imax nd  = detail::abs_den(detail::notch64<inside>.Denominator);
            const imax     num = detail::raw_imax(*this) + detail::lower_imax<inside> * nd;
            const int      s   = std::countr_zero(static_cast<umax>(num) | static_cast<umax>(nd)); // 0 → 0/1
            return {num >> s, nd >> s};
        } else {
            auto r = detail::as_rational(*this);
            return {signed_numerator(r), static_cast<imax>(detail::abs_den(r.Denominator))};
        }
    }

  public:
    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `beman::inside::math` — `beman::inside::math::floor(b)` etc. (auto-deduced Out)
    // or `beman::inside::math::floor_into<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<beman/inside/cmath.hpp>`.

    [[nodiscard]] constexpr negated_type operator-() const {
        negated_type neg;
        if constexpr (detail::point_storage<inside>)
            neg = negated_type::from_raw({});        // −point is a point: no raw
        else if constexpr (!detail::notched<inside>) // continuous: the raw is the value
            neg = negated_type::from_raw(-(Raw));
        else {
            // Integer raws: the negated value index is −J (wide_value.hpp), in imax
            // when the bounds allow, else by wrapping. Index storage on both sides
            // counts the slot from the opposite end instead.
            using W = detail::index_work_t<negated_type, inside, G.Notch, inside, G.Notch>;
            if constexpr (detail::index_storage<inside> && detail::index_storage<negated_type>) {
                constexpr W count = static_cast<W>(G.slot_count());
                neg = negated_type::from_raw(static_cast<detail::raw_t<negated_type>>(count - static_cast<W>(Raw)));
            } else
                neg = detail::from_value_index<negated_type>(W{0} - detail::value_index<W>(*this));
        }
        return neg;
    }

    // policy<F>() — per-operation policy override. On an lvalue it returns a
    // policy_ref holding `*this` by reference (needed so `b.policy<…>() = x` writes
    // back into b, and cheap for the common immediate use). On an *rvalue* receiver
    // it returns a policy_buffer that OWNS the moved-in value, so a snapped temporary
    // survives being returned/stored (`return (a*b).with_snap();`) — no dangling.
    template <policy_flag F = none, typename Self>
    [[nodiscard]] constexpr auto policy(this Self&& self) {
        auto pol = make_policy<P | F>();
        if constexpr (std::is_lvalue_reference_v<Self>)
            return detail::policy_ref<inside, decltype(pol)>{self, pol, {}};
        else
            return detail::policy_buffer<inside, decltype(pol)>{std::move(self), pol, {}};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(errc& ec) {
        auto pol = make_policy<P | F>(ec);
        return detail::policy_ref<inside, decltype(pol)>{*this, pol, {}};
    }

    // with_snap<Mode>() — opt this assignment into snapping with the given rounding
    // mode. Bare `with_snap()` is truncate-toward-zero (Mode == snap); pass an
    // explicit mode for the others: with_snap<round_nearest>(), <round_floor>,
    // <round_ceil>, <round_half_even>. Like policy<F>(), a temporary receiver
    // yields a value-owning policy_buffer.
    template <policy_flag Mode = snap, typename Self>
    [[nodiscard]] constexpr auto with_snap(this Self&& self) {
        static_assert(has_flag(Mode, snap),
                      "with_snap<Mode>: Mode must be a snapping mode — snap (truncate), round_nearest, "
                      "round_floor, round_ceil, or round_half_even");
        return std::forward<Self>(self).template policy<Mode>();
    }
    template <typename Self>
    [[nodiscard]] constexpr auto with_clamp(this Self&& self) {
        return std::forward<Self>(self).template policy<clamp>();
    }
    template <typename Self>
    [[nodiscard]] constexpr auto with_wrap(this Self&& self) {
        return std::forward<Self>(self).template policy<wrap>();
    }

  private:
    // Shared builder for the single-action fluent hooks below (internal). Merges
    // the tag's implied policy flag, then returns a policy_ref bound to *this
    // carrying the tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action) {
        using tag = Tag<std::remove_cvref_t<A>>;
        auto pol  = make_policy<P | detail::implied_flags<tag>>();
        return detail::policy_ref<inside, decltype(pol), tag>{*this, pol, tag{std::forward<A>(action)}};
    }

  public:
    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& a) {
        return make_action_ref<on_wrap_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& a) {
        return make_action_ref<on_clamp_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& a) {
        return make_action_ref<on_error_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_overflow(A&& a) {
        return make_action_ref<on_overflow_t>(std::forward<A>(a));
    }

    // Multi-action entry point: combine N tagged actions into one policy_ref.
    // policy_ref rejects mutually exclusive combinations at compile time. E.g.
    // `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs` — the arithmetic fires λ1,
    // the narrowing back into b fires λ2.
    template <typename... Actions>
    [[nodiscard]] constexpr auto with(Actions&&... actions) {
        constexpr policy_flag merged = detail::merged_implied_flags<Actions...>;
        auto                  pol    = make_policy<P | merged>();
        return detail::policy_ref<inside, decltype(pol), std::remove_cvref_t<Actions>...>{
            *this, pol, std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

  private:
    // Raw-space fast paths of += and -=. Each adds a delta to the raw: a
    // compile-time constant (point rhs), or ±rhs raw plus a constant bias.
    // With equal notches, raw(v_l ± v_r) = raw_l ± (raw_r + bias): the bias is
    // Lower/Notch of an index-raw rhs (its raw is Lower-relative), else 0.
    //   point_delta<R>   — rhs is one whole number of notches: the delta.
    //   raw_add_ok<R>    — rhs raw adds or subtracts with that bias.
    // The add runs in raw_work_t, sized from the raw and delta ranges: imax for
    // every grid within int64, a wide_int beyond — never overflowing.
    template <insidable R>
    static constexpr bool point_delta_ok =
        detail::notched<inside> && detail::point_grid<R> && detail::grid_divides_evenly(lower_of<R>, notch_of<inside>);

    // (R anchored: its Lower is a whole number of notches, the bias.)
    template <insidable R>
    static constexpr bool raw_add_ok = detail::notched<inside> && notch_of<inside> == notch_of<R> &&
                                       !detail::point_storage<R> && detail::anchored<R>; // ⇒ integer raws

    template <insidable R>
    static constexpr detail::grid_int point_delta = detail::exact_quotient(lower_of<R>, notch_of<inside>);
    template <insidable R>
    static constexpr detail::grid_int add_bias = [] {
        if constexpr (detail::index_storage<R>)
            return detail::slot_base<R>;
        else
            return detail::grid_int{0};
    }();

    // Work type of a raw-space add whose delta lies in [Dlo, Dhi]: it holds
    // every raw of this grid, the delta, the new raw (in [raw_lo + Dlo,
    // raw_hi + Dhi]), the wrap range raw_hi − raw_lo + 1 and the new raw's
    // offset from raw_lo (wrap divides it), so the add cannot overflow. imax
    // for every grid within int64.
    // (A variable template, not a function: Clang would instantiate a plain
    // member function's body while the class is still incomplete.)
    template <detail::grid_int Dlo, detail::grid_int Dhi>
    static constexpr int raw_work_bits = [] {
        using W        = detail::grid_int;
        constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
        return detail::signed_value_bits_of({lo, hi, Dlo, Dhi, lo + Dlo, hi + Dhi, hi - lo + W{1}, hi - lo + Dhi});
    }();
    template <detail::grid_int Dlo, detail::grid_int Dhi>
    using raw_work_t = detail::work_int_t<raw_work_bits<Dlo, Dhi>>;

    // Work type of the raw delta ±(rhs.raw() + bias) itself: it holds R's raws,
    // their negations and the bias, so neither step overflows before the delta
    // (in raw_work_t's range) is formed.
    template <insidable R>
    using delta_work_t = detail::work_int_t<detail::signed_value_bits_of({detail::raw_lo_exact<R>,
                                                                          detail::raw_hi_exact<R>,
                                                                          -detail::raw_lo_exact<R>,
                                                                          -detail::raw_hi_exact<R>,
                                                                          add_bias<R>,
                                                                          -add_bias<R>,
                                                                          detail::raw_lo_exact<R> + add_bias<R>,
                                                                          detail::raw_hi_exact<R> + add_bias<R>,
                                                                          -detail::raw_lo_exact<R> - add_bias<R>,
                                                                          -detail::raw_hi_exact<R> - add_bias<R>})>;

    // A compound op's constraint: its binary op's result (an expected's
    // value) assigns to this type, so `requires { x op= y; }` is honest.
    template <typename Result>
    static constexpr bool compound_assignable = inside_assignable<inside, detail::unwrap_t<Result>, P>;

  public:
    template <insidable R>
        requires(point_delta_ok<R> || raw_add_ok<R> ||
                 compound_assignable<decltype(std::declval<const inside&>() + std::declval<const R&>())>)
    constexpr inside& operator+=(const R& rhs) {
        // Point-inside rhs (just<v> / 1_ins / ++) whose value is a whole number of
        // this grid's notches: the raw delta is a compile-time constant and the
        // raw encoding cancels every Lower term (raw(v+d) = raw(v) + d/Notch for
        // index and value raws alike), so this compiles to one integer add.
        if constexpr (point_delta_ok<R>) {
            using W           = raw_work_t<point_delta<R>, point_delta<R>>;
            constexpr W delta = static_cast<W>(point_delta<R>);
            return store_raw<W>(static_cast<W>(Raw) + delta);
        } else if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<detail::raw_lo_exact<R> + add_bias<R>, detail::raw_hi_exact<R> + add_bias<R>>;
            using D = delta_work_t<R>;
            constexpr D bias = static_cast<D>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(static_cast<D>(rhs.raw()) + bias));
        } else
            return assign_op_result(*this + rhs);
    }

  private:
    // Store a raw computed by the raw-space fast paths of += and -=, in their
    // work type W. Under clamp/wrap/checked an out-of-range raw is clamped,
    // wrapped or reported.
    template <detail::raw_integer W>
    constexpr inside& store_raw(W new_raw) {
        constexpr W lo = static_cast<W>(detail::raw_lo_exact<inside>);
        constexpr W hi = static_cast<W>(detail::raw_hi_exact<inside>);
        if constexpr (range_handled(P))
            if (new_raw < lo || new_raw > hi) {
                if constexpr (P & clamp)
                    new_raw = new_raw < lo ? lo : hi;
                else if constexpr (P & wrap)
                    new_raw = lo + detail::floor_divmod(new_raw - lo, hi - lo + W{1}).Rem;
                else {
                    make_policy<P>().report(errc::overflow);
                    return *this;
                }
            }
        Raw = static_cast<raw_type>(new_raw);
        return *this;
    }

    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
    template <typename Result>
    constexpr inside& assign_op_result(const Result& r) {
        if constexpr (detail::expected_like<Result>) {
            // A failed op is reported through this type's policy (throw / handler);
            // *this stays unchanged. `*r` (not value()): no bad_expected_access.
            if (!r.has_value())
                return report_error(r.error());
            *this = *r;
        } else
            *this = r;
        return *this;
    }

    constexpr inside& report_error(errc e) {
        make_policy<P>().report(e);
        return *this;
    }

    // A zero divisor in /= or %=; ignore_zero on either operand silences it,
    // as it does for div/mod.
    template <insidable R>
    constexpr inside& report_div_by_zero() {
        if constexpr (!has_flag(P | flags_of<R>, ignore_zero))
            make_policy<P>().report(errc::division_by_zero);
        return *this;
    }

    // A rational on a Q-format grid's lattice as a signed notch count (rhs·K
    // for notch 1/K), when it is a whole number of notches of moderate size.
    static constexpr bool raw_delta_ok = detail::qformat_codec_fits<inside> && detail::values_fit_imax<inside>;
    // The largest count it takes: store_raw forms Raw ± n and, to wrap, its
    // offset from raw_lo, so n leaves room for the raws and their span in imax.
    static constexpr umax max_notch_delta = [] {
        using W        = detail::grid_int;
        constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
        const W     mag  = lo.negative() && hi < -lo ? -lo : hi;
        const W     room = W{std::numeric_limits<imax>::max()} - (hi - lo + W{1}) - mag;
        return room.negative() ? umax{0} : static_cast<umax>(room);
    }();
    static constexpr std::optional<imax> notch_delta(const detail::rational& r) noexcept {
        constexpr umax K = detail::abs_den(detail::notch64<inside>.Denominator);
        const umax     d = detail::abs_den(r.Denominator);
        if (K % d != 0 || r.Numerator > max_notch_delta / (K / d))
            return std::nullopt;
        const imax n = static_cast<imax>(r.Numerator * (K / d));
        return r.Denominator < 0 ? -n : n;
    }

  public:
    // Only a `rational` (a library type) may join an inside in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_ins` /
    // `just<1>` / `inside<{lo,hi}>{n}`), mirroring the binary operators.

    template <std::same_as<detail::rational> A>
    constexpr inside& operator+=(const A& rhs) {
        if constexpr (raw_delta_ok)
            if (const auto n = notch_delta(rhs)) [[likely]]
                return store_raw<imax>(static_cast<imax>(Raw) + *n);
        return assign_op_result(detail::rational{*this} + rhs);
    }

    template <insidable R>
        requires(raw_add_ok<R> ||
                 compound_assignable<decltype(std::declval<const inside&>() - std::declval<const R&>())>)
    constexpr inside& operator-=(const R& rhs) {
        // Raw-space fast path, the subtraction mirror of +='s.
        if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<-detail::raw_hi_exact<R> - add_bias<R>, -detail::raw_lo_exact<R> - add_bias<R>>;
            using D = delta_work_t<R>;
            constexpr D bias = static_cast<D>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(-static_cast<D>(rhs.raw()) - bias));
        } else
            return *this += (-rhs);
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator-=(const A& rhs) {
        if constexpr (raw_delta_ok)
            if (const auto n = notch_delta(rhs)) [[likely]]
                return store_raw<imax>(static_cast<imax>(Raw) - *n);
        return assign_op_result(detail::rational{*this} - rhs);
    }

    template <insidable R>
        requires compound_assignable<decltype(std::declval<const inside&>() * std::declval<const R&>())>
    constexpr inside& operator*=(const R& rhs) {
        return assign_op_result(*this * rhs);
    }

    // The outer zero check is semantic, not redundant: the binary `a / b`
    // yields an error value on a zero divisor (expected vocabulary), so the compound
    // form's report comes from here. (Measured perf-neutral to remove.)
    template <insidable R>
        requires compound_assignable<decltype(std::declval<const inside&>() / std::declval<const R&>())>
    constexpr inside& operator/=(const R& rhs) {
        if (rhs == 0)
            return report_div_by_zero<R>();
        return assign_op_result(*this / rhs);
    }

    template <insidable R>
        requires compound_assignable<
            decltype(mod(std::declval<const inside&>(), std::declval<const R&>(), make_policy<P>()))>
    constexpr inside& operator%=(const R& rhs) {
        if (rhs == 0)
            return report_div_by_zero<R>();
        return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator*=(const A& rhs) {
        return assign_op_result(detail::rational{*this} * rhs);
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator/=(const A& rhs) {
        if (detail::is_canonical_zero(rhs))
            return report_div_by_zero<inside>();
        return assign_op_result(detail::rational{*this} / rhs);
    }

    // expected<inside> RHS (e.g. `x += a / b`): unwrap once, reporting an error
    // through this type's policy like any other failed compound op.
    template <insidable R>
        requires requires(inside& self, const R& r) { self += r; }
    constexpr inside& operator+=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this += *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self -= r; }
    constexpr inside& operator-=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this -= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self *= r; }
    constexpr inside& operator*=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this *= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self /= r; }
    constexpr inside& operator/=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this /= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self %= r; }
    constexpr inside& operator%=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this %= *rhs) : report_error(rhs.error());
    }

    // ++/-- move one notch: they add the point inside of ±Notch through the
    // insidable += (which has the raw-level integer fast path) instead of the
    // rational round-trip (~30× the instructions on an integer grid). On a
    // notch-1 grid that is ±1; on a cursor, one step. `just` itself is declared
    // after the class, so spell the point inside directly.
    constexpr inside& operator++() {
        static_assert(G.Notch != 0, "inside: ++ steps one notch - a grid without a notch has none to step");
        // constexpr local: the point inside is materialized at compile time.
        constexpr auto notch_step = inside<grid{G.Notch}>::from_raw({});
        return *this += notch_step;
    }
    constexpr inside operator++(int) {
        inside t = *this;
        ++*this;
        return t;
    }
    constexpr inside& operator--() {
        static_assert(G.Notch != 0, "inside: -- steps one notch - a grid without a notch has none to step");
        constexpr auto notch_step = inside<grid{-G.Notch}>::from_raw({});
        return *this += notch_step;
    }
    constexpr inside operator--(int) {
        inside t = *this;
        --*this;
        return t;
    }

    // *t: a cursor's value without the end, so a cursor is its own iterator
    // and end(t) its sentinel. At the end it reports out of range.
    [[nodiscard]] constexpr auto operator*() const
        requires(has_flag(P, detail::cursor_marker))
    {
        return detail::cursor_value_t<G, P>{*this};
    }

    // try_make<F>(value): the constructors' store under this type's policy plus
    // the flags F, with the failure as the error. An expected value passes its
    // error on, so fallible steps chain.
    template <policy_flag F = none, numeric A>
        requires inside_assignable<inside, A, P | F>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value) {
        return detail::try_store<inside, P | F>([&](inside& r, auto&& pol) { r.store_value(value, pol); });
    }
    template <policy_flag F = none, numeric A>
        requires inside_assignable<inside, A, P | F>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(const std::expected<A, errc>& value) {
        return value.and_then([](const A& a) { return try_make<F>(a); });
    }
    template <policy_flag F = none, std::same_as<detail::grid_rational> V>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(const V& value) {
        return detail::try_store<inside, P | F>(
            [&](inside& r, auto&& pol) { detail::assign_grid_number(r, value, pol); });
    }
};

//---------------------------------------------------------------------------
// to<T>(b) / as<T>(b) — free-function forms, for generic code that would
// otherwise need the `.template` disambiguator. Same semantics as the members.
//---------------------------------------------------------------------------
template <typename T, insidable B>
[[nodiscard]] constexpr auto to(const B& b)
    requires requires { b.template to<T>(); }
{
    return b.template to<T>();
}

template <typename T, insidable B>
[[nodiscard]] constexpr T as(const B& b)
    requires requires { b.template as<T>(); }
{
    return b.template as<T>();
}

//---------------------------------------------------------------------------
// parse<B, F>(first, last) — text → B, exactly (no double round-trip). The
// whole range must be one number: an optional sign, then the literal grammar
// (1'000, 1.25, 1.5e2, 0xff, 0b1010, 0x1.8p3) or a fraction N/D. Malformed text
// is errc::invalid_format; the value then goes through B::try_make<F>, so B's
// policy plus the flags F round, clamp or wrap it and report overflow /
// rounding_error. parse_exact<B> accepts only a value B holds exactly:
// off the grid is rounding_error and out of range overflow, whatever B's
// policy. (io.hpp adds std::string_view overloads and operator>>.)
//---------------------------------------------------------------------------
namespace detail {
// parse past the 64-bit rational (a long decimal, a wide grid's value,
// 1e-200): the text parsed exactly, or past that width as a tiny or huge
// value's stand-in. Out of line: the common parse stays small.
template <insidable B, policy_flag F>
[[gnu::cold]] [[gnu::noinline]] constexpr std::expected<B, errc> parse_wide(const char* first, const char* last) {
    constexpr std::size_t K = exact_max<exact_limbs<B>, parse_limbs>;
    auto                  w = parse_exact<K>(first, last);
    if (!w && w.error() == errc::overflow)
        if (const auto t = stand_in<B, K, static_cast<bool>((flags_of<B> | F) & wrap)>(first, last))
            w = *t;
    if (!w)
        return std::unexpected{w.error()};
    return try_store<B, flags_of<B> | F>([&](B& b, auto&& pol) { assign_exact<rational>(b, *w, pol, no_action{}); });
}
} // namespace detail

template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> parse(const char* first, const char* last) {
    const auto v = detail::parse_text(first, last);
    if (!v && v.error() == errc::overflow) [[unlikely]]
        return detail::parse_wide<B, F>(first, last);
    if (!v)
        return std::unexpected{v.error()};
    return B::template try_make<F>(*v);
}

template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> parse_exact(const char* first, const char* last) {
    const auto v = detail::parse_text(first, last);
    if (v) {
        if (conversion_overflows<B>(*v))
            return std::unexpected{errc::overflow};
        if (conversion_rounds<B>(*v))
            return std::unexpected{errc::rounding_error};
        return B::template try_make<snap>(*v); // on the grid: nothing rounds
    }
    if (v.error() != errc::overflow)
        return std::unexpected{v.error()};
    constexpr std::size_t K = detail::exact_max<detail::exact_limbs<B>, detail::parse_limbs>;
    auto                  w = detail::parse_exact<K>(first, last);
    if (!w && w.error() == errc::overflow)
        if (const auto t = detail::stand_in<B, K>(first, last))
            w = *t; // never on the grid: rounding_error, or overflow outside it
    if (!w)
        return std::unexpected{w.error()};
    if (*w < detail::exact_of_grid<K>(lower_of<B>) || detail::exact_of_grid<K>(upper_of<B>) < *w)
        return std::unexpected{errc::overflow};
    if constexpr (detail::notched<B>) {
        if (!detail::exact_index<B, detail::round_mode::trunc>(*w).Exact)
            return std::unexpected{errc::rounding_error};
    } else if constexpr (grid_of<B>.MaxDenominator != 0) {
        // A bounded continuous grid holds denominators up to its bound only.
        if (static_cast<detail::wide_sint<K>>(detail::wide_numerator(grid_of<B>.MaxDenominator)) <
            detail::reduced(*w).Den)
            return std::unexpected{errc::rounding_error};
    }
    return detail::try_store<B, flags_of<B>>(
        [&](B& b, auto&& pol) { detail::assign_exact<detail::rational>(b, *w, pol, no_action{}); });
}

//---------------------------------------------------------------------------
// just
//---------------------------------------------------------------------------
// The point grid holds the value, so no value constructor is needed: grid
// numbers past 64 bits (`_g`) work too. just<N, D> is the point N/D
// for an integer D ≥ 1: just<1, 3>, just<-6, 5> for -1.2.
namespace detail {
template <auto value, auto den>
consteval auto just_point() {
    if constexpr (std::integral<decltype(den)> && den == 1)
        return inside<grid{value}>::from_raw({});
    else {
        static_assert(std::integral<decltype(den)> && den >= 1, "just<N, D>: D is an integer of at least 1");
        return inside<grid{grid_rational{value} / grid_rational{den}}>::from_raw({});
    }
}
} // namespace detail
template <auto value, auto den = 1>
inline constexpr auto just = detail::just_point<value, den>();

//---------------------------------------------------------------------------
// _ins literal — compile-time `inside<{V, V}>` from a numeric literal.
//   5_ins           // inside<{5, 5}>            integer
//   1.25_ins        // inside<{rational{5,4}}>   decimal
//   1.5e2_ins       // inside<{150}>             decimal scientific
//   0xff_ins        // inside<{255}>             hex integer
//   0b1010_ins      // inside<{10}>              binary integer
//   0x1p15_ins      // inside<{32768}>           hex with 2^N exponent (Q-format)
//   0x1p-15_ins     // inside<{rational{1,32768}}>   1/2^15 grid notch
//   0x1.8p3_ins     // inside<{12}>              hex float
//   1'000_ins       // inside<{1000}>            digit separator
//
// Parse is exact (no double round-trip); same parser backs `_r` in
// detail/number_text.hpp. `-1.5_ins` parses as `-(1.5_ins)`.
//---------------------------------------------------------------------------
template <char... Chars>
constexpr auto operator""_ins() {
    return just<detail::parse_ins_literal<Chars...>()>;
}

} // namespace beman::inside

// A cursor is a bidirectional iterator: it steps by ++/--, *t reads its value,
// and end(t) is its sentinel. Not random access: t - u is arithmetic, an
// inside, not a distance. Plain insides get no traits, so they stay numbers.
template <beman::inside::grid G, beman::inside::policy_flag P>
    requires(beman::inside::has_flag(P, beman::inside::detail::cursor_marker))
struct std::incrementable_traits<beman::inside::inside<G, P>> {
    using difference_type = beman::inside::imax;
};

template <beman::inside::grid G, beman::inside::policy_flag P>
    requires(beman::inside::has_flag(P, beman::inside::detail::cursor_marker))
struct std::indirectly_readable_traits<beman::inside::inside<G, P>> {
    using value_type = beman::inside::detail::cursor_value_t<G, P>;
};


// ======================================================================
//  beman/inside/detail/compare.hpp
// ======================================================================
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.




//---------------------------------------------------------------------------
// Comparisons of insides: across grids (by value index when both fit imax,
// else exactly) and against scalars (integers, floating points of any width,
// rationals).
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// comparison
//---------------------------------------------------------------------------
namespace detail {
// Integer value-index comparison eligibility: an integer-backed anchored
// inside whose value indices (value/Notch, integers) fit imax, so it
// compares with a scalar as `bias + raw` without a rational decode.
template <insidable B>
inline constexpr bool index_cmp_fits = [] {
    if constexpr (!notched<B> || !anchored<B> || !values_fit_imax<B>)
        return false;
    else
        return index_lo64<B> && index_hi64<B> && std::in_range<imax>(index_lo64<B>->Numerator) &&
               std::in_range<imax>(index_hi64<B>->Numerator);
}();

// Signed value index of Raw == 0: Lower/Notch for offset (index) storage,
// 0 for a value raw (raw is already the value == the index at notch 1).
template <insidable B>
inline constexpr imax index_cmp_bias = [] {
    if constexpr (index_storage<B>)
        return signed_numerator(*index_lo64<B>);
    else
        return imax{0};
}();

// Inside values are finite and exact, so they are always ordered: <=> of two
// insides, or of an inside and an integer, is a strong_ordering whatever the
// storage; against a floating scalar it is a partial_ordering (NaN is
// unordered), three_way_partial.
// Integers of mixed signedness compare by value (std::cmp_*), not by the
// usual arithmetic conversions; same-signed pairs keep the plain operator.
template <typename A, typename B>
concept mixed_sign_integers     = std::integral<std::remove_cvref_t<A>> && std::integral<std::remove_cvref_t<B>> &&
                                  std::is_signed_v<std::remove_cvref_t<A>> != std::is_signed_v<std::remove_cvref_t<B>>;
inline constexpr auto three_way = [](const auto& a, const auto& b) -> std::strong_ordering {
    if constexpr (mixed_sign_integers<decltype(a), decltype(b)>)
        return std::cmp_less(a, b)    ? std::strong_ordering::less
               : std::cmp_equal(a, b) ? std::strong_ordering::equal
                                      : std::strong_ordering::greater;
    else
        return a <=> b;
};
inline constexpr auto three_way_partial = [](const auto& a, const auto& b) -> std::partial_ordering {
    return a <=> b;
};
inline constexpr auto equal_to = [](const auto& a, const auto& b) {
    if constexpr (mixed_sign_integers<decltype(a), decltype(b)>)
        return std::cmp_equal(a, b);
    else
        return a == b;
};

// A point P that is one of X's slots, with X storing a builtin index: X's
// raw orders like its value, so X ⋈ P is its raw ⋈ that constant slot
// (`t != end(t)`, `x <= 4_ins`).
template <insidable X, insidable P>
inline constexpr bool point_is_slot =
    integer_index_storage<X> && !point_grid<X> && point_grid<P> && grid_of<X>.representable(lower_of<P>);
template <insidable X, insidable P>
inline constexpr raw_t<X> point_slot_of =
    static_cast<raw_t<X>>(exact_quotient(lower_of<P> - lower_of<X>, notch_of<X>));

// inside ⋈ inside (⋈ = `cmp`: <=> or ==) in the cheapest exact form the two
// storage shapes allow.
template <insidable L, insidable R, class Cmp>
constexpr auto compare(const L& lhs, const R& rhs, Cmp cmp) {
    // same grid, so the same encoding: Raw is monotonically ordered and comparable
    if constexpr (grid_of<L> == grid_of<R>)
        return cmp(lhs.raw(), rhs.raw());
    else if constexpr (point_is_slot<L, R>)
        return cmp(lhs.raw(), point_slot_of<L, R>);
    else if constexpr (point_is_slot<R, L>)
        return cmp(point_slot_of<R, L>, rhs.raw());
    // a wide-valued operand (wide index, big grid numbers, fraction raws): exact wide fractions
    else if constexpr (wide_valued<L> || wide_valued<R>)
        return cmp(exact_of(lhs), exact_of(rhs));
    // integer raws: both values as exact counts of a unit dividing them both
    // (the gcd of their value units), one integer compare — any notches and
    // offsets, without the rational decode.
    else if constexpr (units_cmp_fits<L, R>) {
        constexpr grid_rational U = gcd(unit_of<L>, unit_of<R>);
        return cmp(value_in_units<imax, U>(lhs), value_in_units<imax, U>(rhs));
    } else
        return cmp(as_rational(lhs), as_rational(rhs));
}
} // namespace detail

template <insidable L, insidable R>
[[nodiscard]] constexpr auto operator<=>(const L& lhs, const R& rhs) {
    return detail::compare(lhs, rhs, detail::three_way);
}

template <insidable L, insidable R>
[[nodiscard]] constexpr bool operator==(const L& lhs, const R& rhs) {
    return detail::compare(lhs, rhs, detail::equal_to);
}

namespace detail {
// inside ⋈ integral scalar without the rational decode: with the positive
// notch n/d, value ⋈ c ⟺ (bias + raw)·n ⋈ c·d — both sides exact
// integers (bias + raw is the signed value index, exact ordering AND
// equality since c·d is exact too). Eligible when both cross terms
// provably fit imax for every representable c of type A.
template <insidable B, typename A>
inline constexpr bool scalar_index_cmp_fits = [] {
    if constexpr (!std::integral<A> || value_storage<B> || !index_cmp_fits<B>)
        return false;
    else {
        constexpr umax notch_num = notch64<B>.Numerator;
        constexpr umax notch_den = static_cast<umax>(notch64<B>.Denominator); // Notch > 0
        constexpr umax index_mag =
            index_lo64<B>->Numerator > index_hi64<B>->Numerator ? index_lo64<B>->Numerator : index_hi64<B>->Numerator;
        // The largest |c| of A: |min| = max + 1 for a signed type.
        constexpr umax scalar_mag = static_cast<umax>(std::numeric_limits<A>::max()) + std::signed_integral<A>;
        umax           product;
        return !mul_overflow(index_mag, notch_num, &product) && std::in_range<imax>(product) &&
               !mul_overflow(scalar_mag, notch_den, &product) && std::in_range<imax>(product);
    }
}();

// inside ⋈ arithmetic scalar. Integer storage compares as integers when the
// scalar's type fits imax, and in double when it is floating point and the
// grid's values are exact in double; everything else goes through rational.
template <insidable B, plain_number A, class Cmp>
constexpr auto compare_scalar(const B& lhs, A rhs, Cmp cmp) {
    constexpr bool imax_scalar = std::signed_integral<A> || (std::unsigned_integral<A> && sizeof(A) < sizeof(imax));
    constexpr bool double_exact_values =
        lower_of<B> >= rational{-(imax{1} << 53)} && upper_of<B> <= rational{imax{1} << 53};
    // A long double with bits past the double's: the paths below read a double.
    if constexpr (wide_float<A>)
        if (is_finite(rhs) && !is_double_value(rhs)) [[unlikely]]
            return cmp(exact_of(lhs), exact_of_long_double(rhs));
    if constexpr (wide_valued<B>) {
        if constexpr (std::floating_point<A>) {
            if (rhs != rhs) // NaN: unordered, as the double comparison says
                return cmp(0.0, static_cast<double>(rhs));
            if (!is_finite(rhs)) // ±inf lies past every grid
                return cmp(exact_of(0), exact_of(rhs < 0 ? -1 : 1));
            return cmp(exact_of(lhs), exact_of_double(static_cast<double>(rhs)));
        } else
            return cmp(exact_of(lhs), exact_of(as_rational(rhs)));
    } else if constexpr (integer_value_storage<B> && values_fit_imax<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
    else if constexpr (integer_value_storage<B> && values_fit_imax<B> && std::unsigned_integral<A>)
        return cmp(raw_imax(lhs), rhs); // a 64-bit unsigned: sign-safe, by the cmp_* forms
    else if constexpr (integer_value_storage<B> && values_fit_imax<B> && std::floating_point<A> && double_exact_values)
        return cmp(static_cast<double>(raw_imax(lhs)), static_cast<double>(rhs));
    else if constexpr (integer_index_storage<B> && std::floating_point<A> && double_exact<grid_of<B>> &&
                       index_double<B>.Ok)
        return cmp(as_double(lhs), static_cast<double>(rhs)); // exact: every value is a double
    else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(notch64<B>.Numerator),
                   static_cast<imax>(rhs) * notch64<B>.Denominator);
    else {
        // |rhs| ≥ 2^64 (or infinite) has no rational form, and every grid value
        // lies strictly inside ±2^64: the sign of rhs decides.
        if constexpr (std::floating_point<A>) {
            if (rhs != rhs) // NaN: unordered, as the double comparison says
                return cmp(0.0, static_cast<double>(rhs));
            if (past_64_bits(rhs))
                return cmp(rational{0}, rational{rhs < 0 ? -1 : 1});
            if (!abs_fraction_exact(static_cast<double>(rhs))) [[unlikely]] // finer than 2^-62
                return cmp(exact_of(lhs), exact_of_double(static_cast<double>(rhs)));
        }
        return cmp(as_rational(lhs), rational{rhs});
    }
}
} // namespace detail

template <insidable B, detail::plain_number A>
[[nodiscard]] constexpr auto operator<=>(const B& lhs, A rhs) {
    if constexpr (std::floating_point<A>)
        return detail::compare_scalar(lhs, rhs, detail::three_way_partial);
    else
        return detail::compare_scalar(lhs, rhs, detail::three_way);
}

template <insidable B, detail::plain_number A>
[[nodiscard]] constexpr bool operator==(const B& lhs, A rhs) {
    return detail::compare_scalar(lhs, rhs, detail::equal_to);
}
} // namespace beman::inside


// ======================================================================
//  beman/inside/casts.hpp
// ======================================================================



//---------------------------------------------------------------------------
// Free-function casts next to the constructors: clamp_cast<B> (optionally
// clamp_cast<B, round_floor> & co.), wrap_cast<B>, checked_cast<B> and
// unchecked_cast<B>. Unlike B{value} they name the intent (clamp, wrap,
// report, trust) at the call site, which reads well in algorithm callbacks.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
inline constexpr policy_flag rounding_flags = round_nearest | round_floor | round_ceil | round_half_even;
inline constexpr policy_flag range_flags    = clamp | wrap;

// B's grid under B's flags with the `Replaced` ones swapped for `Cast`: a
// one-shot flag overrides the declared one of its kind (OR-ing them would
// let the type's rounding mode outrank the cast's, or its clamp the cast's
// wrap). Same grid, so the raw carries over.
template <insidable B, policy_flag Cast, policy_flag Replaced>
using cast_twin = inside<grid_of<B>, (flags_of<B> & ~Replaced) | Cast>;

// Each cast constructs through such a twin of B — its policy overrides B's
// declared one for this conversion only — and is constrained like that
// constructor, so a probe sees whether it compiles.

// clamp, and with a rounding mode, that mode in place of B's own:
// clamp_cast<B, round_floor> is the "double in, bounded integer out, never
// throw" pipeline.
template <insidable B, policy_flag Round>
using clamp_twin = std::conditional_t<Round == none,
                                      cast_twin<B, clamp, range_flags>,
                                      cast_twin<B, clamp | Round, range_flags | rounding_flags>>;

template <insidable B>
using wrap_twin = cast_twin<B, wrap, range_flags>;

template <policy_flag Round>
concept cast_rounding =
    Round == none || Round == round_nearest || Round == round_floor || Round == round_ceil || Round == round_half_even;
} // namespace detail

template <insidable B, policy_flag Round = none, numeric N>
    requires detail::cast_rounding<Round> &&
             inside_assignable<detail::clamp_twin<B, Round>, N, flags_of<detail::clamp_twin<B, Round>>>
[[nodiscard]] constexpr B clamp_cast(N value) {
    return B::from_raw(detail::clamp_twin<B, Round>{value}.raw());
}

// `wrap_cast` — modular semantics: the input is reduced into the target grid's
// interval rather than clipped. For integer-style wraparound (angles, indices).
template <insidable B, numeric N>
    requires inside_assignable<detail::wrap_twin<B>, N, flags_of<detail::wrap_twin<B>>>
[[nodiscard]] constexpr B wrap_cast(N value) {
    return B::from_raw(detail::wrap_twin<B>{value}.raw());
}

// `checked_cast` — throws (via the installed handler) when the value would not
// fit exactly: errc::overflow out of the interval (as to<T> and the predicate
// name it), errc::rounding_error off the notch. Any numeric source, insides
// included; once both checks pass the store is exact.
template <insidable B, numeric A>
    requires inside_assignable<B, A, flags_of<B> | snap>
[[nodiscard]] constexpr B checked_cast(A value) {
    if (conversion_overflows<B>(value))
        detail::raise(errc::overflow, "checked_cast: value out of inside interval");
    if (conversion_rounds<B>(value))
        detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value, make_policy<snap>()};
}

// `unchecked_cast` routes through `inside<G, unsafe>` so the compiler elides
// every domain/round check. UB if the value is actually out of range.
template <insidable B, numeric A>
    requires inside_assignable<inside<grid_of<B>, unsafe>, A, unsafe>
[[nodiscard]] constexpr B unchecked_cast(A value) {
    using twin = inside<grid_of<B>, unsafe>;
    return B::from_raw(twin{value}.raw()); // same grid → identical raw layout
}

} // namespace beman::inside


// ======================================================================
//  beman/inside/arithmetic.hpp
// ======================================================================




//---------------------------------------------------------------------------
// Free-function arithmetic over detail::addition/multiplication/division/
// modulo:
//   add, sub, mul, div, mod, divmod   with a policy, hooks or an errc&
//   mul_into<Out>, div_into<Out>      the exact result, rounded once into Out
//   add_all, mul_all, sum<Target>     folds; sum checks the total once
//   min, max, midpoint                ADL-found, exact on mixed grids
//   dot, cross, lerp                  2-D helpers that widen like + and *
//   common_inside_t, std::common_type the hull of two grids
// and the operators, including the std::expected-lift overloads (an error
// operand passes through; detail::lift_operand_v names the operand types).
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// add / sub / mul / div / mod / divmod — each takes one of three trailing forms:
//   op(l, r [, policy [, action]])   explicit per-call policy (+ action)
//   op(l, r, on_overflow(λ), ...)    tagged actions (policy = their implied flags)
//   op(l, r, ec [, action])          error-code form (checked, reports into ec)
// detail::arith normalizes the form to (policy, action) for the op's core.
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_WHY_arith_action                                                                               \
    "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error apply to assignment — use them with " \
    "b.with(...) or a cast"

namespace detail {
// Arithmetic fires only on_overflow; any other action would be silently
// ignored, so it is rejected.
template <class A>
inline constexpr bool arith_accepts_action =
    std::same_as<std::remove_cvref_t<A>, no_action> || overflow_action<std::remove_cvref_t<A>>;

template <class Op, class L, class R, policy_like P = policy<>, class A = no_action>
constexpr auto arith(Op op, const L& l, const R& r, P&& pol = {}, A&& act = {}) {
    static_assert(arith_accepts_action<A>, BEMAN_INSIDE_WHY_arith_action);
    return op(l, r, std::forward<P>(pol), std::forward<A>(act));
}

// Action-first form: on_overflow actions (the only kind arithmetic fires;
// any other tag is rejected below with a message, not a bare mismatch).
template <class A>
inline constexpr bool is_action_tag_v =
    is_overflow_action<A>::value || is_clamp_action<A>::value || is_wrap_action<A>::value || is_error_action<A>::value;

template <class Op, class L, class R, class... Actions>
    requires(sizeof...(Actions) >= 1) && (is_action_tag_v<std::remove_cvref_t<Actions>> && ...)
constexpr auto arith(Op op, const L& l, const R& r, Actions&&... acts) {
    static_assert((overflow_action<std::remove_cvref_t<Actions>> && ...), BEMAN_INSIDE_WHY_arith_action);
    return op(l, r, make_policy<merged_implied_flags<Actions...>>(), pick_action<is_overflow_action>(acts...));
}

template <class Op, class L, class R, class A = no_action>
constexpr auto arith(Op op, const L& l, const R& r, errc& ec, A&& act = {}) {
    static_assert(arith_accepts_action<A>, BEMAN_INSIDE_WHY_arith_action);
    return op(l, r, make_policy<checked>(ec), std::forward<A>(act));
}

struct add_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return addition<L, R>::add(l, r, std::forward<P>(p), std::forward<A>(a));
    }
};
struct sub_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return add_op{}(l, -r, std::forward<P>(p), std::forward<A>(a));
    }
};
struct mul_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return multiplication<L, R>::mul(l, r, std::forward<P>(p), std::forward<A>(a));
    }
};
struct div_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return division<L, R, flags_of<P>>::div(l, r, p, std::forward<A>(a));
    }
};
struct mod_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return modulo<L, R, flags_of<P>>::mod(l, r, p, std::forward<A>(a));
    }
};
struct divmod_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return quotient_remainder<L, R, flags_of<P>>::divmod(l, r, p, std::forward<A>(a));
    }
};
} // namespace detail

// A deleted overload takes each op's failing case: `requires { a + b; }` is
// then false, and an integer inside cannot fall back on its imax conversion
// and the built-in operator.

#define BEMAN_INSIDE_WHY_add "addition: a wide-index operand meets a continuous result (not supported yet)"
#define BEMAN_INSIDE_WHY_sub "subtraction: a wide-index operand meets a continuous result (not supported yet)"
#define BEMAN_INSIDE_WHY_mul                                                                                       \
    "multiplication: a wide-index operand meets a continuous result (not supported yet) - round the product into " \
    "a coarser type with mul_into<Out>(a, b)"
#define BEMAN_INSIDE_WHY_div \
    "division: no result grid - the divisor grid is the point 0, or an unbounded continuous divisor reaches 0"
#define BEMAN_INSIDE_WHY_mod "modulo requires integer-valued grids"
#define BEMAN_INSIDE_WHY_divmod "divmod requires integer-valued grids"

namespace detail {
// The policy flags a call's trailing arguments select, as arith normalizes
// them: a policy's own, checked for an errc&, an action's implied flags.
template <class... Args>
inline constexpr policy_flag call_flags = [] {
    if constexpr (sizeof...(Args) == 0)
        return none;
    else if constexpr (policy_like<std::remove_cvref_t<Args...[0]>>)
        return flags_of<Args...[0]>;
    else if constexpr (std::same_as<Args...[0], errc&>)
        return checked;
    else
        return merged_implied_flags<Args...>;
}();

// Each op's well-formedness under flags F is detail::add_defined & co.
// (detail/addition.hpp, multiplication.hpp, division.hpp): the first
// constraint of its function and operator, so a requires-expression or
// `if constexpr` probe is false where the op cannot compile.
} // namespace detail

#define BEMAN_INSIDE_ARITH_FN(name, op)                                             \
    template <insidable L, insidable R, class... Args>                              \
        requires detail::name                                                       \
    ##_defined<L, R, detail::call_flags<Args...>>&&                                 \
        requires(L const& l, R const& r, Args && ... args)                          \
    {                                                                               \
        detail::arith(detail::op{}, l, r, std::forward<Args>(args)...);             \
    }                                                                               \
    [[nodiscard]] constexpr auto name(L const& lhs, R const& rhs, Args&&... args) { \
        return detail::arith(detail::op{}, lhs, rhs, std::forward<Args>(args)...);  \
    }                                                                               \
    template <insidable L, insidable R, class... Args>                              \
        requires(!detail::name##_defined<L, R, detail::call_flags<Args...>>)        \
    auto name(L const&, R const&, Args&&...) = delete (BEMAN_INSIDE_WHY_##name);

BEMAN_INSIDE_ARITH_FN(add, add_op)
BEMAN_INSIDE_ARITH_FN(sub, sub_op)
BEMAN_INSIDE_ARITH_FN(mul, mul_op)
BEMAN_INSIDE_ARITH_FN(div, div_op)
BEMAN_INSIDE_ARITH_FN(mod, mod_op)
BEMAN_INSIDE_ARITH_FN(divmod, divmod_op)
#undef BEMAN_INSIDE_ARITH_FN

//---------------------------------------------------------------------------
// mul_into<Out> / div_into<Out> — the exact product or quotient, rounded once
// onto Out by Out's policy (plus the per-call flags F). Neither forms the
// product or quotient grid, so they work where `a * b` would meet a wide index
// with a continuous result, or a quotient past the 64-bit fraction. A store that
// fails is reported through the policy (it throws under `checked`).
// div_into returns std::expected<Out, errc> when the divisor's grid holds 0 —
// then every failure, a zero divisor or a failed store, is the error — else Out.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Out, std::size_t K, typename P>
constexpr Out store_exact_into(const wide_rational<K>& v, P&& policy) {
    Out out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
}
// The same store from a value with 64-bit numbers: an inside or a rational.
template <insidable Out, numeric V, typename P>
constexpr Out store_into(const V& v, P&& policy) {
    Out out{};
    assignment<Out, V>::assign(out, v, policy, no_action{});
    return out;
}
// Native paths: the exact product as an inside when its grid has 64-bit raws,
// else the product or quotient as a 64-bit rational when it fits. Each rounds
// once, as the wide path does; past them the wide path is the fallback.
// An inside-to-inside store accepts the product only where its compile-time
// checks pass: notches that fit (or snap) and an interval that meets Out's (or
// clamp/wrap). The wide path reports those cases at run time instead.
template <insidable Out, policy_flag F, insidable A, insidable B>
inline constexpr bool native_product = [] {
    if constexpr (!mul_defined<A, B>)
        return false;
    else {
        using P                   = typename multiplication<A, B>::result;
        constexpr policy_flag all = flags_of<Out> | F;
        return values_fit_imax<P> && (notches_compatible<Out, P> || has_flag(all, snap)) &&
               (has_flag(all, clamp) || has_flag(all, wrap) || !excludes(interval_of<Out>, interval_of<P>));
    }
}();
template <insidable A, insidable B>
inline constexpr bool rational_operands = rational_readable<A> && rational_readable<B>;

template <insidable Out, insidable A, insidable B, typename P>
constexpr Out mul_into_wide(const A& a, const B& b, P&& policy) {
    using E = wide_rational<exact_limbs<A, B, Out>>;
    return store_exact_into<Out>(E{exact_of(a)} * E{exact_of(b)}, policy);
}
template <insidable Out, insidable A, insidable B, typename P>
constexpr Out div_into_wide(const A& a, const B& b, P&& policy) {
    using E = wide_rational<exact_limbs<A, B, Out>>;
    return store_exact_into<Out>(E{exact_of(a)} / E{exact_of(b)}, policy);
}
} // namespace detail

template <insidable Out, policy_flag F = none, insidable A, insidable B>
[[nodiscard]] constexpr Out mul_into(const A& a, const B& b) {
    auto policy = make_policy<flags_of<Out> | F>();
    if constexpr (detail::native_product<Out, F, A, B>)
        return detail::store_into<Out>(a * b, policy);
    else {
        if constexpr (detail::rational_operands<A, B>)
            if (const auto p = detail::try_mul(detail::as_rational(a), detail::as_rational(b)))
                return detail::store_into<Out>(*p, policy);
        return detail::mul_into_wide<Out>(a, b, policy);
    }
}

template <insidable Out, policy_flag F = none, insidable A, insidable B>
[[nodiscard]] constexpr auto div_into(const A& a, const B& b)
    -> std::conditional_t<detail::divisor_excludes_zero<B>, Out, std::expected<Out, errc>> {
    const auto quotient = [&](auto&& policy) {
        if constexpr (detail::rational_operands<A, B>)
            if (const auto q = detail::try_div(detail::as_rational(a), detail::as_rational(b)))
                return detail::store_into<Out>(*q, policy);
        return detail::div_into_wide<Out>(a, b, policy);
    };
    if constexpr (detail::divisor_excludes_zero<B>)
        return quotient(make_policy<flags_of<Out> | F>());
    else {
        if (b == 0)
            return std::unexpected{errc::division_by_zero};
        return detail::try_store<Out, flags_of<Out> | F>([&](Out& out, auto&& policy) { out = quotient(policy); });
    }
}

// Binary operators: +, -, * use the default policy; / and % carry the
// operands' own policies (snap/rounding select the native integer paths).
// Each is constrained on its op's well-formedness, like the named forms.
template <insidable L, insidable R>
    requires detail::add_defined<L, R>
[[nodiscard]] constexpr auto operator+(L lhs, R rhs) {
    return beman::inside::add(lhs, rhs);
}
template <insidable L, insidable R>
    requires(!detail::add_defined<L, R>)
auto operator+(L, R) = delete (BEMAN_INSIDE_WHY_add);

template <insidable L, insidable R>
    requires detail::sub_defined<L, R>
[[nodiscard]] constexpr auto operator-(L lhs, R rhs) {
    return beman::inside::sub(lhs, rhs);
}
template <insidable L, insidable R>
    requires(!detail::sub_defined<L, R>)
auto operator-(L, R) = delete (BEMAN_INSIDE_WHY_sub);

template <insidable L, insidable R>
    requires detail::mul_defined<L, R>
[[nodiscard]] constexpr auto operator*(L lhs, R rhs) {
    return beman::inside::mul(lhs, rhs);
}
template <insidable L, insidable R>
    requires(!detail::mul_defined<L, R>)
auto operator*(L, R) = delete (BEMAN_INSIDE_WHY_mul);

template <insidable L, insidable R>
    requires detail::div_defined<L, R, flags_of<L> | flags_of<R>>
[[nodiscard]] constexpr auto operator/(L lhs, R rhs) {
    return beman::inside::div(lhs, rhs, make_policy<flags_of<L> | flags_of<R>>());
}
template <insidable L, insidable R>
    requires(!detail::div_defined<L, R, flags_of<L> | flags_of<R>>)
auto operator/(L, R) = delete (BEMAN_INSIDE_WHY_div);

template <insidable L, insidable R>
    requires detail::mod_defined<L, R, flags_of<L> | flags_of<R>>
[[nodiscard]] constexpr auto operator%(L lhs, R rhs) {
    return beman::inside::mod(lhs, rhs, make_policy<flags_of<L> | flags_of<R>>());
}
template <insidable L, insidable R>
    requires(!detail::mod_defined<L, R, flags_of<L> | flags_of<R>>)
auto operator%(L, R) = delete (BEMAN_INSIDE_WHY_mod);
#undef BEMAN_INSIDE_WHY_arith_action
#undef BEMAN_INSIDE_WHY_add
#undef BEMAN_INSIDE_WHY_sub
#undef BEMAN_INSIDE_WHY_mul
#undef BEMAN_INSIDE_WHY_div
#undef BEMAN_INSIDE_WHY_mod
#undef BEMAN_INSIDE_WHY_divmod

//---------------------------------------------------------------------------
// add_all / mul_all — variadic folds (pairwise widening, same as `a + b + c`
// but reads cleaner; matches Chromium's `CheckAdd(a, b, c)`).
//---------------------------------------------------------------------------
template <insidable First, insidable... Rest>
[[nodiscard]] constexpr auto add_all(const First& first, const Rest&... rest) {
    return (first + ... + rest);
}

template <insidable First, insidable... Rest>
[[nodiscard]] constexpr auto mul_all(const First& first, const Rest&... rest) {
    return (first * ... * rest);
}

//---------------------------------------------------------------------------
// sum<Target> — bulk reduction with ONE deferred range check. Per-element
// `target += b` re-validates every step (blocks vectorization); this computes
// the exact total and stores it into Target once, by Target's policy
// (semantic difference: the *total* is validated, not every prefix).
// Notched integer raws (wide ones too) sum value indices exactly — Σvalue =
// Notch·ΣJ — in a wide integer with 64 bits of headroom for the count; ≤32-bit
// raws add in imax blocks of 2^30 elements, the loop that vectorizes.
// Continuous elements add as 64-bit
// rationals; a total past that reports overflow through Target's policy.
//---------------------------------------------------------------------------
template <insidable Target, std::ranges::input_range Rng>
    requires insidable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
[[nodiscard]] constexpr Target sum(Rng&& r) {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    Target out{};
    auto   policy = make_policy<flags_of<Target>>();

    if constexpr (detail::integer_storage<B> && !detail::point_storage<B> && detail::notched<B>) {
        // The total in units of U, the value unit: the notch on an anchored
        // grid, so each value counts as its value index.
        constexpr detail::grid_rational U = grid_of<B>.value_unit();
        constexpr int bits = detail::signed_value_bits_of({detail::units_lo<B, U>, detail::units_hi<B, U>}) + 64;
        using I            = detail::wide_sint<detail::limbs_for_bits(bits)>;
        I total{0};
        if constexpr (!detail::wide_index_storage<B> && sizeof(detail::raw_t<B>) <= 4) {
            // Each value is base + raw·scale units (scale 1 when anchored).
            constexpr imax base =
                detail::index_storage<B> ? static_cast<imax>(detail::exact_quotient(lower_of<B>, U)) : 0;
            constexpr imax scale = static_cast<imax>(detail::exact_quotient(notch_of<B>, U));
            auto           it    = std::ranges::begin(r);
            auto           end   = std::ranges::end(r);
            while (it != end) {
                imax acc = 0, cnt = 0;
                // (A random-access range whose end is a sentinel, not a
                // reachable iterator, has no distance to it: the loop below.)
                if constexpr (std::ranges::random_access_range<Rng> &&
                              std::sized_sentinel_for<std::ranges::sentinel_t<Rng>, std::ranges::iterator_t<Rng>>) {
                    const imax block = std::min<imax>(end - it, imax{1} << 30);
                    for (imax j = 0; j < block; ++j)
                        acc += detail::raw_imax(it[j]);
                    it += block;
                    cnt = block;
                } else {
                    for (; it != end && cnt < (imax{1} << 30); ++it, ++cnt)
                        acc += detail::raw_imax(*it);
                }
                if constexpr (scale == 1)
                    total += I{acc} + I{cnt} * I{base};
                else
                    total += I{acc} * I{scale} + I{cnt} * I{base};
            }
        } else
            for (const auto& b : r)
                total += detail::value_in_units<I, U>(b);
        if constexpr (!detail::wide_valued<B>) {
            // A total within imax: the cheaper 64-bit rational store.
            constexpr imax lo = std::numeric_limits<imax>::min(), hi = std::numeric_limits<imax>::max();
            if (!(total < I{lo}) && !(I{hi} < total))
                if (const auto v =
                        detail::try_mul(detail::rational{static_cast<imax>(total)}, detail::to_rational(U))) {
                    detail::assignment<Target, detail::rational>::assign(out, *v, policy, no_action{});
                    return out;
                }
        }
        constexpr std::size_t K =
            detail::limbs_for_bits(I::bits + detail::exact_value_bits<B> + detail::exact_value_bits<Target>);
        using W = detail::wide_sint<K>;
        const detail::wide_rational<K> v{static_cast<W>(total) * static_cast<W>(detail::wide_numerator(U)),
                                         static_cast<W>(detail::wide_denominator(U))};
        no_action                      no_op;
        detail::assign_exact<detail::rational>(out, v, policy, no_op);
    } else {
        detail::rational total{0};
        for (const auto& b : r) {
            const auto s = total + detail::as_rational(b);
            if (!s) [[unlikely]] {
                no_action no_op;
                detail::report_failure(out, policy, no_op, s.error());
                return out;
            }
            total = *s;
        }
        detail::assignment<Target, detail::rational>::assign(out, total, policy, no_action{});
    }
    return out;
}

//---------------------------------------------------------------------------
// dot / cross / lerp — 2-D inside-space vector helpers. Each widens its result
// grid like the underlying `+`/`*`, so no overflow and the result is a plain
// `inside`. (cross is the z-component, useful for "which side" tests.)
//---------------------------------------------------------------------------
[[nodiscard]] constexpr auto dot(insidable auto ax, insidable auto ay, insidable auto bx, insidable auto by) {
    return ax * bx + ay * by;
}

[[nodiscard]] constexpr auto cross(insidable auto ax, insidable auto ay, insidable auto bx, insidable auto by) {
    return ax * by - ay * bx;
}

// lerp(a, b, t) = a + (b - a) * t. `t` is itself an inside (typically a
// [0, 1] fixed-point grid), so the interpolation never leaves inside-space.
[[nodiscard]] constexpr auto lerp(insidable auto a, insidable auto b, insidable auto t) { return a + (b - a) * t; }

//---------------------------------------------------------------------------
// common_inside — the "hull" type able to hold every value of L and R exactly:
// interval hull + notch gcd (grid `hull`), checked like arithmetic results.
// Backs the std::common_type specialization and mixed-grid min/max below.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Lhs, insidable Rhs>
struct common_inside {
    using type = inside<hull(grid_of<Lhs>, grid_of<Rhs>)>;
};

// Same type stays itself (policy included) — mirrors std::common_type<T, T>.
template <insidable Same>
struct common_inside<Same, Same> {
    using type = Same;
};
} // namespace detail

template <insidable Lhs, insidable Rhs>
using common_inside_t = typename detail::common_inside<Lhs, Rhs>::type;

//---------------------------------------------------------------------------
// std-vocabulary helpers — ADL-found `min` / `max` / `midpoint` for generic
// code. min/max mirror std; midpoint returns the *exact* average on a refined
// grid (so, unlike std::midpoint, it neither rounds nor overflows). There is
// no free `beman::inside::clamp` (the name is the policy flag — use clamp_cast<Target>).
//---------------------------------------------------------------------------
// Both return the common type: the operand type itself for one type, else
// the hull, which holds each operand exactly.
template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto min(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs> {
    common_inside_t<Lhs, Rhs> ca{a}, cb{b};
    return (cb < ca) ? cb : ca;
}

template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto max(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs> {
    common_inside_t<Lhs, Rhs> ca{a}, cb{b};
    return (ca < cb) ? cb : ca;
}

// The exact average, on the refined sum grid.
template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto midpoint(Lhs a, Rhs b) {
    return (a + b) * just<1, 2>;
}

//---------------------------------------------------------------------------
// expected-lift operators — fallible results (division, modulo, checked
// rational arithmetic, beman::inside::math) chain without per-step unwrapping:
// `a / b * gain + offset` and `math::tan(x) * gain` stay a
// std::expected<inside, errc> end to end. The first error short-circuits and
// keeps its cause; an operation that fails inside the chain reports its own
// (overflow, division_by_zero, ...). One overload per operator covers all
// three shapes (expected op inside, inside op expected, expected op expected);
// the lambda's `l + r` re-enters resolution on the unwrapped values,
// inheriting whichever bare overload applies.
//---------------------------------------------------------------------------
namespace detail {
// Other types whose results chain the same way (units.hpp: quantity).
template <class T>
inline constexpr bool lift_operand_v = insidable<T>;

template <class L, class R>
concept expected_operands =
    (expected_like<L> || expected_like<R>) && (lift_operand_v<unwrap_t<L>> || lift_operand_v<unwrap_t<R>>);
} // namespace detail

#define BEMAN_INSIDE_LIFT_OP(op)                                                            \
    template <class L, class R>                                                             \
        requires detail::expected_operands<L, R> &&                                         \
                 requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l op r; }         \
    [[nodiscard]] constexpr auto operator op(L const& lhs, R const& rhs) {                  \
        return detail::lift([](auto const& l, auto const& r) { return l op r; }, lhs, rhs); \
    }

BEMAN_INSIDE_LIFT_OP(+)
BEMAN_INSIDE_LIFT_OP(-)
BEMAN_INSIDE_LIFT_OP(*)
BEMAN_INSIDE_LIFT_OP(/)
BEMAN_INSIDE_LIFT_OP(%)
#undef BEMAN_INSIDE_LIFT_OP

//---------------------------------------------------------------------------
// Grid-less scalar operands are rejected. A raw int/double carries no grid, so
// `inside op rawscalar` has no type-safe result; rather than silently escape
// into rational/double, these guidance overloads make it ill-formed with a fix
// (give the literal a grid: `1_ins` / `just<1>`, or an inside over its range).
// Compound assignment with a raw scalar is rejected the same way; comparisons
// with raw scalars are unaffected.
//
// Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
// so `requires { b + 1; }` stays well-formed and the static_assert fires only
// on a call.
//---------------------------------------------------------------------------
template <typename A>
concept builtin_arithmetic = std::integral<A> || std::floating_point<A>;

#define BEMAN_INSIDE_SCALAR_MSG                                                     \
    "an inside cannot be combined with a raw scalar: give the scalar a grid — "     \
    "`1_ins`, `just<1>`, or `inside<{lo,hi}>{n}` for a runtime value with a known " \
    "range"
#define BEMAN_INSIDE_NO_SCALAR(op)                     \
    template <insidable B, builtin_arithmetic A>       \
    B operator op(B const&, A) {                       \
        static_assert(false, BEMAN_INSIDE_SCALAR_MSG); \
    }                                                  \
    template <builtin_arithmetic A, insidable B>       \
    B operator op(A, B const&) {                       \
        static_assert(false, BEMAN_INSIDE_SCALAR_MSG); \
    }                                                  \
    template <insidable B, builtin_arithmetic A>       \
    B& operator op## = (B&, A) {                       \
        static_assert(false, BEMAN_INSIDE_SCALAR_MSG); \
    }

BEMAN_INSIDE_NO_SCALAR(+)
BEMAN_INSIDE_NO_SCALAR(-)
BEMAN_INSIDE_NO_SCALAR(*)
BEMAN_INSIDE_NO_SCALAR(/)
BEMAN_INSIDE_NO_SCALAR(%)
#undef BEMAN_INSIDE_NO_SCALAR
#undef BEMAN_INSIDE_SCALAR_MSG

} // namespace beman::inside

// std::common_type is the grid hull (beman::inside::common_inside_t above), so
// mixed-grid insides work in generic code (std::max over two grids, ranges)
// without opting into numeric_limits.hpp.
template <beman::inside::grid G1, beman::inside::policy_flag P1, beman::inside::grid G2, beman::inside::policy_flag P2>
struct std::common_type<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>>
    : beman::inside::detail::common_inside<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>> {};


// ======================================================================
//  beman/inside/range.hpp
// ======================================================================




//---------------------------------------------------------------------------
// inside_range — random-access range over a grid, and values<T>, the same
// range named by a type (every value of T). Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. A random-access sized view (view_interface adds [], front, back,
// empty) and a borrowed range: its iterators never refer to it, so an
// algorithm on a temporary still returns an iterator. iterator_category is
// input_iterator_tag because operator* returns by value; iterator_concept
// carries the random-access capability.
//---------------------------------------------------------------------------
namespace beman::inside {
template <grid G, policy_flag P = checked>
    requires(G.Notch != 0)
struct inside_range : std::ranges::view_interface<inside_range<G, P>> {
    using value_type = inside<G, P>;
    // The iterator's difference is an imax: end() − begin() must be the count.
    static_assert(G.max_index_representable() &&
                      detail::max_index_of<value_type> < static_cast<umax>(std::numeric_limits<imax>::max()),
                  "inside_range: the grid has more points than an iterator difference (int64) can count");
    static constexpr umax slot_count = detail::max_index_of<value_type> + 1;

    struct iterator {
        using iterator_concept  = std::random_access_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type        = inside<G, P>;
        using difference_type   = imax;

        umax Start{0}; // slot of the first element (the range wraps past the top)
        imax Pos{0};   // position in [0, slot_count]; the loop variable

        constexpr iterator() = default;
        constexpr iterator(umax s, imax p) : Start{s}, Pos{p} {}

        // Grid slot of this position: Start + Pos, wrapped once (no overflow).
        constexpr umax slot() const {
            const umax p = static_cast<umax>(Pos);
            return p < slot_count - Start ? Start + p : p - (slot_count - Start);
        }

        [[nodiscard]] constexpr value_type operator*() const {
            // value = Lower + index * Notch: an integer raw (a range needs a
            // notch), slot() for an index raw, Lower + slot() for a value raw.
            return value_type::from_raw(detail::raw_from_offset<value_type>(slot()));
        }

        [[nodiscard]] constexpr value_type operator[](difference_type n) const { return *(*this + n); }

        constexpr iterator& operator++() {
            ++Pos;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto t = *this;
            ++Pos;
            return t;
        }
        constexpr iterator& operator--() {
            --Pos;
            return *this;
        }
        constexpr iterator operator--(int) {
            auto t = *this;
            --Pos;
            return t;
        }
        constexpr iterator& operator+=(difference_type n) {
            Pos += n;
            return *this;
        }
        constexpr iterator& operator-=(difference_type n) {
            Pos -= n;
            return *this;
        }

        [[nodiscard]] constexpr iterator operator+(difference_type n) const {
            auto t = *this;
            t += n;
            return t;
        }
        [[nodiscard]] constexpr iterator operator-(difference_type n) const {
            auto t = *this;
            t -= n;
            return t;
        }
        [[nodiscard]] friend constexpr iterator operator+(difference_type n, iterator it) { return it + n; }

        [[nodiscard]] constexpr difference_type operator-(iterator o) const { return Pos - o.Pos; }
        [[nodiscard]] constexpr bool            operator==(iterator o) const { return Pos == o.Pos; }
        [[nodiscard]] constexpr auto            operator<=>(iterator o) const { return Pos <=> o.Pos; }
    };

    umax StartIndex{0};

    constexpr inside_range() = default;
    // A grid value's notch index: its raw's offset.
    constexpr inside_range(value_type start) : StartIndex{detail::raw_offset<value_type>(start.raw())} {}

    constexpr iterator begin() const { return {StartIndex, 0}; }
    constexpr iterator end() const { return {StartIndex, static_cast<imax>(slot_count)}; }

    constexpr std::size_t size() const { return slot_count; }
};

// values<T>: every value of T, Lower to Upper, as a random-access sized range.
// A cursor has none: its grid ends past T's Upper.
template <insidable T>
    requires(notch_of<T> != 0 && !has_flag(flags_of<T>, detail::cursor_marker))
inline constexpr inside_range<grid_of<T>, flags_of<T>> values{};

} // namespace beman::inside

template <beman::inside::grid G, beman::inside::policy_flag P>
inline constexpr bool std::ranges::enable_borrowed_range<beman::inside::inside_range<G, P>> = true;


// ======================================================================
//  beman/inside/cursor.hpp
// ======================================================================



namespace beman::inside {
//---------------------------------------------------------------------------
// cursor<T, Step> — an inside on {{Lower, Upper + Step}, Step} of T: it steps
// (++ moves one notch) from Lower through Upper and one step past it, the
// end. It keeps T's checks and rounding, default construction starts at
// Lower, and it carries cursor_marker, which gives it
// `end(t)`. Step is a number (an integer, `rational`, `per<N>`), T's notch by
// default: a positive whole number of notches of T dividing its range, so
// every value but the end is a value of T.
//   for (cursor<time_t, per<4>> t; t != end(t); ++t) …
//---------------------------------------------------------------------------
namespace detail {
template <insidable T>
consteval grid cursor_grid(const auto& step) {
    const grid_rational s = grid{step}.Interval.Lower;
    if (!(s > 0))
        constexpr_error<"cursor<T, Step>: the step must be positive - a cursor steps forward, one past the end">();
    if (notch_of<T> == 0)
        constexpr_error<"cursor<T, Step>: T has no notch - a continuous grid has no lattice to step on">();
    if (!interval_of<T>.divides_evenly(s))
        constexpr_error<"cursor<T, Step>: the step must divide the range of T evenly">();
    if (!grid_divides_evenly(s, notch_of<T>))
        constexpr_error<"cursor<T, Step>: the step must be a whole number of notches of T">();
    return grid{interval{lower_of<T>, upper_of<T> + s}, s};
}
} // namespace detail

template <insidable T, auto Step = notch_of<T>>
using cursor = inside<detail::cursor_grid<T>(Step), flags_of<T> | detail::cursor_marker>;

// end(t): a cursor's past-the-end value, one step past T's Upper. Only a
// cursor has one — on any other inside its Upper is a value, not an end.
template <insidable B>
    requires(has_flag(flags_of<B>, detail::cursor_marker))
[[nodiscard]] constexpr auto end(const B&) noexcept {
    return just<upper_of<B>>;
}
} // namespace beman::inside



// ======================================================================
//  beman/inside/cmath.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/math_constants.hpp
// ======================================================================



//---------------------------------------------------------------------------
// Mathematical constants as point insides: for each, the best rational whose
// numerator and denominator stay below 2^31, so products of a few of them
// keep 64-bit grid numbers (the fast paths). The comment gives
// |rational − value|. They compose directly in inside-space
// (`angle * math::pi`); for a correctly rounded value on a grid of your
// choice, use the functions (math::sqrt_into<Out>(just<2>)). Public from
// cmath.hpp and constants.hpp.
//---------------------------------------------------------------------------
namespace beman::inside::math {
inline constexpr auto pi          = just<rational{1068966896, 340262731}>;  // 2^-58.2
inline constexpr auto two_pi      = just<rational{2137933792, 340262731}>;  // 2^-57.2
inline constexpr auto half_pi     = just<rational{2015008331, 1282794145}>; // 2^-60.2
inline constexpr auto inv_pi      = just<rational{340262731, 1068966896}>;  // 2^-61.5
inline constexpr auto inv_sqrt_pi = just<rational{1196609286, 2120934737}>; // 2^-58.0
inline constexpr auto e           = just<rational{848456353, 312129649}>;   // 2^-60.5
inline constexpr auto log2e       = just<rational{717140287, 497083768}>;   // 2^-61.6
inline constexpr auto log10e      = just<rational{774923109, 1784326399}>;  // 2^-66.1
inline constexpr auto ln2         = just<rational{497083768, 717140287}>;   // 2^-62.6
inline constexpr auto ln10        = just<rational{1784326399, 774923109}>;  // 2^-63.7
inline constexpr auto sqrt2       = just<rational{1855077841, 1311738121}>; // 2^-62.1
inline constexpr auto sqrt3       = just<rational{1934726305, 1117014753}>; // 2^-60.9
inline constexpr auto inv_sqrt2   = just<rational{1311738121, 1855077841}>; // 2^-63.1
inline constexpr auto inv_sqrt3   = just<rational{1117014753, 1934726305}>; // 2^-62.5
inline constexpr auto phi         = just<rational{1836311903, 1134903170}>; // golden ratio, 2^-61.3
inline constexpr auto egamma      = just<rational{376566901, 652385103}>;   // Euler–Mascheroni γ, 2^-62.2
inline constexpr auto catalan     = just<rational{150820265, 164657129}>;   // Catalan's G, 2^-59.4
inline constexpr auto zeta3       = just<rational{461424925, 383862797}>;   // Apéry's ζ(3), 2^-60.6
} // namespace beman::inside::math


// ======================================================================
//  beman/inside/detail/math_entry.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/math_cores.hpp
// ======================================================================



// ======================================================================
//  beman/inside/detail/math_adaptive.hpp
// ======================================================================



#if BEMAN_INSIDE_MATH_INSTRUMENT
    #include <atomic>
    #include <cstdint>
#endif

//---------------------------------------------------------------------------
// The adaptive math engine's foundation: correctly rounded results on any
// output grid, at a precision chosen from that grid.
//
// A core computes f(x) in signed fixed point Q.W (an integer Y standing for
// Y·2^-W) and says how far off it may be: the true value lies within
// Error·2^-W of Y (an `approx`). `decide` maps both ends of that interval onto
// the output grid under the grid's rounding mode. When they land on the same
// slot, that slot is the correctly rounded result — whichever core, tier or
// precision produced it. When they straddle a rounding boundary, the driver
// (`evaluate`) runs the core again at twice the precision (Ziv's strategy).
//
// A result can land exactly on a boundary only where it is rational: at a
// few exact inputs (exp(0), log(1), √(9/4), ...), which the cores return with
// Error 0. Every other result is irrational, so the escalation ends at some
// precision, though no bound on it is known for an arbitrary grid. A cap
// stops it: past the cap the result is the slot nearest Y, the same in every
// build but not proven to be the correctly rounded one.
//
// Everything here is constexpr and every value is computed in integers
// (wide_int; doubles only size series lengths and error bounds, with
// margins), so a result is the same at compile time and at runtime.
//---------------------------------------------------------------------------
#if BEMAN_INSIDE_MATH_INSTRUMENT
namespace beman::inside::math {
// The engine's run-time instruments (BEMAN_INSIDE_MATH_INSTRUMENT): calls
// that reached the integer path, its deep steps, and results still undecided
// at the cap, stored as the nearest slot (guesses). IntegerOnly sends every
// call to the integer path, past the double and dd tiers. Constant
// evaluation counts nothing.
struct driver_probe {
    std::atomic<std::uint64_t> IntegerRuns{0}, Deep{0}, Guesses{0};
    std::atomic<bool>          IntegerOnly{false};
};
inline driver_probe driver;
} // namespace beman::inside::math
#endif

namespace beman::inside::math::detail::ax {
using namespace ::beman::inside::detail;

// Signed fixed point with at least Bits value bits (the word of a Q.W value
// whose magnitude stays below 2^(Bits − W)).
template <int Bits>
using fixed_t = wide_sint<limbs_for_bits(Bits + 1)>;

template <typename T>
inline constexpr std::size_t limbs_of = sizeof(T) / sizeof(umax);

// The same value in a word of twice the limbs (for products and shifted
// dividends).
template <std::size_t K>
using twice_t = wide_sint<2 * K>;

//---------------------------------------------------------------------------
// Q.W arithmetic. Products and quotients truncate toward zero: each is
// within one unit 2^-W of the true value, and a series' shrinking terms
// reach 0.
//---------------------------------------------------------------------------
template <std::size_t K>
constexpr wide_sint<K> mul_q(const wide_sint<K>& a, const wide_sint<K>& b, int W) noexcept {
    if constexpr (K == 1) {
        // One limb: the 128-bit product of the magnitudes, shifted.
        const bool             na = a.negative(), nb = b.negative();
        const umax             ma = na ? umax{0} - a.Word[0] : a.Word[0];
        const umax             mb = nb ? umax{0} - b.Word[0] : b.Word[0];
        const limb::pair<umax> p  = limb::mul(ma, mb);
        const umax             m  = W == 0 ? p.Lo : W < 64 ? (p.Lo >> W) | (p.Hi << (64 - W)) : p.Hi >> (W - 64);
        wide_sint<1>           r;
        r.Word[0] = (na != nb) ? umax{0} - m : m;
        return r;
    } else {
        // The magnitudes' product into 2K limbs, then the shift. (Not
        // limb::mul_basecase + shift_right: with fast_index on the same
        // kernels the math_decimal workload grows.)
        const bool         na = a.negative(), nb = b.negative();
        const wide_uint<K> ma{na ? -a : a}, mb{nb ? -b : b};
        umax               prod[2 * K]{};
        if constexpr (K == 2) { // the four partial products
            const limb::pair<umax> p00 = limb::mul(ma.Word[0], mb.Word[0]);
            const limb::pair<umax> p01 = limb::mul(ma.Word[0], mb.Word[1]);
            const limb::pair<umax> p10 = limb::mul(ma.Word[1], mb.Word[0]);
            const limb::pair<umax> p11 = limb::mul(ma.Word[1], mb.Word[1]);
            umax                   c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            prod[0] = p00.Lo;
            prod[1] = limb::add_carry(limb::add_carry(p00.Hi, p01.Lo, c1), p10.Lo, c2);
            prod[2] = limb::add_carry(limb::add_carry(limb::add_carry(p11.Lo, p01.Hi, c3), p10.Hi, c4), c1 + c2, c5);
            prod[3] = p11.Hi + c3 + c4 + c5;
        } else
            for (std::size_t i = 0; i < K; ++i) {
                umax carry = 0;
                for (std::size_t j = 0; j < K; ++j) {
                    const limb::pair<umax> t  = limb::mul(ma.Word[i], mb.Word[j]);
                    umax                   c1 = 0, c2 = 0;
                    umax                   v = limb::add_carry(prod[i + j], t.Lo, c1);
                    v                        = limb::add_carry(v, carry, c2);
                    prod[i + j]              = v;
                    carry                    = t.Hi + c1 + c2;
                }
                prod[i + K] = carry;
            }
        const std::size_t ws = static_cast<std::size_t>(W / 64);
        const int         bs = W % 64;
        wide_uint<K>      m;
        for (std::size_t i = 0; i < K; ++i) {
            const umax lo = i + ws < 2 * K ? prod[i + ws] : 0;
            const umax hi = i + ws + 1 < 2 * K ? prod[i + ws + 1] : 0;
            m.Word[i]     = bs == 0 ? lo : (lo >> bs) | (hi << (64 - bs));
        }
        const wide_sint<K> r{m};
        return na != nb ? -r : r;
    }
}

// a / b at scale 2^W. Pre: b != 0.
template <std::size_t K>
constexpr wide_sint<K> div_q(const wide_sint<K>& a, const wide_sint<K>& b, int W) noexcept {
    using D = twice_t<K>;
    return static_cast<wide_sint<K>>((D{a} << W) / D{b});
}

// 2^W as a Q.W one.
template <std::size_t K>
constexpr wide_sint<K> one_q(int W) noexcept {
    return wide_sint<K>{1} << W;
}

// An error bound v·2^-n, rounded down (callers add 1): 0 once n ≥ 64, where
// the shift itself would be undefined.
constexpr umax shr_bound(umax v, int n) noexcept { return n < 64 ? v >> n : 0; }

// v rounded to the nearest multiple of 2^-sh (half away from zero), shifted
// down by sh: changes the scale from 2^(W+sh) to 2^W within ½ unit.
template <std::size_t K>
constexpr wide_sint<K> round_shift(const wide_sint<K>& v, int sh) noexcept {
    if (sh <= 0)
        return v << (-sh);
    const wide_sint<K> half = wide_sint<K>{1} << (sh - 1);
    return v.negative() ? -((-v + half) >> sh) : (v + half) >> sh;
}

//---------------------------------------------------------------------------
// Exact values into fixed point.
//---------------------------------------------------------------------------
// n/d (d > 0) rounded to nearest, half away from zero; one-limb divisions
// when d fits a limb.
template <std::size_t K>
constexpr wide_sint<K> nearest_div(const wide_sint<K>& n, umax d) noexcept {
    const bool neg    = n.negative();
    const auto [q, r] = divmod_small(wide_uint<K>{neg ? -n : n}, d);
    wide_sint<K> m{q};
    if (r >= d - r)
        m += wide_sint<K>{1}; // 2r ≥ d
    return neg ? -m : m;
}
template <std::size_t K>
constexpr wide_sint<K> nearest_div(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    return bit_width_of(d) <= 64 ? nearest_div(n, d.Word[0]) : rounded_div<round_mode::nearest>(n, d);
}

// x·2^W rounded to the nearest integer (half away from zero), within ½
// unit, for a W known only at runtime. KI limbs must hold Num·2^W and Den·2^-W.
template <std::size_t K, std::size_t KI, std::size_t E>
constexpr wide_sint<K> to_q_at(const wide_rational<E>& x, int W) noexcept {
    using I = wide_sint<KI>;
    I n{x.Num}, d{x.Den};
    if (W >= 0)
        n = n << W;
    else
        d = d << (-W);
    return static_cast<wide_sint<K>>(nearest_div(n, d));
}
// The same for a W known at compile time.
template <int W, std::size_t K, std::size_t E>
constexpr wide_sint<K> to_q(const wide_rational<E>& x) noexcept {
    constexpr std::size_t KI = exact_max<E, K> + limbs_for_bits(W >= 0 ? W : -W) + 1;
    if constexpr (W >= 0 && E == 1) // the denominator fits a limb
        return static_cast<wide_sint<K>>(nearest_div(wide_sint<KI>{x.Num} << W, x.Den.Word[0]));
    else
        return to_q_at<K, KI>(x, W);
}

// ⌊log2 |x|⌋ for x != 0.
template <std::size_t E>
constexpr int floor_log2(const wide_rational<E>& x) noexcept {
    using I    = wide_sint<E>;
    const I  n = abs(x.Num);
    const I& d = x.Den;
    int      e = bit_width_of(n) - bit_width_of(d);
    // n/d ≥ 2^e  ⇔  n ≥ d·2^e (or n·2^-e ≥ d).
    const bool ge = e >= 0 ? n >= (d << e) : (n << (-e)) >= d;
    return ge ? e : e - 1;
}

//---------------------------------------------------------------------------
// Output precision. out_bits: fractional bits that resolve Out's notch
// (2^-out_bits ≤ notch/2). mag_bits: integer bits of Out's largest value.
//---------------------------------------------------------------------------
template <insidable Out>
inline constexpr int out_bits = [] {
    if constexpr (!notched<Out>)
        return 64;
    else {
        // notch = p/q: need 2^-b ≤ p/(2q), i.e. b ≥ log2(2q/p).
        const grid_int p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
        return std::max(bit_width_of(q) - bit_width_of(p) + 2, 0);
    }
}();

template <insidable Out>
inline constexpr int mag_bits = std::max(grid_magnitude_bits<Out>, 0);

//---------------------------------------------------------------------------
// approx — a core's result: the true value lies in
// [(Value − Error)·2^-Scale, (Value + Error)·2^-Scale]. Error 0 means exact.
//---------------------------------------------------------------------------
template <std::size_t K>
struct approx {
    wide_sint<K> Value;
    int          Scale;
    umax         Error;

    // Stand-ins that decide reads like any other approx: ±2^(KMax+2) exactly,
    // past every output whose magnitude is below 2^KMax; [0, 2^-(S+1)] for a
    // value below that; and an error past every width (undecided).
    static constexpr approx beyond(bool negative, int KMax) noexcept {
        return {wide_sint<K>{negative ? -1 : 1}, -(KMax + 2), 0};
    }
    static constexpr approx below(int S) noexcept { return {wide_sint<K>{1}, S + 2, 1}; }
    static constexpr approx unresolved(int S) noexcept { return {wide_sint<K>{0}, S, ~umax{0}}; }
};

//---------------------------------------------------------------------------
// decide — the slot offset of an approx on Out's grid under Out's rounding
// mode, when both ends of its error interval round to the same slot.
//---------------------------------------------------------------------------
template <insidable Out>
inline constexpr round_mode out_rounding = rounding_of(flags_of<Out>);

// Limbs for the exact value Y/2^W of a K-limb approx, with room for the
// product with Out's notch in exact_index.
template <insidable Out, std::size_t K>
inline constexpr std::size_t decide_limbs = K + 1 + exact_limbs<Out>;

template <insidable Out, std::size_t K>
struct decision {
    bool                            Decided;
    wide_sint<decide_limbs<Out, K>> Index; // Out's slot offset (may be out of range)
};

// Y·2^-W as an exact value in KR limbs.
template <std::size_t KR, std::size_t K>
constexpr wide_rational<KR> scaled_rational(const wide_sint<K>& y, int W) noexcept {
    using I = wide_sint<KR>;
    if (W < 0)
        return {I{y} << (-W), I{1}}; // a scale past the units: Y·2^|W|
    return {I{y}, I{1} << W};
}

// The fast index: a notch p/q with p and q in 64 bits and a scale S ≥ 1.
// value/notch = Y·q/(p·2^S): split Y·q = A·2^S + R, then A = B·p + r with
// one-limb divisions; the rounding reads r and R exactly (see below).
template <insidable Out>
inline constexpr bool notch_fits64 = [] {
    if constexpr (!notched<Out>)
        return false;
    else {
        const grid_int p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
        return p.fits_limb() && q.fits_limb();
    }
}();

// v / d for a small positive d, truncating toward zero (series terms).
template <std::size_t K>
constexpr wide_sint<K> div_small(const wide_sint<K>& v, umax d) noexcept {
    const bool         neg = v.negative();
    const wide_sint<K> q{divmod_small(wide_uint<K>{neg ? -v : v}, d).Quotient};
    return neg ? -q : q;
}

// The rounding step of fast_index: the magnitude's quotient b by p, its
// remainder r, and how the shifted-out part R compares with half a unit
// (−1 below, 0 equal, 1 above; R == 0 known separately).
template <round_mode M>
constexpr bool round_up(bool neg, umax b0, umax r, umax p, bool low_zero, int low_vs_half) noexcept {
    remainder_class c = (r == 0 && low_zero) ? remainder_class::zero : remainder_class::below_half;
    if constexpr (M == round_mode::nearest || M == round_mode::half_even) {
        const umax d = p - r; // r + R against p/2, R the shifted-out part
        if (r > d || (r == d && !low_zero))
            c = remainder_class::above_half;
        else if (r == d)
            c = remainder_class::half;
        else if (r + 1 == d)
            c = low_vs_half < 0    ? remainder_class::below_half
                : low_vs_half == 0 ? remainder_class::half
                                   : remainder_class::above_half;
    }
    return rounds_away(M, neg, c, (b0 & 1u) != 0);
}

// Out's slot offset of y·2^-S (S ≥ 1) rounded by M, as value-index rounding
// (the sign rules of rounded_div).
template <insidable Out, round_mode M, std::size_t K>
constexpr wide_sint<K + 2> fast_index(const wide_sint<K>& y, int S) noexcept {
    // On limb arrays: |y|·q, the part above 2^S divided by p, and the part
    // below read as the half-unit bit (bit S−1) plus a sticky bit for the rest.
    using J                   = wide_sint<K + 2>;
    constexpr std::size_t N   = K + 1;
    constexpr umax        p   = static_cast<umax>(wide_numerator(notch_of<Out>));
    constexpr umax        q   = static_cast<umax>(wide_denominator(notch_of<Out>));
    const bool            neg = y.negative();
    const wide_uint<K>    mag{neg ? -y : y};
    umax                  a[N];
    a[K]     = limb::mul_1(a, mag.Word, K, q);
    auto bit = [&](int i) -> bool {
        const std::size_t w = static_cast<std::size_t>(i / 64);
        return w < N && ((a[w] >> (i % 64)) & 1u) != 0;
    };
    // Any bit set below position n?
    auto sticky = [&](int n) -> bool {
        const std::size_t full = static_cast<std::size_t>(n / 64);
        for (std::size_t w = 0; w < full && w < N; ++w)
            if (a[w] != 0)
                return true;
        const int rest = n % 64;
        return full < N && rest != 0 && (a[full] & ((umax{1} << rest) - 1)) != 0;
    };
    const bool half_bit = bit(S - 1), below_half = sticky(S - 1);
    const bool low_zero = !half_bit && !below_half;
    const int  vs_half  = !half_bit ? -1 : below_half ? 1 : 0;
    // hi = a >> S
    umax hi[N];
    limb::shift_right(hi, N, a, N, S);
    umax r = 0;
    if constexpr (p != 1)
        r = limb::div_1(hi, hi, N, p);
    J m{0};
    std::copy_n(hi, N, m.Word);
    if (round_up<M>(neg, hi[0], r, p, low_zero, vs_half))
        m += J{1};
    constexpr J base = static_cast<J>(slot_base<Out>);
    return (neg ? -m : m) - base;
}

// The one-pass decision, for a notch p/q in 64 bits, a scale S ≥ 1 and an
// error e > 0. On the magnitude u = |y|·q (plus half a slot, p·2^(S−1),
// for the nearest modes) the slot cells are [j·D, (j+1)·D), D = p·2^S.
// When both ends u ∓ e·q lie strictly inside one cell, every value of the
// interval rounds to the same slot under every rule, ties included, so
// this is the slot fast_index gives at each end: from one product, two
// shifts and one short division by p. Otherwise false, and decide asks
// fast_index at both ends.
// (Its limb loops stay written out: on limb::mul_1 / shift_right / div_1,
// shared with fast_index, the math_integer workload grows by about 10%.)
template <insidable Out, round_mode M, std::size_t K>
constexpr bool decide_fast(const wide_sint<K>& y, umax e, int S, wide_sint<K + 2>& index) noexcept {
    using J                       = wide_sint<K + 2>;
    constexpr std::size_t N       = K + 1; // |y|·q fits K + 1 limbs
    constexpr umax        p       = static_cast<umax>(wide_numerator(notch_of<Out>));
    constexpr umax        q       = static_cast<umax>(wide_denominator(notch_of<Out>));
    constexpr bool        nearest = M == round_mode::nearest || M == round_mode::half_even;
    const bool            neg     = y.negative();
    const wide_uint<K>    mag{neg ? -y : y};
    umax                  u[N]{};
    {
        umax carry = 0;
        for (std::size_t i = 0; i < K; ++i) {
            const limb::pair<umax> t = limb::mul(mag.Word[i], q);
            umax                   c = 0;
            u[i]                     = limb::add_carry(t.Lo, carry, c);
            carry                    = t.Hi + c;
        }
        u[K] = carry;
    }
    if constexpr (nearest) // + p·2^(S−1)
    {
        const std::size_t w = static_cast<std::size_t>((S - 1) / 64);
        const int         b = (S - 1) % 64;
        if (w >= N)
            return false;
        umax carry = 0;
        u[w]       = limb::add_carry(u[w], p << b, carry);
        if (w + 1 < N)
            u[w + 1] = limb::add_carry(u[w + 1], b == 0 ? 0 : p >> (64 - b), carry);
        for (std::size_t i = w + 2; i < N; ++i)
            u[i] = limb::add_carry(u[i], umax{0}, carry);
        if (carry != 0)
            return false;
    }
    // The ends u − E and u + E, E = e·q.
    const limb::pair<umax> E = limb::mul(e, q);
    umax                   lo[N], hi[N];
    {
        umax borrow = 0, carry = 0;
        for (std::size_t i = 0; i < N; ++i) {
            const umax ei = i == 0 ? E.Lo : i == 1 ? E.Hi : 0;
            lo[i]         = limb::sub_borrow(u[i], ei, borrow);
            hi[i]         = limb::add_carry(u[i], ei, carry);
        }
        if (borrow != 0 || carry != 0)
            return false; // near 0, or past the limbs
    }
    // Shifted down by S: the cell index times p, plus the offset within.
    const std::size_t ws          = static_cast<std::size_t>(S / 64);
    const int         bs          = S % 64;
    bool              low_nonzero = false; // lo below 2^S
    for (std::size_t i = 0; i < ws && i < N; ++i)
        low_nonzero = low_nonzero || lo[i] != 0;
    if (ws < N && bs != 0)
        low_nonzero = low_nonzero || (lo[ws] & ((umax{1} << bs) - 1)) != 0;
    umax hl[N]{}, hh[N]{};
    for (std::size_t i = 0; i + ws < N; ++i) {
        const umax l0 = lo[i + ws], l1 = i + ws + 1 < N ? lo[i + ws + 1] : 0;
        const umax h0 = hi[i + ws], h1 = i + ws + 1 < N ? hi[i + ws + 1] : 0;
        hl[i] = bs == 0 ? l0 : (l0 >> bs) | (l1 << (64 - bs));
        hh[i] = bs == 0 ? h0 : (h0 >> bs) | (h1 << (64 - bs));
    }
    // δ = hh − hl, one limb at most.
    umax delta;
    {
        umax borrow = 0, d[N];
        for (std::size_t i = 0; i < N; ++i)
            d[i] = limb::sub_borrow(hh[i], hl[i], borrow);
        for (std::size_t i = 1; i < N; ++i)
            if (d[i] != 0)
                return false;
        delta = d[0];
    }
    // B = ⌊hl/p⌋, r = hl mod p: same cell when r + δ < p; strictly inside
    // when lo is not on the cell's lower edge.
    umax r = 0;
    if constexpr (p != 1)
        for (std::size_t i = N; i-- > 0;) {
            const limb::pair<umax> d = limb::div(r, hl[i], p);
            hl[i]                    = d.Hi;
            r                        = d.Lo;
        }
    if (!(delta < p - r))
        return false;
    if (r == 0 && !low_nonzero)
        return false;
    // Strictly inside: ⌊u/D⌋ = B and ⌈u/D⌉ = B + 1. Rounding the magnitude
    // up is ceil for a positive value and floor for a negative one. Then
    // the offset ±B − base, on the limbs.
    umax up = 0;
    if constexpr (!nearest && M != round_mode::trunc)
        up = (M == round_mode::ceil) != neg ? 1 : 0;
    constexpr J base  = static_cast<J>(slot_base<Out>);
    umax        carry = up, sign_borrow = 0, borrow = 0;
    for (std::size_t i = 0; i < N + 1; ++i) {
        umax w = limb::add_carry(i < N ? hl[i] : umax{0}, umax{0}, carry); // B (+1)
        if (neg)
            w = limb::sub_borrow(umax{0}, w, sign_borrow);
        index.Word[i] = limb::sub_borrow(w, static_cast<umax>(base.Word[i]), borrow);
    }
    return true;
}

template <insidable Out, std::size_t K>
constexpr decision<Out, K> decide(const approx<K>& a) noexcept {
    constexpr round_mode M = out_rounding<Out>;
    using I                = wide_sint<decide_limbs<Out, K>>;
    const wide_sint<K> e{a.Error};
    auto               index = [&](const wide_sint<K>& y) -> I {
        if constexpr (notch_fits64<Out>)
            if (a.Scale >= 1)
                return static_cast<I>(fast_index<Out, M>(y, a.Scale));
        return static_cast<I>(exact_index<Out, M>(scaled_rational<decide_limbs<Out, K>>(y, a.Scale)).Index);
    };
    if (a.Error == 0)
        return {true, index(a.Value)};
    // An error past wide_sint<K>'s range (~umax{0}: "unresolved" at K = 1)
    // would come out negative and invert the interval: undecided.
    if constexpr (K == 1)
        if (!std::in_range<imax>(a.Error)) [[unlikely]]
            return {false, I{}};
    const I lo = index(a.Value - e), hi = index(a.Value + e);
    // Both ends past the same end of Out's range also decide: the policy
    // (clamp, or an overflow report) sees the same thing for every value there.
    // Not under wrap: it folds each value there to a different point.
    if constexpr (has_flag(flags_of<Out>, wrap))
        return {lo == hi, lo};
    constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
    const bool  below = lo.negative() && hi.negative();
    const bool  above = count < lo && count < hi;
    return {lo == hi || below || above, lo};
}

// decide_fast where it applies: the slot in K + 2 limbs, which store takes
// directly (decide's index is sized for exact_index's worst case).
template <insidable Out, std::size_t K>
constexpr bool quick_slot(const approx<K>& a, wide_sint<K + 2>& index) noexcept {
    if constexpr (notch_fits64<Out>)
        return a.Scale >= 1 && a.Error != 0 && decide_fast<Out, out_rounding<Out>>(a.Value, a.Error, a.Scale, index);
    else
        return false;
}

// The slot nearest the approx's midpoint under Out's rounding (the capped
// escalation's answer).
template <insidable Out, std::size_t K>
constexpr auto nearest_index(const approx<K>& a) noexcept {
    using I = wide_sint<decide_limbs<Out, K>>;
    return static_cast<I>(
        exact_index<Out, out_rounding<Out>>(scaled_rational<decide_limbs<Out, K>>(a.Value, a.Scale)).Index);
}

//---------------------------------------------------------------------------
// store — Out at slot offset `index`. In range it is a raw; out of range the
// grid point goes through the policy cascade (clamp, wrap or report), as any
// assignment. Policy is Out's, optionally with an errc sink.
//---------------------------------------------------------------------------
// Whether slot offset i lies in Out's range.
template <insidable Out, typename I>
constexpr bool in_slots(const I& i) noexcept {
    constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
    return !i.negative() && !(count < i);
}

template <insidable Out, std::size_t K, typename P>
constexpr Out store(const wide_sint<K>& index, P&& policy) {
    if constexpr (integer_storage<Out>)
        if (in_slots<Out>(index)) [[likely]]
            return Out::from_raw(raw_of_slot<Out>(index));
    // Past Out's range: the grid point (index + Lower/Notch)·Notch, exact,
    // through Out's assignment (its policy clamps, wraps or reports).
    constexpr std::size_t KK = exact_max<K, exact_limbs<Out>>;
    using J                  = wide_sint<KK>;
    return store_exact_into<Out>(
        wide_rational<KK>{(J{index} + static_cast<J>(slot_base<Out>)) * static_cast<J>(wide_numerator(notch_of<Out>)),
                          static_cast<J>(wide_denominator(notch_of<Out>))},
        policy);
}

template <insidable Out, std::size_t K>
constexpr Out store(const wide_sint<K>& index) {
    return store<Out>(index, make_policy<flags_of<Out>>());
}

// An exact value through Out's policy (an output without slots, or an exact
// result: rounding an exact value by the mode is correct rounding).
template <insidable Out, std::size_t K, typename P>
constexpr Out store_exact(const wide_rational<K>& v, P&& policy) {
    // At least the exact paths' minimum width (try_rational's 64-bit bounds
    // need it).
    return store_exact_into<Out>(wide_rational<exact_max<K, exact_min_limbs>>{v}, policy);
}

// An approx's midpoint as an exact value.
template <std::size_t K>
constexpr auto midpoint(const approx<K>& a) noexcept {
    return scaled_rational<exact_max<K + 1, exact_min_limbs> + 8>(a.Value, a.Scale);
}

//---------------------------------------------------------------------------
// evaluate — the Ziv driver. Core is a callable object with a member
// template `run<W>()` returning an approx within about 2^-W, and optionally
// `exact()` returning a std::optional wide_rational when the result is an
// exact rational (taken as is). It starts at W0 and doubles W while the
// result is undecided, up to Cap.
//---------------------------------------------------------------------------
// The deep step: a function whose results come close to a grid point by an
// amount the input bounds (tanh x → 1 as 2e^−2x, sin x → x by x³/6, e^x → 0)
// declares deep_extra, the bits past W0 that separate such a result from
// the point. Still undecided at the cap, the driver runs once at W0 +
// deep_extra before it falls back to the nearest slot.
inline constexpr int deep_limit = 2048; // past it a result stays the cap's guess

template <typename Core, int W0>
inline constexpr int deep_bits = [] {
    if constexpr (requires { Core::deep_extra; })
        return W0 + std::min(Core::deep_extra, deep_limit);
    else
        return 0;
}();

// Whether math::driver.IntegerOnly sends this call past the fast tiers.
constexpr bool integer_only() noexcept {
#if BEMAN_INSIDE_MATH_INSTRUMENT
    if !consteval {
        return driver.IntegerOnly.load(std::memory_order_relaxed);
    }
#endif
    return false;
}
// Counts an event on math::driver (the instrumented build, at run time).
enum class probe { integer_run, deep, guess };
template <probe C>
constexpr void note() noexcept {
#if BEMAN_INSIDE_MATH_INSTRUMENT
    if !consteval {
        auto& count = C == probe::integer_run ? driver.IntegerRuns : C == probe::deep ? driver.Deep : driver.Guesses;
        count.fetch_add(1, std::memory_order_relaxed);
    }
#endif
}

// The precision cap for a start precision W0: two doublings. A result still
// undecided at 4·W0 lies within about 2^-4W0 of a rounding boundary: the cap
// gives up on it and stores the nearest slot (counted as a guess).
constexpr int precision_cap(int W0) noexcept { return 4 * W0; }

// Runs core at precision W, doubling it while the result is undecided, up to
// Cap: on_slot(index) for a decided slot offset, else on_cap(the last approx).
template <insidable Out, int W, int Cap, typename Core, typename Slot, typename Capped>
constexpr auto escalate(const Core& core, const Slot& on_slot, const Capped& on_cap) {
    const auto a = core.template run<W>();
    if (wide_sint<limbs_of<decltype(a.Value)> + 2> j; quick_slot<Out>(a, j)) [[likely]]
        return on_slot(j);
    if (const auto d = decide<Out>(a); d.Decided)
        return on_slot(d.Index);
    if constexpr (2 * W > Cap)
        return on_cap(a);
    else
        return escalate<Out, 2 * W, Cap>(core, on_slot, on_cap);
}

template <insidable Out, int W0, typename Core, typename P>
constexpr Out evaluate(const Core& core, P&& policy) {
    if constexpr (requires { core.exact(); })
        if (const auto e = core.exact())
            return store_exact<Out>(*e, policy);
    if constexpr (!notched<Out>) {
        // A 64-bit value: keep the denominator a 64-bit power of two.
        const auto    a = core.template run<W0>();
        constexpr int A = std::max(60 - mag_bits<Out>, 1);
        if (a.Scale <= A)
            return store_exact<Out>(midpoint(a), policy);
        using I = decltype(a.Value);
        return store_exact<Out>(midpoint(approx<limbs_of<I>>{round_shift(a.Value, a.Scale - A), A, 0}), policy);
    } else {
        constexpr int Cap   = precision_cap(W0);
        const auto    slot  = [&](const auto& index) { return store<Out>(index, policy); };
        const auto    guess = [&](const auto& a) {
            note<probe::guess>();
            return store<Out>(nearest_index<Out>(a), policy);
        };
        if constexpr (constexpr int Deep = deep_bits<Core, W0>; Deep > Cap)
            return escalate<Out, W0, Cap>(core, slot, [&](const auto&) {
                note<probe::deep>();
                return escalate<Out, Deep, Deep>(core, slot, guess); // once, then the guess
            });
        else
            return escalate<Out, W0, Cap>(core, slot, guess);
    }
}

//---------------------------------------------------------------------------
// The table tier: for an input with few slots, every result is computed at
// compile time (the same driver, so the same correctly rounded slots) and a
// call is one load. Only when every result lands inside Out's range — a
// result the policy would clamp, wrap or report keeps the computed path, as
// does one that needs the deep step (too costly per slot at compile time) or
// stays undecided at the cap (so that the run-time counters see it).
// BEMAN_INSIDE_MATH_TABLE_SLOTS sets the largest input (0 turns tables off).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_TABLE_SLOTS
    #define BEMAN_INSIDE_MATH_TABLE_SLOTS 256
#endif

// A slot offset i of Out, or −1 when it falls outside Out's range.
template <insidable Out, typename I>
constexpr imax slot_in_range(const I& i) {
    return in_slots<Out>(i) ? static_cast<imax>(i) : imax{-1};
}

// Out's slot offset for core's result (escalating as evaluate does), or
// −1 when it falls outside Out's range or needs the deep step (too costly per
// slot at compile time) or the guess.
template <insidable Out, int W0, typename Core>
constexpr imax slot_of(const Core& core) {
    if constexpr (requires { core.exact(); })
        if (const auto e = core.exact())
            return slot_in_range<Out>(exact_index<Out, out_rounding<Out>>(*e).Index);
    return escalate<Out, W0, precision_cap(W0)>(
        core, [](const auto& index) { return slot_in_range<Out>(index); }, [](const auto&) { return imax{-1}; });
}

template <insidable In>
inline constexpr bool table_input =
    !wide_valued<In> && notched<In> && grid_of<In>.slot_count() < grid_int{BEMAN_INSIDE_MATH_TABLE_SLOTS};

// Outputs whose raw a table can hold: an integer index or value.
template <insidable Out>
inline constexpr bool table_output = notched<Out> && !wide_valued<Out>;

// The slot offset of an input value (0 … slot count).
template <insidable In>
constexpr std::size_t offset_of(const In& x) noexcept {
    return static_cast<std::size_t>(value_index<imax>(x) - static_cast<imax>(slot_base<In>));
}

// T's grid point at slot offset k (a notched, integer raw).
template <insidable T>
constexpr T at_slot(umax k) noexcept {
    return T::from_raw(raw_from_offset<T>(k));
}

// Out's slot for In's slot I, or −1 past Out's range: one constant
// expression per entry, so each has the compiler's whole evaluation budget.
template <insidable Out, insidable In, int W0, auto MakeCore, std::size_t I>
inline constexpr imax table_slot = slot_of<Out, W0>(MakeCore(at_slot<In>(I)));

// Out's raw for slot offset I (a table output is an integer raw).
template <insidable Out>
constexpr raw_t<Out> table_raw(imax i) noexcept {
    return raw_from_offset<Out>(static_cast<umax>(i));
}

// Out's raws for every In slot; Valid when all lie in Out's range. MakeCore
// builds the core from an In value.
template <insidable Out, insidable In, int W0, auto MakeCore>
struct result_table {
    static constexpr std::size_t N = static_cast<std::size_t>(static_cast<imax>(grid_of<In>.slot_count())) + 1;
    struct data {
        std::array<raw_t<Out>, N> Raw;
        bool                      Valid;
    };
    // Whether the slots from I on all lie in Out's range, stopping at the
    // first that does not (a table that cannot be built computes no more).
    template <std::size_t I = 0>
    static consteval bool valid_from() {
        if constexpr (I == N)
            return true;
        else if constexpr (table_slot<Out, In, W0, MakeCore, I> < 0)
            return false;
        else
            return valid_from<I + 1>();
    }
    static constexpr data Table = []<std::size_t... I>(std::index_sequence<I...>) {
        if constexpr (!valid_from())
            return data{{}, false};
        else
            return data{{table_raw<Out>(table_slot<Out, In, W0, MakeCore, I>)...}, true};
    }(std::make_index_sequence<N>{});
};

template <insidable Out, int W0, typename Core>
constexpr Out evaluate(const Core& core) {
    return evaluate<Out, W0>(core, make_policy<flags_of<Out>>());
}

// The checked form: a result Out's policy reports (out of range without
// clamp) comes back as its errc instead.
template <insidable Out, int W0, typename Core>
constexpr std::expected<Out, errc> evaluate_checked(const Core& core) {
    return try_store<Out, flags_of<Out>>([&](Out& out, auto&& policy) { out = evaluate<Out, W0>(core, policy); });
}

//---------------------------------------------------------------------------
// Constants at any precision, computed at compile time with wide integer
// series. Each is within 1 unit 2^-W of the true value.
//---------------------------------------------------------------------------
// Guard bits for a series of about W/2 truncated terms.
constexpr int series_guard(int W) noexcept { return std::bit_width(static_cast<unsigned>(W)) + 4; }

// Σ_k s^k / ((2k+1)·n^(2k+1)) at scale 2^G, with s = +1 (atanh(1/n)) or
// −1 (atan(1/n)). Each term truncates once (p) and once (p/(2k+1)).
template <std::size_t K>
constexpr wide_sint<K> inverse_series(int n, int sign, int G) noexcept {
    using I = wide_sint<K>;
    I p     = div_small(I{1} << G, static_cast<umax>(n)); // 2^G / n^(2k+1)
    I sum{0};
    for (int k = 0; !p.is_zero(); ++k) {
        const I t = div_small(p, static_cast<umax>(2 * k + 1));
        sum       = (k % 2 != 0 && sign < 0) ? sum - t : sum + t;
        p         = div_small(p, static_cast<umax>(n) * static_cast<umax>(n));
    }
    return sum;
}

// ln 2 = 2·atanh(1/3).
template <int W>
inline constexpr fixed_t<W + 2> ln2_q = [] {
    constexpr int G = W + series_guard(W);
    using I         = fixed_t<G + 4>;
    const I v       = I{2} * inverse_series<limbs_of<I>>(3, +1, G);
    return static_cast<fixed_t<W + 2>>(round_shift(v, G - W));
}();

// π = 16·atan(1/5) − 4·atan(1/239) (Machin).
template <int W>
inline constexpr fixed_t<W + 3> pi_q = [] {
    constexpr int G = W + series_guard(W) + 4;
    using I         = fixed_t<G + 6>;
    const I v       = I{16} * inverse_series<limbs_of<I>>(5, -1, G) - I{4} * inverse_series<limbs_of<I>>(239, -1, G);
    return static_cast<fixed_t<W + 3>>(round_shift(v, G - W));
}();

// ln 10 = 3·ln 2 + ln(5/4) = 6·atanh(1/3) + 2·atanh(1/9).
template <int W>
inline constexpr fixed_t<W + 4> ln10_q = [] {
    constexpr int G = W + series_guard(W) + 2;
    using I         = fixed_t<G + 6>;
    const I v       = I{6} * inverse_series<limbs_of<I>>(3, +1, G) + I{2} * inverse_series<limbs_of<I>>(9, +1, G);
    return static_cast<fixed_t<W + 4>>(round_shift(v, G - W));
}();

//---------------------------------------------------------------------------
// Exact integer helpers.
//---------------------------------------------------------------------------
// ⌊√t⌋ for one limb: Newton from above.
constexpr umax isqrt64(umax t) noexcept {
    if (t < 2)
        return t;
    umax x = umax{1} << ((std::bit_width(t) + 1) / 2);
    for (;;) {
        const umax y = (x + t / x) >> 1;
        if (y >= x)
            return x;
        x = y;
    }
}
// ⌊∛t⌋ digit by digit, no division (Hacker's Delight 11-2): each step
// brings down three bits and appends one bit to the root.
constexpr umax icbrt64(umax t) noexcept {
    umax y = 0;
    for (int s = 63; s >= 0; s -= 3) {
        y <<= 1;
        const umax b = 3 * y * (y + 1) + 1;
        if ((t >> s) >= b) {
            t -= b << s;
            y += 1;
        }
    }
    return y;
}

// The N-th root (N = 2, 3) helpers: the 64-bit root, and a shift that keeps
// `keep` top bits and is a multiple of N (so the root shifts back by sh / N).
template <int N>
constexpr umax iroot64(umax t) noexcept {
    if constexpr (N == 2)
        return isqrt64(t);
    else
        return icbrt64(t);
}
template <int N>
constexpr int iroot_shift(int bw, int keep) noexcept {
    return N == 2 ? (bw - keep + 1) & ~1 : (bw - keep + 2) / 3 * 3;
}

#if BEMAN_INSIDE_HAS_INT128
// ⌊√t⌋ / ⌊∛t⌋ for two limbs: Newton from above on 128-bit words, seeded from
// the root of the top 62 (√) or 63 (∛) bits; each quotient fits one limb.
template <int N>
constexpr umax iroot128(unsigned __int128 t) noexcept {
    if ((t >> 64) == 0)
        return iroot64<N>(static_cast<umax>(t));
    const int         bw = 128 - std::countl_zero(static_cast<umax>(t >> 64));
    const int         sh = iroot_shift<N>(bw, N == 2 ? 62 : 63);
    unsigned __int128 x  = static_cast<unsigned __int128>(iroot64<N>(static_cast<umax>(t >> sh)) + 1) << (sh / N);
    for (;;) {
        unsigned __int128 y;
        if constexpr (N == 2)
            y = (x + t / x) >> 1;
        else
            y = (2 * x + t / (x * x)) / 3;
        if (!(y < x))
            return static_cast<umax>(x);
        x = y;
    }
}

#endif

// ⌊√n⌋ (N = 2) or ⌊∛n⌋ (N = 3) for n ≥ 0. Two limbs or fewer directly;
// wider, the root of the top 126 bits (a shift by a multiple of N) seeds
// Newton from above with at least 61 (√) or 40 (∛) correct bits, each step
// doubling them; once they cover the root, powers correct the last unit
// (Newton from above never lands below the floor root).
template <int N, std::size_t K>
constexpr wide_sint<K> iroot(const wide_sint<K>& n) noexcept {
    using I = wide_sint<K>;
    if (n.is_zero())
        return n;
    const int bw = bit_width_of(n);
    if (bw <= 64)
        return I{iroot64<N>(static_cast<umax>(n))};
    I   x{};
    int seed = 0;
#if !BEMAN_INSIDE_HAS_INT128
    // (No 128-bit integer: the root of the top 61 to 63 bits seeds Newton
    // from above with about 30 (√) or 20 (∛) correct bits.)
    const int sh = iroot_shift<N>(bw, N == 2 ? 62 : 63);
    x            = (I{iroot64<N>(static_cast<umax>(n >> sh))} + I{1}) << (sh / N);
    seed         = N == 2 ? 30 : 20;
#else
    if constexpr (K < 2)
        return I{iroot64<N>(static_cast<umax>(n))};
    else {
        const int               sh  = bw > 126 ? iroot_shift<N>(bw, 126) : 0;
        const I                 top = n >> sh;
        const unsigned __int128 t =
            (static_cast<unsigned __int128>(static_cast<umax>(top.Word[1])) << 64) | static_cast<umax>(top.Word[0]);
        const umax r = iroot128<N>(t);
        if (sh == 0)
            return I{r};
        x    = (I{r} + I{1}) << (sh / N); // ≥ the root, within 2^-61 (√) or 2^-40 (∛) of it
        seed = N == 2 ? 61 : 40;
    }
#endif
    for (int bits = seed; bits < bw / N + 2; bits *= 2) {
        if constexpr (N == 2)
            x = (x + n / x) >> 1;
        else
            x = (I{2} * x + n / (x * x)) / I{3};
    }
    if constexpr (N == 2)
        while (n < x * x)
            x -= I{1};
    else
        while (n < x * x * x)
            x -= I{1};
    return x;
}
template <std::size_t K>
constexpr wide_sint<K> isqrt(const wide_sint<K>& n) noexcept {
    return iroot<2>(n);
}
template <std::size_t K>
constexpr wide_sint<K> icbrt(const wide_sint<K>& n) noexcept {
    return iroot<3>(n);
}

template <std::size_t K>
constexpr bool is_zero(const wide_rational<K>& f) noexcept {
    return f.Num.is_zero();
}
template <std::size_t K>
constexpr bool is_one(const wide_rational<K>& f) noexcept {
    return f.Num == f.Den;
}
template <std::size_t K>
constexpr wide_rational<K> abs(const wide_rational<K>& f) noexcept {
    return {abs(f.Num), f.Den};
}
// 1/f for f != 0.
template <std::size_t K>
constexpr wide_rational<K> inverse(const wide_rational<K>& f) noexcept {
    return f.Num.negative() ? wide_rational<K>{-f.Den, -f.Num} : wide_rational<K>{f.Den, f.Num};
}

//---------------------------------------------------------------------------
// Polynomial kernels on Q.S values: Taylor polynomials in Horner form with
// coefficients rounded to scale S at compile time (no divisions at
// runtime). Each returns the value and an error bound in units 2^-S: the
// coefficients are within ½ unit, each Horner step truncates once, and the
// argument's own error passes through the derivative; the bounds below
// round those sums up generously.
//---------------------------------------------------------------------------
template <std::size_t K>
struct fx {
    wide_sint<K> Value;
    umax         Error;
};

// A positive double as mantissa·2^exponent, so a term bound far below the
// double range (2^-S for S in the thousands) can still be compared.
struct scaled_bound {
    double         Mant = 1;
    int            Exp  = 0;
    constexpr void mul(double f) noexcept {
        int e = 0;
        Mant  = ::beman::inside::detail::frexp(Mant * f, &e);
        Exp += e;
    }
    constexpr bool below(int e) const noexcept { return Exp < e; } // < 2^e
};

// Terms for Σ_k x^(Step·k + Off)/Den(k) with |x| ≤ R to reach 2^-(S+3):
// the first omitted term is below that, and so is the rest (each term is
// at most half the one before it).
enum class series { exp, sin, cos, atanh, sinh, cosh, log1p };

template <series Kind, int S>
inline constexpr int series_terms = [] {
    constexpr double R = (Kind == series::exp || Kind == series::sinh || Kind == series::cosh) ? 0.36
                         : Kind == series::atanh                                               ? 0.18
                         : Kind == series::log1p                                               ? 0.025
                                                                                               : 0.8;
    // The first term left out, of index k = n + 1, is R^m/m! (R^m/m for
    // atanh and log1p).
    constexpr bool factorial = Kind != series::atanh && Kind != series::log1p;
    for (int n = 0;; ++n) {
        const int    k = n + 1;
        const int    m = Kind == series::exp                             ? k
                         : Kind == series::log1p                         ? k + 1
                         : (Kind == series::cos || Kind == series::cosh) ? 2 * k
                                                                         : 2 * k + 1;
        scaled_bound u;
        for (int i = 1; i <= m; ++i)
            u.mul(factorial ? R / i : R);
        if constexpr (!factorial)
            u.mul(1.0 / m);
        if (u.below(-(S + 3)))
            return n;
    }
}();

// Coefficients at scale S, within ½ unit: exp 1/k!, sin (−1)^k/(2k+1)!,
// cos (−1)^k/(2k)!, sinh 1/(2k+1)!, cosh 1/(2k)!, atanh 1/(2k+1),
// atan (−1)^k/(2k+1), log1p (−1)^k/(k+1).
template <series Kind, int S, bool Alternate = false>
inline constexpr auto series_coef = [] {
    constexpr int N = series_terms<Kind, S>;
    constexpr int G = 16; // guard bits for the running quotient
    using I         = fixed_t<S + G + 2>;
    std::array<fixed_t<S + 2>, N + 1> c{};
    I                                 v = I{1} << (S + G);
    for (int k = 0; k <= N; ++k) {
        if constexpr (Kind == series::atanh)
            v = div_small(I{1} << (S + G), static_cast<umax>(2 * k + 1));
        else if constexpr (Kind == series::log1p)
            v = div_small(I{1} << (S + G), static_cast<umax>(k + 1));
        else if (k > 0) {
            const int d = Kind == series::exp                             ? k
                          : (Kind == series::sin || Kind == series::sinh) ? (2 * k) * (2 * k + 1)
                                                                          : (2 * k - 1) * (2 * k);
            v           = div_small(v, static_cast<umax>(d));
        }
        const bool neg =
            (Kind == series::sin || Kind == series::cos || Kind == series::log1p || Alternate) && k % 2 != 0;
        const auto r                   = static_cast<fixed_t<S + 2>>(round_shift(v, G));
        c[static_cast<std::size_t>(k)] = neg ? -r : r;
    }
    return c;
}();

// Σ c_k x^k by Horner at scale S. (A loop, not std::accumulate: that copies
// the accumulator every step, +8% on the math_integer workload.)
template <series Kind, int S, bool Alternate, std::size_t K>
constexpr wide_sint<K> horner(const wide_sint<K>& x) noexcept {
    constexpr auto& c = series_coef<Kind, S, Alternate>;
    wide_sint<K>    p = static_cast<wide_sint<K>>(c[c.size() - 1]);
    for (std::size_t k = c.size() - 1; k-- > 0;)
        p = static_cast<wide_sint<K>>(c[k]) + mul_q(p, x, S);
    return p;
}

// e^r for |r| ≤ 0.36, r within dr units.
template <int S, std::size_t K>
constexpr fx<K> exp_series(const wide_sint<K>& r, umax dr) noexcept {
    return {horner<series::exp, S, false>(r), 2 * dr + 8};
}

// sin r and cos r for |r| ≤ 0.8, r within dr units.
template <int S, std::size_t K>
constexpr fx<K> sin_series(const wide_sint<K>& r, umax dr) noexcept {
    const wide_sint<K> z = mul_q(r, r, S);
    return {mul_q(r, horner<series::sin, S, false>(z), S), 3 * dr + 12};
}
template <int S, std::size_t K>
constexpr fx<K> cos_series(const wide_sint<K>& r, umax dr) noexcept {
    return {horner<series::cos, S, false>(mul_q(r, r, S)), 4 * dr + 12};
}

// log(j/32) and 32/j for j = 22 … 46 at scale S (within 1 and ½ unit).
inline constexpr int log_tab_lo = 22, log_tab_hi = 46;

template <int S>
inline constexpr auto log_table = [] {
    constexpr int T = S + 16;
    using I         = fixed_t<T + 8>;
    struct entry {
        fixed_t<S + 2> Log;
        fixed_t<S + 8> Inv;
    };
    std::array<entry, log_tab_hi - log_tab_lo + 1> t{};
    for (int j = log_tab_lo; j <= log_tab_hi; ++j) {
        // log(j/32) = 2·atanh(z), z = (j − 32)/(j + 32), |z| ≤ 0.19.
        const I z  = div_small(I{j - 32} << T, static_cast<umax>(j + 32));
        const I z2 = mul_q(z, z, T);
        I       p  = z, sum{0};
        for (int k = 0; !p.is_zero(); ++k) {
            sum += div_small(p, static_cast<umax>(2 * k + 1));
            p = mul_q(p, z2, T);
        }
        auto& e = t[static_cast<std::size_t>(j - log_tab_lo)];
        e.Log   = static_cast<fixed_t<S + 2>>(round_shift(sum << 1, T - S));
        e.Inv   = static_cast<fixed_t<S + 8>>(round_shift(div_small(I{32} << T, static_cast<umax>(j)), T - S));
    }
    return t;
}();

// log m for m in [0.7, 1.42], m within dm units, with no division: j =
// round(32m), u = m·(32/j) − 1 (|u| ≤ 1/44), log m = log(j/32) + log(1 + u).
template <int S, std::size_t K>
constexpr fx<K> log_series(const wide_sint<K>& m, umax dm) noexcept {
    using I       = wide_sint<K>;
    int j         = static_cast<int>(static_cast<imax>(round_shift(m, S - 5)));
    j             = std::clamp(j, log_tab_lo, log_tab_hi);
    const auto& e = log_table<S>[static_cast<std::size_t>(j - log_tab_lo)];
    const I     u = mul_q(m, static_cast<I>(e.Inv), S) - one_q<K>(S); // within 1.5·dm + 2
    const I     l = mul_q(u, horner<series::log1p, S, false>(u), S);
    return {static_cast<I>(e.Log) + l, 3 * dm + 12};
}

// atan u for |u| ≤ 1/16 (the atanh table, sized for 0.18, with
// alternating signs), u within du units.
template <int S, std::size_t K>
constexpr fx<K> atan_series(const wide_sint<K>& u, umax du) noexcept {
    return {mul_q(u, horner<series::atanh, S, true>(mul_q(u, u, S)), S), du + 6};
}

// √a for a ≥ 0 at scale S: ⌊√(a·2^S)⌋; an a within da units gives a root
// within da/2 + 1 units (a ≥ 1/4).
template <std::size_t K>
constexpr wide_sint<K> sqrt_q(const wide_sint<K>& a, int S) noexcept {
    using D = twice_t<K>;
    return static_cast<wide_sint<K>>(isqrt(D{a} << S));
}

// 1/√m for m in [1/4, 4) at scale S, within 6 units, by multiplications
// only: a 29-bit seed from the top bits (isqrt64), then Newton steps
// y ← y·(3 − m·y²)/2, each doubling the correct bits.
template <int S, std::size_t K>
constexpr wide_sint<K> rsqrt_q(const wide_sint<K>& m) noexcept {
    using R = fixed_t<S + 34>;
    const R       mr{m};
    const umax    mm    = static_cast<umax>(S >= 60 ? mr >> (S - 60) : mr << (60 - S)); // m·2^60 < 2^62
    const umax    sd    = isqrt64(mm);                                                  // √m·2^30, ≥ 2^29
    R             y     = div_small(R{1} << (S + 30), sd);
    const R       three = R{3} << S;
    constexpr int steps = [] {
        int n = 0;
        for (int p = 29; p < S + 4; p *= 2)
            ++n;
        return n;
    }();
    for (int i = 0; i < steps; ++i)
        y = mul_q(y, three - mul_q(mr, mul_q(y, y, S), S), S) >> 1;
    return static_cast<wide_sint<K>>(y);
}

// atan(j/8) for j = 0 … 8 at scale S, within 1 unit: three half-angle steps
// t ← t/(1 + √(1+t²)) bring j/8 below tan(π/32), then the series with no table.
template <int S>
inline constexpr auto atan_eighths = [] {
    constexpr int T                 = S + 16;
    using I                         = fixed_t<T + 8>;
    constexpr std::size_t         K = limbs_of<I>;
    std::array<fixed_t<S + 2>, 9> a{};
    const I                       one = one_q<K>(T);
    for (int j = 0; j <= 8; ++j) {
        I t = (I{j} << T) / I{8};
        for (int i = 0; i < 3; ++i)
            t = div_q(t, one + sqrt_q(one + mul_q(t, t, T), T), T);
        // |t| ≤ tan(π/32) < 0.1; a plain alternating series.
        const I t2 = mul_q(t, t, T);
        I       p  = t, sum{0};
        for (int k = 0; !p.is_zero(); ++k) {
            const I term = div_small(p, static_cast<umax>(2 * k + 1));
            sum          = (k % 2 == 0) ? sum + term : sum - term;
            p            = mul_q(p, t2, T);
        }
        a[static_cast<std::size_t>(j)] = static_cast<fixed_t<S + 2>>(round_shift(sum << 3, T - S));
    }
    return a;
}();

// atan t for |t| ≤ 1, t within dt units: c = j/8 nearest t, then
// atan t = atan c + atan((t − c)/(1 + t·c)) with |(t − c)/(1 + t·c)| ≤ 1/16.
template <int S, std::size_t K>
constexpr fx<K> atan_fixed(const wide_sint<K>& t, umax dt) noexcept {
    using I          = wide_sint<K>;
    const I     one  = one_q<K>(S);
    const I     j    = round_shift(t, S - 3); // round(8t), in [−8, 8]
    const int   jj   = static_cast<int>(static_cast<imax>(j));
    const I     c    = j << (S - 3);                          // exact
    const I     u    = div_q(t - c, one + mul_q(t, c, S), S); // within dt + 2
    const fx<K> a    = atan_series<S>(u, dt + 2);
    const I     base = static_cast<I>(atan_eighths<S>[static_cast<std::size_t>(jj < 0 ? -jj : jj)]);
    return {(jj < 0 ? -base : base) + a.Value, a.Error + 1};
}

// ⌊√2·2^S⌋: a value m at scale S is past √2 exactly when m exceeds it.
template <int S>
inline constexpr fixed_t<S + 2> sqrt2_q = static_cast<fixed_t<S + 2>>(isqrt(fixed_t<2 * S + 4>{2} << (2 * S)));

// log a for a ≥ 1 at scale S (so b ≥ 0), a within da units: a = m·2^b with
// m in [0.7, 1.42], log a = log m + b·ln 2.
template <int S, std::size_t K>
constexpr fx<K> log_fixed(const wide_sint<K>& a, umax da) noexcept {
    using I = wide_sint<K>;
    int b   = bit_width_of(a) - 1 - S; // 2^b ≤ a/2^S < 2^(b+1)
    I   m   = b >= 0 ? a >> b : a << (-b);
    if (m > static_cast<I>(sqrt2_q<S>)) {
        ++b;
        m = b >= 0 ? a >> b : a << (-b);
    }
    const umax  dm = shr_bound(da, b) + 1;
    const fx<K> l  = log_series<S>(m, dm);
    return {l.Value + I{b} * static_cast<I>(ln2_q<S>), l.Error + static_cast<umax>(b < 0 ? -b : b) + 1};
}

// 1/ln 2 and 2/π at scale S (within 1 unit), for range reduction by a
// product.
template <int S, std::size_t K>
constexpr fixed_t<S + 2> inverse_q(const wide_sint<K>& c, int up = 0) noexcept { // 2^up/c, c at scale S + 16
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 16 + up)) / static_cast<I>(c));
}
template <int S>
inline constexpr fixed_t<S + 2> log2e_q = inverse_q<S>(ln2_q<S + 16>);
template <int S>
inline constexpr fixed_t<S + 2> log10e_q = inverse_q<S>(ln10_q<S + 16>);
template <int S>
inline constexpr fixed_t<S + 2> two_over_pi_q = inverse_q<S>(pi_q<S + 16>, 1);

// e^t for t at scale S, within dt units: e^r·2^k for t = k·ln 2 + r, as an
// approx at scale S − k. k comes from t·(1/ln 2), so |r| ≤ ln 2/2 plus a
// hair (the series covers 0.36). Past KMax the value lies above 2^(KMax+1)
// (approx::beyond); below 2^-(S+1), approx::below.
template <int S, std::size_t K>
constexpr approx<K> exp_fixed(const wide_sint<K>& t, umax dt, int KMax) noexcept {
    using I   = wide_sint<K>;
    const I k = round_shift(mul_q(t, static_cast<I>(log2e_q<S>), S), S);
    if (I{KMax} < k)
        return approx<K>::beyond(false, KMax);
    if (k < I{-(S + 2)})
        return approx<K>::below(S);
    const fx<K> e  = exp_series<S>(t - k * static_cast<I>(ln2_q<S>), 0);
    const int   kk = static_cast<int>(static_cast<imax>(k));
    const umax  ak = static_cast<umax>(kk < 0 ? -kk : kk);
    return {e.Value, S - kk, e.Error + 2 * (dt + ak + 1)};
}
} // namespace beman::inside::math::detail::ax



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


// ======================================================================
//  beman/inside/detail/math_fp.hpp
// ======================================================================
// The double kernels of the math engine's double tier (detail/math_entry.hpp):
// a small libm in `double` — Taylor polynomials in Horner form with explicit
// std::fma, Cody-Waite range reduction, and only std::fma / sqrt / nearbyint
// from <cmath>.
//
// A kernel takes a target T in bits: its polynomials get the fewest terms
// whose truncation stays below 2^-T relative, and its error bound for that
// size is proved at compile time from the coefficients and argument range —
// the first omitted term, each Horner step's rounding, the constants' and the
// reduction's roundings. These are first-order bounds of IEEE round-to-nearest
// (each operation within U = unit_roundoff = 2^-53, relatively); up() widens each by 2^-30 of
// itself for the second-order terms and the bound arithmetic's roundings.
//---------------------------------------------------------------------------



#ifndef BEMAN_INSIDE_MATH_NO_FP // ===== FP engine present (needs <cmath> + an FPU) =====

    #include <algorithm>
    #include <array>
    #include <cmath> // std::fma, std::sqrt, std::nearbyint ONLY
    #include <concepts>
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

inline constexpr double half_pi_lo  = 0x1.1a62633145c07p-54; // π/2 − pi_half
inline constexpr double two_over_pi = 0x1.45f306dc9c883p-1;  // 2/π
inline constexpr double ln2_hi      = 0x1.62e42fee00000p-1;  // ln2  high (33 bits)
inline constexpr double ln2_lo      = 0x1.a39ef35793c76p-33; // ln2  low
inline constexpr double log2e       = 0x1.71547652b82fep+0;  // 1/ln2
inline constexpr double ln2_full    = 0x1.62e42fefa39efp-1;  // ln2
inline constexpr double log10e      = 0x1.bcb7b1526e50ep-2;  // 1/ln10
inline constexpr double sqrt_half   = 0x1.6a09e667f3bcdp-1;  // √½
inline constexpr double pi_double   = 0x1.921fb54442d18p+1;  // π
inline constexpr double pi_half     = 0x1.921fb54442d18p+0;  // π/2
inline constexpr double pi_sixth    = 0x1.0c152382d7366p-1;  // π/6
inline constexpr double inv_sqrt3   = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
inline constexpr double tan_pi12    = 0x1.126145e9ecd56p-2;  // tan(π/12) ≈ 0.2679
inline constexpr double third       = 1.0 / 3.0;
inline constexpr double third_lo    = 0x1.5555555555555p-56; // 1/3 − third = 2^-54/3

// Under -ffp-contract=fast (GCC's default) a product feeding a later sum
// may be fused into an fma, which
// adds the exact product where an error-free transformation expects the
// rounded one. fenced() keeps an operand of an error-free sum out of that, at
// no runtime cost.
constexpr double fenced(double x) noexcept { return __builtin_assoc_barrier(x); }

// The product rounded once, which the compiler cannot fuse further:
// fma(a, b, +0) (+0 keeps it from folding back to a·b), one instruction with
// hardware FMA; without it nothing contracts.
constexpr double rounded_product(double a, double b) noexcept {
    #if BEMAN_INSIDE_HAS_FMA
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
inline constexpr double unit_roundoff  = 0x1p-53; // U in the comments
inline constexpr double unit_roundoff2 = unit_roundoff * unit_roundoff;
// Absolute slack for results near the bottom of the double range (a
// subnormal or underflowing ldexp, a square below 2^-1022): far below
// every grid the tier serves.
inline constexpr double tiny = 0x1p-500;

// How far each constant is from its value (computed offline at 300 bits,
// rounded up).
inline constexpr double half_pi_res   = 0x1p-109;       // |π/2 − Hi − Lo|      (2^-109.04)
inline constexpr double ln2_res       = 0x1p-86;        // |ln2 − Hi − Lo|      (2^-86.15)
inline constexpr double ln2_full_err  = 1.25 * 0x1p-55; // relative          (2^-54.73)
inline constexpr double log2e_err     = 1.02 * 0x1p-56; // relative          (2^-55.98)
inline constexpr double log10e_err    = 1.0 * 0x1p-55;  // relative          (2^-55.13)
inline constexpr double pi_err        = 1.11 * 0x1p-53; // absolute          (2^-52.86)
inline constexpr double pi_half_err   = 1.11 * 0x1p-54; // absolute          (2^-53.86)
inline constexpr double pi_sixth_err  = 1.0 * 0x1p-54;  // absolute          (2^-54.05)
inline constexpr double inv_sqrt3_err = 1.21 * 0x1p-55; // absolute          (2^-54.73)

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

// A polynomial's coefficients, highest degree first, each with its
// relative distance from the exact rational it stands for.
template <std::size_t N>
struct poly {
    std::array<double, N> C;
    std::array<double, N> Err;
};

// Σ_{k<N} sign(k)/den(k)·z^k: each coefficient one correctly rounded
// division of integers below 2^53, so within U of its value (exact when
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
        p.Err[N - 1 - k] = pow2 ? 0.0 : unit_roundoff;
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
// rounding adds U of it.
struct horner_bound {
    double Mag, Err;
};

template <std::size_t N>
consteval horner_bound horner_error(const poly<N>& p, double Z, double Dz) {
    double P = __builtin_fabs(p.C[0]), E = __builtin_fabs(p.C[0]) * p.Err[0];
    for (std::size_t k = 1; k < N; ++k) {
        E = E * Z * (1 + Dz) + P * Z * Dz + __builtin_fabs(p.C[k]) * p.Err[k];
        P = P * Z + __builtin_fabs(p.C[k]);
        E += unit_roundoff * (P + E);
    }
    return {P, E};
}

// The fewest terms n ≤ Max whose truncation trunc(n) is at most 2^-T.
template <std::invocable<int> F>
consteval int terms_for(int T, int Max, F trunc) {
    for (int n = 1; n < Max; ++n)
        if (trunc(n) <= pow_n(0.5, T))
            return n;
    return Max;
}

// The target of the full kernels: truncation below 2^-58, a thirty-second
// of their rounding error.
inline constexpr int full_bits = 58;

// sinh, asinh and acosh have a sharp form, which reaches 47 bits but costs
// 15–40% more, and a plain one, which reaches 46: targets up to plain_bits
// (fp_target = bits + 10) take the plain one.
inline constexpr int plain_bits = 56;

//---------------------------------------------------------------------------
// sin and cos on the reduced argument r, |r| ≤ r_trig. The reduction of
// |x| ≤ 2^20 leaves |r| ≤ π/4 + 2^-32 (x·2/π is within 2^-32.6 of exact).
//---------------------------------------------------------------------------
inline constexpr double r_trig  = 0x1.9220p-1; // 0.78540039 ≥ π/4 + 2^-30
inline constexpr double z_trig  = r_trig * r_trig;
inline constexpr double sin_min = 1 - z_trig / 6; // ≤ sin r / r
inline constexpr double cos_min = 1 - z_trig / 2 + z_trig * z_trig / 24 - z_trig * z_trig * z_trig / 720; // ≤ cos r

// sin r = r·Σ (−1)^k z^k/(2k+1)!; cos r = Σ (−1)^k z^k/(2k)!, z = r². Both
// series alternate with falling terms, so the first omitted term bounds
// the tail.
constexpr double sin_trunc(int n) { return pow_n(z_trig, n) / factorial(2 * n + 1) / sin_min; }
constexpr double cos_trunc(int n) { return pow_n(z_trig, n) / factorial(2 * n) / cos_min; }
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
    const horner_bound h = horner_error(sin_c<N>, z_trig, unit_roundoff);
    return up((h.Err + sin_trunc(N) * sin_min) / sin_min + unit_roundoff);
}();
template <int N>
inline constexpr double cos_poly_rel = [] {
    const horner_bound h = horner_error(cos_c<N>, z_trig, unit_roundoff);
    return up((h.Err + cos_trunc(N) * cos_min) / cos_min);
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
// half_pi_res: r is within U·|r| + |k|·half_pi_res of x − k·π/2.
inline double reduce_quadrant(double x, long& q, double& k) {
    k        = std::nearbyint(x * two_over_pi);
    double r = fma(-k, pi_half, x);
    r        = fma(-k, half_pi_lo, r);
    q        = static_cast<long>(k) & 3;
    return r;
}

// sin, cos and tan of |x| ≤ 2^20, sized to T bits. The reduction's U·|r|
// moves sin r by U·|r| ≤ U·|sin r|/sin_min and cos r by U·r² ≤
// U·z_trig/cos r, relatively; its |k|·half_pi_res is absolute, with
// |k| ≤ 1.2·max(1, |x|). So each value is within Rel·|v| + AbsX·max(1, |x|).
template <int T>
struct trig_k {
    static constexpr int    NS = sin_terms<T>, NC = cos_terms<T>;
    static constexpr double Rel =
        up(std::max(sin_poly_rel<NS> + unit_roundoff / sin_min, cos_poly_rel<NC> + unit_roundoff * z_trig / cos_min));
    static constexpr double AbsX = up(1.2 * half_pi_res);
    // tan = s/c or −c/s: both relative errors and the division's; the
    // reduction's U·|r| moves tan r by 2U·|r|/|sin 2r| ≤ 1.571U of it,
    // and |k|·half_pi_res by sec² = 1 + t² times itself.
    static constexpr double TanRel = up(sin_poly_rel<NS> + cos_poly_rel<NC> + unit_roundoff + 1.5710 * unit_roundoff);

    // sin x is sin, cos, −sin, −cos of r in quadrants 0…3; cos x is sin x one
    // quadrant on.
    static double quadrant(double x, long shift, double& bound) {
        long         q;
        double       k;
        const double r = reduce_quadrant(x, q, k);
        q              = (q + shift) & 3;
        const double p = q & 1 ? cos_poly<NC>(r) : sin_poly<NS>(r);
        const double v = q & 2 ? -p : p;
        bound          = Rel * __builtin_fabs(v) + AbsX * std::max(1.0, __builtin_fabs(x)) + tiny;
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
        const double mx = std::max(1.0, __builtin_fabs(x));
        bound           = TanRel * __builtin_fabs(t) + (1 + t * t) * AbsX * mx + tiny;
        return true;
    }
};

//---------------------------------------------------------------------------
// e^r on |r| ≤ r_exp: Σ r^k/k!, whose tail is at most r^n/n!·e^|r|.
//---------------------------------------------------------------------------
inline constexpr double r_exp   = 0x1.63p-2; // 0.34668 ≥ ln2/2 + 2^-40
inline constexpr double exp_max = 1.4144;    // ≥ e^r_exp
constexpr double        exp_trunc(int n) { return pow_n(r_exp, n) / factorial(n) * exp_max * exp_max; }
template <int T>
inline constexpr int exp_terms = terms_for(T, 15, exp_trunc);

template <int N>
inline constexpr poly<N> exp_c = series<N>([](int k) { return term{1.0, factorial(k)}; });

// Relative error of exp_poly at the r it is given (e^r ≥ 1/exp_max).
template <int N>
inline constexpr double exp_poly_rel = [] {
    const horner_bound h = horner_error(exp_c<N>, r_exp, 0);
    return up(h.Err * exp_max + exp_trunc(N));
}();

template <int N>
[[gnu::always_inline]] inline double exp_poly(double r) {
    return horner(r, exp_c<N>.C);
}

// sinh a = a + a·z·P(z) for 0 ≤ a < ln 2, z = a² ≤ z_sinh, P = Σ z^k/(2k+3)!.
// Relative to sinh a ≥ a: z·P's truncation (the first omitted term,
// widened by 1% for the rest), z and a·z rounding (2U of z·P), P's
// Horner error at z, and the fma's rounding.
inline constexpr double z_sinh = 0.4805; // ≥ (ln 2)² = 0.480453
constexpr double        sinh_trunc(int n) { return pow_n(z_sinh, n + 1) / factorial(2 * n + 3) * 1.01; }
template <int T>
inline constexpr int sinh_terms = terms_for(T, 9, sinh_trunc);

template <int N>
inline constexpr poly<N> sinh_c = series<N>([](int k) { return term{1.0, factorial(2 * k + 3)}; });

template <int N>
inline constexpr double sinh_poly_rel = [] {
    const horner_bound h = horner_error(sinh_c<N>, z_sinh, unit_roundoff);
    return up(z_sinh * (2 * unit_roundoff * h.Mag + h.Err) + sinh_trunc(N) + unit_roundoff);
}();

// e^x = 2^k·e^r, x = k·ln2 + r, for |x| ≤ 745 (|k| ≤ 1100) — larger |x|
// overflow to infinity or underflow to 0, both within the bound's tiny
// or failing every test downstream. r1 = x − k·Hi is exact (Hi has 33
// bits, k at most 11, and x − k·Hi is a multiple of 2^-54 below ½); the
// second fma rounds once (U·|r|, moving e^r by U·r_exp relatively) and
// Hi + Lo misses ln 2 by ln2_res (|k|·ln2_res). An optional low part of x
// adds one more rounding of r.
template <int T>
struct exp_k {
    static constexpr int    N     = exp_terms<T>;
    static constexpr double Rel   = up(exp_poly_rel<N> + unit_roundoff * r_exp + 1100 * ln2_res);
    static constexpr double RelLo = up(Rel + unit_roundoff * r_exp);

    [[gnu::always_inline]] static double reduce(double x, double& k) {
        k = std::nearbyint(x * log2e);
        return fma(-k, ln2_lo, fma(-k, ln2_hi, x));
    }
    [[gnu::always_inline]] static double value(double x) {
        double       k;
        const double r = reduce(x, k);
        return ::beman::inside::detail::scale_pow2(exp_poly<N>(r), static_cast<int>(k));
    }
    [[gnu::always_inline]] static double value(double x, double lo) {
        double       k;
        const double r = reduce(x, k) + lo;
        return ::beman::inside::detail::scale_pow2(exp_poly<N>(r), static_cast<int>(k));
    }
    static double exp(double x, double& bound) {
        const double v = value(x);
        bound          = Rel * v + tiny;
        return v;
    }

    // 2^x = 2^k·e^((x − k)·ln 2): x − k is exact (|x| < 2^52), and its
    // product with ln 2 rounds once on top of the constant's 2^-54.5.
    static constexpr double Rel2 = up(exp_poly_rel<N> + (unit_roundoff + ln2_full_err) * r_exp);
    static double           exp2(double x, double& bound) {
        const double k = std::nearbyint(x);
        const double r = (x - k) * ln2_full;
        const double v = ::beman::inside::detail::scale_pow2(exp_poly<N>(r), static_cast<int>(k));
        bound          = Rel2 * v + tiny;
        return v;
    }

    // The hyperbolics from e = e^x within Rel: cosh's two positive terms
    // keep it relative (1/e adds one rounding, the sum another);
    // tanh = (e^2x − 1)/(e^2x + 1) is within Rel + |t|·(Rel + 3U). sinh of
    // a = |x| ≥ ln 2 (ln2_full is above it) takes (e − 1/e)/2 with e ≥ 2:
    // e's Rel and 1/e's Rel + U over the difference are at most
    // Rel·coth a + U/(e² − 1) ≤ Rel·5/3 + U/3, and the difference rounds
    // once; below ln 2 the odd series, relative (sinh_poly_rel). The
    // plain form takes the difference throughout: within (Rel + U)·cosh x
    // + U·|sinh x|, and cosh ≤ |sinh| + 1.
    static constexpr int    NSh = sinh_terms<T>;
    static constexpr double SinhRel =
        up(std::max(sinh_poly_rel<NSh>, Rel * 5 / 3 + unit_roundoff / 3 + unit_roundoff));
    static constexpr double SinhPlainRel = up(Rel + 2 * unit_roundoff), SinhPlainAbs = up(Rel + unit_roundoff);
    static constexpr double CoshRel = up(Rel + 2 * unit_roundoff);
    static constexpr double TanhRel = up(Rel + 3 * unit_roundoff), TanhAbs = up(Rel);

    static double sinh(double x, double& bound) {
        if constexpr (T <= plain_bits) {
            const double e = value(x);
            const double v = (e - 1.0 / e) * 0.5;
            bound          = SinhPlainRel * __builtin_fabs(v) + SinhPlainAbs + tiny;
            return v;
        }
        const double a = x < 0 ? -x : x;
        double       m;
        if (a < ln2_full) {
            const double z = a * a;
            m              = fma(a * z, horner(z, sinh_c<NSh>.C), a);
        } else {
            const double e = value(a);
            m              = (e - 1.0 / e) * 0.5;
        }
        bound = SinhRel * m + tiny;
        return x < 0 ? -m : m;
    }
    static double cosh(double x, double& bound) {
        const double e = value(x);
        const double v = (e + 1.0 / e) * 0.5;
        bound          = CoshRel * v + tiny;
        return v;
    }
    static double tanh(double x, double& bound) {
        const double e = value(x + x);
        const double v = (e - 1.0) / (e + 1.0);
        bound          = TanhRel * __builtin_fabs(v) + TanhAbs + tiny;
        return v;
    }
};

//---------------------------------------------------------------------------
// ln x for x > 0: frexp to m ∈ [√½, √2), ln x = e·ln2 + 2·atanh f with
// f = (m − 1)/(m + 1), |f| ≤ f_max, and 2·atanh f = 2f·Σ f^2k/(2k+1),
// whose tail is at most f^2n/((2n+1)(1 − f²)) of the sum (which is ≥ 1).
//---------------------------------------------------------------------------
inline constexpr double f_max = 0.1716; // ≥ (√2 − 1)/(√2 + 1) = 0.171573
inline constexpr double z_log = f_max * f_max;
constexpr double        log_trunc(int n) { return pow_n(z_log, n) / ((2 * n + 1) * (1 - z_log)); }
template <int T>
inline constexpr int log_terms = terms_for(T, 11, log_trunc);

template <int N>
inline constexpr poly<N> log_c = series<N>([](int k) { return term{1.0, 2.0 * k + 1}; });
// log_c without its constant term, over z: Σ z^k/(2k+3).
template <int N>
inline constexpr poly<N> log_tail_c = series<N>([](int k) { return term{1.0, 2.0 * k + 3}; });

// m − 1 is exact (Sterbenz), m + 1 and the quotient round: f within 2U,
// which moves atanh f by 2U/(1 − f²) of itself. The Horner error and
// the truncation are relative (the sum is ≥ 1); 2f·p rounds once. Then
// e·Hi is exact and the two fmas round once each, relative to |v|: for
// e ≠ 0, |v| ≥ ln2/2 ≥ |2 atanh f|, and |e|·ln2_res ≤ 3·ln2_res·|v|;
// for e = 0 both fmas return 2f·p exactly.
template <int T>
struct log_k {
    static constexpr int    N    = log_terms<T>;
    static constexpr double MRel = [] {
        const horner_bound h = horner_error(log_c<N>, z_log, unit_roundoff);
        return h.Err + log_trunc(N) + unit_roundoff + 2 * unit_roundoff / (1 - z_log);
    }();
    static constexpr double Rel   = up(MRel + 2 * unit_roundoff + 3 * ln2_res);
    static constexpr double Rel2  = up(Rel + unit_roundoff + log2e_err);
    static constexpr double Rel10 = up(Rel + unit_roundoff + log10e_err);

    // x = m·2^e with m ∈ [√½, √2).
    [[gnu::always_inline]] static double mantissa(double x, int& e) {
        double m = ::beman::inside::detail::frexp(x, &e);
        if (m < sqrt_half) {
            m += m;
            --e;
        }
        return m;
    }

    [[gnu::always_inline]] static double value(double x) {
        int          e;
        const double m    = mantissa(x, e);
        const double f    = (m - 1.0) / (m + 1.0);
        const double logm = 2.0 * f * horner(f * f, log_c<N>.C);
        const double r    = fma(static_cast<double>(e), ln2_hi, logm);
        return fma(static_cast<double>(e), ln2_lo, r);
    }
    // ln x as Hi + Lo, for pow and cbrt, whose y = c·ln x multiplies the
    // log's error by |y|. With b + Bl = m + 1 exactly (both differences
    // exact by Sterbenz) and r = a − f·b exact (f is the rounded quotient),
    // Fl = (r − f·Bl)/b puts f + Fl within 4U² of a/(m + 1), and |Fl| ≤
    // 2U·|f|. Then ln m = 2f + T, T = 2Fl + 2f·z·q(z), q = Σ z^k/(2k+3),
    // and 2f joins e·Hi in an exact two-sum; only T and the low parts
    // round. Relative to |ln x|, the error is HlRel:
    // - e = 0 (|ln x| ≥ |2f|), in units of |2f|: q evaluated at f, not f +
    //   Fl (the slope of 2·atanh f − 2f is 2z/(1 − z): 2U·z_log/(1 −
    //   z_log)); Fl's own error; Horner and the truncation; the product
    //   2f·z and T's fma (U each);
    // - e ≠ 0 (|ln x| ≥ 0.3466·|e|, |2f| ≤ 0.3432): that, plus the two
    //   additions of the low parts, e·Lo's rounding and ln2_res, per |e|.
    static constexpr double HlRel = [] {
        const horner_bound h = horner_error(log_tail_c<N - 1>, z_log, unit_roundoff);
        const double       t = z_log * h.Mag + 2 * unit_roundoff; // |T|/|2f|
        const double a2f = 2 * unit_roundoff * z_log / (1 - z_log) + 4 * unit_roundoff2 / (1 - z_log) + log_trunc(N) +
                           z_log * (h.Err + unit_roundoff * h.Mag) + unit_roundoff * t;
        const double per_e = 2 * unit_roundoff * (0.7 * unit_roundoff + ln2_lo) + unit_roundoff * ln2_lo + ln2_res;
        const double rest  = 2 * unit_roundoff * (0.35 * unit_roundoff + 0.3432 * t);
        return up(std::max(a2f, (0.3432 * a2f + rest + per_e) / 0.3466));
    }();
    [[gnu::always_inline]] static double value_hl(double x, double& lo) {
        int          e;
        const double m = mantissa(x, e);
        const double a = m - 1.0, b = m + 1.0;
        const double bl = m - (b - 1.0);
        const double f  = a / b;
        const double fl = fma(-f, bl, fma(-f, b, a)) / b;
        const double z  = f * f;
        const double f2 = f + f;
        const double t  = fma(fenced(f2 * z), horner(z, log_tail_c<N - 1>.C), fl + fl);
        const double ed = static_cast<double>(e);
        const hilo   h  = two_sum(ed * ln2_hi, f2);
        const hilo   v  = fast_two_sum(h.Hi, (h.Lo + ed * ln2_lo) + t);
        lo              = v.Lo;
        return v.Hi;
    }

    static double log(double x, double& bound) {
        const double v = value(x);
        bound          = Rel * __builtin_fabs(v) + tiny;
        return v;
    }
    static double log2(double x, double& bound) {
        const double v = value(x) * log2e;
        bound          = Rel2 * __builtin_fabs(v) + tiny;
        return v;
    }
    static double log10(double x, double& bound) {
        const double v = value(x) * log10e;
        bound          = Rel10 * __builtin_fabs(v) + tiny;
        return v;
    }

    // asinh and acosh as A = ln(1 + u), u ≥ 0 given as uh + ul with
    // |ul| ≤ U·uh, through ln's Hi + Lo: 1 + uh = wh + e1 exactly,
    // wl = e1 + ul rounds once, and A = ln wh + ln(1 + q), q = wl/wh. If
    // wh = 1, q = wl and only wl's rounding is left; below 2, |wl| ≤ U·wh
    // and A ≥ U, so |q| ≤ A·(1 + 2^-50); from 2 on, |q| ≤ 2U ≤ 3U·A. Relative to A: ln wh within HlRel of
    // |ln wh| ≤ 2A; wl's rounding, the quotient's, the sum with ln wh's Lo
    // and the final sum (U each, as |q| ≤ A); q − ln(1 + q) ≤ q²/2
    // (½U). Log1pRel collects these; the callers add u's own error.
    static constexpr double Log1pRel = up(2 * HlRel + 4 * unit_roundoff * (1 + 0x1p-50) + 0.5 * unit_roundoff);
    [[gnu::always_inline]] static double log1p_hl(double uh, double ul) {
        const hilo   w  = two_sum(1.0, uh);
        const double wl = w.Lo + ul;
        double       lo;
        const double v = value_hl(w.Hi, lo);
        return v + (lo + wl / w.Hi);
    }
    // asinh a, a = |x| ≤ 2^500: u = a + t, t = a²/(1 + √(1 + a²)), the sum
    // exact. t is within 4.5U (a², the fma's ½U through the root, the
    // root, 1 + √ and the quotient), which moves A by 4.5U·t/(1 + u)
    // absolutely; with a = sinh A, t = cosh A − 1, so relative to A that
    // is 4.5U·(1 − e^-A)²/(2A) ≤ 4.5U·0.2037 (the maximum, at A ≈ 1.26).
    static constexpr double AsinhRel = up(Log1pRel + 4.5 * unit_roundoff * 0.2037);
    // The plain form: below 1, ln(a + √(a² + 1)), whose argument is within
    // 2.5U (fma, √, sum); above, ln a + ln(1 + √(1 + 1/a²)), the second
    // argument within 2U and the sum one more rounding. Both: within
    // (Rel + U)·|v| + 2.5U.
    static constexpr double AhRel = up(Rel + unit_roundoff), AsinhAbs = up(2.5 * unit_roundoff);
    static double           asinh(double x, double& bound) {
        const double a = x < 0 ? -x : x;
        if constexpr (T <= plain_bits) {
            const double m = a <= 1.0 ? value(a + std::sqrt(fma(a, a, 1.0)))
                                      : value(a) + value(1.0 + std::sqrt(1.0 + 1.0 / (a * a)));
            bound          = AhRel * m + AsinhAbs + tiny;
            return x < 0 ? -m : m;
        }
        const double t = (a * a) / (1.0 + std::sqrt(fma(a, a, 1.0)));
        const hilo   u = two_sum(a, t);
        const double m = log1p_hl(u.Hi, u.Lo);
        bound          = AsinhRel * m + tiny;
        return x < 0 ? -m : m;
    }

    // acosh x, 1 ≤ x ≤ 2^500: u = y + s, y = x − 1, s = √(y² + 2y) (one
    // fma), the sum exact. y is exact below 2^53; above, its rounding moves
    // A by at most U·y/(1 + u) ≤ U, and A ≥ 37. s is within 1.5U, which
    // moves A by 1.5U·s/(1 + u); with s = sinh A, 1 + u = e^A, relative
    // to A that is 1.5U·(1 − e^-2A)/(2A) ≤ 1.5U.
    static constexpr double AcoshRel = up(Log1pRel + 1.5 * unit_roundoff + unit_roundoff / 37);
    // The plain form: ln x + ln(1 + s), s = √d, d = 1 − 1/x². 1/x² is
    // within 2U, and d (exact by Sterbenz, else one rounding of at most
    // U·d) within 2U of 1 − 1/x², which moves √d by at most 2U/s; the
    // root, the sum and the logs add (Rel + U)·|v| + 2U. At x = 1, s = 0
    // makes the bound infinite.
    static double acosh(double x, double& bound) {
        if constexpr (T <= plain_bits) {
            const double s = std::sqrt(1.0 - 1.0 / (x * x));
            const double v = value(x) + value(1.0 + s);
            bound          = AhRel * v + up(2 * unit_roundoff) + up(2 * unit_roundoff * (1 + 0x1p-50)) / s + tiny;
            return v;
        }
        const double y = x - 1.0;
        const double s = std::sqrt(fma(y, y, y + y));
        const hilo   u = two_sum(y, s);
        const double v = log1p_hl(u.Hi, u.Lo);
        bound          = AcoshRel * v + tiny;
        return v;
    }

    // atanh |x| < 1: ½·ln((1 + a)/(1 − a)), the quotient within 3U.
    static constexpr double AtanhAbs = up(1.5 * unit_roundoff);
    static double           atanh(double x, double& bound) {
        const double a = x < 0 ? -x : x;
        const double m = 0.5 * value((1.0 + a) / (1.0 - a));
        bound          = Rel * m + AtanhAbs + tiny;
        return x < 0 ? -m : m;
    }
};

//---------------------------------------------------------------------------
// atan: |x| > 1 through π/2 − atan(1/x); then a > tan(π/12) through the
// π/6 addition formula, so |t| ≤ t_max; atan t = t·Σ (−1)^k t^2k/(2k+1),
// alternating with falling terms.
//---------------------------------------------------------------------------
inline constexpr double t_max    = 0.2681; // ≥ tan(π/12) = 0.267949
inline constexpr double z_atan   = t_max * t_max;
inline constexpr double atan_min = 1 - z_atan / 3; // ≤ atan t / t
constexpr double        atan_trunc(int n) { return pow_n(z_atan, n) / (2 * n + 1) / atan_min; }
template <int T>
inline constexpr int atan_terms = terms_for(T, 15, atan_trunc);

template <int N>
inline constexpr poly<N> atan_c = series<N>([](int k) { return term{k % 2 ? -1.0 : 1.0, 2.0 * k + 1}; });

// Unreduced (|x| ≤ tan(π/12), the only case where |v| can be small):
// the Horner error and truncation over atan t / t, and fma(t, p, 0)'s
// rounding — relative. Every other path adds absolute errors, in units of
// U: t's three roundings (a − c, the fma, the quotient) move atan by
// 3·t_max; the constants c and π/6 by 0.23 and 0.5 (atan(c̃) is within
// |c̃ − c|·¾ of π/6); fma(t, p, π/6) rounds by π/4; 1/a moves atan by ½;
// π/2 − r by 0.56 (the constant) and π/2 (the rounding); and t·(Horner +
// truncation) of the polynomial itself.
template <int T>
struct atan_k {
    static constexpr int    N       = atan_terms<T>;
    static constexpr double PolyAbs = [] {
        const horner_bound h = horner_error(atan_c<N>, z_atan, unit_roundoff);
        return h.Err + atan_trunc(N) * atan_min;
    }();
    static constexpr double Rel = up(PolyAbs / atan_min + unit_roundoff);
    static constexpr double Abs =
        up(t_max * PolyAbs + 3 * t_max * unit_roundoff + inv_sqrt3_err * 0.75 + pi_sixth_err + r_trig * unit_roundoff +
           0.5 * unit_roundoff + pi_half_err + 1.5708 * unit_roundoff);

    [[gnu::always_inline]] static double value(double x) {
        const bool neg = x < 0;
        double     a   = neg ? -x : x;
        const bool inv = a > 1.0;
        if (inv)
            a = 1.0 / a;
        double off = 0.0;
        if (a > tan_pi12) {
            a   = (a - inv_sqrt3) / fma(a, inv_sqrt3, 1.0);
            off = pi_sixth;
        }
        double r = fma(a, horner(a * a, atan_c<N>.C), off);
        if (inv)
            r = pi_half - r;
        return neg ? -r : r;
    }
    static double atan(double x, double& bound) {
        const double v = value(x);
        bound          = Rel * __builtin_fabs(v) + Abs + tiny;
        return v;
    }

    // atan2: y/x rounds (½U on the angle); x < 0 adds ±π (its constant
    // and one rounding).
    static constexpr double Atan2Rel = up(Rel + unit_roundoff), Atan2Abs = up(Abs + 0.5 * unit_roundoff + pi_err);
    static double           atan2(double y, double x, double& bound) {
        double v;
        if (x > 0.0)
            v = value(y / x);
        else if (x < 0.0)
            v = value(y / x) + (y >= 0.0 ? pi_double : -pi_double);
        else
            v = y > 0.0 ? pi_half : y < 0.0 ? -pi_half : 0.0;
        bound = Atan2Rel * __builtin_fabs(v) + Atan2Abs + tiny;
        return v;
    }

    // asin = atan(x/√((1 − x)(1 + x))): the quotient within 3.5U, which
    // moves atan by 3.5U of itself at most. acos = π/2 − asin: absolute.
    static constexpr double AsinRel = up(Rel + 3.5 * unit_roundoff), AsinAbs = Abs;
    static constexpr double AcosRel = unit_roundoff, AcosAbs = up(AsinRel * 1.5708 + AsinAbs + pi_half_err);
    [[gnu::always_inline]] static double asin_value(double x) { return value(x / std::sqrt((1.0 - x) * (1.0 + x))); }
    static double                        asin(double x, double& bound) {
        const double v = asin_value(x);
        bound          = AsinRel * __builtin_fabs(v) + AsinAbs + tiny;
        return v;
    }
    static double acos(double x, double& bound) {
        const double v = pi_half - asin_value(x);
        bound          = AcosRel * __builtin_fabs(v) + AcosAbs + tiny;
        return v;
    }
};

//---------------------------------------------------------------------------
// Compositions.
//---------------------------------------------------------------------------
// e^(y), y = ln a / 3 (or e·ln b): ln's relative error Rel_log and the
// product's roundings move y by |y|·(Rel_log + 2U), and e^y by that much
// relatively, on top of Rel_exp. T counts the bits of the result; the
// log's error is multiplied by |y| (below 2^10), so it gets 10 more. Past
// T = 52 (outputs past 42 bits, where that would not decide at |y| = 16)
// y comes from the log's Hi + Lo instead: within |y|·(HlRel + 2^-103)
// (the low parts' roundings), and e^y sees one more rounding (RelLo).
template <int T>
struct pow_k {
    using L                      = log_k<T + 10 < full_bits ? T + 10 : full_bits>;
    using E                      = exp_k<T>;
    static constexpr bool   Hl   = T > 52;
    static constexpr double YRel = Hl ? up(L::HlRel + 0x1p-103) : up(L::Rel + 2 * unit_roundoff);
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
            y              = rounded_product(h, third);
            m              = E::value(y, fma(h, third, -y) + (h * third_lo + lo * third));
        } else {
            y = L::value(a) * third;
            m = E::value(y);
        }
        bound = m * (ERel + __builtin_fabs(y) * YRel) + tiny;
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
        bound = v * (ERel + __builtin_fabs(y) * YRel) + tiny;
        return true;
    }

    // Base^x with ln Base as a double-double (Hi, Lo within 2^-104 of it
    // relatively): y = x·ln Base as an exact product plus x·Lo, so e^y sees
    // only the low part's extra rounding and 2^-100·|y|.
    static double pow_ln(double x, double lnHi, double lnLo, double& bound, double& y) {
        y                = x * lnHi;
        const double ylo = fma(x, lnHi, -y) + x * lnLo;
        const double v   = E::value(y, ylo);
        bound            = v * (E::RelLo + __builtin_fabs(y) * 0x1p-100) + tiny;
        return v;
    }
};

// √ (correctly rounded) and √(x² + y²): the squares and their sum within
// 2U, the root halves that and rounds once more. |x|, |y| ≤ 2^500 keep
// the squares finite; squares below 2^-1022 lose at most 2^-1074, which
// moves the root by far less than tiny.
inline double           fp_sqrt(double x) { return std::sqrt(x); }
inline constexpr double sqrt_rel = up(unit_roundoff), hypot_rel = up(2 * unit_roundoff);
inline double           fp_hypot(double x, double y) { return std::sqrt(fma(x, x, y * y)); }

// Seeds of the dd tier's Newton steps: one step squares a seed's error,
// so 2^-48 is enough (the dd kernels' test checks the result).
inline constexpr int seed_bits = 48;
inline double        seed_cbrt(double x) {
    if (x == 0.0)
        return 0.0;
    const double m = exp_k<seed_bits>::value(log_k<seed_bits>::value(x < 0 ? -x : x) * third);
    return x < 0 ? -m : m;
}

} // namespace beman::inside::math::detail::fp

#endif // !BEMAN_INSIDE_MATH_NO_FP


// ======================================================================
//  beman/inside/detail/math_dd.hpp
// ======================================================================
// The double-double kernels of the math engine's dd tier (detail/math_entry.hpp):
// values as an unevaluated sum Hi + Lo of two doubles (about 106 bits), with
// error-free sums and products (std::fma), for outputs finer than the double
// tier decides. Every constant — ln 2, π, the 2^(j/64), 2^(j/4096) and sin(jπ/128)
// tables, the Taylor coefficients — comes at compile time from the integer
// path's exact series (detail/math_adaptive.hpp), never from a generator.
// Results are within 2^-97 relative, 2^-99 absolute at worst (measured; the
// dd tier in math_entry.hpp); the tier bounds them
// generously and lets the integer path decide whenever the bound does not.
//---------------------------------------------------------------------------



#ifndef BEMAN_INSIDE_MATH_NO_FP

    #include <algorithm>
    #include <array>
    #include <bit>
    #include <cmath> // std::fma
    #include <concepts>
    #include <cstdint>
    #include <iterator>
    #include <numeric>
    #include <ranges>
    #include <utility>

namespace beman::inside::math::detail::dd {
using namespace ::beman::inside::detail;
namespace ax  = ::beman::inside::math::detail::ax;
namespace fpk = ::beman::inside::math::detail::fp;

using dd = fpk::hilo;

//---------------------------------------------------------------------------
// Error-free transformations and the arithmetic on them (QD-library style),
// products by std::fma.
//---------------------------------------------------------------------------
using fpk::fast_two_sum;
using fpk::fenced;
using fpk::two_sum;

using fpk::rounded_product;

constexpr dd two_prod(double a, double b) noexcept {
    const double p = rounded_product(a, b);
    return {p, std::fma(a, b, -p)};
}

constexpr dd neg(dd a) noexcept { return {-a.Hi, -a.Lo}; }

constexpr dd add(dd a, dd b) noexcept {
    dd       s = two_sum(a.Hi, b.Hi);
    const dd t = two_sum(a.Lo, b.Lo);
    s          = fast_two_sum(s.Hi, s.Lo + t.Hi);
    return fast_two_sum(s.Hi, s.Lo + t.Lo);
}

constexpr dd add(dd a, double b) noexcept {
    const dd s = two_sum(a.Hi, b);
    return fast_two_sum(s.Hi, s.Lo + a.Lo);
}

constexpr dd sub(dd a, dd b) noexcept { return add(a, neg(b)); }

// a + b for |b| ≤ |a| (or a == 0), as in a Horner step: no cancellation, so
// the high parts need only fast_two_sum.
constexpr dd add_dominant(dd a, dd b) noexcept {
    const dd s = fast_two_sum(a.Hi, b.Hi);
    return fast_two_sum(s.Hi, s.Lo + (a.Lo + b.Lo));
}

constexpr dd add_dominant(dd a, double b) noexcept {
    const dd s = fast_two_sum(a.Hi, b);
    return fast_two_sum(s.Hi, s.Lo + a.Lo);
}

constexpr dd mul(dd a, dd b) noexcept {
    const dd p = two_prod(a.Hi, b.Hi);
    return fast_two_sum(p.Hi, p.Lo + (a.Hi * b.Lo + a.Lo * b.Hi));
}

constexpr dd mul(dd a, double b) noexcept {
    const dd p = two_prod(a.Hi, b);
    return fast_two_sum(p.Hi, p.Lo + a.Lo * b);
}

constexpr dd sqr(dd a) noexcept {
    const dd p = two_prod(a.Hi, a.Hi);
    return fast_two_sum(p.Hi, p.Lo + 2 * a.Hi * a.Lo);
}

// a/b: two quotient digits from one reciprocal; the remainder
// a − q1·b cancels its high part exactly (Sterbenz).
constexpr dd div(dd a, dd b) noexcept {
    const double inv = 1 / b.Hi;
    const double q1  = a.Hi * inv;
    const dd     p   = two_prod(q1, b.Hi);
    const double r   = ((a.Hi - p.Hi) - p.Lo) + (a.Lo - q1 * b.Lo);
    return fast_two_sum(q1, r * inv);
}

// a·2^m for |m| ≤ 1022 (a power-of-two factor, exact unless the result is
// subnormal).
constexpr dd scale(dd a, long m) noexcept {
    const double f = std::bit_cast<double>(static_cast<std::uint64_t>(m + 1023) << 52);
    return {a.Hi * f, a.Lo * f};
}

//---------------------------------------------------------------------------
// Constants from the integer path's fixed-point values Y·2^-S.
//---------------------------------------------------------------------------
// c·2^-S cut into N parts of B bits each (the last one of 53), so that
// k·part is exact for |k| < 2^(53−B): Cody–Waite reduction constants.
template <std::size_t N, std::size_t K>
constexpr std::array<double, N> parts(wide_sint<K> y, int S, int B) noexcept {
    std::array<double, N> r{};
    int                   n = bit_width_of(y); // the bits still to place
    for (std::size_t i = 0; i < N && !y.is_zero(); ++i) {
        const int          bits = i + 1 < N ? B : 53;
        const int          sh   = n > bits ? n - bits : 0;
        const wide_sint<K> top  = y >> sh;
        r[i]                    = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
        y                       = y - (top << sh);
        n                       = sh;
    }
    return r;
}

template <std::size_t K>
constexpr dd of_fixed(const wide_sint<K>& y, int S) noexcept {
    const auto [hi, lo] = parts<2>(y.negative() ? -y : y, S, 53);
    const dd r          = fast_two_sum(hi, lo);
    return y.negative() ? neg(r) : r;
}

inline constexpr int scale_bits = 136; // the scale (bits after the point) of the constants
using fixed                     = ax::fixed_t<2 * scale_bits + 16>;

// 1/n! and 1/n at full dd precision.
template <int S>
constexpr dd inv_fact(int n) noexcept {
    fixed f{1};
    for (int i = 2; i <= n; ++i)
        f = f * fixed{i};
    return of_fixed((fixed{1} << (2 * S)) / f, 2 * S);
}
template <int S>
constexpr dd inv_int(int n) noexcept {
    return of_fixed((fixed{1} << (2 * S)) / fixed{n}, 2 * S);
}

// 2^(j/2^shift), j = 0 … 63: the root 2^(1/2^shift) from the integer exp,
// then its powers at scale S (a unit lost per step).
template <int S>
constexpr std::array<dd, 64> exp2_table(int shift) noexcept {
    const auto         a    = ax::exp_fixed<S>(static_cast<fixed>(ax::ln2_q<S>) >> shift, 1, 4);
    const fixed        root = a.Scale == S ? a.Value : a.Value << (S - a.Scale);
    std::array<dd, 64> t{};
    fixed              p = fixed{1} << S;
    for (std::size_t j = 0; j < 64; ++j) {
        t[j] = of_fixed(p, S);
        p    = (p * root) >> S;
    }
    return t;
}

// sin(jπ/128), j = 0 … 255: cos and sin of π/128 by five half-angle square
// roots from cos π/4, then j rotations at scale scale_bits (a unit lost per step).
template <int S>
constexpr std::array<dd, 256> sin_table() noexcept {
    const fixed one  = fixed{1} << S;
    auto        root = [](fixed v) { return ax::isqrt(v << S); }; // √(v·2^-S) at scale S
    fixed       c    = root(one >> 1);                            // cos π/4
    for (int i = 0; i < 4; ++i)
        c = root((one + c) >> 1);                                             // cos π/64
    const fixed         s1 = root((one - c) >> 1), c1 = root((one + c) >> 1); // π/128
    std::array<dd, 256> t{};
    fixed               sj{0}, cj = one;
    for (int j = 0; j <= 64; ++j) {
        const dd v                           = of_fixed(sj, S);
        t[static_cast<std::size_t>(j)]       = v;
        t[static_cast<std::size_t>(128 - j)] = v;
        t[static_cast<std::size_t>(128 + j)] = neg(v);
        if (j > 0)
            t[static_cast<std::size_t>(256 - j)] = neg(v);
        const fixed sn = (sj * c1 + cj * s1) >> S;
        cj             = (cj * c1 - sj * s1) >> S;
        sj             = sn;
    }
    t[64]  = dd{1, 0};
    t[192] = dd{-1, 0};
    return t;
}

// atan(j/32), j = 0 … 32, from the integer atan: one constant expression
// per entry, so each has the compiler's whole evaluation budget.
template <int S, int J>
inline constexpr dd atan_entry = of_fixed(ax::atan_fixed<S>(fixed{J} << (S - 5), 0).Value, S);

template <int S>
constexpr std::array<dd, 33> atan_table() noexcept {
    return []<int... J>(std::integer_sequence<int, J...>) {
        return std::array<dd, 33>{atan_entry<S, J>...};
    }(std::make_integer_sequence<int, 33>{});
}

// ln(j/128), j = 96 … 192: sums of ln(i/(i − 1)) = 2·atanh(1/(2i − 1))
// outward from j = 128, each a short series at scale S + 16 (a unit lost
// per term).
template <int S>
constexpr std::array<dd, 97> log_table() noexcept {
    constexpr int W    = S + 16;
    auto          step = [](int i) {
        const fixed s = ax::inverse_series<ax::limbs_of<fixed>>(2 * i - 1, +1, W);
        return s + s;
    };
    const auto         to_dd = [](const fixed& s) { return of_fixed(s, W); };
    std::array<dd, 97> r{}; // r[32] = ln 1 = 0
    // Upward: r[j − 96] = Σ step(i), i = 129 … j.
    std::array<fixed, 64> up;
    std::ranges::transform(std::views::iota(129, 193), up.begin(), step);
    std::inclusive_scan(up.begin(), up.end(), up.begin());
    std::ranges::transform(up, r.begin() + 33, to_dd);
    // Downward: r[j − 96] = −Σ step(i), i = j + 1 … 128, j = 127 … 96.
    std::array<fixed, 32> down;
    std::ranges::transform(
        std::views::iota(97, 129) | std::views::reverse, down.begin(), [&](int i) { return -step(i); });
    std::inclusive_scan(down.begin(), down.end(), down.begin());
    std::ranges::transform(down, std::make_reverse_iterator(r.begin() + 32), to_dd);
    return r;
}

// Every constant and table, as members of a class template: computed on
// first use only, so a translation unit that never reaches the dd tier
// pays nothing for them (the scale S is a parameter so that GCC cannot
// fold the initializers early). D is always dd.
template <typename D, int S = scale_bits>
struct consts {
    static constexpr D Ln2    = of_fixed(ax::ln2_q<S>, S);
    static constexpr D Log2e  = of_fixed(ax::log2e_q<S>, S);
    static constexpr D Log10e = of_fixed(ax::log10e_q<S>, S);
    static constexpr D Pi     = of_fixed(ax::pi_q<S>, S);
    static constexpr D HalfPi = scale(Pi, -1);
    // ln 2/4096 in parts of 31 bits (|k| < 2^22: |x| ≤ 700 · 4096/ln 2), and
    // π/128 in parts of 27 bits (|n| < 2^26: |x| ≤ 2^20 · 128/π).
    static constexpr std::array<double, 3> Ln2By4096  = parts<3>(ax::ln2_q<S>, S + 12, 31);
    static constexpr std::array<double, 4> PiBy128    = parts<4>(ax::pi_q<S>, S + 7, 27);
    static constexpr std::array<D, 64>     Exp2By64   = exp2_table<S>(6);
    static constexpr std::array<D, 64>     Exp2By4096 = exp2_table<S>(12);
    static constexpr std::array<D, 256>    Sin        = sin_table<S>();
    static constexpr std::array<D, 33>     Atan       = atan_table<S>();
    static constexpr std::array<D, 97>     Log        = log_table<S>();
    static constexpr D F2 = inv_fact<S>(2), F3 = inv_fact<S>(3), F4 = inv_fact<S>(4), F5 = inv_fact<S>(5);
    static constexpr D F6 = inv_fact<S>(6), F7 = inv_fact<S>(7);
    static constexpr D I3 = inv_int<S>(3), I5 = inv_int<S>(5), I7 = inv_int<S>(7);
};

inline constexpr double inv_ln2_by_4096 = 4096 * fpk::log2e;
inline constexpr double inv_pi_by_128   = 128 / fpk::pi_double;

// x − k·(c0 + c1 + c2 + c3), the parts from parts<>: x.Hi − k·c0 is exact
// (k·c0 fits 53 bits and lies within a factor 2 of x.Hi), k·c1 and k·c2
// are exact and summed beside it, and k·c3 is far below the result's
// last bit — so the chain is two sums long.
inline dd reduce(dd x, double k, double c0, double c1, double c2, double c3) noexcept {
    const double a = std::fma(-k, c0, x.Hi);
    const dd     w = fast_two_sum(k * c1, k * c2);
    const dd     d = two_sum(a, -w.Hi);
    return fast_two_sum(d.Hi, d.Lo + ((x.Lo - w.Lo) - k * c3));
}

//---------------------------------------------------------------------------
// Kernels. Each is within about 2^-97 of its value (relative, plus an
// absolute 2^-99 where a subtraction cancels; dd_kernels_stay_far_inside_
// their_bound in math_adaptive.test.cpp checks it). Horner steps add a
// small product to a larger coefficient, so they use add_dominant. They are
// templates on D = dd only so that consts<D> is instantiated on first use.
//---------------------------------------------------------------------------
template <typename D>
concept dd_type = std::same_as<D, dd>;

// e^x for |x| ≤ 700: x = k·ln 2/4096 + r, |r| ≤ ln 2/8192, k = 4096m + 64i + j,
// e^x = 2^m · 2^(i/64) · 2^(j/4096) · (1 + p(r)). The terms of p from r^4
// on are below 2^-53 relative and summed in double.
template <dd_type D>
struct exp_reduced {
    D    R; // the reduced argument
    D    T; // 2^(i/64)·2^(j/4096)
    long M; // the power of 2
};

template <dd_type D>
[[gnu::always_inline]] inline exp_reduced<D> exp_reduce(D x) noexcept {
    using C         = consts<D>;
    const double k  = __builtin_nearbyint(x.Hi * inv_ln2_by_4096);
    const long   ik = static_cast<long>(k);
    return {
        reduce(x, k, C::Ln2By4096[0], C::Ln2By4096[1], C::Ln2By4096[2], 0),
        mul(C::Exp2By64[static_cast<std::size_t>((ik >> 6) & 63)], C::Exp2By4096[static_cast<std::size_t>(ik & 63)]),
        ik >> 12};
}

template <dd_type D>
inline D exp(D x) noexcept {
    using C              = consts<D>;
    const auto [r, t, m] = exp_reduce(x);
    const double rh      = r.Hi;
    const double tail    = fpk::horner(rh, 1.0 / 5040, 1.0 / 720, 1.0 / 120, 1.0 / 24);
    D            p       = add_dominant(C::F3, rh * tail);  // 1/3! + r/4! + …
    p                    = add_dominant(C::F2, mul(r, p));  // 1/2! + r/3! + …
    p                    = add_dominant(r, mul(sqr(r), p)); // e^r − 1
    return scale(add_dominant(t, mul(t, p)), m);
}

// ln x for normal x > 0: x = 2^m·f, f in [0.75, 1.5), c = j/128 the
// table point nearest f, ln x = m·ln 2 + ln c + 2·atanh s with
// s = (f − c)/(f + c), |s| ≤ 1/384. atanh s = s·Σ s^2k/(2k+1); the terms
// from s^7 on are below 2^-53 relative and summed in double, those from
// s^13 on below 2^-106.
template <dd_type D>
struct log_reduced {
    long M; // the power of 2
    D    S; // (f − c)/(f + c)
    D    L; // ln c
};

template <dd_type D>
[[gnu::always_inline]] inline log_reduced<D> log_reduce(D x) noexcept {
    long m = static_cast<long>(std::bit_cast<std::uint64_t>(x.Hi) >> 52) - 1023;
    D    f = scale(x, -m); // [1, 2)
    if (f.Hi >= 1.5) {
        f = {f.Hi * 0.5, f.Lo * 0.5};
        ++m;
    }
    const double j = __builtin_nearbyint(f.Hi * 128);
    const double c = j * 0x1p-7;
    return {m,
            div(fast_two_sum(f.Hi - c, f.Lo), add(f, c)), // f.Hi − c is exact, 0 or past |f.Lo|
            consts<D>::Log[static_cast<std::size_t>(j) - 96]};
}

template <dd_type D>
inline D log(D x) noexcept {
    using C               = consts<D>;
    const auto [m, s, lc] = log_reduce(x);
    const D      z        = sqr(s);
    const double zh       = z.Hi;
    D            p        = add_dominant(C::I5, zh * fpk::horner(zh, 1.0 / 11, 1.0 / 9, 1.0 / 7));
    p                     = add_dominant(C::I3, mul(z, p));     // 1/3 + z/5 + …
    p                     = add_dominant(s, mul(mul(s, z), p)); // atanh s
    const D t             = add(mul(C::Ln2, static_cast<double>(m)), lc);
    return add(t, D{2 * p.Hi, 2 * p.Lo});
}

struct sincos_t {
    dd Sin, Cos;
};

// sin and cos for |x| ≤ 2^20: x = n·π/128 + r, |r| ≤ π/256, then
// sin(a + r) = sin a + (sin a·(cos r − 1) + cos a·sin r) and the like with
// the table. The Taylor terms from r^9 (sin) and r^8 (cos) on are below
// 2^-53 relative and summed in double.
template <dd_type D>
struct trig_reduced {
    D R;      // the reduced argument
    D Sa, Ca; // sin and cos of nπ/128
};

template <dd_type D>
[[gnu::always_inline]] inline trig_reduced<D> trig_reduce(D x) noexcept {
    using C        = consts<D>;
    const double n = __builtin_nearbyint(x.Hi * inv_pi_by_128);
    const long   j = static_cast<long>(n);
    return {reduce(x, n, C::PiBy128[0], C::PiBy128[1], C::PiBy128[2], C::PiBy128[3]),
            C::Sin[static_cast<std::size_t>(j & 255)],
            C::Sin[static_cast<std::size_t>((j + 64) & 255)]};
}

template <dd_type D>
[[gnu::always_inline]] inline sincos_t sincos(D x) noexcept {
    using C                = consts<D>;
    const auto [r, sa, ca] = trig_reduce(x);
    const D      z         = sqr(r);
    const double zh        = z.Hi;
    const double st        = fpk::horner(zh, 1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0);
    const double ct        = fpk::horner(zh, 1.0 / 479001600.0, -1.0 / 3628800.0, 1.0 / 40320.0);
    D            s         = add_dominant(neg(C::F7), zh * st); // −1/7! + z/9! − …
    s                      = add_dominant(C::F5, mul(z, s));
    s                      = add_dominant(neg(C::F3), mul(z, s));
    const D sr             = add_dominant(r, mul(mul(r, z), s)); // sin r
    D       c              = add_dominant(neg(C::F6), zh * ct);
    c                      = add_dominant(C::F4, mul(z, c));
    c                      = add_dominant(neg(C::F2), mul(z, c));
    const D cm             = mul(z, c); // cos r − 1
    return {add_dominant(sa, add(mul(sa, cm), mul(ca, sr))), add_dominant(ca, sub(mul(ca, cm), mul(sa, sr)))};
}

template <dd_type D>
inline D sin(D x) noexcept {
    return sincos(x).Sin;
}
template <dd_type D>
inline D cos(D x) noexcept {
    return sincos(x).Cos;
}

// atan a for 0 ≤ a ≤ 1: c = j/32 nearest a, atan a = atan c + atan t with
// t = (a − c)/(1 + a·c), |t| ≤ 1/64. The series terms from t^9 on (2^-51
// relative at most) are summed in double, within 2^-104.
template <dd_type D>
inline D atan_unit(D a) noexcept {
    using C           = consts<D>;
    const double j    = __builtin_nearbyint(a.Hi * 32);
    const double c    = j * (1.0 / 32);
    const D      t    = div(add(a, -c), add_dominant(D{1, 0}, mul(a, c)));
    const D      z    = sqr(t);
    const double zh   = z.Hi;
    const double tail = fpk::horner(zh, -1.0 / 19, 1.0 / 17, -1.0 / 15, 1.0 / 13, -1.0 / 11, 1.0 / 9);
    D            p    = add_dominant(neg(C::I7), zh * tail); // −1/7 + z/9 − …
    p                 = add_dominant(C::I5, mul(z, p));
    p                 = add_dominant(neg(C::I3), mul(z, p));
    const D r         = add_dominant(t, mul(mul(t, z), p)); // atan t
    return add_dominant(C::Atan[static_cast<std::size_t>(j)], r);
}

// atan2(y, x) by `unit` (atan on [0, 1], the full or the lean kernel): the
// ratio of the smaller to the larger magnitude, then the octant.
template <dd_type D, class Unit>
inline D atan2_with(D y, D x, Unit unit) noexcept {
    using C         = consts<D>;
    const double ay = __builtin_fabs(y.Hi), ax_ = __builtin_fabs(x.Hi);
    if (ay == 0 && ax_ == 0)
        return D{0, 0};
    if (ay <= ax_) {
        const D q = div(y, x);
        const D r = q.Hi < 0 ? neg(unit(neg(q))) : unit(q);
        if (x.Hi > 0)
            return r;
        return y.Hi < 0 ? sub(r, C::Pi) : add(r, C::Pi);
    }
    const D q = div(x, y); // |q| < 1
    const D r = q.Hi < 0 ? neg(unit(neg(q))) : unit(q);
    return y.Hi > 0 ? sub(C::HalfPi, r) : sub(neg(C::HalfPi), r);
}

template <dd_type D, class Unit>
inline D atan_with(D x, Unit unit) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    r        = a.Hi <= 1 ? unit(a) : sub(consts<D>::HalfPi, unit(div(D{1, 0}, a)));
    return negative ? neg(r) : r;
}

template <dd_type D>
inline D atan2(D y, D x) noexcept {
    return atan2_with(y, x, [](D q) { return atan_unit(q); });
}
template <dd_type D>
inline D atan(D x) noexcept {
    return atan_with(x, [](D q) { return atan_unit(q); });
}

// √x: one Newton step from the correctly rounded double root.
template <dd_type D>
inline D sqrt(D x) noexcept {
    if (!(x.Hi > 0))
        return D{0, 0};
    const double y = fpk::fp_sqrt(x.Hi);
    const D      e = sub(x, two_prod(y, y));
    return fast_two_sum(y, e.Hi / (2 * y));
}

// ∛x: one Newton step from the double root.
template <dd_type D>
inline D cbrt(D x) noexcept {
    if (x.Hi == 0)
        return D{0, 0};
    const double y = fpk::seed_cbrt(x.Hi);
    const D      e = sub(mul(two_prod(y, y), y), x); // y³ − x
    return fast_two_sum(y, -e.Hi / (3 * y * y));
}

// √(1 − x²) as √((1 − x)(1 + x)): asin x = atan2(x, that), acos x = atan2(that, x).
template <dd_type D>
inline D asin_cofactor(D x) noexcept {
    return sqrt(mul(add(neg(x), 1.0), add(x, 1.0)));
}

template <dd_type D>
inline D tan(D x) noexcept {
    const sincos_t sc = sincos(x);
    return div(sc.Sin, sc.Cos);
}

template <dd_type D>
inline D exp2(D x) noexcept {
    return exp(mul(x, consts<D>::Ln2));
}
template <dd_type D>
inline D log2(D x) noexcept {
    return mul(log(x), consts<D>::Log2e);
}
template <dd_type D>
inline D log10(D x) noexcept {
    return mul(log(x), consts<D>::Log10e);
}
template <dd_type D>
inline D sinh(D x) noexcept {
    const D e = exp(x);
    return scale(sub(e, div(D{1, 0}, e)), -1);
}
template <dd_type D>
inline D cosh(D x) noexcept {
    const D e = exp(x);
    return scale(add(e, div(D{1, 0}, e)), -1);
}
template <dd_type D>
inline D tanh(D x) noexcept {
    const D e = exp(scale(x, 1));
    return div(add(e, -1.0), add(e, 1.0));
}

// The inverse hyperbolics on |x|, as in the double tier.
template <dd_type D>
inline D asinh(D x) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = log(add(a, sqrt(add(sqr(a), 1.0))));
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D acosh(D x) noexcept {
    return log(add(x, sqrt(mul(add(x, -1.0), add(x, 1.0)))));
}
template <dd_type D>
inline D atanh(D x) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = scale(log(div(add(a, 1.0), add(neg(a), 1.0))), -1);
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D asin(D x) noexcept {
    return atan2(x, asin_cofactor(x));
}
template <dd_type D>
inline D acos(D x) noexcept {
    return atan2(asin_cofactor(x), x);
}
template <dd_type D>
inline D hypot(D x, D y) noexcept {
    return sqrt(add(sqr(x), sqr(y)));
}

//---------------------------------------------------------------------------
// The lean kernels: the dd tier's first attempt for sin, cos, exp, exp2 and log.
// The tier serves outputs whose value indices stay below 2^62, which about
// 2^-70 decides nearly always, so these carry in double-double only the
// terms that need it — the table values, the reduced argument and its
// leading products, which are error-free — and the rest in double. Their
// bounds are proved the way the double tier's are (detail/math_fp.hpp):
// every rounding counted at its worst, up() for the second-order terms.
// Results they leave undecided go to the full kernels above.
//---------------------------------------------------------------------------
using fpk::unit_roundoff;
using fpk::unit_roundoff2;
using fpk::up;

// A table entry or constant (of_fixed: the top 53 bits and the next 53,
// both cut, from a value within 2^-120 of exact) is within 2^-104 of its
// value, relatively.
inline constexpr double table_err = 0x1p-104;

// reduce()'s error for |x.Lo| ≤ U·|x.Hi|, |k| ≤ Kx·max(1, |x|), the second
// part below 2^E1 and a residual of the parts below 2^-Res of their sum: in
// units of max(1, |x|), the roundings of x.Lo − w.Lo (|w.Lo| ≤ U·|k|·2^E1),
// k·c3 (below |k|·2^E3), their difference and the sum with d.Lo
// (|d.Lo| ≤ U·R): three of U on the terms, U² of R, and |k|·Res.
consteval double reduce_abs(double Kx, int E1, int E3, int Res, double R) {
    const double t =
        unit_roundoff * (1 + unit_roundoff) + unit_roundoff * Kx * fpk::pow_n(0.5, -E1) + Kx * fpk::pow_n(0.5, -E3);
    return up(3 * unit_roundoff * t * (1 + 4 * unit_roundoff) + Kx * fpk::pow_n(0.5, Res) + unit_roundoff2 * R);
}

// sin r = r + r³·S(r²), S = −1/3! + z/5! − z²/7!; cos r = 1 − r²/2 + r⁴·C(r²),
// C = 1/4! − z/6! + z²/8!.
inline constexpr fpk::poly<3> lean_sin_c =
    fpk::series<3>([](int k) { return fpk::term{k % 2 ? 1.0 : -1.0, fpk::factorial(2 * k + 3)}; });
inline constexpr fpk::poly<3> lean_cos_c =
    fpk::series<3>([](int k) { return fpk::term{k % 2 ? -1.0 : 1.0, fpk::factorial(2 * k + 4)}; });

// sin and cos for |x| ≤ 2^20, x = n·π/128 + r as in sincos. n's product
// rounds within 2^-26.6 of a step, and |x.Lo| ≤ 2^-33, so |r| ≤ R. With
// r = rh + rl (|rl| ≤ U·|rh|), z = rh² exactly as zh + zl, and the table's
// A = sin(nπ/128), B = cos(nπ/128):
//   sin(a + r) = A + B·sr + A·cm,  sr = sin r = rh + ts,
//   cos(a + r) = B − A·sr + B·cm,  cm = cos r − 1 = −zh/2 + tc,
// ts = rh·zh·S + rl and tc = zh²·C − zl/2 − rh·rl in double. The products
// B·rh and A·(−zh/2) are error-free and join A in two exact fast two-sums
// (|A| ≥ sin(π/128) > 2|B·rh| unless A = 0); everything else is summed in
// double. All bounds are absolute (|A|, |B| ≤ 1):
// - sr: rl·(cos − 1) for rl·cos (U·R³/2); rh·zh's two roundings, S's
//   Horner error at zh (horner_error with Dz = U), its truncation; ts's
//   fma (U·|ts|);
// - cm: rl·(rh − sin rh) and rl² (U·R⁴/6, U²·Z); zh²'s three roundings,
//   C's Horner error and truncation; the fmas of tc and of rh·rl + zl/2;
// - the sum of the low parts: at most 10 roundings of U, each partial sum
//   within Lmax; the dropped B.Lo·ts and A.Lo·tc; the table's 2^-104 of A
//   and B times (1 + R).
// The reduction's error (reduce_abs: |n| ≤ 42·max(1, |x|), c1 < 2^-31,
// c3 < 2^-86, the parts within 2^-139 of π/128) moves either value by as
// much, times 1 + R: AbsX per max(1, |x|).
struct lean_trig {
    static constexpr double            R = 0.01228; // ≥ (½ + 2^-26)·π/128 + 2^-33
    static constexpr double            Z = R * R, R3 = Z * R, R4 = Z * Z;
    static constexpr fpk::horner_bound HS = fpk::horner_error(lean_sin_c, Z, unit_roundoff);
    static constexpr fpk::horner_bound HC = fpk::horner_error(lean_cos_c, Z, unit_roundoff);
    static constexpr double            TS =
        R3 * HS.Mag * (1 + 4 * unit_roundoff) + unit_roundoff * R * (1 + unit_roundoff); // ≥ |ts|
    static constexpr double TC =
        R4 * HC.Mag * (1 + 4 * unit_roundoff) + 1.5 * unit_roundoff * Z * (1 + unit_roundoff); // ≥ |tc|
    static constexpr double SinR = unit_roundoff * R3 / 2 +
                                   R3 * (2 * unit_roundoff * HS.Mag + HS.Err + fpk::pow_n(Z, 3) / fpk::factorial(9)) +
                                   unit_roundoff * TS;
    static constexpr double CosR = unit_roundoff * R4 / 6 + 2 * unit_roundoff2 * Z +
                                   R4 * (3 * unit_roundoff * HC.Mag + HC.Err + fpk::pow_n(Z, 3) / fpk::factorial(10)) +
                                   unit_roundoff * TC;
    static constexpr double Lmax = 2 * unit_roundoff * (1 + R + Z) + unit_roundoff * R + unit_roundoff * Z / 2 +
                                   unit_roundoff + TS + unit_roundoff * R + TC + unit_roundoff * Z / 2;
    static constexpr double Abs =
        up(SinR + CosR + 10 * unit_roundoff * Lmax + unit_roundoff * (TS + TC) + table_err * (1 + R));
    static constexpr double AbsX = up(reduce_abs(42, -31, -86, 139, R) * (1 + R));
};

// A + B·(rh + ts) + A·(h + tc), h = −zh/2, as above.
template <dd_type D>
[[gnu::always_inline]] inline D lean_rotate(D a, D b, double rh, double ts, double h, double tc) noexcept {
    const D p  = two_prod(b.Hi, rh);
    const D q  = two_prod(a.Hi, h);
    const D s1 = fast_two_sum(a.Hi, p.Hi);
    const D s2 = fast_two_sum(s1.Hi, q.Hi);
    double  lo = std::fma(a.Lo, h, std::fma(b.Lo, rh, a.Lo + (p.Lo + q.Lo)));
    lo         = std::fma(a.Hi, tc, lo);
    lo         = std::fma(b.Hi, ts, lo);
    return fast_two_sum(s2.Hi, (s1.Lo + s2.Lo) + lo);
}

template <dd_type D>
[[gnu::always_inline]] inline sincos_t sincos_lean(D x) noexcept {
    const auto [r, sa, ca] = trig_reduce(x);
    const double rh = r.Hi, rl = r.Lo;
    const D      z  = two_prod(rh, rh);
    const double zh = z.Hi;
    const double ts = std::fma(fpk::rounded_product(rh, zh), fpk::horner(zh, lean_sin_c.C), rl);
    const double tc =
        std::fma(fpk::rounded_product(zh, zh), fpk::horner(zh, lean_cos_c.C), -std::fma(rh, rl, 0.5 * z.Lo));
    const double h = -0.5 * zh;
    return {lean_rotate(sa, ca, rh, ts, h, tc), lean_rotate(ca, neg(sa), rh, ts, h, tc)};
}

// The bound of a lean sin or cos at x.
constexpr double lean_trig_bound(double x) noexcept {
    return lean_trig::Abs + lean_trig::AbsX * std::max(1.0, __builtin_fabs(x)) + fpk::tiny;
}

template <dd_type D>
inline D sin_lean(D x, double& bound) noexcept {
    bound = lean_trig_bound(x.Hi);
    return sincos_lean(x).Sin;
}
template <dd_type D>
inline D cos_lean(D x, double& bound) noexcept {
    bound = lean_trig_bound(x.Hi);
    return sincos_lean(x).Cos;
}

// e^r − 1 = r + r²·P(r), P = 1/2! + r/3! + r²/4! + r³/5!.
inline constexpr fpk::poly<4> lean_exp_c = fpk::series<4>([](int k) { return fpk::term{1.0, fpk::factorial(k + 2)}; });

// e^x for |x| ≤ 700, x = k·ln 2/4096 + r as in exp: n's product rounds
// within 2^-29 of a step and |x.Lo| ≤ 2^-43, so |r| ≤ R. With
// r = rh + rl and T = 2^(i/64)·2^(j/4096) (a dd product, within
// 2·2^-104 + 6U² of its value), e^x = 2^m·(T + T·(rh + tp)),
// tp = rh²·P(rh) + rl in double; T.Hi·rh is error-free and joins T.Hi in
// an exact fast two-sum. Relative to T:
// - rh + tp: rl·(e^rh·(e^rl − 1)/rl − 1) (U·R²·(1 + R)); rh²'s rounding,
//   P's Horner error (at rh, exact) and truncation, tp's fma;
// - the low parts: 5 roundings within Lmax, the dropped T.Lo·tp, T's error
//   times 1 + R + TP;
// divided by the least e^r/T = 1 − R − TP, plus the reduction's error
// (reduce_abs: |k| ≤ 5910·max(1, |x|), c1 < 2^-42, no c3, the parts within
// 2^-127 of ln 2/4096) at |x| = 700, which moves e^x by as much,
// relatively.
struct lean_exp {
    static constexpr double            R  = 8.462e-5; // ≥ (½ + 2^-29)·ln 2/4096 + 2^-43
    static constexpr fpk::horner_bound HP = fpk::horner_error(lean_exp_c, R, 0);
    static constexpr double            TP =
        R * R * HP.Mag * (1 + 3 * unit_roundoff) + unit_roundoff * R * (1 + unit_roundoff); // ≥ |tp|
    static constexpr double ER = unit_roundoff * R * R * (1 + R) +
                                 R * R * (unit_roundoff * HP.Mag + HP.Err + fpk::pow_n(R, 4) / 720 * 1.0001) +
                                 unit_roundoff * TP;
    static constexpr double Et = up(2 * table_err + 6 * unit_roundoff2);
    static constexpr double Lmax =
        unit_roundoff * (1 + R) + unit_roundoff * R + unit_roundoff + TP + unit_roundoff * R;
    static constexpr double Rel =
        up((ER + Et * (1 + R + TP) + 5 * unit_roundoff * Lmax + unit_roundoff * TP) / (1 - R - TP) +
           700 * reduce_abs(5910, -42, -1074, 127, R));
    // exp2: y = x·ln 2 (|x| ≤ 1000, so |y| ≤ 694) as a dd product, within
    // 2^-104 + 6U² of itself; it moves e^y by |y| times that.
    static constexpr double Rel2 = up(Rel + 694 * (table_err + 6 * unit_roundoff2));
};

template <dd_type D>
[[gnu::always_inline]] inline D exp_lean_value(D x) noexcept {
    const auto [r, t, m] = exp_reduce(x);
    const double rh      = r.Hi;
    const double tp      = std::fma(fpk::rounded_product(rh, rh), fpk::horner(rh, lean_exp_c.C), r.Lo);
    const D      p       = two_prod(t.Hi, rh);
    const D      s       = fast_two_sum(t.Hi, p.Hi);
    const double lo      = std::fma(t.Hi, tp, std::fma(t.Lo, rh, s.Lo + (p.Lo + t.Lo)));
    return scale(fast_two_sum(s.Hi, lo), m);
}

template <dd_type D>
inline D exp_lean(D x, double& bound) noexcept {
    const D v = exp_lean_value(x);
    bound     = lean_exp::Rel * v.Hi + fpk::tiny;
    return v;
}
template <dd_type D>
inline D exp2_lean(D x, double& bound) noexcept {
    const D v = exp_lean_value(mul(x, consts<D>::Ln2));
    bound     = lean_exp::Rel2 * v.Hi + fpk::tiny;
    return v;
}
// 2·atanh s = 2s + 2s·z·P(z), P = 1/3 + z/5 + z²/7.
inline constexpr fpk::poly<3> lean_log_c = fpk::log_tail_c<3>;

// ln x for normal x > 0, reduced as in log: ln x = m·ln 2 + ln c + 2·atanh s,
// |s| ≤ S since |f − c| ≤ 1/256 and f + c ≥ 2·0.75 − 1/256. s is a dd
// quotient: its numerator is exact, the denominator one dd sum (2U² of
// it), and the division within about 15U², so s is within 32U²·|s|.
// m·Ln2.Hi, ln c's high part and 2s join in two exact fast two-sums
// (|m·ln 2| ≥ ln 2 > B unless m = 0, and |m·ln 2 + ln c| ≥ ln(129/128) > 2S
// unless it is 0); the tail
// t = 2·sh·zh·P(zh) (sh = s.Hi, zh = sh²) and the low parts are summed in
// double. All bounds absolute, |ln c| ≤ B = ln 1.5:
// - t: zh, sh·zh and the product with P round (3U of |t|); P's Horner
//   error at zh; the truncation (2S⁹/9, widened 1%); and t taken at sh, not
//   s (|s.Lo| ≤ U·S moves it by 2S²·U·S);
// - 2s: 64U²·S;
// - the low sum: 7 roundings within Lsum = 5U·(B + 2S) + |t|, plus per
//   unit of |m|·ln 2 the same 5U in Lsum (35U²), the product
//   m·Ln2.Lo's rounding (U²) and Ln2's 2^-104 (PerM);
// - ln c's table entry: 2^-104 of B.
struct lean_log {
    static constexpr double            S  = 0.002612; // ≥ (1/256)/1.4961
    static constexpr double            Z  = S * S;
    static constexpr double            B  = 0.4055; // ≥ ln 1.5
    static constexpr fpk::horner_bound HP = fpk::horner_error(lean_log_c, Z, unit_roundoff);
    static constexpr double            TT = 2 * S * Z * HP.Mag * (1 + 4 * unit_roundoff); // ≥ |t|
    static constexpr double ET   = 3 * unit_roundoff * TT + 2 * S * Z * HP.Err + 2 * fpk::pow_n(S, 9) / 9 * 1.01 +
                                   2 * Z * unit_roundoff * S * 1.0001;
    static constexpr double Lsum = 5 * unit_roundoff * (B + 2 * S) + TT;
    static constexpr double Abs  = up(ET + 64 * unit_roundoff2 * S + 7 * unit_roundoff * Lsum + table_err * B);
    static constexpr double PerM = up(0.6932 * (36 * unit_roundoff2 + table_err));
};

template <dd_type D>
[[gnu::always_inline]] inline D log_lean(D x, double& bound) noexcept {
    const auto [m, s, lc] = log_reduce(x);
    const double sh = s.Hi, zh = sh * sh;
    const double t  = 2 * (fpk::rounded_product(sh, zh) * fpk::horner(zh, lean_log_c.C));
    const double md = static_cast<double>(m);
    const D      a  = two_prod(md, consts<D>::Ln2.Hi);
    const D      s1 = fast_two_sum(a.Hi, lc.Hi);
    const D      s2 = fast_two_sum(s1.Hi, 2 * s.Hi);
    const double lo = (((s1.Lo + s2.Lo) + (a.Lo + md * consts<D>::Ln2.Lo)) + (lc.Lo + 2 * s.Lo)) + t;
    bound           = lean_log::Abs + lean_log::PerM * __builtin_fabs(md);
    return fast_two_sum(s2.Hi, lo);
}

// log2 and log10: ln x times the constant (within 2^-104) as a dd product
// (within 4U²), so the bound scales by the constant (≤ 1.4427) and adds
// 2^-100 of the result.
template <dd_type D>
inline D log2_lean(D x, double& bound) noexcept {
    const D v = mul(log_lean(x, bound), consts<D>::Log2e);
    bound     = bound * 1.4427 + 0x1p-100 * __builtin_fabs(v.Hi);
    return v;
}
template <dd_type D>
inline D log10_lean(D x, double& bound) noexcept {
    const D v = mul(log_lean(x, bound), consts<D>::Log10e);
    bound     = bound * 0.4343 + 0x1p-100 * __builtin_fabs(v.Hi);
    return v;
}
//---------------------------------------------------------------------------
// Lean kernels built on the ones above. Error terms of the dd operations,
// relative to their exact results: a sum (add, sub, add_dominant) rounds
// within 4U² of |a| + |b|; a product (mul, sqr) within 4U²; a quotient
// (div: one reciprocal, a residual that cancels exactly, two digits) within
// 32U²; sqrt (one Newton step from the correctly rounded root) within 7U²
// plus half its argument's. x ± 1 is exact for 0.5 ≤ |x.Hi| ≤ 2 (Sterbenz:
// the low part then adds to an exact zero), and within 2U²·(1 + |x|) of
// itself otherwise.
//---------------------------------------------------------------------------
// tan = sin/cos from the lean sincos, each within e: |s/c − S/C| ≤
// e·(1 + |t|)/(|c| − e), plus the quotient's 32U². No bound (infinity)
// where |c| ≤ e: the full kernel decides there.
template <dd_type D>
inline D tan_lean(D x, double& bound) noexcept {
    const sincos_t sc = sincos_lean(x);
    const double   e  = lean_trig_bound(x.Hi);
    const D        t  = div(sc.Sin, sc.Cos);
    const double   at = __builtin_fabs(t.Hi), c = __builtin_fabs(sc.Cos.Hi) - 2 * e;
    bound = c > 0 ? e * (1 + at) / c * (1 + 0x1p-40) + 32 * unit_roundoff2 * at : __builtin_inf();
    return t;
}

// sinh, cosh = (e ± 1/e)/2 with e = e^x within Rel: 1/e within Rel + 32U²,
// the sum's 4U² of e + 1/e; all within (Rel + 20U²)·cosh x, and
// cosh ≤ |sinh| + 1. tanh = (e2 − 1)/(e2 + 1), e2 = e^(2x): e2's Rel moves it
// by 2·e2·Rel/(e2 + 1)² ≤ Rel/2; the two sums with 1 round within
// 2U²·(e2 + 1), 2U² of the quotient each; the quotient 32U².
template <dd_type D>
inline D sinh_lean(D x, double& bound) noexcept {
    const D e = exp_lean_value(x);
    const D v = scale(sub(e, div(D{1, 0}, e)), -1);
    bound     = (lean_exp::Rel + 20 * unit_roundoff2) * (__builtin_fabs(v.Hi) + 1) * (1 + 0x1p-40) + fpk::tiny;
    return v;
}
template <dd_type D>
inline D cosh_lean(D x, double& bound) noexcept {
    const D e = exp_lean_value(x);
    const D v = scale(add(e, div(D{1, 0}, e)), -1);
    bound     = (lean_exp::Rel + 20 * unit_roundoff2) * v.Hi * (1 + 0x1p-40) + fpk::tiny;
    return v;
}
template <dd_type D>
inline D tanh_lean(D x, double& bound) noexcept {
    const D e2 = exp_lean_value(scale(x, 1));
    bound      = (lean_exp::Rel / 2 + 36 * unit_roundoff2) * (1 + 0x1p-40);
    return div(add(e2, -1.0), add(e2, 1.0));
}

// The inverse hyperbolics as ln w, w within ε relative: within the lean
// log's bound plus ε·(1 + 2^-40).
// - asinh |x|: w = |x| + √(x² + 1): the square 4U², + 1 2U², the root
//   2U² + 7U², the sum 4U²: ε = 17U².
// - acosh x ≥ 1: w = x + √((x − 1)(x + 1)): x − 1 exact for x.Hi ≤ 2, else
//   within 2U²·(1 + x)/(x − 1) ≤ 6U²; x + 1 2U²·(1 + x)/(x + 1) ≤ 2U²;
//   the product 4U², the root 6U² + 7U², the sum 4U²: ε = 23U².
// - atanh |x| < 1: ½·ln((1 + a)/(1 − a)): 1 − a exact for a.Hi ≥ 0.5, else
//   within 2U²·(1 + a)/(1 − a) ≤ 6U²; 1 + a 2U²; the quotient 32U²:
//   ε = 40U², and the bound halves.
template <dd_type D>
inline D asinh_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = log_lean(add(a, sqrt(add(sqr(a), 1.0))), bound);
    bound += 17 * unit_roundoff2 * (1 + 0x1p-40);
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D acosh_lean(D x, double& bound) noexcept {
    const D v = log_lean(add(x, sqrt(mul(add(x, -1.0), add(x, 1.0)))), bound);
    bound += 23 * unit_roundoff2 * (1 + 0x1p-40);
    return v;
}
template <dd_type D>
inline D atanh_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = scale(log_lean(div(add(a, 1.0), add(neg(a), 1.0)), bound), -1);
    bound               = (bound + 40 * unit_roundoff2) * 0.5 * (1 + 0x1p-40);
    return negative ? neg(m) : m;
}

// atan t = t − t·z·P(z), P = 1/3 − z/5 + z²/7 − z³/9 + z⁴/11.
inline constexpr fpk::poly<5> lean_atan_c =
    fpk::series<5>([](int k) { return fpk::term{k % 2 ? -1.0 : 1.0, 2.0 * k + 3}; });

// atan a for 0 ≤ a ≤ 1 (a.Hi ≤ 1), reduced as in atan_unit: c = j/32,
// t = (a − c)/(1 + a·c), |t| ≤ S. a − c is exact (Sterbenz, c ≥ 1/32, or
// c = 0); 1 + a·c within 3U²; so t within 36U². atan a = atan c + t + T,
// T = −th·zh·P(zh) in double (th = t.Hi, zh = th²); atan c + t.Hi in one
// exact fast two-sum (atan c ≥ atan(1/32) > S unless c = 0). Absolute:
// - T: three roundings (3U of |T|), P's Horner error at zh, the
//   alternating series' first omitted term S¹³/13, and T taken at th, not
//   t (U·S·S²);
// - t: 36U²·S;
// - the low sum: 4 roundings within Lsum; atan c's table 2^-104.
struct lean_atan {
    static constexpr double            S  = 0.015626; // ≥ 1/64
    static constexpr double            Z  = S * S;
    static constexpr double            A  = 0.7854; // ≥ π/4
    static constexpr fpk::horner_bound HP = fpk::horner_error(lean_atan_c, Z, unit_roundoff);
    static constexpr double            TT = S * Z * HP.Mag * (1 + 4 * unit_roundoff); // ≥ |T|
    static constexpr double            ET =
        3 * unit_roundoff * TT + S * Z * HP.Err + fpk::pow_n(S, 13) / 13 + unit_roundoff * S * Z;
    static constexpr double Lsum = 2 * unit_roundoff * (A + S) + unit_roundoff * A + unit_roundoff * S + TT;
    static constexpr double Unit = up(ET + 36 * unit_roundoff2 * S + 4 * unit_roundoff * Lsum + table_err * A);
    // atan |x| > 1: π/2 − atan(1/|x|): the quotient 32U² (slope ≤ 1), the
    // difference 4U²·(π/2 + π/4), π/2's 2^-104.
    static constexpr double Atan = up(Unit + 32 * unit_roundoff2 + 10 * unit_roundoff2 + table_err * 1.571);
    // atan2: the quotient of the smaller by the larger 32U²; ±π or ±π/2 and
    // the sum 4U²·(π + π/4) and 2^-104 of π.
    static constexpr double Atan2 = up(Unit + 32 * unit_roundoff2 + 16 * unit_roundoff2 + table_err * 3.1416);
    // asin, acos = atan2 with √((1 − x)(1 + x)) as one argument: 1 ∓ x within
    // 6U² (as x ± 1 above), the product 4U², the root 8U² + 7U² — 15U²
    // relative, which moves the angle by at most half of it.
    static constexpr double Asin = up(Atan2 + 8 * unit_roundoff2);
};

template <dd_type D>
[[gnu::always_inline]] inline D atan_unit_lean(D a) noexcept {
    const double j  = __builtin_nearbyint(a.Hi * 32);
    const double c  = j * (1.0 / 32);
    const D      t  = div(add(a, -c), add_dominant(D{1, 0}, mul(a, c)));
    const double th = t.Hi, zh = th * th;
    const double T  = -(fpk::rounded_product(th, zh) * fpk::horner(zh, lean_atan_c.C));
    const D      ac = consts<D>::Atan[static_cast<std::size_t>(j)];
    const D      s  = fast_two_sum(ac.Hi, th);
    return fast_two_sum(s.Hi, ((s.Lo + ac.Lo) + t.Lo) + T);
}

// The full kernels' atan forms with the lean unit atan.
template <dd_type D>
inline D atan_lean(D x, double& bound) noexcept {
    bound = lean_atan::Atan;
    return atan_with(x, [](D q) { return atan_unit_lean(q); });
}
template <dd_type D>
inline D atan2_lean(D y, D x, double& bound) noexcept {
    bound = y.Hi == 0 && x.Hi == 0 ? 0 : lean_atan::Atan2;
    return atan2_with(y, x, [](D q) { return atan_unit_lean(q); });
}
template <dd_type D>
inline D asin_lean(D x, double& bound) noexcept {
    bound = lean_atan::Asin;
    return atan2_with(x, asin_cofactor(x), [](D q) { return atan_unit_lean(q); });
}
template <dd_type D>
inline D acos_lean(D x, double& bound) noexcept {
    bound = lean_atan::Asin;
    return atan2_with(asin_cofactor(x), x, [](D q) { return atan_unit_lean(q); });
}
} // namespace beman::inside::math::detail::dd

#endif // !BEMAN_INSIDE_MATH_NO_FP



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
        const grid_int lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.slot_count();
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
inline constexpr double notch_p = grid_double(wide_numerator(notch_of<Out>));
template <insidable Out>
inline constexpr double notch_q = grid_double(wide_denominator(notch_of<Out>));
template <insidable Out>
inline constexpr bool dyadic_notch =
    is_pow2(wide_numerator(notch_of<Out>)) && is_pow2(wide_denominator(notch_of<Out>));

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
    constexpr double     hi = lo + static_cast<double>(static_cast<imax>(grid_of<Out>.slot_count()));
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
        const ddk::dd q = ddk::div(exact(r.Numerator), exact(abs_den(r.Denominator)));
        return r.Denominator < 0 ? ddk::neg(q) : q;
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
    constexpr imax       last  = static_cast<imax>(slot_base<Out> + grid_of<Out>.slot_count());
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
BEMAN_INSIDE_AX_KERNEL(exp, exp_k, exp, unbounded, fp_limit(fp_full_exp::Rel), __builtin_fabs(v))
BEMAN_INSIDE_AX_KERNEL(exp2, exp_k, exp2, unbounded, fp_limit(fp_full_exp::Rel2), __builtin_fabs(v))
BEMAN_INSIDE_AX_KERNEL(sinh, exp_k, sinh, unbounded, fp_limit(fp_full_exp::SinhRel), __builtin_fabs(v) + 1)
BEMAN_INSIDE_AX_KERNEL(cosh, exp_k, cosh, unbounded, fp_limit(fp_full_exp::CoshRel), __builtin_fabs(v) + 1)
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
BEMAN_INSIDE_AX_KERNEL(log, log_k, log, unbounded, fp_limit(fp_full_log::Rel), 1.0 / __builtin_fabs(x))
BEMAN_INSIDE_AX_KERNEL(log2, log_k, log2, unbounded, fp_limit(fp_full_log::Rel2), 1.5 / __builtin_fabs(x))
BEMAN_INSIDE_AX_KERNEL(log10, log_k, log10, unbounded, fp_limit(fp_full_log::Rel10), 1.0 / __builtin_fabs(x))
BEMAN_INSIDE_AX_KERNEL(asinh, log_k, asinh, unbounded, fp_limit(fp_full_log::AsinhRel), 1.0)
BEMAN_INSIDE_AX_KERNEL(
    atanh, log_k, atanh, unbounded, fp_limit(fp_full_log::Rel + fp_full_log::AtanhAbs), 1.0 / ((1.0 - x) * (1.0 + x)))
// cbrt = e^(ln|x|/3): the log's error grows with |ln|x||, up to 2^4.
BEMAN_INSIDE_AX_KERNEL(cbrt,
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
inline constexpr grid_rational half_unit =
    ::beman::inside::detail::gcd(notch_of<Out> / grid_rational{2}, lower_of<Out>);

template <insidable Out, policy_flag R, grid_rational Unit = half_unit<Out>>
using bracket_t =
    inside<grid{{lower_of<Out> - notch_of<Out>, upper_of<Out> + notch_of<Out>}, Unit}, R | (flags_of<Out> & clamp)>;

// The integer form: a bracket 1024 times finer than half_unit, counted in
// imax. Rounded down there, the value lies in [y, y + F); only when y is
// itself a rounding boundary of Out (a point or a half point — at most one
// bracket point in 1024) does rounding up decide whether the value is exactly y.
// Then Out's slot follows from the bracket index by one integer division.
template <insidable Out>
inline constexpr grid_rational fine_unit = half_unit<Out> / grid_rational{1024};

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
        if constexpr (checked)
            return try_store<Out, flags_of<Out>>(
                [&](Out& out, auto&& policy) { out = ax::store_exact<Out>(v, policy); });
        else
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
            return ::beman::inside::detail::try_store<Out, flags_of<Out>>(
                [&](Out& out, auto&& policy) { out = ax::store_exact<Out>(mid(*lo, *hi), policy); });
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
        ax::note<ax::probe::integer_run>();
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
    #define BEMAN_INSIDE_AX_TIERS(Out, FP, DD, fp_call, dd_call, ...)                                     \
        ::beman::inside::math::detail::ax::note<::beman::inside::math::detail::ax::probe::integer_run>(); \
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
        constexpr std::size_t E = std::max(ax::input_limbs<InY>, ax::input_limbs<InX>);
        using F                 = ::beman::inside::detail::wide_rational<E>;
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
                              ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_of<2>(Base), ax::exact_input(x)}))
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
    std::max(ax::bound_at<In, cosh_core<In>, true, true>, ax::bound_at<In, cosh_core<In>, false, true>);
template <insidable In>
using cosh = ax::result_grid_t<In, cosh_lo<In>, cosh_hi<In>>;

// sqrt of a mixed-sign input: [0, √max(|Lower|, |Upper|)].
template <insidable In>
inline constexpr grid_rational max_abs = std::max(upper_of<In>, -lower_of<In>);
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
        : ax::pow_core<2, E, Mag, KMax>{ax::exact_of<2>(Base), x} {}
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
    ax::lattice_bound<ax::gcd_notch<A, B>, true>(ax::atan2_core<1>{ax::exact_of<1>(0), ax::exact_of<1>(-1)});
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
[[nodiscard]] constexpr auto atan2(InY y, InX x) {
    static_assert(deducible<InY>() && deducible<InX>());
    return atan2_into<results::atan2<InY, InX>>(y, x);
}

template <insidable InX, insidable InY>
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



//---------------------------------------------------------------------------
// beman::inside::math — the transcendental functions (detail/math_entry.hpp),
// the grid operations (abs, sign, copysign, floor, ceil, round, trunc, fmod,
// pown), the constants (pi, e, … from detail/math_constants.hpp) and
// unit_amplitude<K>, the [-1, 1] output grid at resolution 1/K.
//
// Every transcendental returns the correctly rounded point of its output grid,
// under the output's rounding mode, at whatever precision that grid needs —
// so results are the same on every platform, at compile time and at runtime,
// with or without an FPU. Inputs and outputs may be grids past 64 bits.
// `fn_into<Out>(x)` names the output grid; `fn(x)` deduces it from the input
// (its notch and policy, the function's range over the input rounded
// outward).
//
// BEMAN_INSIDE_MATH_NO_FP (auto-enabled when freestanding) leaves out the
// engine's double and dd tiers and <cmath>; results do not change. Builds
// with -ffast-math are rejected (policy_flag.hpp).
//---------------------------------------------------------------------------
namespace beman::inside::math {
namespace detail {
using namespace beman::inside::detail;

// Policy of an auto-deduced output: the input's (a cursor's output is no
// cursor).
template <insidable In>
inline constexpr policy_flag out_policy = flags_of<In> & ~cursor_marker;

// A 64-bit grid operation's result through Out's assignment.
template <insidable Out, typename V>
constexpr Out store_value(const V& v) {
    return Out{v};
}

// The sign of a grid number, for the deduced sign() output.
constexpr grid_rational grid_sign(const grid_rational& r) { return grid_rational{(r > 0) - (r < 0)}; }

// max(|Lower|, |Upper|): sizes the abs and copysign outputs.
template <insidable In>
inline constexpr grid_rational abs_result_upper = abs(lower_of<In>) > abs(upper_of<In>) ? abs(lower_of<In>)
                                                                                        : abs(upper_of<In>);

// The lattice of ±x: x's notch, refined by 2·Lower when x's lattice does
// not pass through 0 (±(0.25 + k/2) lie on 0.25 + ℤ/2; ±(0.5 + k) on ℤ/2).
template <insidable In>
inline constexpr grid_rational sym_notch =
    anchored<In> ? notch_of<In> : gcd(notch_of<In>, lower_of<In> + lower_of<In>);

// The lowest non-negative point of that lattice: 0 when it passes through 0.
template <insidable In>
inline constexpr grid_rational sym_floor = [] {
    if constexpr (anchored<In>)
        return grid_rational{0};
    else {
        const grid_rational n = sym_notch<In>, l = lower_of<In>;
        // l − n·⌊l/n⌋, with ⌊l/n⌋ from the rounding helper below
        return l - (n * grid_to_int<round_mode::floor>(l / n));
    }
}();

// An exact math output's grid: the lattice when it has a notch, else
// continuous with the denominator bound d (an exact ±x or x − k·y keeps the
// inputs' denominators bounded, grid.hpp den_bound).
consteval grid exact_out_grid(const interval& iv, const grid_rational& notch, const grid_int& d) {
    return notch == 0 ? bounded_continuous(iv, d) : grid{iv, notch};
}

template <insidable In>
using abs_result_t =
    inside<exact_out_grid({sym_floor<In>, abs_result_upper<In>}, sym_notch<In>, den_bound(grid_of<In>)),
           out_policy<In>>;

// sign(x) ∈ {sign(Lower) … sign(Upper)}, integer notch.
template <insidable In>
using sign_result_t = inside<{grid_sign(lower_of<In>), grid_sign(upper_of<In>)}, out_policy<In>>;

// copysign(mag, sgn): |mag| with sgn's possible signs. |mag| ranges over
// [m_lo, m_hi] (m_lo = 0 when mag's interval spans 0); ±|mag| lies on
// sym_notch's lattice (mag's own when it passes through 0).
template <insidable Mag>
inline constexpr grid_rational abs_result_lower = (lower_of<Mag> <= 0 && upper_of<Mag> >= 0)  ? sym_floor<Mag>
                                                  : (abs(lower_of<Mag>) < abs(upper_of<Mag>)) ? abs(lower_of<Mag>)
                                                                                              : abs(upper_of<Mag>);

template <insidable Mag, insidable Sgn>
using copysign_result_t = inside<exact_out_grid({lower_of<Sgn> < 0 ? -abs_result_upper<Mag> : abs_result_lower<Mag>,
                                                 upper_of<Sgn> >= 0 ? abs_result_upper<Mag> : -abs_result_lower<Mag>},
                                                sym_notch<Mag>,
                                                den_bound(grid_of<Mag>)),
                                 out_policy<Mag>>;

template <insidable In, round_mode M>
using integer_result_t = inside<{{grid_to_int<M>(lower_of<In>), grid_to_int<M>(upper_of<In>)}, 1}, out_policy<In>>;

template <insidable In>
using floor_result_t = integer_result_t<In, round_mode::floor>;
template <insidable In>
using ceil_result_t = integer_result_t<In, round_mode::ceil>;
template <insidable In>
using round_result_t = integer_result_t<In, round_mode::nearest>;
template <insidable In>
using trunc_result_t = integer_result_t<In, round_mode::trunc>;

// The exact path: an input or output past the 64-bit rationals (more than
// 2^64 slots, or grid numbers past 64 bits) computes on exact values.
template <insidable... Bs>
inline constexpr bool any_wide_valued = (wide_valued<Bs> || ...);

template <insidable Out, std::size_t E>
constexpr Out store_exact(const wide_rational<E>& v) {
    return ax::store_exact<Out>(v, make_policy<flags_of<Out>>());
}

template <round_mode M, std::size_t E>
constexpr wide_rational<E> exact_to_int(const wide_rational<E>& v) noexcept {
    return {rounded_div<M>(v.Num, v.Den), wide_sint<E>{1}};
}

// Integer fast path for the algebraic tier: an anchored integer raw holds
// the value J·p/q (J its value index, Notch p/q), so its integer rounding is
// J·p over q rounded. It applies when the auto-deduced Out (which holds every
// result by construction) is the target and |J·p| fits imax.
template <insidable Out, insidable AutoOut, insidable In>
inline constexpr bool int_direct = [] {
    if constexpr (!std::same_as<Out, AutoOut> || !integer_storage<In> || !integer_storage<Out> || !notched<In> ||
                  !anchored<In> || wide_valued<In> || wide_valued<Out>)
        return false;
    else {
        const umax m = index_lo64<In>->Numerator > index_hi64<In>->Numerator ? index_lo64<In>->Numerator
                                                                             : index_hi64<In>->Numerator; // max |J|
        return m <= static_cast<umax>(std::numeric_limits<imax>::max()) / notch64<In>.Numerator;
    }
}();

// x rounded to an integer by M: exactly, on the value index, or through
// rational. Round is half away from zero, as rational round() is.
template <round_mode M, insidable Out, insidable In>
constexpr Out integer_into(In x) {
    if constexpr (any_wide_valued<Out, In>)
        return store_exact<Out>(exact_to_int<M>(ax::exact_input(x)));
    else if constexpr (int_direct<Out, integer_result_t<In, M>, In>) {
        constexpr imax p = static_cast<imax>(notch64<In>.Numerator);
        constexpr imax q = static_cast<imax>(abs_den(notch64<In>.Denominator));
        const imax     j = value_index<imax>(x) * p;
        if constexpr (p == 1 && std::has_single_bit(static_cast<umax>(q)) && M != round_mode::half_even) {
            // A notch 2^-k: shifts. floor is the arithmetic shift, ceil its
            // mirror; nearest and trunc shift the magnitude (half away from 0).
            constexpr int k = std::countr_zero(static_cast<umax>(q));
            if constexpr (M == round_mode::floor)
                return from_value_index<Out>(j >> k);
            else if constexpr (M == round_mode::ceil)
                return from_value_index<Out>(-((-j) >> k));
            else {
                const imax s = j >> 63, a = (j ^ s) - s; // sign mask, |j|
                const imax r = (a + (M == round_mode::nearest ? q / 2 : 0)) >> k;
                return from_value_index<Out>((r ^ s) - s);
            }
        } else
            return from_value_index<Out>(div_rounded(j, q, M));
    } else // (as a rational: the integer may pass imax, a uint64's)
        return store_value<Out>(round_to_integral(rational{x}, M));
}
} // namespace detail

//---------------------------------------------------------------------------
// Algebraic tier — exact, no polynomial machinery. Each function wraps the
// corresponding `rational` operation and routes through `Out`'s assignment.
//---------------------------------------------------------------------------

namespace detail {
template <class Out, class In>
consteval bool abs_into_ok() {
    return lower_of<Out> <= abs_result_lower<In>;
}
} // namespace detail

// |x|. Out must include the smallest |x|, 0 when x's range spans 0 — a
// constraint, so a probe sees it.
template <insidable Out, insidable In>
    requires(detail::abs_into_ok<Out, In>())
[[nodiscard]] constexpr Out abs_into(In x) {
    if constexpr (detail::any_wide_valued<Out, In>)
        return detail::store_exact<Out>(detail::ax::abs(detail::ax::exact_input(x)));
    else if constexpr (std::same_as<Out, detail::abs_result_t<In>> && detail::notched<In> && detail::anchored<In> &&
                       notch_of<Out> == notch_of<In>) {
        // The same lattice through 0: |x| is the value index's magnitude.
        // In umax: |INT64_MIN| = 2^63 is past imax (Out then reaches it).
        const imax j = detail::value_index<imax>(x);
        return detail::from_value_index<Out>(j < 0 ? umax{0} - static_cast<umax>(j) : static_cast<umax>(j));
    } else
        return detail::store_value<Out>(beman::inside::detail::abs(rational{x}));
}

// sign(x) ∈ {−1, 0, 1}, by exact comparison (no decode).
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out sign_into(In x) {
    return Out{imax{(x > 0) - (x < 0)}};
}

// copysign(mag, sgn) — |mag| with the sign of sgn; sgn == 0 counts as positive.
template <insidable Out, insidable Mag, insidable Sgn>
[[nodiscard]] constexpr Out copysign_into(Mag mag, Sgn sgn) {
    if constexpr (detail::any_wide_valued<Out, Mag>) {
        const auto a = detail::ax::abs(detail::ax::exact_input(mag));
        return detail::store_exact<Out>(sgn < 0 ? -a : a);
    } else if constexpr (std::same_as<Out, detail::copysign_result_t<Mag, Sgn>> && detail::notched<Mag> &&
                         detail::anchored<Mag> && notch_of<Out> == notch_of<Mag>) {
        // The same lattice through 0: ±|value index|. j is never INT64_MIN:
        // then Out, symmetric, would hold 2^64 slots and be wide (above).
        const imax j = detail::value_index<imax>(mag), a = j < 0 ? -j : j;
        return detail::from_value_index<Out>(sgn < 0 ? -a : a);
    } else {
        const rational a = beman::inside::detail::abs(rational{mag});
        return detail::store_value<Out>(sgn < 0 ? -a : a);
    }
}

// ⌊x⌋, ⌈x⌉, x rounded half away from zero, and x truncated toward zero.
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out floor_into(In x) {
    return detail::integer_into<detail::round_mode::floor, Out>(x);
}
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out ceil_into(In x) {
    return detail::integer_into<detail::round_mode::ceil, Out>(x);
}
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out round_into(In x) {
    return detail::integer_into<detail::round_mode::nearest, Out>(x);
}
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out trunc_into(In x) {
    return detail::integer_into<detail::round_mode::trunc, Out>(x);
}

namespace detail {
// Gate for fmod's integer fast path. When both operands and Out are
// integer-backed on commensurable notches, fmod collapses to ONE integer
// remainder in units of g = gcd(notch of InX, notch of InY): with x
// = a·g and y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ % is truncated division, the same
// convention). Conditions:
//   * integer raws only (rational raws keep the rational path),
//   * non-zero notches, g on Out's grid (g / notch of Out integer),
//   * Out's interval covers the remainders: ±min(max|x|, max|y|), x's sign,
//   * all unit counts fit comfortably in imax (headroom 4).
template <insidable Out, insidable InX, insidable InY>
inline constexpr bool fmod_int_fast = [] {
    if (!notched<InX> || !notched<InY> || !notched<Out>)
        return false;
    // (y != 0 is fmod_nonzero's precondition.) Lower/g must be an integer:
    // the lattices pass through 0.
    if (!anchored<InX> || !anchored<InY> || !anchored<Out>)
        return false;
    auto go = gcd(notch64<InX>, notch64<InY>);
    if (!go.has_value())
        return false;
    rational g = *go;
    // (try_ ops: a quotient past 64 bits makes the fast path inapplicable,
    // not the build fail.)
    auto qo = try_div(g, notch64<Out>);
    if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
    rational maxx = abs(lower64<InX>) > abs(upper64<InX>) ? abs(lower64<InX>) : abs(upper64<InX>);
    rational maxy = abs(lower64<InY>) > abs(upper64<InY>) ? abs(lower64<InY>) : abs(upper64<InY>);
    // Out holds every remainder: |r| ≤ min(max|x|, max|y|), with x's sign.
    const rational bound = maxx < maxy ? maxx : maxy;
    if (lower64<Out> > (lower64<InX> < 0 ? -bound : rational{0}) ||
        upper64<Out> < (upper64<InX> > 0 ? bound : rational{0}))
        return false;
    constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
    auto           ux  = try_div(maxx, g);
    auto           uy  = try_div(maxy, g);
    auto           uo  = try_div(maxy, notch64<Out>);
    return ux.has_value() && uy.has_value() && uo.has_value() && ux->Numerator <= lim && uy->Numerator <= lim &&
           uo->Numerator <= lim;
}();
} // namespace detail

// x mod y = x − trunc(x/y)·y (truncated division, matching std::fmod).
// Result has the sign of x. Pre: y != 0 (fmod_into checks it).
template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y) {
    // x − trunc(x/y)·y on exact values.
    auto exact = [&] {
        constexpr std::size_t E =
            2 * (detail::ax::input_limbs<InX> > detail::ax::input_limbs<InY> ? detail::ax::input_limbs<InX>
                                                                             : detail::ax::input_limbs<InY>)+1;
        using F = detail::wide_rational<E>;
        const F    a{detail::ax::exact_input(x)}, b{detail::ax::exact_input(y)};
        const auto q = (a.Num * b.Den) / (a.Den * b.Num); // truncated
        return detail::store_exact<Out>(a + F{-(q * b.Num), b.Den});
    };
    if constexpr (detail::any_wide_valued<Out, InX, InY>)
        return exact();
    else if constexpr (detail::fmod_int_fast<Out, InX, InY>) {
        // One integer remainder in g-units; bit-identical to the rational path.
        constexpr rational g   = *detail::gcd(detail::notch64<InX>, detail::notch64<InY>);
        constexpr imax     wx  = trunc((detail::notch64<InX> / g).value());
        constexpr imax     wy  = trunc((detail::notch64<InY> / g).value());
        constexpr imax     wo  = trunc((g / detail::notch64<Out>).value());
        constexpr imax     lox = trunc((detail::lower64<InX> / g).value()); // exact: grid invariant
        constexpr imax     loy = trunc((detail::lower64<InY> / g).value());
        constexpr imax     loo = trunc((detail::lower64<Out> / detail::notch64<Out>).value());
        const imax         a   = detail::raw_imax(x) * wx + (detail::index_storage<InX> ? lox : 0);
        const imax         b   = detail::raw_imax(y) * wy + (detail::index_storage<InY> ? loy : 0);
        const imax         r   = a % b; // |r| < |b|, in Out's range
        return Out::from_raw(detail::raw_from_offset<Out>(r * wo - loo));
    } else {
        // In 64-bit rationals while every step fits (the quotient within
        // imax); else exactly — the remainder itself always fits Out.
        const rational xv = x, yv = y;
        if (const auto q = try_div(xv, yv);
            q && *q < rational{std::numeric_limits<imax>::max()} && rational{std::numeric_limits<imax>::min()} < *q)
            if (const auto qy = try_mul(rational{trunc(*q)}, yv))
                if (const auto r = try_sub(xv, *qy)) [[likely]]
                    return detail::store_value<Out>(*r);
        return exact();
    }
}

// Like `/`: a plain Out when y's grid excludes 0, else expected<Out, errc>
// with division_by_zero for y == 0.
template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr auto fmod_into(InX x, InY y) {
    if constexpr (beman::inside::detail::divisor_excludes_zero<InY>)
        return fmod_nonzero<Out>(x, y);
    else {
        if (y == 0)
            return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
        return std::expected<Out, errc>{fmod_nonzero<Out>(x, y)};
    }
}

//---------------------------------------------------------------------------
// pown<E> — compile-time integer powers, pure grid arithmetic
//---------------------------------------------------------------------------
// Repeated squaring in inside-space: every multiply widens the result grid
// corner-correctly, so the result is exact for exact inputs and negative
// bases are fine. No engine — works on any inside
// (like abs/floor/fmod). Checked rational raws may return
// std::expected<inside, errc> per the usual arithmetic vocabulary. Negative
// exponents are deferred (they need the division error story).
namespace detail {
template <imax E, insidable In>
constexpr auto pown_impl(In x) noexcept {
    if constexpr (E == 0) {
        (void)x;
        return just<1>;
    } else if constexpr (E == 1)
        return x;
    else if constexpr (E % 2)
        return x * pown_impl<E - 1>(x);
    else {
        auto h = pown_impl<E / 2>(x);
        return h * h;
    }
}

// pown<E>(x) exists when each squaring and multiply does: a checked
// continuous product (an expected) does not multiply further.
template <imax E, class In>
inline constexpr bool pown_defined = [] {
    if constexpr (E < 0)
        return false;
    else if constexpr (E <= 1)
        return true;
    else if constexpr (E % 2 == 1) {
        if constexpr (!pown_defined<E - 1, In>)
            return false;
        else
            return requires(In x) { x * pown_impl<E - 1>(x); };
    } else {
        if constexpr (!pown_defined<E / 2, In>)
            return false;
        else
            return requires(In x) { pown_impl<E / 2>(x) * pown_impl<E / 2>(x); };
    }
}();
} // namespace detail

template <imax E, insidable In>
    requires detail::pown_defined<E, In>
[[nodiscard]] constexpr auto pown(In x) noexcept {
    return detail::pown_impl<E>(x);
}

//---------------------------------------------------------------------------
// Auto-deducing forms — algebraic tier.
//
// Each `fn_into<Out>(x)` has an auto form `fn(x)` that derives `Out` from `In`
// and delegates to it. abs and copysign take a notch symmetric about 0
// (sym_notch), fmod the gcd of both inputs' notches; floor/ceil/round/trunc
// deduce notch 1 since their outputs are integer-valued.
//---------------------------------------------------------------------------
template <insidable In>
[[nodiscard]] constexpr auto abs(In x) {
    return abs_into<detail::abs_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto sign(In x) {
    return sign_into<detail::sign_result_t<In>>(x);
}

template <insidable Mag, insidable Sgn>
[[nodiscard]] constexpr auto copysign(Mag mag, Sgn sgn) {
    return copysign_into<detail::copysign_result_t<Mag, Sgn>>(mag, sgn);
}

template <insidable In>
[[nodiscard]] constexpr auto floor(In x) {
    return floor_into<detail::floor_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto ceil(In x) {
    return ceil_into<detail::ceil_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto round(In x) {
    return round_into<detail::round_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto trunc(In x) {
    return trunc_into<detail::trunc_result_t<In>>(x);
}

namespace detail {
// fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
// both notches (x − k·y lies on that lattice, so the result is exact).
template <insidable InX, insidable InY>
inline constexpr grid_rational fmod_bound =
    abs_result_upper<InX> < abs_result_upper<InY> ? abs_result_upper<InX> : abs_result_upper<InY>;

template <insidable InX, insidable InY>
using fmod_result_t = inside<exact_out_grid({(lower_of<InX> < 0 ? -fmod_bound<InX, InY> : grid_rational{0}),
                                             (upper_of<InX> > 0 ? fmod_bound<InX, InY> : grid_rational{0})},
                                            ax::gcd_notch<InX, InY>,
                                            den_bound(grid_of<InX>) * den_bound(grid_of<InY>)),
                             out_policy<InX> | round_nearest>;
} // namespace detail

template <insidable InX, insidable InY>
[[nodiscard]] constexpr auto fmod(InX x, InY y) {
    return fmod_into<detail::fmod_result_t<InX, InY>>(x, y);
}

//---------------------------------------------------------------------------
// unit_amplitude<K> — amplitude grid [-1, 1] at 1/K resolution: a ready-made explicit
// output for sin / cos (`math::sin_into<math::unit_amplitude<32768>>(angle)`), decoupling
// the output precision from the angle's grid; results round to nearest. All
// angles are radians, as in <cmath>.
//---------------------------------------------------------------------------
template <std::uint64_t K>
using unit_amplitude = inside<{{rational{-1}, rational{1}}, per<K>}, round_nearest>;

//---------------------------------------------------------------------------
// The transcendentals: the adaptive engine.
//---------------------------------------------------------------------------
using adaptive::acos;
using adaptive::acos_into;
using adaptive::acosh;
using adaptive::acosh_into;
using adaptive::asin;
using adaptive::asin_into;
using adaptive::asinh;
using adaptive::asinh_into;
using adaptive::atan;
using adaptive::atan2;
using adaptive::atan2_into;
using adaptive::atan_into;
using adaptive::atanh;
using adaptive::atanh_into;
using adaptive::cbrt;
using adaptive::cbrt_into;
using adaptive::cos;
using adaptive::cos_into;
using adaptive::cosh;
using adaptive::cosh_into;
using adaptive::exp;
using adaptive::exp2;
using adaptive::exp2_into;
using adaptive::exp_into;
using adaptive::hypot;
using adaptive::hypot_into;
using adaptive::log;
using adaptive::log10;
using adaptive::log10_into;
using adaptive::log2;
using adaptive::log2_into;
using adaptive::log_into;
using adaptive::pow;
using adaptive::pow_base;
using adaptive::pow_base_into;
using adaptive::pow_into;
using adaptive::sin;
using adaptive::sin_into;
using adaptive::sinh;
using adaptive::sinh_into;
using adaptive::sqrt;
using adaptive::sqrt_into;
using adaptive::tan;
using adaptive::tan_into;
using adaptive::tanh;
using adaptive::tanh_into;
} // namespace beman::inside::math


#if !defined(BEMAN_INSIDE_NO_STRING) && !defined(BEMAN_INSIDE_NO_UNITS)

// ======================================================================
//  beman/inside/constants.hpp
// ======================================================================



// ======================================================================
//  beman/inside/units.hpp
// ======================================================================
// Opt-in: physical units and quantity kinds on inside — the main features of
// mp-units with an inside as the only representation.
//
//   quantity<si::metre, inside<{0, 10}>> d{7};      // 7 m
//   auto k = just<5> * si::kilo(si::metre);         // 5 km, a point grid
//   auto s = k + d;                                 // 5007 m, exact
//   auto v = d.in(si::milli(si::metre));            // 7000 mm, the same raw
//   quantity<isq::height[si::metre], H> h{…};       // a quantity kind
//
// A unit conversion is a grid rescale: km → m multiplies the interval and
// notch by 1000 and keeps the raw, so it is exact, total and free. A sum of
// two units runs in their common unit (the gcd of the magnitudes), so it is
// exact too. Products and quotients take inside's result grids.
//
// Compile time: dimensions, magnitudes, units, quantity specs and references
// are structural values combined by consteval functions — no type lists, no
// type-level sorting. The names a user writes (si::metre, isq::height) are
// empty tag objects whose type carries the value.
//
// Magnitudes are exact rationals times a power of π. A π magnitude (degree ↔
// radian) has no exact rescale: it rounds once, through value_cast with a
// target representation and a rounding flag (units_math.hpp).
//
// Also here:
//   quantity_point, point<U>(v), the origins     ℃ and ℉ points, exact shifts
//   quantity_cast, value_cast, try_value_cast    kinds and representations
//   named_unit, quantity_spec, point_origin,     units, kinds, origins and
//   base_dimension, unit_c / spec_c / origin_c   dimensions of your own
//   q.on_wrap / on_clamp / on_error / on_overflow, ++q, Q::try_make
//   mul_into, div_into, add_all, mul_all, sum<Q>, divmod, parse<Q>,
//   conversion_overflows / rounds / loses<Q>, clamp_cast / wrap_cast<Q>,
//   dot, cross, lerp                             inside's algorithms
//   percent, per_mille                           dimensionless units
//   to_string, operator<<, std::formatter        number, then the symbol
// The catalog: isq, si, si2019, international. The derived physical
// constants are in constants.hpp.
//---------------------------------------------------------------------------



// ======================================================================
//  beman/inside/io.hpp
// ======================================================================
// io — ALL of the library's string / stream / std::format support, gathered
// into one opt-in header. The core (inside.hpp and everything it pulls) never
// includes this, so a freestanding / bare-metal build that never includes
// "beman/inside/io.hpp" pays zero <string>/<ostream>/<format> cost. In the single-
// header amalgamation this whole region is wrapped in `#ifndef BEMAN_INSIDE_NO_STRING`,
// so defining BEMAN_INSIDE_NO_STRING drops it (and its heavy includes) wholesale.
//
// Provides:
//   * to_string(rational | interval | grid | insidable | arithmetic | errc)
//   * to_string_debug(insidable)        — value + raw + raw type + grid
//   * operator<< and operator>> (one token, failbit on error)
//   * parse<B, F>(std::string_view), parse_exact<B>(std::string_view)
//   * std::formatter<inside<G, P>>, std::formatter<rational> (with <format>)
//   * type_name_of<B>                   — a readable name of an inside type
// units.hpp builds on it (to_string and std::formatter of quantities).
//---------------------------------------------------------------------------



#include <algorithm>
#include <format>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace beman::inside {
namespace detail {
// Decimal digits of a wide integer: 19 digits per division by 10^19.
template <std::size_t N, bool S>
std::string wide_to_decimal(wide_int<N, S> v) {
    const bool         neg = v.negative();
    wide_int<N, false> u(neg ? -v : v);
    std::string        out;
    do {
        const auto [q, r] = divmod_small(u, 10'000'000'000'000'000'000ull);
        std::string part  = std::to_string(r);
        if (!q.is_zero())
            part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
        u = q;
    } while (!u.is_zero());
    return neg ? "-" + out : out;
}

// Decimal digits of num/den (den > 0) on demand: the integer part, then the
// fraction one chunk at a time. Tail() says whether anything nonzero is left,
// which rounding needs to tell a tie from a value above it.
template <std::size_t K>
struct decimal_stream {
    using W = wide_uint<K + 1>;
    W Rem, Den;

    decimal_stream(const wide_uint<K>& num, const wide_uint<K>& den) : Rem{num}, Den{den} {}

    std::string whole() {
        const auto [q, r] = W::divmod(Rem, Den);
        Rem               = r;
        return wide_to_decimal(q);
    }
    std::string next(int n) {
        std::string out;
        for (; n > 0; n -= 19) {
            const int chunk = n < 19 ? n : 19;
            umax      p     = 1;
            for (int i = 0; i < chunk; ++i)
                p *= 10;
            const auto [q, r]      = W::divmod(Rem * W{p}, Den);
            const std::string part = std::to_string(static_cast<umax>(q));
            out.append(static_cast<std::size_t>(chunk) - part.size(), '0') += part;
            Rem = r;
        }
        return out;
    }
    bool tail() const { return !Rem.is_zero(); }
};

// num/den (den > 0) in the forms parse reads: its exact decimal when the
// reduced denominator is 2^a·5^b (however many digits, and at least
// min_digits decimals), else "num/den".
template <std::size_t K>
std::string fraction_to_string(bool neg, wide_uint<K> num, wide_uint<K> den, int min_digits = 0) {
    using U   = wide_uint<K>;
    const U g = wide_gcd(den, num);
    num /= g;
    den /= g;
    std::string str   = neg && !num.is_zero() ? "-" : "";
    U           rest  = den;
    const int   twos  = static_cast<int>(strip_factor(rest, 2)); // ≤ the bit width
    const int   fives = static_cast<int>(strip_factor(rest, 5));
    if (!(rest == U{1}))
        return str += wide_to_decimal(num) + "/" + wide_to_decimal(den);
    // num·10^digits/den is an integer: its decimals end there.
    decimal_stream<K> ds{num, den};
    str += ds.whole();
    int digits = twos > fives ? twos : fives;
    if (digits < min_digits)
        digits = min_digits;
    if (digits != 0)
        str += '.' + ds.next(digits);
    return str;
}
} // namespace detail

//-------------------------------------------------------------------------
// to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
// for plain arithmetic types and the exact form for insidables. A value
// prints as its exact decimal when it has one, else as num/den.
//-------------------------------------------------------------------------
[[nodiscard]] inline std::string to_string(beman::inside::detail::rational r) {
    return detail::fraction_to_string<1>(r.Denominator < 0, r.Numerator, detail::abs_den(r.Denominator));
}

namespace detail {
// The decimals every value of B prints with: n for a decimal lattice — one
// whose value unit (the notch, or gcd(Notch, Lower) off 0) has a
// denominator 2^a·5^b with b ≥ 1 (per<100>, 0.05, 1e-18) — with
// n = max(a, b); else 0 (the value decides).
template <insidable B>
inline constexpr int fixed_decimals = [] {
    if constexpr (!notched<B>)
        return 0;
    else {
        grid_int  q    = wide_denominator(grid_of<B>.value_unit());
        const int two  = static_cast<int>(strip_factor(q, 2)); // ≤ the bit width
        const int five = static_cast<int>(strip_factor(q, 5));
        return q == grid_int{1} && five > 0 ? (two > five ? two : five) : 0;
    }
}();
} // namespace detail

// A grid number of any size, in the forms of to_string(rational).
[[nodiscard]] inline std::string to_string(const detail::big_rational& r);

[[nodiscard]] inline std::string to_string(interval ival) {
    std::string str{"["};

    str += beman::inside::to_string(ival.Lower);
    str += "..";
    str += beman::inside::to_string(ival.Upper);
    str += "]";
    return str;
}

[[nodiscard]] inline std::string to_string(grid g) {
    std::string str{"{"};

    str += beman::inside::to_string(g.Interval);
    str += ", ";
    str += beman::inside::to_string(g.Notch);
    if (g.MaxDenominator != 0) {
        str += ", den<=";
        str += beman::inside::to_string(g.MaxDenominator);
    }
    str += "}";
    return str;
}

// delegate to std::to_string
template <typename V>
    requires(std::integral<V> || std::floating_point<V>)
[[nodiscard]] auto to_string(V value) {
    return std::to_string(value);
}

//-------------------------------------------------------------------------
// type_name_of<T> — short raw-type label for to_string_debug. Lives here (not
// in numbers.hpp) so the core never pulls <string_view>.
//-------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr std::string_view type_name_of = wide_integer<T>         ? "wide_int"
                                                 : wide_rational_like<T> ? "wide_rational"
                                                                         : "unknown";
template <>
inline constexpr std::string_view type_name_of<std::uint8_t> = "uint8_t";
template <>
inline constexpr std::string_view type_name_of<std::uint16_t> = "uint16_t";
template <>
inline constexpr std::string_view type_name_of<std::uint32_t> = "uint32_t";
template <>
inline constexpr std::string_view type_name_of<std::uint64_t> = "uint64_t";
template <>
inline constexpr std::string_view type_name_of<std::int8_t> = "int8_t";
template <>
inline constexpr std::string_view type_name_of<std::int16_t> = "int16_t";
template <>
inline constexpr std::string_view type_name_of<std::int32_t> = "int32_t";
template <>
inline constexpr std::string_view type_name_of<std::int64_t> = "int64_t";
template <>
inline constexpr std::string_view type_name_of<rational> = "rational";
template <>
inline constexpr std::string_view type_name_of<point_slot> = "point";
template <>
inline constexpr std::string_view type_name_of<wide_uint<2>> = "wide_uint<2>";
template <>
inline constexpr std::string_view type_name_of<wide_uint<3>> = "wide_uint<3>";
} // namespace detail

//-------------------------------------------------------------------------
// to_string / to_string_debug / operator<< for insidables and rational.
// The debug form also prints the raw value, raw type, and grid — useful when
// inspecting failing tests or storage choices.
//-------------------------------------------------------------------------
namespace detail {
// An exact value, with at least min_digits decimals.
template <std::size_t K>
std::string exact_to_string(wide_rational<K> f, int min_digits = 0) {
    const bool neg = f.Num.negative();
    return fraction_to_string(neg, wide_uint<K>{neg ? -f.Num : f.Num}, wide_uint<K>{f.Den}, min_digits);
}
} // namespace detail

template <std::size_t N, bool S>
[[nodiscard]] inline std::string to_string(detail::wide_int<N, S> v) {
    return detail::wide_to_decimal(v);
}

// A grid number of any size, in decimal (reads the interned limbs; no new
// big values are formed at runtime).
[[nodiscard]] inline std::string to_string(const detail::big_int& v) {
    detail::big::mag m = v.magnitude();
    std::string      out;
    do {
        // m /= 10^19, rem = m % 10^19
        const umax rem = detail::limb::div_1(m.data(), m.data(), m.size(), umax{10'000'000'000'000'000'000ull});
        detail::big::trim(m);
        std::string part = std::to_string(rem);
        if (!m.empty())
            part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
    } while (!m.empty());
    return v.negative() ? "-" + out : out;
}

namespace detail {
template <std::size_t K>
std::string big_fraction_to_string(const big_rational& r) {
    const auto n = static_cast<wide_uint<K>>(r.Num); // two's complement
    return fraction_to_string(r.Num.negative(), r.Num.negative() ? -n : n, static_cast<wide_uint<K>>(r.Den));
}
} // namespace detail

[[nodiscard]] inline std::string to_string(const detail::big_rational& r) {
    const int bits = r.Num.bit_width() > r.Den.bit_width() ? r.Num.bit_width() : r.Den.bit_width();
    if (bits < 512)
        return detail::big_fraction_to_string<8>(r);
    if (bits < 4096)
        return detail::big_fraction_to_string<64>(r);
    const std::string num = beman::inside::to_string(r.Num);
    return r.is_integer() ? num : num + "/" + beman::inside::to_string(r.Den);
}

// An inside's exact value: with a decimal notch, every value prints that
// notch's decimals (19.90, 2.00); otherwise its shortest exact form — a
// decimal when it has one, else N/D.
template <insidable B>
[[nodiscard]] inline std::string to_string(B b) {
    constexpr int n = detail::fixed_decimals<B>;
    if constexpr (detail::wide_valued<B>)
        return detail::exact_to_string(detail::exact_of(b), n);
    else {
        const detail::rational r = detail::as_rational(b);
        return detail::fraction_to_string<1>(r.Denominator < 0, r.Numerator, detail::abs_den(r.Denominator), n);
    }
}

template <insidable B>
[[nodiscard]] inline std::string to_string_debug(B b) {
    std::string str;
    str += beman::inside::to_string(b);
    str += " {";
    if constexpr (detail::fraction_storage<B>)
        str += detail::exact_to_string(b.raw());
    else
        str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name_of<detail::raw_t<B>>);
    constexpr auto slots = grid_of<B>.slot_count();
    str += " Max:" + beman::inside::to_string(slots) + "] ";
    str += beman::inside::to_string(grid_of<B>);
    str += "}";
    return str;
}

inline std::ostream& operator<<(std::ostream& stream, beman::inside::detail::rational r) {
    stream << beman::inside::to_string(r);
    return stream;
}

// An error code as its message: "division by zero".
[[nodiscard]] inline std::string to_string(errc e) { return errc_message(e); }

inline std::ostream& operator<<(std::ostream& stream, errc e) { return stream << errc_message(e); }

template <insidable B>
inline std::ostream& operator<<(std::ostream& stream, B b) {
    stream << beman::inside::to_string(b);
    return stream;
}

// parse<B, F>(text) / parse_exact<B>(text) — the std::string_view
// forms of the pointer-pair overloads.
template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> parse(std::string_view text) {
    return parse<B, F>(text.data(), text.data() + text.size());
}
template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> parse_exact(std::string_view text) {
    return parse_exact<B>(text.data(), text.data() + text.size());
}

// Reads one whitespace-delimited token and parses it with parse<B>. On an
// error the stream's failbit is set and `b` is left unchanged.
template <insidable B>
inline std::istream& operator>>(std::istream& stream, B& b) {
    std::string token;
    if (!(stream >> token))
        return stream;
    if (const auto r = parse<B>(token); r)
        b = *r;
    else
        stream.setstate(std::ios_base::failbit);
    return stream;
}

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// Shared spec handling: an empty `{}` is left to the derived format() (exact
// to_string); a non-empty spec is parsed and applied by `Numeric`.
template <class Inner>
struct numeric_spec_formatter {
    Inner Numeric{};
    bool  HasSpec = false;

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            HasSpec = true;
            return Numeric.parse(ctx);
        }
        return it;
    }
};

// Keep the first `keep` digits of the magnitude `d` (digits[keep..] and a
// nonzero tail are what is cut), rounded by `mode` for a value of the given
// sign. A carry out of the first digit prepends a '1'; returns whether it did.
inline bool round_digits(std::string& d, std::size_t keep, bool tail, round_mode mode, bool negative) {
    const bool more = tail || d.find_first_not_of('0', keep + 1) != std::string::npos;
    const char cut  = keep < d.size() ? d[keep] : '0';
    d.resize(keep);
    const bool inexact = cut != '0' || more;
    bool       up      = false;
    switch (mode) {
    case round_mode::trunc:
        break;
    case round_mode::floor:
        up = negative && inexact;
        break;
    case round_mode::ceil:
        up = !negative && inexact;
        break;
    case round_mode::nearest: // ties away from zero
        up = cut >= '5';
        break;
    case round_mode::half_even:
        up = cut > '5' || (cut == '5' && (more || (keep > 0 && (d[keep - 1] - '0') % 2 == 1)));
        break;
    }
    if (!up)
        return false;
    for (std::size_t i = keep; i-- > 0;) {
        if (d[i] != '9') {
            ++d[i];
            return false;
        }
        d[i] = '0';
    }
    d.insert(d.begin(), '1');
    return true;
}

// A std::format spec for exact values: [[fill]align][sign][#][0][width]
// [.precision][type], type one of f F e E g G (or none). The digits come from
// the exact value, rounded once by the type's rounding mode (display_rounding),
// so wide and big values print correctly.
struct exact_format_spec {
    char Fill = ' ', Align = 0, Sign = '-', Type = 0;
    bool Alt = false, Zero = false;
    int  Width = 0, Precision = -1;

    constexpr auto parse(std::format_parse_context& ctx) {
        auto       it       = ctx.begin();
        const auto end      = ctx.end();
        auto       is_align = [](char c) { return c == '<' || c == '>' || c == '^'; };
        auto       number   = [&](int& v) {
            for (v = 0; it != end && *it >= '0' && *it <= '9'; ++it)
                v = v * 10 + (*it - '0');
        };
        if (it != end && it + 1 != end && is_align(it[1]) && *it != '{' && *it != '}') {
            Fill  = *it;
            Align = it[1];
            it += 2;
        } else if (it != end && is_align(*it))
            Align = *it++;
        if (it != end && (*it == '+' || *it == '-' || *it == ' '))
            Sign = *it++;
        if (it != end && *it == '#') {
            Alt = true;
            ++it;
        }
        if (it != end && *it == '0') {
            Zero = true;
            ++it;
        }
        number(Width);
        if (it != end && *it == '.') {
            ++it;
            if (it == end || *it < '0' || *it > '9')
                throw std::format_error("inside: precision needs digits");
            number(Precision);
        }
        if (it != end && std::string_view{"fFeEgG"}.find(*it) != std::string_view::npos)
            Type = *it++;
        if (it != end && *it != '}')
            throw std::format_error("inside: unsupported format spec (use f, e or g with fill, align, sign, #, 0, "
                                    "width and precision)");
        return it;
    }

    // v's shortest form (to_string) without its sign, which body() reads only
    // without a type or precision (else empty).
    template <class V>
    std::string plain_text(const V& v) const {
        if (Type != 0 || Precision >= 0)
            return {};
        std::string s = beman::inside::to_string(v);
        if (!s.empty() && s[0] == '-')
            s.erase(0, 1);
        return s;
    }

    // An exact value's text by the spec, then written with its sign.
    template <std::size_t K, typename Ctx>
    auto write_exact(const wide_rational<K>& f, std::string_view plain, round_mode mode, Ctx& ctx) const {
        const bool neg = f.Num.negative();
        return write(neg, body(wide_uint<K>{abs(f.Num)}, wide_uint<K>{f.Den}, plain, neg, mode), ctx);
    }

    // The value's text: `plain` (to_string) for no type and no precision,
    // else the digits the spec asks for.
    template <std::size_t K>
    std::string body(const wide_uint<K>& num,
                     const wide_uint<K>& den,
                     std::string_view    plain,
                     bool                negative,
                     round_mode          mode) const {
        if (Type == 0 && Precision < 0)
            return std::string{plain};
        const char t     = Type == 0 ? 'g' : static_cast<char>(Type | 0x20); // lower case
        const bool upper = Type == 'F' || Type == 'E' || Type == 'G';
        int        p     = Precision < 0 ? 6 : Precision;
        if (t == 'f')
            return fixed(num, den, p, negative, mode);
        if (t == 'g') {
            p = p == 0 ? 1 : p;
            // The exponent after rounding to p significant digits picks the form.
            int         x = 0;
            const auto  e = scientific(num, den, p - 1, x, negative, mode);
            std::string out;
            if (x < p && x >= -4)
                out = fixed(num, den, p - 1 - x, negative, mode);
            else
                out = e;
            if (!Alt && out.find('.') != std::string::npos) {
                const std::size_t ep = out.find('e');
                std::string       m = out.substr(0, ep), tail = ep == std::string::npos ? "" : out.substr(ep);
                m.erase(m.find_last_not_of('0') + 1);
                if (m.back() == '.')
                    m.pop_back();
                out = m + tail;
            }
            return upper ? to_upper(out) : out;
        }
        int               x   = 0;
        const std::string out = scientific(num, den, p, x, negative, mode);
        return upper ? to_upper(out) : out;
    }

    // Sign, then fill and alignment (numbers align right; `0` pads after the sign).
    template <typename Ctx>
    auto write(bool negative, std::string text, Ctx& ctx) const {
        std::string sign = negative ? "-" : Sign == '+' ? "+" : Sign == ' ' ? " " : "";
        std::size_t len  = sign.size() + text.size();
        std::size_t pad  = static_cast<std::size_t>(Width) > len ? static_cast<std::size_t>(Width) - len : 0;
        std::string out;
        if (Zero && Align == 0)
            out = sign + std::string(pad, '0') + text;
        else if (Align == '<')
            out = sign + text + std::string(pad, Fill);
        else if (Align == '^')
            out = std::string(pad / 2, Fill) + sign + text + std::string(pad - pad / 2, Fill);
        else
            out = std::string(pad, Fill) + sign + text;
        return std::format_to(ctx.out(), "{}", out);
    }

  private:
    static std::string to_upper(std::string s) {
        std::ranges::replace(s, 'e', 'E');
        return s;
    }

    template <std::size_t K>
    std::string fixed(const wide_uint<K>& num, const wide_uint<K>& den, int p, bool negative, round_mode mode) const {
        decimal_stream<K> ds{num, den};
        const std::string whole = ds.whole();
        std::string       d     = whole + ds.next(p + 1);
        round_digits(d, whole.size() + static_cast<std::size_t>(p), ds.tail(), mode, negative); // a carry lengthens d
        std::string out = d.substr(0, d.size() - static_cast<std::size_t>(p));
        if (p > 0 || Alt)
            out += '.';
        return out + d.substr(d.size() - static_cast<std::size_t>(p));
    }

    // d.ddd…e±XX with p decimals; x receives the exponent.
    template <std::size_t K>
    std::string
    scientific(const wide_uint<K>& num, const wide_uint<K>& den, int p, int& x, bool negative, round_mode mode) const {
        decimal_stream<K> ds{num, den};
        std::string       d = ds.whole(); // from the first significant digit on
        x                   = static_cast<int>(d.size()) - 1;
        if (d == "0" && !num.is_zero())
            for (x = 0; d == "0"; --x) // the first nonzero digit of the fraction
                d = ds.next(1);
        if (static_cast<int>(d.size()) < p + 2)
            d += ds.next(p + 2 - static_cast<int>(d.size()));
        if (round_digits(d, static_cast<std::size_t>(p) + 1, ds.tail(), mode, negative)) {
            ++x;
            d.pop_back();
        }
        std::string out = d.substr(0, 1);
        if (p > 0 || Alt)
            out += '.';
        out += d.substr(1);
        const int ax = x < 0 ? -x : x;
        return out + (x < 0 ? "e-" : "e+") + (ax < 10 ? "0" : "") + std::to_string(ax);
    }
};

// The rounding a format spec applies to B: B's own mode when its policy
// names one (as storing at that precision would round), else ties to even.
template <insidable B>
inline constexpr round_mode display_rounding =
    has_any_flag(flags_of<B>, round_floor | round_ceil | round_nearest | round_half_even | snap)
        ? rounding_of(flags_of<B>)
        : round_mode::half_even;
} // namespace beman::inside::detail

// Empty `{}` prints to_string. A notched integer grid within imax takes the
// integer specs (std::formatter<imax>: {:>4}, {:#x}); every other inside takes
// the exact specs ({:.2f}, {:e}, {:g}, fill / align / sign / width), rounded
// from the exact value by the type's rounding mode (ties to even without one).
template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
    : beman::inside::detail::numeric_spec_formatter<
          std::conditional_t<beman::inside::detail::integer_lattice<beman::inside::inside<G, P>> &&
                                 beman::inside::detail::values_fit_imax<beman::inside::inside<G, P>>,
                             std::formatter<beman::inside::imax>,
                             beman::inside::detail::exact_format_spec>> {
    using B = beman::inside::inside<G, P>;
    // Integer formatting only for a notched integer grid: a continuous grid
    // (notch 0) holds fractions even between integer bounds.
    static constexpr bool integer_path =
        beman::inside::detail::integer_lattice<B> && beman::inside::detail::values_fit_imax<B>;

    template <typename Ctx>
    auto format(const B& b, Ctx& ctx) const {
        namespace d = beman::inside::detail;
        if constexpr (integer_path)
            return this->Numeric.format(d::to_value(b), ctx);
        else if (!this->HasSpec)
            return std::format_to(ctx.out(), "{}", beman::inside::to_string(b));
        else
            return this->Numeric.write_exact(d::exact_of(b), this->Numeric.plain_text(b), d::display_rounding<B>, ctx);
    }
};

template <>
struct std::formatter<beman::inside::detail::rational>
    : beman::inside::detail::numeric_spec_formatter<beman::inside::detail::exact_format_spec> {
    template <typename Ctx>
    auto format(const beman::inside::detail::rational& r, Ctx& ctx) const {
        namespace d = beman::inside::detail;
        if (!HasSpec)
            return std::format_to(ctx.out(), "{}", beman::inside::to_string(r));
        return Numeric.write_exact(d::exact_of(r), Numeric.plain_text(r), d::round_mode::half_even, ctx);
    }
};


#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// The diagnostic of a π conversion asked to be exact (rescale, in()).
#define BEMAN_INSIDE_UNITS_WHY_PI                                                                  \
    "units: this conversion multiplies by a power of pi, which has no exact grid - round it with " \
    "value_cast<unit, Rep>(q) from <beman/inside/units_math.hpp>, Rep declared with a rounding mode"

namespace beman::inside::units {
using beman::inside::detail::grid_rational;

//---------------------------------------------------------------------------
// dimension — the exponents of the seven SI base dimensions
// (L M T I Θ N J), and of up to four base dimensions you define
// (base_dimension: currency, information). Angle is dimensionless, as in SI.
//---------------------------------------------------------------------------
struct base_dimension;

namespace detail {
inline constexpr std::size_t base_dimensions     = 7;
inline constexpr std::size_t max_user_dimensions = 4;

// A user base dimension to a power. By content (its name), as specs and units.
struct user_exponent {
    const base_dimension* D{};
    std::int8_t           Exp{};
};
} // namespace detail

// A base dimension of your own: inline constexpr base_dimension currency{"currency"}.
struct base_dimension {
    std::string_view Name;
};

struct dimension {
    std::int8_t           Exp[detail::base_dimensions]{};
    detail::user_exponent User[detail::max_user_dimensions]{}; // sorted by name, no zero exponents
    std::int8_t           UserSize{0};

    // An exponent past ±127 is an error, not a wrap.
    static constexpr std::int8_t exponent(int e) {
        if (e < -127 || e > 127)
            beman::inside::detail::constexpr_error<"units: a dimension exponent past 127">();
        return static_cast<std::int8_t>(e);
    }

    friend constexpr bool operator==(const dimension& a, const dimension& b) {
        return std::ranges::equal(a.Exp, b.Exp) && a.UserSize == b.UserSize &&
               std::equal(a.User, a.User + a.UserSize, b.User, [](const auto& x, const auto& y) {
                   return x.D->Name == y.D->Name && x.Exp == y.Exp;
               });
    }
    friend consteval dimension operator*(dimension a, const dimension& b) {
        for (std::size_t i = 0; i < detail::base_dimensions; ++i)
            a.Exp[i] = exponent(a.Exp[i] + b.Exp[i]);
        for (std::int8_t i = 0; i < b.UserSize; ++i)
            a.add_user(b.User[i].D, b.User[i].Exp);
        return a;
    }
    friend consteval dimension operator/(const dimension& a, const dimension& b) { return a * b.pow(-1); }
    consteval dimension        pow(int n) const {
        if (n == 0)
            return dimension{};
        dimension r = *this;
        for (auto& e : r.Exp)
            e = exponent(e * n);
        for (std::int8_t i = 0; i < r.UserSize; ++i)
            r.User[i].Exp = exponent(r.User[i].Exp * n);
        return r;
    }
    // The n-th root; every exponent must divide.
    consteval dimension root(int n) const {
        dimension  r    = *this;
        const auto take = [n](std::int8_t& e) {
            if (e % n != 0)
                beman::inside::detail::constexpr_error<"units: the root of a dimension needs exponents it divides">();
            e = static_cast<std::int8_t>(e / n);
        };
        for (auto& e : r.Exp)
            take(e);
        for (std::int8_t i = 0; i < r.UserSize; ++i)
            take(r.User[i].Exp);
        return r;
    }

    // b^e merged in: kept sorted by name, an exponent reaching 0 drops out.
    constexpr void add_user(const base_dimension* b, int e);
};

namespace detail {
// x^e merged into the sorted items[0, size): an item `same` as x takes the
// summed exponent and drops out at 0; otherwise x goes in at its place by
// `before`. False when items is full.
template <class T, std::size_t N, class Same, class Before>
constexpr bool sorted_add(T (&items)[N], std::int8_t& size, T x, int e, Same same, Before before) {
    T* const end = items + size;
    if (T* const it = std::find_if(items, end, [&](const T& y) { return same(y, x); }); it != end) {
        it->Exp = dimension::exponent(it->Exp + e);
        if (it->Exp == 0) {
            std::shift_left(it, end, 1);
            items[--size] = {};
        }
        return true;
    }
    if (e == 0)
        return true;
    if (size == static_cast<std::int8_t>(N))
        return false;
    x.Exp          = dimension::exponent(e);
    std::int8_t at = size;
    while (at > 0 && before(x, items[at - 1])) {
        items[at] = items[at - 1];
        --at;
    }
    items[at] = x;
    ++size;
    return true;
}
} // namespace detail

constexpr void dimension::add_user(const base_dimension* b, int e) {
    auto name = [](const detail::user_exponent& u) { return u.D->Name; };
    if (!detail::sorted_add(
            User,
            UserSize,
            detail::user_exponent{b, 0},
            e,
            [&](const auto& a, const auto& c) { return name(a) == name(c); },
            [&](const auto& a, const auto& c) { return name(a) < name(c); }))
        beman::inside::detail::constexpr_error<"units: a dimension holds at most 4 base dimensions of your own">();
}

// The dimension of a base dimension of your own (its exponent 1).
constexpr dimension dimension_of(const base_dimension& b) {
    dimension d{};
    d.add_user(&b, 1);
    return d;
}

inline constexpr dimension dim_one{};
inline constexpr dimension dim_length{{1, 0, 0, 0, 0, 0, 0}};
inline constexpr dimension dim_mass{{0, 1, 0, 0, 0, 0, 0}};
inline constexpr dimension dim_time{{0, 0, 1, 0, 0, 0, 0}};
inline constexpr dimension dim_electric_current{{0, 0, 0, 1, 0, 0, 0}};
inline constexpr dimension dim_thermodynamic_temperature{{0, 0, 0, 0, 1, 0, 0}};
inline constexpr dimension dim_amount_of_substance{{0, 0, 0, 0, 0, 1, 0}};
inline constexpr dimension dim_luminous_intensity{{0, 0, 0, 0, 0, 0, 1}};

//---------------------------------------------------------------------------
// magnitude — Ratio·π^Pi, exact.
//---------------------------------------------------------------------------
struct magnitude {
    grid_rational Ratio{1};
    std::int8_t   Pi{0};

    friend constexpr bool      operator==(const magnitude&, const magnitude&) = default;
    friend consteval magnitude operator*(const magnitude& a, const magnitude& b) {
        return {a.Ratio * b.Ratio, static_cast<std::int8_t>(a.Pi + b.Pi)};
    }
    friend consteval magnitude operator/(const magnitude& a, const magnitude& b) {
        return {a.Ratio / b.Ratio, static_cast<std::int8_t>(a.Pi - b.Pi)};
    }
    consteval magnitude pow(int n) const {
        magnitude r{};
        for (int i = 0; i < (n < 0 ? -n : n); ++i)
            r = r * *this;
        return n < 0 ? magnitude{} / r : r;
    }
    constexpr bool is_one() const { return Ratio == grid_rational{1} && Pi == 0; }
};

// mag<r> — a magnitude tag: mag<1000> * si::metre, mag<just<1, 3>> * …
template <magnitude M>
struct mag_t {
    static constexpr magnitude value = M;
};
template <auto R>
inline constexpr mag_t<magnitude{grid_rational{R}}>    mag{};
inline constexpr mag_t<magnitude{grid_rational{1}, 1}> mag_pi{};

template <magnitude A, magnitude B>
consteval auto operator*(mag_t<A>, mag_t<B>) {
    return mag_t<A * B>{};
}
template <magnitude A, magnitude B>
consteval auto operator/(mag_t<A>, mag_t<B>) {
    return mag_t<A / B>{};
}

//---------------------------------------------------------------------------
// quantity_spec — a quantity kind (length, height, speed, energy, torque…).
// Named specs are static objects; the tags users write (isq::height) carry a
// pointer to one. A spec has a parent (height → altitude → length) or a
// recipe (speed = length / time), or both (velocity: a speed whose recipe is
// displacement / duration). IsKind starts a separate kind under its parent:
// it never converts implicitly across that boundary (angular_measure vs a
// dimensionless ratio).
//---------------------------------------------------------------------------
struct quantity_spec;

// An optional reference to a static object (a parent spec, a unit's kind, an
// origin). It is tested by its flag, never against nullptr, and compared by
// content, never by address: under GCC's -fsanitize=undefined `&obj ==
// nullptr`, and under -fsanitize=pointer-compare `&a == &b`, is no constant
// expression.
namespace detail {
template <class T>
struct ref_or_none {
    const T* P{};
    bool     Set{false};

    constexpr ref_or_none() = default;
    constexpr ref_or_none(std::nullptr_t) {}
    constexpr ref_or_none(const T* p) : P{p}, Set{true} {} // p is not null
    constexpr explicit operator bool() const { return Set; }
    constexpr const T* operator->() const { return P; }
};

using spec_ref = ref_or_none<quantity_spec>;

struct spec_factor {
    const quantity_spec*  Spec{};
    std::int8_t           Exp{};
    friend constexpr bool operator==(const spec_factor& a, const spec_factor& b);
};

inline constexpr std::size_t max_spec_factors = 6;

// A product of named specs to integer powers, sorted by name.
struct recipe {
    spec_factor           F[max_spec_factors]{};
    std::int8_t           Size{0};
    friend constexpr bool operator==(const recipe& a, const recipe& b) {
        return a.Size == b.Size && std::equal(a.F, a.F + a.Size, b.F);
    }
};

// An anonymous spec expression: what length / time is, before it is named.
struct spec_expr {
    recipe    Recipe{};
    dimension Dim{};
};
} // namespace detail

struct is_kind_t {
    explicit is_kind_t() = default;
};
inline constexpr is_kind_t is_kind{};

template <const quantity_spec* S>
struct spec_t;

struct quantity_spec {
    std::string_view Name;
    dimension        Dim{};
    detail::spec_ref Parent{};
    detail::recipe   Recipe{};
    bool             IsKind{false};

    // A base quantity: a dimension of its own.
    constexpr quantity_spec(std::string_view name, dimension d) : Name{name}, Dim{d} {}
    // A base quantity of a base dimension of your own (money: currency).
    constexpr quantity_spec(std::string_view name, const base_dimension& b) : Name{name}, Dim{dimension_of(b)} {}
    // A child of a named spec.
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>) : Name{name}, Dim{P->Dim}, Parent{P} {}
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, is_kind_t)
        : Name{name}, Dim{P->Dim}, Parent{P}, IsKind{true} {}
    // A derived root: named by its recipe.
    constexpr quantity_spec(std::string_view name, const detail::spec_expr& e)
        : Name{name}, Dim{e.Dim}, Recipe{e.Recipe} {}
    constexpr quantity_spec(std::string_view name, const detail::spec_expr& e, is_kind_t)
        : Name{name}, Dim{e.Dim}, Recipe{e.Recipe}, IsKind{true} {}
    // A child with a recipe of its own (velocity: speed, displacement / duration).
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, const detail::spec_expr& e)
        : Name{name}, Dim{P->Dim}, Parent{P}, Recipe{e.Recipe} {
        if (!(e.Dim == P->Dim))
            beman::inside::detail::constexpr_error<"units: a spec's recipe must have its parent's dimension">();
    }
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, const detail::spec_expr& e, is_kind_t)
        : quantity_spec{name, spec_t<P>{}, e} {
        IsKind = true;
    }
};

namespace detail {
// Identity by content (see ref_or_none). A spec is its name, dimension, recipe, kind boundary and parent chain: a
// user spec that reuses a name ("length") under another parent is another
// spec.
constexpr bool same(const quantity_spec* a, const quantity_spec* b) { // both set
    if (a->Name != b->Name || !(a->Dim == b->Dim) || a->IsKind != b->IsKind || !(a->Recipe == b->Recipe))
        return false;
    if (!a->Parent || !b->Parent)
        return !a->Parent && !b->Parent;
    return same(a->Parent.P, b->Parent.P);
}
constexpr bool same(const spec_ref& a, const spec_ref& b) {
    if (!a || !b)
        return !a && !b;
    return same(a.P, b.P);
}

constexpr bool operator==(const spec_factor& a, const spec_factor& b) {
    return same(a.Spec, b.Spec) && a.Exp == b.Exp;
}

// The spec's kind root: the first IsKind spec on the way up, else the root.
consteval const quantity_spec* kind_root(const quantity_spec* s) {
    while (!s->IsKind && s->Parent)
        s = s->Parent.P;
    return s;
}
// a is s or an ancestor of s within s's kind (not above an IsKind boundary).
consteval bool is_within_kind_ancestor(const quantity_spec* s, const quantity_spec* a) {
    for (;;) {
        if (same(s, a))
            return true;
        if (s->IsKind || !s->Parent)
            return false;
        s = s->Parent.P;
    }
}

// r · f^e, merged and kept sorted by name; an exponent that reaches 0 drops
// out. Returns false when the recipe is full.
consteval bool recipe_add(recipe& r, const quantity_spec* f, int e) {
    return sorted_add(
        r.F,
        r.Size,
        spec_factor{f, 0},
        e,
        [](const spec_factor& a, const spec_factor& b) { return same(a.Spec, b.Spec); },
        [](const spec_factor& a, const spec_factor& b) { return a.Spec->Name < b.Spec->Name; });
}

// a · b^sign; false when the product does not fit a recipe.
consteval bool recipe_merge(recipe& a, const recipe& b, int sign) {
    for (std::int8_t i = 0; i < b.Size; ++i)
        if (!recipe_add(a, b.F[i].Spec, b.F[i].Exp * sign))
            return false;
    return true;
}

consteval recipe recipe_of(const quantity_spec* s) {
    if (s->Recipe.Size > 0)
        return s->Recipe;
    recipe r{};
    recipe_add(r, s, 1);
    return r;
}

// The recipe with every factor replaced by its kind root, summed.
consteval recipe recipe_by_kind(const recipe& r) {
    recipe out{};
    for (std::int8_t i = 0; i < r.Size; ++i)
        recipe_add(out, kind_root(r.F[i].Spec), r.F[i].Exp);
    return out;
}

// Each factor of `want` matched by a factor of `have` with the same exponent
// that is it or descends from it, one to one.
consteval bool recipe_matches(const recipe& have, const recipe& want) {
    if (have.Size != want.Size)
        return false;
    bool used[max_spec_factors]{};
    for (std::int8_t i = 0; i < want.Size; ++i) {
        bool found = false;
        for (std::int8_t j = 0; j < have.Size && !found; ++j)
            if (!used[j] && have.F[j].Exp == want.F[i].Exp &&
                is_within_kind_ancestor(have.F[j].Spec, want.F[i].Spec)) {
                used[j] = true;
                found   = true;
            }
        if (!found)
            return false;
    }
    return true;
}
} // namespace detail

template <const quantity_spec* S>
struct spec_t {
    static constexpr const quantity_spec* value = S;
    constexpr operator detail::spec_ref() const { return S; } // unit_options{.kind = isq::frequency}
    // isq::height[si::metre] — defined after unit_t and reference_t.
    template <class U>
    consteval auto operator[](U) const;
};

// Spec algebra for recipes: length / time, pow<2>(length), inverse(radius).
template <const quantity_spec* S>
consteval detail::spec_expr expr_of(spec_t<S>) {
    detail::spec_expr e{};
    detail::recipe_add(e.Recipe, S, 1);
    e.Dim = S->Dim;
    return e;
}
consteval detail::spec_expr expr_of(const detail::spec_expr& e) { return e; }

template <class T>
concept spec_operand = requires(T t) { expr_of(t); };

namespace detail {
consteval spec_expr spec_combine(const spec_expr& a, const spec_expr& b, int sign) {
    spec_expr r = a;
    if (!recipe_merge(r.Recipe, b.Recipe, sign))
        beman::inside::detail::constexpr_error<"units: a recipe holds at most 6 factors">();
    r.Dim = sign > 0 ? a.Dim * b.Dim : a.Dim / b.Dim;
    return r;
}
} // namespace detail

template <spec_operand A, spec_operand B>
consteval detail::spec_expr operator*(A a, B b) {
    return detail::spec_combine(expr_of(a), expr_of(b), 1);
}
template <spec_operand A, spec_operand B>
consteval detail::spec_expr operator/(A a, B b) {
    return detail::spec_combine(expr_of(a), expr_of(b), -1);
}
// Two specs are equal when their expressions are: one named spec, or one
// recipe of named specs (isq::length / isq::duration == isq::length / isq::time).
template <spec_operand A, spec_operand B>
consteval bool operator==(A a, B b) {
    const detail::spec_expr x = expr_of(a), y = expr_of(b);
    return x.Recipe == y.Recipe && x.Dim == y.Dim;
}

template <int N, spec_operand A>
consteval detail::spec_expr pow(A a) {
    detail::spec_expr r{};
    for (int i = 0; i < (N < 0 ? -N : N); ++i)
        r = detail::spec_combine(r, expr_of(a), N < 0 ? -1 : 1);
    return r;
}
template <spec_operand A>
consteval detail::spec_expr inverse(A a) {
    return detail::spec_combine(detail::spec_expr{}, expr_of(a), -1);
}

struct named_unit;

//---------------------------------------------------------------------------
// point_origin — where a quantity_point's number counts from. An absolute
// origin (absolute zero) or one at an exact offset from one, in the coherent
// SI unit of the dimension (the ice point: absolute zero + 273.15 K).
//---------------------------------------------------------------------------
struct point_origin {
    std::string_view                  Name;
    dimension                         Dim{};
    detail::ref_or_none<point_origin> Absolute{}; // none for an absolute origin
    grid_rational                     Offset{0};  // from Absolute, in the coherent unit
};

namespace detail {
using origin_ref = ref_or_none<point_origin>; // none: the implicit zero of a dimension
} // namespace detail

template <detail::origin_ref O>
struct origin_t {
    static constexpr detail::origin_ref value = O;
    constexpr operator detail::origin_ref() const { return O; } // unit_options{.origin = si::ice_point}
};

namespace detail {
consteval origin_ref absolute_of(origin_ref o) { return o && o->Absolute ? o->Absolute : o; }
// By content, as same() for specs and units (see there).
constexpr bool same(const origin_ref& a, const origin_ref& b) {
    if (!a || !b)
        return !a && !b;
    return a->Name == b->Name && a->Dim == b->Dim && a->Offset == b->Offset && same(a->Absolute, b->Absolute);
}
consteval grid_rational offset_of(origin_ref o) { return o && o->Absolute ? o->Offset : grid_rational{0}; }
} // namespace detail

// A named unit's options, all optional (designated, in this order):
//   .ascii         the ASCII symbol ("ohm" for Ω), else the symbol itself
//   .kind          the quantity kind it measures, else its definition's
//   .origin        the default origin of its points (℃: the ice point)
//   .space_before  false for a symbol written against the number (90°)
struct unit_options {
    std::string_view   ascii{};
    detail::spec_ref   kind{};
    detail::origin_ref origin{};
    bool               space_before{true};
};

namespace detail {
struct unit_atom {
    const named_unit*     U{};
    std::int8_t           Prefix{0}; // power of ten
    std::int8_t           Exp{0};
    friend constexpr bool operator==(const unit_atom& a, const unit_atom& b);
};

inline constexpr std::size_t max_unit_atoms = 6;
} // namespace detail

//---------------------------------------------------------------------------
// unit — a product of named units to integer powers, each with an optional
// decimal prefix (k, m, µ…), times a scale (mag<1000> * m; the common unit of
// a sum). A structural value: it is the template argument.
//---------------------------------------------------------------------------
struct unit {
    detail::unit_atom Atoms[detail::max_unit_atoms]{};
    std::int8_t       Size{0};
    magnitude         Scale{};

    constexpr unit() = default;
    constexpr unit(const named_unit& u) : Atoms{{&u, 0, 1}}, Size{1} {}

    friend constexpr bool operator==(const unit& a, const unit& b) {
        return a.Size == b.Size && a.Scale == b.Scale && std::equal(a.Atoms, a.Atoms + a.Size, b.Atoms);
    }
};

template <unit U>
struct unit_t {
    static constexpr unit value = U;
};

//---------------------------------------------------------------------------
// named_unit — a unit with a symbol (m, s, N, Ω, °C): a static object. Its
// magnitude is relative to the coherent SI unit of its dimension; Kind (if
// any) is the quantity kind it measures (Hz: frequency, Bq: activity).
//---------------------------------------------------------------------------
struct named_unit {
    std::string_view   Utf8;
    std::string_view   Ascii;
    dimension          Dim{};
    magnitude          Mag{};
    detail::spec_ref   Kind{};
    bool               SpaceBefore{true};
    detail::origin_ref Origin{}; // the default origin of its points (K: absolute zero)

    // A base unit of a kind: the coherent unit of its dimension, or a scale
    // of it (g: 1/1000 of the coherent kg).
    template <const quantity_spec* K>
    constexpr named_unit(std::string_view sym, spec_t<K>, unit_options o = {})
        : named_unit{sym, mag_t<magnitude{}>{}, spec_t<K>{}, o} {}
    template <magnitude M, const quantity_spec* K>
    constexpr named_unit(std::string_view sym, mag_t<M>, spec_t<K>, unit_options o = {})
        : Utf8{sym},
          Ascii{o.ascii.empty() ? sym : o.ascii},
          Dim{K->Dim},
          Mag{M},
          Kind{K},
          SpaceBefore{o.space_before},
          Origin{o.origin} {}
    // A unit defined by others: named_unit{"N", kg * m / s², isq::force}. Its
    // kind is the given one, else .kind, else its one factor's (min: 60 s).
    template <unit U>
    constexpr named_unit(std::string_view sym, unit_t<U>, unit_options o = {});
    template <unit U, const quantity_spec* K>
    constexpr named_unit(std::string_view sym, unit_t<U>, spec_t<K>, unit_options o = {});
};

namespace detail {
// A unit is its symbols, dimension, magnitude, kind and origin.
constexpr bool same(const named_unit* a, const named_unit* b) { // both set
    return a->Utf8 == b->Utf8 && a->Ascii == b->Ascii && a->Dim == b->Dim && a->Mag == b->Mag &&
           same(a->Kind, b->Kind) && same(a->Origin, b->Origin);
}

constexpr bool operator==(const unit_atom& a, const unit_atom& b) {
    return same(a.U, b.U) && a.Prefix == b.Prefix && a.Exp == b.Exp;
}

constexpr bool atom_before(const unit_atom& a, const unit_atom& b) {
    if (a.U->Utf8 != b.U->Utf8)
        return a.U->Utf8 < b.U->Utf8;
    return a.Prefix < b.Prefix;
}

consteval void unit_add(unit& u, unit_atom a) {
    if (!sorted_add(
            u.Atoms,
            u.Size,
            a,
            a.Exp,
            [](const unit_atom& x, const unit_atom& y) { return same(x.U, y.U) && x.Prefix == y.Prefix; },
            atom_before))
        beman::inside::detail::constexpr_error<"units: a unit holds at most 6 factors - name it with named_unit">();
}

consteval unit unit_mul(unit a, const unit& b, int sign) {
    for (std::int8_t i = 0; i < b.Size; ++i) {
        unit_atom t = b.Atoms[i];
        t.Exp       = static_cast<std::int8_t>(t.Exp * sign);
        unit_add(a, t);
    }
    a.Scale = sign > 0 ? a.Scale * b.Scale : a.Scale / b.Scale;
    return a;
}

consteval unit unit_pow(const unit& u, int n) {
    unit r{};
    for (std::int8_t i = 0; i < u.Size; ++i)
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, dimension::exponent(u.Atoms[i].Exp * n)});
    r.Scale = u.Scale.pow(n);
    return r;
}

consteval unit unit_scaled(unit u, const magnitude& m) {
    u.Scale = m * u.Scale;
    return u;
}
consteval unit unit_prefixed(unit u, int p) {
    u.Atoms[0].Prefix = static_cast<std::int8_t>(p);
    return u;
}

consteval magnitude ten_pow(int p) {
    magnitude ten{grid_rational{10}};
    return ten.pow(p);
}
} // namespace detail

// A unit's dimension, magnitude (relative to the coherent SI unit) and kind.
consteval dimension dimension_of(const unit& u) {
    dimension d{};
    for (std::int8_t i = 0; i < u.Size; ++i)
        d = d * u.Atoms[i].U->Dim.pow(u.Atoms[i].Exp);
    return d;
}
consteval magnitude magnitude_of(const unit& u) {
    magnitude m = u.Scale;
    for (std::int8_t i = 0; i < u.Size; ++i)
        m = m * (u.Atoms[i].U->Mag * detail::ten_pow(u.Atoms[i].Prefix)).pow(u.Atoms[i].Exp);
    return m;
}
namespace detail {
// One named unit (with a prefix too) to the power 1, unscaled: its kind,
// origin and spacing are the unit's.
consteval bool is_single_named(const unit& u) { return u.Size == 1 && u.Atoms[0].Exp == 1 && u.Scale.is_one(); }
// Whether a quantity's text puts a space before the unit (not before °).
consteval bool space_before(const unit& u) { return !(is_single_named(u) && !u.Atoms[0].U->SpaceBefore); }
} // namespace detail

// The kind a unit measures: a named unit's (with a prefix too), else none.
consteval detail::spec_ref kind_of(const unit& u) {
    return detail::is_single_named(u) ? u.Atoms[0].U->Kind : detail::spec_ref{};
}

namespace detail {
// The kind a unit defined by U inherits: its one factor's (min: 60 s).
consteval spec_ref inherited_kind(const unit& u) {
    return u.Size == 1 && u.Atoms[0].Exp == 1 ? u.Atoms[0].U->Kind : spec_ref{};
}
} // namespace detail

template <unit U>
constexpr named_unit::named_unit(std::string_view sym, unit_t<U>, unit_options o)
    : Utf8{sym},
      Ascii{o.ascii.empty() ? sym : o.ascii},
      Dim{dimension_of(U)},
      Mag{magnitude_of(U)},
      Kind{o.kind ? o.kind : detail::inherited_kind(U)},
      SpaceBefore{o.space_before},
      Origin{o.origin} {
    if (Kind && !(Kind->Dim == Dim))
        beman::inside::detail::constexpr_error<"units: a unit's kind must have the unit's dimension">();
    if (Origin && !(Origin->Dim == Dim))
        beman::inside::detail::constexpr_error<"units: a unit's origin must have the unit's dimension">();
}
template <unit U, const quantity_spec* K>
constexpr named_unit::named_unit(std::string_view sym, unit_t<U>, spec_t<K>, unit_options o)
    : named_unit{sym, unit_t<U>{}, unit_options{o.ascii, K, o.origin, o.space_before}} {}

// The tags of your own entries, each made from its static object:
//   inline constexpr auto metre  = unit_c<metre_def>;     // a named_unit
//   inline constexpr auto height = spec_c<height_def>;    // a quantity_spec
//   inline constexpr auto origin = origin_c<origin_def>;  // a point_origin
template <const named_unit& N>
inline constexpr unit_t<unit{N}> unit_c{};
template <const quantity_spec& S>
inline constexpr spec_t<&S> spec_c{};
template <const point_origin& O>
inline constexpr origin_t<&O> origin_c{};

// The dimensionless unit 1.
inline constexpr unit_t<unit{}> one{};

template <unit A, unit B>
consteval bool operator==(unit_t<A>, unit_t<B>) {
    return A == B;
}
template <unit A, unit B>
consteval auto operator*(unit_t<A>, unit_t<B>) {
    return unit_t<detail::unit_mul(A, B, 1)>{};
}
template <unit A, unit B>
consteval auto operator/(unit_t<A>, unit_t<B>) {
    return unit_t<detail::unit_mul(A, B, -1)>{};
}
template <magnitude M, unit B>
consteval auto operator*(mag_t<M>, unit_t<B>) {
    return unit_t<detail::unit_scaled(B, M)>{};
}
template <magnitude M, unit B>
consteval auto operator/(mag_t<M>, unit_t<B>) {
    return unit_t<detail::unit_scaled(detail::unit_pow(B, -1), M)>{};
}
template <unit B, magnitude M>
consteval auto operator*(unit_t<B>, mag_t<M>) {
    return unit_t<detail::unit_scaled(B, M)>{};
}
template <unit B, magnitude M>
consteval auto operator/(unit_t<B>, mag_t<M>) {
    return unit_t<detail::unit_scaled(B, magnitude{} / M)>{};
}
template <int N, unit A>
consteval auto pow(unit_t<A>) {
    return unit_t<detail::unit_pow(A, N)>{};
}
template <unit A>
consteval auto square(unit_t<A>) {
    return unit_t<detail::unit_pow(A, 2)>{};
}
template <unit A>
consteval auto cubic(unit_t<A>) {
    return unit_t<detail::unit_pow(A, 3)>{};
}
template <unit A>
consteval auto inverse(unit_t<A>) {
    return unit_t<detail::unit_pow(A, -1)>{};
}

// A decimal prefix: si::kilo(si::metre). Only on a named unit, once.
template <int P>
struct prefix_t {
    template <unit U>
    consteval auto operator()(unit_t<U>) const {
        static_assert(U.Size == 1 && U.Atoms[0].Exp == 1 && U.Atoms[0].Prefix == 0 && U.Scale.is_one(),
                      "units: a prefix applies to a named unit without a prefix (kilo(metre), not kilo(m/s))");
        return unit_t<detail::unit_prefixed(U, P)>{};
    }
};

//---------------------------------------------------------------------------
// reference — what a quantity's number means: a unit plus a quantity spec.
//   Spec named, KindOf false   isq::height[m]: exactly that spec
//   Spec named, KindOf true    a unit's kind (si::hertz): any spec of its kind
//   Spec null, Derived set     a product of specs (width * height)
//   Spec null, Derived empty   any quantity of the unit's dimension (m/s)
//---------------------------------------------------------------------------
struct reference {
    detail::spec_ref Spec{};
    bool             KindOf{false};
    detail::recipe   Derived{};
    unit             U{};

    constexpr reference() = default;
    constexpr reference(detail::spec_ref s, bool kind_of, detail::recipe derived, unit u)
        : Spec{s}, KindOf{kind_of}, Derived{derived}, U{u} {}
    // A unit tag as a reference: quantity<si::metre, …>.
    template <unit V>
    constexpr reference(unit_t<V>) : Spec{kind_of(V)}, KindOf{static_cast<bool>(kind_of(V))}, U{V} {}

    friend constexpr bool operator==(const reference& a, const reference& b) {
        return detail::same(a.Spec, b.Spec) && a.KindOf == b.KindOf && a.Derived == b.Derived && a.U == b.U;
    }
};

consteval reference reference_of(const unit& u) {
    const detail::spec_ref k = kind_of(u);
    return reference{k, static_cast<bool>(k), detail::recipe{}, u};
}

template <reference R>
struct reference_t {
    static constexpr reference value = R;
    constexpr                  operator reference() const { return R; }
};

template <class T>
inline constexpr bool is_unit_tag_v = false;
template <unit U>
inline constexpr bool is_unit_tag_v<unit_t<U>> = true;

template <class T>
inline constexpr bool is_reference_tag_v = false;
template <unit U>
inline constexpr bool is_reference_tag_v<unit_t<U>> = true;
template <reference R>
inline constexpr bool is_reference_tag_v<reference_t<R>> = true;

// The reference of a tag: a unit_t is the unit with its kind.
template <unit U>
consteval reference reference_of(unit_t<U>) {
    return reference_of(U);
}
template <reference R>
consteval reference reference_of(reference_t<R>) {
    return R;
}

template <const quantity_spec* S>
template <class U>
consteval auto spec_t<S>::operator[](U) const {
    static_assert(is_reference_tag_v<U> && requires { U::value.Size; }, "units: spec[unit] takes a unit");
    constexpr unit u = U::value;
    static_assert(dimension_of(u) == S->Dim, "units: spec[unit] - the unit has another dimension than the spec");
    constexpr detail::spec_ref k = kind_of(u);
    static_assert(!k || detail::same(detail::kind_root(k.P), detail::kind_root(S)),
                  "units: spec[unit] - the unit measures another kind of quantity (Hz for activity?)");
    return reference_t<reference{S, false, detail::recipe{}, u}>{};
}

namespace detail {
// Which spec facts a reference carries.
consteval bool is_generic(const reference& r) { return !r.Spec && r.Derived.Size == 0; }

// Whether a reference's quantity may convert implicitly to a named spec `to`.
consteval bool derived_converts_to(const reference& from, const quantity_spec* to) {
    if (to->IsKind && !from.KindOf)
        return false;
    // Any quantity of the dimension names a kind root: m/s → speed, N·m → energy.
    if (!to->Parent && !to->IsKind)
        return true;
    if (from.KindOf)
        return derived_converts_to(reference{nullptr, false, from.Derived, from.U}, kind_root(to)) ||
               recipe_matches(from.Derived, recipe_of(kind_root(to)));
    return to->Recipe.Size > 0 && recipe_matches(from.Derived, to->Recipe);
}

// Implicit conversion between the specs of two references of one dimension.
consteval bool spec_converts(const reference& from, const reference& to) {
    if (is_generic(to)) // a dimension's quantity takes any of its kinds but an IsKind one
        return !from.Spec || !kind_root(from.Spec.P)->IsKind;
    if (is_generic(from))
        return !to.Spec || !kind_root(to.Spec.P)->IsKind;
    if (!to.Spec) { // a derived target: the source's recipe, by kind
        const recipe have = from.Spec ? recipe_of(from.Spec.P) : from.Derived;
        return recipe_by_kind(have) == recipe_by_kind(to.Derived);
    }
    if (!from.Spec)
        return derived_converts_to(from, to.KindOf ? kind_root(to.Spec.P) : to.Spec.P);
    if (to.KindOf || from.KindOf) // a unit's kind takes and gives any spec of that kind
        return same(kind_root(from.Spec.P), kind_root(to.Spec.P));
    return is_within_kind_ancestor(from.Spec.P, to.Spec.P);
}

// Explicit conversion (quantity_cast): any spec of the same dimension, but
// not across IsKind kinds.
consteval bool spec_casts(const reference& from, const quantity_spec* to) {
    const spec_ref       fk = from.Spec ? spec_ref{kind_root(from.Spec.P)} : spec_ref{};
    const quantity_spec* tk = kind_root(to);
    if (same(fk, spec_ref{tk}))
        return true;
    if (fk && fk->IsKind)
        return false;
    return !tk->IsKind || !from.Spec;
}

// The spec part of a sum's reference; ok = false when the two cannot add.
struct spec_result {
    bool      Ok{false};
    reference R{};
};

consteval spec_result common_spec(const reference& a, const reference& b) {
    if (same(a.Spec, b.Spec) && a.KindOf == b.KindOf && a.Derived == b.Derived)
        return {true, a};
    if (spec_converts(a, b) && !spec_converts(b, a))
        return {true, b};
    if (spec_converts(b, a) && !spec_converts(a, b))
        return {true, a};
    if (a.Spec && b.Spec) {
        if (a.KindOf && b.KindOf && same(kind_root(a.Spec.P), kind_root(b.Spec.P)))
            return {true, reference{kind_root(a.Spec.P), true, {}, a.U}};
        if (a.KindOf != b.KindOf && same(kind_root(a.Spec.P), kind_root(b.Spec.P))) // kind_of + a named spec of it
            return {true, a.KindOf ? b : a};
        // The nearest common ancestor within one kind.
        for (const quantity_spec* s = a.Spec.P;; s = s->Parent.P) {
            if (is_within_kind_ancestor(b.Spec.P, s))
                return {true, reference{s, false, {}, a.U}};
            if (s->IsKind || !s->Parent)
                break;
        }
        return {};
    }
    if (spec_converts(a, b) && spec_converts(b, a)) // e.g. two recipes of one kind
        return {true, reference{nullptr, false, {}, a.U}};
    return {};
}

// The spec part of a product: named factors multiply into a recipe.
// (Spec null, KindOf, no recipe) is the empty product: 1 / x starts there.
consteval reference spec_product(const reference& a, const reference& b, int sign, const unit& u) {
    const auto empty = [](const reference& r) consteval { return !r.Spec && r.KindOf && r.Derived.Size == 0; };
    if ((is_generic(a) && !empty(a)) || (is_generic(b) && !empty(b)))
        return reference{nullptr, false, {}, u};
    const auto factors = [](const reference& r) consteval {
        if (!r.Spec)
            return r.Derived;
        recipe f{};
        recipe_add(f, r.KindOf ? kind_root(r.Spec.P) : r.Spec.P, 1);
        return f;
    };
    recipe f = factors(a);
    if (!recipe_merge(f, factors(b), sign) || f.Size == 0) // too many factors, or a pure ratio
        return reference{nullptr, false, {}, u};
    const bool kind_of = (a.KindOf || empty(a)) && (b.KindOf || empty(b));
    if (f.Size == 1 && f.F[0].Exp == 1)
        return reference{f.F[0].Spec, kind_of, {}, u};
    return reference{nullptr, kind_of, f, u};
}

consteval reference spec_pow(const reference& a, int n, const unit& u) {
    if (is_generic(a) || n == 0)
        return reference{nullptr, false, {}, u};
    reference r = a;
    r.U         = u;
    for (int i = 1; i < (n < 0 ? -n : n); ++i)
        r = spec_product(r, a, 1, u);
    return n < 0 ? spec_product(reference{nullptr, true, {}, u}, r, -1, u) : r;
}
} // namespace detail

//---------------------------------------------------------------------------
// Unit conversion of a number: the value in unit From as a value in unit To,
// value · mag(From) / mag(To). A rational factor rescales the grid (the raw is
// kept where the storage allows); a π factor has no exact rescale.
//---------------------------------------------------------------------------
namespace detail {
template <magnitude M>
inline constexpr bool exact_factor_v = M.Pi == 0;

template <magnitude M, insidable V>
constexpr auto rescale(const V& v) {
    static_assert(exact_factor_v<M>, BEMAN_INSIDE_UNITS_WHY_PI);
    if constexpr (M.is_one())
        return v;
    else {
        // The product's grid with v's policy: a conversion keeps clamp, wrap
        // and the rounding mode.
        const auto x = v * just<M.Ratio>;
        if constexpr (flags_of<decltype(x)> == flags_of<V>)
            return x;
        else
            return inside<grid_of<decltype(x)>, flags_of<V>>::from_raw(x.raw());
    }
}

template <unit From, unit To>
inline constexpr magnitude conversion_factor = magnitude_of(From) / magnitude_of(To);

// A fixed order of units: fewer atoms first, then by symbol and exponent.
consteval bool unit_before(const unit& a, const unit& b) {
    if (a.Size != b.Size)
        return a.Size < b.Size;
    return std::lexicographical_compare(
        a.Atoms, a.Atoms + a.Size, b.Atoms, b.Atoms + b.Size, [](const unit_atom& x, const unit_atom& y) {
            if (atom_before(x, y) || atom_before(y, x))
                return atom_before(x, y);
            return x.Exp < y.Exp;
        });
}

// The common unit of a sum: the smaller unit when it divides the other
// (km + m → m), else the smaller unit scaled to the gcd of the magnitudes
// (ft + m and m + ft → [1/381 ft]); of two spellings of one size, the
// shorter (J + N m → J). The sum's type is the same either way.
consteval unit common_unit(const unit& a, const unit& b) {
    if (a == b)
        return a;
    const magnitude ma = magnitude_of(a), mb = magnitude_of(b);
    if (ma == mb) // one size, two spellings (J, N m): the shorter, then by symbol
        return unit_before(b, a) ? b : a;
    if (ma.Pi != mb.Pi)
        beman::inside::detail::constexpr_error<
            "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first">();
    const grid_rational g = beman::inside::detail::gcd(ma.Ratio, mb.Ratio);
    if (g == ma.Ratio)
        return a;
    if (g == mb.Ratio)
        return b;
    const bool a_smaller = ma.Ratio < mb.Ratio;
    unit       u         = a_smaller ? a : b;
    u.Scale              = u.Scale * magnitude{g / (a_smaller ? ma.Ratio : mb.Ratio)};
    return u;
}

// Whether rescale<M>(v) forms: inside's v · M exists. A pi factor passes, for
// rescale's static_assert to explain.
template <magnitude M, class V>
concept rescale_forms = !exact_factor_v<M> || M.is_one() || requires(const V& v) { v * just<M.Ratio>; };

// Whether a number of V in unit From rescales to unit To.
template <unit From, unit To, class V>
concept rescalable = rescale_forms<conversion_factor<From, To>, V>;

// Whether inside's x Op y exists (Op one of + - % * / = <, the last two for
// == and <=>).
template <class X, class Y, char Op>
concept number_op = (Op == '+' && requires(const X& x, const Y& y) { x + y; }) ||
                    (Op == '-' && requires(const X& x, const Y& y) { x - y; }) ||
                    (Op == '%' && requires(const X& x, const Y& y) { x % y; }) ||
                    (Op == '*' && requires(const X& x, const Y& y) { x * y; }) ||
                    (Op == '/' && requires(const X& x, const Y& y) { x / y; }) ||
                    (Op == '=' && requires(const X& x, const Y& y) { x == y; }) ||
                    (Op == '<' && requires(const X& x, const Y& y) { x <=> y; });

// Why q.in(To) cannot convert from From, if it cannot: another dimension, or
// a factor of pi (which value_cast rounds). A deleted overload per reason.
enum class in_error : unsigned char { none, dimension, pi };
template <unit From, unit To>
consteval in_error in_error_of() {
    if (dimension_of(From) != dimension_of(To))
        return in_error::dimension;
    if (magnitude_of(From).Pi != magnitude_of(To).Pi)
        return in_error::pi;
    return in_error::none;
}
} // namespace detail

//---------------------------------------------------------------------------
// quantity<R, Rep> — a number Rep (an inside) of the reference R.
//---------------------------------------------------------------------------
template <reference R, insidable Rep>
class quantity;

template <class T>
inline constexpr bool is_quantity_v = false;
template <reference R, class Rep>
inline constexpr bool is_quantity_v<quantity<R, Rep>> = true;

template <class T>
concept quantity_like = is_quantity_v<std::remove_cvref_t<T>>;

namespace detail {
// An exact conversion factor from unit From to unit To: it holds no π.
template <unit From, unit To>
inline constexpr bool exact_rescale_v = exact_factor_v<conversion_factor<From, To>>;

template <reference From, reference To>
inline constexpr bool implicit_v =
    exact_rescale_v<From.U, To.U> && dimension_of(From.U) == dimension_of(To.U) && spec_converts(From, To);
// A quantity of From on V that converts implicitly to To; one of R that
// converts to unit U (q.in<U>()).
template <reference From, reference To, class V>
concept implicit_rescalable = implicit_v<From, To> && rescalable<From.U, To.U, V>;
template <reference R, unit U, class V>
concept in_rescalable = in_error_of<R.U, U>() == in_error::none && rescalable<R.U, U, V>;

// A dimensionless quantity that is a plain number: no is_kind spec (an
// angle), and an exact factor to unit one.
template <reference R>
inline constexpr bool number_like_v = dimension_of(R.U) == dim_one && exact_factor_v<conversion_factor<R.U, unit{}>> &&
                                      (!R.Spec || !kind_root(R.Spec.P)->IsKind);

// Its number's type; void for any other quantity (never forming the
// rescale, whose pi check would fire for an angle in degrees).
template <reference R, class V, bool = number_like_v<R>>
struct number_type {
    using type = void;
};
template <reference R, class V>
struct number_type<R, V, true> {
    using type = decltype(rescale<conversion_factor<R.U, unit{}>>(std::declval<const V&>()));
};
template <reference R, class V>
using number_t = typename number_type<R, V>::type;

template <reference From, reference To, insidable V>
using rescaled_t = decltype(rescale<conversion_factor<From.U, To.U>>(std::declval<const V&>()));

// q's number in unit U: exact, its grid rescaled.
template <unit U, reference R, class V>
constexpr auto number_in_unit(const quantity<R, V>& q) {
    return rescale<conversion_factor<R.U, U>>(q.numerical_value());
}

using beman::inside::detail::is_expected_v;

// Q from v, or an expected Q from an expected v (its error passed on).
template <class Q, class V>
constexpr auto lift_to(V&& v) {
    if constexpr (is_expected_v<std::remove_cvref_t<V>>)
        return std::forward<V>(v).transform([](auto&& x) { return Q{std::forward<decltype(x)>(x)}; });
    else
        return Q{std::forward<V>(v)};
}

// A quantity of reference R on a number that inside's arithmetic returned: an
// inside, or an expected of one (division by a grid holding 0).
template <reference R, class V>
constexpr auto make_quantity(V&& v) {
    return lift_to<quantity<R, beman::inside::detail::unwrap_t<V>>>(std::forward<V>(v));
}

// q.on_wrap(f) and the other hooks: the inside's policy_ref on q's number,
// with stores taking quantities (rescaled exactly, as +=) and f called on the
// quantity. The other arguments (carry, overshoot, errc) are the inside's.
template <class F, class Q>
constexpr auto on_quantity(Q& q, F&& f) {
    return [&q, f = std::forward<F>(f)](auto&, auto&&... a) mutable -> decltype(auto) {
        return f(q, std::forward<decltype(a)>(a)...);
    };
}

template <reference R, class Q, class Ref>
struct quantity_action_ref {
    Q&  Self;
    Ref NumRef;

    template <reference R2, class V2>
        requires implicit_rescalable<R2, R, V2> && requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r = v; }
    constexpr Q& operator=(const quantity<R2, V2>& q) {
        NumRef = number_in_unit<R.U>(q);
        return Self;
    }
    template <reference R2, class V2>
        requires implicit_rescalable<R2, R, V2> && requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r += v; }
    constexpr Q& operator+=(const quantity<R2, V2>& q) {
        NumRef += number_in_unit<R.U>(q);
        return Self;
    }
    template <reference R2, class V2>
        requires implicit_rescalable<R2, R, V2> && requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r -= v; }
    constexpr Q& operator-=(const quantity<R2, V2>& q) {
        NumRef -= number_in_unit<R.U>(q);
        return Self;
    }
};
template <reference R, class Q, class Ref>
constexpr auto make_action_ref(Q& q, Ref&& r) {
    return quantity_action_ref<R, Q, std::remove_cvref_t<Ref>>{q, std::forward<Ref>(r)};
}
} // namespace detail

template <reference R, insidable Rep>
class quantity {
    Rep Num;

  public:
    using rep                                   = Rep;
    static constexpr units::reference reference = R;
    static constexpr units::unit      unit      = R.U;
    static constexpr units::dimension dimension = dimension_of(R.U);
    // The spec as a value: its tag for a named spec (isq::height), else the
    // spec expression of its recipe. Compare with ==: q.quantity_spec == isq::length.
    static constexpr auto quantity_spec = [] {
        if constexpr (static_cast<bool>(R.Spec))
            return spec_t<R.Spec.P>{};
        else
            return detail::spec_expr{R.Derived, dimension_of(R.U)};
    }();

    constexpr quantity() = default;

    // From a number in the unit R: quantity<si::metre, inside<{0, 100}>>{42}.
    template <class V>
        requires std::constructible_from<Rep, V> && (!quantity_like<V>)
    constexpr explicit quantity(V&& v) : Num(std::forward<V>(v)) {}

    // From another quantity: implicit when the spec converts implicitly and
    // the number, in R's unit, converts implicitly into Rep.
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> && std::constructible_from<Rep, detail::rescaled_t<R2, R, V2>>
    constexpr explicit(!std::convertible_to<detail::rescaled_t<R2, R, V2>, Rep>) quantity(const quantity<R2, V2>& q)
        : Num(detail::number_in_unit<R.U>(q)) {}

    [[nodiscard]] constexpr const Rep& numerical_value() const noexcept { return Num; }

    // The same quantity in unit U: exact, the number's grid rescaled.
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto in() const {
        constexpr units::reference to{R.Spec, R.KindOf, R.Derived, U};
        return detail::make_quantity<to>(detail::rescale<detail::conversion_factor<R.U, U>>(Num));
    }
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto in(units::unit_t<U>) const {
        return in<U>();
    }
    // The number in unit U.
    template <units::unit U>
        requires detail::in_rescalable<R, U, Rep>
    [[nodiscard]] constexpr auto numerical_value_in(units::unit_t<U>) const {
        return in<U>().numerical_value();
    }
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::dimension)
    void in() const = delete ("units: in() - the unit has another dimension");
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::dimension)
    void in(units::unit_t<U>) const = delete ("units: in() - the unit has another dimension");
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::pi)
    void in() const = delete (BEMAN_INSIDE_UNITS_WHY_PI);
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::pi)
    void in(units::unit_t<U>) const = delete (BEMAN_INSIDE_UNITS_WHY_PI);

    // A dimensionless quantity is a number: its value in unit one (km/m is
    // ×1000, % is ×1/100), into any inside that takes it — implicitly when
    // that number converts implicitly. An angle (an is_kind spec) stays a
    // quantity; numerical_value() reads its number.
    template <insidable T>
        requires detail::number_like_v<R> && std::constructible_from<T, detail::number_t<R, Rep>>
    constexpr explicit(!std::convertible_to<detail::number_t<R, Rep>, T>) operator T() const {
        return T(detail::rescale<detail::conversion_factor<R.U, units::unit{}>>(Num));
    }
    template <insidable T>
        requires(dimension_of(R.U) == dim_one && !detail::number_like_v<R>)
    operator T() const = delete ("units: an angle (an is_kind quantity) or a pi-scaled ratio is no plain "
                                 "number - read numerical_value(), or value_cast it first");

    [[nodiscard]] constexpr auto operator+() const { return detail::make_quantity<R>(+Num); }
    [[nodiscard]] constexpr auto operator-() const { return detail::make_quantity<R>(-Num); }

    // Compound assignment keeps the type: the number is stored back under Rep's
    // policy (checked by default).
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> &&
                 requires(Rep& n, const detail::rescaled_t<R2, R, V2>& r) { n += r; }
    constexpr quantity& operator+=(const quantity<R2, V2>& q) {
        Num += detail::number_in_unit<R.U>(q);
        return *this;
    }
    template <units::reference R2, class V2>
        requires detail::implicit_rescalable<R2, R, V2> &&
                 requires(Rep& n, const detail::rescaled_t<R2, R, V2>& r) { n -= r; }
    constexpr quantity& operator-=(const quantity<R2, V2>& q) {
        Num -= detail::number_in_unit<R.U>(q);
        return *this;
    }
    template <insidable V>
        requires requires(Rep& n, const V& v) { n *= v; }
    constexpr quantity& operator*=(const V& v) {
        Num *= v;
        return *this;
    }
    template <insidable V>
        requires requires(Rep& n, const V& v) { n /= v; }
    constexpr quantity& operator/=(const V& v) {
        Num /= v;
        return *this;
    }

    // One unit up or down: ++q is q += 1·unit, under Rep's policy.
    constexpr quantity& operator++()
        requires requires(Rep& n) { ++n; }
    {
        ++Num;
        return *this;
    }
    constexpr quantity& operator--()
        requires requires(Rep& n) { --n; }
    {
        --Num;
        return *this;
    }
    constexpr quantity operator++(int)
        requires requires(Rep& n) { ++n; }
    {
        auto old = *this;
        ++Num;
        return old;
    }
    constexpr quantity operator--(int)
        requires requires(Rep& n) { --n; }
    {
        auto old = *this;
        --Num;
        return old;
    }

    // A store with a hook, as on inside: q.on_wrap(f) += d calls f(q, carry)
    // when the sum wraps. Stores take quantities; f gets the quantity.
    template <class F>
    [[nodiscard]] constexpr auto on_wrap(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_wrap(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_clamp(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_clamp(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_error(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_error(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_overflow(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_overflow(detail::on_quantity(*this, std::forward<F>(f))));
    }

    // Rep's try_make in the unit R: quantity<si::metre, M>::try_make(12.5).
    template <policy_flag F = none, class V>
        requires requires(const V& v) { Rep::template try_make<F>(v); }
    [[nodiscard]] static constexpr auto try_make(const V& v) {
        return detail::make_quantity<R>(Rep::template try_make<F>(v));
    }
};

// A quantity chains through inside's expected-lift operators: a division by
// a grid holding 0 and the rest of the expression stay one std::expected.
} // namespace beman::inside::units
namespace beman::inside::detail {
template <units::reference R, class Rep>
inline constexpr bool lift_operand_v<units::quantity<R, Rep>> = true;
} // namespace beman::inside::detail
namespace beman::inside::units {

//---------------------------------------------------------------------------
// Construction: number * unit, number * spec[unit]. A raw scalar has no grid.
//---------------------------------------------------------------------------
template <insidable V, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const V& v, Tag t) {
    return quantity<reference_of(t), V>{v};
}
template <class N, class Tag>
    requires is_reference_tag_v<Tag> && std::is_arithmetic_v<N>
void operator*(N, Tag) =
    delete ("units: a raw number has no grid - write just<42> * unit, or quantity<unit, inside<…>>{42}");
// number / unit: 5 / s.
template <insidable V, unit U>
[[nodiscard]] constexpr auto operator/(const V& v, unit_t<U>) {
    return quantity<reference_of(detail::unit_pow(U, -1)), V>{v};
}

// A magnitude scales a quantity's unit, not its number: mag_pi * q is q
// in [π·unit], exact and free (π is never rounded; value_cast to a plain
// unit rounds it once, units_math.hpp). mag<2> * q, q / mag_pi likewise.
namespace detail {
consteval reference scaled_reference(reference r, const magnitude& m) {
    r.U = unit_scaled(r.U, m);
    return r;
}
} // namespace detail
template <magnitude M, reference R, class V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const quantity<R, V>& q) {
    return quantity<detail::scaled_reference(R, M), V>{q.numerical_value()};
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator*(const quantity<R, V>& q, mag_t<M>) {
    return quantity<detail::scaled_reference(R, M), V>{q.numerical_value()};
}
// A magnitude times a number: the number in the scaled unit one ([π]), so
// mag_pi * just<2> * si::second is 2 in [π s].
template <magnitude M, insidable V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const V& v) {
    return quantity<reference_of(detail::unit_scaled(unit{}, M)), V>{v};
}
template <insidable V, magnitude M>
[[nodiscard]] constexpr auto operator*(const V& v, mag_t<M>) {
    return quantity<reference_of(detail::unit_scaled(unit{}, M)), V>{v};
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator/(const quantity<R, V>& q, mag_t<M>) {
    return quantity<detail::scaled_reference(R, magnitude{} / M), V>{q.numerical_value()};
}

//---------------------------------------------------------------------------
// Arithmetic
//---------------------------------------------------------------------------
namespace detail {
template <reference A, reference B>
consteval reference sum_reference() {
    static_assert(dimension_of(A.U) == dimension_of(B.U),
                  "units: adding or comparing quantities of different dimensions");
    constexpr spec_result s = common_spec(A, B);
    static_assert(s.Ok, "units: adding or comparing quantities of different kinds");
    reference r = s.R;
    r.U         = common_unit(A.U, B.U);
    return r;
}

// Why a sum or comparison of A and B is a unit error, if it is: other
// dimensions or kinds, or no common unit. Each reason is a deleted overload
// with its message.
enum class sum_error : unsigned char { none, dimensions, kinds, pi };

template <reference A, reference B>
consteval sum_error sum_error_of() {
    if (dimension_of(A.U) != dimension_of(B.U))
        return sum_error::dimensions;
    if (!common_spec(A, B).Ok)
        return sum_error::kinds;
    const magnitude ma = magnitude_of(A.U), mb = magnitude_of(B.U);
    if (ma.Pi != mb.Pi)
        return sum_error::pi;
    return sum_error::none;
}
template <reference A, reference B>
consteval bool sum_diagnosed() {
    return sum_error_of<A, B>() != sum_error::none;
}

// Whether the two quantities have a common unit (no sum diagnostic) and both
// numbers rescale to it.
template <reference A, class VA, reference B, class VB>
concept common_rescalable = (!sum_diagnosed<A, B>()) && rescalable<A.U, sum_reference<A, B>().U, VA> &&
                            rescalable<B.U, sum_reference<A, B>().U, VB>;
// The number of a quantity of A on V, in A and B's common unit.
template <reference A, class V, reference B>
using common_number_t = rescaled_t<A, sum_reference<A, B>(), V>;

template <reference A, class VA, reference B, class VB, char Op>
concept summable =
    common_rescalable<A, VA, B, VB> && number_op<common_number_t<A, VA, B>, common_number_t<B, VB, A>, Op>;

template <reference A, reference B, int Sign>
consteval reference product_reference() {
    const unit u = unit_mul(A.U, B.U, Sign);
    return spec_product(A, B, Sign, u);
}

// q's number in the common unit of A and B: the unit of their sum.
template <reference A, reference B, reference R, class V>
constexpr auto in_common_unit(const quantity<R, V>& q) {
    return number_in_unit<sum_reference<A, B>().U>(q);
}
} // namespace detail

template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '+'>
[[nodiscard]] constexpr auto operator+(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::in_common_unit<A, B>(a) + detail::in_common_unit<A, B>(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '-'>
[[nodiscard]] constexpr auto operator-(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::in_common_unit<A, B>(a) - detail::in_common_unit<A, B>(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '%'>
[[nodiscard]] constexpr auto operator%(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::in_common_unit<A, B>(a) % detail::in_common_unit<A, B>(b));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '='>
[[nodiscard]] constexpr bool operator==(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::in_common_unit<A, B>(a) == detail::in_common_unit<A, B>(b);
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '<'>
[[nodiscard]] constexpr auto operator<=>(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::in_common_unit<A, B>(a) <=> detail::in_common_unit<A, B>(b);
}

// A sum, difference, remainder or comparison of two units that cannot meet:
// a deleted overload per reason, so a requires-test is false and a call says
// why.
#define BEMAN_INSIDE_UNITS_SUM_DELETED(ret, op)                                   \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::dimensions)   \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                      \
        delete ("units: adding or comparing quantities of different dimensions"); \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::kinds)        \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                      \
        delete ("units: adding or comparing quantities of different kinds");      \
    template <reference A, class VA, reference B, class VB>                       \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::pi)           \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) = delete (             \
        "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first");
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator+)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator-)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator%)
BEMAN_INSIDE_UNITS_SUM_DELETED(bool, operator==)
BEMAN_INSIDE_UNITS_SUM_DELETED(std::strong_ordering, operator<=>)
#undef BEMAN_INSIDE_UNITS_SUM_DELETED

template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, 1>()>(a.numerical_value() * b.numerical_value());
}
template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, -1>()>(a.numerical_value() / b.numerical_value());
}

// A quantity scaled by a dimensionless number (an inside).
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() * v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '*'>
[[nodiscard]] constexpr auto operator*(const V& v, const quantity<A, VA>& a) {
    return detail::make_quantity<A>(v * a.numerical_value());
}
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() / v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '/'>
[[nodiscard]] constexpr auto operator/(const V& v, const quantity<A, VA>& a) {
    constexpr reference R = detail::product_reference<reference{nullptr, true, {}, unit{}}, A, -1>();
    return detail::make_quantity<R>(v / a.numerical_value());
}
// A quantity times a unit: (5 * m) / s.
template <reference A, class VA, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), 1>()>(a.numerical_value());
}
template <reference A, class VA, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), -1>()>(a.numerical_value());
}
// No raw scalars: q * 2 needs a grid for the 2.
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator*(const quantity<A, VA>&, N) = delete ("units: a raw number has no grid - scale by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator*(N, const quantity<A, VA>&) = delete ("units: a raw number has no grid - scale by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator/(const quantity<A, VA>&, N) = delete ("units: a raw number has no grid - divide by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator/(N, const quantity<A, VA>&) = delete ("units: a raw number has no grid - divide by just<2>, not 2");

// pow<N>(q) — q·q·…·q (N ≥ 1): exact, the grid of each product.
template <int N, reference R, class V>
    requires(N == 1 || (N % 2 == 1 && requires(const quantity<R, V>& q) { q * pow<N - 1>(q); }) ||
             (N >= 2 && N % 2 == 0 && requires(const quantity<R, V>& q) { pow<N / 2>(q) * pow<N / 2>(q); }))
[[nodiscard]] constexpr auto pow(const quantity<R, V>& q) {
    if constexpr (N == 1)
        return q;
    else if constexpr (N % 2 == 1)
        return q * pow<N - 1>(q);
    else {
        const auto h = pow<N / 2>(q);
        return h * h;
    }
}

//---------------------------------------------------------------------------
// Casts
//---------------------------------------------------------------------------
// quantity_cast<spec>(q) — the same number as another spec of the dimension:
// down the tree (length → height) or across one kind; never across IsKind.
namespace detail {
// Why quantity_cast<To> cannot take R, if it cannot.
enum class cast_error : unsigned char { none, dimension, is_kind };
template <reference R, const quantity_spec* To>
consteval cast_error cast_error_of() {
    if (!(To->Dim == dimension_of(R.U)))
        return cast_error::dimension;
    return spec_casts(R, To) ? cast_error::none : cast_error::is_kind;
}
} // namespace detail

template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::none)
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return quantity<reference{SpecTag::value, false, {}, R.U}, V>{q.numerical_value()};
}
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::none)
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return quantity_cast<decltype(Spec)>(q);
}
template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::dimension)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - another dimension");
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::dimension)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - another dimension");
template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::is_kind)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - across an is_kind boundary");
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::is_kind)
void quantity_cast(const quantity<R, V>&) = delete ("units: quantity_cast - across an is_kind boundary");

// value_cast<Rep2>(q) — another representation, the same unit: Rep2's
// constructor (checked unless Rep2's policy says otherwise).
template <insidable Rep2, reference R, class V>
    requires std::constructible_from<Rep2, const V&>
[[nodiscard]] constexpr quantity<R, Rep2> value_cast(const quantity<R, V>& q) {
    return quantity<R, Rep2>{Rep2(q.numerical_value())};
}
// value_cast<U, Rep2>(q) — in unit U with representation Rep2: the exact
// value in U, stored by Rep2's constructor. (A pi factor, degree ↔ radian,
// rounds once: units_math.hpp.)
template <auto U, insidable Rep2, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2(q.in(U).numerical_value()); }
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    constexpr units::unit u = decltype(U)::value;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    return quantity<to, Rep2>{Rep2(q.in(U).numerical_value())};
}

// value_cast<Q>(q) — into the quantity type Q = quantity<R2, Rep2>: q in
// R2's unit, stored by Rep2's constructor. Q's spec must take q's, as for an
// implicit conversion; only the representation may round.
namespace detail {
template <class Q, reference R>
concept value_castable_to = quantity_like<Q> && dimension_of(Q::reference.U) == dimension_of(R.U) &&
                            spec_converts(R, Q::reference) && exact_rescale_v<R.U, Q::reference.U>;
}
template <class Q, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        static_cast<typename Q::rep>(q.template in<Q::reference.U>().numerical_value());
    }
[[nodiscard]] constexpr Q value_cast(const quantity<R, V>& q) {
    return Q{static_cast<typename Q::rep>(q.template in<Q::reference.U>().numerical_value())};
}
// try_value_cast<Q, F>(q) — the same through Q's Rep2::try_make<F>.
template <class Q, policy_flag F = none, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        Q::rep::template try_make<F>(q.template in<Q::reference.U>().numerical_value());
    }
[[nodiscard]] constexpr std::expected<Q, errc> try_value_cast(const quantity<R, V>& q) {
    return detail::lift_to<Q>(Q::rep::template try_make<F>(q.template in<Q::reference.U>().numerical_value()));
}

// try_value_cast<Rep2, F>(q) — as Rep2::try_make<F>: an expected.
template <insidable Rep2, policy_flag F = none, reference R, class V>
    requires requires(const V& v) { Rep2::template try_make<F>(v); }
[[nodiscard]] constexpr std::expected<quantity<R, Rep2>, errc> try_value_cast(const quantity<R, V>& q) {
    return detail::lift_to<quantity<R, Rep2>>(Rep2::template try_make<F>(q.numerical_value()));
}

// try_value_cast<U, Rep2, F>(q) — in unit U, as Rep2::try_make<F>.
template <auto U, insidable Rep2, policy_flag F = none, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2::template try_make<F>(q.in(U).numerical_value()); }
[[nodiscard]] constexpr auto try_value_cast(const quantity<R, V>& q) {
    constexpr reference to{R.Spec, R.KindOf, R.Derived, decltype(U)::value};
    return detail::lift_to<quantity<to, Rep2>>(Rep2::template try_make<F>(q.in(U).numerical_value()));
}

//---------------------------------------------------------------------------
// inside's algorithms on quantities: the number goes through the inside
// function in the target's unit, so each still rounds once and checks once.
//---------------------------------------------------------------------------
namespace detail {
// A plain inside is a quantity of unit one.
template <class T>
constexpr auto as_quantity(const T& x) {
    if constexpr (quantity_like<T>)
        return x;
    else
        return quantity<reference_of(one), T>{x};
}
template <class T>
using as_quantity_t = decltype(as_quantity(std::declval<const T&>()));

// The reference an Out takes: a quantity's, or unit one for a plain inside.
template <class Out>
constexpr reference out_reference() {
    if constexpr (quantity_like<Out>)
        return Out::reference;
    else
        return reference_of(one);
}
template <class Out>
struct out_rep {
    using type = Out;
};
template <class Out>
    requires quantity_like<Out>
struct out_rep<Out> {
    using type = typename Out::rep;
};

// Out from its number (or an expected of one).
template <class Out, class V>
constexpr auto wrap_out(V&& v) {
    if constexpr (!quantity_like<Out>)
        return std::forward<V>(v);
    else
        return lift_to<Out>(std::forward<V>(v));
}

// a·b (E = 1) or a/b (E = −1) stored into Out: the product's reference must
// convert to Out's, implicitly; a plain Out takes a dimensionless result.
template <class Out, int E, class A, class B>
concept into_operands =
    (quantity_like<A> || quantity_like<B> || quantity_like<Out>) &&
    requires {
        requires implicit_v<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>(),
                            out_reference<Out>()>;
    } &&
    rescalable<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>().U,
               out_reference<Out>().U,
               typename as_quantity_t<A>::rep>;
template <class Out, int E, class A>
constexpr auto into_scaled(const A& a, const auto& b) {
    constexpr reference P =
        product_reference<as_quantity_t<A>::reference, std::remove_cvref_t<decltype(as_quantity(b))>::reference, E>();
    return rescale<conversion_factor<P.U, out_reference<Out>().U>>(as_quantity(a).numerical_value());
}
} // namespace detail

// mul_into<Out, F>(a, b) / div_into<Out, F>(a, b): a·b or a/b rounded once
// into Out, a quantity type (or a plain inside for a dimensionless result).
// The unit factor is folded into the exact product: mul_into<usd>(eth, usd/eth).
template <class Out, policy_flag F = none, class A, class B>
    requires detail::into_operands<Out, 1, A, B> && requires(const A& a, const B& b) {
        beman::inside::mul_into<typename detail::out_rep<Out>::type, F>(detail::into_scaled<Out, 1>(a, b),
                                                                        detail::as_quantity(b).numerical_value());
    }
[[nodiscard]] constexpr auto mul_into(const A& a, const B& b) {
    return detail::wrap_out<Out>(beman::inside::mul_into<typename detail::out_rep<Out>::type, F>(
        detail::into_scaled<Out, 1>(a, b), detail::as_quantity(b).numerical_value()));
}
template <class Out, policy_flag F = none, class A, class B>
    requires detail::into_operands<Out, -1, A, B> && requires(const A& a, const B& b) {
        beman::inside::div_into<typename detail::out_rep<Out>::type, F>(detail::into_scaled<Out, -1>(a, b),
                                                                        detail::as_quantity(b).numerical_value());
    }
[[nodiscard]] constexpr auto div_into(const A& a, const B& b) {
    return detail::wrap_out<Out>(beman::inside::div_into<typename detail::out_rep<Out>::type, F>(
        detail::into_scaled<Out, -1>(a, b), detail::as_quantity(b).numerical_value()));
}

// add_all / mul_all with at least one quantity: the same folds as on inside.
template <class First, class... Rest>
    requires(quantity_like<First> || ... || quantity_like<Rest>)
[[nodiscard]] constexpr auto add_all(const First& first, const Rest&... rest) {
    return (first + ... + rest);
}
template <class First, class... Rest>
    requires(quantity_like<First> || ... || quantity_like<Rest>)
[[nodiscard]] constexpr auto mul_all(const First& first, const Rest&... rest) {
    return (first * ... * rest);
}

// sum<Q>(r): the quantities of r in Q's unit, added by inside's sum with one
// range check.
template <class Q, std::ranges::input_range Rng>
    requires quantity_like<Q> && quantity_like<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>> &&
             detail::implicit_v<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>::reference, Q::reference> &&
             detail::exact_rescale_v<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>::reference.U,
                                     Q::reference.U>
[[nodiscard]] constexpr Q sum(Rng&& r) {
    return Q{beman::inside::sum<typename Q::rep>(std::forward<Rng>(r) | std::views::transform([](const auto& q) {
                                                     return q.template in<Q::reference.U>().numerical_value();
                                                 }))};
}

// parse<Q, F>(text) / parse_exact<Q>(text): the number, in Q's unit.
template <class Q, policy_flag F = none>
    requires quantity_like<Q>
[[nodiscard]] constexpr std::expected<Q, errc> parse(std::string_view text) {
    return detail::wrap_out<Q>(beman::inside::parse<typename Q::rep, F>(text));
}
template <class Q>
    requires quantity_like<Q>
[[nodiscard]] constexpr std::expected<Q, errc> parse_exact(std::string_view text) {
    return detail::wrap_out<Q>(beman::inside::parse_exact<typename Q::rep>(text));
}

// conversion_overflows<Q>(v) / conversion_rounds / conversion_loses: v, a
// number in Q's unit, against Q's grid.
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_overflows(A value) noexcept {
    return beman::inside::conversion_overflows<typename Q::rep>(value);
}
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_rounds(A value) noexcept {
    return beman::inside::conversion_rounds<typename Q::rep>(value);
}
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_loses(A value) noexcept {
    return beman::inside::conversion_loses<typename Q::rep>(value);
}

// clamp_cast<Q, Round>(x) / wrap_cast<Q>(x): x a number in Q's unit, or a
// quantity converted to it exactly first.
namespace detail {
template <class Q, class X>
constexpr decltype(auto) number_in(const X& x) {
    if constexpr (quantity_like<X>)
        return number_in_unit<Q::reference.U>(x);
    else
        return (x);
}
template <class Q, class X>
concept castable_quantity =
    quantity_like<X> && implicit_v<X::reference, Q::reference> && exact_rescale_v<X::reference.U, Q::reference.U>;
template <class Q, class X>
concept castable_from = quantity_like<Q> && (castable_quantity<Q, X> || (!quantity_like<X> && numeric<X>));
} // namespace detail
template <class Q, policy_flag Round = none, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::clamp_cast<typename Q::rep, Round>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr Q clamp_cast(const X& x) {
    return Q{beman::inside::clamp_cast<typename Q::rep, Round>(detail::number_in<Q>(x))};
}
template <class Q, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::wrap_cast<typename Q::rep>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr Q wrap_cast(const X& x) {
    return Q{beman::inside::wrap_cast<typename Q::rep>(detail::number_in<Q>(x))};
}

// dot, cross, lerp on quantity components (lerp's t is a plain number).
template <quantity_like AX, quantity_like AY, quantity_like BX, quantity_like BY>
[[nodiscard]] constexpr auto dot(const AX& ax, const AY& ay, const BX& bx, const BY& by)
    -> decltype(ax * bx + ay * by) {
    return ax * bx + ay * by;
}
template <quantity_like AX, quantity_like AY, quantity_like BX, quantity_like BY>
[[nodiscard]] constexpr auto cross(const AX& ax, const AY& ay, const BX& bx, const BY& by)
    -> decltype(ax * by - ay * bx) {
    return ax * by - ay * bx;
}
template <quantity_like A, quantity_like B, insidable T>
[[nodiscard]] constexpr auto lerp(const A& a, const B& b, const T& t) -> decltype(a + (b - a) * t) {
    return a + (b - a) * t;
}

// divmod(a, b, args...) of two quantities of one dimension, in their common
// unit: the quotient is a plain number, the remainder a quantity.
template <class Q, class R>
struct divmod_result {
    Q Quotient;
    R Remainder;
};
namespace detail {
// The remainder as a quantity in C (an expected's error passed on).
template <reference C, class D>
constexpr auto quantity_divmod(D&& d) {
    const auto with_unit = [](const auto& v) {
        using M = quantity<C, decltype(v.Remainder)>;
        return divmod_result<decltype(v.Quotient), M>{v.Quotient, M{v.Remainder}};
    };
    if constexpr (is_expected_v<std::remove_cvref_t<D>>)
        return d.transform(with_unit);
    else
        return with_unit(d);
}
} // namespace detail
template <reference A, class VA, reference B, class VB, class... Args>
    requires detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x,
                      const detail::common_number_t<B, VB, A>& y,
                      Args&&... args) { beman::inside::divmod(x, y, std::forward<Args>(args)...); }
[[nodiscard]] constexpr auto divmod(const quantity<A, VA>& a, const quantity<B, VB>& b, Args&&... args) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::quantity_divmod<C>(beman::inside::divmod(
        detail::in_common_unit<A, B>(a), detail::in_common_unit<A, B>(b), std::forward<Args>(args)...));
}

//---------------------------------------------------------------------------
// quantity_point<R, O, Rep> — a point: a quantity of R counted from origin O
// (null: the implicit zero of the dimension). A change of origin shifts the
// grid by the exact offset, a change of unit rescales it.
//   auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});   // 20 ℃
//   t.quantity_from(si::absolute_zero)                         // 293.15 K… in ℃
//   t.point_for(si::absolute_zero).in(si::kelvin)              // the point 293.15 K
//---------------------------------------------------------------------------
template <reference R, detail::origin_ref O, insidable Rep>
class quantity_point;

template <class T>
inline constexpr bool is_quantity_point_v = false;
template <reference R, detail::origin_ref O, class Rep>
inline constexpr bool is_quantity_point_v<quantity_point<R, O, Rep>> = true;

namespace detail {
// The number, in unit U, that takes a quantity from origin From to origin To:
// (offset(From) − offset(To)) / mag(U).
template <origin_ref From, origin_ref To, unit U>
consteval grid_rational origin_shift() {
    static_assert(same(absolute_of(From), absolute_of(To)), "units: points of different absolute origins");
    constexpr magnitude m = magnitude_of(U);
    static_assert(m.Pi == 0, "units: an origin shift in a unit with a pi magnitude has no exact grid");
    return (offset_of(From) - offset_of(To)) / m.Ratio;
}

// Whether shift<C> forms on V: inside's v + C exists.
template <grid_rational C, class V>
concept shift_forms = C == grid_rational{0} || requires(const V& v) { v + just<C>; };

template <grid_rational C, insidable V>
constexpr auto shift(const V& v) {
    if constexpr (C == grid_rational{0})
        return v;
    else
        return v + just<C>;
}

// The default origin of a unit's points: its named unit's (K, ℃, ℉).
consteval origin_ref origin_of_unit(const unit& u) { return is_single_named(u) ? u.Atoms[0].U->Origin : origin_ref{}; }

template <origin_ref O, class Q>
constexpr auto make_point(Q&& q) {
    using T = beman::inside::detail::unwrap_t<Q>;
    return lift_to<quantity_point<T::reference, O, typename T::rep>>(std::forward<Q>(q));
}
} // namespace detail

template <reference R, detail::origin_ref O, insidable Rep>
class quantity_point {
    quantity<R, Rep> Q;

  public:
    using quantity_type                           = quantity<R, Rep>;
    using rep                                     = Rep;
    static constexpr units::reference   reference = R;
    static constexpr detail::origin_ref origin    = O;

    constexpr quantity_point() = default;
    constexpr explicit quantity_point(const quantity<R, Rep>& q) : Q{q} {}

    // From a point of another origin (of the same absolute one) or unit:
    // shifted, then rescaled, exactly; implicit as the quantities are.
    template <units::reference R2, detail::origin_ref O2, class V2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O))) && detail::implicit_v<R2, R> &&
                std::constructible_from<
                    quantity<R, Rep>,
                    decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{}))>
    constexpr explicit(
        !std::convertible_to<decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{})),
                             quantity<R, Rep>>) quantity_point(const quantity_point<R2, O2, V2>& p)
        : Q(p.quantity_from(origin_t<O>{})) {}

    // The quantity from this point's origin, or from another one.
    [[nodiscard]] constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept { return Q; }
    template <detail::origin_ref O2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.U).Pi == 0 &&
                 detail::shift_forms<detail::origin_shift<O, O2, R.U>(), Rep>)
    [[nodiscard]] constexpr auto quantity_from(origin_t<O2>) const {
        constexpr grid_rational c = detail::origin_shift<O, O2, R.U>();
        return quantity<R, std::remove_cvref_t<decltype(detail::shift<c>(Q.numerical_value()))>>{
            detail::shift<c>(Q.numerical_value())};
    }
    template <detail::origin_ref O2>
        requires(!detail::same(detail::absolute_of(O2), detail::absolute_of(O)))
    void quantity_from(origin_t<O2>) const = delete ("units: points of different absolute origins");
    template <detail::origin_ref O2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.U).Pi != 0)
    void quantity_from(origin_t<O2>) const =
        delete ("units: an origin shift in a unit with a pi magnitude has no exact grid");
    // The quantity from the absolute origin of the dimension (absolute zero,
    // or the implicit zero): 20 ℃ → 293.15 K counted in ℃.
    [[nodiscard]] constexpr auto quantity_from_zero() const
        requires requires(const quantity_point& p) { p.quantity_from(origin_t<detail::absolute_of(O)>{}); }
    {
        return quantity_from(origin_t<detail::absolute_of(O)>{});
    }
    // The same point counted from another origin.
    template <detail::origin_ref O2>
        requires requires(const quantity_point& p, origin_t<O2> o) { p.quantity_from(o); }
    [[nodiscard]] constexpr auto point_for(origin_t<O2> o) const {
        return detail::make_point<O2>(quantity_from(o));
    }
    // The same point in another unit.
    template <units::unit U>
        requires requires(const quantity<R, Rep>& q, unit_t<U> u) { q.in(u); }
    [[nodiscard]] constexpr auto in(unit_t<U> u) const {
        return detail::make_point<O>(Q.in(u));
    }
    // The same point in unit U, counted from U's own origin: 350 ℉ → 530/3 ℃.
    template <units::unit U>
        requires requires(const quantity_point& p, unit_t<U> u) {
            p.point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
        }
    [[nodiscard]] constexpr auto point_in(unit_t<U> u) const {
        return point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
    }

    template <units::reference R2, class V2>
        requires requires(quantity<R, Rep>& a, const quantity<R2, V2>& b) { a += b; }
    constexpr quantity_point& operator+=(const quantity<R2, V2>& q) {
        Q += q;
        return *this;
    }
    template <units::reference R2, class V2>
        requires requires(quantity<R, Rep>& a, const quantity<R2, V2>& b) { a -= b; }
    constexpr quantity_point& operator-=(const quantity<R2, V2>& q) {
        Q -= q;
        return *this;
    }
};

// value_cast on a point: its quantity from the origin cast, the origin kept.
template <insidable Rep2, reference R, detail::origin_ref O, class V>
    requires std::constructible_from<Rep2, const V&>
[[nodiscard]] constexpr quantity_point<R, O, Rep2> value_cast(const quantity_point<R, O, V>& p) {
    return quantity_point<R, O, Rep2>{value_cast<Rep2>(p.quantity_from_origin())};
}
template <auto U, insidable Rep2, reference R, detail::origin_ref O, class V>
    requires requires(const quantity<R, V>& q) { value_cast<U, Rep2>(q); }
[[nodiscard]] constexpr auto value_cast(const quantity_point<R, O, V>& p) {
    const auto q = value_cast<U, Rep2>(p.quantity_from_origin());
    return quantity_point<std::remove_cvref_t<decltype(q)>::reference, O, Rep2>{q};
}

// point<si::degree_Celsius>(v) — a point at the unit's default origin (the
// implicit zero for a unit without one).
template <auto U, insidable V>
[[nodiscard]] constexpr auto point(const V& v) {
    return quantity_point<reference_of(U), detail::origin_of_unit(decltype(U)::value), V>{
        quantity<reference_of(U), V>{v}};
}
// delta<si::degree_Celsius>(v) — a difference: a plain quantity.
template <auto U, insidable V>
[[nodiscard]] constexpr auto delta(const V& v) {
    return v * U;
}

// origin + quantity: si::absolute_zero + just<300> * si::kelvin.
template <detail::origin_ref O, reference R, class V>
[[nodiscard]] constexpr auto operator+(origin_t<O>, const quantity<R, V>& q) {
    return quantity_point<R, O, V>{q};
}
template <reference R, class V, detail::origin_ref O>
[[nodiscard]] constexpr auto operator+(const quantity<R, V>& q, origin_t<O>) {
    return quantity_point<R, O, V>{q};
}

// point ± quantity: a point at the same origin, in the point's unit when the
// quantity converts to it exactly (5 K onto a ℃ point stays ℃), else in the
// common unit; point − point: a quantity (the second counted from the first's
// origin).
namespace detail {
template <unit U, reference R2, class V2>
constexpr auto in_point_unit(const quantity<R2, V2>& q) {
    if constexpr (requires { q.template in<U>(); })
        return q.template in<U>();
    else
        return q;
}
} // namespace detail
template <reference R, detail::origin_ref O, class V, reference R2, class V2>
    requires requires(const quantity<R, V>& a, const quantity<R2, V2>& b) { a + detail::in_point_unit<R.U>(b); }
[[nodiscard]] constexpr auto operator+(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() + detail::in_point_unit<R.U>(q));
}
template <reference R2, class V2, reference R, detail::origin_ref O, class V>
    requires requires(const quantity<R2, V2>& a, const quantity<R, V>& b) { detail::in_point_unit<R.U>(a) + b; }
[[nodiscard]] constexpr auto operator+(const quantity<R2, V2>& q, const quantity_point<R, O, V>& p) {
    return detail::make_point<O>(detail::in_point_unit<R.U>(q) + p.quantity_from_origin());
}
template <reference R, detail::origin_ref O, class V, reference R2, class V2>
    requires requires(const quantity<R, V>& a, const quantity<R2, V2>& b) { a - detail::in_point_unit<R.U>(b); }
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() - detail::in_point_unit<R.U>(q));
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr bool operator==(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr auto operator<=>(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
}

//---------------------------------------------------------------------------
// Text: a unit's symbol (kg m²/s², km/h, 1/s), a quantity as "5 km".
//---------------------------------------------------------------------------
enum class text_encoding : unsigned char { utf8, ascii };

namespace detail {
constexpr std::string_view prefix_symbol(int p, text_encoding e) {
    // 10^-2 … 10^2, then 10^-30 … 10^30 in steps of 3.
    constexpr std::string_view near_one[]  = {"c", "d", "", "da", "h"};
    constexpr std::string_view thousands[] = {"q", "r", "y", "z", "a", "f", "p", "n", "µ", "m", "",
                                              "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};
    if (p == -6 && e == text_encoding::ascii)
        return "u";
    if (p >= -2 && p <= 2)
        return near_one[p + 2];
    if (p >= -30 && p <= 30 && p % 3 == 0)
        return thousands[(p + 30) / 3];
    return "?";
}

inline void append_exponent(std::string& s, int e, text_encoding enc) {
    if (e == 1)
        return;
    if (enc == text_encoding::ascii) {
        s += '^';
        s += std::to_string(e);
        return;
    }
    static constexpr std::string_view sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    if (e < 0) {
        s += "⁻";
        e = -e;
    }
    const std::string digits = std::to_string(e);
    for (char c : digits)
        s += sup[c - '0'];
}

// The atoms of one sign with |exponent| (sign ±1), or all of them with their
// own exponent (sign 0).
inline void append_atoms(std::string& s, const unit& u, int sign, text_encoding enc) {
    bool first = true;
    for (std::int8_t i = 0; i < u.Size; ++i) {
        const unit_atom& a = u.Atoms[i];
        if (sign != 0 && (a.Exp > 0) != (sign > 0))
            continue;
        if (!first)
            s += ' ';
        first = false;
        s += prefix_symbol(a.Prefix, enc);
        s += enc == text_encoding::utf8 ? a.U->Utf8 : a.U->Ascii;
        append_exponent(s, sign == 0 ? a.Exp : a.Exp * sign, enc);
    }
}
} // namespace detail

// The symbol of a unit: "km", "kg m²/s²", "s⁻¹", "[1/381 ft]" (a scaled unit).
inline std::string unit_symbol(const unit& u, text_encoding enc = text_encoding::utf8) {
    std::string s;
    const int   num =
        static_cast<int>(std::count_if(u.Atoms, u.Atoms + u.Size, [](const auto& a) { return a.Exp > 0; }));
    const int den = u.Size - num;
    if (num == 0) // only a denominator: s⁻¹, m⁻¹ s⁻¹
        detail::append_atoms(s, u, 0, enc);
    else
        detail::append_atoms(s, u, 1, enc);
    if (num > 0 && den > 0) {
        s += '/';
        if (den > 1)
            s += '(';
        detail::append_atoms(s, u, -1, enc);
        if (den > 1)
            s += ')';
    }
    if (!u.Scale.is_one()) {
        // "1/1250", "π", "2π", "π⁻¹" (ASCII "pi^-1").
        std::string scale =
            u.Scale.Pi != 0 && u.Scale.Ratio == grid_rational{1} ? "" : beman::inside::to_string(u.Scale.Ratio);
        if (u.Scale.Pi != 0) {
            scale += enc == text_encoding::utf8 ? "π" : (scale.empty() ? "pi" : "*pi");
            detail::append_exponent(scale, u.Scale.Pi, enc);
        }
        s = "[" + scale + (s.empty() ? "" : " " + s) + "]";
    }
    return s;
}
template <unit U>
inline std::string unit_symbol(unit_t<U>, text_encoding enc = text_encoding::utf8) {
    return unit_symbol(U, enc);
}

// "5 km", "90°", "3" (unit one).
template <reference R, class V>
std::string to_string(const quantity<R, V>& q, text_encoding enc = text_encoding::utf8) {
    std::string       s   = beman::inside::to_string(q.numerical_value());
    const std::string sym = unit_symbol(R.U, enc);
    if (sym.empty())
        return s;
    return s + (detail::space_before(R.U) ? " " : "") + sym;
}

template <reference R, class V>
std::ostream& operator<<(std::ostream& os, const quantity<R, V>& q) {
    return os << units::to_string(q);
}

// A point as its quantity from its origin: "20 ℃".
template <reference R, detail::origin_ref O, class V>
std::string to_string(const quantity_point<R, O, V>& p, text_encoding enc = text_encoding::utf8) {
    return units::to_string(p.quantity_from_origin(), enc);
}
template <reference R, detail::origin_ref O, class V>
std::ostream& operator<<(std::ostream& os, const quantity_point<R, O, V>& p) {
    return os << units::to_string(p);
}

} // namespace beman::inside::units

// std::format: the spec formats the number as for its inside ({:.2f}, {:>8}),
// then the unit symbol follows ("5 km", "90°").
template <beman::inside::units::reference R, class V>
struct std::formatter<beman::inside::units::quantity<R, V>> : std::formatter<V> {
    template <typename Ctx>
    auto format(const beman::inside::units::quantity<R, V>& q, Ctx& ctx) const {
        namespace u           = beman::inside::units;
        auto              out = std::formatter<V>::format(q.numerical_value(), ctx);
        const std::string sym = u::unit_symbol(R.U);
        if (sym.empty())
            return out;
        if constexpr (u::detail::space_before(R.U))
            *out++ = ' ';
        return std::ranges::copy(sym, out).out;
    }
};

template <beman::inside::units::reference R, beman::inside::units::detail::origin_ref O, class V>
struct std::formatter<beman::inside::units::quantity_point<R, O, V>>
    : std::formatter<beman::inside::units::quantity<R, V>> {
    template <typename Ctx>
    auto format(const beman::inside::units::quantity_point<R, O, V>& p, Ctx& ctx) const {
        return std::formatter<beman::inside::units::quantity<R, V>>::format(p.quantity_from_origin(), ctx);
    }
};

namespace beman::inside::units {
//---------------------------------------------------------------------------
// The catalog: ISQ quantity specs, SI units and prefixes, international units.
// Each name is a tag object; its definition is a static object in *_defs.
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_SPEC(name, ...)                         \
    namespace isq_defs {                                     \
    inline constexpr quantity_spec name{#name, __VA_ARGS__}; \
    }                                                        \
    namespace isq {                                          \
    inline constexpr auto name = spec_c<isq_defs::name>;     \
    }
#define BEMAN_INSIDE_ORIGIN(ns, name, ...)                      \
    namespace ns##_defs {                                       \
        inline constexpr point_origin name{#name, __VA_ARGS__}; \
    }                                                           \
    namespace ns {                                              \
    inline constexpr auto name = origin_c<ns##_defs::name>;     \
    }
#define BEMAN_INSIDE_UNIT(ns, name, ...)                  \
    namespace ns##_defs {                                 \
        inline constexpr named_unit name{__VA_ARGS__};    \
    }                                                     \
    namespace ns {                                        \
    inline constexpr auto name = unit_c<ns##_defs::name>; \
    }

// Base quantities (ISO 80000-1) and the dimensionless one.
BEMAN_INSIDE_SPEC(dimensionless, dim_one)
BEMAN_INSIDE_SPEC(length, dim_length)
BEMAN_INSIDE_SPEC(mass, dim_mass)
BEMAN_INSIDE_SPEC(duration, dim_time)
BEMAN_INSIDE_SPEC(electric_current, dim_electric_current)
BEMAN_INSIDE_SPEC(thermodynamic_temperature, dim_thermodynamic_temperature)
BEMAN_INSIDE_SPEC(amount_of_substance, dim_amount_of_substance)
BEMAN_INSIDE_SPEC(luminous_intensity, dim_luminous_intensity)
namespace isq {
inline constexpr auto time = duration;
}

// Space and time (ISO 80000-3).
BEMAN_INSIDE_SPEC(width, isq::length)
BEMAN_INSIDE_SPEC(thickness, isq::width)
BEMAN_INSIDE_SPEC(diameter, isq::width)
BEMAN_INSIDE_SPEC(radius, isq::width)
BEMAN_INSIDE_SPEC(altitude, isq::length)
BEMAN_INSIDE_SPEC(height, isq::altitude)
BEMAN_INSIDE_SPEC(path_length, isq::length)
BEMAN_INSIDE_SPEC(arc_length, isq::path_length)
BEMAN_INSIDE_SPEC(distance, isq::path_length)
BEMAN_INSIDE_SPEC(radial_distance, isq::distance)
BEMAN_INSIDE_SPEC(wavelength, isq::length)
BEMAN_INSIDE_SPEC(displacement, isq::length)
BEMAN_INSIDE_SPEC(position_vector, isq::displacement)
BEMAN_INSIDE_SPEC(area, pow<2>(isq::length))
BEMAN_INSIDE_SPEC(volume, pow<3>(isq::length))
BEMAN_INSIDE_SPEC(angular_measure, isq::dimensionless, isq::arc_length / isq::radius, is_kind)
BEMAN_INSIDE_SPEC(rotational_displacement, isq::angular_measure, isq::path_length / isq::radius)
BEMAN_INSIDE_SPEC(phase_angle, isq::angular_measure, is_kind)
BEMAN_INSIDE_SPEC(solid_angular_measure, isq::dimensionless, isq::area / pow<2>(isq::radius), is_kind)
BEMAN_INSIDE_SPEC(period_duration, isq::duration)
BEMAN_INSIDE_SPEC(time_constant, isq::duration)
BEMAN_INSIDE_SPEC(speed, isq::length / isq::duration)
BEMAN_INSIDE_SPEC(velocity, isq::speed, isq::displacement / isq::duration)
BEMAN_INSIDE_SPEC(acceleration, isq::velocity / isq::duration)
BEMAN_INSIDE_SPEC(acceleration_of_free_fall, isq::acceleration)
BEMAN_INSIDE_SPEC(angular_velocity, isq::rotational_displacement / isq::duration)
BEMAN_INSIDE_SPEC(angular_acceleration, isq::angular_velocity / isq::duration)
BEMAN_INSIDE_SPEC(frequency, inverse(isq::period_duration))
BEMAN_INSIDE_SPEC(rotation, isq::dimensionless)
BEMAN_INSIDE_SPEC(rotational_frequency, isq::rotation / isq::duration)
BEMAN_INSIDE_SPEC(angular_frequency, isq::phase_angle / isq::duration)

// Mechanics (ISO 80000-4).
BEMAN_INSIDE_SPEC(mass_density, isq::mass / isq::volume)
BEMAN_INSIDE_SPEC(momentum, isq::mass* isq::velocity)
BEMAN_INSIDE_SPEC(force, isq::mass* isq::length / pow<2>(isq::duration))
BEMAN_INSIDE_SPEC(weight, isq::force, isq::mass* isq::acceleration_of_free_fall)
BEMAN_INSIDE_SPEC(impulse, isq::momentum, isq::force* isq::duration)
BEMAN_INSIDE_SPEC(moment_of_force, isq::position_vector* isq::force)
BEMAN_INSIDE_SPEC(torque, isq::moment_of_force)
BEMAN_INSIDE_SPEC(pressure, isq::force / isq::area)
BEMAN_INSIDE_SPEC(gauge_pressure, isq::pressure)
BEMAN_INSIDE_SPEC(energy, isq::mass* pow<2>(isq::length) / pow<2>(isq::duration))
BEMAN_INSIDE_SPEC(mechanical_work, isq::energy, isq::force* isq::displacement)
BEMAN_INSIDE_SPEC(mechanical_energy, isq::mechanical_work)
BEMAN_INSIDE_SPEC(potential_energy, isq::mechanical_energy)
BEMAN_INSIDE_SPEC(kinetic_energy, isq::mechanical_energy, isq::mass* pow<2>(isq::speed))
BEMAN_INSIDE_SPEC(power, isq::mass* pow<2>(isq::length) / pow<3>(isq::duration))
BEMAN_INSIDE_SPEC(mechanical_power, isq::power, isq::force* isq::velocity)
BEMAN_INSIDE_SPEC(action, isq::energy* isq::duration)
BEMAN_INSIDE_SPEC(mass_flow_rate, isq::mass / isq::duration)
BEMAN_INSIDE_SPEC(volume_flow_rate, isq::volume / isq::duration)
namespace isq {
inline constexpr auto work = mechanical_work;
}

// Thermodynamics (ISO 80000-5).
BEMAN_INSIDE_SPEC(internal_energy, isq::energy)
BEMAN_INSIDE_SPEC(heat, isq::internal_energy)
BEMAN_INSIDE_SPEC(enthalpy, isq::internal_energy)
BEMAN_INSIDE_SPEC(heat_capacity, isq::heat / isq::thermodynamic_temperature)
BEMAN_INSIDE_SPEC(entropy, isq::heat / isq::thermodynamic_temperature)
BEMAN_INSIDE_SPEC(heat_flow_rate, isq::heat / isq::duration)

// Electromagnetism (IEC 80000-6).
BEMAN_INSIDE_SPEC(electric_charge, isq::electric_current* isq::duration)
BEMAN_INSIDE_SPEC(electric_potential, isq::energy / (isq::electric_current * isq::duration))
BEMAN_INSIDE_SPEC(voltage, isq::electric_potential)
BEMAN_INSIDE_SPEC(capacitance, isq::electric_current* isq::duration / isq::electric_potential)
BEMAN_INSIDE_SPEC(impedance, isq::electric_potential / isq::electric_current)
BEMAN_INSIDE_SPEC(resistance, isq::impedance, isq::voltage / isq::electric_current)
BEMAN_INSIDE_SPEC(admittance, inverse(isq::impedance))
BEMAN_INSIDE_SPEC(conductance, isq::admittance, inverse(isq::resistance))
BEMAN_INSIDE_SPEC(magnetic_flux_density, isq::mass / (isq::electric_current * pow<2>(isq::duration)))
BEMAN_INSIDE_SPEC(magnetic_flux, isq::magnetic_flux_density* isq::area)
BEMAN_INSIDE_SPEC(inductance, isq::magnetic_flux / isq::electric_current)
BEMAN_INSIDE_SPEC(electric_power, isq::power)

// Light (ISO 80000-7), photopic vision.
BEMAN_INSIDE_SPEC(luminous_flux, isq::luminous_intensity* isq::solid_angular_measure)
BEMAN_INSIDE_SPEC(illuminance, isq::luminous_flux / isq::area)

// Physical chemistry and ionizing radiation (ISO 80000-9, -10).
BEMAN_INSIDE_SPEC(catalytic_activity, isq::amount_of_substance / isq::duration)
BEMAN_INSIDE_SPEC(activity, inverse(isq::duration))
BEMAN_INSIDE_SPEC(absorbed_dose, isq::energy / isq::mass)
BEMAN_INSIDE_SPEC(ionizing_radiation_quality_factor, isq::dimensionless)
BEMAN_INSIDE_SPEC(dose_equivalent, isq::absorbed_dose* isq::ionizing_radiation_quality_factor)

// SI prefixes. Past 10^±18 a prefix needs a grid number past 64 bits.
namespace si {
inline constexpr prefix_t<-30> quecto{};
inline constexpr prefix_t<-27> ronto{};
inline constexpr prefix_t<-24> yocto{};
inline constexpr prefix_t<-21> zepto{};
inline constexpr prefix_t<-18> atto{};
inline constexpr prefix_t<-15> femto{};
inline constexpr prefix_t<-12> pico{};
inline constexpr prefix_t<-9>  nano{};
inline constexpr prefix_t<-6>  micro{};
inline constexpr prefix_t<-3>  milli{};
inline constexpr prefix_t<-2>  centi{};
inline constexpr prefix_t<-1>  deci{};
inline constexpr prefix_t<1>   deca{};
inline constexpr prefix_t<2>   hecto{};
inline constexpr prefix_t<3>   kilo{};
inline constexpr prefix_t<6>   mega{};
inline constexpr prefix_t<9>   giga{};
inline constexpr prefix_t<12>  tera{};
inline constexpr prefix_t<15>  peta{};
inline constexpr prefix_t<18>  exa{};
inline constexpr prefix_t<21>  zetta{};
inline constexpr prefix_t<24>  yotta{};
inline constexpr prefix_t<27>  ronna{};
inline constexpr prefix_t<30>  quetta{};
} // namespace si

// SI base units.
BEMAN_INSIDE_UNIT(si, second, "s", isq::duration)
BEMAN_INSIDE_UNIT(si, metre, "m", isq::length)
BEMAN_INSIDE_UNIT(si, gram, "g", mag<rational{1, 1000}>, isq::mass)
namespace si {
inline constexpr auto kilogram = kilo(gram);
}
BEMAN_INSIDE_UNIT(si, ampere, "A", isq::electric_current)
BEMAN_INSIDE_ORIGIN(si, absolute_zero, dim_thermodynamic_temperature)
BEMAN_INSIDE_ORIGIN(si, ice_point, dim_thermodynamic_temperature, &si_defs::absolute_zero, rational{27315, 100})
BEMAN_INSIDE_UNIT(si, kelvin, "K", isq::thermodynamic_temperature, {.origin = si::absolute_zero})
BEMAN_INSIDE_UNIT(si, mole, "mol", isq::amount_of_substance)
BEMAN_INSIDE_UNIT(si, candela, "cd", isq::luminous_intensity)

// SI derived units with special names.
BEMAN_INSIDE_UNIT(si, radian, "rad", si::metre / si::metre, isq::angular_measure)
BEMAN_INSIDE_UNIT(si, steradian, "sr", square(si::metre) / square(si::metre), isq::solid_angular_measure)
BEMAN_INSIDE_UNIT(si, hertz, "Hz", one / si::second, isq::frequency)
BEMAN_INSIDE_UNIT(si, newton, "N", si::kilogram* si::metre / square(si::second), isq::force)
BEMAN_INSIDE_UNIT(si, pascal, "Pa", si::newton / square(si::metre), isq::pressure)
BEMAN_INSIDE_UNIT(si, joule, "J", si::newton* si::metre, isq::energy)
BEMAN_INSIDE_UNIT(si, watt, "W", si::joule / si::second)
BEMAN_INSIDE_UNIT(si, coulomb, "C", si::ampere* si::second)
BEMAN_INSIDE_UNIT(si, volt, "V", si::watt / si::ampere, isq::electric_potential)
BEMAN_INSIDE_UNIT(si, farad, "F", si::coulomb / si::volt, isq::capacitance)
BEMAN_INSIDE_UNIT(si, ohm, "Ω", si::volt / si::ampere, isq::impedance, {.ascii = "ohm"})
BEMAN_INSIDE_UNIT(si, siemens, "S", one / si::ohm, isq::admittance)
BEMAN_INSIDE_UNIT(si, weber, "Wb", si::volt* si::second)
BEMAN_INSIDE_UNIT(si, tesla, "T", si::weber / square(si::metre), isq::magnetic_flux_density)
BEMAN_INSIDE_UNIT(si, henry, "H", si::weber / si::ampere)
BEMAN_INSIDE_UNIT(si, degree_Celsius, "℃", si::kelvin, {.ascii = "`C", .origin = si::ice_point})
BEMAN_INSIDE_UNIT(si, lumen, "lm", si::candela* si::steradian, isq::luminous_flux)
BEMAN_INSIDE_UNIT(si, lux, "lx", si::lumen / square(si::metre), isq::illuminance)
BEMAN_INSIDE_UNIT(si, becquerel, "Bq", one / si::second, isq::activity)
BEMAN_INSIDE_UNIT(si, gray, "Gy", si::joule / si::kilogram, isq::absorbed_dose)
BEMAN_INSIDE_UNIT(si, sievert, "Sv", si::joule / si::kilogram, isq::dose_equivalent)
BEMAN_INSIDE_UNIT(si, katal, "kat", si::mole / si::second, isq::catalytic_activity)

// Non-SI units accepted for use with the SI.
BEMAN_INSIDE_UNIT(si, minute, "min", mag<60>* si::second)
BEMAN_INSIDE_UNIT(si, hour, "h", mag<60>* si::minute)
BEMAN_INSIDE_UNIT(si, day, "d", mag<24>* si::hour)
BEMAN_INSIDE_UNIT(si, astronomical_unit, "au", mag<149'597'870'700>* si::metre)
BEMAN_INSIDE_UNIT(si, degree, "°", mag_pi* mag<rational{1, 180}>* si::radian, {.ascii = "deg", .space_before = false})
BEMAN_INSIDE_UNIT(si, arcminute, "′", mag<rational{1, 60}>* si::degree, {.ascii = "'", .space_before = false})
BEMAN_INSIDE_UNIT(si, arcsecond, "″", mag<rational{1, 60}>* si::arcminute, {.ascii = "''", .space_before = false})
BEMAN_INSIDE_UNIT(si, are, "a", square(si::deca(si::metre)))
BEMAN_INSIDE_UNIT(si, litre, "L", cubic(si::deci(si::metre)))
BEMAN_INSIDE_UNIT(si, tonne, "t", mag<1000>* si::kilogram)
namespace si {
inline constexpr auto hectare = hecto(are);
}
BEMAN_INSIDE_UNIT(si, dalton, "Da", mag<1.66053906892e-27_g>* si::kilogram) // CODATA 2022
BEMAN_INSIDE_UNIT(si, electronvolt, "eV", mag<1.602176634e-19_g>* si::joule)

// The defining constants of the SI (2019) and standard gravity, as units:
// just<1> * si2019::speed_of_light_in_vacuum is c, exactly, and .in(m/s)
// gives its number. h, e, k and N_A need grid numbers past 64 bits.
BEMAN_INSIDE_UNIT(
    si2019, hyperfine_structure_transition_frequency_of_cs, "Δν_Cs", mag<9'192'631'770>* si::hertz, {.ascii = "dv_Cs"})
BEMAN_INSIDE_UNIT(si2019, speed_of_light_in_vacuum, "c", mag<299'792'458>* si::metre / si::second)
BEMAN_INSIDE_UNIT(si2019, luminous_efficacy, "K_cd", mag<683>* si::lumen / si::watt)
BEMAN_INSIDE_UNIT(
    si2019, standard_gravity, "g₀", mag<rational{980665, 100000}>* si::metre / square(si::second), {.ascii = "g_0"})
BEMAN_INSIDE_UNIT(si2019, planck_constant, "h", mag<6.62607015e-34_g>* si::joule* si::second)
BEMAN_INSIDE_UNIT(si2019, elementary_charge, "e", mag<1.602176634e-19_g>* si::coulomb)
BEMAN_INSIDE_UNIT(si2019, boltzmann_constant, "k", mag<1.380649e-23_g>* si::joule / si::kelvin)
BEMAN_INSIDE_UNIT(si2019, avogadro_constant, "N_A", mag<6.02214076e23_g> / si::mole)

// International (yard and pound agreement, 1959) and nautical units.
BEMAN_INSIDE_UNIT(international, yard, "yd", mag<rational{9144, 10000}>* si::metre)
BEMAN_INSIDE_UNIT(international, foot, "ft", mag<rational{1, 3}>* international::yard)
BEMAN_INSIDE_UNIT(international, inch, "in", mag<rational{1, 12}>* international::foot)
BEMAN_INSIDE_UNIT(international, mile, "mi", mag<1760>* international::yard)
BEMAN_INSIDE_UNIT(international, nautical_mile, "nmi", mag<1852>* si::metre)
BEMAN_INSIDE_UNIT(international, pound, "lb", mag<rational{45359237, 100000000}>* si::kilogram)
BEMAN_INSIDE_UNIT(international, ounce, "oz", mag<rational{1, 16}>* international::pound)
BEMAN_INSIDE_UNIT(international,
                  pound_force,
                  "lbf",
                  mag<rational{980665, 100000}>* international::pound* si::metre / square(si::second))
BEMAN_INSIDE_UNIT(international, knot, "kn", international::nautical_mile / si::hour)
BEMAN_INSIDE_ORIGIN(international,
                    zeroth_degree_Fahrenheit,
                    dim_thermodynamic_temperature,
                    &si_defs::absolute_zero,
                    rational{45967, 180})
BEMAN_INSIDE_UNIT(international,
                  degree_Fahrenheit,
                  "℉",
                  mag<rational{5, 9}>* si::kelvin,
                  {.ascii = "`F", .origin = international::zeroth_degree_Fahrenheit})

// Scaled dimensionless units, as in mp-units: 8 % is the number 2/25.
namespace dimensionless_defs {
inline constexpr named_unit percent{"%", mag<rational{1, 100}> * one};
inline constexpr named_unit per_mille{"‰", mag<rational{1, 1000}> * one, {.ascii = "permille"}};
} // namespace dimensionless_defs
inline constexpr auto percent   = unit_c<dimensionless_defs::percent>;
inline constexpr auto per_mille = unit_c<dimensionless_defs::per_mille>;

#undef BEMAN_INSIDE_UNIT
#undef BEMAN_INSIDE_ORIGIN
#undef BEMAN_INSIDE_SPEC

} // namespace beman::inside::units

#undef BEMAN_INSIDE_UNITS_WHY_PI


//---------------------------------------------------------------------------
// constants.hpp — mathematical and physical constants.
//
// beman::inside::math            mathematical constants as point insides:
//                                 pi, e, sqrt2, ln2, phi, egamma, … as
//                                 rationals within 2^-57 of the value
//                                 (detail/math_constants.hpp; cmath.hpp has
//                                 them too).
// beman::inside::units::constants physical constants as quantities of their
//                                 ISQ kind, each the number 1 of the
//                                 constant's own unit, so products stay
//                                 exact and .in(u) gives the SI number.
//
// Exact: the SI 2019 defining constants and what follows from them (ħ, R, F,
// K_J, R_K, G₀, Φ₀, σ, c₁, c₂), standard gravity and the standard atmosphere.
// Measured: the CODATA 2022 recommended values (codata2022::), held exactly as
// published; their uncertainty is not represented.
//---------------------------------------------------------------------------
namespace beman::inside::units {
//---------------------------------------------------------------------------
// The constants' units. Each is a named unit whose magnitude is the constant,
// in the namespace of its source, next to si2019's defining constants.
//---------------------------------------------------------------------------
namespace si2019_defs {
inline constexpr named_unit standard_atmosphere{"atm", mag<101'325> * si::pascal};
inline constexpr named_unit reduced_planck_constant{
    "ħ", si2019::planck_constant / (mag<2> * mag_pi), {.ascii = "hbar"}};
inline constexpr named_unit molar_gas_constant{"R", si2019::avogadro_constant * si2019::boltzmann_constant};
inline constexpr named_unit faraday_constant{"F", si2019::avogadro_constant * si2019::elementary_charge};
inline constexpr named_unit josephson_constant{"K_J", mag<2> * si2019::elementary_charge / si2019::planck_constant};
inline constexpr named_unit von_klitzing_constant{"R_K", si2019::planck_constant / square(si2019::elementary_charge)};
inline constexpr named_unit conductance_quantum{
    "G₀", mag<2> * square(si2019::elementary_charge) / si2019::planck_constant, {.ascii = "G_0"}};
inline constexpr named_unit magnetic_flux_quantum{
    "Φ₀", si2019::planck_constant / (mag<2> * si2019::elementary_charge), {.ascii = "Phi_0"}};
inline constexpr named_unit stefan_boltzmann_constant{
    "σ",
    mag<2> * mag_pi * mag_pi * mag_pi * mag_pi * mag_pi * square(square(si2019::boltzmann_constant)) /
        (mag<15> * cubic(si2019::planck_constant) * square(si2019::speed_of_light_in_vacuum)),
    {.ascii = "sigma"}};
inline constexpr named_unit first_radiation_constant{
    "c₁", mag<2> * mag_pi * si2019::planck_constant * square(si2019::speed_of_light_in_vacuum), {.ascii = "c_1"}};
inline constexpr named_unit second_radiation_constant{
    "c₂", si2019::planck_constant * si2019::speed_of_light_in_vacuum / si2019::boltzmann_constant, {.ascii = "c_2"}};
} // namespace si2019_defs

namespace si2019 {
inline constexpr auto standard_atmosphere       = unit_c<si2019_defs::standard_atmosphere>;
inline constexpr auto reduced_planck_constant   = unit_c<si2019_defs::reduced_planck_constant>;
inline constexpr auto molar_gas_constant        = unit_c<si2019_defs::molar_gas_constant>;
inline constexpr auto faraday_constant          = unit_c<si2019_defs::faraday_constant>;
inline constexpr auto josephson_constant        = unit_c<si2019_defs::josephson_constant>;
inline constexpr auto von_klitzing_constant     = unit_c<si2019_defs::von_klitzing_constant>;
inline constexpr auto conductance_quantum       = unit_c<si2019_defs::conductance_quantum>;
inline constexpr auto magnetic_flux_quantum     = unit_c<si2019_defs::magnetic_flux_quantum>;
inline constexpr auto stefan_boltzmann_constant = unit_c<si2019_defs::stefan_boltzmann_constant>;
inline constexpr auto first_radiation_constant  = unit_c<si2019_defs::first_radiation_constant>;
inline constexpr auto second_radiation_constant = unit_c<si2019_defs::second_radiation_constant>;
} // namespace si2019

// CODATA 2022 recommended values (Mohr et al., Rev. Mod. Phys. 97, 025002).
namespace codata2022_defs {
inline constexpr named_unit gravitational_constant{
    "G", mag<6.67430e-11_g> * cubic(si::metre) / (si::kilogram * square(si::second))};
inline constexpr named_unit fine_structure_constant{"α", mag<7.2973525643e-3_g> * one, {.ascii = "alpha"}};
inline constexpr named_unit rydberg_constant{"R_∞", mag<10'973'731.568157_g> / si::metre, {.ascii = "R_inf"}};
inline constexpr named_unit vacuum_magnetic_permeability{
    "μ₀", mag<1.25663706127e-6_g> * si::newton / square(si::ampere), {.ascii = "mu_0"}};
inline constexpr named_unit characteristic_impedance_of_vacuum{"Z₀", mag<376.730313412_g> * si::ohm, {.ascii = "Z_0"}};
inline constexpr named_unit electron_mass{"m_e", mag<9.1093837139e-31_g> * si::kilogram};
inline constexpr named_unit proton_mass{"m_p", mag<1.67262192595e-27_g> * si::kilogram};
inline constexpr named_unit neutron_mass{"m_n", mag<1.67492750056e-27_g> * si::kilogram};
inline constexpr named_unit atomic_mass_constant{"m_u", mag<1.66053906892e-27_g> * si::kilogram};
inline constexpr named_unit bohr_radius{"a₀", mag<5.29177210544e-11_g> * si::metre, {.ascii = "a_0"}};
inline constexpr named_unit vacuum_electric_permittivity{
    "ε₀", mag<8.8541878188e-12_g> * si::farad / si::metre, {.ascii = "epsilon_0"}};
} // namespace codata2022_defs

namespace codata2022 {
inline constexpr auto gravitational_constant             = unit_c<codata2022_defs::gravitational_constant>;
inline constexpr auto fine_structure_constant            = unit_c<codata2022_defs::fine_structure_constant>;
inline constexpr auto rydberg_constant                   = unit_c<codata2022_defs::rydberg_constant>;
inline constexpr auto vacuum_magnetic_permeability       = unit_c<codata2022_defs::vacuum_magnetic_permeability>;
inline constexpr auto characteristic_impedance_of_vacuum = unit_c<codata2022_defs::characteristic_impedance_of_vacuum>;
inline constexpr auto electron_mass                      = unit_c<codata2022_defs::electron_mass>;
inline constexpr auto proton_mass                        = unit_c<codata2022_defs::proton_mass>;
inline constexpr auto neutron_mass                       = unit_c<codata2022_defs::neutron_mass>;
inline constexpr auto atomic_mass_constant               = unit_c<codata2022_defs::atomic_mass_constant>;
inline constexpr auto bohr_radius                        = unit_c<codata2022_defs::bohr_radius>;
inline constexpr auto vacuum_electric_permittivity       = unit_c<codata2022_defs::vacuum_electric_permittivity>;
} // namespace codata2022

//---------------------------------------------------------------------------
// The constants as quantities: the number 1 of the constant's unit, under its
// ISQ kind where the kind is unambiguous.
//---------------------------------------------------------------------------
namespace detail {
template <reference R>
inline constexpr quantity<R, std::remove_cvref_t<decltype(just<1>)>> one_of{just<1>};
} // namespace detail

namespace constants {
// SI 2019 defining constants, standard gravity and the standard atmosphere.
inline constexpr auto hyperfine_structure_transition_frequency_of_cs =
    detail::one_of<isq::frequency[si2019::hyperfine_structure_transition_frequency_of_cs]>;
inline constexpr auto speed_of_light_in_vacuum = detail::one_of<isq::speed[si2019::speed_of_light_in_vacuum]>;
inline constexpr auto luminous_efficacy        = detail::one_of<si2019::luminous_efficacy>;
inline constexpr auto standard_gravity    = detail::one_of<isq::acceleration_of_free_fall[si2019::standard_gravity]>;
inline constexpr auto standard_atmosphere = detail::one_of<isq::pressure[si2019::standard_atmosphere]>;
inline constexpr auto planck_constant     = detail::one_of<isq::action[si2019::planck_constant]>;
inline constexpr auto reduced_planck_constant   = detail::one_of<isq::action[si2019::reduced_planck_constant]>;
inline constexpr auto elementary_charge         = detail::one_of<isq::electric_charge[si2019::elementary_charge]>;
inline constexpr auto boltzmann_constant        = detail::one_of<si2019::boltzmann_constant>;
inline constexpr auto avogadro_constant         = detail::one_of<si2019::avogadro_constant>;
inline constexpr auto molar_gas_constant        = detail::one_of<si2019::molar_gas_constant>;
inline constexpr auto faraday_constant          = detail::one_of<si2019::faraday_constant>;
inline constexpr auto josephson_constant        = detail::one_of<si2019::josephson_constant>;
inline constexpr auto von_klitzing_constant     = detail::one_of<isq::resistance[si2019::von_klitzing_constant]>;
inline constexpr auto conductance_quantum       = detail::one_of<isq::conductance[si2019::conductance_quantum]>;
inline constexpr auto magnetic_flux_quantum     = detail::one_of<isq::magnetic_flux[si2019::magnetic_flux_quantum]>;
inline constexpr auto stefan_boltzmann_constant = detail::one_of<si2019::stefan_boltzmann_constant>;
inline constexpr auto first_radiation_constant  = detail::one_of<si2019::first_radiation_constant>;
inline constexpr auto second_radiation_constant = detail::one_of<si2019::second_radiation_constant>;

// CODATA 2022 recommended values.
inline constexpr auto gravitational_constant       = detail::one_of<codata2022::gravitational_constant>;
inline constexpr auto fine_structure_constant      = detail::one_of<codata2022::fine_structure_constant>;
inline constexpr auto rydberg_constant             = detail::one_of<codata2022::rydberg_constant>;
inline constexpr auto vacuum_magnetic_permeability = detail::one_of<codata2022::vacuum_magnetic_permeability>;
inline constexpr auto characteristic_impedance_of_vacuum =
    detail::one_of<isq::impedance[codata2022::characteristic_impedance_of_vacuum]>;
inline constexpr auto electron_mass                = detail::one_of<isq::mass[codata2022::electron_mass]>;
inline constexpr auto proton_mass                  = detail::one_of<isq::mass[codata2022::proton_mass]>;
inline constexpr auto neutron_mass                 = detail::one_of<isq::mass[codata2022::neutron_mass]>;
inline constexpr auto atomic_mass_constant         = detail::one_of<isq::mass[codata2022::atomic_mass_constant]>;
inline constexpr auto bohr_radius                  = detail::one_of<isq::radius[codata2022::bohr_radius]>;
inline constexpr auto vacuum_electric_permittivity = detail::one_of<codata2022::vacuum_electric_permittivity>;
} // namespace constants
} // namespace beman::inside::units


#endif // !BEMAN_INSIDE_NO_STRING && !BEMAN_INSIDE_NO_UNITS

// ======================================================================
//  beman/inside/formats.hpp
// ======================================================================



//---------------------------------------------------------------------------
// formats — predefined `inside` aliases mapping to hardware byte widths, so you
// can write `beman::inside::uint8` / `beman::inside::unorm16` / `beman::inside::q8_8` directly.
//
// `uint8` … `uint64` and `int8` … `int64` are named after the std::uintN_t /
// std::intN_t they are stored in, and each uses that type's full range:
// `uint8` is [0, 255] in a std::uint8_t (save int64, which stops one short of
// -2^63). Q-format types keep power-of-two notches.
//
// The integer widths are `checked`; unorm and the Q formats round to nearest.
// For wraparound or saturation declare your own (`inside<{0, 255}, wrap>`),
// or use the counters clamp_counter<Max> and wrap_counter<Max> below.
//---------------------------------------------------------------------------
namespace beman::inside {
//-------------------------------------------------------------------------
// Native integer widths — value storage (Raw == value), `checked`.
// Full native range.
//-------------------------------------------------------------------------
using uint8  = inside<{0, 255}>;
using uint16 = inside<{0, 65535}>;
using uint32 = inside<{0, 4294967295}>;
// uint64 reaches past int64, so it has no implicit `operator imax`; read it
// with `to<std::uint64_t>()`. A difference of two uint64s spans 2^65 values and
// gets a wide-integer index.
using uint64 = inside<{0, 18446744073709551615u}>;

using int8  = inside<{-128, 127}>;
using int16 = inside<{-32768, 32767}>;
using int32 = inside<{-2147483648, 2147483647}>;
// int64 stays symmetric: the internal value path is `imax`, and -2^63
// has no negation in int64.
using int64 = inside<{-9223372036854775807, 9223372036854775807}>;

//-------------------------------------------------------------------------
// Unsigned normalized (UNORM) — [0, 1] at N-bit resolution, `round_nearest`.
// The notch denominator is the type max, so the index 0..max fills the native
// width; both endpoints (0 and 1) are exactly representable.
//-------------------------------------------------------------------------
using unorm8  = inside<{{0, 1}, per<255>}, round_nearest>;        // uint8
using unorm16 = inside<{{0, 1}, per<65535>}, round_nearest>;      // uint16
using unorm32 = inside<{{0, 1}, per<4294967295>}, round_nearest>; // uint32

//-------------------------------------------------------------------------
// Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
// full natural range. `round_nearest`.
//-------------------------------------------------------------------------
using q4_4   = inside<{{0, rational{255, 16}}, per<16>}, round_nearest>;                // uint8: [0, 15.9375]
using q8_8   = inside<{{0, rational{65535, 256}}, per<256>}, round_nearest>;            // uint16
using q16_16 = inside<{{0, rational{4294967295u, 65536u}}, per<65536>}, round_nearest>; // uint32

//-------------------------------------------------------------------------
// Counters — a counter is an inside over [0, Max] whose overflow policy says
// what `++` does at the ceiling (the boundary behavior is in the type).
//-------------------------------------------------------------------------
// Saturating counter: `++` caps at Max (never throws or wraps) — the honest
// "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
template <umax Max>
using clamp_counter = inside<{0, Max}, clamp>;
// Wrapping counter: `++` wraps Max → 0 (sequence numbers, epochs).
template <umax Max>
using wrap_counter = inside<{0, Max}, wrap>;

} // namespace beman::inside


#ifndef BEMAN_INSIDE_NO_STRING

#endif // BEMAN_INSIDE_NO_STRING

// ======================================================================
//  beman/inside/numeric_limits.hpp
// ======================================================================




//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specializations for inside<G, P>.
// numeric_limits reports the *grid* bounds (Lower/Upper), not the raw type's
// limits. std::hash hashes the Raw member (rational raw: Numerator+Denominator,
// boost-style combine). (std::common_type lives in arithmetic.hpp, always on.)
//---------------------------------------------------------------------------

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::numeric_limits<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    static constexpr bool is_specialized = true;
    static constexpr bool is_signed      = (G.Interval.Lower < beman::inside::detail::rational{0});
    // Every value is an integer: a non-zero integer notch over an integer Lower.
    static constexpr bool is_integer        = beman::inside::detail::integer_lattice<B>;
    static constexpr bool is_exact          = true; // rational + integer raw are both exact
    static constexpr bool is_bounded        = true;
    static constexpr bool is_modulo         = (P & beman::inside::wrap) != 0;
    static constexpr bool has_infinity      = false;
    static constexpr bool has_quiet_NaN     = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool traps             = beman::inside::is_checked(P);
    static constexpr bool is_iec559         = false;
    static constexpr int  radix             = 2;
    // The mode stores round by (rounding_of, the one precedence every path uses).
    static constexpr std::float_round_style round_style = [] {
        using enum beman::inside::detail::round_mode;
        switch (beman::inside::detail::rounding_of(P)) {
        case floor:
            return std::round_toward_neg_infinity;
        case ceil:
            return std::round_toward_infinity;
        case nearest:
        case half_even:
            return std::round_to_nearest;
        default:
            return std::round_toward_zero;
        }
    }();

    // digits / digits10 forward to the raw type so generic algorithms see the
    // storage size, not the rational interval count.
    using deduced_raw             = beman::inside::detail::raw_t<B>;
    static constexpr int digits   = std::numeric_limits<deduced_raw>::digits;
    static constexpr int digits10 = std::numeric_limits<deduced_raw>::digits10;

  private:
    // The value v of B (on its grid). Past 64 bits through the exact store:
    // no 64-bit rational holds a big grid's limits.
    static constexpr B at(const beman::inside::detail::grid_rational& v) noexcept {
        namespace d = ::beman::inside::detail;
        if constexpr (d::wide_valued<B>) {
            B                   b{};
            beman::inside::errc ec{};
            d::assign_exact<d::rational>(b,
                                         d::exact_of_grid<d::exact_limbs<B>>(v),
                                         beman::inside::make_policy<P>(ec),
                                         beman::inside::no_action{});
            return b;
        } else
            return B{d::to_rational(v)};
    }

  public:
    static constexpr B min() noexcept { return at(G.Interval.Lower); }
    static constexpr B max() noexcept { return at(G.Interval.Upper); }
    static constexpr B lowest() noexcept { return at(G.Interval.Lower); }
    // Exact types have no rounding noise — epsilon and round_error are 0 when
    // 0 is on the grid. A lattice that misses 0 but spans it gives its first
    // point above 0; when 0 is outside the interval, the grid minimum — the
    // closest representable stand-in for "no error" the type can express.
    static constexpr B epsilon() noexcept {
        namespace d = ::beman::inside::detail;
        if constexpr (G.representable(d::grid_rational{0}))
            return at(d::grid_rational{0});
        else if constexpr (G.Interval.Lower < d::grid_rational{0} && d::grid_rational{0} < G.Interval.Upper) {
            // Lower + ⌈−Lower/Notch⌉·Notch
            constexpr d::grid_rational k = d::grid_to_int<d::round_mode::ceil>(-G.Interval.Lower / G.Notch);
            return at(G.Interval.Lower + k * G.Notch);
        } else
            return at(G.Interval.Lower);
    }
    static constexpr B round_error() noexcept { return epsilon(); }
};

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::hash<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    // Boost-style combine of h with v, and with the limbs of a wide integer.
    template <class V>
    static constexpr std::size_t mix(std::size_t h, V v) noexcept {
        return h ^ (std::hash<V>{}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    }
    static constexpr std::size_t combine(std::size_t h, const auto& w) noexcept {
        for (beman::inside::umax limb : w.Word)
            h = mix(h, limb);
        return h;
    }

    constexpr std::size_t operator()(const B& b) const noexcept {
        if constexpr (beman::inside::detail::point_storage<B>)
            return 0; // one value: the type
        else if constexpr (beman::inside::detail::fraction_storage<B>)
            return combine(combine(0, b.raw().Num), b.raw().Den); // a reduced fraction
        else if constexpr (beman::inside::detail::rational_storage<B>)
            return mix(std::hash<beman::inside::umax>{}(b.raw().Numerator), b.raw().Denominator);
        else if constexpr (beman::inside::detail::wide_index_storage<B>)
            return combine(0, b.raw());
        else
            return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
    }
};


#if __STDC_HOSTED__ && !defined(BEMAN_INSIDE_MATH_NO_FP)

// ======================================================================
//  beman/inside/random.hpp
// ======================================================================
// Opt-in: uniform sampling over a grid. `uniform<B>(rng)` returns a B drawn
// uniformly from the grid's slots (Lower, Lower + Notch, …, Upper) — exact, any
// storage. The draw depends only on the generator's outputs, so a seed gives
// the same B on every standard library (the std::*_distribution algorithms are
// each library's own). Kept out of the umbrella because <random> is heavy,
// hosted-only and pulls <cmath> (so the single header drops it under
// BEMAN_INSIDE_MATH_NO_FP).
//---------------------------------------------------------------------------



#include <bit>
#include <limits>
#include <random>

namespace beman::inside {
namespace detail {
// 64 uniform bits from g by a fixed recipe: one output when G spans 64 bits,
// else b bits from each output, b the widest power of two G covers (outputs
// past 2^b − 1 above G::min() are drawn again), high bits first.
template <std::uniform_random_bit_generator G>
constexpr umax random_word(G& g) {
    static_assert(std::numeric_limits<std::invoke_result_t<G&>>::digits <= 64,
                  "uniform<B>: a generator wider than 64 bits");
    constexpr umax span = static_cast<umax>(G::max()) - static_cast<umax>(G::min());
    if constexpr (span == std::numeric_limits<umax>::max())
        return static_cast<umax>(g());
    else {
        constexpr int  bits = std::bit_width(span + 1) - 1;
        constexpr umax mask = (umax{1} << bits) - 1;
        umax           word = 0;
        for (int got = 0; got < 64; got += bits) {
            umax d;
            do
                d = static_cast<umax>(g()) - static_cast<umax>(G::min());
            while (d > mask);
            word = word << bits | d;
        }
        return word;
    }
}

// An index in [0, N]: a word masked to N's bit width, drawn again past N
// (accepts more than half of the draws). N = 0 draws nothing.
template <umax N, std::uniform_random_bit_generator G>
constexpr umax random_index(G& g) {
    if constexpr (N == 0)
        return 0;
    else {
        constexpr umax mask = std::numeric_limits<umax>::max() >> std::countl_zero(N);
        for (;;)
            if (const umax k = random_word(g) & mask; k <= N)
                return k;
    }
}
} // namespace detail

template <insidable B, std::uniform_random_bit_generator G>
[[nodiscard]] B uniform(G& g) {
    static_assert(detail::notched<B> || detail::point_grid<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    if constexpr (detail::wide_index_storage<B>) {
        // More than 2^64 slots: the same recipe limb by limb, low limb first,
        // the top limb masked to the slot count's bit width.
        using W                = detail::raw_t<B>;
        constexpr W   count    = static_cast<W>(grid_of<B>.slot_count());
        constexpr int top_bits = grid_of<B>.slot_bits() - 64 * (static_cast<int>(sizeof(W) / 8) - 1);
        for (;;) {
            W k;
            for (auto& w : k.Word)
                w = detail::random_word(g);
            if constexpr (top_bits < 64)
                k.Word[sizeof(W) / 8 - 1] &= (umax{1} << top_bits) - 1;
            if (!(k > count))
                return B::from_raw(k);
        }
    } else
        return B::from_raw(detail::raw_from_offset<B>(detail::random_index<detail::max_index_of<B>>(g)));
}
} // namespace beman::inside


#endif // __STDC_HOSTED__ && !BEMAN_INSIDE_MATH_NO_FP

#if !defined(BEMAN_INSIDE_NO_STRING) && !defined(BEMAN_INSIDE_NO_UNITS)

#endif // !BEMAN_INSIDE_NO_STRING && !BEMAN_INSIDE_NO_UNITS

#if !defined(BEMAN_INSIDE_NO_STRING) && !defined(BEMAN_INSIDE_NO_UNITS)

// ======================================================================
//  beman/inside/units_math.hpp
// ======================================================================
// Opt-in: math on units::quantity, over the correctly rounded engine
// (cmath.hpp). Kept apart from units.hpp, which stays free of the engine's
// compile time.
//
//   value_cast<si::radian, Out>(q)   degree → radian (a pi factor): the exact
//                                    x·π/180, rounded once onto Out's grid
//   sqrt(q), cbrt(q)                 the unit's root (m² → m), the number's
//   abs, floor, ceil, round, trunc   on the number, in q's unit
//   sin, cos, tan(q)                 of an angle in radians: a plain inside
//   sin_into, cos_into<Out>(q)       also of an angle in a unit with a factor
//                                    of π (°, a turn): reduced exactly by
//                                    quarter turns, exact at Niven's values
//   asin, acos, atan(x), atan2(y, x) an angle in radians (x an inside or a
//                                    dimensionless quantity)
//   hypot(x, y)                      in the common unit
//   exp_into … atanh_into, log10_into, pow_base_into<Out, B>, pow_into
//                                    of an inside or a dimensionless quantity
//   fn_into<Out>(…)                  each of them with the result's number
//                                    named: Out rounds, the input need not
//
// Every result rounds as the inside function does: onto the output grid,
// under its rounding mode (the deduced forms take the input's grid).
//---------------------------------------------------------------------------



#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace beman::inside::math::detail::ax {
// x·(n/d)·π^P for P = ±1: π from pi_q at T = S + B bits, B covering |x·n/d|,
// so its error adds at most one unit; the division truncates one more. The
// factor is 64-bit (KF = 1), or KF limbs for a factor past 64 bits (ħ = h/2π
// in J·s).
template <std::size_t E, int Bits, int P, std::size_t KF = 1>
struct pi_scale_core {
    using factor = std::conditional_t<KF == 1, umax, beman::inside::detail::wide_uint<KF>>;
    wide_rational<E> X;
    factor           N; // the rational factor n/d
    factor           D;
    bool             Negative;

    constexpr maybe_exact<E> exact() const { return is_zero(X) ? maybe_exact<E>{X} : std::nullopt; }

    template <int W>
    constexpr auto run() const {
        constexpr int         S = W + 2;
        constexpr int         T = S + Bits + 4;
        constexpr std::size_t KJ =
            limbs_for_bits(64 * static_cast<int>(E) + 64 * static_cast<int>(KF) + T + 2 * S + 16);
        using J = wide_sint<KJ>;
        using I = fixed_t<Bits + S + 4>;
        const J pi_fixed{static_cast<J>(pi_q<T>)};
        const J num = J{X.Num} * J{N};
        const J den = J{X.Den} * J{D};
        J       y;
        if constexpr (P > 0)
            y = (num * pi_fixed) / (den << (T - S));
        else
            y = (num << (S + T)) / (den * pi_fixed);
        const I v = static_cast<I>(y);
        return approx<limbs_of<I>>{Negative ? -v : v, S, 2};
    }
};

// sin or cos of an angle x·(n/d)·π (a unit with a factor of π: °, a turn):
// t = x·n/d half turns is reduced exactly by k = round(2t) quarter turns, so
// only r = π·(t − k/2), |r| ≤ π/4, is approximated. Exact where Niven's
// theorem allows a rational value: 0, ±1/2, ±1.
template <std::size_t E, bool Cos, std::size_t KF = 1>
struct pi_trig_core {
    using factor = std::conditional_t<KF == 1, umax, beman::inside::detail::wide_uint<KF>>;
    wide_rational<E> X;
    factor           N; // the rational factor n/d
    factor           D;
    bool             Negative;

    template <class J>
    constexpr J numerator() const {
        const J num = J{X.Num} * J{N};
        return Negative ? -num : num;
    }

    constexpr maybe_exact<E> exact() const {
        using J     = wide_sint<limbs_for_bits(64 * static_cast<int>(E + KF) + 8)>;
        const J six = numerator<J>() * J{6}, den = J{X.Den} * J{D};
        if (!(six % den).is_zero())
            return std::nullopt;
        J m = (six / den) % J{12}; // 6t mod 12: the angle in multiples of 30°
        if (m.negative())
            m += J{12};
        // 2·sin and 2·cos at m·30°; 3 marks an irrational value (√3/2).
        constexpr signed char twice_sin[12] = {0, 1, 3, 2, 3, 1, 0, -1, 3, -2, 3, -1};
        constexpr signed char twice_cos[12] = {2, 3, 1, 0, -1, 3, -2, 3, -1, 0, 1, 3};
        const int             v             = (Cos ? twice_cos : twice_sin)[m.Word[0]];
        if (v == 3)
            return std::nullopt;
        return wide_rational<E>{wide_sint<E>{v}, wide_sint<E>{2}};
    }

    template <int W>
    constexpr auto run() const {
        constexpr int S               = W + 10;
        constexpr int P               = S + 4;
        using J                       = wide_sint<limbs_for_bits(64 * static_cast<int>(E + KF) + P + 16)>;
        using I                       = fixed_t<S + 2>;
        constexpr std::size_t K       = limbs_of<I>;
        const J               two_num = J{2} * numerator<J>(), den = J{X.Den} * J{D};
        const J               k = nearest_div(two_num, den); // quarter turns
        // r = π·(2t − k)/2 at scale S, within 1 unit.
        const I r = static_cast<I>(nearest_div((two_num - k * den) * static_cast<J>(pi_q<P>), den << (P - S + 1)));
        const unsigned q   = static_cast<unsigned>(k.Word[0] & 3u);
        auto           neg = [](fx<K> f) { return fx<K>{-f.Value, f.Error}; };
        const fx<K>    v   = Cos ? ((q == 0)   ? cos_series<S>(r, 2)
                                    : (q == 1) ? neg(sin_series<S>(r, 2))
                                    : (q == 2) ? neg(cos_series<S>(r, 2))
                                               : sin_series<S>(r, 2))
                                 : ((q == 0)   ? sin_series<S>(r, 2)
                                    : (q == 1) ? cos_series<S>(r, 2)
                                    : (q == 2) ? neg(sin_series<S>(r, 2))
                                               : neg(cos_series<S>(r, 2)));
        return approx<K>{v.Value, S, v.Error};
    }
};
} // namespace beman::inside::math::detail::ax

namespace beman::inside::units {
namespace detail {
// The n-th root of a unit: every factor's exponent must divide.
consteval unit unit_root(const unit& u, int n) {
    unit r{};
    for (std::int8_t i = 0; i < u.Size; ++i) {
        if (u.Atoms[i].Exp % n != 0)
            beman::inside::detail::constexpr_error<
                "units: the root of a unit needs exponents it divides (m², not m)">();
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, static_cast<std::int8_t>(u.Atoms[i].Exp / n)});
    }
    if (!u.Scale.is_one())
        beman::inside::detail::constexpr_error<
            "units: the root of a scaled unit - convert it to a named unit first">();
    return r;
}

// The root of a reference's spec: its recipe's, when the exponents divide.
consteval reference spec_root(const reference& a, int n, const unit& u) {
    if (is_generic(a))
        return reference{nullptr, false, {}, u};
    const recipe f = a.Spec ? recipe_of(a.KindOf ? kind_root(a.Spec.P) : a.Spec.P) : a.Derived;
    recipe       r{};
    for (std::int8_t i = 0; i < f.Size; ++i) {
        if (f.F[i].Exp % n != 0)
            return reference{nullptr, false, {}, u};
        recipe_add(r, f.F[i].Spec, f.F[i].Exp / n);
    }
    if (r.Size == 1 && r.F[0].Exp == 1)
        return reference{r.F[0].Spec, a.KindOf, {}, u};
    return reference{nullptr, a.KindOf, r, u};
}

template <reference R, int N>
consteval reference root_reference() {
    const unit u = unit_root(R.U, N);
    return spec_root(R, N, u);
}

// An angle in radians: a dimensionless quantity of magnitude one.
template <reference R>
inline constexpr bool radians_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).is_one();

inline constexpr reference radian_reference = reference{si::radian};

// An angle in a unit with a factor of π (°, a turn): sin and cos reduce it exactly.
template <reference R>
inline constexpr bool pi_angle_v = dimension_of(R.U) == dim_one && magnitude_of(R.U).Pi == 1 &&
                                   beman::inside::detail::fits_rational(magnitude_of(R.U).Ratio);

// sin_into / cos_into of a π-scaled angle: one rounding onto Out.
template <insidable Out, bool Cos, reference R, class V>
constexpr Out pi_trig_into(const quantity<R, V>& q) {
    namespace ax = beman::inside::math::detail::ax;
    beman::inside::math::adaptive::require_rounding<Out>();
    constexpr rational r = beman::inside::detail::to_rational(magnitude_of(R.U).Ratio);
    using core           = ax::pi_trig_core<ax::input_limbs<V>, Cos>;
    return ax::evaluate<Out, ax::start_bits<Out>>(core{ax::exact_input(q.numerical_value()),
                                                       r.Numerator,
                                                       beman::inside::detail::abs_den(r.Denominator),
                                                       r.Denominator < 0});
}
} // namespace detail

// value_cast<U, Out>(q) across a pi factor (degree ↔ radian): the exact
// x·r·π^±1, rounded once onto Out's grid under its rounding mode.
template <auto U, insidable Out, reference R, class V>
    requires is_unit_tag_v<decltype(U)> &&
             (!detail::exact_factor_v<detail::conversion_factor<R.U, decltype(U)::value>>)
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    namespace ax            = beman::inside::math::detail::ax;
    constexpr units::unit u = decltype(U)::value;
    constexpr magnitude   f = detail::conversion_factor<R.U, u>;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    static_assert(f.Pi == 1 || f.Pi == -1, "units: value_cast - only a factor of pi or 1/pi rounds");
    beman::inside::math::adaptive::require_rounding<Out>();
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    // The factor n/d in one limb when it is a 64-bit rational, else in as
    // many as it needs (ħ = h/2π in J·s).
    using beman::inside::detail::grid_int;
    constexpr grid_int    n  = beman::inside::detail::wide_numerator(f.Ratio);
    constexpr grid_int    d  = beman::inside::detail::wide_denominator(f.Ratio);
    constexpr bool        ng = n.negative();
    constexpr int         NB = n.bit_width();
    constexpr std::size_t KF = beman::inside::detail::fits_rational(f.Ratio)
                                   ? 1
                                   : beman::inside::detail::limbs_for_bits(std::max(NB, d.bit_width()) + 1);
    using core               = ax::pi_scale_core<ax::input_limbs<V>, ax::in_mag<V> + NB + 4, f.Pi, KF>;
    using factor             = typename core::factor;
    return quantity<to, Out>{ax::evaluate<Out, ax::start_bits<Out>>(
        core{ax::exact_input(q.numerical_value()), static_cast<factor>(ng ? -n : n), static_cast<factor>(d), ng})};
}

// The number's functions, in q's unit.
#define BEMAN_INSIDE_UNITS_SAME_UNIT_FN(fn)                                            \
    template <reference R, class V>                                                    \
    [[nodiscard]] constexpr auto fn(const quantity<R, V>& q) {                         \
        return detail::make_quantity<R>(beman::inside::math::fn(q.numerical_value())); \
    }
BEMAN_INSIDE_UNITS_SAME_UNIT_FN(abs)
BEMAN_INSIDE_UNITS_SAME_UNIT_FN(floor)
BEMAN_INSIDE_UNITS_SAME_UNIT_FN(ceil)
BEMAN_INSIDE_UNITS_SAME_UNIT_FN(round)
BEMAN_INSIDE_UNITS_SAME_UNIT_FN(trunc)
#undef BEMAN_INSIDE_UNITS_SAME_UNIT_FN

// Trigonometry of an angle in radians: a plain number.
template <reference R, class V>
[[nodiscard]] constexpr auto sin(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>,
                  "units: sin takes an angle in radians - sin_into<Out>(q) takes a unit with a factor of pi");
    return beman::inside::math::sin(q.numerical_value());
}
template <reference R, class V>
[[nodiscard]] constexpr auto cos(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>,
                  "units: cos takes an angle in radians - cos_into<Out>(q) takes a unit with a factor of pi");
    return beman::inside::math::cos(q.numerical_value());
}
template <reference R, class V>
[[nodiscard]] constexpr auto tan(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: tan takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::tan(q.numerical_value());
}

// atan2(y, x) of two quantities of one dimension, in their common unit.
template <reference A, class VA, reference B, class VB>
    requires(detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::atan2(x, y);
             })
[[nodiscard]] constexpr auto atan2(const quantity<A, VA>& y, const quantity<B, VB>& x) {
    return detail::make_quantity<detail::radian_reference>(
        beman::inside::math::atan2(detail::in_common_unit<A, B>(y), detail::in_common_unit<A, B>(x)));
}

// hypot(x, y): √(x² + y²) in the common unit.
template <reference A, class VA, reference B, class VB>
    requires(detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::hypot(x, y);
             })
[[nodiscard]] constexpr auto hypot(const quantity<A, VA>& x, const quantity<B, VB>& y) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::math::hypot(detail::in_common_unit<A, B>(x), detail::in_common_unit<A, B>(y)));
}

//---------------------------------------------------------------------------
// The _into forms: Out names the result's number (an inside with a rounding
// mode); the input needs no rounding mode — an arithmetic result (checked)
// goes straight in. As math::fn_into, a result may be an expected (sqrt of a
// grid that reaches below 0, tan).
//---------------------------------------------------------------------------
namespace detail {
// x's number as an inside: an inside itself, or a dimensionless quantity's.
template <class X>
constexpr auto number_of(const X& x) {
    if constexpr (insidable<X>)
        return x;
    else {
        static_assert(quantity_like<X> && number_like_v<X::reference>,
                      "units: the argument must be a number - an inside or a dimensionless quantity");
        return rescale<conversion_factor<X::reference.U, unit{}>>(x.numerical_value());
    }
}
} // namespace detail

// Roots: the unit's (m² → m, m³ → m) and the number's.
#define BEMAN_INSIDE_UNITS_ROOT_FN(fn, n)                                                                           \
    template <reference R, class V>                                                                                 \
    [[nodiscard]] constexpr auto fn(const quantity<R, V>& q) {                                                      \
        return detail::make_quantity<detail::root_reference<R, n>()>(beman::inside::math::fn(q.numerical_value())); \
    }                                                                                                               \
    template <insidable Out, reference R, class V>                                                                  \
    [[nodiscard]] constexpr auto fn##_into(const quantity<R, V>& q) {                                               \
        return detail::make_quantity<detail::root_reference<R, n>()>(                                               \
            beman::inside::math::fn##_into<Out>(q.numerical_value()));                                              \
    }
BEMAN_INSIDE_UNITS_ROOT_FN(sqrt, 2)
BEMAN_INSIDE_UNITS_ROOT_FN(cbrt, 3)
#undef BEMAN_INSIDE_UNITS_ROOT_FN

// An angle in radians, or in a unit with a factor of π (°, a turn, 2π/65536
// rad): that one is reduced exactly, and the result rounds once.
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto sin_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R> || detail::pi_angle_v<R>,
                  "units: sin_into takes an angle in radians or in a unit with a factor of pi");
    if constexpr (detail::radians_v<R>)
        return beman::inside::math::sin_into<Out>(q.numerical_value());
    else
        return detail::pi_trig_into<Out, false>(q);
}
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto cos_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R> || detail::pi_angle_v<R>,
                  "units: cos_into takes an angle in radians or in a unit with a factor of pi");
    if constexpr (detail::radians_v<R>)
        return beman::inside::math::cos_into<Out>(q.numerical_value());
    else
        return detail::pi_trig_into<Out, true>(q);
}
template <insidable Out, reference R, class V>
[[nodiscard]] constexpr auto tan_into(const quantity<R, V>& q) {
    static_assert(detail::radians_v<R>, "units: tan takes an angle in radians - value_cast<si::radian, Out>(q) first");
    return beman::inside::math::tan_into<Out>(q.numerical_value());
}

// Inverse trigonometry: an angle in radians.
#define BEMAN_INSIDE_UNITS_ARC_FN(fn)                                                       \
    template <insidable V>                                                                  \
    [[nodiscard]] constexpr auto fn(const V& x) {                                           \
        return detail::make_quantity<detail::radian_reference>(beman::inside::math::fn(x)); \
    }                                                                                       \
    template <insidable Out, class X>                                                       \
    [[nodiscard]] constexpr auto fn##_into(const X& x) {                                    \
        return detail::make_quantity<detail::radian_reference>(                             \
            beman::inside::math::fn##_into<Out>(detail::number_of(x)));                     \
    }
BEMAN_INSIDE_UNITS_ARC_FN(asin)
BEMAN_INSIDE_UNITS_ARC_FN(acos)
BEMAN_INSIDE_UNITS_ARC_FN(atan)
#undef BEMAN_INSIDE_UNITS_ARC_FN

// Exponentials, logarithms and the hyperbolic functions of a number: an
// inside or a dimensionless quantity (10^(L / 20 dB), log10(P / 1 mW)).
#define BEMAN_INSIDE_UNITS_NUMBER_FN(fn)                                  \
    template <insidable Out, class X>                                     \
    [[nodiscard]] constexpr auto fn##_into(const X& x) {                  \
        return beman::inside::math::fn##_into<Out>(detail::number_of(x)); \
    }
BEMAN_INSIDE_UNITS_NUMBER_FN(exp)
BEMAN_INSIDE_UNITS_NUMBER_FN(exp2)
BEMAN_INSIDE_UNITS_NUMBER_FN(log)
BEMAN_INSIDE_UNITS_NUMBER_FN(log2)
BEMAN_INSIDE_UNITS_NUMBER_FN(log10)
BEMAN_INSIDE_UNITS_NUMBER_FN(sinh)
BEMAN_INSIDE_UNITS_NUMBER_FN(cosh)
BEMAN_INSIDE_UNITS_NUMBER_FN(tanh)
BEMAN_INSIDE_UNITS_NUMBER_FN(asinh)
BEMAN_INSIDE_UNITS_NUMBER_FN(acosh)
BEMAN_INSIDE_UNITS_NUMBER_FN(atanh)
#undef BEMAN_INSIDE_UNITS_NUMBER_FN
template <insidable Out, imax Base, class X>
[[nodiscard]] constexpr auto pow_base_into(const X& x) {
    return beman::inside::math::pow_base_into<Out, Base>(detail::number_of(x));
}
template <insidable Out, class B, class E>
[[nodiscard]] constexpr auto pow_into(const B& base, const E& exp) {
    return beman::inside::math::pow_into<Out>(detail::number_of(base), detail::number_of(exp));
}
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::atan2_into<Out>(x, y);
             })
[[nodiscard]] constexpr auto atan2_into(const quantity<A, VA>& y, const quantity<B, VB>& x) {
    return detail::make_quantity<detail::radian_reference>(
        beman::inside::math::atan2_into<Out>(detail::in_common_unit<A, B>(y), detail::in_common_unit<A, B>(x)));
}
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(detail::common_rescalable<A, VA, B, VB> &&
             requires(const detail::common_number_t<A, VA, B>& x, const detail::common_number_t<B, VB, A>& y) {
                 beman::inside::math::hypot_into<Out>(x, y);
             })
[[nodiscard]] constexpr auto hypot_into(const quantity<A, VA>& x, const quantity<B, VB>& y) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(
        beman::inside::math::hypot_into<Out>(detail::in_common_unit<A, B>(x), detail::in_common_unit<A, B>(y)));
}
// atan2 and hypot of two quantities that do not add (another dimension or
// kind, no common unit).
template <reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void atan2(const quantity<A, VA>&, const quantity<B, VB>&) =
    delete ("units: atan2 takes two quantities that add - one dimension and kind, a common unit");
template <reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void hypot(const quantity<A, VA>&, const quantity<B, VB>&) =
    delete ("units: hypot takes two quantities that add - one dimension and kind, a common unit");
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void atan2_into(const quantity<A, VA>&, const quantity<B, VB>&) =
    delete ("units: atan2 takes two quantities that add - one dimension and kind, a common unit");
template <insidable Out, reference A, class VA, reference B, class VB>
    requires(detail::sum_diagnosed<A, B>())
void hypot_into(const quantity<A, VA>&, const quantity<B, VB>&) =
    delete ("units: hypot takes two quantities that add - one dimension and kind, a common unit");
} // namespace beman::inside::units


#endif // !BEMAN_INSIDE_NO_STRING && !BEMAN_INSIDE_NO_UNITS

#endif // BEMAN_INSIDE_SINGLE_HEADER_HPP
