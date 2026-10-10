// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// beman.inside 0.1.0 — single-header amalgamation
//
//   *** GENERATED FILE — DO NOT EDIT BY HAND ***
//
// Regenerate with:  cmake --build <build-dir> --target amalgamate
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
#include <numeric>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// ======================================================================
//  beman/inside/inside.hpp
// ======================================================================
// Public umbrella header. Include this to get the full `beman::inside::inside` API:
// the core type (core.hpp) plus the free-function layers that depend on the
// complete type — casts, arithmetic operators, and inside_range. Those three
// must follow core.hpp because they need `inside<G, P>` fully defined.
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/core.hpp
// ======================================================================
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
// Defines the core `beman::inside::inside<G, P>` type; the umbrella adds the free-function
// casts/arithmetic/range layers that depend on this complete type.
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/generic.hpp
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    #define BEMAN_INSIDE_HAS_EXCEPTIONS 1
    #include <stdexcept>
#else
    #define BEMAN_INSIDE_HAS_EXCEPTIONS 0
#endif

//---------------------------------------------------------------------------
// Attribute shims. Error/throw paths are marked cold + non-inline so the
// optimiser keeps them out of the hot path (and out of the inlined body of
// otherwise-trivial assignment/arithmetic). Standard [[noreturn]] / [[unlikely]]
// are used directly at the throw sites and branches.
//---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
    #define BEMAN_INSIDE_COLD [[gnu::cold]]
    #define BEMAN_INSIDE_NOINLINE [[gnu::noinline]]
    #define BEMAN_INSIDE_TRAP() __builtin_trap()
#elif defined(_MSC_VER)
    #define BEMAN_INSIDE_COLD
    #define BEMAN_INSIDE_NOINLINE __declspec(noinline)
    #define BEMAN_INSIDE_TRAP() __debugbreak()
#else
    #define BEMAN_INSIDE_COLD
    #define BEMAN_INSIDE_NOINLINE
    #include <cstdlib>
    #define BEMAN_INSIDE_TRAP() ::abort()
#endif

//---------------------------------------------------------------------------
// debug — error codes, the replaceable failure handler, and diagnostic helpers.
// `errc` enumerates the failure modes. Every runtime failure funnels through
// `detail::raise`, which calls the installed `error_handler` — by default one
// that throws `beman::inside::inside_error` (hosted) or traps (freestanding / no
// exceptions). `set_error_handler` lets a bare-metal target redirect failures
// to a reset/log without depending on <system_error> or the C++ exception ABI.
// `print_types` is a static_assert debug helper for template instantiation.
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
    invalid_format,   // text that is not a number (from_chars, operator>>)
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
// originating `errc` so `catch (inside_error& e) { e.Code; }` replaces the old
// `e.code() == make_error_code(...)` idiom.
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
[[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE inline void default_error_handler(errc code, const char* what) {
#if BEMAN_INSIDE_HAS_EXCEPTIONS
    throw inside_error(code, what);
#else
    (void)code;
    (void)what;
    BEMAN_INSIDE_TRAP();
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
// paths hard-fail the build (the old `throw` did the same).
//-----------------------------------------------------------------------
[[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE inline void raise(errc code, const char* what = nullptr) {
    g_error_handler(code, what ? what : errc_message(code));
    BEMAN_INSIDE_TRAP(); // handler must not return; trap if it did
}

//-----------------------------------------------------------------------
// Compile-time-only diagnostic. A fixed_string NTTP carries the message
// into the type, so a malformed literal / overflow aborts constant
// evaluation with the text in the instantiation — no `throw` token, so it
// also compiles under -fno-exceptions. Never reached at runtime (all call
// sites are guarded by std::is_constant_evaluated / are consteval).
//-----------------------------------------------------------------------
template <unsigned N>
struct fixed_string {
    char Data[N]{};
    constexpr fixed_string(const char (&s)[N]) {
        for (unsigned i = 0; i < N; ++i)
            Data[i] = s[i];
    }
};

template <fixed_string Msg>
[[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE inline void constexpr_error() {
    raise(errc::overflow, Msg.Data);
}

// Debug helper: instantiating print_types<Ts...> fails and names Ts.
template <typename... Ts>
struct print_types {
    static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
};
} // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/lift.hpp
// ======================================================================



//---------------------------------------------------------------------------
// lift — monadic composition for `std::expected<T, errc>`. `lift(op, args...)`
// unwraps each expected arg, calls `op`, and re-wraps; the first arg holding an
// error short-circuits with that error. An `op` already returning expected<R>
// is forwarded as-is, so the inner operation reports its own cause. Used by
// interval/grid/inside arithmetic and rational's operators.
//
// std::expected is larger than its value (flag + errc), so it only ever
// travels as a parameter or return value — never as stored state.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
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
    if constexpr (is_expected_v<T>)
        return *std::forward<T>(v);
    else
        return std::forward<T>(v);
}

// Copy an arg's error into `e`; true if the arg holds one.
template <class T>
constexpr bool lift_take_error([[maybe_unused]] const T& v, [[maybe_unused]] errc& e) {
    if constexpr (is_expected_v<T>)
        if (!v.has_value()) {
            e = v.error();
            return true;
        }
    return false;
}
} // namespace detail

//---------------------------------------------------------------------------
// detail::lift(op, args...) — call op on the unwrapped args → expected<result,
// errc>; the first erroneous arg (left to right) short-circuits with its error.
// An op already returning expected<R, errc> passes through. Internal: it backs
// the expected-lift operators; user code chains through those.
//---------------------------------------------------------------------------
namespace detail {
template <class Op, class... Args>
[[nodiscard]] constexpr auto lift(Op op, Args&&... args) {
    using R   = std::remove_cvref_t<decltype(op(detail::lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<detail::is_expected_v<R>, R, std::expected<R, errc>>;

    errc e{};
    if ((detail::lift_take_error(args, e) || ...))
        return Ret{std::unexpected{e}};

    if constexpr (detail::is_expected_v<R>)
        return op(detail::lift_unwrap(std::forward<Args>(args))...);
    else
        return Ret{op(detail::lift_unwrap(std::forward<Args>(args))...)};
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
//  beman/inside/math.hpp
// ======================================================================



//---------------------------------------------------------------------------
// math — primitive numeric utilities: umax/imax, smallest_uint_for /
// smallest_int_for (grid storage selection), the arithmetic/fractional concepts,
// safe_abs, constexpr frexp/ldexp, and abs_fraction (the double → rational
// engine behind rational(double)).
//---------------------------------------------------------------------------
namespace beman::inside {
using umax = std::uint64_t;
using imax = std::int64_t;

namespace detail {
struct rational;
}

namespace detail {
// A plain number a grid-less value can be: integral, floating point, or the
// library's exact rational. Internal; the public operand concept is `numeric`.
template <typename T>
concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;
} // namespace detail

namespace detail {

// Smallest unsigned type whose range holds every index 0..N.
template <std::uintmax_t N>
using smallest_uint_for_t = std::conditional_t<
    (N == 0),
    rational,
    std::conditional_t<(N <= UINT8_MAX),
                       std::uint8_t,
                       std::conditional_t<(N <= UINT16_MAX),
                                          std::uint16_t,
                                          std::conditional_t<(N <= UINT32_MAX), std::uint32_t, std::uint64_t>>>>;

// Smallest signed type whose range holds Low..High.
template <std::intmax_t Low, std::intmax_t High>
using smallest_int_for_t = std::conditional_t<
    (Low >= INT8_MIN && High <= INT8_MAX),
    std::int8_t,
    std::conditional_t<(Low >= INT16_MIN && High <= INT16_MAX),
                       std::int16_t,
                       std::conditional_t<(Low >= INT32_MIN && High <= INT32_MAX), std::int32_t, std::int64_t>>>;

// (type_name_v<T> — used only by the debug stringifier — lives in
// "beman/inside/io.hpp" so the core stays free of <string_view>.)

// Subset of arithmetic excluding integrals — the rhs types that need the
// rational-arithmetic assignment path.
template <typename T>
concept fractional = std::floating_point<T> || std::same_as<rational, T>;

template <std::signed_integral V>
[[nodiscard]] constexpr umax safe_abs(V value) noexcept {
    return (value >= 0) ? static_cast<umax>(value) : umax{0} - static_cast<umax>(value);
}

inline constexpr double frexp(double value, int* exp) noexcept {
    if (value == 0.0) {
        *exp = 0;
        return value;
    }

    auto                    bits          = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    auto                    e             = static_cast<int>((bits >> 52) & 0x7FF);

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

constexpr double ldexp(double value, int exp) noexcept {
    if (value == 0.0 || exp == 0)
        return value;

    auto                    bits          = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    auto                    e             = static_cast<int>((bits >> 52) & 0x7FF);

    if (e == 0x7FF)
        return value; // inf or NaN

    // Normalize subnormals
    int extra = 0;
    if (e == 0) {
        bits  = std::bit_cast<std::uint64_t>(value * 0x1p53);
        e     = static_cast<int>((bits >> 52) & 0x7FF);
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

// Freestanding finite check: `v - v` is 0 for every finite v, NaN otherwise
// (and inf - inf is NaN). Avoids <cmath>/std::isfinite so the core stays
// bare-metal clean; the same idiom is used in the store paths.
constexpr bool is_finite(double v) noexcept { return v - v == 0; }

// Exact conversion of a finite double to num/den (den a power of two), the
// engine behind rational(double). A finite double is exactly
// `significand · 2^exp2` (a 53-bit significand), both read straight from the
// IEEE-754 bits — no <cmath>, no FPU rounding, bit-identical across platforms.
constexpr std::pair<umax, umax> abs_fraction(double value) {
    if (not is_finite(value))
        raise(errc::not_finite, "beman::inside::detail::abs_fraction: non-finite double");

    if (value == 0.0)
        return {0, 1};
    if (value < 0)
        value = -value; // |value|; sign is the caller's job

    const auto bits        = std::bit_cast<std::uint64_t>(value);
    const int  e           = static_cast<int>((bits >> 52) & 0x7FF);
    umax       significand = bits & 0x000F'FFFF'FFFF'FFFF;
    int        exp2;
    if (e == 0) // subnormal: no implicit leading 1
        exp2 = 1 - 1023 - 52;
    else // normal: restore the implicit bit
    {
        significand |= (umax{1} << 52);
        exp2 = e - 1023 - 52;
    }

    // value == significand * 2^exp2. Re-express as num/den with den = 2^k.
    if (exp2 >= 0) // integer-valued: scale up, den = 1
    {
        // |value| ≥ 2^64 has no 64-bit numerator: fail rather than wrap the shift.
        if (exp2 > 64 - std::bit_width(significand)) {
            if consteval {
                // Clang prints only the first ~34 characters: lead with the cause.
                constexpr_error<"no 64-bit rational for a double of magnitude 2^64 or more (a grid limit needs C++26 "
                                "big grids)">();
            }
            raise(errc::overflow, "beman::inside::detail::abs_fraction: |double| >= 2^64");
        }
        return {significand << exp2, 1};
    }

    // exp2 < 0 → den = 2^(-exp2), capped at 2^62 (den is stored signed). Beyond
    // the cap the value is too small to keep: drop the significand's low bits
    // (shift ≥ 64 folds to zero). Reachable for every subnormal and normals
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalises to 0/1).
    int           den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow) {
        const int drop = den_pow - max_pow;
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
    // <algorithm> in this core header (libc++ does not provide std::min
    // transitively).
    if (significand != 0) {
        const int trailing = std::countr_zero(significand);
        const int shift    = trailing < den_pow ? trailing : den_pow;
        significand >>= shift;
        den >>= shift;
    }
    return {significand, den};
}

} // namespace detail
} // namespace beman::inside


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
// The limb kernels below are also used by the C++26 big_rational.
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
// __int128; else a schoolbook 32-bit split (32-bit targets).
template <std::unsigned_integral L>
constexpr pair<L> mul(L a, L b) noexcept {
    if constexpr (bits<L> <= 32) {
        const std::uint64_t p = std::uint64_t{a} * b;
        return {static_cast<L>(p >> bits<L>), static_cast<L>(p)};
    } else {
        static_assert(bits<L> == 64, "wide_int: limbs are at most 64 bits");
#if defined(__SIZEOF_INT128__)
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
#if defined(__SIZEOF_INT128__)
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
#if defined(__SIZEOF_INT128__)
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

// Knuth's algorithm D (TAOCP 4.3.1). u has m limbs, v has n limbs with
// v[n−1] != 0 and m >= n. Writes the m−n+1 quotient limbs to q and the n
// remainder limbs to r. The caller provides scratch: un (m+1) and vn (n).
template <std::unsigned_integral L>
constexpr void divmod(const L* u, std::size_t m, const L* v, std::size_t n, L* q, L* r, L* un, L* vn) noexcept {
    constexpr int B = bits<L>;
    if (n == 1) {
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
#if defined(__SIZEOF_INT128__)
        using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
        using W = std::uint64_t;
#endif
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
        const L fill = o.negative() ? static_cast<L>(~L{0}) : L{0};
        for (std::size_t i = 0; i < N; ++i)
            Word[i] = (i < M) ? o.Word[i] : fill;
    }

    [[nodiscard]] constexpr bool negative() const noexcept { return Signed && (Word[N - 1] >> (limb_bits - 1)) != 0; }

    [[nodiscard]] constexpr bool is_zero() const noexcept {
        for (std::size_t i = 0; i < N; ++i)
            if (Word[i] != 0)
                return false;
        return true;
    }

    constexpr explicit operator bool() const noexcept { return !is_zero(); }

    // To a builtin integer: keeps the low bits (two's complement), as the
    // builtin narrowing conversions do.
    template <std::integral T>
    constexpr explicit operator T() const noexcept {
#if defined(__SIZEOF_INT128__)
        using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
        using W = std::uint64_t;
#endif
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
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] = static_cast<L>(~a.Word[i]);
        return a;
    }
    friend constexpr wide_int operator&(wide_int a, const wide_int& b) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] &= b.Word[i];
        return a;
    }
    friend constexpr wide_int operator|(wide_int a, const wide_int& b) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] |= b.Word[i];
        return a;
    }
    friend constexpr wide_int operator^(wide_int a, const wide_int& b) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] ^= b.Word[i];
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
        for (std::size_t i = 0; i < N; ++i)
            if (a.Word[i] != b.Word[i])
                return false;
        return true;
    }
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

    // Number of significant bits of a non-negative value (0 for 0).
    [[nodiscard]] friend constexpr int bit_width_of(const wide_int& a) noexcept {
        for (std::size_t i = N; i-- > 0;)
            if (a.Word[i] != 0)
                return static_cast<int>(i) * limb_bits + (limb_bits - std::countl_zero(a.Word[i]));
        return 0;
    }

  private:
    static constexpr wide_int shl(const wide_int& a, int s) noexcept {
        wide_int r{0};
        if (s >= bits)
            return r;
        const std::size_t ws = static_cast<std::size_t>(s / limb_bits);
        const int         bs = s % limb_bits;
        for (std::size_t i = ws; i < N; ++i) {
            L w = static_cast<L>(a.Word[i - ws] << bs);
            if (bs != 0 && i > ws)
                w |= static_cast<L>(a.Word[i - ws - 1] >> (limb_bits - bs));
            r.Word[i] = w;
        }
        return r;
    }
    static constexpr wide_int shr(const wide_int& a, int s, L fill) noexcept {
        wide_int r;
        if (s >= bits) {
            for (std::size_t i = 0; i < N; ++i)
                r.Word[i] = fill;
            return r;
        }
        const std::size_t ws   = static_cast<std::size_t>(s / limb_bits);
        const int         bs   = s % limb_bits;
        const auto        word = [&](std::size_t k) { return k < N ? a.Word[k] : fill; };
        for (std::size_t i = 0; i < N; ++i) {
            L w = static_cast<L>(word(i + ws) >> bs);
            if (bs != 0)
                w |= static_cast<L>(word(i + ws + 1) << (limb_bits - bs));
            r.Word[i] = w;
        }
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
    if (b.is_zero()) {
        if consteval {
            constexpr_error<"wide_int: division by zero">();
        }
        raise(errc::division_by_zero, "wide_int: division by zero");
    }
    // Divide the magnitudes; −min reads correctly as an unsigned magnitude.
    const bool     na = a.negative(), nb = b.negative();
    const wide_int ua = na ? -a : a, ub = nb ? -b : b;
    std::size_t    m = N, n = N;
    while (m > 0 && ua.Word[m - 1] == 0)
        --m;
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
    for (std::size_t i = K; i-- > 0;) {
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

//---------------------------------------------------------------------------
// exact_frac — an exact value as a fraction of K-limb integers: the exact
// paths' intermediate (wide_value.hpp), and, reduced, the raw of a continuous
// grid whose limits pass 64 bits. Structural, so it can be a raw.
//---------------------------------------------------------------------------
inline constexpr std::size_t exact_min_limbs = 8; // scalars, 64-bit rationals

template <std::size_t K>
struct exact_frac {
    wide_sint<K> Num{0};
    wide_sint<K> Den{1}; // > 0; not reduced

    constexpr exact_frac() = default;
    constexpr exact_frac(wide_sint<K> n, wide_sint<K> d) noexcept : Num{n}, Den{d} {}
    template <std::size_t M>
        requires(M < K)
    constexpr exact_frac(const exact_frac<M>& o) noexcept : Num{o.Num}, Den{o.Den} {} // widens

    constexpr explicit operator double() const noexcept { return static_cast<double>(Num) / static_cast<double>(Den); }
};

template <typename T>
inline constexpr bool is_exact_frac_v = false;
template <std::size_t K>
inline constexpr bool is_exact_frac_v<exact_frac<K>> = true;
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
[[nodiscard]] constexpr umax abs_den(imax d) noexcept {
    return (d >= 0) ? static_cast<umax>(d) : umax{0} - static_cast<umax>(d);
}

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
    denominator = (denominator < 0) ? -ad : ad;
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
// Overflow / malformed-literal signalling
//---------------------------------------------------------------------------
// Failure aborts constant evaluation via `detail::constexpr_error<Msg>()` — a
// non-constexpr [[noreturn]] helper carrying the message in an NTTP (literal
// parsers and the checked paths' `fail<"...">` under `if consteval`),
// hard-failing the build with the text in the diagnostic; at runtime those
// paths fall through to `std::unexpected`. No `throw`, so it is -fno-exceptions clean.

//---------------------------------------------------------------------------
// rational — structural type for NTTP (public members only). Sign is encoded
// in the denominator (negative = negative rational); numerator is unsigned to
// represent e.g. umax itself.
//---------------------------------------------------------------------------
struct rational {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    constexpr rational(std::floating_point auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1) : Numerator{num}, Denominator{den} {
        canonicalize(Numerator, Denominator);
    }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. Numerator is unsigned anyway; the cast on `den` is
    // safe because the unsigned-integral concept exclude negative inputs.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den) : Numerator{num}, Denominator{static_cast<imax>(den)} {
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
    template <arithmetic T>
    [[nodiscard]] constexpr bool operator==(T value) const {
        return operator==(rational{value});
    }

    [[nodiscard]] constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr std::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T() const {
        if (Denominator < 0)
            raise(errc::domain_error, "cannot convert negative rational to unsigned");
        return Numerator / abs_den(Denominator);
    }

    template <std::signed_integral T>
    explicit constexpr operator T() const {
        umax q = Numerator / abs_den(Denominator);
        return (Denominator < 0) ? -q : q;
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
    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    // Two domain errors: Denominator == 0 (undefined)
    // and Denominator == imax_min (cannot be negated without UB, which every
    // sign-flip in the file assumes is well-defined).
    static constexpr void canonicalize(umax& num, imax& den) {
        if (den == 0)
            raise(errc::domain_error, "Denominator of Zero is invalid");
        if (den == std::numeric_limits<imax>::min())
            raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
        if (num == 0)
            den = 1;
        trim(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB (the ctor-body canonicalize() would catch
    // these too, but only after the mem-init already evaluated `-den`).
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den) {
        if (den == 0)
            raise(errc::domain_error, "Denominator of Zero is invalid");
        if (den == std::numeric_limits<imax>::min())
            raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
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
[[nodiscard]] constexpr imax signed_numerator(rational v) noexcept {
    const imax n = static_cast<imax>(v.Numerator);
    return (v.Denominator < 0) ? -n : n;
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
    return neg ? -mag : mag;
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
    if (denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
        return std::unexpected{errc::overflow};

    auto numerator = std::gcd(lhs.Numerator, rhs.Numerator);
    return rational{numerator, denominator};
}

//---------------------------------------------------------------------------
// rational::rational
//---------------------------------------------------------------------------
constexpr rational::rational(std::signed_integral auto num, imax den)
    : Numerator{safe_abs(num)}, Denominator{signed_den_from(num, den)} {
    canonicalize(Numerator, Denominator);
}

constexpr rational::rational(std::floating_point auto value) {
    if (not is_finite(value))
        raise(errc::not_finite, "non-finite double");

    if (value == 0.0) {
        Numerator   = 0;
        Denominator = 1;
        return;
    }

    bool neg = (value < 0.0);
    if (neg)
        value = -value;

    auto [num, den] = abs_fraction(value);
    Numerator       = num;
    Denominator     = neg ? -den : den;
    // trim not needed, because abs_fraction already trims in its special case
}

//---------------------------------------------------------------------------
// to
//---------------------------------------------------------------------------
template <std::unsigned_integral T>
constexpr std::expected<T, errc> rational::to() const {
    if (Denominator < 0)
        return std::unexpected{errc::domain_error};
    const umax q = Numerator / abs_den(Denominator);
    if (q > static_cast<umax>(std::numeric_limits<T>::max()))
        return std::unexpected{errc::overflow};
    return static_cast<T>(q);
}

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
// runtime form behind from_chars.
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

// Unsigned number in [first, last): the grammar of the _ins / _r literals.
constexpr parsed_number parse_number(const char* first, const char* last) {
    const std::size_t N    = static_cast<std::size_t>(last - first);
    auto              fail = [](parse_fail f) { return parsed_number{rational{}, f}; };

    // Detect radix prefix.
    int         base = 10;
    std::size_t i    = 0;
    if (N >= 2 && first[0] == '0') {
        if (first[1] == 'x' || first[1] == 'X') {
            base = 16;
            i    = 2;
        } else if (first[1] == 'b' || first[1] == 'B') {
            base = 2;
            i    = 2;
        }
    }

    umax num            = 0;
    int  frac_len       = 0;
    bool in_frac        = false;
    bool seen_digit     = false;
    int  exp            = 0;
    bool exp_neg        = false;
    bool has_p_exp      = false; // 2^exp (hex floats)
    bool has_e_exp      = false; // 10^exp (decimal scientific)
    bool in_exp         = false;
    bool exp_seen_digit = false;
    int  frac_zeros     = 0; // deferred fractional zeros (see the digit loop)

    for (; i < N; ++i) {
        const char c = first[i];
        if (c == '\'')
            continue;

        if (in_exp) {
            if (!exp_seen_digit && (c == '+' || c == '-')) {
                exp_neg = (c == '-');
                continue;
            }
            if (c >= '0' && c <= '9') {
                if (exp < 100000)
                    exp = exp * 10 + (c - '0'); // past 10^5 it overflows anyway
                exp_seen_digit = true;
                continue;
            }
            return fail(parse_fail::invalid_exponent);
        }

        if (c == '.') {
            if (in_frac)
                return fail(parse_fail::multiple_dots);
            if (base == 2)
                return fail(parse_fail::dot_in_binary);
            in_frac = true;
            continue;
        }

        if ((c == 'p' || c == 'P') && base == 16) {
            has_p_exp = true;
            in_exp    = true;
            continue;
        }

        if ((c == 'e' || c == 'E') && base == 10) {
            has_e_exp = true;
            in_exp    = true;
            continue;
        }

        const int d = parse_digit(c, base);
        if (d < 0)
            return fail(parse_fail::invalid_digit);
        seen_digit = true;

        const umax base_u = static_cast<umax>(base);
        // A fractional zero only matters if a non-zero digit follows: defer it,
        // so trailing zeros ("1.000…0") cannot overflow num / den.
        if (in_frac && d == 0) {
            ++frac_zeros;
            continue;
        }
        for (; frac_zeros > 0; --frac_zeros, ++frac_len) {
            if (num > (~umax{0}) / base_u)
                return fail(parse_fail::numerator_overflow);
            num *= base_u;
        }
        if (num > (~umax{0} - static_cast<umax>(d)) / base_u)
            return fail(parse_fail::numerator_overflow);
        num = num * base_u + static_cast<umax>(d);
        if (in_frac)
            ++frac_len;
    }
    if (!seen_digit)
        return fail(parse_fail::empty);
    if (in_exp && !exp_seen_digit)
        return fail(parse_fail::invalid_exponent);

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
    if (has_p_exp) {
        if (exp >= 63)
            return fail(parse_fail::p_exponent_too_large);
        if (!exp_neg) {
            if (num > (~umax{0}) >> exp)
                return fail(parse_fail::numerator_overflow);
            num <<= exp;
        } else {
            if (den > (~umax{0}) >> exp)
                return fail(parse_fail::p_exponent_denominator_overflow);
            den <<= exp;
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

    if (den > static_cast<umax>(std::numeric_limits<imax>::max()))
        return fail(parse_fail::denominator_overflow);
    return parsed_number{rational{num, den}, parse_fail::none};
}

template <char... Chars>
consteval rational parse_ins_literal() {
    constexpr char          src[] = {Chars..., '\0'};
    constexpr parsed_number r     = parse_number(src, src + sizeof...(Chars));
    if constexpr (r.Fail == parse_fail::invalid_exponent)
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
constexpr std::expected<rational, errc> parse_text(const char* first, const char* last) {
    bool neg = false;
    if (first != last && (*first == '+' || *first == '-')) {
        neg = (*first == '-');
        ++first;
    }
    const char* slash = first;
    while (slash != last && *slash != '/')
        ++slash;

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

    std::expected<rational, errc> v = one(first, slash);
    if (v && slash != last) {
        const auto d = one(slash + 1, last);
        if (!d)
            return d;
        if (d->Numerator == 0)
            return std::unexpected{errc::division_by_zero};
        v = *v / *d;
    }
    if (v && neg)
        v = -*v;
    return v;
}

template <char... Chars>
constexpr rational operator""_r() {
    return parse_ins_literal<Chars...>();
}

// per<D> and frac<N, D> are defined publicly in `namespace beman::inside` (see the
// re-export block at the end of this header) so consumers spell grid values
// without naming the internal representation type.

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
            return ret_t{0_r};
        rational r;
        r.Numerator   = num;
        r.Denominator = r_neg ? -a_ad : a_ad;
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
            denominator > static_cast<umax>(std::numeric_limits<imax>::max())) {
            return ret_t{fail<"rational +: denominator overflow", Loud>(errc::overflow)};
        }
        if (mul_overflow(a.Numerator, b_ad_r, &A) || mul_overflow(b.Numerator, a_ad_r, &B)) {
            // Mixed signs: |A − B| can fit umax even when a cross-product alone
            // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
            // subtraction brings it back in range — the former double engine's store hit
            // exactly this). Retry the difference in 128-bit before giving up.
            if (a_neg != b_neg) {
                const limb::pair<umax> A2       = limb::mul(a.Numerator, b_ad_r);
                const limb::pair<umax> B2       = limb::mul(b.Numerator, a_ad_r);
                const bool             a_bigger = A2.Hi != B2.Hi ? A2.Hi > B2.Hi : A2.Lo > B2.Lo;
                const limb::pair<umax> big = a_bigger ? A2 : B2, small = a_bigger ? B2 : A2;
                if (big.Hi - small.Hi - (big.Lo < small.Lo ? 1u : 0u) == 0) {
                    rational r;
                    r.Numerator   = big.Lo - small.Lo;
                    r.Denominator = (a_neg ? a_bigger : !a_bigger) ? -denominator : denominator;
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
        // assemble directly and trim.
        rational r;
        r.Numerator   = numerator;
        r.Denominator = a_neg ? -denominator : denominator;
        trim(r.Numerator, r.Denominator);
        return ret_t{r};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which for canonical inputs has equal denominators and
    // took the equal-denominator branch above.
    umax     numerator = (A > B) ? (A - B) : (B - A);
    bool     r_neg     = a_neg ? (A > B) : (B > A);
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator : denominator;
    trim(r.Numerator, r.Denominator);
    return ret_t{r};
}

template <bool Checked, bool Loud>
inline constexpr auto rational::mul_impl(const rational& a_in, const rational& b_in) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0)
        return ret_t{0_r};

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
            denominator > static_cast<umax>(std::numeric_limits<imax>::max())) {
            return ret_t{fail<"rational *: numerator or denominator overflow", Loud>(errc::overflow)};
        }
    } else {
        numerator   = a.Numerator * b.Numerator;
        denominator = a_ad * b_ad;
    }

    // The cross-trims above guarantee gcd(numerator, denominator) == 1, so
    // bypass rational(num, den) and skip its redundant trim.
    return ret_t{make_raw(numerator, r_neg ? -denominator : denominator)};
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
        if (a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max())) {
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
        auto inv_b = inv_impl<true, Loud>(b);
        if (!inv_b.has_value())
            return ret_t{std::unexpected{inv_b.error()}};
        return mul_impl<true, Loud>(a, *inv_b);
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

template <typename T>
[[nodiscard]] inline constexpr auto operator<=>(const std::expected<T, errc>& lhs, const rational& rhs) {
    return rational{lhs.value()} <=> rhs;
}

template <typename T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, const std::expected<T, errc>& rhs) {
    return lhs <=> rational{rhs.value()};
}

template <arithmetic T>
[[nodiscard]] inline constexpr auto operator<=>(T lhs, const rational& rhs) {
    return rational{lhs} <=> rhs;
}

template <arithmetic T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, T rhs) {
    return lhs <=> rational{rhs};
}

//---------------------------------------------------------------------------
// Expected-lifting operators — one generic overload per arithmetic operator
// that engages when an operand is a std::expected, both unwrap to arithmetic,
// and at least one to rational. Gating on `arithmetic` (not `insidable`, which
// isn't visible this low) excludes inside operands, so inside-involving expected
// expressions partition cleanly to arithmetic.hpp's generic instead.
//---------------------------------------------------------------------------
template <class L, class R>
concept rational_lift_operands =
    (is_expected_v<L> || is_expected_v<R>) && arithmetic<unwrap_t<L>> && arithmetic<unwrap_t<R>> &&
    (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

//---------------------------------------------------------------------------
// Binary operators: the checked rational ⋈ rational cores, then per operator
// the arithmetic-operand forms (direct construction, no lift overhead), the
// expected-operand form (propagates via lift), and the compound assignments.
// Compound assignments unwrap with .value() — std::bad_expected_access on
// overflow; callers needing a non-throwing path use the binary operators.
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<rational, errc> operator+(const rational& lhs, const rational& rhs) {
    return rational::add_impl<true>(lhs, rhs);
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
    return rational::add_impl<true, false>(a, b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_sub(const rational& a, const rational& b) {
    return rational::add_impl<true, false>(a, -b);
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
    template <arithmetic T>                                                                                 \
    [[nodiscard]] inline constexpr auto operator op(T lhs, rational const& rhs) {                           \
        return rational{lhs} op rhs;                                                                        \
    }                                                                                                       \
    template <arithmetic T>                                                                                 \
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
    template <arithmetic T>                                                                                 \
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

} // namespace beman::inside::detail

namespace beman::inside::detail {
// Checked builders for the public helpers below: a static_assert here fires at
// the user's spelling. Denominators are unsigned, so a sign can only sit in
// frac's numerator.
template <umax D>
consteval rational make_per() {
    static_assert(D >= 1, "per<D> is the positive step 1/D; a continuous grid is spelled 0");
    static_assert(D <= static_cast<umax>(std::numeric_limits<imax>::max()), "per<D>: denominator too large");
    return rational{umax{1}, D};
}

template <imax N, umax D>
consteval rational make_frac() {
    static_assert(D >= 1, "frac<N, D>: the denominator must be at least 1");
    static_assert(D <= static_cast<umax>(std::numeric_limits<imax>::max()), "frac<N, D>: denominator too large");
    return rational{N, static_cast<imax>(D)};
}
} // namespace beman::inside::detail

namespace beman::inside {
// `rational` — an exact 64-bit fraction: the value of a literal (`0.1_r`), a
// continuous inside's raw, and a runtime exact value to compute with.
using detail::rational;

// The grid-building helpers:
//   per<D>       the step 1/D — the common notch (per<256> is Q·8);
//   frac<N, D>   any exact ratio (signed numerator): a limit like frac<-6, 5>
//                for -1.2, or another step like frac<360, 4096>;
//   _r literal   an exact decimal / hex value, e.g. 0.1_r is exactly 1/10.
// Integer steps are plain integers ({{0, 100}, 5}); grid::validate rejects a
// negative notch however it is spelled.
template <umax D>
inline constexpr detail::rational per = detail::make_per<D>();

template <imax N, umax D = 1>
inline constexpr detail::rational frac = detail::make_frac<N, D>();

using detail::operator""_r;
} // namespace beman::inside


// ======================================================================
//  beman/inside/interval.hpp
// ======================================================================


// ======================================================================
//  beman/inside/detail/grid_rational.hpp
// ======================================================================


//---------------------------------------------------------------------------
// grid_rational — the number type of a grid's limits and notch (the NTTP
// substrate of `interval` and `grid`).
//
// BEMAN_INSIDE_BIG_GRIDS selects it. Under C++26 static reflection
// (std::define_static_array) a grid number has no size limit: its limbs are
// interned in static storage, so equal values stay the same template argument.
// Without reflection (C++23, older compilers) it is the 64-bit runtime
// `rational`, and grids keep today's limits. Define the macro to 0 to force the
// 64-bit grids on a C++26 compiler.
//
// The two modes give `grid` and `interval` different layouts, so each mode
// lives in its own inline namespace: linking a C++23 TU against a C++26 one is
// a link error, not a silent ODR violation.
//---------------------------------------------------------------------------
#if !defined(BEMAN_INSIDE_BIG_GRIDS)
    #if defined(__cpp_impl_reflection) && __has_include(<meta>)
        #define BEMAN_INSIDE_BIG_GRIDS 1
    #else
        #define BEMAN_INSIDE_BIG_GRIDS 0
    #endif
#endif


// ======================================================================
//  beman/inside/detail/big_rational.hpp
// ======================================================================



//---------------------------------------------------------------------------
// big_int / big_rational — grid numbers of any size (C++26 static reflection).
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
// Included by grid_rational.hpp, which detects BEMAN_INSIDE_BIG_GRIDS.
#if defined(BEMAN_INSIDE_BIG_GRIDS) && BEMAN_INSIDE_BIG_GRIDS
    #include <meta>

namespace beman::inside::detail {
namespace big {
using mag = std::vector<umax>; // little-endian, no top zeros

constexpr void trim(mag& m) {
    while (!m.empty() && m.back() == 0)
        m.pop_back();
}

constexpr std::strong_ordering compare(const mag& a, const mag& b) {
    if (a.size() != b.size())
        return a.size() <=> b.size();
    for (std::size_t i = a.size(); i-- > 0;)
        if (a[i] != b[i])
            return a[i] <=> b[i];
    return std::strong_ordering::equal;
}

constexpr mag add(const mag& a, const mag& b) {
    mag  r(a.size() > b.size() ? a.size() + 1 : b.size() + 1, 0);
    umax c = 0;
    for (std::size_t i = 0; i + 1 < r.size(); ++i)
        r[i] = limb::add_carry(i < a.size() ? a[i] : umax{0}, i < b.size() ? b[i] : umax{0}, c);
    r.back() = c;
    trim(r);
    return r;
}

// a − b for a ≥ b.
constexpr mag sub(const mag& a, const mag& b) {
    mag  r(a.size(), 0);
    umax br = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        r[i] = limb::sub_borrow(a[i], i < b.size() ? b[i] : umax{0}, br);
    trim(r);
    return r;
}

constexpr mag mul(const mag& a, const mag& b) {
    if (a.empty() || b.empty())
        return {};
    mag r(a.size() + b.size(), 0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        umax carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j) {
            const limb::pair<umax> p  = limb::mul(a[i], b[j]);
            umax                   c1 = 0, c2 = 0;
            umax                   s = limb::add_carry(r[i + j], p.Lo, c1);
            s                        = limb::add_carry(s, carry, c2);
            r[i + j]                 = s;
            carry                    = p.Hi + c1 + c2;
        }
        r[i + b.size()] = carry;
    }
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
    while (!b.empty()) {
        mag t = divmod(a, b).Remainder;
        a     = std::move(b);
        b     = std::move(t);
    }
    return a;
}

constexpr int bit_width(const mag& m) {
    return m.empty() ? 0 : static_cast<int>(m.size() - 1) * 64 + (64 - std::countl_zero(m.back()));
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
        const bool                  neg = w.negative();
        const wide_int<N, false, L> m(neg ? -w : w);
        big::mag                    v;
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (std::numeric_limits<L>::digits == 64)
                v.push_back(m.Word[i]);
            else
                static_assert(std::numeric_limits<L>::digits == 64, "big_int: umax limbs only");
        }
        *this = from_mag(std::move(v), neg);
    }

    // By value, not by pointer (see Size): equal magnitudes intern to the
    // same array, so this matches the defaulted comparison.
    friend constexpr bool operator==(const big_int& a, const big_int& b) noexcept {
        if (a.Size != b.Size || a.Small != b.Small || a.Negative != b.Negative)
            return false;
        for (std::size_t i = 0; i < a.Size; ++i)
            if (a.Limbs[i] != b.Limbs[i])
                return false;
        return true;
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
        requires(std::integral<T> || is_wide_int_v<T>)
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
        big::mag          m     = a.magnitude();
        const std::size_t words = static_cast<std::size_t>(k / 64);
        const int         bits  = k % 64;
        big::mag          r(m.size() + words + 1, 0);
        for (std::size_t i = 0; i < m.size(); ++i) {
            r[i + words] |= m[i] << bits;
            if (bits != 0)
                r[i + words + 1] |= m[i] >> (64 - bits);
        }
        return from_mag(std::move(r), a.Negative);
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
    if (b.is_zero()) {
        if consteval {
            constexpr_error<"big_int: division by zero">();
        }
        raise(errc::division_by_zero, "big_int: division by zero");
    }
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

constexpr big_int gcd(big_int a, big_int b) {
    a = abs(a);
    b = abs(b);
    while (!b.is_zero()) {
        const big_int t = a % b;
        a               = b;
        b               = t;
    }
    return a;
}

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
    constexpr big_rational(std::floating_point auto d) // exact binary value
    {
        if (d != d || d - d != 0) {
            if consteval {
                constexpr_error<"big_rational: not a finite number">();
            }
            raise(errc::not_finite, "big_rational: not a finite number");
        }
        int        e   = 0;
        double     m   = frexp(static_cast<double>(d), &e); // d = m·2^e, |m| in [0.5, 1)
        const bool neg = m < 0;
        if (neg)
            m = -m;
        const umax mant = static_cast<umax>(ldexp(m, 53)); // exact: 53 bits
        e -= 53;
        big_int n{mant}, den{1};
        for (; e > 0; --e)
            n = n * big_int{2};
        for (; e < 0; ++e)
            den = den * big_int{2};
        Num = neg ? -n : n;
        Den = den;
        normalize();
    }

    friend constexpr bool operator==(const big_rational&, const big_rational&) = default;

    constexpr void normalize() {
        if (Den.is_zero()) {
            if consteval {
                constexpr_error<"big_rational: zero denominator">();
            }
            raise(errc::division_by_zero, "big_rational: zero denominator");
        }
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
        return Num.fits_limb() && Den.fits_limb() && Den.Small <= static_cast<umax>(std::numeric_limits<imax>::max());
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
        return a.Num * b.Den <=> b.Num * a.Den;
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

    // The nearest double, by way of the top 64 bits of each part.
    constexpr explicit operator double() const {
        auto to_double = [](const big_int& v) {
            const big::mag m  = v.magnitude();
            const int      bw = big::bit_width(m);
            double         d;
            if (bw <= 64)
                d = static_cast<double>(m.empty() ? umax{0} : m[0]);
            else {
                // top 64 bits plus a sticky bit for everything below
                const int         sh  = bw - 64;
                const std::size_t w   = static_cast<std::size_t>(sh / 64);
                const int         b   = sh % 64;
                umax              top = m[w] >> b;
                if (b != 0 && w + 1 < m.size())
                    top |= m[w + 1] << (64 - b);
                bool sticky = b != 0 && (m[w] << (64 - b)) != 0;
                for (std::size_t i = 0; i < w && !sticky; ++i)
                    sticky = m[i] != 0;
                d = ldexp(static_cast<double>(top | (sticky ? 1u : 0u)), sh);
            }
            return v.negative() ? -d : d;
        };
        return to_double(Num) / to_double(Den);
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

#endif // BEMAN_INSIDE_BIG_GRIDS

#if BEMAN_INSIDE_BIG_GRIDS
    #define BEMAN_INSIDE_GRID_ABI big_grids_v1
#else
    #define BEMAN_INSIDE_GRID_ABI small_grids_v1
#endif

//---------------------------------------------------------------------------
// The grid-number vocabulary, the same in both modes:
//   grid_rational          the type of a grid's limits and notch
//   grid_wide              exact integers for grid computations (slot counts,
//                          value indices): wide enough for every grid
//   wide_numerator(r) /    a grid number's signed numerator and positive
//   wide_denominator(r)    denominator as grid_wide (also for a rational)
//   grid_divides_evenly    a / n is an integer (true for n == 0)
//   grid_same_lattice      (a − b) / n is an integer (true for n == 0)
//   grid_gcd               gcd of two fractions; may fail only with 64-bit
//                          grid numbers (returns expected there)
//   fits_rational(r) /     whether, and as which, 64-bit rational a grid
//   to_rational(r)         number serves the 64-bit-only paths
//---------------------------------------------------------------------------
namespace beman::inside::detail {
#if BEMAN_INSIDE_BIG_GRIDS
using grid_rational = big_rational;
using grid_wide     = big_int;

constexpr grid_wide wide_numerator(const big_rational& r) { return r.Num; }
constexpr grid_wide wide_denominator(const big_rational& r) { return r.Den; }
constexpr grid_wide wide_numerator(const rational& r) {
    const grid_wide n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
}
constexpr grid_wide wide_denominator(const rational& r) { return grid_wide{abs_den(r.Denominator)}; }

// (Convention, as divides_evenly: everything divides 0 evenly.)
constexpr bool grid_divides_evenly(const big_rational& a, const big_rational& n) {
    return n == 0 || (a / n).is_integer();
}

// (a − b) / n is an integer (true for n == 0): b and a share n's lattice.
constexpr bool grid_same_lattice(const big_rational& a, const big_rational& b, const big_rational& n) {
    return grid_divides_evenly(a - b, n);
}

// The 64-bit rational of a grid number, for 64-bit-only paths.
constexpr bool         fits_rational(const big_rational& r) { return r.fits_rational(); }
constexpr rational     to_rational(const big_rational& r) { return r; }
constexpr big_rational grid_gcd(const big_rational& a, const big_rational& b) { return gcd(a, b); }
#else
using grid_rational = rational;
// A product of three 64-bit magnitudes plus a sign.
using grid_wide = wide_sint<4>;

constexpr grid_wide wide_numerator(const rational& r) noexcept {
    const grid_wide n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
}
constexpr grid_wide wide_denominator(const rational& r) noexcept { return grid_wide{abs_den(r.Denominator)}; }

constexpr bool grid_divides_evenly(const rational& a, const rational& n) { return divides_evenly(a, n); }

// (A difference past the rational range has no lattice offset to test:
// compare both ends' residues instead.)
constexpr bool grid_same_lattice(const rational& a, const rational& b, const rational& n) {
    if (n == 0)
        return true;
    if (const auto d = try_sub(a, b))
        return divides_evenly(*d, n);
    return divides_evenly(a, n) && divides_evenly(b, n);
}

constexpr bool                          fits_rational(const rational&) { return true; }
constexpr rational                      to_rational(const rational& r) { return r; }
constexpr std::expected<rational, errc> grid_gcd(const rational& a, const rational& b) { return gcd(a, b); }
#endif

// + − × and gcd of grid numbers as grid numbers in both modes, for the
// compile-time grid computations (a 64-bit overflow stops the build).
constexpr grid_rational grid_add(const grid_rational& a, const grid_rational& b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return a + b;
#else
    return (a + b).value();
#endif
}
constexpr grid_rational grid_sub(const grid_rational& a, const grid_rational& b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return a - b;
#else
    return (a - b).value();
#endif
}
constexpr grid_rational grid_mul(const grid_rational& a, const grid_rational& b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return a * b;
#else
    return (a * b).value();
#endif
}
constexpr grid_rational grid_div_of(const grid_rational& a, const grid_rational& b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return a / b;
#else
    return (a / b).value();
#endif
}
constexpr grid_rational grid_gcd_of(const grid_rational& a, const grid_rational& b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return grid_gcd(a, b);
#else
    return grid_gcd(a, b).value();
#endif
}

//---------------------------------------------------------------------------
// parse_grid_literal — the _g literal: decimal digits (with ' separators),
// an optional point and an optional e±n exponent, taken exactly. With big
// grid numbers any size; with 64-bit grid numbers the value must fit a
// 64-bit rational.
//---------------------------------------------------------------------------
template <char... Chars>
consteval grid_rational parse_grid_literal() {
    constexpr char  text[] = {Chars...};
    const grid_wide ten{10};
    grid_wide       num{0}, den{1};
    std::size_t     i     = 0;
    bool            point = false, digits = false;
    for (; i < sizeof(text); ++i) {
        const char c = text[i];
        if (c == '\'')
            continue;
        if (c == '.' && !point) {
            point = true;
            continue;
        }
        if (c < '0' || c > '9')
            break;
        num = num * ten + grid_wide{c - '0'};
        if (point)
            den = den * ten;
        digits = true;
    }
    if (!digits)
        constexpr_error<"_g literal: expected decimal digits">();
    if (i < sizeof(text)) {
        if (text[i] != 'e' && text[i] != 'E')
            constexpr_error<"_g literal: decimal digits, a point and e±n only">();
        ++i;
        bool neg = false;
        if (i < sizeof(text) && (text[i] == '+' || text[i] == '-')) {
            neg = text[i] == '-';
            ++i;
        }
        if (i == sizeof(text))
            constexpr_error<"_g literal: missing exponent digits">();
        int e = 0;
        for (; i < sizeof(text); ++i) {
            if (text[i] < '0' || text[i] > '9')
                constexpr_error<"_g literal: decimal digits, a point and e±n only">();
            e = e * 10 + (text[i] - '0');
            if (e > 10000)
                constexpr_error<"_g literal: exponent too large">();
        }
        for (; e > 0; --e)
            (neg ? den : num) = (neg ? den : num) * ten;
    }
#if BEMAN_INSIDE_BIG_GRIDS
    return big_rational{num, den};
#else
    grid_wide a = num, b = den; // reduce, then it must fit
    while (!(b == grid_wide{0})) {
        const grid_wide t = a % b;
        a                 = b;
        b                 = t;
    }
    num = num / a;
    den = den / a;
    if (grid_wide{std::numeric_limits<umax>::max()} < num || grid_wide{std::numeric_limits<imax>::max()} < den)
        constexpr_error<"_g literal: past the 64-bit grid numbers (needs C++26 big grids)">();
    return rational{static_cast<umax>(num), static_cast<imax>(den)};
#endif
}
} // namespace beman::inside::detail

namespace beman::inside {
// A grid number: `inside<{0, 1267650600228229401496703205376_g}>`, or an
// exact decimal notch `{{0, 1}, 1e-30_g}`. Any size under C++26 big grids.
template <char... Chars>
consteval detail::grid_rational operator""_g() {
    return detail::parse_grid_literal<Chars...>();
}
} // namespace beman::inside



namespace beman::inside {
//---------------------------------------------------------------------------
// interval — structural NTTP type (public members only) with inclusive Lower
// and Upper bounds. Like `grid`, its operator+/-/*// computes result intervals
// at compile time; division returns errc::division_by_zero when the divisor straddles zero
// (grid::operator/ re-runs on the two zero-free halves and unions them).
//---------------------------------------------------------------------------
inline namespace BEMAN_INSIDE_GRID_ABI {
struct interval {
    detail::grid_rational Lower;
    detail::grid_rational Upper;

    interval() = default;

    constexpr interval(detail::grid_rational lower, detail::grid_rational upper) : Lower{lower}, Upper{upper} {}
    constexpr interval(detail::arithmetic auto lower, detail::arithmetic auto upper) : Lower{lower}, Upper{upper} {}

    template <auto I>
    static constexpr bool validate() {
        static_assert(I.Lower <= I.Upper);
        return true;
    }

    [[nodiscard]] constexpr bool     operator==(const interval& rhs) const = default;
    [[nodiscard]] constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    // With 64-bit grid numbers a span past the rational range (an interval
    // reaching past int64 on both sides) is tested endpoint by endpoint:
    // equal residues mod notch.
    [[nodiscard]] constexpr bool divides_evenly(const detail::grid_rational& notch) const {
#if BEMAN_INSIDE_BIG_GRIDS
        return detail::grid_divides_evenly(Upper - Lower, notch);
#else
        if (const auto span = detail::try_sub(Upper, Lower))
            return detail::divides_evenly(*span, notch);
        return detail::divides_evenly(Lower, notch) && detail::divides_evenly(Upper, notch);
#endif
    }

    // The span in units of `notch` (the slot count of a grid).
    [[nodiscard]] constexpr std::expected<detail::grid_rational, errc>
    operator/(const detail::grid_rational& notch) const {
#if BEMAN_INSIDE_BIG_GRIDS
        if (notch == 0)
            return std::unexpected{errc::division_by_zero};
        return (Upper - Lower) / notch;
#else
        return (Upper - Lower) / notch;
#endif
    }
};
} // namespace BEMAN_INSIDE_GRID_ABI

// Containment / disjointness — free functions over the public endpoints
// (siblings of the binary interval operators below).
[[nodiscard]] constexpr bool includes(const interval& iv, const interval& rhs) noexcept {
    return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper;
}

[[nodiscard]] constexpr bool includes(const interval& iv, const detail::rational& r) noexcept {
    return iv.Lower <= r && r <= iv.Upper;
}

#if BEMAN_INSIDE_BIG_GRIDS
[[nodiscard]] constexpr bool includes(const interval& iv, const detail::grid_rational& r) noexcept {
    return iv.Lower <= r && r <= iv.Upper;
}
#endif

[[nodiscard]] constexpr bool includes(const interval& iv, detail::arithmetic auto a) noexcept {
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
} // namespace detail

[[nodiscard]] constexpr std::expected<interval, errc> operator+(const interval&, const interval&);
[[nodiscard]] constexpr std::expected<interval, errc> operator-(const interval&, const interval&);
[[nodiscard]] constexpr std::expected<interval, errc> operator*(const interval&, const interval&);
[[nodiscard]] constexpr std::expected<interval, errc> operator/(const interval&, const interval&);
[[nodiscard]] constexpr auto operator<=>(const interval&, const interval&) -> std::partial_ordering;

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator+(const interval& lhs, const interval& rhs) {
    return detail::lift([](detail::grid_rational l, detail::grid_rational u) { return interval{l, u}; },
                        lhs.Lower + rhs.Lower,
                        lhs.Upper + rhs.Upper);
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator-(const interval& lhs, const interval& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator*(const interval& lhs, const interval& rhs) {
    return detail::lift(detail::corner_hull,
                        lhs.Lower * rhs.Lower,
                        lhs.Lower * rhs.Upper,
                        lhs.Upper * rhs.Lower,
                        lhs.Upper * rhs.Upper);
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<interval, errc> operator/(const interval& lhs, const interval& rhs) {
    if (includes(rhs, 0))
        return std::unexpected{errc::division_by_zero};

    return detail::lift(detail::corner_hull,
                        lhs.Lower / rhs.Lower,
                        lhs.Lower / rhs.Upper,
                        lhs.Upper / rhs.Lower,
                        lhs.Upper / rhs.Upper);
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
//  beman/inside/policy_flag.hpp
// ======================================================================



// BEMAN_INSIDE_MATH_NO_FP — no hardware floating point anywhere: the math
// engine's double and dd tiers compile out (its integer path computes every
// result; results do not change). Resolved here, in a header every other one
// includes, so all the math headers agree. Define it to force the
// FP-free build; it is auto-enabled on freestanding targets
// (__STDC_HOSTED__ == 0). Public API and grid deduction are unchanged.
#if !defined(BEMAN_INSIDE_MATH_NO_FP)
    #if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
        #define BEMAN_INSIDE_MATH_NO_FP
    #endif
#endif

// -ffast-math is not supported. The library's results are exact or correctly
// rounded, and that rests on IEEE arithmetic as written: double sources are
// screened for infinities and NaN, and the math engine's error bounds and
// error-free sums count every rounding in program order. Fast-math
// lets the compiler assume no NaN or infinity and reassociate, which can
// change results silently, so a build that announces it stops here.
#if defined(__FAST_MATH__) || defined(__ASSOCIATIVE_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
    #error \
        "beman::inside does not support -ffast-math, -fassociative-math or -ffinite-math-only: its results rely on IEEE floating point as written"
#endif

namespace beman::inside {
//---------------------------------------------------------------------------
// policy_flag
//---------------------------------------------------------------------------
using policy_flag = unsigned long long;

// Check model: compile-time checks always run. When success can't be proven
// statically, compilation fails unless the matching ignore flag is set; else a
// runtime check is inserted that throws (or reports via an error_code param).
// Binary operations OR the flags of both operands.
inline constexpr policy_flag none{0ull};
inline constexpr policy_flag ignore_zero{1ull << 1};
inline constexpr policy_flag ignore_range{1ull << 2};
// `snap` — an off-notch value is rounded to fit the grid instead of
// rejected; on its own truncate-toward-zero. Without it, an off-notch value is
// a compile/runtime error and div/mod fall through to exact-rational results.
inline constexpr policy_flag snap{1ull << 4};
inline constexpr policy_flag round_nearest{(1ull << 5) | snap};
// Rounding modes each pick a unique bit and OR in `snap`. Conceptually
// exclusive; combining two is allowed but dispatch (assignment.hpp) picks the
// first match: nearest → floor → ceil → half_even → trunc.
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
// storage_min, docs/storage.md).

// opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
// out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
// check). Includes `snap` so notch-incompatible assigns compile.
namespace detail {
inline constexpr policy_flag unsafe_marker{1ull << 36};
// Marks a cursor (`T::cursor<Step>`): default construction starts at Lower,
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
// Action detection: the `*Pred` struct is the primary detector; the concept
// derives from it and strips cvref so the ref form matches the value form.
template <typename T>
struct is_clamp_action : std::false_type {};
template <typename F>
struct is_clamp_action<on_clamp_t<F>> : std::true_type {};
template <typename T>
struct is_wrap_action : std::false_type {};
template <typename F>
struct is_wrap_action<on_wrap_t<F>> : std::true_type {};
template <typename T>
struct is_error_action : std::false_type {};
template <typename F>
struct is_error_action<on_error_t<F>> : std::true_type {};
template <typename T>
struct is_overflow_action : std::false_type {};
template <typename F>
struct is_overflow_action<on_overflow_t<F>> : std::true_type {};

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
// The `*Pred` structs are reused as template-template parameters (concepts
// can't be passed as such in C++23).
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
// matching the trait, or a static `no_action` fallback if none does. Conflict
// diagnostics elsewhere ensure at most one match.
template <template <typename> class Trait>
inline no_action& pick_action_fallback() {
    static no_action n;
    return n;
}

template <template <typename> class Trait, typename A, typename... Rest>
constexpr auto& pick_action_impl(A& a, Rest&... rest) {
    if constexpr (Trait<std::remove_cvref_t<A>>::value)
        return a;
    else if constexpr (sizeof...(Rest) > 0)
        return pick_action_impl<Trait>(rest...);
    else
        return pick_action_fallback<Trait>();
}

template <template <typename> class Trait, typename... As>
constexpr auto& pick_action(As&... as) {
    if constexpr (sizeof...(As) == 0)
        return pick_action_fallback<Trait>();
    else
        return pick_action_impl<Trait>(as...);
}

// Same, but operating on a tuple (lvalue or rvalue ref).
template <template <typename> class Trait, typename Tuple>
constexpr auto& pick_action_in(Tuple& t) {
    return std::apply([](auto&... as) -> auto& { return pick_action<Trait>(as...); }, t);
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

// Bits counts value bits; a signed type spends one more on the sign.
template <int Bits, bool Signed>
struct int_for_bits {
    static constexpr int total = Bits + (Signed ? 1 : 0);
    using type                 = std::conditional_t<
        (total <= 8),
        std::conditional_t<Signed, std::int8_t, std::uint8_t>,
        std::conditional_t<
            (total <= 16),
            std::conditional_t<Signed, std::int16_t, std::uint16_t>,
            std::conditional_t<(total <= 32),
                               std::conditional_t<Signed, std::int32_t, std::uint32_t>,
                               std::conditional_t<(total <= 64),
                                                  std::conditional_t<Signed, std::int64_t, std::uint64_t>,
                                                  wide_int<limbs_for_bits(total), Signed>>>>>;
};

template <int Bits, bool Signed>
using int_for_bits_t = typename int_for_bits<Bits, Signed>::type;

// An integer the library may hold in a raw or intermediate: a builtin
// integer or a wide_int. (std::integral cannot be extended to wide_int.)
template <typename T>
concept raw_integer = std::integral<T> || is_wide_int_v<T>;

// Signedness of a raw_integer (std::is_signed_v is false for class types).
template <raw_integer T>
inline constexpr bool raw_signed = std::numeric_limits<T>::is_signed;
} // namespace beman::inside::detail




namespace beman::inside {
//---------------------------------------------------------------------------
// grid — structural NTTP type (public members only). Discretizes its interval
// into notch-sized steps (interval must divide evenly by notch; Notch == 0
// allows every rational, raw not offset). Its operator+/-/*// is the engine of
// compile-time result-grid inference: every inside arithmetic operator computes
// its result grid here, so the result interval contains every reachable value.
//---------------------------------------------------------------------------
// A grid corner: a number (int / float / rational / grid number), or
// anything that converts to the 64-bit rational, such as an inside.
template <typename T>
concept grid_number_like = std::convertible_to<T, detail::grid_rational> || std::convertible_to<T, detail::rational>;

inline namespace BEMAN_INSIDE_GRID_ABI {
struct grid {
    interval              Interval;
    detail::grid_rational Notch;

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
    // limits give 1; {0.5, 10} gives 1/2; {frac<-6,5>, frac<3,5>} gives 1/5.
    constexpr grid(grid_number_like auto lower, grid_number_like auto upper)
        : grid{interval{to_number(lower), to_number(upper)}, derive_notch(lower, upper)} {}
    constexpr grid(grid_number_like auto lower)
        : grid{interval{to_number(lower), to_number(lower)}, detail::grid_rational{0}} {}
    constexpr grid(interval val, detail::grid_rational notch) : Interval{val}, Notch{notch} {}

  private:
    template <typename T>
    static constexpr detail::grid_rational to_number(const T& v) {
        if constexpr (std::convertible_to<T, detail::grid_rational>)
            return detail::grid_rational{v};
        else
            return detail::grid_rational{detail::rational{v}};
    }

    // With 64-bit grid numbers a combined denominator past imax has no
    // rational notch: fall back to a continuous grid (notch 0), always valid.
    static constexpr detail::grid_rational derive_notch(auto lower, auto upper) {
        check_short_binary(lower);
        check_short_binary(upper);
        const detail::grid_rational lo = to_number(lower), hi = to_number(upper);
        return detail::lift([](const detail::grid_rational& a,
                               const detail::grid_rational& b) { return detail::grid_gcd(a, b); },
                            detail::lift([](const detail::grid_rational& a,
                                            const detail::grid_rational& b) { return detail::grid_gcd(a, b); },
                                         detail::grid_rational{1},
                                         lo),
                            hi)
            .value_or(detail::grid_rational{0});
    }

    // A floating-point limit is taken as its exact binary value, so 0.1 would
    // derive a 2^-55 notch. Past 1/1024 the literal almost surely meant a
    // decimal: reject it at compile time and point to the exact spellings.
    template <typename T>
    static constexpr void check_short_binary([[maybe_unused]] T v) {
        if constexpr (std::floating_point<T>)
            if (std::is_constant_evaluated() &&
                detail::grid_wide{1024} < detail::wide_denominator(detail::grid_rational{v}))
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
        static_assert(G.Notch >= 0, "grid: the notch must be non-negative");
        // The values are Lower, Lower + Notch, …, Upper: the notch steps from
        // Lower to Upper. Lower need not be a multiple of the notch
        // ({{0.5, 10.5}, 1} holds 0.5, 1.5, …).
        static_assert(G.Interval.divides_evenly(G.Notch), "grid: the notch must divide Upper − Lower evenly");

        return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be an inside<G,P> template argument.
    [[nodiscard]] static constexpr std::expected<grid, errc> try_make(interval iv, detail::grid_rational notch) {
        if (iv.Lower > iv.Upper)
            return std::unexpected{errc::domain_error};
        if (notch < 0)
            return std::unexpected{errc::domain_error};
        if (!iv.divides_evenly(notch))
            return std::unexpected{errc::rounding_error};
        return grid{iv, notch};
    }

    // Exact slot count (Upper − Lower)/Notch, however large: with Upper = a/b,
    // Lower = c/d and Notch = e/f it is (a·d − c·b)·f / (b·d·e), exact on a
    // valid grid. 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_wide slot_count() const noexcept {
        using detail::wide_numerator, detail::wide_denominator;
        if (Notch == 0)
            return detail::grid_wide{0};
        const auto&             U = Interval.Upper;
        const auto&             L = Interval.Lower;
        const detail::grid_wide num =
            (wide_numerator(U) * wide_denominator(L) - wide_numerator(L) * wide_denominator(U)) *
            wide_denominator(Notch);
        return num / (wide_denominator(U) * wide_denominator(L) * wide_numerator(Notch));
    }

    // Bits needed to hold every slot index 0..slot_count().
    [[nodiscard]] constexpr int slot_bits() const noexcept { return bit_width_of(slot_count()); }

    // The slot count as a umax; false (out = 0) when it needs more than 64
    // bits — such a grid stores a wide_int index.
    [[nodiscard]] constexpr bool max_index_checked(umax& out) const {
        const detail::grid_wide c    = slot_count();
        const bool              fits = !(detail::grid_wide{std::numeric_limits<umax>::max()} < c);
        out                          = fits ? static_cast<umax>(c) : umax{0};
        return fits;
    }

    // Index-storage slot count (0 on overflow: such grids store a wide index).
    [[nodiscard]] constexpr umax max_index() const {
        umax c = 0;
        (void)max_index_checked(c);
        return c;
    }

    // True when the slot count fits umax (a builtin index). False ⇒ the grid is
    // still valid and stores a wide index.
    [[nodiscard]] constexpr bool max_index_representable() const {
        umax c = 0;
        return max_index_checked(c);
    }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_ins`) regardless of whole-range mapping.
    [[nodiscard]] constexpr bool representable(detail::grid_rational v) const noexcept {
        if (!includes(Interval, v))
            return false;
        if (Notch == 0)
            return true;
#if BEMAN_INSIDE_BIG_GRIDS
        return detail::grid_divides_evenly(v - Interval.Lower, Notch);
#else
        auto diff = v - Interval.Lower; // expected<rational, errc>
        if (!diff)
            return false;
        auto off = diff.value() / Notch; // expected<rational, errc>
        return off.has_value() && detail::abs_den(off->Denominator) == 1;
#endif
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
#if BEMAN_INSIDE_BIG_GRIDS
        return detail::grid_gcd(Notch, Interval.Lower);
#else
        return detail::grid_gcd(Notch, Interval.Lower).value();
#endif
    }

    // operator== be default for structural type
    [[nodiscard]] constexpr bool operator==(const grid& rhs) const = default;
    [[nodiscard]] constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // whole-number grid above 0 has an unsigned raw that IS the value.)
};
} // namespace BEMAN_INSIDE_GRID_ABI

namespace detail {
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
} // namespace detail

// Raw of a point grid (Lower == Upper): its value lives in the type, so the
// raw is empty. It acts as index slot 0 — constructible from any index,
// converting to integer 0 — so the index-storage decode (Lower + raw·Notch)
// yields the point's value without special cases. Declared
// [[no_unique_address]] in inside, a point member of another struct (also
// marked [[no_unique_address]]) takes no space.
namespace detail {
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
} // namespace detail

// Both endpoints lie in imax — the signed value raw candidates (and every
// `trunc(endpoint)` constant) are only meaningful then.
namespace detail {
// Notch 1 from an integer Lower: the values are integers, so a raw can hold
// the value itself (value storage). {{0.5, 10.5}, 1} has notch 1 but not
// integer values.
constexpr bool unit_lattice(const grid& g) noexcept {
    return g.Notch == 1 && wide_denominator(g.Interval.Lower) == grid_wide{1};
}

constexpr bool fits_imax(const interval& iv) noexcept {
    return iv.Lower >= rational{std::numeric_limits<imax>::min()} &&
           iv.Upper <= rational{std::numeric_limits<imax>::max()};
}
} // namespace detail

// Storage is a function of the grid alone. Order: point → empty point_slot;
// notch zero → an exact fraction (no integer index space); more than 2^64
// slots → a wide_int index; a whole-number grid → its value where that costs
// no width (deduces_value); otherwise the unsigned 0-based index.
namespace detail {
// Unsigned index raw for G's slots: a builtin up to 64 bits, else wide.
template <grid G>
using index_raw_for_t = std::conditional_t<G.max_index_representable(),
                                           smallest_uint_for_t<G.max_index()>,
                                           int_for_bits_t<G.slot_bits(), false>>;

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
        return sizeof(smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>) <=
               sizeof(smallest_uint_for_t<G.max_index()>);
}();

// A continuous grid stores its value as an exact fraction: the 64-bit
// rational, or — for limits past 64 bits (C++26) — a reduced fraction of K-limb
// integers, K holding twice the limits' bits. A value that needs more reports
// overflow, as one past the 64-bit rational does.
#if BEMAN_INSIDE_BIG_GRIDS
template <grid G>
inline constexpr std::size_t frac_limbs = [] {
    auto bits = [](const grid_rational& r) {
        const grid_wide n = wide_numerator(r);
        return bit_width_of(n.negative() ? -n : n) + bit_width_of(wide_denominator(r));
    };
    const int b = bits(G.Interval.Lower) > bits(G.Interval.Upper) ? bits(G.Interval.Lower) : bits(G.Interval.Upper);
    return limbs_for_bits(2 * b + 2);
}();
template <grid G>
using continuous_raw_t = std::conditional_t<fits_rational(G.Interval.Lower) && fits_rational(G.Interval.Upper),
                                            detail::rational,
                                            exact_frac<frac_limbs<G>>>;
#else
template <grid G>
using continuous_raw_t = detail::rational;
#endif

template <grid G>
constexpr auto storage_min() {
    if constexpr (G.Interval.Lower == G.Interval.Upper)
        return point_slot{};
    else if constexpr (G.Notch == 0)
        return continuous_raw_t<G>{};
    else if constexpr (!deduces_value<G>)
        return index_raw_for_t<G>{};
    else if constexpr (G.Interval.Lower < 0)
        return smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>{};
    else if constexpr (G.Interval.Lower == 0)
        return smallest_uint_for_t<G.max_index()>{};
    else
        return smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>{};
}

// The raw type of inside<G, P> (the policy plays no part).
template <grid G>
using storage_min_t = decltype(storage_min<G>());

// Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
// on-grid value is a binary fraction (a double when it fits double_exact).
// A positive power of two, and its log2 (exact at any width).
constexpr bool is_pow2(const grid_wide& v) noexcept {
    return grid_wide{0} < v && v == (grid_wide{1} << (bit_width_of(v) - 1));
}

template <grid G>
inline constexpr bool dyadic_grid =
    G.Notch != 0 && is_pow2(wide_denominator(G.Notch)) && is_pow2(wide_denominator(G.Interval.Lower));

// Bits of |r · 2^f| — an integer on a dyadic grid, where r's denominator is
// a power of two dividing 2^f (0 for r == 0).
constexpr int scaled_numerator_bits(const grid_rational& r, int f) noexcept {
    const grid_wide n = wide_numerator(r);
    if (n == grid_wide{0})
        return 0;
    return bit_width_of(n.negative() ? -n : n) + f - (bit_width_of(wide_denominator(r)) - 1);
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

} // namespace detail

[[nodiscard]] constexpr std::expected<grid, errc> operator+(const grid&, const grid&);
[[nodiscard]] constexpr std::expected<grid, errc> operator-(const grid&, const grid&);
[[nodiscard]] constexpr std::expected<grid, errc> operator*(const grid&, const grid&);
[[nodiscard]] constexpr std::expected<grid, errc> operator/(const grid&, const grid&);

//---------------------------------------------------------------------------
// grid_sum_fits / grid_product_fits — whether a + b / a × b has a result
// grid. With 64-bit grid numbers a limit or notch can leave the rational
// range; these test it with the quiet try_ ops, so an arithmetic operator
// can static_assert with its own message before the result grid's loud
// rational error. Big grid numbers (C++26) always fit.
//---------------------------------------------------------------------------
namespace detail {
constexpr bool grid_sum_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    return try_add(a.Interval.Lower, b.Interval.Lower) && try_add(a.Interval.Upper, b.Interval.Upper) &&
           gcd(a.Notch, b.Notch);
#endif
}

// The notch of a × b. A product (La + i·Na)(Lb + j·Nb) differs from La·Lb
// by multiples of Na·Nb, Na·Lb and Nb·La; on an anchored operand its term is
// already a multiple of Na·Nb, so only an unanchored operand adds one. A
// point c (notch 0) scales the other lattice: the notch becomes N·|c|.
constexpr std::expected<grid_rational, errc> product_notch(const grid& a, const grid& b) {
    const bool          ap = a.Interval.Lower == a.Interval.Upper, bp = b.Interval.Lower == b.Interval.Upper;
    const grid_rational an  = (ap && !bp) ? abs(a.Interval.Lower) : a.Notch;
    const grid_rational bn  = (bp && !ap) ? abs(b.Interval.Lower) : b.Notch;
    auto                gcd = [](const grid_rational& x, const grid_rational& y) { return grid_gcd(x, y); };
    std::expected<grid_rational, errc> n = lift([](const grid_rational& x) { return x; }, an * bn);
    if (ap || bp)
        return n;
    if (!b.anchored())
        n = lift(gcd, n, a.Notch * b.Interval.Lower);
    if (!a.anchored())
        n = lift(gcd, n, b.Notch * a.Interval.Lower);
    return n;
}

constexpr bool grid_product_fits([[maybe_unused]] const grid& a, [[maybe_unused]] const grid& b) noexcept {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    return try_mul(a.Interval.Lower, b.Interval.Lower) && try_mul(a.Interval.Lower, b.Interval.Upper) &&
           try_mul(a.Interval.Upper, b.Interval.Lower) && try_mul(a.Interval.Upper, b.Interval.Upper) &&
           product_notch(a, b).has_value();
#endif
}
} // namespace detail

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator+(const grid& lhs, const grid& rhs) {
    // A continuous operand makes the sum continuous. (A point, also notch 0,
    // shifts the other lattice: gcd(0, n) = n is its notch.)
    auto continuous = [](const grid& g) { return g.Notch == 0 && g.Interval.Lower != g.Interval.Upper; };
    if (continuous(lhs) || continuous(rhs))
        return detail::lift([](interval i) { return grid{i, detail::grid_rational{0}}; }, lhs.Interval + rhs.Interval);
    // gcd returns expected — lift it so a notch-denominator overflow produces
    // errc::overflow rather than a silently wrapped result grid.
    return detail::lift([](interval i, detail::grid_rational n) { return grid{i, n}; },
                        lhs.Interval + rhs.Interval,
                        detail::grid_gcd(lhs.Notch, rhs.Notch));
}

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator-(const grid& lhs, const grid& rhs) {
    return operator+(lhs, -rhs);
}

//---------------------------------------------------------------------------
// operator*
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator*(const grid& lhs, const grid& rhs) {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous.
    return detail::lift([](interval i, detail::grid_rational n) { return grid{i, n}; },
                        lhs.Interval * rhs.Interval,
                        detail::product_notch(lhs, rhs));
}

//---------------------------------------------------------------------------
// operator/
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> operator/(const grid& lhs, const grid& rhs) {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
        return grid{*d, detail::grid_rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
        return std::unexpected{errc::division_by_zero};

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    detail::grid_rational step    = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : detail::grid_rational{1};
    bool                  has_pos = 0 < rhs.Interval.Upper;
    bool                  has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg) {
        return detail::lift(
            [](interval pos, interval neg) {
                return grid{interval{neg.Lower < pos.Lower ? neg.Lower : pos.Lower,
                                     neg.Upper < pos.Upper ? pos.Upper : neg.Upper},
                            detail::grid_rational{0}};
            },
            lhs.Interval / interval{step, rhs.Interval.Upper},
            lhs.Interval / interval{rhs.Interval.Lower, -step});
    } else if (has_pos) {
        return detail::lift([](interval i) { return grid{i, detail::grid_rational{0}}; },
                            lhs.Interval / interval{step, rhs.Interval.Upper});
    } else {
        return detail::lift([](interval i) { return grid{i, detail::grid_rational{0}}; },
                            lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
}

//---------------------------------------------------------------------------
// hull
//---------------------------------------------------------------------------
// The smallest grid that represents every value of both operands exactly:
// interval hull + notch gcd, refined by the offset between the two lattices
// when they do not line up ({{0, 2}, 1} and {{0.5, 1.5}, 1} hull to notch
// 1/2). A continuous operand (Notch 0) makes the hull continuous.
// errc::overflow when the notch's combined denominator exceeds the
// representable rational range.
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr std::expected<grid, errc> hull(const grid& lhs, const grid& rhs) {
    const interval iv{lhs.Interval.Lower < rhs.Interval.Lower ? lhs.Interval.Lower : rhs.Interval.Lower,
                      lhs.Interval.Upper < rhs.Interval.Upper ? rhs.Interval.Upper : lhs.Interval.Upper};
    // A continuous operand makes the hull continuous; a point (also notch 0)
    // joins the other lattice through the offset between them.
    auto continuous = [](const grid& g) { return g.Notch == 0 && g.Interval.Lower != g.Interval.Upper; };
    if (continuous(lhs) || continuous(rhs))
        return grid{iv, detail::grid_rational{0}};
    auto gcd = [](const detail::grid_rational& x, const detail::grid_rational& y) { return detail::grid_gcd(x, y); };
    std::expected<detail::grid_rational, errc> n = detail::lift(gcd, lhs.Notch, rhs.Notch);
    if (n && (*n == 0 || !detail::grid_same_lattice(lhs.Interval.Lower, rhs.Interval.Lower, *n)))
        n = detail::lift(gcd, n, rhs.Interval.Lower - lhs.Interval.Lower);
    return detail::lift([iv](detail::grid_rational g) { return grid{iv, g}; }, n);
}
} // namespace beman::inside



//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`grid_of<B>`, `policy_of<B>`, `notch64<B>`,
// `interval_of<B>`) plus the `insidable`/`numeric`/`inside_assignable` concepts; the
// storage-shape predicates and raw/value converters are internal (`beman::inside::detail`).
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
    static constexpr grid        grid_v   = G;
    static constexpr policy_flag policy_v = P;
};
} // namespace detail

template <insidable B>
inline constexpr grid grid_of = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

template <insidable B>
inline constexpr policy_flag policy_of = detail::inside_params<std::remove_cvref_t<B>>::policy_v;

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
// the build here, never truncates. Under C++23 they equal lower_of & co.
template <insidable B>
inline constexpr rational lower64 = to_rational(grid_of<B>.Interval.Lower);
template <insidable B>
inline constexpr rational upper64 = to_rational(grid_of<B>.Interval.Upper);
template <insidable B>
inline constexpr rational notch64 = to_rational(grid_of<B>.Notch);
} // namespace detail

template <typename N>
concept numeric = insidable<N> or detail::arithmetic<N>;

//---------------------------------------------------------------------------
// Internal plumbing — storage shape, raw/value conversion, dispatch.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
using plain_t = std::remove_cvref_t<T>;

// Always-false but template-dependent: lets a `static_assert` inside a
// template body fire only when that template is actually instantiated
// (e.g. the guidance overloads that make `inside + 1` ill-formed).
template <typename...>
inline constexpr bool dependent_false = false;

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
    if constexpr (arithmetic<N>)
        return rational{v};
    else
        return v;
}

// Canonical-zero test for a divisor. rational stores zero as {0, 1}, so
// Numerator == 0 catches it regardless of representation; other types compare
// against their own zero.
template <typename T>
[[nodiscard]] constexpr bool is_canonical_zero(const T& v) {
    if constexpr (std::same_as<T, rational>)
        return v.Numerator == 0;
    else
        return v == T{0};
}

template <insidable B>
using raw_t = typename B::raw_type;

//-------------------------------------------------------------------------
// Storage — how an inside's value lives in its raw, a function of the grid
// alone (grid.hpp storage_min). The leaves partition every
// inside; each names the raw type and what it means:
//
//   value_storage — the raw IS the value
//     rational_storage                         a 64-bit rational
//     fraction_storage                         an exact_frac (continuous, limits past 64 bits)
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

// A continuous grid whose limits pass 64 bits (C++26): a reduced exact
// fraction of as many limbs as the limits need.
template <typename B>
concept fraction_storage = insidable<B> && is_exact_frac_v<raw_t<B>>;

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
concept wide_index_storage = insidable<B> && is_wide_int_v<raw_t<B>>;

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
//                 (wide_grid_numbers, never under C++23). They take the
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

template <insidable B>
inline constexpr bool wide_valued = wide_index_storage<B> || wide_grid_numbers<B>;

// Ungated double view of any inside (the public operator double() is gated
// on a rounding flag; this is always available). Everything but index storage holds the value verbatim; an
// index decodes through the grid.
template <insidable B>
constexpr auto exact_of(const B& b); // wide_value.hpp

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
        const auto     lo = lower64<B> / n, hi = upper64<B> / n;
        if (!lo || !hi || abs_den(n.Denominator) > k53)
            return index_double_codec{};
        const umax m = lo->Numerator > hi->Numerator ? lo->Numerator : hi->Numerator; // max |J|
        if (m > k53 / n.Numerator)
            return index_double_codec{};
        return index_double_codec{
            true, signed_numerator(*lo), static_cast<imax>(n.Numerator), static_cast<double>(abs_den(n.Denominator))};
    }
}();

template <insidable B>
[[nodiscard]] constexpr double as_double(const B& b) noexcept {
    if constexpr (wide_valued<B>)
        return static_cast<double>(exact_of(b));
    else if constexpr (point_storage<B>)
        return static_cast<double>(detail::lower64<B>);
    else if constexpr (value_storage<B>)
        return static_cast<double>(b.raw());
    else if constexpr (constexpr auto c = index_double<B>; c.Ok)
        return static_cast<double>((static_cast<imax>(b.raw()) + c.LowerIndex) * c.P) / c.Q;
    else
        return static_cast<double>((*(b.raw() * detail::notch64<B>)+detail::lower64<B>).value());
}

// True when R's interval cannot contain zero — so `a / b` can return a plain
// `inside` instead of `expected<inside, errc>` (see detail/division.hpp). A point
// grid at 0 is *not* excluded.
template <insidable R>
inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

// Storage-agnostic int truncation of interval endpoints — intent-revealing
// `static_cast<imax>(detail::lower64<B>)`. Used by from_value, raw_lo, the fast paths.
template <insidable B>
inline constexpr imax lower_imax = trunc(detail::lower64<B>);

template <insidable B>
inline constexpr imax upper_imax = trunc(detail::upper64<B>);

// Slot count via grid::max_index (overflow-safe: 0 when it doesn't fit umax,
// for grids with a wide index, which take the exact wide paths).
template <insidable B>
inline constexpr umax max_index_v = grid_of<B>.max_index();

// Every value — and, for index storage, every slot — fits imax. Gates the
// integer fast paths that work in imax (raw_imax / to_value / raw_lo /
// raw_hi); a grid reaching past int64 (e.g. {0, 2^64−1} in a uint64) takes
// the exact rational / umax paths instead.
template <insidable B>
inline constexpr bool values_fit_imax =
    !wide_valued<B> && fits_imax(interval_of<B>) &&
    (value_storage<B> || max_index_v<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

// Notch a non-zero integer and Lower an integer — every value is an
// integer ({{0.5, 10.5}, 1} has an integer notch but not integer values).
// Gates the implicit imax/size_t conversions.
template <grid G>
inline constexpr bool integer_notch =
    wide_denominator(G.Notch) == grid_wide{1} && G.Notch != 0 && wide_denominator(G.Interval.Lower) == grid_wide{1};

// ONLY type conversion, NO value representation conversion calculation
template <insidable B>
[[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept {
    return static_cast<raw_t<B>>(value);
}

template <insidable B>
[[nodiscard]] constexpr raw_t<B> raw_cast(rational value) noexcept {
    if constexpr (rational_storage<B>)
        return value;
    else
        return value.to<raw_t<B>>().value_or(0);
}

// Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
// grids where raw is an index rather than a value — naming separates the
// two intents that today both spell `static_cast<imax>`.
template <insidable B>
    requires(!wide_index_storage<B>) // a wide raw does not fit imax: use the exact wide path
constexpr imax raw_imax(B b) noexcept {
    return static_cast<imax>(b.raw());
}

//-------------------------------------------------------------------------
// Q-format integer fast path: for grids with integer Lower, unit-numerator
// Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
// by operator rational(), from_value, and assignment::store.
//-------------------------------------------------------------------------
template <insidable B>
inline constexpr bool qformat_codec_fits =
    abs_den(detail::lower64<B>.Denominator) == 1 && detail::notch64<B>.Numerator == 1 &&
    values_fit_imax<B>; // Lower·nd and the raw both in imax

// value → raw, integer math only.
template <insidable B>
    requires qformat_codec_fits<B>
constexpr raw_t<B> q_format_encode(imax value) noexcept {
    constexpr imax nd = abs_den(detail::notch64<B>.Denominator);
    return raw_cast<B>((value - lower_imax<B>)*nd);
}

// raw → rational, integer math only.
template <insidable B>
    requires qformat_codec_fits<B>
constexpr rational q_format_decode(B b) noexcept {
    constexpr imax nd = abs_den(detail::notch64<B>.Denominator);
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
    else if constexpr (abs_den(detail::notch64<B>.Denominator) == 1 && abs_den(detail::lower64<B>.Denominator) == 1)
        return lower_imax<B> + raw_imax(b) * static_cast<imax>(detail::notch64<B>.Numerator);
    else if constexpr (qformat_codec_fits<B>) {
        constexpr imax nd = abs_den(detail::notch64<B>.Denominator);
        return (raw_imax(b) + lower_imax<B> * nd) / nd; // q_format_decode, truncated
    } else                                              // index storage, generic rational path
        return trunc(as_rational(b));
}

template <insidable B>
constexpr void from_value(B& b, imax val) {
    if constexpr (value_storage<B>)
        b = B::from_raw(raw_cast<B>(val));
    else if constexpr (abs_den(detail::notch64<B>.Denominator) == 1 && abs_den(detail::lower64<B>.Denominator) == 1)
        b = B::from_raw(raw_cast<B>((val - lower_imax<B>) / static_cast<imax>(detail::notch64<B>.Numerator)));
    else if constexpr (qformat_codec_fits<B>)
        b = B::from_raw(q_format_encode<B>(val));
    else // index storage, generic rational path
    {
        auto offset = (rational{val} - detail::lower64<B>) / detail::notch64<B>;
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
inline constexpr imax raw_hi = integer_value_storage<B> ? upper_imax<B> : static_cast<imax>(max_index_v<B>);

// The exact raw range: 0 .. slot count for index storage, Lower .. Upper
// for value storage (integers there). Sizes the work types below.
template <insidable B>
inline constexpr grid_wide raw_lo_exact = index_storage<B> ? grid_wide{0} : wide_numerator(lower_of<B>);
template <insidable B>
inline constexpr grid_wide raw_hi_exact = index_storage<B> ? grid_of<B>.slot_count() : wide_numerator(upper_of<B>);

// Value bits a signed integer needs to hold every value in [lo, hi].
constexpr int signed_value_bits(const grid_wide& lo, const grid_wide& hi) noexcept {
    auto      mag = [](const grid_wide& v) { return bit_width_of(v.negative() ? -(v + grid_wide{1}) : v); };
    const int a = mag(lo), b = mag(hi);
    return a > b ? a : b;
}

// Value bits for every value in a list (their min .. max).
constexpr int signed_value_bits_of(std::initializer_list<grid_wide> vals) noexcept {
    grid_wide mn = *vals.begin(), mx = *vals.begin();
    for (const grid_wide& v : vals) {
        if (v < mn)
            mn = v;
        if (mx < v)
            mx = v;
    }
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
template <typename W>
[[nodiscard]] constexpr floored<W> floor_divmod(const W& t, const W& m) noexcept {
    floored<W> f;
    if constexpr (is_wide_int_v<W>) {
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

// The raw of L's Lower (low) or Upper endpoint: the exact constant for a
// rational raw, slot 0 or max_index_v for an integer one.
template <insidable L>
constexpr raw_t<L> endpoint_raw(bool low) noexcept {
    if constexpr (rational_storage<L>)
        return low ? detail::lower64<L> : detail::upper64<L>;
    else
        return raw_from_offset<L>(low ? umax{0} : max_index_v<L>);
}

// The raw of a value v on L's lattice within [Lower, Upper], through its
// exact offset (v − Lower)/Notch.
template <insidable L>
constexpr raw_t<L> raw_of_lattice_value(rational v) {
    return raw_from_offset<L>(((v - detail::lower64<L>).value() / detail::notch64<L>).value().Numerator);
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
    wide_denominator(lower_of<B>) == grid_wide{1} && wide_denominator(upper_of<B>) == grid_wide{1};

template <insidable B>
inline constexpr bool integer_lattice =
    integer_notch<grid_of<B>> || (lower_of<B> == upper_of<B> && wide_denominator(lower_of<B>) == grid_wide{1});

// Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
// unit numerator with integer denominator > 1, Lower is an integer at 0.
// Value = Raw / Notch.Denominator. Used to gate the integer fast path for
// fixed-point division, which would otherwise fall into the slow rational
// route because Notch.Denominator > 1 disqualifies integer_lattice.
template <insidable B>
inline constexpr bool qformat_grid =
    wide_numerator(notch_of<B>) == grid_wide{1} && wide_denominator(notch_of<B>) > grid_wide{1} && lower_of<B> == 0;

// Policy test: checks both type-level and per-operation policy.
// Composite flags (e.g. round_nearest = bit5 | snap) require all
// their bits set — having a subset like just `snap` does NOT match.
template <insidable B, typename P, policy_flag F>
inline constexpr bool has_policy = has_flag(policy_of<B>, F) || plain_t<P>::test(F);

// rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
template <insidable L, typename P>
inline constexpr round_mode rounding_for = has_policy<L, P, round_floor>       ? round_mode::floor
                                           : has_policy<L, P, round_ceil>      ? round_mode::ceil
                                           : has_policy<L, P, round_half_even> ? round_mode::half_even
                                           : has_policy<L, P, round_nearest>   ? round_mode::nearest
                                                                               : round_mode::trunc;

// Whether the lattice index of Lower, ⌊Lower/Notch⌋ (= Lower/Notch when
// anchored), is odd: with an offset's parity it says which lattice points are
// even. Exact past imax.
template <insidable L>
inline constexpr bool lower_index_odd =
    detail::notched<L> &&
    (round_to_integral((detail::lower64<L> / detail::notch64<L>).value(), round_mode::floor).Numerator & 1) != 0;

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
// space, ties half away from zero). Pre: the result fits the rational range
// (a v within one notch of L's limits); try_round_to_lattice checks it.
template <insidable L, typename P>
[[nodiscard]] constexpr rational round_to_lattice(rational v) {
    if constexpr (!detail::notched<L>)
        return v;
    else if constexpr (anchored<L>) {
        return (lattice_step_of<L, P>((v / detail::notch64<L>).value(), v < 0).Base * detail::notch64<L>).value();
    } else {
        const auto [k, up] =
            lattice_step_of<L, P>(((v - detail::lower64<L>).value() / detail::notch64<L>).value(), v < 0);
        const rational j = up ? (k + rational{1}).value() : k;
        return (detail::lower64<L> + (j * detail::notch64<L>).value()).value();
    }
}

// The same for any v — not limited to [Lower, Upper], so wrap can round
// first and fold an on-lattice value after: a result past the rational
// range is errc::overflow. (Separate from round_to_lattice: an expected
// result on that hot store path cost instructions.)
template <insidable L, typename P>
[[nodiscard]] constexpr std::expected<rational, errc> try_round_to_lattice(rational v) {
    if constexpr (!detail::notched<L>)
        return v;
    else {
        const auto q =
            anchored<L> ? try_div(v, detail::notch64<L>) : try_sub(v, detail::lower64<L>).and_then([](rational off) {
                return try_div(off, detail::notch64<L>);
            });
        if (!q)
            return q;
        const auto [base, step] = lattice_step_of<L, P>(*q, v < 0);
        const auto j            = step ? try_add(base, rational{1}) : std::expected<rational, errc>{base};
        if (!j)
            return j;
        const auto offset = try_mul(*j, detail::notch64<L>);
        return (offset && !anchored<L>) ? try_add(detail::lower64<L>, *offset) : offset;
    }
}

// Round, then range-check. Lower and Upper are lattice points, so rounding
// an in-range value keeps it in range; only an out-of-range value can change
// outcome. When the policy may round (snap), rounds_into_range rounds such a
// value and reports whether it lands inside the interval (`out` = the
// rounded value). Only values within one notch of the interval can, which
// also keeps round_to_lattice's division bounded for huge sources.
template <insidable L, typename P>
inline constexpr bool rounds_before_range_check = detail::notched<L> && has_policy<L, P, snap>;

template <insidable L, typename P>
[[nodiscard]] constexpr bool rounds_into_range(rational v, rational& out) {
    if constexpr (!rounds_before_range_check<L, P>)
        return false;
    else {
        constexpr rational lo = (detail::lower64<L> - detail::notch64<L>).value_or(detail::lower64<L>);
        constexpr rational hi = (detail::upper64<L> + detail::notch64<L>).value_or(detail::upper64<L>);
        if (v <= lo || v >= hi)
            return false;
        out = round_to_lattice<L, P>(v); // within one notch of the limits: fits
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
[[gnu::cold, gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v) {
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
    if constexpr (detail::lower64<L> < 0) {
        constexpr rational c = (-detail::lower64<L> / detail::notch64<L>).value(); // > 0
        using W              = wide_uint<2>;
        negative             = W{num} * W{abs_den(c.Denominator)} < W{c.Numerator} * W{den};
    }
    return q + rounds_up(m, negative, classify_remainder(m, r, den), ((q & 1) != 0) != lower_index_odd<L>);
}

// Round the non-negative offset quotient num/den (den >= 1) to an integer
// notch index per L's rounding policy.
//
// Tie/sign rules are in VALUE space, not offset space, so assigning a value
// rounds it the same way dividing down to it does (detail::div_rounded is the
// reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
// we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
// it like div_rounded, and return the offset J - m. m is integral on every
// anchored grid; an unanchored one rounds the offset against the value's sign.
template <insidable L, typename P>
[[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept {
    constexpr rational zl =
        (!detail::notched<L>) ? rational{0} : (detail::lower64<L> / detail::notch64<L>).value_or(rational{0});
    constexpr bool vidx = (zl.Denominator == 1 || zl.Denominator == -1);
    constexpr imax m    = vidx ? signed_numerator(zl) : imax{0};

    if constexpr (!anchored<L>)
        return round_offset_unanchored<L, P>(num, den);
    else if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
    else {
        // Round the signed value-index NUM/di exactly like detail::div_rounded.
        // A numerator or m·di beyond imax (fine-denominator sources on grids
        // with large |Lower·count|) cannot rebuild the signed index: round the
        // offset's floor q instead, in value space — the value is below zero
        // exactly when the floor's value index m + q is.
        const imax di = static_cast<imax>(den);
        imax       mdi, NUM;
        if (num > static_cast<umax>(std::numeric_limits<imax>::max()) || mul_overflow(m, di, &mdi) ||
            add_overflow(mdi, static_cast<imax>(num), &NUM)) [[unlikely]] {
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

// The two non-trivial clauses of `inside_assignable`, named so the concept and
// its `inside_assignable_why` diagnostic share one definition. Concepts (not
// bools) so `||` short-circuits *instantiation* (e.g. assignment<L,R>::Factor
// is never formed when R isn't insidable).
template <typename L, typename R, policy_flag P = checked>
concept assign_intervals_ok =
    (!insidable<R> && !std::integral<R>)
    // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
    // for them (the integral-rhs path already allows it — int's interval is unbounded).
    || ((policy_of<L> | P) & (wrap | clamp)) != 0 || not excludes(interval_of<L>, interval_of<R>);

template <typename L, typename R, policy_flag P>
concept assign_notch_ok = !insidable<R> || ((policy_of<L> | P) & snap) != 0 || notches_compatible<L, R>;
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
// `BEMAN_INSIDE_STRICT_SFINAE`) and the public `why_assignable` probe below.
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
    static_assert(intervals_ok,
                  "inside_assignable: rhs interval lies entirely outside lhs interval and the policy "
                  "(not wrap/clamp) cannot bring it into range — assignment can never succeed");
    static_assert(notch_ok,
                  "inside_assignable: incompatible notches — use `with_snap()` or `policy<snap>()` to allow rounding");
    static constexpr bool value = inside_assignable<L, R, P>;
};

// Public manual probe: `static_assert(beman::inside::why_assignable<DstInside, decltype(src)>);`
// emits the named per-clause reasons in any build — including a strict
// (`BEMAN_INSIDE_STRICT_SFINAE`) build where the automatic in-`inside` fallbacks are absent.
template <typename Dst, typename Src, policy_flag P = policy_of<Dst>>
inline constexpr bool why_assignable = inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
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
    else if constexpr (wide_valued<B> && anchored<B>) {
        const I j = static_cast<I>(slot_base<B>) + I{b.raw()};
        return exact_frac<K>{j * static_cast<I>(wide_numerator(notch_of<B>)),
                             static_cast<I>(wide_denominator(notch_of<B>))};
    } else if constexpr (wide_valued<B>) // Lower + raw·Notch over their common denominator
        return exact_of_grid<K>(lower_of<B>) + exact_frac<K>{I{b.raw()} * static_cast<I>(wide_numerator(notch_of<B>)),
                                                             static_cast<I>(wide_denominator(notch_of<B>))};
    else
        return exact_of<K>(as_rational(b));
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


// ======================================================================
//  beman/inside/policy.hpp
// ======================================================================


// ======================================================================
//  beman/inside/detail/assignment.hpp
// ======================================================================



namespace beman::inside::detail {
//---------------------------------------------------------------------------
// assignment — narrowing/coercion between bounded and arithmetic types. Three
// specialisations dispatch on the source (integral / fractional / insidable),
// each routing through `store` (in-range) and `handle_out_of_range` /
// `apply_clamp` / `apply_wrap` (policy). The insidable path also exposes
// `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
//---------------------------------------------------------------------------
// needs_runtime_range_check<L, P, A>: true iff any out-of-range handler would
// fire (an action, a clamp/wrap bit, or default-throw under checked).
// When false (typically `unsafe`, no action) the runtime range branch in
// `assign` is dead code and skipped, letting the autovectorizer kick in.
//---------------------------------------------------------------------------
template <insidable L, typename P, typename A>
inline constexpr bool needs_runtime_range_check =
    clamp_action<plain_t<A>> || wrap_action<plain_t<A>> || error_action<plain_t<A>> ||
    range_handled(policy_of<L> | plain_t<P>::Flags);

// Shared out-of-range policy cascade. Order: clamp/wrap/error *actions*, then
// clamp/wrap *policy* bits, then `range_fail`. The callers say how clamp and
// wrap store; `Wrappable` is false on the fractional path (no wrap *action*
// branch). Returns true when a handler resolved the write.
template <bool Wrappable, insidable L, typename P, typename A, typename DoClamp, typename DoWrap>
constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action, DoClamp do_clamp, DoWrap do_wrap) {
    using PA = plain_t<A>;
    if constexpr (clamp_action<PA>) {
        do_clamp();
        return true;
    } else if constexpr (Wrappable && wrap_action<PA>) {
        do_wrap();
        return true;
    } else if constexpr (error_action<PA>) {
        action.Fn(lhs, errc::overflow, errc_message(errc::overflow));
        return true;
    } else if constexpr (has_policy<L, P, clamp>) {
        do_clamp();
        return true;
    } else if constexpr (has_policy<L, P, wrap>) {
        do_wrap();
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
    constexpr imax kMin = std::numeric_limits<imax>::min();
    constexpr imax kMax = std::numeric_limits<imax>::max();
    // A range past the rational range (a grid spanning 2^64 or more) has
    // carries in {−1, 0, 1} at most: the full imax grid holds them.
    constexpr auto span_r = try_sub(detail::upper64<L>, detail::lower64<L>);
    if constexpr (!span_r || !try_add(*span_r, detail::notch64<L>))
        return grid{kMin, kMax};
    else if constexpr (std::integral<R> || insidable<R>) {
        // An integral source spans its type's limits, an inside its interval.
        constexpr rational src_lo = [] {
            if constexpr (insidable<R>)
                return detail::lower64<R>;
            else
                return rational{std::numeric_limits<R>::min()};
        }();
        constexpr rational src_hi = [] {
            if constexpr (insidable<R>)
                return detail::upper64<R>;
            else
                return rational{std::numeric_limits<R>::max()};
        }();
        const rational range    = *try_add(*span_r, detail::notch64<L>);
        auto           carry_of = [&](rational v, imax fallback) -> imax {
            const auto off = try_sub(v, detail::lower64<L>);
            if (!off)
                return fallback;
            const auto q = try_div(*off, range);
            if (!q || *q < rational{kMin} || *q > rational{kMax})
                return fallback;
            return floor(*q);
        };
        return grid{carry_of(src_lo, kMin), carry_of(src_hi, kMax)};
    } else
        return grid{kMin, kMax};
}

template <insidable L, typename R>
constexpr auto make_wrap_carry(imax q) {
    beman::inside::inside<wrap_carry_grid<L, R>()> carry;
    from_value(carry, q); // in the carry grid by construction
    return carry;
}

//---------------------------------------------------------------------------
// unit_fold — clamp/wrap of an integer value v in [SrcLo, SrcHi] onto a
// unit-notch integer grid L, exactly. v − Lower and the range
// Upper − Lower + 1 may need 65 bits (a grid can span 2^64 values), so the
// arithmetic runs in a work type sized from all of them: imax when that
// suffices, else a wide_int.
//---------------------------------------------------------------------------
template <insidable L, grid_wide SrcLo, grid_wide SrcHi>
struct unit_fold {
    static constexpr grid_wide lo = wide_numerator(lower_of<L>);
    static constexpr grid_wide hi = wide_numerator(upper_of<L>);
    using W = work_int_t<signed_value_bits_of({SrcLo, SrcHi, lo, hi, SrcLo - hi, SrcHi - lo, hi - lo + grid_wide{1}})>;

    static constexpr W lower = static_cast<W>(lo);
    static constexpr W upper = static_cast<W>(hi);
    static constexpr W span  = static_cast<W>(hi - lo);

    // d saturated to imax (a clamp overshoot or a wrap carry).
    static constexpr imax saturate(const W& d) noexcept {
        constexpr W kMin = static_cast<W>(std::numeric_limits<imax>::min());
        constexpr W kMax = static_cast<W>(std::numeric_limits<imax>::max());
        return static_cast<imax>(d < kMin ? kMin : kMax < d ? kMax : d);
    }

    // v = Lower + carry·(span + 1) + offset with 0 ≤ offset ≤ span.
    struct folded {
        imax Carry;
        W    Offset;
    };
    static constexpr folded fold(const W& v) noexcept {
        const auto [q, w] = floor_divmod(v - lower, span + W{1});
        return {saturate(q), w};
    }
};

// The integer range of a source: its type's limits, or ±2^64 (every
// integer a 64-bit rational or a grid value can be).
template <typename R>
inline constexpr grid_wide source_lo = [] {
    if constexpr (std::integral<R>)
        return grid_wide{std::numeric_limits<R>::min()};
    else
        return -(grid_wide{1} << 64);
}();
template <typename R>
inline constexpr grid_wide source_hi = [] {
    if constexpr (std::integral<R>)
        return grid_wide{std::numeric_limits<R>::max()};
    else
        return grid_wide{1} << 64;
}();

//---------------------------------------------------------------------------
// assignment
//---------------------------------------------------------------------------
template <typename L, typename R>
struct assignment;

//---------------------------------------------------------------------------
// assign_exact — store an exact value into L. The path for a wide raw (more
// than 2^64 slots) on either side of an assignment, where neither imax nor
// the 64-bit rational holds every value. Rounds first, then range-checks,
// like the builtin paths; out of range runs the usual policy cascade.
//---------------------------------------------------------------------------
// R is the source type: an on_clamp action gets the overshoot and an
// on_wrap action the carry in the same shape as on the builtin paths.
template <typename R, insidable L, typename P, typename A, std::size_t K>
constexpr L& assign_exact(L& lhs, const exact_frac<K>& v, P&& policy, A&& action) {
    auto fail = [&](errc code) { report_failure(lhs, policy, action, code); };
    if constexpr (fraction_storage<L>) {
        // A continuous grid past 64 bits: the value itself, reduced, when it
        // lies within the limits and fits the raw's limbs.
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(dependent_false<A>, "on_clamp / on_wrap: not supported on a continuous grid past 64 bits");
        constexpr std::size_t KK = exact_max<K, exact_limbs<L>>;
        const exact_frac<KK>  lo = exact_of_grid<KK>(lower_of<L>), hi = exact_of_grid<KK>(upper_of<L>);
        auto                  store = [&](const exact_frac<KK>& x) {
            if (const auto r = frac_raw_of<raw_t<L>>(x)) [[likely]]
                lhs = L::from_raw(*r);
            else
                fail(errc::overflow);
        };
        const exact_frac<KK> x{v};
        if (x < lo || hi < x) [[unlikely]] {
            dispatch_out_of_range<true>(
                lhs,
                policy,
                action,
                [&] { store(x < lo ? lo : hi); },
                [&] {
                    // x − q·span with q = ⌊(x − lo)/span⌋: into [lo, hi).
                    const exact_frac<KK> span = hi + -lo, t = (x + -lo) / span;
                    const auto           q = floor_divmod(t.Num, t.Den).Quot;
                    store(x + -(exact_frac<KK>{q, wide_sint<KK>{1}} * span));
                });
            return lhs;
        }
        store(x);
        return lhs;
    } else if constexpr (rational_storage<L>) {
        // L holds 64-bit values: narrow through the rational (a value that does
        // not fit lies outside every such grid).
        const auto r = try_rational(v);
        if (!r) [[unlikely]] {
            fail(errc::overflow);
            return lhs;
        }
        if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
            static_assert(
                dependent_false<A>,
                "on_clamp / on_wrap: a source past the 64-bit rational into a rational inside is not supported");
        return assignment<L, rational>::assign(lhs, *r, policy, std::forward<A>(action));
    } else {
        // (Plain variables, not a structured binding: Clang rejects a binding
        // captured by the lambdas below in constant evaluation.)
        const auto slot             = exact_index<L, rounding_for<L, plain_t<P>>>(v);
        using I                     = decltype(slot.Index);
        constexpr std::size_t KK    = sizeof(I) / sizeof(umax);
        const I               index = slot.Index;
        if (!slot.Exact && !has_policy<L, P, snap> && policy.round_check()) [[unlikely]] {
            fail(errc::rounding_error);
            return lhs;
        }
        constexpr I count = static_cast<I>(grid_of<L>.slot_count());
        if (index.negative() || index > count) [[unlikely]] {
            auto saturate = [](const I& d) -> imax {
                constexpr imax kMin = std::numeric_limits<imax>::min(), kMax = std::numeric_limits<imax>::max();
                return d < I{kMin} ? kMin : I{kMax} < d ? kMax : static_cast<imax>(d);
            };
            if (dispatch_out_of_range<true>(
                    lhs,
                    policy,
                    action,
                    [&] {
                        const bool low = index.negative();
                        lhs            = L::from_raw(raw_of_slot<L>(low ? I{0} : count));
                        if constexpr (clamp_action<plain_t<A>>) {
                            // The overshoot rhs − bound, shaped like the builtin paths'.
                            const auto over =
                                v + exact_frac<KK>{I{-1}, I{1}} * exact_of_grid<KK>(low ? lower_of<L> : upper_of<L>);
                            if constexpr (insidable<R>)
                                action.Fn(
                                    lhs, exact_result<beman::inside::inside<(grid_of<R> - grid_of<L>).value()>>(over));
                            else if constexpr (std::integral<R>)
                                action.Fn(lhs, saturate(trunc(over)));
                            else if constexpr (std::floating_point<R>)
                                action.Fn(lhs, static_cast<R>(static_cast<double>(over)));
                            else
                                action.Fn(lhs, try_rational(over).value_or(rational{0}));
                        }
                    },
                    [&] {
                        const auto [q, w] = floor_divmod(index, count + I{1});
                        lhs               = L::from_raw(raw_of_slot<L>(w));
                        if constexpr (wrap_action<plain_t<A>>)
                            action.Fn(lhs, make_wrap_carry<L, R>(saturate(q)));
                    }))
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
    notched<L> && notch_of<L> == notch_of<R> && !point_storage<L> && !point_storage<R>; // ⇒ integer raws
// The shift is the exact offset between the Lowers in notches, plus the raws
// of the Lowers (0 for an index raw, Lower for a value raw): exact on
// unanchored lattices too.
template <insidable L, insidable R>
inline constexpr grid_wide same_notch_shift =
    exact_quotient(grid_sub(lower_of<R>, lower_of<L>), notch_of<L>) + raw_lo_exact<L> - raw_lo_exact<R>;
template <insidable L, insidable R>
inline constexpr int same_notch_bits = signed_value_bits_of({raw_lo_exact<R> + same_notch_shift<L, R>,
                                                             raw_hi_exact<R> + same_notch_shift<L, R>,
                                                             raw_lo_exact<L>,
                                                             raw_hi_exact<L>});

//---------------------------------------------------------------------------
// assign(insidable, integral)
//---------------------------------------------------------------------------
template <insidable L, std::integral R>
struct assignment<L, R> {
  private:
    // A 64-bit unsigned source can exceed imax: `static_cast<imax>(rhs)` would
    // turn 2^64−1 into −1. Compare such a source in umax instead; every
    // narrower or signed source is exact in imax and keeps the plain cast.
    static constexpr bool wide_unsigned = std::is_unsigned_v<R> && sizeof(R) >= sizeof(imax);

    // Exact integer arithmetic for the bounds and the cold clamp/wrap paths:
    // an integer interval may reach past int64 on either side (e.g.
    // {0, 2^64−1}), and rhs − Lower may need 65 bits.
    using fold = unit_fold<L, source_lo<R>, source_hi<R>>;
    using W    = typename fold::W;

    // Hot-path range test. Bounds within imax compare as plain integers;
    // bounds past int64 compare exactly in the fold's work type.
    static constexpr bool out_of_range(R rhs) noexcept {
        if constexpr (fits_imax(interval_of<L>)) {
            constexpr imax lower = lower_imax<L>, upper = upper_imax<L>;
            if constexpr (wide_unsigned)
                return (lower > 0 && static_cast<umax>(rhs) < static_cast<umax>(lower)) || upper < 0 ||
                       static_cast<umax>(rhs) > static_cast<umax>(upper);
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
            action.Fn(lhs, fold::saturate(v - (low ? fold::lower : fold::upper)));
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
        else if constexpr (detail::point_grid<L>)
            lhs = L::from_raw(0); // notch_storage point grid: 0 is the only offset
        else if constexpr (qformat_codec_fits<L>)
            lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage on a notch 1/K grid: the offset is an exact integer
        {
            rational raw = ((rhs - detail::lower64<L>) / detail::notch64<L>).value();
            lhs          = L::from_raw(raw_cast<L>(raw.Numerator));
        }
    }

  public:
    // An integer lands on L's grid whenever the notch is 1/K over an integer
    // Lower (or the grid is continuous); otherwise it may fall between notches
    // and must round or report exactly like the same value given as a rational.
    static constexpr bool integers_on_grid =
        !detail::notched<L> || (detail::notch64<L>.Numerator == 1 && abs_den(detail::lower64<L>.Denominator) == 1);

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
                            if constexpr (detail::notch64<L> == 1) {
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
        const rational n = detail::notch64<L>, lo = detail::lower64<L>, hi = detail::upper64<L>;
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
            return !(rhs < 0x1p64 && rhs > -0x1p64);
        else
            return false;
    }

    static constexpr bool below_lower(const R& rhs) { return huge(rhs) ? rhs < 0 : rhs < detail::lower64<L>; }

    template <typename P, typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action) {
        const bool low     = below_lower(rhs);
        R          clamped = low ? static_cast<R>(detail::lower64<L>) : static_cast<R>(detail::upper64<L>);
        R          overshoot;
        if constexpr (std::same_as<R, rational>)
            overshoot = (rhs - clamped).value_or(rational{0});
        else
            overshoot = rhs - clamped;

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
    // apply_wrap for a fractional R — modular reduction into [Lower, Lower + range)
    // followed by store_checked so the rounding policy still applies if rhs
    // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action) {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        // The fold's quotient q is an imax: a floating source of 2^63 or more
        // cannot be wrapped with a deliverable carry (nor converted exactly).
        if constexpr (std::floating_point<R>)
            if (!(rhs < 0x1p63 && rhs > -0x1p63)) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
        rational rhs_r{rhs};
        if constexpr (has_policy<L, P, snap>) {
            const auto r = try_round_to_lattice<L, P>(rhs_r);
            if (!r) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
            rhs_r = *r;
        }
        imax     q;
        rational wrapped;
        if (abs_den(rhs_r.Denominator) == 1 && detail::notch64<L> == 1 &&
            abs_den(detail::lower64<L>.Denominator) == 1) {
            // Integer value on a unit lattice: fold exactly (the grid may span
            // 2^64 values, past the rational range).
            using fold         = unit_fold<L, source_lo<rational>, source_hi<rational>>;
            using W            = typename fold::W;
            const auto [qq, w] = fold::fold(static_cast<W>(wide_numerator(rhs_r)));
            const W v          = fold::lower + w;
            q                  = qq;
            wrapped            = v < W{0} ? -rational{static_cast<umax>(-v)} : rational{static_cast<umax>(v)};
        } else {
            // q = floor((rhs - lower) / range), wrapped = rhs - q * range. A step
            // past the 64-bit rational range, or a fold count past imax, cannot
            // be computed exactly: report it rather than store a wrapped guess.
            const auto span  = try_sub(detail::upper64<L>, detail::lower64<L>);
            const auto range = span ? try_add(*span, detail::notch64<L>) : span;
            const auto off   = try_sub(rhs_r, detail::lower64<L>);
            const auto quot  = (range && off) ? try_div(*off, *range) : off;
            if (!range || !quot || *quot < rational{std::numeric_limits<imax>::min()} ||
                *quot > rational{std::numeric_limits<imax>::max()}) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
            q             = floor(*quot);
            const auto qr = try_mul(rational{q}, *range);
            const auto w  = qr ? try_sub(rhs_r, *qr) : qr;
            if (!w) [[unlikely]] {
                policy.report(errc::overflow);
                return;
            }
            wrapped = *w;
        }

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

        if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(q));
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
    [[gnu::cold, gnu::noinline]] static constexpr wide_slot_result wide_slot(const rational& v) {
        const exact_index_result r = exact_index<L, rounding_for<L, P>>(exact_of(v));
        return {static_cast<umax>(r.Index), r.Exact}; // in range: fits 64 bits
    }

    template <typename P, typename A = no_action>
    static constexpr void store_checked(L& lhs, R rhs, P&& policy, A&& action = {}) {
        if constexpr (rational_storage<L>)
            lhs = L::from_raw(rhs); // continuous: store verbatim
        else if constexpr (detail::point_grid<L>)
            lhs = L::from_raw({}); // a point: its value is the type
        else {
            // Store the k-th notch slot: raw_from_offset<L> covers index and
            // value raws.
            auto store_slot = [&](auto k) { lhs = L::from_raw(raw_from_offset<L>(k)); };

            constexpr bool has_round_flag = has_policy<L, P, round_nearest> || has_policy<L, P, round_floor> ||
                                            has_policy<L, P, round_ceil> || has_policy<L, P, round_half_even> ||
                                            has_policy<L, P, snap>;

            // A floating source on an anchored grid with a power-of-two notch
            // whose values double holds: v/Notch is exact, so round it in
            // double (the integer storage's rule) and read the slot off the
            // value index. No rational round trip.
            if constexpr (std::floating_point<R> && anchored<L> && double_exact<grid_of<L>> &&
                          std::has_single_bit(detail::notch64<L>.Numerator)) {
                constexpr imax lo = signed_numerator((detail::lower64<L> / detail::notch64<L>).value());
                const double   v  = static_cast<double>(rhs);
                const imax     j  = snap_double_index<grid_of<L>, rounding_for<L, P>>(v);
                if constexpr (!has_round_flag)
                    if (static_cast<double>(j) * static_cast<double>(detail::notch64<L>) != v && policy.round_check())
                        [[unlikely]]
                        return report_failure(lhs, policy, action, errc::rounding_error);
                store_slot(static_cast<umax>(j - lo));
                return;
            }

            // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
            // (num − Lo·aden)·K / aden — integer ops instead of two rational ops.
            // round_quotient is invariant under reduction, so the slot is
            // bit-identical to the rational path. Oversized denominators
            // fall through (the kMaxDen guard keeps every product inside imax).
            if constexpr (qformat_codec_fits<L> && detail::notched<L>) {
                constexpr imax K   = abs_den(detail::notch64<L>.Denominator);
                constexpr imax Lo  = lower_imax<L>;
                constexpr umax kKM = [] {
                    // 2 · K · M with saturation (M bounds |value| and the offset span)
                    umax k = static_cast<umax>(K);
                    umax m = static_cast<umax>(ceil(((detail::abs(detail::lower64<L>) > detail::abs(detail::upper64<L>)
                                                          ? detail::abs(detail::lower64<L>)
                                                          : detail::abs(detail::upper64<L>))))) *
                                 2 +
                             2;
                    if (k > std::numeric_limits<umax>::max() / m)
                        return std::numeric_limits<umax>::max();
                    umax km = k * m;
                    return (km > std::numeric_limits<umax>::max() / 2) ? std::numeric_limits<umax>::max() : km * 2;
                }();
                constexpr umax kMaxDen = static_cast<umax>(std::numeric_limits<imax>::max()) / kKM;

                const rational rv{rhs}; // exact (copy for rational R)
                const umax     aden = abs_den(rv.Denominator);
                if (kMaxDen != 0 && aden <= kMaxDen) {
                    // The offset (num − Lo·aden)·K / aden, unreduced: round_quotient
                    // rounds it as the reduced fraction would, and kMaxDen keeps the
                    // product in imax.
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>((num - Lo * static_cast<imax>(aden)) * K);
                    if (onum % aden == 0) {
                        store_slot(onum / aden);
                        return;
                    }
                    if constexpr (has_round_flag) {
                        store_slot(round_quotient<L, P>(onum, aden));
                        return;
                    }
                    // strict policy, off-notch: fall through to the rational path for
                    // the error message / action plumbing (cold).
                }
            }

            // The same shortcut for an unanchored lattice: with K the common
            // denominator of Notch and Lower, Notch = sN/K and Lower = m/K, the
            // offset is (num·K − m·aden)/(aden·sN), reduced by g = gcd(aden, K).
            if constexpr (unanchored_codec<L>.Ok) {
                constexpr auto c = unanchored_codec<L>;
                const rational rv{rhs};
                const umax     aden = abs_den(rv.Denominator);
                if (aden <= c.MaxDen) {
                    const umax g    = std::gcd(aden, static_cast<umax>(c.K));
                    const imax num  = signed_numerator(rv);
                    const umax onum = // ≥ 0: rhs ≥ Lower (in range)
                        static_cast<umax>(num * (c.K / static_cast<imax>(g)) - c.M * static_cast<imax>(aden / g));
                    const umax den2 = (aden / g) * static_cast<umax>(c.S);
                    if (onum % den2 == 0) {
                        store_slot(onum / den2);
                        return;
                    }
                    if constexpr (has_round_flag) {
                        store_slot(round_quotient<L, P>(onum, den2));
                        return;
                    }
                }
            }

            // The exact quotient can overflow the 64-bit rational range (huge
            // source denominator × fine notch): take the slot from the exact wide
            // index instead (rhs is in range, so it fits L's raw). Rounding is
            // the value-space rule, as everywhere else.
            const auto quotient = (rhs - detail::lower64<L>) / detail::notch64<L>;
            if (!quotient.has_value()) [[unlikely]] {
                const wide_slot_result slot = wide_slot<P>(rational{rhs});
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
    static constexpr bool double_bounds_exact =
        std::floating_point<R> && rational{static_cast<double>(detail::lower64<L>)} == detail::lower64<L> &&
        rational{static_cast<double>(detail::upper64<L>)} == detail::upper64<L>;

    // A rational source on integer endpoints compares by multiplying the
    // endpoint by the denominator (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d
    // exceeds any n) — exact, and no division.
    static constexpr bool integer_bounds = std::same_as<R, rational> && abs_den(detail::lower64<L>.Denominator) == 1 &&
                                           abs_den(detail::upper64<L>.Denominator) == 1;

    static constexpr bool out_of_interval(const R& rhs) {
        if (huge(rhs))
            return true;
        if constexpr (double_bounds_exact) {
            constexpr double lo = static_cast<double>(detail::lower64<L>);
            constexpr double hi = static_cast<double>(detail::upper64<L>);
            return rhs < lo || rhs > hi;
        } else if constexpr (integer_bounds) {
            constexpr imax lo = signed_numerator(detail::lower64<L>);
            constexpr imax hi = signed_numerator(detail::upper64<L>);
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
        } else
            return not includes(interval_of<L>, rhs);
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        if constexpr (wide_valued<L>) {
            // NaN / ±inf first, as below; a finite |rhs| ≥ 2^64 is an integer,
            // taken exactly (or by its side, past the grid: exact_of_large).
            if constexpr (std::floating_point<R>)
                if (!(rhs - rhs == 0) || huge(rhs)) [[unlikely]] {
                    if (rhs != rhs) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    if (!(rhs - rhs == 0) && !has_policy<L, P, clamp>) {
                        report_failure(lhs, policy, action, errc::not_finite);
                        return lhs;
                    }
                    return assign_exact<R>(
                        lhs, exact_of_large<L>(static_cast<double>(rhs)), policy, std::forward<A>(action));
                }
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
            if (!(rhs - rhs == 0)) [[unlikely]] {
                if constexpr (has_policy<L, P, clamp>)
                    if (rhs == rhs)
                        return assignment<L, rational>::assign(
                            lhs, rhs > 0 ? detail::upper64<L> : detail::lower64<L>, policy);
                report_failure(lhs, policy, action, errc::not_finite);
                return lhs;
            }

        if (out_of_interval(rhs)) [[unlikely]] {
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
    static constexpr bool maps_raws = integer_storage<L> && integer_storage<R>;

    static constexpr rational calcOffset() {
        if constexpr (!maps_raws || !detail::notched<L>)
            return rational{0};
        else
            return ((detail::lower64<R> - detail::lower64<L>) / detail::notch64<L>).value();
    }

    static constexpr rational calcFactor() {
        if constexpr (!maps_raws || !detail::notched<L> || point_storage<R>)
            return rational{0}; // a point R's raw is always 0: the mapping is Offset alone
        else
            return (detail::notch64<R> / detail::notch64<L>).value();
    }

  public:
    static constexpr rational Offset = calcOffset();
    static constexpr rational Factor = calcFactor();

    // Raw-space integer-only mapping — requires integer raw storage on both
    // sides (not rational).
    // It also needs every raw and every mapped raw in imax: map_raw's L-raw
    // range is [Offset, Offset + Factor·max_index<R>] (+ Lower for value storage).
    static constexpr bool is_integer_mapping = [] {
        if constexpr (!maps_raws || abs_den(Factor.Denominator) != 1 || abs_den(Offset.Denominator) != 1 ||
                      !values_fit_imax<L> || !values_fit_imax<R>)
            return false;
        else {
            const rational base = index_storage<L> ? rational{0} : detail::lower64<L>;
            const auto     lo   = Offset + base;
            const auto     span = Factor * rational{max_index_v<R>};
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
        if constexpr (!maps_raws || !detail::notched<L> || is_integer_mapping || !values_fit_imax<L> ||
                      !values_fit_imax<R>)
            return no;
        else {
            constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
            if (Factor.Numerator > cap || Offset.Numerator > cap)
                return no;
            const imax   f_n = static_cast<imax>(Factor.Numerator); // Factor > 0
            const imax   f_d = abs_den(Factor.Denominator);
            const imax   o_s = signed_numerator(Offset);
            const imax   o_d = abs_den(Offset.Denominator);
            affine_map_t m{0, 0, 0, true};
            if (mul_overflow(f_n, o_d, &m.Mul) || mul_overflow(o_s, f_d, &m.Add) || mul_overflow(o_d, f_d, &m.Den))
                return no;
            // worst-case |numerator| over R's offset range [0, max_index]
            if (max_index_v<R> > cap)
                return no;
            const imax rmax = static_cast<imax>(max_index_v<R>);
            imax       term, num;
            if (mul_overflow(rmax, m.Mul, &term) || add_overflow(term, m.Add < 0 ? -m.Add : m.Add, &num))
                return no;
            // round_quotient equivalence: rounding is reduction-invariant, but
            // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
            // imax — mirror those checks for the unreduced den so both forms
            // take the same branch (ties on negatives differ across branches).
            constexpr auto zl = (detail::lower64<L> / detail::notch64<L>).value_or(rational{0});
            if (abs_den(zl.Denominator) == 1) {
                if (zl.Numerator > cap)
                    return no;
                const imax mbias = signed_numerator(zl);
                imax       mdi, total;
                if (mul_overflow(mbias, m.Den, &mdi) || add_overflow(mdi, num, &total))
                    return no;
            }
            return m;
        }
    }();

    // Map rhs.Raw into L's raw space (requires is_integer_mapping): Offset and
    // Factor map 0-based offsets, and raw_lo turns a raw into its offset and
    // back (0 for an index raw, Lower for a value raw). Offset is an exact
    // integer here, so trunc(Offset) is a constant.
    static constexpr imax map_raw(auto rhs_raw) {
        return static_cast<imax>(Factor.Numerator) * (static_cast<imax>(rhs_raw) - raw_lo<R>)+trunc(Offset) +
               raw_lo<L>;
    }

  private:
    template <typename A>
    static constexpr void apply_clamp(L& lhs, const R& rhs, A&& action) {
        lhs = L::from_raw(endpoint_raw<L>(as_rational(rhs) < detail::lower64<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are insides, so the overshoot is too. It is always
        // in-grid and on-notch for grid_of<R> − grid_of<L>, so the construction is exact.
        if constexpr (clamp_action<plain_t<A>>) {
            constexpr grid            OG = (grid_of<R> - grid_of<L>).value();
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

    template <typename P>
    static constexpr void store(L& lhs, const R& rhs, P&& policy) {
        if constexpr (!maps_raws)
            // A continuous side: store the source's value through the rational
            // store, which rounds it by the policy or reports rounding_error.
            assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy, no_action{});
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
            const rational r_offset = [&] {
                if constexpr (raw_lo<R> == 0)
                    return rational{rhs.raw()};
                else
                    return (rational{rhs.raw()} - detail::lower64<R>).value();
            }();
            rational rat = *(Offset + *(Factor * r_offset));
            umax     ad  = static_cast<umax>(abs_den(rat.Denominator));
            // Round the L-offset to a notch index in VALUE space via round_quotient
            // (same as the scalar path), honouring every rounding mode.
            umax q = round_quotient<L, P>(rat.Numerator, ad);
            // rat is the L-offset; raw_from_offset<L> adds detail::lower64<L> back for value storage.
            lhs =
                L::from_raw((rat.Denominator < 0) ? raw_from_offset<L>(-static_cast<imax>(q)) : raw_from_offset<L>(q));
        }
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {}) {
        // A wide raw on either side: the exact wide path.
        if constexpr (wide_valued<L> || wide_valued<R>) {
            static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                              not excludes(interval_of<L>, interval_of<R>),
                          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
            static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
                          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");
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
    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action) {
        // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
        // for them (matches the integral-rhs path); only strict policies reject it.
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp> ||
                          not excludes(interval_of<L>, interval_of<R>),
                      "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
                      "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        if constexpr (not includes(interval_of<L>, interval_of<R>)) {
            if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>) {
                if constexpr (is_integer_mapping) {
                    if (imax mapped = map_raw(rhs.raw()); mapped < raw_lo<L> || mapped > raw_hi<L>)
                        if (try_clamp_or_fail(lhs, rhs, policy, action))
                            return lhs;
                } else if (const rational v = as_rational(rhs); not includes(interval_of<L>, v)) {
                    // Round first: a value just outside may round onto an endpoint.
                    // (The integer mapping above lands on the lattice: nothing to round.)
                    if constexpr (rounds_before_range_check<L, plain_t<P>>)
                        if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(v); rr.Ok) {
                            lhs = L::from_raw(rr.Raw);
                            return lhs;
                        }
                    if (try_clamp_or_fail(lhs, rhs, policy, action))
                        return lhs;
                }
            }
        }

        store(lhs, rhs, policy);
        return lhs;
    }
};
} // namespace beman::inside::detail



//---------------------------------------------------------------------------
// policy — runtime policy carrier, plus `policy_ref` for per-operation dispatch.
//   policy<F, E>  — compile-time flags F plus an optional error_ref E holding a
//                   beman::inside::errc& (EBO so the no-error-code form is zero-sized).
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
        if (std::is_constant_evaluated())
            return true;
        return is_checked(W) && not test(ignore_range);
    }

    static constexpr bool round_check() {
        if (std::is_constant_evaluated())
            return true;
        return is_checked(W) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error) — also during constant evaluation, so
    // try_make / from_chars / tan_into return error values in constexpr code.
    // Throw mode funnels through the installed handler via an outlined cold
    // helper; at compile time it names a fixed-string diagnostic instead of
    // "non-constexpr function called".
    constexpr void report(errc code) {
        if constexpr (std::is_same_v<E, detail::error_ref>)
            E::Code = E::Code != errc{} ? E::Code : code;
        else {
            if (std::is_constant_evaluated())
                detail::constexpr_error<"inside: value out of range during constant evaluation "
                                        "(checked policy hit; choose clamp/wrap or widen the interval)">();
            detail::raise(code);
        }
    }
};

policy(errc&) -> policy<none, detail::error_ref>;

//---------------------------------------------------------------------------
// is_policy — true for policy<F,E> specializations, false otherwise.
// Used to gate free-fn overloads so they don't accidentally bind P = action tag.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr bool is_policy = false;
template <policy_flag F, typename E>
inline constexpr bool is_policy<policy<F, E>> = true;

// Concept form of is_policy — pulls cvref off so the constraint matches
// forwarded `policy<F,E>` references in template parameters.
template <typename T>
concept policy_like = is_policy<std::remove_cvref_t<T>>;

// policy_flags_of<T> — the flag-set a one-shot `policy<F,E>` carries (else
// `none`). Lets the value+policy constructor and policy_ref's conversion fold
// the per-call flags into their `inside_assignable` check, so a one-shot
// clamp/round actually relaxes the constraint it enables.
template <typename T>
inline constexpr policy_flag policy_flags_of = none;
template <policy_flag F, typename E>
inline constexpr policy_flag policy_flags_of<policy<F, E>> = F;

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
constexpr auto report_or_unexpected(A&& action, P&& policy, errc code, [[maybe_unused]] const char* what)
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
// Each is named after its flag: snapped = snap (truncate toward zero),
// rounded_* = round_*, clamped = clamp, wrapped = wrap.
inline constexpr auto snapped           = make_policy<snap>();
inline constexpr auto rounded_nearest   = make_policy<round_nearest>();
inline constexpr auto rounded_floor     = make_policy<round_floor>();
inline constexpr auto rounded_ceil      = make_policy<round_ceil>();
inline constexpr auto rounded_half_even = make_policy<round_half_even>();
inline constexpr auto clamped           = make_policy<clamp>();
inline constexpr auto wrapped           = make_policy<wrap>();

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
        requires inside_assignable<Target, B, policy_of<Target> | policy_flags_of<P>>
    constexpr operator Target() {
        Target r;
        dispatch_assign(r, Owned, Policy, Actions);
        return r;
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

  public:
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
        requires inside_assignable<Target, B, policy_of<Target> | policy_flags_of<P>>
    constexpr operator Target() {
        Target r;
        dispatch_assign(r, Ref, Policy, Actions);
        return r;
    }

    // expected<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    template <numeric C>
    constexpr B& operator=(const std::expected<C, errc>& other) {
        return dispatch_assign(Ref, other.value(), Policy, Actions);
    }

  private:
    constexpr void report_zero(errc code, const char* what) {
        if constexpr (has_action<is_error_action, As...>)
            pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
        else if constexpr (!has_policy<B, P, ignore_zero>)
            Policy.report(code);
    }

  public:
    //-------------------------------------------------------------------------
    // insidable RHS overloads — route through the inside's arithmetic, then
    // assign via dispatch_assign so callbacks fire on the narrowing back to B.
    // An expected<inside> result carrying an error (rational-raw overflow,
    // division by zero) surfaces its errc through on_overflow if registered,
    // else report.
    //-------------------------------------------------------------------------
  private:
    template <typename R>
    constexpr B& finalise_arith(R&& result, [[maybe_unused]] const char* msg) {
        if constexpr (is_expected_v<R>) {
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

    // Shared body for the rational `+=`/`-=`/`*=`/`/=` operators: lift Ref to
    // rational and route the checked result through `finalise_arith`.
    template <typename RatOp>
    constexpr B& rational_assign(const rational& rhs, RatOp rat_op, const char* msg) {
        return finalise_arith(rat_op(rational{Ref}, rhs), msg);
    }

  public:
    template <insidable C>
    constexpr B& operator+=(const C& rhs) {
        return finalise_arith(add(Ref, rhs, Policy), "policy_ref::operator+= overflow");
    }

    template <insidable C>
    constexpr B& operator-=(const C& rhs) {
        return finalise_arith(sub(Ref, rhs, Policy), "policy_ref::operator-= overflow");
    }

    template <insidable C>
    constexpr B& operator*=(const C& rhs) {
        return finalise_arith(mul(Ref, rhs, Policy), "policy_ref::operator*= overflow");
    }

    // A zero divisor is reported (on_error / ignore_zero) and leaves Ref
    // unchanged, like inside::operator/=: div/mod under ignore_zero skip their
    // own check, so dividing here would be a division by zero.
    template <insidable C>
    constexpr B& operator/=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (!has_flag(policy_of<C>, ignore_zero)) // either operand silences it
                report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return finalise_arith(div(Ref, rhs, Policy), "policy_ref::operator/= division/overflow");
    }

    template <insidable C>
    constexpr B& operator%=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (!has_flag(policy_of<C>, ignore_zero)) // either operand silences it
                report_zero(errc::division_by_zero, "policy_ref::operator%= division by zero");
            return Ref;
        }
        return finalise_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow");
    }

    //-------------------------------------------------------------------------
    // rational RHS overloads — the only non-inside operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_ins` / `just<1>` / `inside<{lo,hi}>{n}`).
    // Lift Ref to rational, checked op, finalise_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(const C& rhs) {
        return rational_assign(rhs, [](rational a, rational b) { return a + b; }, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(const C& rhs) {
        return rational_assign(rhs, [](rational a, rational b) { return a - b; }, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(const C& rhs) {
        return rational_assign(rhs, [](rational a, rational b) { return a * b; }, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(const C& rhs) {
        if (is_canonical_zero(rhs)) {
            report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return rational_assign(
            rhs, [](rational a, rational b) { return a / b; }, "policy_ref::operator/= division/overflow");
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
// Whether a + or × into Result runs its overflow check. The result grid
// holds every result, so only a continuous result can overflow: a fraction
// raw always may (its denominator grows); a rational raw under a checked
// policy (a continuous operand bounds no denominator).
template <insidable Result, insidable L, insidable R, policy_flag F>
inline constexpr bool lattice_op_checked =
    fraction_storage<Result> ||
    (rational_storage<Result> && (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)));
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
    static_assert(grid_sum_fits(grid_of<L>, grid_of<R>),
                  "addition: the result grid exceeds the 64-bit grid numbers — coarsen the "
                  "operand grids, or build with C++26 big grids");
    // (Falls back to L's grid when the assertion failed, so the build stops at
    // that message instead of the rational overflow behind it.)
    static constexpr grid result_grid =
        grid_sum_fits(grid_of<L>, grid_of<R>) ? (grid_of<L> + grid_of<R>).value() : grid_of<L>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check = lattice_op_checked<result, L, R, F>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> return_t<F, A> {
        result res;
        if constexpr (fraction_storage<result>) {
            const auto sum = frac_raw_of<raw_t<result>>(exact_of(lhs) + exact_of(rhs));
            if (!sum) [[unlikely]]
                return report_or_unexpected<result>(action, policy, errc::overflow, "fraction overflow in add");
            res = result::from_raw(*sum);
        } else if constexpr (rational_storage<result>) {
            static_assert(!wide_valued<L> && !wide_valued<R>,
                          "addition: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<F>) {
                auto sum = rational::add(lhs, rhs);
                if (!sum) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow, "rational overflow in add");
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
            constexpr grid_rational U = grid_gcd_of(unit_of<L>, unit_of<R>);
            using W                   = index_work_t<result, L, U, R, U, U>;
            res = from_value_in_units<result, U>(value_in_units<W, U>(lhs) + value_in_units<W, U>(rhs));
        }
        return res;
    }
};
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
    static_assert(grid_product_fits(grid_of<L>, grid_of<R>),
                  "multiplication: the result grid exceeds the 64-bit grid numbers — round the "
                  "product into a coarser type with mul_into<Out>(a, b), coarsen the operand "
                  "grids, or build with C++26 big grids");
    // (Falls back to L's grid when the assertion failed, so the build stops at
    // that message instead of the rational overflow behind it.)
    static constexpr grid result_grid =
        grid_product_fits(grid_of<L>, grid_of<R>) ? (grid_of<L> * grid_of<R>).value() : grid_of<L>;
    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check = lattice_op_checked<result, L, R, F>;

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>, result, std::expected<result, errc>>;

    // `x * just<c>` (c != 0): the result lattice is x's lattice scaled by c
    // (see grid operator*), so the result offset IS x's offset — counted from
    // the far end when c < 0. No multiply at all.
    template <insidable Point, insidable X>
    static constexpr bool point_scale =
        point_grid<Point> && lower_of<Point> != 0 && notched<X> && !wide_valued<X> && !wide_valued<result>;

    template <bool Negate, insidable X>
    static constexpr result scale_by_point(const X& x) {
        static_assert(max_index_v<result> == max_index_v<X>);
        umax off;
        if constexpr (index_storage<X>)
            off = static_cast<umax>(x.raw());
        else
            off = static_cast<umax>(x.raw()) - static_cast<umax>(raw_lo<X>);
        return result::from_raw(raw_from_offset<result>(Negate ? max_index_v<X> - off : off));
    }

    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> return_t<policy_flags_of<plain_t<P>>, A> {
        if constexpr (point_scale<R, L>)
            return scale_by_point<(lower_of<R> < 0)>(lhs);
        else if constexpr (point_scale<L, R>)
            return scale_by_point<(lower_of<L> < 0)>(rhs);
        else if constexpr (fraction_storage<result>) {
            const auto prod = frac_raw_of<raw_t<result>>(exact_of(lhs) * exact_of(rhs));
            if (!prod) [[unlikely]]
                return report_or_unexpected<result>(action, policy, errc::overflow, "fraction overflow in mul");
            return result::from_raw(*prod);
        } else if constexpr (rational_storage<result>) {
            static_assert(!wide_valued<L> && !wide_valued<R>,
                          "multiplication: a wide-index operand with a continuous result is not supported yet");
            if constexpr (needs_overflow_check<policy_flags_of<plain_t<P>>>) {
                auto prod = as_rational(lhs) * as_rational(rhs);
                if (!prod) [[unlikely]]
                    return report_or_unexpected<result>(action, policy, errc::overflow, "rational overflow in mul");
                return result::from_raw(*prod);
            } else
                return result::from_raw(rational::mul_unchecked(as_rational(lhs), as_rational(rhs)));
        } else if constexpr (point_storage<result>)
            return result::from_raw(raw_t<result>{}); // a product with 0: the point 0
        else {
            // Integer raws (a notched result has no continuous operand): multiply the operands' values in their own
            // units (a notch or value unit, or |c| for a point c), in imax or by wrapping arithmetic (wide_value.hpp).
            // The product of the unit counts counts the product in the product of the units — on anchored grids the
            // result notch — exact for every grid and sign, at any width.
            static_assert(integer_storage<L> && integer_storage<R>);
            constexpr grid_rational U = grid_mul(unit_of<L>, unit_of<R>);
            using W                   = index_work_t<result, L, unit_of<L>, R, unit_of<R>, U>;
            return from_value_in_units<result, U>(value_in_units<W, unit_of<L>>(lhs) *
                                                  value_in_units<W, unit_of<R>>(rhs));
        }
    }
};
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/detail/division.hpp
// ======================================================================



//---------------------------------------------------------------------------
// division / modulo. `division::div` returns expected<result, errc> (division by zero
// is always runtime-possible). Two paths: native (integer-aligned grids +
// snap → native integer division) and rational (exact, can overflow under
// checked). `modulo::mod` is integer-only — non-integer remainders aren't
// well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// Both operands are plain integer grids and the caller accepted integer
// truncation (snap) — the prerequisite for native integer div / mod.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_ops =
    ((F | policy_of<L> | policy_of<R>)&snap) && integer_lattice<L> && integer_lattice<R>;

// ...and every value fits imax, so the builtin integer division applies.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_native_ops = integer_ops<L, R, F> && values_fit_imax<L> && values_fit_imax<R>;

//---------------------------------------------------------------------------
// Rounding mode for the native div & mod paths (fire when `snap` is set).
// Decided from the combined flags by rounding_of (policy_flag.hpp), the one
// precedence all rounding paths share; `snap` alone is truncate-toward-zero.
// The runtime quotient and the compile-time grid endpoints MUST agree on the
// mode (both read rounding_of), or a result could escape its own grid.
//---------------------------------------------------------------------------

// Round the signed exact quotient a/b (b != 0) to an integer per `m`.
template <std::signed_integral T>
constexpr T div_rounded(T a, T b, round_mode m) noexcept {
    using U        = std::make_unsigned_t<T>;
    const T    t   = a / b;                        // C++ truncation toward zero
    const T    r   = a % b;                        // sign of a, |r| < |b|
    const bool neg = (a < 0) != (b < 0);           // exact quotient is negative
    const U    ar  = r < 0 ? U(~U(r) + 1u) : U(r); // |r|, |b| in U (safe for T::min)
    const U    ab  = b < 0 ? U(~U(b) + 1u) : U(b);
    if (!rounds_away(m, neg, classify_remainder(m, ar, ab), (t & 1) != 0))
        return t;
    return neg ? T(t - 1) : T(t + 1);
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

// Round a non-negative quotient num/den (den != 0) per `m`. Used by the
// Q-format path, whose raws are non-negative (Lower == 0).
// U is a builtin unsigned integer or an unsigned wide_int.
template <raw_integer U>
constexpr U round_uquotient(U num, U den, round_mode m) noexcept {
    const U t = num / den;
    return rounds_away(m, false, classify_remainder(m, U(num % den), den), (t & 1) != 0) ? t + 1 : t;
}

// The zero-divisor check is skipped when R's grid excludes zero or
// `ignore_zero` is set (a zero divisor is then UB, matching the `/= 0` no-op).
template <insidable L, insidable R, policy_flag F, policy_flag G>
inline constexpr bool divisor_unchecked =
    divisor_excludes_zero<R> || ((G | F | policy_of<L> | policy_of<R>)&ignore_zero) != 0;

// Compile-time rounding of a quotient-interval endpoint to an integer index.
// Under half_even the endpoint is bracketed by [floor, ceil] (Upper: ceil)
// rather than reproducing the parity rule at compile time.
constexpr imax round_rat(rational q, round_mode m, bool upper) noexcept {
    if (m == round_mode::half_even)
        m = upper ? round_mode::ceil : round_mode::floor;
    return round_to_int(q, m);
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
        constexpr umax         imax_max = static_cast<umax>(std::numeric_limits<imax>::max());
        umax                   ad, bc;
        return l.Ok && r.Ok && r.Num <= imax_max && !mul_overflow(l.Num, r.Den, &ad) &&
               !mul_overflow(l.Den, r.Num, &bc) && bc <= imax_max;
    }
}

template <insidable L, insidable R = L, policy_flag F = none>
struct division {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_qformat — both same Q-format (Notch = 1/N, Lower = 0); formula
    //                        `(a·N)/b` (the native `(a << log2 N)/b` idiom).
    // Otherwise the exact-rational path returns inside<rational>.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    // (if constexpr: naming a 64-bit view instantiates it, even where && would
    // skip it — so an exact-valued operand returns before any is named.)
    static constexpr bool native_div_qformat = [] {
        if constexpr (wide_valued<L> || wide_valued<R>)
            return false;
        else
            return ((F | policy_of<L> | policy_of<R>)&snap) && qformat_grid<L> && qformat_grid<R> &&
                   notch_of<L> == notch_of<R>;
    }();

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode = rounding_of(F | policy_of<L> | policy_of<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (grid_of<L> / grid_of<R>).has_value(),
                  "division: result grid not representable (notch/interval exceeds the "
                  "representable rational range) — coarsen the operand grids");
    static_assert(
        [] {
            if constexpr (native_div_qformat)
                return (detail::upper64<L> / detail::notch64<R>).has_value();
            else
                return true;
        }(),
        "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid = [] {
        if constexpr (native_div_integer)
            return grid{round_rat(to_rational((*(grid_of<L> / grid_of<R>)).Interval.Lower), rmode, false),
                        round_rat(to_rational((*(grid_of<L> / grid_of<R>)).Interval.Upper), rmode, true)};
        else if constexpr (native_div_qformat)
            return grid{interval{rational{0}, (detail::upper64<L> / detail::notch64<R>).value()}, detail::notch64<L>};
        else
            return *(grid_of<L> / grid_of<R>);
    }();

    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>. The
    // operand grids may prove the quotient fits (quotient_fits_rational).
    // A wide-index operand's quotient may outgrow the 64-bit rational
    // whatever the policy, so that path always reports.
    static constexpr bool fits_rational = quotient_fits_rational<L, R>();
    static constexpr bool may_overflow_nonzero =
        !native_div && !fits_rational && (needs_overflow_check<F> != 0 || wide_valued<L> || wide_valued<R>);

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
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> return_t<A> {
        if constexpr (overflow_action<plain_t<A>>)
            return report_or_unexpected<result>(action, policy, code, what); // -> result
        else if constexpr (!divisor_excludes_zero<R> || may_overflow_nonzero)
            return report_or_unexpected<result>(action, policy, code, what); // -> expected<result, errc>
        else
            return result{}; // unreachable: divisor excludes zero, op cannot fail
    };

    // The fail arms stay keyed on divisor_excludes_zero (which narrows the
    // return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;

    if constexpr (native_div_qformat) {
        // rhs.Raw == 0 iff rhs.value == 0 (detail::lower64<R> == 0). Formula folds to
        // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
        if constexpr (!zero_unchecked)
            if (rhs.raw() == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        constexpr umax N = abs_den(detail::notch64<L>.Denominator);
        // The scaled dividend raw·N in the narrowest type that holds it: a 32-bit
        // divide where it fits (Q8.8, Q16.15, ...), else 64 bits, else a wide_int.
        constexpr int dividend_bits = std::bit_width(max_index_v<L>) + std::bit_width(N);
        using U = std::conditional_t<(max_index_v<L> <= std::numeric_limits<std::uint32_t>::max() / N),
                                     std::uint32_t,
                                     int_for_bits_t<(dividend_bits < 64 ? 64 : dividend_bits), false>>;
        return result::from_raw(raw_cast<result>(
            round_uquotient<U>(static_cast<U>(static_cast<U>(lhs.raw()) * U{N}), static_cast<U>(rhs.raw()), rmode)));
    } else if constexpr (native_div_integer) {
        using T         = native_div_t<L, R>;
        const T rhs_val = static_cast<T>(to_value(rhs));
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        result res;
        from_value(res, imax{div_rounded(static_cast<T>(to_value(lhs)), rhs_val, rmode)});
        return res;
    } else if constexpr (wide_valued<L> || wide_valued<R>) {
        // A wide-index or big-grid operand: the exact quotient, in the result's raw.
        const auto d = exact_of(rhs);
        if constexpr (!zero_unchecked)
            if (d.Num.is_zero())
                return fail(errc::division_by_zero, "division by zero in div");
        if constexpr (fraction_storage<result>) {
            // Grids past 64 bits: the exact quotient in the result's wide fraction.
            const auto q = frac_raw_of<raw_t<result>>(exact_of(lhs) / d);
            if (!q) [[unlikely]]
                return fail(errc::overflow, "quotient past its fraction raw in div");
            return result::from_raw(*q);
        } else {
            const auto q = try_rational(exact_of(lhs) / d);
            if (!q) [[unlikely]]
                return fail(errc::overflow, "rational overflow in div");
            return result::from_raw(*q);
        }
    } else if constexpr (needs_overflow_check<G> && !fits_rational) {
        rational rhs_r = rhs;
        if constexpr (!zero_unchecked)
            if (rhs_r.Numerator == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        auto q = as_rational(lhs) / rhs_r;
        if (!q) [[unlikely]]
            return fail(errc::overflow, "rational overflow in div");
        return result::from_raw(*q);
    } else {
        rational rhs_r = rhs;
        if constexpr (!zero_unchecked)
            if (rhs_r.Numerator == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
}
//---------------------------------------------------------------------------
// modulo (requires integer-valued grids + snap)
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F = none>
struct modulo {
    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(integer_ops<L, R, F>, "modulo requires integer-valued grids and snap");

    // Builtin division when every value fits imax; else exact wide integers.
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    static constexpr grid_rational max_rem =
        lift_unwrap((abs(lower_of<R>) > abs(upper_of<R>) ? abs(lower_of<R>) : abs(upper_of<R>)) - grid_rational{1});

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode = rounding_of(F | policy_of<L> | policy_of<R>);

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
        const I b{trunc(exact_of(rhs))};
        if constexpr (!zero_unchecked)
            if (b.is_zero())
                return report_or_unexpected<result>(action, policy, errc::division_by_zero, "division by zero in mod");
        const I a{trunc(exact_of(lhs))};
        return exact_result<result>(exact_frac<exact_limbs<L, R, result>>{a - rounded_div<rmode>(a, b) * b, I{1}});
    } else {
        using T                       = native_div_t<L, R>;
        const T        rhs_val        = static_cast<T>(to_value(rhs));
        constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return report_or_unexpected<result>(action, policy, errc::division_by_zero, "division by zero in mod");
        result res;
        // Remainder consistent with the rounded quotient (trunc → C++ `%`).
        const T lhs_val = static_cast<T>(to_value(lhs));
        // The product stays in imax: q·b can exceed |a| + |b| ≥ 2^31 in T.
        from_value(res, lhs_val - imax{div_rounded(lhs_val, rhs_val, rmode)} * rhs_val);
        return res;
    }
}
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/predicates.hpp
// ======================================================================


//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   conversion_overflows<B>(v) — v falls outside B's interval.
//   conversion_rounds<B>(v)    — v is in-range but off-notch (would round).
//   conversion_is_lossy<B>(v)  — either of the two.
//---------------------------------------------------------------------------
namespace beman::inside {
template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_overflows(A value) noexcept {
    if constexpr (std::floating_point<A>)
        if (!(value - value == 0))
            return true; // NaN / ±inf fit no grid (and must not raise here)
    if constexpr (std::floating_point<A>)
        if (!(value < 0x1p64 && value > -0x1p64))
            return true; // beyond every grid, no rational form
    const detail::rational r = detail::as_rational(value);
    if (includes(interval_of<B>, r))
        return false;
    // B's policy rounds before it range-checks: a value that rounds onto the
    // grid does not overflow.
    detail::rational rounded;
    return !detail::rounds_into_range<B, policy<>>(r, rounded);
}

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_rounds(A value) noexcept {
    if constexpr (!::beman::inside::detail::notched<B>)
        return false; // continuous grid: no notch to miss
    if constexpr (std::floating_point<A>)
        if (!(value - value == 0))
            return false; // non-finite — overflow, not truncation
    if constexpr (std::floating_point<A>)
        if (!(value < 0x1p64 && value > -0x1p64))
            return false; // overflow, not truncation
    detail::rational r = detail::as_rational(value);
    if (not includes(interval_of<B>, r)) {
        // Out of range: a rounding policy that brings it onto the grid rounds;
        // anything else is overflow, not rounding.
        detail::rational rounded;
        return detail::rounds_into_range<B, policy<>>(r, rounded);
    }
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - ::beman::inside::detail::lower64<B>) / ::beman::inside::detail::notch64<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
}

template <insidable B, numeric A>
[[nodiscard]] constexpr bool conversion_is_lossy(A value) noexcept {
    return conversion_overflows<B>(value) || conversion_rounds<B>(value);
}
} // namespace beman::inside



// Deducing `this` (P0847) folds the lvalue/rvalue overload pairs below.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
    #error "beman.inside requires GCC 14 or newer (deducing this)"
#endif

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
template <insidable Out, insidable In>
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
// (per-operator code), assignment.hpp (narrowing/clamp/wrap), and
// generic.hpp/policy.hpp (traits + policy machinery).
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// inside
//---------------------------------------------------------------------------
template <grid G, policy_flag P>
struct inside {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");

    using negative = inside<-G, P & ~detail::cursor_marker>; // −cursor is no cursor
    using raw_type = detail::storage_min_t<G>;

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
    // default-constructed inside has no value until assigned. (A previous checked
    // overload zero-filled Raw, which decoded to an out-of-range value or an invalid
    // {0,0} rational for grids not containing 0 — a defined-but-invalid footgun.
    // Value-init `inside{}` still zero-fills where a zero raw is genuinely wanted.)
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
    template <numeric A>
        requires inside_assignable<inside, A, P>
    constexpr inside(A value) {
        store_value(value);
    }

    // One-shot policy: `pol`'s flags widen the assignable check (a clamp/round
    // relaxes the interval/notch clause, e.g. clamp_round<B>(some_inside)) and
    // apply to this store. If it reports an error (ec mode), Raw is ill-defined.
    template <numeric A, typename Pol>
        requires inside_assignable<inside, A, P | detail::policy_flags_of<std::remove_cvref_t<Pol>>>
    constexpr inside(A value, Pol&& pol) {
        store_value(value, pol);
    }

    // No error-code constructor: construction that can fail is `try_make(value)`,
    // which returns expected<inside, errc>. This overload only turns
    // `inside x(value, ec)` into a readable error (an errc& would otherwise bind
    // the Pol&& constructor above).
    template <numeric A>
    constexpr inside(A, errc&) {
        static_assert(detail::dependent_false<A>,
                      "inside(value, errc&) was removed: use `auto r = B::try_make(value);` "
                      "(expected<B, errc>), or `b.policy(ec) = value` to assign with an error code");
    }

    // expected<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws
    // std::bad_expected_access on an error.
    template <numeric A>
        requires inside_assignable<inside, A, P>
    constexpr inside(const std::expected<A, errc>& value) {
        store_value(value.value());
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
        requires((detail::integer_notch<G> || ((P & snap) != 0 && !detail::wide_valued<inside>)) &&
                 G.Interval.Lower >= detail::rational{std::numeric_limits<imax>::min()} &&
                 G.Interval.Upper <= detail::rational{std::numeric_limits<imax>::max()})
    {
        if constexpr (detail::integer_notch<G>)
            return detail::to_value(*this);
        else
            return round_to_int(detail::as_rational(*this), detail::rounding_of(P));
    }

    constexpr explicit operator double() const
        requires((P & (round_floor | round_ceil | round_nearest | round_half_even | snap)) != 0)
    {
        return detail::as_double(*this);
    }

    // Unavailable on a wide-index grid: its values outgrow the 64-bit rational
    // (compare it, or read it with to<T>()).
    constexpr operator detail::rational() const
        requires(!detail::wide_valued<inside>)
    {
        if constexpr (G.Interval.Lower == G.Interval.Upper)
            return G.Interval.Lower;
        else if constexpr (detail::value_storage<inside>)
            return Raw;
        // Q-format-with-integer-Lower fast path skips the generic path's three
        // rational ops. Falls through to the rational path when the raw is too wide
        // to widen safely (e.g. uint64 from a Q16.16 × Q16.16 result type).
        else if constexpr (detail::qformat_codec_fits<inside>)
            return detail::q_format_decode(*this);
        else
            return (*(Raw * detail::notch64<inside>)+detail::lower64<inside>).value();
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
        else if constexpr (detail::wide_valued<inside>) {
            const auto v = detail::exact_of(*this);
            if (check_lo && v < detail::exact_of(lim::min()))
                return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if (check_hi && v > detail::exact_of(lim::max()))
                return std::unexpected{errc::overflow};
            return static_cast<T>(trunc(v));
        } else {
            const auto r = detail::as_rational(*this);
            if (check_lo && r < detail::rational{lim::min()})
                return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if (check_hi && r > detail::rational{lim::max()})
                return std::unexpected{errc::overflow};
            return static_cast<T>(trunc(r));
        }
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const {
        return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict inside rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
        requires(!std::floating_point<T> ||
                 (P & (round_floor | round_ceil | round_nearest | round_half_even | snap)) != 0)
    {
        return to<T>().value();
    }

    // numerator() / denominator() — the exact value in lowest terms as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
    // imax when every value fits it, else a wide integer that holds every value
    // (a wide grid, a grid past int64).
    [[nodiscard]] constexpr auto numerator() const {
        if constexpr (detail::values_fit_imax<inside>)
            return fraction().first;
        else
            return detail::reduced_exact(*this).Num;
    }
    [[nodiscard]] constexpr auto denominator() const {
        if constexpr (detail::values_fit_imax<inside>)
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

    [[nodiscard]] constexpr negative operator-() const {
        negative neg;
        if constexpr (detail::point_storage<inside>)
            neg = negative::from_raw({});            // −point is a point: no raw
        else if constexpr (!detail::notched<inside>) // continuous: the raw is the value
            neg = negative::from_raw(-(Raw));
        else {
            // Integer raws: the negated value index is −J (wide_value.hpp), in imax
            // when the bounds allow, else by wrapping. Index storage on both sides
            // counts the slot from the opposite end instead.
            using W = detail::index_work_t<negative, inside, G.Notch, inside, G.Notch>;
            if constexpr (detail::index_storage<inside> && detail::index_storage<negative>) {
                constexpr W count = static_cast<W>(G.slot_count());
                neg = negative::from_raw(static_cast<detail::raw_t<negative>>(count - static_cast<W>(Raw)));
            } else
                neg = detail::from_value_index<negative>(W{0} - detail::value_index<W>(*this));
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
        detail::notched<inside> && detail::point_grid<R> &&
        (detail::wide_numerator(lower_of<R>) * detail::wide_denominator(notch_of<inside>)) %
                (detail::wide_denominator(lower_of<R>) * detail::wide_numerator(notch_of<inside>)) ==
            detail::grid_wide{0};

    // (R anchored: its Lower is a whole number of notches, the bias.)
    template <insidable R>
    static constexpr bool raw_add_ok = detail::notched<inside> && notch_of<inside> == notch_of<R> &&
                                       !detail::point_storage<R> && detail::anchored<R>; // ⇒ integer raws

    template <insidable R>
    static constexpr detail::grid_wide point_delta = detail::exact_quotient(lower_of<R>, notch_of<inside>);
    template <insidable R>
    static constexpr detail::grid_wide add_bias = [] {
        if constexpr (detail::index_storage<R>)
            return detail::slot_base<R>;
        else
            return detail::grid_wide{0};
    }();

    // Work type of a raw-space add whose delta lies in [Dlo, Dhi]: it holds
    // every raw of this grid, the delta, the new raw (in [raw_lo + Dlo,
    // raw_hi + Dhi]) and the wrap range raw_hi − raw_lo + 1, so the add cannot
    // overflow. imax for every grid within int64.
    // (A variable template, not a function: Clang would instantiate a plain
    // member function's body while the class is still incomplete.)
    template <detail::grid_wide Dlo, detail::grid_wide Dhi>
    static constexpr int raw_work_bits = [] {
        using W        = detail::grid_wide;
        constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
        return detail::signed_value_bits_of({lo, hi, Dlo, Dhi, lo + Dlo, hi + Dhi, hi - lo + W{1}});
    }();
    template <detail::grid_wide Dlo, detail::grid_wide Dhi>
    using raw_work_t = detail::work_int_t<raw_work_bits<Dlo, Dhi>>;

  public:
    template <insidable R>
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
            constexpr W bias = static_cast<W>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(rhs.raw()) + bias);
        } else
            return assign_op_result(*this + rhs);
    }

  private:
    // Store a raw computed by the raw-space fast paths of += and -=, in their
    // work type W. Under clamp/wrap/checked an out-of-range raw is clamped,
    // wrapped or reported.
    template <typename W>
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

  public:
    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
  private:
    template <typename Result>
    constexpr inside& assign_op_result(const Result& r) {
        if constexpr (detail::is_expected_v<Result>) {
            // A failed op is reported through this type's policy (throw / handler);
            // *this stays unchanged. `*r` (not value()): no bad_expected_access.
            if (r.has_value())
                *this = *r;
            else
                make_policy<P>().report(r.error());
        } else
            *this = r;
        return *this;
    }

    // A zero divisor in /= or %=; ignore_zero on either operand silences it,
    // as it does for div/mod.
    template <typename R>
    constexpr inside& report_div_by_zero() {
        if constexpr (!has_flag(P | policy_of<R>, ignore_zero))
            make_policy<P>().report(errc::division_by_zero);
        return *this;
    }

  public:
    // Only a `rational` (a library type) may join an inside in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_ins` /
    // `just<1>` / `inside<{lo,hi}>{n}`), mirroring the binary operators.
    // A rational on a Q-format grid's lattice as a signed notch count (rhs·K
    // for notch 1/K), when it is a whole number of notches of moderate size.
    static constexpr bool raw_delta_ok = detail::qformat_codec_fits<inside> && detail::values_fit_imax<inside>;
    static constexpr std::optional<imax> notch_delta(const detail::rational& r) noexcept {
        constexpr umax K = detail::abs_den(detail::notch64<inside>.Denominator);
        const umax     d = detail::abs_den(r.Denominator);
        if (K % d != 0 || r.Numerator > (umax{1} << 62) / (K / d))
            return std::nullopt;
        const imax n = static_cast<imax>(r.Numerator * (K / d));
        return r.Denominator < 0 ? -n : n;
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator+=(const A& rhs) {
        if constexpr (raw_delta_ok)
            if (const auto n = notch_delta(rhs)) [[likely]]
                return store_raw<imax>(static_cast<imax>(Raw) + *n);
        return assign_op_result(detail::rational{*this} + rhs);
    }

    template <insidable R>
    constexpr inside& operator-=(const R& rhs) {
        // Raw-space fast path, the subtraction mirror of +='s.
        if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<-detail::raw_hi_exact<R> - add_bias<R>, -detail::raw_lo_exact<R> - add_bias<R>>;
            constexpr W bias = static_cast<W>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) - static_cast<W>(rhs.raw()) - bias);
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
    constexpr inside& operator*=(const R& rhs) {
        return assign_op_result(*this * rhs);
    }

    // The outer zero check is semantic, not redundant: the binary `a / b`
    // yields an error value on a zero divisor (expected vocabulary), so the compound
    // form's report comes from here. (Measured perf-neutral to remove.)
    template <insidable R>
    constexpr inside& operator/=(const R& rhs) {
        if (rhs == 0)
            return report_div_by_zero<R>();
        return assign_op_result(*this / rhs);
    }

    template <insidable R>
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
    constexpr inside& operator+=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this += *rhs) : report_error(rhs.error());
    }
    template <insidable R>
    constexpr inside& operator-=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this -= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
    constexpr inside& operator*=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this *= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
    constexpr inside& operator/=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this /= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
    constexpr inside& operator%=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this %= *rhs) : report_error(rhs.error());
    }

  private:
    constexpr inside& report_error(errc e) {
        make_policy<P>().report(e);
        return *this;
    }

  public:
    // ++/-- move one notch: they add the point inside of ±Notch through the
    // insidable += (which has the raw-level integer fast path) instead of the
    // rational round-trip (~30× the instructions on an integer grid). On a
    // notch-1 grid that is ±1; on a cursor, one step. `just` itself is declared
    // after the class, so spell the point inside directly.
    constexpr inside& operator++() {
        static_assert(G.Notch != 0, "inside: ++ steps one notch - a grid without a notch has none to step");
        // constexpr local: the point inside is materialised at compile time.
        constexpr auto kStep = inside<grid{G.Notch}>::from_raw({});
        return *this += kStep;
    }
    constexpr inside operator++(int) {
        inside t = *this;
        ++*this;
        return t;
    }
    constexpr inside& operator--() {
        static_assert(G.Notch != 0, "inside: -- steps one notch - a grid without a notch has none to step");
        constexpr auto kStep = inside<grid{-G.Notch}>::from_raw({});
        return *this += kStep;
    }
    constexpr inside operator--(int) {
        inside t = *this;
        --*this;
        return t;
    }

    // try_make<F>(value): the constructors' store under this type's policy plus
    // the flags F, with the failure as the error. An expected value passes its
    // error on, so fallible steps chain.
    template <policy_flag F = none, numeric A>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value) {
        errc   ec{};
        inside result;
        result.store_value(value, make_policy<P | F>(ec));
        if (ec != errc{})
            return std::unexpected{ec};
        return result;
    }
    template <policy_flag F = none, numeric A>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(const std::expected<A, errc>& value) {
        if (!value)
            return std::unexpected{value.error()};
        return try_make<F>(*value);
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
// from_chars<B, F>(first, last) — text → B, exactly (no double round-trip). The
// whole range must be one number: an optional sign, then the literal grammar
// (1'000, 1.25, 1.5e2, 0xff, 0b1010, 0x1.8p3) or a fraction N/D. Malformed text
// is errc::invalid_format; the value then goes through B::try_make<F>, so B's
// policy plus the flags F round, clamp or wrap it and report overflow /
// rounding_error. from_chars_exact<B> accepts only a value B holds exactly:
// off the grid is rounding_error and out of range overflow, whatever B's
// policy. (io.hpp adds std::string_view overloads and operator>>.)
//---------------------------------------------------------------------------
template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> from_chars(const char* first, const char* last) {
    const auto v = detail::parse_text(first, last);
    if (!v && v.error() == errc::overflow) {
        // A value past the 64-bit rational (a long decimal, a wide grid's
        // value): parse it exactly instead.
        const auto w = detail::parse_exact<detail::exact_limbs<B>>(first, last);
        if (!w)
            return std::unexpected{w.error()};
        errc ec{};
        B    b{};
        detail::assign_exact<detail::rational>(b, *w, make_policy<policy_of<B> | F>(ec), no_action{});
        if (ec != errc{})
            return std::unexpected{ec};
        return b;
    }
    if (!v)
        return std::unexpected{v.error()};
    return B::template try_make<F>(*v);
}

template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> from_chars_exact(const char* first, const char* last) {
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
    constexpr std::size_t K = detail::exact_limbs<B>;
    const auto            w = detail::parse_exact<K>(first, last);
    if (!w)
        return std::unexpected{w.error()};
    if (*w < detail::exact_of_grid<K>(lower_of<B>) || detail::exact_of_grid<K>(upper_of<B>) < *w)
        return std::unexpected{errc::overflow};
    if constexpr (detail::notched<B>)
        if (!detail::exact_index<B, detail::round_mode::trunc>(*w).Exact)
            return std::unexpected{errc::rounding_error};
    errc ec{};
    B    b{};
    detail::assign_exact<detail::rational>(b, *w, make_policy<policy_of<B>>(ec), no_action{});
    if (ec != errc{})
        return std::unexpected{ec};
    return b;
}

//---------------------------------------------------------------------------
// comparison
//---------------------------------------------------------------------------
namespace detail {
// Integer value-index comparison eligibility: an integer-backed anchored
// inside whose value indices (value/Notch, integers) fit imax, so two
// same-notch insides compare as `bias + raw` without a rational decode.
template <insidable B>
inline constexpr bool index_cmp_fits = [] {
    if constexpr (!detail::notched<B> || !detail::anchored<B> || !values_fit_imax<B>)
        return false;
    else {
        constexpr auto lo  = detail::lower64<B> / detail::notch64<B>;
        constexpr auto hi  = detail::upper64<B> / detail::notch64<B>;
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        return lo.has_value() && hi.has_value() && (*lo).Numerator <= cap && (*hi).Numerator <= cap;
    }
}();

// Signed value index of Raw == 0: Lower/Notch for offset (index) storage,
// 0 for a value raw (raw is already the value == the index at notch 1).
template <insidable B>
inline constexpr imax index_cmp_bias = [] {
    if constexpr (index_storage<B>) {
        constexpr auto lo = *(detail::lower64<B> / detail::notch64<B>);
        return signed_numerator(lo);
    } else
        return imax{0};
}();
} // namespace detail

namespace detail {
// Inside values are finite and exact, so they are always ordered: <=> of two
// insides, or of an inside and an integer, is a strong_ordering whatever the
// storage; against a floating
// scalar it is a partial_ordering (NaN is unordered).
inline constexpr auto three_way = [](const auto& a, const auto& b) -> std::strong_ordering {
    const auto c = a <=> b;
    if constexpr (std::is_same_v<std::remove_const_t<decltype(c)>, std::partial_ordering>)
        return c < 0   ? std::strong_ordering::less
               : c > 0 ? std::strong_ordering::greater
                       : std::strong_ordering::equal;
    else
        return c;
};
inline constexpr auto three_way_partial = [](const auto& a, const auto& b) -> std::partial_ordering {
    return a <=> b;
};
inline constexpr auto equal_to = [](const auto& a, const auto& b) { return a == b; };

// A point P that is one of X's slots, with X storing a builtin index: X's
// raw orders like its value, so X ⋈ P is its raw ⋈ that constant slot
// (`t != end(t)`, `x <= 4_ins`).
template <insidable X, insidable P>
inline constexpr bool point_is_slot =
    integer_index_storage<X> && !point_grid<X> && point_grid<P> && grid_of<X>.representable(lower_of<P>);
template <insidable X, insidable P>
inline constexpr raw_t<X> point_slot_of =
    static_cast<raw_t<X>>(exact_quotient(grid_sub(lower_of<P>, lower_of<X>), notch_of<X>));

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
    // a wide-index operand: exact wide fractions
    else if constexpr (wide_valued<L> || wide_valued<R>)
        return cmp(exact_of(lhs), exact_of(rhs));
    // both integer value raws (notch 1, Raw == value): compare as integers
    else if constexpr (integer_value_storage<L> && integer_value_storage<R> && values_fit_imax<L> &&
                       values_fit_imax<R>)
        return cmp(raw_imax(lhs), raw_imax(rhs));
    // same nonzero notch, integer-backed: compare signed value indices
    // (compile-time bias + raw) — e.g. two same-Q-format fixed-point types
    // with different intervals, without the rational decode.
    else if constexpr (detail::notch64<L> == detail::notch64<R> && index_cmp_fits<L> && index_cmp_fits<R>)
        return cmp(index_cmp_bias<L> + raw_imax(lhs), index_cmp_bias<R> + raw_imax(rhs));
    else
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
        constexpr umax cap       = static_cast<umax>(std::numeric_limits<imax>::max());
        constexpr umax notch_num = detail::notch64<B>.Numerator;
        constexpr umax notch_den = static_cast<umax>(detail::notch64<B>.Denominator); // Notch > 0
        constexpr umax index_mag = [] {
            constexpr auto lo = *(detail::lower64<B> / detail::notch64<B>);
            constexpr auto hi = *(detail::upper64<B> / detail::notch64<B>);
            return lo.Numerator > hi.Numerator ? lo.Numerator : hi.Numerator;
        }();
        constexpr umax scalar_mag = [] {
            umax mag = static_cast<umax>(std::numeric_limits<A>::max());
            if constexpr (std::signed_integral<A>) {
                umax min_mag = static_cast<umax>(-(std::numeric_limits<A>::min() + 1)) + 1;
                if (min_mag > mag)
                    mag = min_mag;
            }
            return mag;
        }();
        umax product;
        return !mul_overflow(index_mag, notch_num, &product) && product <= cap &&
               !mul_overflow(scalar_mag, notch_den, &product) && product <= cap;
    }
}();
} // namespace detail

namespace detail {
// inside ⋈ arithmetic scalar. Integer storage compares as integers when the
// scalar's type fits imax, and in double when it is floating point and the
// grid's values are exact in double; everything else goes through rational.
template <insidable B, arithmetic A, class Cmp>
constexpr auto compare_scalar(const B& lhs, A rhs, Cmp cmp) {
    constexpr bool imax_scalar = std::signed_integral<A> || (std::unsigned_integral<A> && sizeof(A) < sizeof(imax));
    constexpr bool double_exact_values =
        lower_of<B> >= rational{-(imax{1} << 53)} && upper_of<B> <= rational{imax{1} << 53};
    if constexpr (wide_valued<B>) {
        if constexpr (std::floating_point<A>) {
            if (rhs == rhs && !(rhs - rhs == 0)) // ±inf lies past every grid
                return cmp(exact_of(0), exact_of(rhs < 0 ? -1 : 1));
            if (rhs == rhs)
                return cmp(exact_of(lhs), exact_of_double(static_cast<double>(rhs)));
        }
        return cmp(exact_of(lhs), exact_of(as_rational(rhs)));
    } else if constexpr (integer_value_storage<B> && values_fit_imax<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
    else if constexpr (integer_value_storage<B> && values_fit_imax<B> && std::floating_point<A> && double_exact_values)
        return cmp(static_cast<double>(raw_imax(lhs)), static_cast<double>(rhs));
    else if constexpr (integer_index_storage<B> && std::floating_point<A> && double_exact<grid_of<B>> &&
                       index_double<B>.Ok)
        return cmp(as_double(lhs), static_cast<double>(rhs)); // exact: every value is a double
    else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(detail::notch64<B>.Numerator),
                   static_cast<imax>(rhs) * detail::notch64<B>.Denominator);
    else {
        // |rhs| ≥ 2^64 (or infinite) has no rational form, and every grid value
        // lies strictly inside ±2^64: the sign of rhs decides.
        if constexpr (std::floating_point<A>)
            if (rhs == rhs && !(rhs < 0x1p64 && rhs > -0x1p64))
                return cmp(rational{0}, rational{rhs < 0 ? -1 : 1});
        return cmp(as_rational(lhs), rational{rhs});
    }
}
} // namespace detail

template <insidable B, detail::arithmetic A>
[[nodiscard]] constexpr auto operator<=>(const B& lhs, A rhs) {
    if constexpr (std::floating_point<A>)
        return detail::compare_scalar(lhs, rhs, detail::three_way_partial);
    else
        return detail::compare_scalar(lhs, rhs, detail::three_way);
}

template <insidable B, detail::arithmetic A>
[[nodiscard]] constexpr bool operator==(const B& lhs, A rhs) {
    return detail::compare_scalar(lhs, rhs, detail::equal_to);
}

//---------------------------------------------------------------------------
// just
//---------------------------------------------------------------------------
// The point grid holds the value, so no value constructor is needed: grid
// numbers past 64 bits (C++26 `_g`) work too.
template <auto value>
inline constexpr auto just = inside<grid{value}>::from_raw({});

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
    return grid{interval{lower_of<T>, grid_add(upper_of<T>, s)}, s};
}

consteval policy_flag cursor_policy(policy_flag p) { return p | cursor_marker; }
} // namespace detail

template <insidable T, auto Step = notch_of<T>>
using cursor = inside<detail::cursor_grid<T>(Step), detail::cursor_policy(policy_of<T>)>;

// end(t): a cursor's past-the-end value, one step past T's Upper. Only a
// cursor has one — on any other inside its Upper is a value, not an end.
template <insidable B>
    requires(has_flag(policy_of<B>, detail::cursor_marker))
[[nodiscard]] constexpr auto end(const B&) noexcept {
    return just<upper_of<B>>;
}

//---------------------------------------------------------------------------
// zero / one — universal exact constants. Single-point insides that assign into
// any grid able to represent the value (compile-time checked) and otherwise
// behave as 0 / 1. `b = zero;` is a compile error when 0 is not on b's grid.
//---------------------------------------------------------------------------
inline constexpr auto zero = just<0>;
inline constexpr auto one  = just<1>;

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
// rational.hpp. `-1.5_ins` parses as `-(1.5_ins)`.
//---------------------------------------------------------------------------
template <char... Chars>
constexpr auto operator""_ins() {
    return just<detail::parse_ins_literal<Chars...>()>;
}

} // namespace beman::inside


// ======================================================================
//  beman/inside/casts.hpp
// ======================================================================


//---------------------------------------------------------------------------
// Free-function casts complementing the constructors. Unlike a direct B{value}
// call, these read naturally in algorithm callbacks and make the intent (clamp
// vs. wrap vs. throw vs. trust) explicit at the call site.
//---------------------------------------------------------------------------
namespace beman::inside {
// Each cast constructs via the value+policy constructor, passing a one-shot
// policy that overrides B's declared one for this conversion only.
template <insidable B, numeric N>
[[nodiscard]] constexpr B clamp_cast(N value) {
    return B{value, make_policy<clamp>()};
}

// `wrap_cast` — modular semantics: the input is reduced into the target grid's
// interval rather than clipped. For integer-style wraparound (angles, indices).
template <insidable B, numeric N>
[[nodiscard]] constexpr B wrap_cast(N value) {
    return B{value, make_policy<wrap>()};
}

//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round — compose `clamp` with a rounding
// mode: the canonical "double in, bounded integer out, never throw" pipeline.
//---------------------------------------------------------------------------
namespace detail {
template <insidable B, policy_flag RoundMode, numeric N>
[[nodiscard]] constexpr B clamp_with_rounding(N value) {
    return B{value, make_policy<clamp | RoundMode>()};
}
} // namespace detail

template <insidable B, numeric N>
[[nodiscard]] constexpr B clamp_floor(N value) {
    return detail::clamp_with_rounding<B, round_floor>(value);
}

template <insidable B, numeric N>
[[nodiscard]] constexpr B clamp_ceil(N value) {
    return detail::clamp_with_rounding<B, round_ceil>(value);
}

template <insidable B, numeric N>
[[nodiscard]] constexpr B clamp_round(N value) {
    return detail::clamp_with_rounding<B, round_nearest>(value);
}

// `checked_cast` — throws (via the installed handler) when the value would not
// fit exactly: errc::overflow out of the interval (as to<T> and the predicate
// name it), errc::rounding_error off the notch. Any numeric source, insides
// included; once both checks pass the store is exact.
template <insidable B, numeric A>
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
[[nodiscard]] constexpr B unchecked_cast(A value) {
    using twin = inside<grid_of<B>, unsafe>;
    return B::from_raw(twin{value}.raw()); // same grid → identical raw layout
}

} // namespace beman::inside


// ======================================================================
//  beman/inside/arithmetic.hpp
// ======================================================================



//---------------------------------------------------------------------------
// Free-function arithmetic — wraps detail::addition/multiplication/division/
// modulo with caller-friendly overloads:
//   add(l, r) / add(l, r, policy<F>{}) / add(l, r, on_overflow(λ)) /
//   add(l, r, ec) / l + r
// Plus the variadic folds add_all/mul_all and *_into<Target>, and the
// std::expected operator overloads (an error operand propagates through).
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// add / sub / mul / div / mod — each takes one of three trailing forms:
//   op(l, r [, policy [, action]])   explicit per-call policy (+ action)
//   op(l, r, on_overflow(λ), ...)    tagged actions (policy = their implied flags)
//   op(l, r, ec [, action])          error-code form (checked, reports into ec)
// detail::arith normalises the form to (policy, action) for the op's core.
//---------------------------------------------------------------------------
namespace detail {
// Arithmetic fires only on_overflow; any other action would be silently
// ignored, so it is rejected.
template <class A>
inline constexpr bool arith_action =
    std::same_as<std::remove_cvref_t<A>, no_action> || overflow_action<std::remove_cvref_t<A>>;

template <class Op, class L, class R, policy_like P = policy<>, class A = no_action>
constexpr auto arith(Op op, const L& l, const R& r, P&& pol = {}, A&& act = {}) {
    static_assert(arith_action<A>,
                  "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
                  "apply to assignment — use them with b.with(...) or a cast");
    return op(l, r, std::forward<P>(pol), std::forward<A>(act));
}

// Action-first form: on_overflow actions (the only kind arithmetic fires;
// any other tag is rejected below with a message, not a bare mismatch).
template <class A>
inline constexpr bool is_action_tag =
    is_overflow_action<A>::value || is_clamp_action<A>::value || is_wrap_action<A>::value || is_error_action<A>::value;

template <class Op, class L, class R, class... Actions>
    requires(sizeof...(Actions) >= 1) && (is_action_tag<std::remove_cvref_t<Actions>> && ...)
constexpr auto arith(Op op, const L& l, const R& r, Actions&&... acts) {
    static_assert((overflow_action<std::remove_cvref_t<Actions>> && ...),
                  "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
                  "apply to assignment — use them with b.with(...) or a cast");
    return op(l, r, make_policy<merged_implied_flags<Actions...>>(), pick_action<is_overflow_action>(acts...));
}

template <class Op, class L, class R, class A = no_action>
constexpr auto arith(Op op, const L& l, const R& r, errc& ec, A&& act = {}) {
    static_assert(arith_action<A>,
                  "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
                  "apply to assignment — use them with b.with(...) or a cast");
    return op(l, r, make_policy<checked>(ec), std::forward<A>(act));
}

template <class P>
inline constexpr policy_flag flags_of = policy_flags_of<std::remove_cvref_t<P>>;

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
} // namespace detail

#define BEMAN_INSIDE_ARITH_FN(name, op)                                             \
    template <insidable L, insidable R, class... Args>                              \
        requires requires(L const& l, R const& r, Args&&... args) {                 \
            detail::arith(detail::op{}, l, r, std::forward<Args>(args)...);         \
        }                                                                           \
    [[nodiscard]] constexpr auto name(L const& lhs, R const& rhs, Args&&... args) { \
        return detail::arith(detail::op{}, lhs, rhs, std::forward<Args>(args)...);  \
    }

BEMAN_INSIDE_ARITH_FN(add, add_op)
BEMAN_INSIDE_ARITH_FN(sub, sub_op)
BEMAN_INSIDE_ARITH_FN(mul, mul_op)
BEMAN_INSIDE_ARITH_FN(div, div_op)
BEMAN_INSIDE_ARITH_FN(mod, mod_op)
#undef BEMAN_INSIDE_ARITH_FN

//---------------------------------------------------------------------------
// mul_into<Out> / div_into<Out> — the exact product or quotient, rounded once
// onto Out by Out's policy (plus the per-call flags F). Neither forms the
// product or quotient grid, so they work where `a * b` would need grid numbers
// past 64 bits (C++23) or a quotient past the 64-bit fraction. A store that
// fails is reported through the policy (it throws under `checked`).
// div_into returns std::expected<Out, errc> when the divisor's grid holds 0 —
// then every failure, a zero divisor or a failed store, is the error — else Out.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Out, std::size_t K, typename P>
constexpr Out store_exact_into(const exact_frac<K>& v, P&& policy) {
    Out out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
}
} // namespace detail

template <insidable Out, policy_flag F = none, insidable A, insidable B>
[[nodiscard]] constexpr Out mul_into(const A& a, const B& b) {
    using E = detail::exact_frac<detail::exact_limbs<A, B, Out>>;
    return detail::store_exact_into<Out>(E{detail::exact_of(a)} * E{detail::exact_of(b)},
                                         make_policy<policy_of<Out> | F>());
}

template <insidable Out, policy_flag F = none, insidable A, insidable B>
[[nodiscard]] constexpr auto div_into(const A& a, const B& b)
    -> std::conditional_t<detail::divisor_excludes_zero<B>, Out, std::expected<Out, errc>> {
    using E     = detail::exact_frac<detail::exact_limbs<A, B, Out>>;
    const E num = E{detail::exact_of(a)}, den = E{detail::exact_of(b)};
    if constexpr (detail::divisor_excludes_zero<B>)
        return detail::store_exact_into<Out>(num / den, make_policy<policy_of<Out> | F>());
    else {
        if (den.Num.is_zero())
            return std::unexpected{errc::division_by_zero};
        errc      ec{};
        const Out out = detail::store_exact_into<Out>(num / den, make_policy<policy_of<Out> | F>(ec));
        if (ec != errc{})
            return std::unexpected{ec};
        return out;
    }
}

// Binary operators: +, -, * use the default policy; / and % carry the
// operands' own policies (snap/rounding select the native integer paths).
[[nodiscard]] constexpr auto operator+(insidable auto lhs, insidable auto rhs) { return beman::inside::add(lhs, rhs); }

[[nodiscard]] constexpr auto operator-(insidable auto lhs, insidable auto rhs) { return beman::inside::sub(lhs, rhs); }

[[nodiscard]] constexpr auto operator*(insidable auto lhs, insidable auto rhs) { return beman::inside::mul(lhs, rhs); }

[[nodiscard]] constexpr auto operator/(insidable auto lhs, insidable auto rhs) {
    return beman::inside::div(lhs, rhs, make_policy<policy_of<decltype(lhs)> | policy_of<decltype(rhs)>>());
}

[[nodiscard]] constexpr auto operator%(insidable auto lhs, insidable auto rhs) {
    return beman::inside::mod(lhs, rhs, make_policy<policy_of<decltype(lhs)> | policy_of<decltype(rhs)>>());
}

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
    auto   policy = make_policy<policy_of<Target>>();

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
                if constexpr (std::ranges::random_access_range<Rng>) {
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
                if (const auto v = detail::rational{static_cast<imax>(total)} * detail::to_rational(U)) {
                    detail::assignment<Target, detail::rational>::assign(out, *v, policy, no_action{});
                    return out;
                }
        }
        constexpr std::size_t K =
            detail::limbs_for_bits(I::bits + detail::exact_value_bits<B> + detail::exact_value_bits<Target>);
        using W = detail::wide_sint<K>;
        const detail::exact_frac<K> v{static_cast<W>(total) * static_cast<W>(detail::wide_numerator(U)),
                                      static_cast<W>(detail::wide_denominator(U))};
        no_action                   none;
        detail::assign_exact<detail::rational>(out, v, policy, none);
    } else {
        detail::rational total{0};
        for (const auto& b : r) {
            const auto s = total + detail::as_rational(b);
            if (!s) [[unlikely]] {
                no_action none;
                detail::report_failure(out, policy, none, s.error());
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
// Backs the
// std::common_type specialisation (numeric_limits.hpp) and mixed-grid
// min/max below. The primary has no `type` when the hull grid is
// unrepresentable, so common_type_t SFINAEs away instead of erroring.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Lhs, insidable Rhs>
struct common_inside {};

// Same type stays itself (policy included) — mirrors std::common_type<T, T>.
template <insidable Same>
struct common_inside<Same, Same> {
    using type = Same;
};

template <insidable Lhs, insidable Rhs>
    requires(!std::same_as<Lhs, Rhs>) && (hull(grid_of<Lhs>, grid_of<Rhs>).has_value())
struct common_inside<Lhs, Rhs> {
    static constexpr grid hull_grid = *hull(grid_of<Lhs>, grid_of<Rhs>);
    using type                      = inside<hull_grid>;
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
    return (a + b) * just<frac<1, 2>>;
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
template <class L, class R>
concept expected_operands =
    (is_expected_v<L> || is_expected_v<R>) && (insidable<unwrap_t<L>> || insidable<unwrap_t<R>>);
}

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
concept raw_scalar = std::integral<A> || std::floating_point<A>;

#define BEMAN_INSIDE_SCALAR_MSG                                                      \
    "an inside cannot be combined with a raw scalar: give the scalar a grid — "      \
    "`1_ins`, `just<1>`, `one`, or `inside<{lo,hi}>{n}` for a runtime value with a " \
    "known range"
#define BEMAN_INSIDE_NO_SCALAR(op)                                          \
    template <insidable B, raw_scalar A>                                    \
    B operator op(B const&, A) {                                            \
        static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); \
    }                                                                       \
    template <raw_scalar A, insidable B>                                    \
    B operator op(A, B const&) {                                            \
        static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); \
    }                                                                       \
    template <insidable B, raw_scalar A>                                    \
    B& operator op## = (B&, A) {                                            \
        static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); \
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
// without opting into numeric_limits.hpp. SFINAE-friendly: no `type` when the
// hull grid is unrepresentable.
template <beman::inside::grid G1, beman::inside::policy_flag P1, beman::inside::grid G2, beman::inside::policy_flag P2>
struct std::common_type<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>>
    : beman::inside::detail::common_inside<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>> {};


// ======================================================================
//  beman/inside/range.hpp
// ======================================================================



//---------------------------------------------------------------------------
// inside_range — random-access range over a grid. Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. Models random_access_range + sized_range (so std::ranges algorithms
// work directly). iterator_category is input_iterator_tag because operator*
// returns by value; iterator_concept carries the random-access capability.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// enumerate_view — C++20 stand-in for std::views::enumerate (C++23), yielding
// pair<index, value> by value (all indexed() needs).
template <class R>
struct enumerate_view {
    R Base;

    struct iterator {
        std::ranges::iterator_t<const R> It{};
        std::size_t                      Index{0};

        using value_type      = std::pair<std::size_t, std::ranges::range_value_t<R>>;
        using difference_type = std::ptrdiff_t;

        [[nodiscard]] constexpr value_type operator*() const { return {Index, *It}; }
        constexpr iterator&                operator++() {
            ++It;
            ++Index;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto t = *this;
            ++*this;
            return t;
        }
        [[nodiscard]] constexpr bool operator==(const iterator& o) const { return It == o.It; }
    };

    constexpr iterator begin() const { return {std::ranges::begin(Base), 0}; }
    constexpr iterator end() const { return {std::ranges::end(Base), 0}; }
};

// stride_view — stand-in for std::views::stride (likewise). Visits every
// `step`-th element; forward-only, and the advance checks `end` so a length
// that isn't a multiple of the stride still terminates.
template <class R>
struct stride_view {
    R           Base;
    std::size_t Step{1};

    struct iterator {
        std::ranges::iterator_t<const R> It{};
        std::ranges::iterator_t<const R> End{};
        std::size_t                      Step{1};

        using value_type      = std::ranges::range_value_t<R>;
        using difference_type = std::ptrdiff_t;

        [[nodiscard]] constexpr value_type operator*() const { return *It; }
        constexpr iterator&                operator++() {
            for (std::size_t k = 0; k < Step && It != End; ++k)
                ++It;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto t = *this;
            ++*this;
            return t;
        }
        [[nodiscard]] constexpr bool operator==(const iterator& o) const { return It == o.It; }
    };

    constexpr iterator begin() const { return {std::ranges::begin(Base), std::ranges::end(Base), Step}; }
    constexpr iterator end() const { return {std::ranges::end(Base), std::ranges::end(Base), Step}; }
};
} // namespace detail

template <grid G, policy_flag P = checked>
    requires(G.Notch != 0)
struct inside_range {
    using value_type = inside<G, P>;
    static_assert(G.max_index_representable() && detail::max_index_v<value_type> < std::numeric_limits<umax>::max(),
                  "inside_range: the grid has more points than a 64-bit count can hold");
    static constexpr umax slot_count = detail::max_index_v<value_type> + 1;

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

    umax StartIndex;

    constexpr inside_range() : StartIndex{0} {}

    constexpr inside_range(value_type start) {
        // Map a grid value back to its notch index: the raw's offset.
        StartIndex = static_cast<umax>(start.raw()) - static_cast<umax>(detail::raw_lo<value_type>);
    }

    constexpr iterator begin() const { return {StartIndex, 0}; }
    constexpr iterator end() const { return {StartIndex, static_cast<imax>(slot_count)}; }

    constexpr std::size_t size() const { return slot_count; }

    // `indexed()` pairs each value with its zero-based position (≈ C++23
    // std::views::enumerate), via detail::enumerate_view for C++20.
    constexpr auto indexed() const { return detail::enumerate_view<inside_range>{*this}; }

    // `strided(step)` visits every `step`-th grid value (≈ C++23 std::views::
    // stride). `std::views::reverse` already works directly, so there's no reverse().
    constexpr auto strided(std::size_t step) const { return detail::stride_view<inside_range>{*this, step}; }
};

} // namespace beman::inside



// ======================================================================
//  beman/inside/cmath.hpp
// ======================================================================


// ======================================================================
//  beman/inside/cmath_adaptive.hpp
// ======================================================================


// ======================================================================
//  beman/inside/detail/math_adaptive.hpp
// ======================================================================




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
// Transcendental results land exactly on a boundary only at a few exact
// inputs (exp(0), log(1), ...); the cores return those with Error 0, so for
// every other input the escalation ends. A cap stops it in any case; past the
// cap the result is the slot nearest Y.
//
// Everything here is constexpr and integer-only (wide_int), so a result is the
// same at compile time, at runtime, with or without an FPU.
//---------------------------------------------------------------------------
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
using double_t = wide_sint<2 * K>;

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
    } else if constexpr (K == 2) {
        // Two limbs: the four partial products into four limbs, then the shift.
        const bool             na = a.negative(), nb = b.negative();
        const wide_uint<2>     ma{na ? -a : a}, mb{nb ? -b : b};
        const limb::pair<umax> p00 = limb::mul(ma.Word[0], mb.Word[0]);
        const limb::pair<umax> p01 = limb::mul(ma.Word[0], mb.Word[1]);
        const limb::pair<umax> p10 = limb::mul(ma.Word[1], mb.Word[0]);
        const limb::pair<umax> p11 = limb::mul(ma.Word[1], mb.Word[1]);
        umax                   c1 = 0, c2 = 0, c3 = 0, c4 = 0;
        const umax             w0 = p00.Lo;
        umax                   w1 = limb::add_carry(p00.Hi, p01.Lo, c1);
        w1                        = limb::add_carry(w1, p10.Lo, c2);
        umax w2                   = limb::add_carry(p11.Lo, p01.Hi, c3);
        w2                        = limb::add_carry(w2, p10.Hi, c4);
        umax c5                   = 0;
        w2                        = limb::add_carry(w2, c1 + c2, c5);
        const umax        w3      = p11.Hi + c3 + c4 + c5;
        const umax        w[4]    = {w0, w1, w2, w3};
        const std::size_t ws      = static_cast<std::size_t>(W / 64);
        const int         bs      = W % 64;
        wide_uint<2>      m;
        for (std::size_t i = 0; i < 2; ++i) {
            const umax lo = i + ws < 4 ? w[i + ws] : 0;
            const umax hi = i + ws + 1 < 4 ? w[i + ws + 1] : 0;
            m.Word[i]     = bs == 0 ? lo : (lo >> bs) | (hi << (64 - bs));
        }
        const wide_sint<2> r{m};
        return na != nb ? -r : r;
    } else {
        // Schoolbook on the magnitudes into 2K limbs, then the shift.
        const bool         na = a.negative(), nb = b.negative();
        const wide_uint<K> ma{na ? -a : a}, mb{nb ? -b : b};
        umax               prod[2 * K]{};
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

// v / d for a small positive d, truncating toward zero (series terms).
template <std::size_t K>
constexpr wide_sint<K> div_small(const wide_sint<K>& v, umax d) noexcept;

// a / b at scale 2^W. Pre: b != 0.
template <std::size_t K>
constexpr wide_sint<K> div_q(const wide_sint<K>& a, const wide_sint<K>& b, int W) noexcept {
    using D = double_t<K>;
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
constexpr wide_sint<K> nearest_div(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    if (bit_width_of(d) <= 64) {
        const umax dd     = d.Word[0];
        const bool neg    = n.negative();
        const auto [q, r] = divmod_small(wide_uint<K>{neg ? -n : n}, dd);
        wide_sint<K> m{q};
        if (r >= dd - r)
            m += wide_sint<K>{1}; // 2r ≥ d
        return neg ? -m : m;
    }
    return rounded_div<round_mode::nearest>(n, d);
}

// x·2^W rounded to the nearest integer (half away from zero): within ½ unit.
template <int W, std::size_t K, std::size_t E>
constexpr wide_sint<K> to_q(const exact_frac<E>& x) noexcept {
    constexpr int shift = W >= 0 ? W : -W;
    using I             = wide_sint<exact_max<E, K> + limbs_for_bits(shift) + 1>;
    I n{x.Num}, d{x.Den};
    if constexpr (W >= 0)
        n = n << W;
    else
        d = d << shift;
    return static_cast<wide_sint<K>>(nearest_div(n, d));
}

// ⌊log2 |x|⌋ for x != 0.
template <std::size_t E>
constexpr int floor_log2(const exact_frac<E>& x) noexcept {
    using I    = wide_sint<E>;
    const I  n = x.Num.negative() ? -x.Num : x.Num;
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
        const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
        const int       b = bit_width_of(q) - bit_width_of(p) + 2;
        return b > 0 ? b : 0;
    }
}();

template <insidable Out>
inline constexpr int mag_bits = grid_magnitude_bits<Out> > 0 ? grid_magnitude_bits<Out> : 0;

//---------------------------------------------------------------------------
// approx — a core's result: the true value lies in
// [(Value − Error)·2^-Scale, (Value + Error)·2^-Scale]. Error 0 means exact.
//---------------------------------------------------------------------------
template <std::size_t K>
struct approx {
    wide_sint<K> Value;
    int          Scale;
    umax         Error;
};

//---------------------------------------------------------------------------
// decide — the slot offset of an approx on Out's grid under Out's rounding
// mode, when both ends of its error interval round to the same slot.
//---------------------------------------------------------------------------
template <insidable Out>
inline constexpr round_mode out_rounding = rounding_of(policy_of<Out>);

// Limbs for the exact value Y/2^W of a K-limb approx, with room for the
// product with Out's notch in exact_index.
template <insidable Out, std::size_t K>
inline constexpr std::size_t decide_limbs = K + 1 + exact_limbs<Out>;

template <insidable Out, std::size_t K>
struct decision {
    bool                            Decided;
    wide_sint<decide_limbs<Out, K>> Index; // Out's slot offset (may be out of range)
};

template <insidable Out, std::size_t K>
constexpr exact_frac<decide_limbs<Out, K>> scaled_value(const wide_sint<K>& y, int W) noexcept {
    using I = wide_sint<decide_limbs<Out, K>>;
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
        const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
        return !(grid_wide{std::numeric_limits<umax>::max()} < p) &&
               !(grid_wide{std::numeric_limits<umax>::max()} < q);
    }
}();

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
    umax                  a[N]{};
    umax                  carry = 0;
    for (std::size_t i = 0; i < K; ++i) {
        const limb::pair<umax> t = limb::mul(mag.Word[i], q);
        umax                   c = 0;
        a[i]                     = limb::add_carry(t.Lo, carry, c);
        carry                    = t.Hi + c;
    }
    a[K]     = carry;
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
    umax              hi[N]{};
    const std::size_t ws = static_cast<std::size_t>(S / 64);
    const int         bs = S % 64;
    for (std::size_t i = 0; i + ws < N; ++i) {
        const umax lo = a[i + ws];
        const umax up = i + ws + 1 < N ? a[i + ws + 1] : 0;
        hi[i]         = bs == 0 ? lo : (lo >> bs) | (up << (64 - bs));
    }
    umax r = 0;
    if constexpr (p != 1)
        for (std::size_t i = N; i-- > 0;) {
            const limb::pair<umax> d = limb::div(r, hi[i], p);
            hi[i]                    = d.Hi;
            r                        = d.Lo;
        }
    J m{0};
    for (std::size_t i = 0; i < N; ++i)
        m.Word[i] = hi[i];
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
        return static_cast<I>(exact_index<Out, M>(scaled_value<Out>(y, a.Scale)).Index);
    };
    if (a.Error == 0)
        return {true, index(a.Value)};
    const I lo = index(a.Value - e), hi = index(a.Value + e);
    // Both ends past the same end of Out's range also decide: the policy
    // (clamp, or an overflow report) sees the same thing for every value there.
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
    return static_cast<I>(exact_index<Out, out_rounding<Out>>(scaled_value<Out>(a.Value, a.Scale)).Index);
}

//---------------------------------------------------------------------------
// store — Out at slot offset `index`. In range it is a raw; out of range the
// grid point goes through the policy cascade (clamp, wrap or report), as any
// assignment. Policy is Out's, optionally with an errc sink.
//---------------------------------------------------------------------------
template <insidable Out, std::size_t K, typename P>
constexpr Out store(const wide_sint<K>& index, P&& policy) {
    using I = wide_sint<K>;
    if constexpr (integer_storage<Out>) {
        constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
        if (!index.negative() && !(count < index)) [[likely]]
            return Out::from_raw(raw_of_slot<Out>(index));
    }
    // Past Out's range: the grid point (index + Lower/Notch)·Notch, exact,
    // through Out's assignment (its policy clamps, wraps or reports).
    constexpr std::size_t KK = exact_max<K, exact_limbs<Out>>;
    using J                  = wide_sint<KK>;
    const exact_frac<KK> v{(J{index} + static_cast<J>(slot_base<Out>)) * static_cast<J>(wide_numerator(notch_of<Out>)),
                           static_cast<J>(wide_denominator(notch_of<Out>))};
    Out                  out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
}

template <insidable Out, std::size_t K>
constexpr Out store(const wide_sint<K>& index) {
    return store<Out>(index, make_policy<policy_of<Out>>());
}

// An exact value through Out's policy (an output without slots, or an exact
// result: rounding an exact value by the mode is correct rounding).
template <insidable Out, std::size_t K, typename P>
constexpr Out store_exact(const exact_frac<K>& v, P&& policy) {
    // At least the exact paths' minimum width (try_rational's 64-bit bounds
    // need it).
    const exact_frac<exact_max<K, exact_min_limbs>> w{v};
    Out                                             out{};
    assign_exact<rational>(out, w, policy, no_action{});
    return out;
}

// An approx's midpoint as an exact value.
template <std::size_t K>
constexpr auto midpoint(const approx<K>& a) noexcept {
    constexpr std::size_t KK = exact_max<K + 1, exact_min_limbs>;
    using I                  = wide_sint<KK + 8>;
    return a.Scale >= 0 ? exact_frac<KK + 8>{I{a.Value}, I{1} << a.Scale}
                        : exact_frac<KK + 8>{I{a.Value} << (-a.Scale), I{1}};
}

//---------------------------------------------------------------------------
// evaluate — the Ziv driver. Core is a callable object with a member
// template `run<W>()` returning an approx within about 2^-W, and optionally
// `exact()` returning a std::optional exact_frac when the result is an
// exact rational (taken as is). It starts at W0 and doubles W while the
// result is undecided, up to Cap.
//---------------------------------------------------------------------------
template <insidable Out, int W, int Cap, typename Core, typename P>
constexpr Out evaluate_from(const Core& core, P&& policy) {
    const auto a = core.template run<W>();
    if constexpr (!notched<Out>) {
        // A 64-bit value: keep the denominator a 64-bit power of two.
        constexpr int A = 60 - mag_bits<Out> > 1 ? 60 - mag_bits<Out> : 1;
        if (a.Scale <= A)
            return store_exact<Out>(midpoint(a), policy);
        using I        = decltype(a.Value);
        const int sh   = a.Scale - A;
        const I   half = I{1} << (sh - 1);
        const I   v    = a.Value.negative() ? -((-a.Value + half) >> sh) : (a.Value + half) >> sh;
        return store_exact<Out>(midpoint(approx<limbs_of<I>>{v, A, 0}), policy);
    } else {
        if (wide_sint<limbs_of<decltype(a.Value)> + 2> j; quick_slot<Out>(a, j)) [[likely]]
            return store<Out>(j, policy);
        const auto d = decide<Out>(a);
        if (d.Decided)
            return store<Out>(d.Index, policy);
        if constexpr (2 * W > Cap)
            return store<Out>(nearest_index<Out>(a), policy);
        else
            return evaluate_from<Out, 2 * W, Cap>(core, policy);
    }
}

// The precision cap for a start precision W0: two doublings. A result still
// undecided at 4·W0 lies within 2^-4W0 of a rounding boundary — for an
// irrational value that is the rare case the cap gives up on (nearest slot).
constexpr int precision_cap(int W0) noexcept { return 4 * W0; }

template <insidable Out, int W0, typename Core, typename P>
constexpr Out evaluate(const Core& core, P&& policy) {
    if constexpr (requires { core.exact(); })
        if (const auto e = core.exact())
            return store_exact<Out>(*e, policy);
    return evaluate_from<Out, W0, precision_cap(W0)>(core, policy);
}

//---------------------------------------------------------------------------
// The table tier: for an input with few slots, every result is computed at
// compile time (the same driver, so the same correctly rounded slots) and a
// call is one load. Only when every result lands inside Out's range — a
// result the policy would clamp, wrap or report keeps the computed path.
// BEMAN_INSIDE_MATH_TABLE_SLOTS sets the largest input (0 turns tables off).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_TABLE_SLOTS
    #define BEMAN_INSIDE_MATH_TABLE_SLOTS 256
#endif

// Out's slot offset for core's result (escalating as evaluate does), or
// −1 when it falls outside Out's range.
template <insidable Out, int W, int Cap, typename Core>
constexpr imax slot_from(const Core& core) {
    const auto a        = core.template run<W>();
    auto       in_range = [](const auto& i) -> imax {
        using I           = std::remove_cvref_t<decltype(i)>;
        constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
        return (i.negative() || count < i) ? imax{-1} : static_cast<imax>(i);
    };
    if (wide_sint<limbs_of<decltype(a.Value)> + 2> j; quick_slot<Out>(a, j))
        return in_range(j);
    const auto d = decide<Out>(a);
    if constexpr (2 * W <= Cap)
        if (!d.Decided)
            return slot_from<Out, 2 * W, Cap>(core);
    return in_range(d.Decided ? d.Index : nearest_index<Out>(a));
}

template <insidable Out, int W0, typename Core>
constexpr imax slot_of(const Core& core) {
    if constexpr (requires { core.exact(); })
        if (const auto e = core.exact()) {
            const auto i      = exact_index<Out, out_rounding<Out>>(*e).Index;
            using I           = std::remove_cvref_t<decltype(i)>;
            constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
            return (i.negative() || count < i) ? imax{-1} : static_cast<imax>(i);
        }
    return slot_from<Out, W0, precision_cap(W0)>(core);
}

template <insidable In>
inline constexpr bool table_input =
    !wide_valued<In> && notched<In> && grid_of<In>.slot_count() < grid_wide{BEMAN_INSIDE_MATH_TABLE_SLOTS};

// Outputs whose raw a table can hold: an integer index or value.
template <insidable Out>
inline constexpr bool table_output = notched<Out> && !wide_valued<Out>;

// The slot offset of an input value (0 … slot count).
template <insidable In>
constexpr std::size_t offset_of(const In& x) noexcept {
    return static_cast<std::size_t>(value_index<imax>(x) - static_cast<imax>(slot_base<In>));
}

// In's value at slot I.
template <insidable In>
constexpr In slot_input(std::size_t i) noexcept {
    return In::from_raw(raw_from_offset<In>(static_cast<umax>(i)));
}

// Out's slot for In's slot I, or −1 past Out's range: one constant
// expression per entry, so each has the compiler's whole evaluation budget.
template <insidable Out, insidable In, int W0, auto MakeCore, std::size_t I>
inline constexpr imax table_slot = slot_of<Out, W0>(MakeCore(slot_input<In>(I)));

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
    static constexpr data Table = []<std::size_t... I>(std::index_sequence<I...>) {
        constexpr bool valid = ((table_slot<Out, In, W0, MakeCore, I> >= 0) && ...);
        if constexpr (!valid)
            return data{{}, false};
        else
            return data{{table_raw<Out>(table_slot<Out, In, W0, MakeCore, I>)...}, true};
    }(std::make_index_sequence<N>{});
};

template <insidable Out, int W0, typename Core>
constexpr Out evaluate(const Core& core) {
    return evaluate<Out, W0>(core, make_policy<policy_of<Out>>());
}

// The checked form: a result Out's policy reports (out of range without
// clamp) comes back as its errc instead.
template <insidable Out, int W0, typename Core>
constexpr std::expected<Out, errc> evaluate_checked(const Core& core) {
    errc      ec{};
    const Out out = evaluate<Out, W0>(core, make_policy<policy_of<Out>>(ec));
    if (ec != errc{})
        return std::unexpected{ec};
    return out;
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
// ⌊√t⌋ and ⌊∛t⌋ for one limb: Newton from above.
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

// ⌊√t⌋ for two limbs: Newton from above on 128-bit words, seeded from the
// top 62 bits; each quotient t/x ≤ √t fits one limb.
constexpr umax isqrt128(unsigned __int128 t) noexcept {
    if ((t >> 64) == 0)
        return isqrt64(static_cast<umax>(t));
    const int         bw = 128 - std::countl_zero(static_cast<umax>(t >> 64));
    const int         sh = (bw - 62 + 1) & ~1; // even, leaves 61 or 62 bits
    unsigned __int128 x  = static_cast<unsigned __int128>(isqrt64(static_cast<umax>(t >> sh)) + 1) << (sh / 2);
    for (;;) {
        const unsigned __int128 y = (x + t / x) >> 1;
        if (!(y < x))
            return static_cast<umax>(x);
        x = y;
    }
}

// ⌊√n⌋ for n ≥ 0. Two limbs or fewer directly; wider, the root of the top
// 126 bits (an even shift) seeds Newton from above with about 62 correct
// bits, each step doubling them; once they cover the root, squares
// correct the last unit (Newton from above never lands below ⌊√n⌋).
template <std::size_t K>
constexpr wide_sint<K> isqrt(const wide_sint<K>& n) noexcept {
    using I = wide_sint<K>;
    if (n.is_zero())
        return n;
    const int bw = bit_width_of(n);
    if (bw <= 64)
        return I{isqrt64(static_cast<umax>(n))};
    if constexpr (K >= 2) {
        const int               sh  = bw > 126 ? (bw - 126 + 1) & ~1 : 0;
        const I                 top = n >> sh;
        const unsigned __int128 t =
            (static_cast<unsigned __int128>(static_cast<umax>(top.Word[1])) << 64) | static_cast<umax>(top.Word[0]);
        const umax r = isqrt128(t);
        if (sh == 0)
            return I{r};
        I x = I{r} + I{1};
        x   = x << (sh / 2); // ≥ √n, within 2^-61 of it
        for (int bits = 61; bits < bw / 2 + 2; bits *= 2)
            x = (x + n / x) >> 1;
        while (n < x * x)
            x -= I{1};
        return x;
    } else
        return I{isqrt64(static_cast<umax>(n))};
}

// ⌊∛t⌋ for two limbs: Newton from above on 128-bit words, seeded from the
// top 63 bits.
constexpr umax icbrt128(unsigned __int128 t) noexcept {
    if ((t >> 64) == 0)
        return icbrt64(static_cast<umax>(t));
    const int         bw = 128 - std::countl_zero(static_cast<umax>(t >> 64));
    const int         sh = (bw - 63 + 2) / 3 * 3; // a multiple of 3, leaves at most 63 bits
    unsigned __int128 x  = static_cast<unsigned __int128>(icbrt64(static_cast<umax>(t >> sh)) + 1) << (sh / 3);
    for (;;) {
        const unsigned __int128 y = (2 * x + t / (x * x)) / 3;
        if (!(y < x))
            return static_cast<umax>(x);
        x = y;
    }
}

// ⌊∛n⌋ for n ≥ 0, as isqrt: two limbs or fewer directly; wider, the root
// of the top 126 bits (a shift by a multiple of 3) seeds Newton from above
// with about 41 correct bits, and cubes correct the last unit.
template <std::size_t K>
constexpr wide_sint<K> icbrt(const wide_sint<K>& n) noexcept {
    using I = wide_sint<K>;
    if (n.is_zero())
        return n;
    const int bw = bit_width_of(n);
    if (bw <= 64)
        return I{icbrt64(static_cast<umax>(n))};
    if constexpr (K >= 2) {
        const int               sh  = bw > 126 ? (bw - 126 + 2) / 3 * 3 : 0;
        const I                 top = n >> sh;
        const unsigned __int128 t =
            (static_cast<unsigned __int128>(static_cast<umax>(top.Word[1])) << 64) | static_cast<umax>(top.Word[0]);
        const umax r = icbrt128(t);
        if (sh == 0)
            return I{r};
        I x = I{r} + I{1};
        x   = x << (sh / 3); // ≥ ∛n, within 2^-40 of it
        for (int bits = 40; bits < bw / 3 + 2; bits *= 2)
            x = (I{2} * x + n / (x * x)) / I{3};
        while (n < x * x * x)
            x -= I{1};
        return x;
    } else
        return I{icbrt64(static_cast<umax>(n))};
}

template <std::size_t K>
constexpr bool is_zero(const exact_frac<K>& f) noexcept {
    return f.Num.is_zero();
}
template <std::size_t K>
constexpr bool is_one(const exact_frac<K>& f) noexcept {
    return f.Num == f.Den;
}
template <std::size_t K>
constexpr bool is_integer(const exact_frac<K>& f) noexcept {
    return (f.Num % f.Den).is_zero();
}
template <std::size_t K>
constexpr exact_frac<K> abs(const exact_frac<K>& f) noexcept {
    return {f.Num.negative() ? -f.Num : f.Num, f.Den};
}
// 1/f for f != 0.
template <std::size_t K>
constexpr exact_frac<K> inverse(const exact_frac<K>& f) noexcept {
    return f.Num.negative() ? exact_frac<K>{-f.Den, -f.Num} : exact_frac<K>{f.Den, f.Num};
}

// x·2^W rounded to the nearest integer, for a W known only at runtime. KI
// limbs must hold Num·2^W and Den·2^-W.
template <std::size_t K, std::size_t KI, std::size_t E>
constexpr wide_sint<K> to_q_at(const exact_frac<E>& x, int W) noexcept {
    using I = wide_sint<KI>;
    I n{x.Num}, d{x.Den};
    if (W >= 0)
        n = n << W;
    else
        d = d << (-W);
    return static_cast<wide_sint<K>>(nearest_div(n, d));
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
        Mant *= f;
        while (Mant < 0.5) {
            Mant *= 2;
            --Exp;
        }
        while (Mant >= 1) {
            Mant /= 2;
            ++Exp;
        }
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
    int              n = 0;
    for (;; ++n) {
        // the term of index n + 1 (the first one left out)
        const int    k = n + 1;
        scaled_bound u;
        if constexpr (Kind == series::exp)
            for (int i = 1; i <= k; ++i)
                u.mul(R / i);
        else if constexpr (Kind == series::sin || Kind == series::sinh)
            for (int i = 1; i <= 2 * k + 1; ++i)
                u.mul(R / i);
        else if constexpr (Kind == series::cos || Kind == series::cosh)
            for (int i = 1; i <= 2 * k; ++i)
                u.mul(R / i);
        else if constexpr (Kind == series::log1p) {
            for (int i = 0; i < k + 1; ++i)
                u.mul(R);
            u.mul(1.0 / (k + 1));
        } else {
            for (int i = 0; i < 2 * k + 1; ++i)
                u.mul(R);
            u.mul(1.0 / (2 * k + 1));
        }
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

// Σ c_k x^k by Horner at scale S.
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
    j             = j < log_tab_lo ? log_tab_lo : j > log_tab_hi ? log_tab_hi : j;
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
    using D = double_t<K>;
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

// atan(j/8) for j = 0 … 8 at scale S, within 1 unit: two half-angle steps
// t ← t/(1 + √(1+t²)) bring j/8 below 1/4, then the series with no table.
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

// log a for a ≥ 1 at scale S (so b ≥ 0), a within da units: a = m·2^b with
// m in [0.7, 1.42], log a = log m + b·ln 2.
template <int S, std::size_t K>
constexpr fx<K> log_fixed(const wide_sint<K>& a, umax da) noexcept {
    using I = wide_sint<K>;
    int b   = bit_width_of(a) - 1 - S; // 2^b ≤ a/2^S < 2^(b+1)
    I   m   = b >= 0 ? a >> b : a << (-b);
    if (mul_q(m, m, S) > (one_q<K>(S) << 1)) {
        ++b;
        m = b >= 0 ? a >> b : a << (-b);
    }
    const umax  dm = shr_bound(da, b) + 1;
    const fx<K> l  = log_series<S>(m, dm);
    return {l.Value + I{b} * static_cast<I>(ln2_q<S>), l.Error + static_cast<umax>(b < 0 ? -b : b) + 1};
}

// 1/ln 2 and 2/π at scale S (within 1 unit), for range reduction by a
// product.
template <int S>
inline constexpr fixed_t<S + 2> log2e_q = [] {
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 16)) / static_cast<I>(ln2_q<S + 16>));
}();
template <int S>
inline constexpr fixed_t<S + 2> log10e_q = [] {
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 16)) / static_cast<I>(ln10_q<S + 16>));
}();
template <int S>
inline constexpr fixed_t<S + 2> two_over_pi_q = [] {
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 17)) / static_cast<I>(pi_q<S + 16>));
}();

// e^t for t at scale S, within dt units: e^r·2^k for t = k·ln 2 + r, as an
// approx at scale S − k. k comes from t·(1/ln 2), so |r| ≤ ln 2/2 plus a
// hair (the series covers 0.36). Past KMax the value lies above 2^(KMax+1):
// returned as exactly 2^(KMax + 2), which every output with magnitude below
// 2^KMax places past its Upper. Below 2^-(S+1) it is returned as the
// interval [0, 2^-(S+1)].
template <int S, std::size_t K>
constexpr approx<K> exp_fixed(const wide_sint<K>& t, umax dt, int KMax) noexcept {
    using I   = wide_sint<K>;
    const I k = round_shift(mul_q(t, static_cast<I>(log2e_q<S>), S), S);
    if (I{KMax} < k)
        return {I{1}, -(KMax + 2), 0};
    if (k < I{-(S + 2)})
        return {I{1}, S + 2, 1};
    const fx<K> e  = exp_series<S>(t - k * static_cast<I>(ln2_q<S>), 0);
    const int   kk = static_cast<int>(static_cast<imax>(k));
    const umax  ak = static_cast<umax>(kk < 0 ? -kk : kk);
    return {e.Value, S - kk, e.Error + 2 * (dt + ak + 1)};
}
} // namespace beman::inside::math::detail::ax


// ======================================================================
//  beman/inside/detail/math_fp.hpp
// ======================================================================
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


// ======================================================================
//  beman/inside/detail/math_dd.hpp
// ======================================================================
// The double-double kernels of the math engine's dd tier (cmath_adaptive.hpp):
// values as an unevaluated sum Hi + Lo of two doubles (about 106 bits), with
// error-free sums and products (std::fma), for outputs finer than the double
// tier decides. Every constant — ln 2, π, the 2^(j/64), 2^(j/4096) and sin(jπ/128)
// tables, the Taylor coefficients — comes at compile time from the integer
// path's exact series (detail/math_adaptive.hpp), never from a generator.
// Results are within about 2^-96 relative (measured); the tier bounds them
// generously and lets the integer path decide whenever the bound does not.
//---------------------------------------------------------------------------


#ifndef BEMAN_INSIDE_MATH_NO_FP

    #include <array>
    #include <bit>
    #include <cmath> // std::fma
    #include <concepts>
    #include <cstdint>
    #include <utility>

namespace beman::inside::math::detail::dd {
using namespace ::beman::inside::detail;
namespace ax  = ::beman::inside::math::detail::ax;
namespace fpk = ::beman::inside::math::detail::fp;

using dd = fpk::hilo;

//---------------------------------------------------------------------------
// Error-free transformations and the arithmetic on them (QD-library style).
// Products use std::fma at runtime and Dekker's split at compile time.
//---------------------------------------------------------------------------
using fpk::fast_two_sum;
using fpk::fenced;
using fpk::two_sum;

constexpr dd split(double a) noexcept {
    const double c  = 134217729.0 * a; // 2^27 + 1
    const double hi = c - (c - a);
    return {hi, a - hi};
}

using fpk::rounded_product;

constexpr dd two_prod(double a, double b) noexcept {
    if consteval {
        const double p = a * b;
        const dd     x = split(a), y = split(b);
        return {p, ((x.Hi * y.Hi - p) + x.Hi * y.Lo + x.Lo * y.Hi) + x.Lo * y.Lo};
    } else {
        const double p = rounded_product(a, b);
        return {p, std::fma(a, b, -p)};
    }
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

constexpr dd ldexp(dd a, int e) noexcept {
    return {::beman::inside::detail::ldexp(a.Hi, e), ::beman::inside::detail::ldexp(a.Lo, e)};
}

//---------------------------------------------------------------------------
// Constants from the integer path's fixed-point values Y·2^-S.
//---------------------------------------------------------------------------
template <std::size_t K>
constexpr dd of_fixed(wide_sint<K> y, int S) noexcept {
    const bool negative = y.negative();
    if (negative)
        y = -y;
    if (y.is_zero())
        return {0, 0};
    const int n = bit_width_of(y);
    if (n <= 53)
        return {::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(y)), -S), 0};
    const int          sh   = n - 53;
    const wide_sint<K> top  = y >> sh;
    const wide_sint<K> rest = y - (top << sh);
    const double       hi   = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
    const double       lo =
        sh > 53
            ? ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(rest >> (sh - 53))), sh - 53 - S)
            : ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(rest)), -S);
    const dd r = fast_two_sum(hi, lo);
    return negative ? neg(r) : r;
}

inline constexpr int kS = 136; // the scale (bits after the point) of the constants
using fixed             = ax::fixed_t<2 * kS + 16>;

template <std::size_t K>
constexpr dd of_fixed_any(const wide_sint<K>& y, int S) noexcept {
    return of_fixed(static_cast<fixed>(y), S);
}

// c·2^-S cut into N parts of B bits each (the last one of 53), so that
// k·part is exact for |k| < 2^(53−B): Cody–Waite reduction constants.
template <std::size_t N>
constexpr std::array<double, N> parts(fixed y, int S, int B) noexcept {
    std::array<double, N> r{};
    int                   n = bit_width_of(y); // the bits still to place
    for (std::size_t i = 0; i < N && !y.is_zero(); ++i) {
        const int   bits = i + 1 < N ? B : 53;
        const int   sh   = n > bits ? n - bits : 0;
        const fixed top  = y >> sh;
        r[i]             = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
        y                = y - (top << sh);
        n                = sh;
    }
    return r;
}

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
// roots from cos π/4, then j rotations at scale kS (a unit lost per step).
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
        const fixed d{2 * i - 1}, d2 = d * d;
        fixed       t = (fixed{1} << W) / d, sum{0};
        for (int k = 1; !t.is_zero(); k += 2) {
            sum = sum + t / fixed{k};
            t   = t / d2;
        }
        return sum + sum;
    };
    std::array<dd, 97> r{};
    fixed              acc{0};
    for (int j = 129; j <= 192; ++j)
        r[static_cast<std::size_t>(j - 96)] = of_fixed(acc = acc + step(j), W);
    acc = fixed{0};
    for (int j = 127; j >= 96; --j)
        r[static_cast<std::size_t>(j - 96)] = of_fixed(acc = acc - step(j + 1), W);
    return r;
}

// Every constant and table, as members of a class template: computed on
// first use only, so a translation unit that never reaches the dd tier
// pays nothing for them (the scale S is a parameter so that GCC cannot
// fold the initializers early). D is always dd.
template <typename D, int S = kS>
struct consts {
    static constexpr D Ln2    = of_fixed_any(ax::ln2_q<S>, S);
    static constexpr D Log2e  = of_fixed_any(ax::log2e_q<S>, S);
    static constexpr D Log10e = of_fixed_any(ax::log10e_q<S>, S);
    static constexpr D Pi     = of_fixed_any(ax::pi_q<S>, S);
    static constexpr D HalfPi = ldexp(Pi, -1);
    // ln 2/4096 in parts of 31 bits (|k| < 2^22: |x| ≤ 700 · 4096/ln 2), and
    // π/128 in parts of 27 bits (|n| < 2^26: |x| ≤ 2^20 · 128/π).
    static constexpr std::array<double, 3> Ln2By4096  = parts<3>(static_cast<fixed>(ax::ln2_q<S>), S + 12, 31);
    static constexpr std::array<double, 4> PiBy128    = parts<4>(static_cast<fixed>(ax::pi_q<S>), S + 7, 27);
    static constexpr std::array<D, 64>     Exp2By64   = exp2_table<S>(6);
    static constexpr std::array<D, 64>     Exp2By4096 = exp2_table<S>(12);
    static constexpr std::array<D, 256>    Sin        = sin_table<S>();
    static constexpr std::array<D, 33>     Atan       = atan_table<S>();
    static constexpr std::array<D, 97>     Log        = log_table<S>();
    static constexpr D F2 = inv_fact<S>(2), F3 = inv_fact<S>(3), F4 = inv_fact<S>(4), F5 = inv_fact<S>(5);
    static constexpr D F6 = inv_fact<S>(6), F7 = inv_fact<S>(7);
    static constexpr D I3 = inv_int<S>(3), I5 = inv_int<S>(5), I7 = inv_int<S>(7);
};

inline constexpr double kInvLn2By4096 = 4096 * fpk::kLog2e;
inline constexpr double kInvPiBy128   = 128 / 0x1.921fb54442d18p+1;

// a·2^m for |m| ≤ 1022 (a power-of-two factor, exact unless the result is
// subnormal).
inline dd scale(dd a, long m) noexcept {
    const double f = std::bit_cast<double>(static_cast<std::uint64_t>(m + 1023) << 52);
    return {a.Hi * f, a.Lo * f};
}

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
// Kernels. Each is within about 2^-96 of its value (relative, plus an
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
    const double k  = __builtin_nearbyint(x.Hi * kInvLn2By4096);
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
            div(two_sum(f.Hi - c, f.Lo), add(f, c)), // f.Hi − c is exact
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
    const double n = __builtin_nearbyint(x.Hi * kInvPiBy128);
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

// atan2(y, x): the ratio of the smaller to the larger magnitude, then the
// octant.
template <dd_type D>
inline D atan2(D y, D x) noexcept {
    using C         = consts<D>;
    const double ay = y.Hi < 0 ? -y.Hi : y.Hi, ax_ = x.Hi < 0 ? -x.Hi : x.Hi;
    if (ay == 0 && ax_ == 0)
        return D{0, 0};
    if (ay <= ax_) {
        const D q = div(y, x);
        const D r = q.Hi < 0 ? neg(atan_unit(neg(q))) : atan_unit(q);
        if (x.Hi > 0)
            return r;
        return y.Hi < 0 ? sub(r, C::Pi) : add(r, C::Pi);
    }
    const D q = div(x, y); // |q| < 1
    const D r = q.Hi < 0 ? neg(atan_unit(neg(q))) : atan_unit(q);
    return y.Hi > 0 ? sub(C::HalfPi, r) : sub(neg(C::HalfPi), r);
}

template <dd_type D>
inline D atan(D x) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    r        = a.Hi <= 1 ? atan_unit(a) : sub(consts<D>::HalfPi, atan_unit(div(D{1, 0}, a)));
    return negative ? neg(r) : r;
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
    return ldexp(sub(e, div(D{1, 0}, e)), -1);
}
template <dd_type D>
inline D cosh(D x) noexcept {
    const D e = exp(x);
    return ldexp(add(e, div(D{1, 0}, e)), -1);
}
template <dd_type D>
inline D tanh(D x) noexcept {
    const D e = exp(ldexp(x, 1));
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
    const D    m        = ldexp(log(div(add(a, 1.0), add(neg(a), 1.0))), -1);
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D asin(D x) noexcept {
    return atan2(x, sqrt(mul(add(neg(x), 1.0), add(x, 1.0))));
}
template <dd_type D>
inline D acos(D x) noexcept {
    return atan2(sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), x);
}
template <dd_type D>
inline D hypot(D x, D y) noexcept {
    return sqrt(add(sqr(x), sqr(y)));
}

//---------------------------------------------------------------------------
// The lean kernels: the dd tier's first attempt for sin, cos, exp and exp2.
// The tier serves outputs whose value indices stay below 2^62, which about
// 2^-70 decides nearly always, so these carry in double-double only the
// terms that need it — the table values, the reduced argument and its
// leading products, which are error-free — and the rest in double. Their
// bounds are proved the way the double tier's are (detail/math_fp.hpp):
// every rounding counted at its worst, up() for the second-order terms.
// Results they leave undecided go to the full kernels above.
//---------------------------------------------------------------------------
using fpk::kU;
using fpk::up;

// A table entry or constant (of_fixed: the top 53 bits and the next 53,
// both cut, from a value within 2^-120 of exact) is within 2^-104 of its
// value, relatively.
inline constexpr double kTableErr = 0x1p-104;

// reduce()'s error for |x.Lo| ≤ kU·|x.Hi|, |k| ≤ Kx·max(1, |x|), the second
// part below 2^E1 and a residual of the parts below 2^-Res of their sum: in
// units of max(1, |x|), the roundings of x.Lo − w.Lo (|w.Lo| ≤ kU·|k|·2^E1),
// k·c3 (below |k|·2^E3), their difference and the sum with d.Lo
// (|d.Lo| ≤ kU·R): three of kU on the terms, kU² of R, and |k|·Res.
consteval double reduce_abs(double Kx, int E1, int E3, int Res, double R) {
    const double t = kU * (1 + kU) + kU * Kx * fpk::pow_n(0.5, -E1) + Kx * fpk::pow_n(0.5, -E3);
    return up(3 * kU * t * (1 + 4 * kU) + Kx * fpk::pow_n(0.5, Res) + kU * kU * R);
}

// sin r = r + r³·S(r²), S = −1/3! + z/5! − z²/7!; cos r = 1 − r²/2 + r⁴·C(r²),
// C = 1/4! − z/6! + z²/8!.
inline constexpr fpk::poly<3> kLeanSinC =
    fpk::series<3>([](int k) { return fpk::term{k % 2 ? 1.0 : -1.0, fpk::factorial(2 * k + 3)}; });
inline constexpr fpk::poly<3> kLeanCosC =
    fpk::series<3>([](int k) { return fpk::term{k % 2 ? -1.0 : 1.0, fpk::factorial(2 * k + 4)}; });

// sin and cos for |x| ≤ 2^20, x = n·π/128 + r as in sincos. n's product
// rounds within 2^-26.6 of a step, and |x.Lo| ≤ 2^-33, so |r| ≤ R. With
// r = rh + rl (|rl| ≤ kU·|rh|), z = rh² exactly as zh + zl, and the table's
// A = sin(nπ/128), B = cos(nπ/128):
//   sin(a + r) = A + B·sr + A·cm,  sr = sin r = rh + ts,
//   cos(a + r) = B − A·sr + B·cm,  cm = cos r − 1 = −zh/2 + tc,
// ts = rh·zh·S + rl and tc = zh²·C − zl/2 − rh·rl in double. The products
// B·rh and A·(−zh/2) are error-free and join A in two exact fast two-sums
// (|A| ≥ sin(π/128) > 2|B·rh| unless A = 0); everything else is summed in
// double. All bounds are absolute (|A|, |B| ≤ 1):
// - sr: rl·(cos − 1) for rl·cos (kU·R³/2); rh·zh's two roundings, S's
//   Horner error at zh (horner_error with Dz = kU), its truncation; ts's
//   fma (kU·|ts|);
// - cm: rl·(rh − sin rh) and rl² (kU·R⁴/6, kU²·Z); zh²'s three roundings,
//   C's Horner error and truncation; the fmas of tc and of rh·rl + zl/2;
// - the sum of the low parts: at most 10 roundings of kU, each partial sum
//   within Lmax; the dropped B.Lo·ts and A.Lo·tc; the table's 2^-104 of A
//   and B times (1 + R).
// The reduction's error (reduce_abs: |n| ≤ 42·max(1, |x|), c1 < 2^-31,
// c3 < 2^-86, the parts within 2^-139 of π/128) moves either value by as
// much, times 1 + R: AbsX per max(1, |x|).
struct lean_trig {
    static constexpr double            R = 0.01228; // ≥ (½ + 2^-26)·π/128 + 2^-33
    static constexpr double            Z = R * R, R3 = Z * R, R4 = Z * Z;
    static constexpr fpk::horner_bound HS = fpk::horner_error(kLeanSinC, Z, kU);
    static constexpr fpk::horner_bound HC = fpk::horner_error(kLeanCosC, Z, kU);
    static constexpr double            TS = R3 * HS.Mag * (1 + 4 * kU) + kU * R * (1 + kU);       // ≥ |ts|
    static constexpr double            TC = R4 * HC.Mag * (1 + 4 * kU) + 1.5 * kU * Z * (1 + kU); // ≥ |tc|
    static constexpr double            SinR =
        kU * R3 / 2 + R3 * (2 * kU * HS.Mag + HS.Err + fpk::pow_n(Z, 3) / fpk::factorial(9)) + kU * TS;
    static constexpr double CosR = kU * R4 / 6 + 2 * kU * kU * Z +
                                   R4 * (3 * kU * HC.Mag + HC.Err + fpk::pow_n(Z, 3) / fpk::factorial(10)) + kU * TC;
    static constexpr double Lmax = 2 * kU * (1 + R + Z) + kU * R + kU * Z / 2 + kU + TS + kU * R + TC + kU * Z / 2;
    static constexpr double Abs  = up(SinR + CosR + 10 * kU * Lmax + kU * (TS + TC) + kTableErr * (1 + R));
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
    const double ts = std::fma(fpk::rounded_product(rh, zh), fpk::horner(zh, kLeanSinC.C), rl);
    const double tc =
        std::fma(fpk::rounded_product(zh, zh), fpk::horner(zh, kLeanCosC.C), -std::fma(rh, rl, 0.5 * z.Lo));
    const double h = -0.5 * zh;
    return {lean_rotate(sa, ca, rh, ts, h, tc), lean_rotate(ca, neg(sa), rh, ts, h, tc)};
}

// The bound of a lean sin or cos at x.
constexpr double lean_trig_bound(double x) noexcept {
    const double ax = x < 0 ? -x : x;
    return lean_trig::Abs + lean_trig::AbsX * (ax > 1 ? ax : 1.0) + fpk::kTiny;
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
inline constexpr fpk::poly<4> kLeanExpC = fpk::series<4>([](int k) { return fpk::term{1.0, fpk::factorial(k + 2)}; });

// e^x for |x| ≤ 700, x = k·ln 2/4096 + r as in exp: n's product rounds
// within 2^-29 of a step and |x.Lo| ≤ 2^-43, so |r| ≤ R. With
// r = rh + rl and T = 2^(i/64)·2^(j/4096) (a dd product, within
// 2·2^-104 + 6kU² of its value), e^x = 2^m·(T + T·(rh + tp)),
// tp = rh²·P(rh) + rl in double; T.Hi·rh is error-free and joins T.Hi in
// an exact fast two-sum. Relative to T:
// - rh + tp: rl·(e^rh·(e^rl − 1)/rl − 1) (kU·R²·(1 + R)); rh²'s rounding,
//   P's Horner error (at rh, exact) and truncation, tp's fma;
// - the low parts: 5 roundings within Lmax, the dropped T.Lo·tp, T's error
//   times 1 + R + TP;
// divided by the least e^r/T = 1 − R − TP, plus the reduction's error
// (reduce_abs: |k| ≤ 5910·max(1, |x|), c1 < 2^-42, no c3, the parts within
// 2^-127 of ln 2/4096) at |x| = 700, which moves e^x by as much,
// relatively.
struct lean_exp {
    static constexpr double            R  = 8.462e-5; // ≥ (½ + 2^-29)·ln 2/4096 + 2^-43
    static constexpr fpk::horner_bound HP = fpk::horner_error(kLeanExpC, R, 0);
    static constexpr double            TP = R * R * HP.Mag * (1 + 3 * kU) + kU * R * (1 + kU); // ≥ |tp|
    static constexpr double            ER =
        kU * R * R * (1 + R) + R * R * (kU * HP.Mag + HP.Err + fpk::pow_n(R, 4) / 720 * 1.0001) + kU * TP;
    static constexpr double Et   = up(2 * kTableErr + 6 * kU * kU);
    static constexpr double Lmax = kU * (1 + R) + kU * R + kU + TP + kU * R;
    static constexpr double Rel  = up((ER + Et * (1 + R + TP) + 5 * kU * Lmax + kU * TP) / (1 - R - TP) +
                                      700 * reduce_abs(5910, -42, -1074, 127, R));
    // exp2: y = x·ln 2 (|x| ≤ 1000, so |y| ≤ 694) as a dd product, within
    // 2^-104 + 6kU² of itself; it moves e^y by |y| times that.
    static constexpr double Rel2 = up(Rel + 694 * (kTableErr + 6 * kU * kU));
};

template <dd_type D>
[[gnu::always_inline]] inline D exp_lean_value(D x) noexcept {
    const auto [r, t, m] = exp_reduce(x);
    const double rh      = r.Hi;
    const double tp      = std::fma(fpk::rounded_product(rh, rh), fpk::horner(rh, kLeanExpC.C), r.Lo);
    const D      p       = two_prod(t.Hi, rh);
    const D      s       = fast_two_sum(t.Hi, p.Hi);
    const double lo      = std::fma(t.Hi, tp, std::fma(t.Lo, rh, s.Lo + (p.Lo + t.Lo)));
    return scale(fast_two_sum(s.Hi, lo), m);
}

template <dd_type D>
inline D exp_lean(D x, double& bound) noexcept {
    const D v = exp_lean_value(x);
    bound     = lean_exp::Rel * v.Hi + fpk::kTiny;
    return v;
}
template <dd_type D>
inline D exp2_lean(D x, double& bound) noexcept {
    const D v = exp_lean_value(mul(x, consts<D>::Ln2));
    bound     = lean_exp::Rel2 * v.Hi + fpk::kTiny;
    return v;
}
// 2·atanh s = 2s + 2s·z·P(z), P = 1/3 + z/5 + z²/7.
inline constexpr fpk::poly<3> kLeanLogC = fpk::series<3>([](int k) { return fpk::term{1.0, 2.0 * k + 3}; });

// ln x for normal x > 0, reduced as in log: ln x = m·ln 2 + ln c + 2·atanh s,
// |s| ≤ S since |f − c| ≤ 1/256 and f + c ≥ 2·0.75 − 1/256. s is a dd
// quotient: its numerator is exact, the denominator one dd sum (2kU² of
// it), and the division within about 15kU², so s is within 32kU²·|s|.
// m·Ln2.Hi, ln c's high part and 2s join in two exact two-sums; the tail
// t = 2·sh·zh·P(zh) (sh = s.Hi, zh = sh²) and the low parts are summed in
// double. All bounds absolute, |ln c| ≤ B = ln 1.5:
// - t: zh, sh·zh and the product with P round (3kU of |t|); P's Horner
//   error at zh; the truncation (2S⁹/9, widened 1%); and t taken at sh, not
//   s (|s.Lo| ≤ kU·S moves it by 2S²·kU·S);
// - 2s: 64kU²·S;
// - the low sum: 7 roundings within Lsum = 5kU·(B + 2S) + |t|, plus per
//   unit of |m|·ln 2 the same 5kU in Lsum (35kU²), the product
//   m·Ln2.Lo's rounding (kU²) and Ln2's 2^-104 (PerM);
// - ln c's table entry: 2^-104 of B.
struct lean_log {
    static constexpr double            S  = 0.002612; // ≥ (1/256)/1.4961
    static constexpr double            Z  = S * S;
    static constexpr double            B  = 0.4055; // ≥ ln 1.5
    static constexpr fpk::horner_bound HP = fpk::horner_error(kLeanLogC, Z, kU);
    static constexpr double            TT = 2 * S * Z * HP.Mag * (1 + 4 * kU); // ≥ |t|
    static constexpr double            ET =
        3 * kU * TT + 2 * S * Z * HP.Err + 2 * fpk::pow_n(S, 9) / 9 * 1.01 + 2 * Z * kU * S * 1.0001;
    static constexpr double Lsum = 5 * kU * (B + 2 * S) + TT;
    static constexpr double Abs  = up(ET + 64 * kU * kU * S + 7 * kU * Lsum + kTableErr * B);
    static constexpr double PerM = up(0.6932 * (35 * kU * kU + kU * kU + kTableErr));
};

template <dd_type D>
[[gnu::always_inline]] inline D log_lean_value(D x, double& bound) noexcept {
    const auto [m, s, lc] = log_reduce(x);
    const double sh = s.Hi, zh = sh * sh;
    const double t  = 2 * (fpk::rounded_product(sh, zh) * fpk::horner(zh, kLeanLogC.C));
    const double md = static_cast<double>(m);
    const D      a  = two_prod(md, consts<D>::Ln2.Hi);
    const D      s1 = two_sum(a.Hi, lc.Hi);
    const D      s2 = two_sum(s1.Hi, 2 * s.Hi);
    const double lo = (((s1.Lo + s2.Lo) + (a.Lo + md * consts<D>::Ln2.Lo)) + (lc.Lo + 2 * s.Lo)) + t;
    bound           = lean_log::Abs + lean_log::PerM * (md < 0 ? -md : md);
    return fast_two_sum(s2.Hi, lo);
}

template <dd_type D>
inline D log_lean(D x, double& bound) noexcept {
    return log_lean_value(x, bound);
}

// log2 and log10: ln x times the constant (within 2^-104) as a dd product
// (within 4kU²), so the bound scales by the constant (≤ 1.4427) and adds
// 2^-100 of the result.
template <dd_type D>
inline D log2_lean(D x, double& bound) noexcept {
    const D v = mul(log_lean_value(x, bound), consts<D>::Log2e);
    bound     = bound * 1.4427 + 0x1p-100 * (v.Hi < 0 ? -v.Hi : v.Hi);
    return v;
}
template <dd_type D>
inline D log10_lean(D x, double& bound) noexcept {
    const D v = mul(log_lean_value(x, bound), consts<D>::Log10e);
    bound     = bound * 0.4343 + 0x1p-100 * (v.Hi < 0 ? -v.Hi : v.Hi);
    return v;
}
//---------------------------------------------------------------------------
// Lean kernels built on the ones above. Error terms of the dd operations,
// relative to their exact results: a sum (add, sub, add_dominant) rounds
// within 4kU² of |a| + |b|; a product (mul, sqr) within 4kU²; a quotient
// (div: one reciprocal, a residual that cancels exactly, two digits) within
// 32kU²; sqrt (one Newton step from the correctly rounded root) within 7kU²
// plus half its argument's. x ± 1 is exact for 0.5 ≤ |x.Hi| ≤ 2 (Sterbenz:
// the low part then adds to an exact zero), and within 2kU²·(1 + |x|) of
// itself otherwise.
//---------------------------------------------------------------------------
inline constexpr double kU2 = kU * kU;

// tan = sin/cos from the lean sincos, each within e: |s/c − S/C| ≤
// e·(1 + |t|)/(|c| − e), plus the quotient's 32kU². No bound (infinity)
// where |c| ≤ e: the full kernel decides there.
template <dd_type D>
inline D tan_lean(D x, double& bound) noexcept {
    const sincos_t sc = sincos_lean(x);
    const double   e  = lean_trig_bound(x.Hi);
    const D        t  = div(sc.Sin, sc.Cos);
    const double   at = t.Hi < 0 ? -t.Hi : t.Hi, c = (sc.Cos.Hi < 0 ? -sc.Cos.Hi : sc.Cos.Hi) - 2 * e;
    bound = c > 0 ? e * (1 + at) / c * (1 + 0x1p-40) + 32 * kU2 * at : __builtin_inf();
    return t;
}

// sinh, cosh = (e ± 1/e)/2 with e = e^x within Rel: 1/e within Rel + 32kU²,
// the sum's 4kU² of e + 1/e; all within (Rel + 20kU²)·cosh x, and
// cosh ≤ |sinh| + 1. tanh = (e2 − 1)/(e2 + 1), e2 = e^(2x): e2's Rel moves it
// by 2·e2·Rel/(e2 + 1)² ≤ Rel/2; the two sums with 1 round within
// 2kU²·(e2 + 1), 2kU² of the quotient each; the quotient 32kU².
template <dd_type D>
inline D sinh_lean(D x, double& bound) noexcept {
    const D e = exp_lean_value(x);
    const D v = ldexp(sub(e, div(D{1, 0}, e)), -1);
    bound     = (lean_exp::Rel + 20 * kU2) * ((v.Hi < 0 ? -v.Hi : v.Hi) + 1) * (1 + 0x1p-40) + fpk::kTiny;
    return v;
}
template <dd_type D>
inline D cosh_lean(D x, double& bound) noexcept {
    const D e = exp_lean_value(x);
    const D v = ldexp(add(e, div(D{1, 0}, e)), -1);
    bound     = (lean_exp::Rel + 20 * kU2) * v.Hi * (1 + 0x1p-40) + fpk::kTiny;
    return v;
}
template <dd_type D>
inline D tanh_lean(D x, double& bound) noexcept {
    const D e2 = exp_lean_value(ldexp(x, 1));
    bound      = (lean_exp::Rel / 2 + 36 * kU2) * (1 + 0x1p-40);
    return div(add(e2, -1.0), add(e2, 1.0));
}

// The inverse hyperbolics as ln w, w within ε relative: within the lean
// log's bound plus ε·(1 + 2^-40).
// - asinh |x|: w = |x| + √(x² + 1): the square 4kU², + 1 2kU², the root
//   2kU² + 7kU², the sum 4kU²: ε = 17kU².
// - acosh x ≥ 1: w = x + √((x − 1)(x + 1)): x − 1 exact for x.Hi ≤ 2, else
//   within 2kU²·(1 + x)/(x − 1) ≤ 6kU²; x + 1 2kU²·(1 + x)/(x + 1) ≤ 2kU²;
//   the product 4kU², the root 6kU² + 7kU², the sum 4kU²: ε = 23kU².
// - atanh |x| < 1: ½·ln((1 + a)/(1 − a)): 1 − a exact for a.Hi ≥ 0.5, else
//   within 2kU²·(1 + a)/(1 − a) ≤ 6kU²; 1 + a 2kU²; the quotient 32kU²:
//   ε = 40kU², and the bound halves.
template <dd_type D>
inline D asinh_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = log_lean_value(add(a, sqrt(add(sqr(a), 1.0))), bound);
    bound += 17 * kU2 * (1 + 0x1p-40);
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D acosh_lean(D x, double& bound) noexcept {
    const D v = log_lean_value(add(x, sqrt(mul(add(x, -1.0), add(x, 1.0)))), bound);
    bound += 23 * kU2 * (1 + 0x1p-40);
    return v;
}
template <dd_type D>
inline D atanh_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = ldexp(log_lean_value(div(add(a, 1.0), add(neg(a), 1.0)), bound), -1);
    bound               = (bound + 40 * kU2) * 0.5 * (1 + 0x1p-40);
    return negative ? neg(m) : m;
}

// atan t = t − t·z·P(z), P = 1/3 − z/5 + z²/7 − z³/9 + z⁴/11.
inline constexpr fpk::poly<5> kLeanAtanC =
    fpk::series<5>([](int k) { return fpk::term{k % 2 ? -1.0 : 1.0, 2.0 * k + 3}; });

// atan a for 0 ≤ a ≤ 1 (a.Hi ≤ 1), reduced as in atan_unit: c = j/32,
// t = (a − c)/(1 + a·c), |t| ≤ S. a − c is exact (Sterbenz, c ≥ 1/32, or
// c = 0); 1 + a·c within 3kU²; so t within 36kU². atan a = atan c + t + T,
// T = −th·zh·P(zh) in double (th = t.Hi, zh = th²); atan c + t.Hi in one
// exact fast two-sum (atan c ≥ atan(1/32) > S unless c = 0). Absolute:
// - T: three roundings (3kU of |T|), P's Horner error at zh, the
//   alternating series' first omitted term S¹³/13, and T taken at th, not
//   t (kU·S·S²);
// - t: 36kU²·S;
// - the low sum: 4 roundings within Lsum; atan c's table 2^-104.
struct lean_atan {
    static constexpr double            S    = 0.015626; // ≥ 1/64
    static constexpr double            Z    = S * S;
    static constexpr double            A    = 0.7854; // ≥ π/4
    static constexpr fpk::horner_bound HP   = fpk::horner_error(kLeanAtanC, Z, kU);
    static constexpr double            TT   = S * Z * HP.Mag * (1 + 4 * kU); // ≥ |T|
    static constexpr double            ET   = 3 * kU * TT + S * Z * HP.Err + fpk::pow_n(S, 13) / 13 + kU * S * Z;
    static constexpr double            Lsum = 2 * kU * (A + S) + kU * A + kU * S + TT;
    static constexpr double            Unit = up(ET + 36 * kU2 * S + 4 * kU * Lsum + kTableErr * A);
    // atan |x| > 1: π/2 − atan(1/|x|): the quotient 32kU² (slope ≤ 1), the
    // difference 4kU²·(π/2 + π/4), π/2's 2^-104.
    static constexpr double Atan = up(Unit + 32 * kU2 + 10 * kU2 + kTableErr * 1.571);
    // atan2: the quotient of the smaller by the larger 32kU²; ±π or ±π/2 and
    // the sum 4kU²·(π + π/4) and 2^-104 of π.
    static constexpr double Atan2 = up(Unit + 32 * kU2 + 16 * kU2 + kTableErr * 3.1416);
    // asin, acos = atan2 with √((1 − x)(1 + x)) as one argument: 1 ∓ x within
    // 6kU² (as x ± 1 above), the product 4kU², the root 8kU² + 7kU² — 15kU²
    // relative, which moves the angle by at most half of it.
    static constexpr double Asin = up(Atan2 + 8 * kU2);
};

template <dd_type D>
[[gnu::always_inline]] inline D atan_unit_lean(D a) noexcept {
    const double j  = __builtin_nearbyint(a.Hi * 32);
    const double c  = j * (1.0 / 32);
    const D      t  = div(add(a, -c), add_dominant(D{1, 0}, mul(a, c)));
    const double th = t.Hi, zh = th * th;
    const double T  = -(fpk::rounded_product(th, zh) * fpk::horner(zh, kLeanAtanC.C));
    const D      ac = consts<D>::Atan[static_cast<std::size_t>(j)];
    const D      s  = fast_two_sum(ac.Hi, th);
    return fast_two_sum(s.Hi, ((s.Lo + ac.Lo) + t.Lo) + T);
}

template <dd_type D>
inline D atan_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    r        = a.Hi <= 1 ? atan_unit_lean(a) : sub(consts<D>::HalfPi, atan_unit_lean(div(D{1, 0}, a)));
    bound               = lean_atan::Atan;
    return negative ? neg(r) : r;
}

// atan2 as in the full kernel, with the lean unit atan.
template <dd_type D>
inline D atan2_lean(D y, D x, double& bound) noexcept {
    using C         = consts<D>;
    bound           = lean_atan::Atan2;
    const double ay = y.Hi < 0 ? -y.Hi : y.Hi, ax_ = x.Hi < 0 ? -x.Hi : x.Hi;
    if (ay == 0 && ax_ == 0) {
        bound = 0;
        return D{0, 0};
    }
    if (ay <= ax_) {
        const D q = div(y, x);
        const D r = q.Hi < 0 ? neg(atan_unit_lean(neg(q))) : atan_unit_lean(q);
        if (x.Hi > 0)
            return r;
        return y.Hi < 0 ? sub(r, C::Pi) : add(r, C::Pi);
    }
    const D q = div(x, y);
    const D r = q.Hi < 0 ? neg(atan_unit_lean(neg(q))) : atan_unit_lean(q);
    return y.Hi > 0 ? sub(C::HalfPi, r) : sub(neg(C::HalfPi), r);
}

template <dd_type D>
inline D asin_lean(D x, double& bound) noexcept {
    const D v = atan2_lean(x, sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), bound);
    bound     = lean_atan::Asin;
    return v;
}
template <dd_type D>
inline D acos_lean(D x, double& bound) noexcept {
    const D v = atan2_lean(sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), x, bound);
    bound     = lean_atan::Asin;
    return v;
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
// decides the slot; and the integer path here (exact square and cube roots,
// series in wide fixed point), which decides every result and is the only
// tier at compile time and without an FPU.
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
    if (sh <= 0)
        return {a.Value << (-sh), a.Error << (-sh)};
    return {round_shift(a.Value, sh), (a.Error >> sh) + 1};
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

// exp x. Exact only at 0.
template <std::size_t E, int Mag, int KMax>
struct exp_core {
    exact_frac<E>            X;
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
    exact_frac<E>            X;
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
    exact_frac<E>            X;
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
        constexpr int S         = W + 12 + std::bit_width(static_cast<unsigned>(2 * Bits + Mag));
        using I                 = fixed_t<S + Mag + 8>;
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

// b^e for b > 0: e^(e·log b). Exact for e = 0, b = 1, and integer e when
// the power fits PowLimbs.
inline constexpr std::size_t pow_limbs = 32; // exact integer powers up to 2048 bits

template <std::size_t EB, std::size_t EE>
constexpr std::optional<exact_frac<pow_limbs>> exact_pow(const exact_frac<EB>& b, const exact_frac<EE>& e) noexcept {
    using P = wide_sint<pow_limbs>;
    if (is_zero(e) || is_one(b))
        return exact_frac<pow_limbs>{P{1}, P{1}};
    if (!is_integer(e))
        return std::nullopt;
    const auto           k    = e.Num / e.Den;
    const auto           ak   = k.negative() ? -k : k;
    const exact_frac<EB> r    = reduced(b);
    const int            bits = bit_width_of(r.Num) + bit_width_of(r.Den);
    if (bit_width_of(ak) > 12 || static_cast<imax>(ak) * bits > 64 * static_cast<imax>(pow_limbs) - 8)
        return std::nullopt;
    P n{1}, d{1};
    for (imax i = 0; i < static_cast<imax>(ak); ++i) {
        n = n * P{r.Num};
        d = d * P{r.Den};
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
    return static_cast<imax>(k) * Notch;
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
    const I    one{1};
    const I    d  = a.Scale >= 0 ? one << a.Scale : one;
    auto       at = [&](const I& y) { return lattice_index<Notch, Up>(a.Scale >= 0 ? y : y << (-a.Scale), d); };
    const I    y{a.Value}, e{a.Error};
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

// ⌈|r|⌉ as an int (deduction bounds).
constexpr imax ceil_abs(const grid_rational& r) {
    const grid_wide n = wide_numerator(r), d = wide_denominator(r);
    const grid_wide a = n.negative() ? -n : n;
    return static_cast<imax>((a + d - grid_wide{1}) / d);
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

// Bits of x² + y² as one fraction: each square doubles its input's bits,
// and the sum over two denominators adds them.
template <insidable InX, insidable InY>
inline constexpr int hypot_bits = 2 * (input_bits<InX> + input_bits<InY>)+2;

// gcd of two value units (0 when either is continuous), for two-input
// outputs: the notches, or finer where a lattice does not pass through 0.
// Every value of both inputs is a multiple of it.
template <insidable A, insidable B>
inline constexpr grid_rational gcd_notch = [] {
    if constexpr (!notched<A> || !notched<B>)
        return grid_rational{0};
    else
        return grid_gcd_of(grid_of<A>.value_unit(), grid_of<B>.value_unit());
}();

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
        const grid_wide q = wide_denominator(notch_of<In>), p = wide_numerator(notch_of<In>);
        const bool      dyadic = (grid_wide{1} << (grid_bits(q) - 1)) == q;
        return dyadic && grid_magnitude_bits<In> + grid_bits(q) + grid_bits(p) <= 53;
    }
}();

// Outputs the double tier decides nearly always (up to 2^-14 of results go
// further): below them no other fast tier runs.
inline constexpr int kFpOnlyBits = 36;

// Bits of Out's value indices: every |index| < 2^index_bits.
template <insidable Out>
inline constexpr int index_bits = [] {
    if constexpr (!notched<Out> || wide_valued<Out>)
        return 1024;
    else {
        const grid_wide lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.slot_count();
        const grid_wide m = (-lo < hi) ? hi : -lo;
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

// A grid's notch p/q as doubles (both below 2^63, so within 2^-53 of p and
// q), and whether it is a power of two, so that scaling by it is exact.
template <insidable Out>
inline constexpr double notch_p = static_cast<double>(static_cast<imax>(wide_numerator(notch_of<Out>)));
template <insidable Out>
inline constexpr double notch_q = static_cast<double>(static_cast<imax>(wide_denominator(notch_of<Out>)));
template <insidable Out>
inline constexpr bool dyadic_notch = [] {
    const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
    return (grid_wide{1} << (grid_bits(p) - 1)) == p && (grid_wide{1} << (grid_bits(q) - 1)) == q;
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
    if (!(j >= lo && j <= hi))
        return false;
    store_slot(static_cast<umax>(static_cast<imax>(j - lo)), out);
    return true;
}

// The kernels' safe argument ranges (compile-time, from In's grid).
template <insidable In>
inline constexpr double in_max = static_cast<double>(max_abs_int<In>);

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

inline constexpr double kDDRel = 0x1p-88, kDDAbs = 0x1p-92;

constexpr double dd_eval_bound(double x, double v) noexcept {
    return kDDRel * fabs_d(v) + kDDAbs * (fabs_d(x) > 1 ? fabs_d(x) : 1.0);
}

inline constexpr double two53 = 0x1p53;

// Value indices of Out of at most 2^62 in magnitude, and a notch whose p and q
// are doubles exactly.
template <insidable Out>
inline constexpr bool dd_output = [] {
    if constexpr (!notched<Out> || wide_valued<Out> || out_bits<Out> + mag_bits<Out> <= kFpOnlyBits)
        return false;
    else {
        if (!(notch_p<Out> < two53 && notch_q<Out> < two53))
            return false;
        const grid_wide lim = grid_wide{1} << 62;
        const grid_wide lo = slot_base<Out>, hi = slot_base<Out> + grid_of<Out>.slot_count();
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
    if (j < first || j > last)
        return false;
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
    fp_bits<Out, K::Mag> + 10 < fpk::kFullBits ? fp_bits<Out, K::Mag> + 10 : fpk::kFullBits;

// One kernel per function: its value and proved bound at a target T, the
// bits of its values (Mag, 1024 for unbounded), its limit, and its slope
// |f′(x)| (for the input's rounding).
inline constexpr int kUnbounded = 1024;
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
using fp_full_trig = fpk::trig_k<fpk::kFullBits>;
using fp_full_exp  = fpk::exp_k<fpk::kFullBits>;
using fp_full_log  = fpk::log_k<fpk::kFullBits>;
using fp_full_atan = fpk::atan_k<fpk::kFullBits>;
BEMAN_INSIDE_AX_KERNEL(sin, trig_k, sin, 1, fp_limit(fp_full_trig::Rel), 1.0)
BEMAN_INSIDE_AX_KERNEL(cos, trig_k, cos, 1, fp_limit(fp_full_trig::Rel), 1.0)
BEMAN_INSIDE_AX_KERNEL(exp, exp_k, exp, kUnbounded, fp_limit(fp_full_exp::Rel), fabs_d(v))
BEMAN_INSIDE_AX_KERNEL(exp2, exp_k, exp2, kUnbounded, fp_limit(fp_full_exp::Rel2), fabs_d(v))
BEMAN_INSIDE_AX_KERNEL(sinh, exp_k, sinh, kUnbounded, fp_limit(fp_full_exp::SinhRel), fabs_d(v) + 1)
BEMAN_INSIDE_AX_KERNEL(cosh, exp_k, cosh, kUnbounded, fp_limit(fp_full_exp::CoshRel), fabs_d(v) + 1)
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
BEMAN_INSIDE_AX_KERNEL(log, log_k, log, kUnbounded, fp_limit(fp_full_log::Rel), 1.0 / fabs_d(x))
BEMAN_INSIDE_AX_KERNEL(log2, log_k, log2, kUnbounded, fp_limit(fp_full_log::Rel2), 1.5 / fabs_d(x))
BEMAN_INSIDE_AX_KERNEL(log10, log_k, log10, kUnbounded, fp_limit(fp_full_log::Rel10), 1.0 / fabs_d(x))
BEMAN_INSIDE_AX_KERNEL(asinh, log_k, asinh, kUnbounded, fp_limit(fp_full_log::AsinhRel), 1.0)
BEMAN_INSIDE_AX_KERNEL(
    atanh, log_k, atanh, kUnbounded, fp_limit(fp_full_log::Rel + fp_full_log::AtanhAbs), 1.0 / ((1.0 - x) * (1.0 + x)))
// cbrt = e^(ln|x|/3): the log's error grows with |ln|x||, up to 2^4.
BEMAN_INSIDE_AX_KERNEL(cbrt,
                       pow_k,
                       cbrt,
                       kUnbounded,
                       fp_limit(fp_full_exp::Rel + 16 * fpk::pow_k<fpk::kFullBits>::YRel),
                       x == 0 ? 0.0 : fabs_d(v / x))

// acosh: the dd kernel's bound grows near 1 as 2^-92/√(1 − 1/x²).
struct fp_acosh_eval {
    static double dd_eval(double x, double v) {
        return kDDAbs / fpk::fp_sqrt(1.0 - 1.0 / (x * x)) + dd_eval_bound(x, v);
    }
};
BEMAN_INSIDE_AX_KERNEL_ON(fp_acosh_eval,
                          acosh,
                          log_k,
                          acosh,
                          kUnbounded,
                          fp_limit(fp_full_log::AcoshRel),
                          1.0 / fpk::fp_sqrt((x - 1.0) * (x + 1.0)))
    #undef BEMAN_INSIDE_AX_KERNEL_ON
    #undef BEMAN_INSIDE_AX_KERNEL

// sqrt: correctly rounded.
struct fp_sqrt : fp_plain {
    static constexpr int Mag   = kUnbounded;
    static constexpr int Limit = fp_limit(fpk::kSqrtRel);
    template <int T>
    static double value(double x, double& bound) {
        const double v = fpk::fp_sqrt(x);
        bound          = fpk::kSqrtRel * v;
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
    static constexpr int Mag   = kUnbounded;
    static constexpr int Limit = fp_limit(fp_full_trig::TanRel);
};
struct fp_atan2 {
    static constexpr int Mag   = 2;
    static constexpr int Limit = fp_limit(4 * fp_full_atan::Atan2Rel + fp_full_atan::Atan2Abs);
};
struct fp_hypot {
    static constexpr int Mag   = kUnbounded;
    static constexpr int Limit = fp_limit(fpk::kHypotRel);
};
// pow's bound grows with |e·ln b|: the limit holds up to 2^4.
struct fp_pow {
    static constexpr int Mag   = kUnbounded;
    static constexpr int Limit = fp_limit(fp_full_exp::Rel + 16 * fpk::pow_k<fpk::kFullBits>::YRel);
};
struct fp_pow_base {
    static constexpr int Mag   = kUnbounded;
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
    const double bound = fpk::kHypotRel * v + fpk::kTiny + input_rel<InX> * fabs_d(x) + input_rel<InY> * fabs_d(y);
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
    const ddk::fixed a = ddk::fixed{Base} << ddk::kS;
    return ddk::of_fixed(log_fixed<ddk::kS>(a, 0).Value, ddk::kS);
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

// What follows a lean kernel: rarely taken, so out of line.
template <typename F>
[[gnu::cold, gnu::noinline]] bool dd_cold(const F& f) {
    return f();
}

// A lean attempt (its value, setting the bound) first, then the full one.
template <insidable Out, typename Lean, typename Full>
[[gnu::always_inline]] inline bool dd_lean_first(const Lean& lean, const Full& full, Out& out) {
    double        bound;
    const ddk::dd v = lean(bound);
    if (dd_decide(v, bound * 1.5, out))
        return true;
    return dd_cold(full);
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
            return dd_decide(t, (kDDRel * sec2 * mx + kDDAbs * mx + input(t.Hi)) * 1.5, out);
        },
        out);
}

// pow = e^(e·ln b), as in the double tier; e·ln b within the exp range.
// The lean form: ln b within the lean log's bound Bl moves y = e·ln b by
// |e|·Bl, the product rounds within 4kU² of |y|, and e^y adds its own Rel,
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
                                              4 * fpk::kU * fpk::kU * L + input(L)) +
                              fpk::kTiny;
            return v;
        },
        [&] {
            const ddk::dd y = ddk::mul(ddk::log(b), e);
            const double  L = fabs_d(y.Hi);
            if (!(L <= 700))
                return false;
            const ddk::dd v = ddk::exp(y);
            return dd_decide(v, (fabs_d(v.Hi) * (kDDRel * (1 + L) + input(L)) + kDDAbs) * 1.5, out);
        },
        out);
}

// Base^x = e^(x·ln Base): ln Base within 2^-104 and the product's 4kU².
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
            bound =
                fabs_d(v.Hi) * (ddk::lean_exp::Rel + (4 * fpk::kU * fpk::kU + ddk::kTableErr + dd_input_rel<In>)*L) +
                fpk::kTiny;
            return v;
        },
        [&] {
            const ddk::dd v = ddk::exp(y);
            return dd_decide(v, (fabs_d(v.Hi) * (kDDRel * (1 + L) + dd_input_rel<In> * L) + kDDAbs) * 1.5, out);
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
                      policy_of<In> & ~::beman::inside::detail::cursor_marker>{x};
}

template <insidable Out>
inline constexpr grid_rational half_unit = ::beman::inside::detail::grid_gcd_of(
    ::beman::inside::detail::grid_div_of(notch_of<Out>, grid_rational{2}), lower_of<Out>);

template <insidable Out, policy_flag R, grid_rational Unit = half_unit<Out>>
using bracket_t = inside<grid{{::beman::inside::detail::grid_sub(lower_of<Out>, notch_of<Out>),
                               ::beman::inside::detail::grid_add(upper_of<Out>, notch_of<Out>)},
                              Unit},
                         R | (policy_of<Out> & clamp)>;

// The integer form: a bracket 16 times finer than half_unit, counted in
// imax. Rounded down there, the value lies in [y, y + F); only when y is
// itself a rounding boundary of Out (a point or a half point — at most one
// bracket point in 16) does rounding up decide whether the value is exactly y.
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
    if (k < 0 || static_cast<umax>(k) > max_index_v<Out>)
        return std::nullopt;
    return k;
}

// fn.operator()<O>(xs...) for an anchored O: Out's result, or its error.
template <insidable Out, typename Fn, insidable... Ins>
constexpr auto via_anchored(const Fn& fn, const Ins&... xs) {
    static_assert(anchored<Out> || !has_flag(policy_of<Out>, wrap),
                  "beman::inside::math: wrap onto a grid that does not pass through 0 is not supported - "
                  "use clamp, or an anchored output grid");
    using ::beman::inside::detail::is_expected_v;
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
        constexpr bool checked          = is_expected_v<R>;
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
                       exact_frac<exact_min_limbs>{wide_sint<exact_min_limbs>{1}, wide_sint<exact_min_limbs>{2}} *
                           exact_of_grid<exact_min_limbs>(F);
        if constexpr (checked) {
            errc      ec{};
            const Out out = ax::store_exact<Out>(v, make_policy<policy_of<Out>>(ec));
            return ec == errc{} ? Res{out} : Res{std::unexpected{ec}};
        } else
            return ax::store_exact<Out>(v, make_policy<policy_of<Out>>());
    } else {
        const auto lo                   = fn.template operator()<bracket_t<Out, round_floor>>(anchored_input(xs)...);
        const auto hi                   = fn.template operator()<bracket_t<Out, round_ceil>>(anchored_input(xs)...);
        auto                        mid = [](const auto& a, const auto& b) {
            using ::beman::inside::detail::exact_of;
            const auto s = exact_of(a) + exact_of(b);
            return decltype(s){s.Num, s.Den + s.Den};
        };
        if constexpr (is_expected_v<decltype(lo)>) {
            using R = std::expected<Out, errc>;
            if (!lo)
                return R{std::unexpected{lo.error()}};
            if (!hi)
                return R{std::unexpected{hi.error()}};
            errc      ec{};
            const Out out = ax::store_exact<Out>(mid(*lo, *hi), make_policy<policy_of<Out>>(ec));
            return ec == errc{} ? R{out} : R{std::unexpected{ec}};
        } else
            return ax::store_exact<Out>(mid(lo, hi), make_policy<policy_of<Out>>());
    }
}

// Whether a call needs via_anchored.
template <insidable Out, insidable... Ins>
inline constexpr bool unanchored_call = !anchored<Out> || (!anchored<Ins> || ...);
} // namespace anchoring

// What follows a fast tier: rarely taken, so out of line, which keeps its
// frame off the tier's fast path.
template <typename F>
[[gnu::cold, gnu::noinline]] constexpr auto cold_call(const F& f) {
    return f();
}

// The tiers in order at runtime: the double kernel (when FP), the dd
// kernel (when DD), the integer path. Each fast tier stores a decided
// result and returns true; whatever runs after the first one is cold.
// Constant evaluation takes the integer path.
template <insidable Out, bool FP, bool DD, typename F, typename D, typename I>
[[gnu::always_inline]] constexpr auto
tiers([[maybe_unused]] const F& fp, [[maybe_unused]] const D& dd, const I& integer) -> decltype(integer()) {
    if constexpr (FP || DD)
        if !consteval {
            if constexpr (FP) {
                if (Out r; fp(r))
                    return r;
                return cold_call([&]() -> decltype(integer()) {
                    if constexpr (DD)
                        if (Out r; dd(r))
                            return r;
                    return integer();
                });
            } else {
                if (Out r; dd(r))
                    return r;
                return cold_call(integer);
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
    #define BEMAN_INSIDE_AX_TIERS(Out, FP, DD, fp_call, dd_call, ...) return __VA_ARGS__;
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
    static_assert(has_flag(policy_of<Out>, snap) || !ax::notched<Out>,
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
    exp, true, "", ax::in_max<In> <= 700, ax::exp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>)
BEMAN_INSIDE_AX_UNARY(
    exp2, true, "", ax::in_max<In> <= 1000, ax::exp2_core<ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>>)
BEMAN_INSIDE_AX_UNARY(
    sin, true, "", ax::in_max<In> <= 0x1p20, ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::sin, 1>)
BEMAN_INSIDE_AX_UNARY(
    cos, true, "", ax::in_max<In> <= 0x1p20, ax::trig_core<ax::input_limbs<In>, ax::in_mag<In>, ax::trig::cos, 1>)
BEMAN_INSIDE_AX_UNARY(atan, true, "", true, ax::atan_core<ax::input_limbs<In>>)
BEMAN_INSIDE_AX_UNARY(sinh,
                      true,
                      "",
                      ax::in_max<In> <= 700,
                      ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::sinh, ax::out_kmax<Out>>)
BEMAN_INSIDE_AX_UNARY(cosh,
                      true,
                      "",
                      ax::in_max<In> <= 700,
                      ax::hyp_core<ax::input_limbs<In>, ax::in_mag<In>, ax::hyp::cosh, ax::out_kmax<Out>>)
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
        using F = ::beman::inside::detail::exact_frac<E>;
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
        using F                    = ::beman::inside::detail::exact_frac<E>;
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

// pow: domain_error for a base ≤ 0; overflow when the result leaves Out
// (without clamp).
template <insidable Out, insidable InB, insidable InE>
[[nodiscard]] constexpr std::expected<Out, errc> pow_into(InB base, InE exp) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InB, InE>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return pow_into<O>(a, b); }, base, exp);
    else {
        const auto integer = [&]() -> std::expected<Out, errc> {
            const auto b = ax::exact_input(base);
            if (b.Num.negative() || b.Num.is_zero())
                return std::unexpected(errc::domain_error);
            using core = ax::pow_core<ax::input_limbs<InB>,
                                      ax::input_limbs<InE>,
                                      ax::in_mag<InE>,
                                      ax::out_kmax<Out>,
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
        using core =
            ax::pow_core<2, ax::input_limbs<In>, ax::in_mag<In>, ax::out_kmax<Out>, 66, ax::input_bits<In>, Base>;
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
    static_assert(has_flag(policy_of<In>, snap),
                  "beman::inside::math: a deduced result is rounded onto the input's grid - its operand "
                  "must permit rounding (declare it with round_nearest, round_floor, ...)");
    static_assert(::beman::inside::detail::notched<In>,
                  "beman::inside::math: a deduced output takes the input's notch - the input needs one");
    return true;
}

namespace auto_t {
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
using sin = inside<{{-1, 1}, notch_of<In>}, ax::auto_policy<In>>;
template <insidable In>
using cos = sin<In>;
template <insidable In>
using tan = inside<{{-1024, 1024}, notch_of<In>}, ax::auto_policy<In>>;

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
using cosh = ax::auto_grid_t<In, cosh_lo<In>, cosh_hi<In>>;

// sqrt of a mixed-sign input: [0, √max(|Lower|, |Upper|)].
template <insidable In>
inline constexpr grid_rational max_abs = (-lower_of<In> > upper_of<In>) ? -lower_of<In> : upper_of<In>;
template <insidable In>
using sqrt_signed =
    ax::auto_grid_t<In,
                    grid_rational{0},
                    ax::lattice_bound<notch_of<In>, true>(
                        ax::sqrt_core<ax::input_limbs<In>, ax::input_bits<In>>{ax::grid_input<In>(max_abs<In>)})>;

// pow_base: pow_core with the base bound.
template <imax Base, std::size_t E, int Mag, int KMax>
struct pow_base_core : ax::pow_core<2, E, Mag, KMax> {
    constexpr pow_base_core(::beman::inside::detail::exact_frac<E> x)
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
using atan2 = inside<{{-pi_up<InY, InX>, pi_up<InY, InX>}, ax::gcd_notch<InY, InX>}, ax::auto_policy<InY>>;

// hypot: [0, hypot of the largest magnitudes] on the gcd notch.
template <insidable InX, insidable InY>
inline constexpr std::size_t hypot_limbs = ::beman::inside::detail::limbs_for_bits(ax::hypot_bits<InX, InY>);
template <insidable InX, insidable InY>
inline constexpr grid_rational hypot_hi = [] {
    using F   = ::beman::inside::detail::exact_frac<hypot_limbs<InX, InY>>;
    const F a = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InX>);
    const F b = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InY>);
    return ax::lattice_bound<ax::gcd_notch<InX, InY>, true>(
        ax::sqrt_core<hypot_limbs<InX, InY>, ax::hypot_bits<InX, InY>>{a * a + b * b});
}();
template <insidable InX, insidable InY>
using hypot = inside<{{0, hypot_hi<InX, InY>}, ax::gcd_notch<InX, InY>}, ax::auto_policy<InX>>;

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
    grid_rational m = pow_corner<InB, InE, false, false, Up>;
    for (const grid_rational& c : {pow_corner<InB, InE, false, true, Up>,
                                   pow_corner<InB, InE, true, false, Up>,
                                   pow_corner<InB, InE, true, true, Up>})
        if (Up ? m < c : c < m)
            m = c;
    return m;
}();
template <insidable InB, insidable InE>
using pow = inside<{{pow_extreme<InB, InE, false>, pow_extreme<InB, InE, true>}, notch_of<InB>}, ax::auto_policy<InB>>;
} // namespace auto_t

#define BEMAN_INSIDE_AX_AUTO(fn)                                                               \
    template <insidable In>                                                                    \
    [[nodiscard]] constexpr auto fn(In x) {                                                    \
        static_assert(deducible<In>());                                                        \
        if constexpr (fn##_domain<In>)                                                         \
            return fn##_into<auto_t::fn<In>>(x);                                               \
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
    return sqrt_into<auto_t::sqrt<In>>(x);
}

template <insidable In>
    requires(lower_of<In> < 0)
[[nodiscard]] constexpr auto sqrt(In x) {
    static_assert(deducible<In>());
    return sqrt_into<auto_t::sqrt_signed<In>>(x);
}

template <imax Base, insidable In>
[[nodiscard]] constexpr auto pow_base(In x) {
    static_assert(deducible<In>());
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
    return pow_base_into<auto_t::pow_base_t<Base, In>, Base>(x);
}

template <insidable InY, insidable InX>
[[nodiscard]] constexpr auto atan2(InY y, InX x) {
    static_assert(deducible<InY>() && deducible<InX>());
    return atan2_into<auto_t::atan2<InY, InX>>(y, x);
}

template <insidable InX, insidable InY>
[[nodiscard]] constexpr auto hypot(InX x, InY y) {
    static_assert(deducible<InX>() && deducible<InY>());
    return hypot_into<auto_t::hypot<InX, InY>>(x, y);
}

template <insidable InB, insidable InE>
    requires(lower_of<InB> > 0)
[[nodiscard]] constexpr auto pow(InB base, InE exp) {
    static_assert(deducible<InB>() && deducible<InE>());
    return pow_into<auto_t::pow<InB, InE>>(base, exp);
}
} // namespace beman::inside::math::adaptive

#undef BEMAN_INSIDE_AX_TIERS
#undef BEMAN_INSIDE_AX_KERNEL_TIERS
#undef BEMAN_INSIDE_AX_TABLE



//---------------------------------------------------------------------------
// beman::inside::math — the transcendental functions (cmath_adaptive.hpp)
// and the grid operations (abs, sign, copysign, floor, ceil, round, trunc,
// fmod, pown).
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

// π as a rational, within 2^-58 (3.1e-18), for the constants below.
inline constexpr rational kPiRat{1068966896, 340262731};
inline constexpr rational kTwoPiRat = 2 * kPiRat;

// Policy of an auto-deduced output: the input's (a cursor's output is no
// cursor).
template <insidable In>
inline constexpr policy_flag out_policy = policy_of<In> & ~cursor_marker;

// A 64-bit grid operation's result through Out's assignment.
template <insidable Out, typename V>
constexpr Out store_value(const V& v) {
    return Out{v};
}
} // namespace detail

// Public irrational constants as point insides, so they compose directly in
// inside-space (`angle * math::pi`) with no rational on the surface.
inline constexpr auto pi     = just<detail::kPiRat>;
inline constexpr auto two_pi = just<detail::kTwoPiRat>;

namespace detail {
// Grid-number helpers for the algebraic tier's deduced outputs: exact for
// grid numbers of any size (C++26), the 64-bit rationals otherwise.
constexpr grid_rational grid_abs(const grid_rational& r) { return r < 0 ? -r : r; }
constexpr grid_rational grid_sign(const grid_rational& r) { return grid_rational{(r > 0) - (r < 0)}; }

// r rounded to an integer by M (half away from zero for nearest).
template <round_mode M>
constexpr grid_rational grid_to_int(const grid_rational& r) {
    const grid_wide n = wide_numerator(r), d = wide_denominator(r);
    grid_wide       q   = n / d;
    const grid_wide rem = n - q * d;
    const bool      neg = n.negative();
    const bool      odd = !(q / grid_wide{2} * grid_wide{2} == q);
    if (rounds_away(M, neg, classify_remainder(M, neg ? -rem : rem, d), odd))
        q = neg ? q - grid_wide{1} : q + grid_wide{1};
#if BEMAN_INSIDE_BIG_GRIDS
    return grid_rational{q};
#else
    return rational{static_cast<imax>(q)};
#endif
}

// max(|Lower|, |Upper|): sizes the abs and copysign outputs.
template <insidable In>
inline constexpr grid_rational abs_auto_upper = grid_abs(lower_of<In>) > grid_abs(upper_of<In>)
                                                    ? grid_abs(lower_of<In>)
                                                    : grid_abs(upper_of<In>);

// The lattice of ±x: x's notch, refined by 2·Lower when x's lattice does
// not pass through 0 (±(0.25 + k/2) lie on 0.25 + ℤ/2; ±(0.5 + k) on ℤ/2).
template <insidable In>
inline constexpr grid_rational sym_notch =
    anchored<In> ? notch_of<In> : grid_gcd_of(notch_of<In>, grid_add(lower_of<In>, lower_of<In>));

// The lowest non-negative point of that lattice: 0 when it passes through 0.
template <insidable In>
inline constexpr grid_rational sym_floor = [] {
    if constexpr (anchored<In>)
        return grid_rational{0};
    else {
        const grid_rational n = sym_notch<In>, l = lower_of<In>;
        // l − n·⌊l/n⌋, with ⌊l/n⌋ from the rounding helper below
        return grid_sub(l, grid_mul(n, grid_to_int<round_mode::floor>(grid_div_of(l, n))));
    }
}();

template <insidable In>
using abs_auto_t = inside<{{sym_floor<In>, abs_auto_upper<In>}, sym_notch<In>}, out_policy<In>>;

// sign(x) ∈ {sign(Lower) … sign(Upper)}, integer notch.
template <insidable In>
using sign_auto_t = inside<{grid_sign(lower_of<In>), grid_sign(upper_of<In>)}, out_policy<In>>;

// copysign(mag, sgn): |mag| with sgn's possible signs. |mag| ranges over
// [m_lo, m_hi] (m_lo = 0 when mag's interval spans 0); ±|mag| lies on
// sym_notch's lattice (mag's own when it passes through 0).
template <insidable Mag>
inline constexpr grid_rational abs_auto_lower =
    (lower_of<Mag> <= 0 && upper_of<Mag> >= 0)            ? sym_floor<Mag>
    : (grid_abs(lower_of<Mag>) < grid_abs(upper_of<Mag>)) ? grid_abs(lower_of<Mag>)
                                                          : grid_abs(upper_of<Mag>);

template <insidable Mag, insidable Sgn>
using copysign_auto_t = inside<{{lower_of<Sgn> < 0 ? -abs_auto_upper<Mag> : abs_auto_lower<Mag>,
                                 upper_of<Sgn> >= 0 ? abs_auto_upper<Mag> : -abs_auto_lower<Mag>},
                                sym_notch<Mag>},
                               out_policy<Mag>>;

template <insidable In, round_mode M>
using integer_auto_t = inside<{{grid_to_int<M>(lower_of<In>), grid_to_int<M>(upper_of<In>)}, 1}, out_policy<In>>;

template <insidable In>
using floor_auto_t = integer_auto_t<In, round_mode::floor>;
template <insidable In>
using ceil_auto_t = integer_auto_t<In, round_mode::ceil>;
template <insidable In>
using round_auto_t = integer_auto_t<In, round_mode::nearest>;
template <insidable In>
using trunc_auto_t = integer_auto_t<In, round_mode::trunc>;

// The exact path: an input or output past the 64-bit rationals (more than
// 2^64 slots, or grid numbers past 64 bits) computes on exact values.
template <insidable... Bs>
inline constexpr bool any_wide_valued = (wide_valued<Bs> || ...);

template <insidable Out, std::size_t E>
constexpr Out store_exact(const exact_frac<E>& v) {
    return ax::store_exact<Out>(v, make_policy<policy_of<Out>>());
}

template <round_mode M, std::size_t E>
constexpr exact_frac<E> exact_to_int(const exact_frac<E>& v) noexcept {
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
        const rational n  = notch64<In>;
        const auto     lo = lower64<In> / n, hi = upper64<In> / n;
        if (!lo || !hi)
            return false;
        const umax m = lo->Numerator > hi->Numerator ? lo->Numerator : hi->Numerator; // max |J|
        return m <= static_cast<umax>(std::numeric_limits<imax>::max()) / n.Numerator;
    }
}();

// x rounded to an integer by M: exactly, on the value index, or through
// rational. Round is half away from zero, as rational round() is.
template <round_mode M, insidable Out, insidable In>
constexpr Out integer_into(In x) {
    if constexpr (any_wide_valued<Out, In>)
        return store_exact<Out>(exact_to_int<M>(ax::exact_input(x)));
    else if constexpr (int_direct<Out, integer_auto_t<In, M>, In>) {
        constexpr imax p = static_cast<imax>(notch64<In>.Numerator);
        constexpr imax q = static_cast<imax>(abs_den(notch64<In>.Denominator));
        return from_value_index<Out>(div_rounded(value_index<imax>(x) * p, q, M));
    } else
        return store_value<Out>(round_to_int(rational{x}, M));
}
} // namespace detail

//---------------------------------------------------------------------------
// Algebraic tier — exact, no polynomial machinery. Each function wraps the
// corresponding `rational` operation and routes through `Out`'s assignment.
//---------------------------------------------------------------------------

// |x|. Output Lower must be ≥ 0 (the result is always non-negative).
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out abs_into(In x) {
    static_assert(lower_of<Out> <= detail::abs_auto_lower<In>,
                  "beman::inside::math::abs: Out must include the smallest |x| (0 when x's range spans 0)");
    if constexpr (detail::any_wide_valued<Out, In>)
        return detail::store_exact<Out>(detail::ax::abs(detail::ax::exact_input(x)));
    else if constexpr (std::same_as<Out, detail::abs_auto_t<In>> && detail::integer_storage<In> &&
                       detail::integer_storage<Out> && detail::notched<In> && detail::anchored<In> &&
                       notch_of<Out> == notch_of<In> && !detail::wide_valued<In>) {
        // The same lattice through 0: |x| is the value index's magnitude.
        const imax j = detail::value_index<imax>(x);
        return detail::from_value_index<Out>(j < 0 ? -j : j);
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
using namespace beman::inside::detail;

// Gate for fmod's integer fast path. When both operands and Out are
// integer-backed on commensurable notches, fmod collapses to ONE integer
// remainder in units of g = gcd(notch of InX, notch of InY): with x
// = a·g and y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ % is truncated division, the same
// convention). Conditions:
//   * integer raws only (rational raws keep the rational path),
//   * non-zero notches, g on Out's grid (g / notch of Out integer),
//   * divisor grid excludes zero (no runtime zero check needed),
//   * Out's interval covers ±max|y| (result magnitude is < |y|),
//   * all unit counts fit comfortably in imax (headroom 4).
template <insidable Out, insidable InX, insidable InY>
inline constexpr bool fmod_int_fast = [] {
    if (!::beman::inside::detail::notched<InX> || !::beman::inside::detail::notched<InY> ||
        !::beman::inside::detail::notched<Out>)
        return false;
    if (!divisor_excludes_zero<InY>)
        return false;
    // Lower/g must be an integer: the lattices pass through 0.
    if (!::beman::inside::detail::anchored<InX> || !::beman::inside::detail::anchored<InY> ||
        !::beman::inside::detail::anchored<Out>)
        return false;
    auto go = gcd(::beman::inside::detail::notch64<InX>, ::beman::inside::detail::notch64<InY>);
    if (!go.has_value())
        return false;
    rational g  = *go;
    auto     qo = g / ::beman::inside::detail::notch64<Out>;
    if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
    rational maxx = abs(::beman::inside::detail::lower64<InX>) > abs(::beman::inside::detail::upper64<InX>)
                        ? abs(::beman::inside::detail::lower64<InX>)
                        : abs(::beman::inside::detail::upper64<InX>);
    rational maxy = abs(::beman::inside::detail::lower64<InY>) > abs(::beman::inside::detail::upper64<InY>)
                        ? abs(::beman::inside::detail::lower64<InY>)
                        : abs(::beman::inside::detail::upper64<InY>);
    if (::beman::inside::detail::lower64<Out> > -maxy || ::beman::inside::detail::upper64<Out> < maxy)
        return false;
    constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
    auto           ux  = maxx / g;
    auto           uy  = maxy / g;
    auto           uo  = maxy / ::beman::inside::detail::notch64<Out>;
    return ux.has_value() && uy.has_value() && uo.has_value() && ux->Numerator <= lim && uy->Numerator <= lim &&
           uo->Numerator <= lim;
}();
} // namespace detail

// x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
// Result has the sign of x. Pre: y != 0 (fmod_into checks it).
template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y) {
    if constexpr (detail::any_wide_valued<Out, InX, InY>) {
        // x − trunc(x/y)·y on exact values.
        constexpr std::size_t E =
            2 * (detail::ax::input_limbs<InX> > detail::ax::input_limbs<InY> ? detail::ax::input_limbs<InX>
                                                                             : detail::ax::input_limbs<InY>)+1;
        using F = beman::inside::detail::exact_frac<E>;
        const F    a{detail::ax::exact_input(x)}, b{detail::ax::exact_input(y)};
        const auto q = (a.Num * b.Den) / (a.Den * b.Num); // truncated
        return detail::store_exact<Out>(a + F{-(q * b.Num), b.Den});
    } else if constexpr (detail::fmod_int_fast<Out, InX, InY>) {
        // One integer remainder in g-units; bit-identical to the rational path.
        constexpr rational g =
            *beman::inside::detail::gcd(::beman::inside::detail::notch64<InX>, ::beman::inside::detail::notch64<InY>);
        constexpr imax wx  = trunc((::beman::inside::detail::notch64<InX> / g).value());
        constexpr imax wy  = trunc((::beman::inside::detail::notch64<InY> / g).value());
        constexpr imax wo  = trunc((g / ::beman::inside::detail::notch64<Out>).value());
        constexpr imax lox = trunc((::beman::inside::detail::lower64<InX> / g).value()); // exact: grid invariant
        constexpr imax loy = trunc((::beman::inside::detail::lower64<InY> / g).value());
        constexpr imax loo =
            trunc((::beman::inside::detail::lower64<Out> / ::beman::inside::detail::notch64<Out>).value());
        const imax a = beman::inside::detail::raw_imax(x) * wx + (beman::inside::detail::index_storage<InX> ? lox : 0);
        const imax b = beman::inside::detail::raw_imax(y) * wy + (beman::inside::detail::index_storage<InY> ? loy : 0);
        const imax r = a % b; // |r| < |b|, in Out's range
        return Out::from_raw(beman::inside::detail::raw_from_offset<Out>(r * wo - loo));
    } else {
        rational xv = x;
        rational yv = y;
        rational q  = xv / yv;
        imax     qt = trunc(q);
        rational qy = qt * yv;
        rational r  = xv - qy;
        return detail::store_value<Out>(r);
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
// Auto-deducing forms — algebraic tier.
//
// Each `fn_into<Out>(x)` has an auto form `fn(x)` that derives `Out` from `In`
// and delegates to it. Notch policy: abs/fmod inherit In's notch; floor/ceil/round/trunc
// deduce notch 1 since their outputs are integer-valued.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// pown<E> — compile-time integer powers, pure grid arithmetic
//---------------------------------------------------------------------------
// Repeated squaring in inside-space: every multiply widens the result grid
// corner-correctly, so the result is exact for exact inputs and negative
// bases are fine. No engine — works on any inside
// (like abs/floor/fmod). Checked rational raws may return
// std::expected<inside, errc> per the usual arithmetic vocabulary. Negative
// exponents are deferred (they need the division error story).
template <imax E, insidable In>
    requires(E >= 0)
[[nodiscard]] constexpr auto pown(In x) noexcept {
    if constexpr (E == 0) {
        (void)x;
        return just<1>;
    } else if constexpr (E == 1)
        return x;
    else if constexpr (E % 2)
        return x * pown<E - 1>(x);
    else {
        auto h = pown<E / 2>(x);
        return h * h;
    }
}

template <insidable In>
[[nodiscard]] constexpr auto abs(In x) {
    return abs_into<detail::abs_auto_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto sign(In x) {
    return sign_into<detail::sign_auto_t<In>>(x);
}

template <insidable Mag, insidable Sgn>
[[nodiscard]] constexpr auto copysign(Mag mag, Sgn sgn) {
    return copysign_into<detail::copysign_auto_t<Mag, Sgn>>(mag, sgn);
}

template <insidable In>
[[nodiscard]] constexpr auto floor(In x) {
    return floor_into<detail::floor_auto_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto ceil(In x) {
    return ceil_into<detail::ceil_auto_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto round(In x) {
    return round_into<detail::round_auto_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto trunc(In x) {
    return trunc_into<detail::trunc_auto_t<In>>(x);
}

namespace detail {
// fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
// both notches (x − k·y lies on that lattice, so the result is exact).
template <insidable InX, insidable InY>
inline constexpr grid_rational fmod_bound =
    abs_auto_upper<InX> < abs_auto_upper<InY> ? abs_auto_upper<InX> : abs_auto_upper<InY>;

template <insidable InX, insidable InY>
using fmod_auto_t = inside<{{(lower_of<InX> < 0 ? -fmod_bound<InX, InY> : grid_rational{0}),
                             (upper_of<InX> > 0 ? fmod_bound<InX, InY> : grid_rational{0})},
                            ax::gcd_notch<InX, InY>},
                           out_policy<InX> | round_nearest>;
} // namespace detail

template <insidable InX, insidable InY>
[[nodiscard]] constexpr auto fmod(InX x, InY y) {
    return fmod_into<detail::fmod_auto_t<InX, InY>>(x, y);
}

//---------------------------------------------------------------------------
// amp<K> — amplitude grid [-1, 1] at 1/K resolution: a ready-made explicit
// output for sin / cos (`math::sin_into<math::amp<32768>>(angle)`), decoupling
// the output precision from the angle's grid; results round to nearest. All
// angles are radians, as in <cmath>.
//---------------------------------------------------------------------------
template <std::uint64_t K>
using amp = inside<{{rational{-1}, rational{1}}, per<K>}, round_nearest>;

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


// ======================================================================
//  beman/inside/formats.hpp
// ======================================================================


//---------------------------------------------------------------------------
// formats — predefined `inside` aliases mapping to hardware byte widths, so you
// can write `beman::inside::byte` / `beman::inside::unorm16` / `beman::inside::q8_8` directly.
//
// The bare `u8`/`i16`/… names are storage-policy flags (policy_flag.hpp); the
// native-width *types* below use width words instead: `byte`/`word`/`dword`
// (unsigned 8/16/32/64: `qword`) and `sbyte`/`sword`/`sdword`/`sqword` (signed 8/16/32/64).
//
// Each alias uses the full range of its native storage type: `byte` is
// [0, 255] in a uint8. Q-format types keep power-of-two notches.
//
// These default to `checked`; for wraparound/saturation declare your own (e.g.
// `inside<{0,254}, wrap>`).
//---------------------------------------------------------------------------
namespace beman::inside {
//-------------------------------------------------------------------------
// Native integer widths — value storage (Raw == value), `checked`.
// Full native range.
//-------------------------------------------------------------------------
using byte  = inside<{0, 255}>;        // uint8
using word  = inside<{0, 65535}>;      // uint16
using dword = inside<{0, 4294967295}>; // uint32
// qword reaches past int64, so it has no implicit `operator imax`; read it
// with `to<std::uint64_t>()`. A difference of qwords spans 2^65 values and
// gets a wide-integer index; a sum's upper bound passes the 64-bit grid
// numbers, so it needs C++26 big grids.
using qword = inside<{0, 18446744073709551615u}>; // uint64

using sbyte  = inside<{-128, 127}>;               // int8
using sword  = inside<{-32768, 32767}>;           // int16
using sdword = inside<{-2147483648, 2147483647}>; // int32
// sqword stays symmetric: the internal value path is `imax`, and -2^63
// has no negation in int64.
using sqword = inside<{-9223372036854775807, 9223372036854775807}>; // int64

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
using q4_4   = inside<{{0, 15}, per<16>}, round_nearest>;       // uint8
using q8_8   = inside<{{0, 255}, per<256>}, round_nearest>;     // uint16
using q16_16 = inside<{{0, 65535}, per<65536>}, round_nearest>; // uint32

//-------------------------------------------------------------------------
// Counters — a counter is an inside over [0, Max] whose overflow policy says
// what `++` does at the ceiling (the boundary behavior is in the type).
//-------------------------------------------------------------------------
// Saturating counter: `++` caps at Max (never throws or wraps) — the honest
// "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
template <umax Max>
using counter = inside<{0, Max}, clamp>;
// Modular / ring counter: `++` wraps Max → 0 (sequence numbers, epochs).
template <umax Max>
using ring_counter = inside<{0, Max}, wrap>;

} // namespace beman::inside


#ifndef BEMAN_INSIDE_NO_STRING

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
//   * to_string(rational | interval | grid | insidable | arithmetic)
//   * to_string_debug(insidable)        — value + raw + raw-type + grid
//   * operator<<(std::ostream&, ...)
//   * std::formatter<inside<G,P>> / std::formatter<rational>   (when <format>)
//---------------------------------------------------------------------------


#include <string>
#include <string_view>
#include <istream>
#include <ostream>
#include <version>

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

// num/den (den > 0) in the forms from_chars reads: its exact decimal when the
// reduced denominator is 2^a·5^b (however many digits, and at least
// min_digits decimals), else "num/den".
template <std::size_t K>
std::string fraction_to_string(bool neg, wide_uint<K> num, wide_uint<K> den, int min_digits = 0) {
    using U = wide_uint<K>;
    U g     = den;
    for (U x = num; !x.is_zero();) {
        const U t = g % x;
        g         = x;
        x         = t;
    }
    num /= g;
    den /= g;
    std::string str  = neg && !num.is_zero() ? "-" : "";
    int         twos = 0, fives = 0;
    U           rest = den;
    for (; (rest.Word[0] & 1) == 0; rest >>= 1)
        ++twos;
    for (auto d = divmod_small(rest, 5); d.Remainder == 0; d = divmod_small(rest, 5)) {
        rest = d.Quotient;
        ++fives;
    }
    if (!(rest == U{1}))
        return str += wide_to_decimal(num) + "/" + wide_to_decimal(den);
    // num·10^digits/den is an integer; 19 digits at a time keep the scratch
    // below den·10^19.
    const auto [whole, frac] = U::divmod(num, den);
    str += wide_to_decimal(whole);
    int digits = twos > fives ? twos : fives;
    if (digits < min_digits)
        digits = min_digits;
    if (digits == 0)
        return str;
    str += '.';
    using W = wide_uint<K + 1>;
    W r{frac};
    for (int left = digits; left > 0; left -= 19) {
        const int chunk = left < 19 ? left : 19;
        umax      p     = 1;
        for (int i = 0; i < chunk; ++i)
            p *= 10;
        const auto [q, m]      = W::divmod(r * W{p}, W{den});
        const std::string part = std::to_string(static_cast<umax>(q));
        str.append(static_cast<std::size_t>(chunk) - part.size(), '0') += part;
        r = m;
    }
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
        grid_wide q   = wide_denominator(grid_of<B>.value_unit());
        int       two = 0, five = 0;
        for (; q % grid_wide{2} == grid_wide{0}; q = q / grid_wide{2})
            ++two;
        for (; q % grid_wide{5} == grid_wide{0}; q = q / grid_wide{5})
            ++five;
        return q == grid_wide{1} && five > 0 ? (two > five ? two : five) : 0;
    }
}();
} // namespace detail

#if BEMAN_INSIDE_BIG_GRIDS
// A grid number of any size, in the forms of to_string(rational).
[[nodiscard]] inline std::string to_string(const detail::big_rational& r);
#endif

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
    str += "}";
    return str;
}

// delegate to std::to_string
template <typename V>
[[nodiscard]] auto to_string(V value) {
    return std::to_string(value);
}

//-------------------------------------------------------------------------
// type_name_v<T> — short raw-type label for to_string_debug. Lives here (not
// in the core math header) so the core never pulls <string_view>.
//-------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr std::string_view type_name_v = is_wide_int_v<T>     ? "wide_int"
                                                : is_exact_frac_v<T> ? "exact_frac"
                                                                     : "unknown";
template <>
inline constexpr std::string_view type_name_v<std::uint8_t> = "uint8_t";
template <>
inline constexpr std::string_view type_name_v<std::uint16_t> = "uint16_t";
template <>
inline constexpr std::string_view type_name_v<std::uint32_t> = "uint32_t";
template <>
inline constexpr std::string_view type_name_v<std::uint64_t> = "uint64_t";
template <>
inline constexpr std::string_view type_name_v<std::int8_t> = "int8_t";
template <>
inline constexpr std::string_view type_name_v<std::int16_t> = "int16_t";
template <>
inline constexpr std::string_view type_name_v<std::int32_t> = "int32_t";
template <>
inline constexpr std::string_view type_name_v<std::int64_t> = "int64_t";
template <>
inline constexpr std::string_view type_name_v<rational> = "rational";
template <>
inline constexpr std::string_view type_name_v<point_slot> = "point";
template <>
inline constexpr std::string_view type_name_v<wide_uint<2>> = "wide_uint<2>";
template <>
inline constexpr std::string_view type_name_v<wide_uint<3>> = "wide_uint<3>";
} // namespace detail

//-------------------------------------------------------------------------
// to_string / to_string_debug / operator<< for insidables and rational.
// The debug form also prints the raw value, raw type, and grid — useful when
// inspecting failing tests or storage choices.
//-------------------------------------------------------------------------
namespace detail {
// An exact value, with at least min_digits decimals.
template <std::size_t K>
std::string exact_to_string(exact_frac<K> f, int min_digits = 0) {
    const bool neg = f.Num.negative();
    return fraction_to_string(neg, wide_uint<K>{neg ? -f.Num : f.Num}, wide_uint<K>{f.Den}, min_digits);
}
} // namespace detail

template <std::size_t N, bool S>
[[nodiscard]] inline std::string to_string(detail::wide_int<N, S> v) {
    return detail::wide_to_decimal(v);
}

#if BEMAN_INSIDE_BIG_GRIDS
// A grid number of any size, in decimal (reads the interned limbs; no new
// big values are formed at runtime).
[[nodiscard]] inline std::string to_string(const detail::big_int& v) {
    detail::big::mag m = v.magnitude();
    std::string      out;
    do {
        umax rem = 0; // m /= 10^19, rem = m % 10^19
        for (std::size_t i = m.size(); i-- > 0;) {
            const auto d = detail::limb::div(rem, m[i], umax{10'000'000'000'000'000'000ull});
            m[i]         = d.Hi;
            rem          = d.Lo;
        }
        detail::big::trim(m);
        std::string part = std::to_string(rem);
        if (!m.empty())
            part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
    } while (!m.empty());
    return v.negative() ? "-" + out : out;
}
#endif

#if BEMAN_INSIDE_BIG_GRIDS
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
#endif

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
    str += "[" + std::string(detail::type_name_v<detail::raw_t<B>>);
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

template <insidable B>
inline std::ostream& operator<<(std::ostream& stream, B b) {
    stream << beman::inside::to_string(b);
    return stream;
}

// from_chars<B, F>(text) / from_chars_exact<B>(text) — the std::string_view
// forms of the pointer-pair overloads.
template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> from_chars(std::string_view text) {
    return from_chars<B, F>(text.data(), text.data() + text.size());
}
template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> from_chars_exact(std::string_view text) {
    return from_chars_exact<B>(text.data(), text.data() + text.size());
}

// Reads one whitespace-delimited token and parses it with from_chars<B>. On an
// error the stream's failbit is set and `b` is left unchanged.
template <insidable B>
inline std::istream& operator>>(std::istream& stream, B& b) {
    std::string token;
    if (!(stream >> token))
        return stream;
    if (const auto r = from_chars<B>(token); r)
        b = *r;
    else
        stream.setstate(std::ios_base::failbit);
    return stream;
}

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration is gated on a working <format>; without it this
// compiles as a no-op and to_string()/operator<< remain.
//---------------------------------------------------------------------------
#ifdef __cpp_lib_format

    #include <format>
    #include <type_traits>

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
} // namespace beman::inside::detail

namespace beman::inside::detail {
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
        for (char& c : s)
            if (c == 'e')
                c = 'E';
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
} // namespace beman::inside::detail

namespace beman::inside::detail {
// The rounding a format spec applies to B: B's own mode when its policy
// names one (as storing at that precision would round), else ties to even.
template <insidable B>
inline constexpr round_mode display_rounding =
    has_any_flag(policy_of<B>, round_floor | round_ceil | round_nearest | round_half_even | snap)
        ? rounding_of(policy_of<B>)
        : round_mode::half_even;

// x's exact value as a sign and a magnitude fraction, for the exact specs.
template <std::size_t K>
bool spec_negative(const exact_frac<K>& f) {
    return f.Num.negative();
}
template <std::size_t K>
wide_uint<K> spec_magnitude(const exact_frac<K>& f) {
    return wide_uint<K>{f.Num.negative() ? -f.Num : f.Num};
}
template <std::size_t K>
wide_uint<K> spec_denominator(const exact_frac<K>& f) {
    return wide_uint<K>{f.Den};
}
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
        else {
            const auto  v     = d::exact_of(b);
            std::string plain = beman::inside::to_string(b);
            if (!plain.empty() && plain[0] == '-')
                plain.erase(0, 1);
            const bool neg = d::spec_negative(v);
            return this->Numeric.write(
                neg,
                this->Numeric.body(d::spec_magnitude(v), d::spec_denominator(v), plain, neg, d::display_rounding<B>),
                ctx);
        }
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
        std::string plain = beman::inside::to_string(r);
        if (!plain.empty() && plain[0] == '-')
            plain.erase(0, 1);
        const bool neg = r.Denominator < 0 && r.Numerator != 0;
        return Numeric.write(neg,
                             Numeric.body(d::wide_uint<1>{r.Numerator},
                                          d::wide_uint<1>{d::abs_den(r.Denominator)},
                                          plain,
                                          neg,
                                          d::round_mode::half_even),
                             ctx);
    }
};

#endif // __cpp_lib_format


#endif // BEMAN_INSIDE_NO_STRING

// ======================================================================
//  beman/inside/numeric_limits.hpp
// ======================================================================



//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specialisations for inside<G, P>.
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

    static constexpr B min() noexcept { return B{::beman::inside::detail::lower64<B>}; }
    static constexpr B max() noexcept { return B{::beman::inside::detail::upper64<B>}; }
    static constexpr B lowest() noexcept { return B{::beman::inside::detail::lower64<B>}; }
    // Exact types have no rounding noise — epsilon and round_error are 0 when
    // 0 is on the grid. A lattice that misses 0 but spans it gives its first
    // point above 0; when 0 is outside the interval, the grid minimum — the
    // closest representable stand-in for "no error" the type can express.
    static constexpr B epsilon() noexcept {
        if constexpr (G.representable(beman::inside::detail::grid_rational{0}))
            return B{beman::inside::detail::rational{0}};
        else if constexpr (G.Interval.Lower < beman::inside::detail::rational{0} &&
                           beman::inside::detail::rational{0} < G.Interval.Upper) {
            constexpr auto lo = ::beman::inside::detail::lower64<B>, n = ::beman::inside::detail::notch64<B>;
            return B{
                (lo + (beman::inside::detail::rational{beman::inside::detail::ceil((-lo / n).value())} * n).value())
                    .value()};
        } else
            return B{::beman::inside::detail::lower64<B>};
    }
    static constexpr B round_error() noexcept { return epsilon(); }
};

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::hash<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    // Boost-style combine of h with the limbs of a wide integer.
    static constexpr std::size_t combine(std::size_t h, const auto& w) noexcept {
        for (auto limb : w.Word)
            h ^= std::hash<beman::inside::umax>{}(limb) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }

    constexpr std::size_t operator()(const B& b) const noexcept {
        if constexpr (beman::inside::detail::point_storage<B>)
            return 0; // one value: the type
        else if constexpr (beman::inside::detail::fraction_storage<B>)
            return combine(combine(0, b.raw().Num), b.raw().Den); // a reduced fraction
        else if constexpr (beman::inside::detail::rational_storage<B>) {
            // Boost-style hash combine over (Numerator, Denominator).
            auto h1 = std::hash<beman::inside::umax>{}(b.raw().Numerator);
            auto h2 = std::hash<beman::inside::imax>{}(b.raw().Denominator);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        } else if constexpr (beman::inside::detail::wide_index_storage<B>) {
            return combine(0, b.raw());
        } else
            return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
    }
};


#if __STDC_HOSTED__ && !defined(BEMAN_INSIDE_MATH_NO_FP)

// ======================================================================
//  beman/inside/random.hpp
// ======================================================================
// Opt-in: uniform sampling over a grid. `uniform<B>(rng)` returns a B drawn
// uniformly from the grid's slots (Lower, Lower + Notch, …, Upper) — exact, any
// storage. Kept out of the umbrella because <random> is heavy, hosted-only and
// pulls <cmath> (so the single header drops it under BEMAN_INSIDE_MATH_NO_FP).
//---------------------------------------------------------------------------


#include <random>

namespace beman::inside {
template <insidable B, std::uniform_random_bit_generator G>
[[nodiscard]] B uniform(G& g) {
    static_assert(detail::notched<B> || detail::point_grid<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    if constexpr (detail::wide_index_storage<B>) {
        // More than 2^64 slots: draw limbs uniformly, masked to the slot count's
        // bit width, and reject draws past the count (accepts > 1/2 of draws).
        using W                = detail::raw_t<B>;
        constexpr W   count    = static_cast<W>(grid_of<B>.slot_count());
        constexpr int top_bits = grid_of<B>.slot_bits() - 64 * (static_cast<int>(sizeof(W) / 8) - 1);
        std::uniform_int_distribution<umax> limb;
        for (;;) {
            W k;
            for (auto& w : k.Word)
                w = limb(g);
            if constexpr (top_bits < 64)
                k.Word[sizeof(W) / 8 - 1] &= (umax{1} << top_bits) - 1;
            if (!(k > count))
                return B::from_raw(k);
        }
    } else {
        std::uniform_int_distribution<umax> pick(0, detail::max_index_v<B>);
        const umax                          k = pick(g);
        return B::from_raw(detail::raw_from_offset<B>(k)); // index or value storage
    }
}
} // namespace beman::inside


#endif // __STDC_HOSTED__ && !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_SINGLE_HEADER_HPP
