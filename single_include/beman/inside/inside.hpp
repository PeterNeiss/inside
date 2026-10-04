// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// beman.inside 1.0.0 — single-header amalgamation
//
//   *** GENERATED FILE — DO NOT EDIT BY HAND ***
//
// Regenerate with:  cmake --build <build-dir> --target amalgamate
// Source of truth:  include/beman/inside/*.hpp, include/beman/inside/detail/*.hpp
//
// Copyright (C) 2026 Peter Neiss
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
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

// ======================================================================
//  beman/inside/inside.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Public umbrella header. Include this to get the full `beman::inside::inside` API:
// the core type (core.hpp) plus the free-function layers that depend on the
// complete type — casts, arithmetic operators, and inside_range. Those three
// must follow core.hpp because they need `inside<G, P>` fully defined.
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/core.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
// Defines the core `beman::inside::inside<G, P>` type; the umbrella adds the free-function
// casts/arithmetic/range layers that depend on this complete type.
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/generic.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/detail/debug.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

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
#  define BEMAN_INSIDE_COLD     [[gnu::cold]]
#  define BEMAN_INSIDE_NOINLINE [[gnu::noinline]]
#  define BEMAN_INSIDE_TRAP()   __builtin_trap()
#elif defined(_MSC_VER)
#  define BEMAN_INSIDE_COLD
#  define BEMAN_INSIDE_NOINLINE __declspec(noinline)
#  define BEMAN_INSIDE_TRAP()   __debugbreak()
#else
#  define BEMAN_INSIDE_COLD
#  define BEMAN_INSIDE_NOINLINE
#  include <cstdlib>
#  define BEMAN_INSIDE_TRAP()   ::abort()
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
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // error codes
  //---------------------------------------------------------------------------
  enum class errc
  {
    domain_error = 1,   // argument outside a function's mathematical domain
    division_by_zero,   // divisor is zero
    overflow,           // value does not fit its destination's range (an inside's
                        // interval, a native type, a rational's 64-bit fields)
    rounding_error,     // notch incompatibility
    not_finite,         // non-finite double input (NaN/Inf)
    invalid_format,     // text that is not a number (from_chars, operator>>)
  };

  // Static, allocation-free message per code. The single source of truth for
  // every error path; returns a null-terminated string literal so it doubles as
  // the default exception's what() and as an on_error message.
  constexpr const char* errc_message(errc e) noexcept
  {
    switch (e)
    {
      case errc::domain_error:     return "argument outside the function's domain";
      case errc::division_by_zero: return "division by zero";
      case errc::overflow:         return "value does not fit its range";
      case errc::rounding_error:   return "notch incompatibility";
      case errc::not_finite:       return "non-finite floating-point value";
      case errc::invalid_format:   return "malformed number";
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
  struct inside_error : std::runtime_error
  {
    errc Code;
    explicit inside_error(errc c)
      : std::runtime_error(errc_message(c)), Code(c) {}
    inside_error(errc c, const char* what)
      : std::runtime_error(what ? what : errc_message(c)), Code(c) {}
  };
#endif

  //---------------------------------------------------------------------------
  // Replaceable failure handler. Contract: it must NOT return (throw / longjmp /
  // abort / reset). If it does return, `raise` traps to honour [[noreturn]].
  //---------------------------------------------------------------------------
  using error_handler_t = void (*)(errc code, const char* what);

  namespace detail
  {
    [[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE
    inline void default_error_handler(errc code, const char* what)
    {
#if BEMAN_INSIDE_HAS_EXCEPTIONS
      throw inside_error(code, what);
#else
      (void)code; (void)what;
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
  inline error_handler_t set_error_handler(error_handler_t h) noexcept
  {
    error_handler_t prev = detail::g_error_handler;
    detail::g_error_handler = h ? h : &detail::default_error_handler;
    return prev;
  }

  inline error_handler_t get_error_handler() noexcept
  { return detail::g_error_handler; }

  namespace detail
  {
    //-----------------------------------------------------------------------
    // Outlined failure funnel. Cold + non-inline so the (rare) machinery is
    // emitted once, off the hot path. Not constexpr: calling it during constant
    // evaluation is ill-formed, which is exactly how the checked compile-time
    // paths hard-fail the build (the old `throw` did the same).
    //-----------------------------------------------------------------------
    [[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE
    inline void raise(errc code, const char* what = nullptr)
    {
      g_error_handler(code, what ? what : errc_message(code));
      BEMAN_INSIDE_TRAP();   // handler must not return; trap if it did
    }

    //-----------------------------------------------------------------------
    // Compile-time-only diagnostic. A fixed_string NTTP carries the message
    // into the type, so a malformed literal / overflow aborts constant
    // evaluation with the text in the instantiation — no `throw` token, so it
    // also compiles under -fno-exceptions. Never reached at runtime (all call
    // sites are guarded by std::is_constant_evaluated / are consteval).
    //-----------------------------------------------------------------------
    template <unsigned N>
    struct fixed_string
    {
      char Data[N]{};
      constexpr fixed_string(const char (&s)[N])
      { for (unsigned i = 0; i < N; ++i) Data[i] = s[i]; }
    };

    template <fixed_string Msg>
    [[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE
    inline void constexpr_error()
    { raise(errc::overflow, Msg.Data); }

    // Debug helper: instantiating print_types<Ts...> fails and names Ts.
    template <typename... Ts>
    struct print_types
    {
        static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
    };
  } // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/lift.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



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
namespace beman::inside
{
  namespace detail
  {
    template <class T> struct is_expected : std::false_type {};
    template <class T, class E> struct is_expected<std::expected<T, E>> : std::true_type {};

    template <class T>
    inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

    // Any std::expected specialization (cv/ref-stripped).
    template <typename T>
    concept expected_like = is_expected_v<T>;

    // strip expected<X, E> down to X, leave non-expected unchanged
    template <class T> struct unwrap { using type = T; };
    template <class T, class E> struct unwrap<std::expected<T, E>> { using type = T; };
    template <class T> using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

    template <class T>
    constexpr decltype(auto) lift_unwrap(T&& v)
    {
      if constexpr (is_expected_v<T>)
        return *std::forward<T>(v);
      else
        return std::forward<T>(v);
    }

    // Copy an arg's error into `e`; true if the arg holds one.
    template <class T>
    constexpr bool lift_take_error([[maybe_unused]] T const& v, [[maybe_unused]] errc& e)
    {
      if constexpr (is_expected_v<T>)
        if (!v.has_value()) { e = v.error(); return true; }
      return false;
    }
  }

  //---------------------------------------------------------------------------
  // detail::lift(op, args...) — call op on the unwrapped args → expected<result,
  // errc>; the first erroneous arg (left to right) short-circuits with its error.
  // An op already returning expected<R, errc> passes through. Internal: it backs
  // the expected-lift operators; user code chains through those.
  //---------------------------------------------------------------------------
  namespace detail
  {
  template <class Op, class... Args>
  [[nodiscard]] constexpr auto lift(Op op, Args&&... args)
  {
    using R = std::remove_cvref_t<
        decltype(op(detail::lift_unwrap(std::forward<Args>(args))...))>;
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
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/detail/rational.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/math.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// math — primitive numeric utilities: umax/imax, smallest_uint_for /
// smallest_int_for (grid storage selection), the arithmetic/fractional concepts,
// safe_abs, constexpr frexp/ldexp, and abs_fraction (the double → rational
// engine behind rational(double)).
//---------------------------------------------------------------------------
namespace beman::inside
{
  using umax = std::uint64_t;
  using imax = std::int64_t;

  namespace detail { struct rational; }

  namespace detail
  {
    // A plain number a grid-less value can be: integral, floating point, or the
    // library's exact rational. Internal; the public operand concept is `numeric`.
    template<typename T>
    concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;
  }

  namespace detail
  {

  // Smallest unsigned type whose range holds every index 0..N.
  template <std::uintmax_t N>
  using smallest_uint_for_t =
    std::conditional_t<(N == 0), rational,
    std::conditional_t<(N <= UINT8_MAX),  std::uint8_t,
    std::conditional_t<(N <= UINT16_MAX), std::uint16_t,
    std::conditional_t<(N <= UINT32_MAX), std::uint32_t,
                                           std::uint64_t>>>>;

  // Smallest signed type whose range holds Low..High.
  template <std::intmax_t Low, std::intmax_t High>
  using smallest_int_for_t =
    std::conditional_t<(Low >= INT8_MIN  && High <= INT8_MAX),  std::int8_t,
    std::conditional_t<(Low >= INT16_MIN && High <= INT16_MAX), std::int16_t,
    std::conditional_t<(Low >= INT32_MIN && High <= INT32_MAX), std::int32_t,
                                                                 std::int64_t>>>;

  // (type_name<T>() — used only by the debug stringifier — lives in
  // "beman/inside/io.hpp" so the core stays free of <string_view>.)

  // Subset of arithmetic excluding integrals — the rhs types that need the
  // rational-arithmetic assignment path.
  template<typename T>
  concept fractional = std::floating_point<T> || std::same_as<rational, T>;

  template <std::signed_integral V>
  [[nodiscard]] constexpr umax safe_abs(V value) noexcept
  { return (value >= 0) ? static_cast<umax>(value) : umax{0} - static_cast<umax>(value); }

  inline constexpr double frexp(double value, int* exp) noexcept
  {
    if (value == 0.0)
    {
        *exp = 0;
        return value;
    }

    auto bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    auto e = static_cast<int>((bits >> 52) & 0x7FF);

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

      auto bits = std::bit_cast<std::uint64_t>(value);
      constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
      constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
      auto e = static_cast<int>((bits >> 52) & 0x7FF);

      if (e == 0x7FF)
          return value; // inf or NaN

      // Normalize subnormals
      int extra = 0;
      if (e == 0) {
          bits = std::bit_cast<std::uint64_t>(value * 0x1p53);
          e = static_cast<int>((bits >> 52) & 0x7FF);
          extra = -53;
      }

      int new_exp = e + exp + extra;

      if (new_exp >= 0x7FF) {
          // overflow → ±inf
          return (bits & sign_mask) ? -std::numeric_limits<double>::infinity()
                                   : std::numeric_limits<double>::infinity();
      }

      if (new_exp > 0) {
          // normal result
          bits = (bits & (sign_mask | mantissa_mask))
               | (static_cast<std::uint64_t>(new_exp) << 52);
          return std::bit_cast<double>(bits);
      }

      // Subnormal or underflow
      auto mantissa = (bits & mantissa_mask) | (std::uint64_t{1} << 52);
      int shift = 1 - new_exp;

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
  constexpr std::pair<umax, umax> abs_fraction(double value)
  {
    if (not is_finite(value))
      raise(errc::not_finite, "beman::inside::detail::abs_fraction: non-finite double");

    if (value == 0.0) return {0, 1};
    if (value < 0)    value = -value;        // |value|; sign is the caller's job

    const auto bits = std::bit_cast<std::uint64_t>(value);
    const int  e    = static_cast<int>((bits >> 52) & 0x7FF);
    umax significand = bits & 0x000F'FFFF'FFFF'FFFF;
    int  exp2;
    if (e == 0)                              // subnormal: no implicit leading 1
      exp2 = 1 - 1023 - 52;
    else                                     // normal: restore the implicit bit
    {
      significand |= (umax{1} << 52);
      exp2 = e - 1023 - 52;
    }

    // value == significand * 2^exp2. Re-express as num/den with den = 2^k.
    if (exp2 >= 0)                           // integer-valued: scale up, den = 1
    {
      // |value| ≥ 2^64 has no 64-bit numerator: fail rather than wrap the shift.
      if (exp2 > 64 - std::bit_width(significand))
        raise(errc::overflow, "beman::inside::detail::abs_fraction: |double| >= 2^64");
      return {significand << exp2, 1};
    }

    // exp2 < 0 → den = 2^(-exp2), capped at 2^62 (den is stored signed). Beyond
    // the cap the value is too small to keep: drop the significand's low bits
    // (shift ≥ 64 folds to zero). Reachable for every subnormal and normals
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalises to 0/1).
    int den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow)
    {
      const int drop = den_pow - max_pow;
      significand = (drop >= 64) ? 0 : (significand >> drop);
      // Return canonical {0,1}: callers take this verbatim, so a non-canonical
      // {0, 2^62} would break the structural rational::operator==.
      if (significand == 0) return {0, 1};
      den_pow = max_pow;
    }
    umax den = umax{1} << den_pow;

    // den is a power of two, so the whole reduction is one shift by the
    // shared factor count (bounded by den's exponent). Plain ternary — no
    // <algorithm> in this core header (libc++ does not provide std::min
    // transitively).
    if (significand != 0)
    {
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
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// This is derived from Peter Sommerlads odins.h, allowed by MIT license
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// overflow — overflow-detecting add/sub/mul for integers: the GCC/Clang
// __builtin_*_overflow intrinsics. Return true on overflow; *result holds the
// wrapped value either way. Used by rational::*_impl and every checked path.
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <std::integral T>
  [[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return __builtin_add_overflow(l, r, result); }

  template <std::integral T>
  [[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept
  { return __builtin_sub_overflow(l, r, result); }

  template <std::integral T>
  [[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept
  { return __builtin_mul_overflow(l, r, result); }
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/wide_int.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



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
namespace beman::inside::detail
{
  namespace limb
  {
    template <std::unsigned_integral L>
    inline constexpr int bits = std::numeric_limits<L>::digits;

    template <std::unsigned_integral L>
    struct pair { L Hi; L Lo; };

    // a + b + carry; carry is 0 or 1 in and out.
    template <std::unsigned_integral L>
    constexpr L add_carry(L a, L b, L& carry) noexcept
    {
      const L s  = static_cast<L>(a + b);
      const L s2 = static_cast<L>(s + carry);
      carry = static_cast<L>((s < a) + (s2 < s));
      return s2;
    }

    // a − b − borrow; borrow is 0 or 1 in and out.
    template <std::unsigned_integral L>
    constexpr L sub_borrow(L a, L b, L& borrow) noexcept
    {
      const L d  = static_cast<L>(a - b);
      const L d2 = static_cast<L>(d - borrow);
      borrow = static_cast<L>((a < b) + (d < borrow));
      return d2;
    }

    // Full product a·b as {hi, lo}. Native where the target has unsigned
    // __int128; else a schoolbook 32-bit split (32-bit targets).
    template <std::unsigned_integral L>
    constexpr pair<L> mul(L a, L b) noexcept
    {
      if constexpr (bits<L> <= 32)
      {
        const std::uint64_t p = std::uint64_t{a} * b;
        return {static_cast<L>(p >> bits<L>), static_cast<L>(p)};
      }
      else
      {
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
    constexpr std::strong_ordering mul_compare(umax a, umax b, umax c, umax d) noexcept
    {
#if defined(__SIZEOF_INT128__)
      return static_cast<unsigned __int128>(a) * b <=> static_cast<unsigned __int128>(c) * d;
#else
      const pair<umax> x = mul(a, b), y = mul(c, d);
      return x.Hi != y.Hi ? x.Hi <=> y.Hi : x.Lo <=> y.Lo;
#endif
    }

    // {hi, lo} / d and the remainder. Requires hi < d, so the quotient fits L.
    template <std::unsigned_integral L>
    constexpr pair<L> div(L hi, L lo, L d) noexcept   // {quotient, remainder}
    {
      if constexpr (bits<L> <= 32)
      {
        const std::uint64_t n = (std::uint64_t{hi} << bits<L>) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
      }
      else
      {
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
#else
        // Restoring shift-subtract; `top` keeps the bit shifted out of r.
        L r = hi, q = 0;
        for (int i = 63; i >= 0; --i)
        {
          const bool top = (r >> 63) != 0;
          r = (r << 1) | ((lo >> i) & 1u);
          q <<= 1;
          if (top || r >= d) { r -= d; q |= 1u; }
        }
        return {q, r};
#endif
      }
    }

    // Knuth's algorithm D (TAOCP 4.3.1). u has m limbs, v has n limbs with
    // v[n−1] != 0 and m >= n. Writes the m−n+1 quotient limbs to q and the n
    // remainder limbs to r. The caller provides scratch: un (m+1) and vn (n).
    template <std::unsigned_integral L>
    constexpr void divmod(const L* u, std::size_t m, const L* v, std::size_t n,
                          L* q, L* r, L* un, L* vn) noexcept
    {
      constexpr int B = bits<L>;
      if (n == 1)
      {
        L rem = 0;
        for (std::size_t i = m; i-- > 0;)
        {
          const pair<L> d = div(rem, u[i], v[0]);
          q[i] = d.Hi;
          rem = d.Lo;
        }
        r[0] = rem;
        return;
      }
      // Normalise: shift so the divisor's top limb has its high bit set.
      const int s = std::countl_zero(v[n - 1]);
      const auto shl = [&](L hi, L lo) -> L
      { return s == 0 ? hi : static_cast<L>(static_cast<L>(hi << s) | static_cast<L>(lo >> (B - s))); };
      for (std::size_t i = n - 1; i > 0; --i) vn[i] = shl(v[i], v[i - 1]);
      vn[0] = static_cast<L>(v[0] << s);
      un[m] = (s == 0) ? L{0} : static_cast<L>(u[m - 1] >> (B - s));
      for (std::size_t i = m - 1; i > 0; --i) un[i] = shl(u[i], u[i - 1]);
      un[0] = static_cast<L>(u[0] << s);

      for (std::size_t j = m - n + 1; j-- > 0;)
      {
        // Estimate q̂ from the top two dividend limbs, then correct it with the
        // next limb (at most two steps).
        L qhat, rhat;
        bool rhat_big = false;                          // r̂ ≥ base: skip the test
        if (un[j + n] == vn[n - 1])
        {
          qhat = static_cast<L>(~L{0});
          rhat = static_cast<L>(un[j + n - 1] + vn[n - 1]);
          rhat_big = rhat < vn[n - 1];
        }
        else
        {
          const pair<L> d = div(un[j + n], un[j + n - 1], vn[n - 1]);
          qhat = d.Hi;
          rhat = d.Lo;
        }
        while (!rhat_big)
        {
          const pair<L> p = mul(qhat, vn[n - 2]);
          if (p.Hi < rhat || (p.Hi == rhat && p.Lo <= un[j + n - 2])) break;
          --qhat;
          const L old = rhat;
          rhat = static_cast<L>(rhat + vn[n - 1]);
          rhat_big = rhat < old;
        }
        // Multiply and subtract q̂·vn from un[j .. j+n].
        L carry = 0, borrow = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
          const pair<L> p = mul(qhat, vn[i]);
          L c = 0;
          const L lo = add_carry(p.Lo, carry, c);
          carry = static_cast<L>(p.Hi + c);
          un[i + j] = sub_borrow(un[i + j], lo, borrow);
        }
        un[j + n] = sub_borrow(un[j + n], carry, borrow);
        if (borrow != 0)                                // q̂ was one too large: add back
        {
          --qhat;
          L c = 0;
          for (std::size_t i = 0; i < n; ++i) un[i + j] = add_carry(un[i + j], vn[i], c);
          un[j + n] = static_cast<L>(un[j + n] + c);
        }
        q[j] = qhat;
      }
      // Denormalise the remainder.
      for (std::size_t i = 0; i + 1 < n; ++i)
        r[i] = (s == 0) ? un[i]
                        : static_cast<L>(static_cast<L>(un[i] >> s) | static_cast<L>(un[i + 1] << (B - s)));
      r[n - 1] = static_cast<L>(un[n - 1] >> s);
    }
  } // namespace limb

  template <std::size_t N, bool Signed, std::unsigned_integral L = umax>
  struct wide_int
  {
    static_assert(N >= 1, "wide_int: at least one limb");
    static constexpr int limb_bits = limb::bits<L>;
    static constexpr int bits = static_cast<int>(N) * limb_bits;
    static constexpr bool is_signed = Signed;

    L Word[N];                                          // little-endian limbs

    wide_int() = default;

    // From a builtin integer: sign-extends a negative signed value, as the
    // builtin conversions do.
    template <std::integral T>
    constexpr wide_int(T v) noexcept
    {
#if defined(__SIZEOF_INT128__)
      using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
      using W = std::uint64_t;
#endif
      using S = std::make_signed_t<W>;
      constexpr int wbits = std::numeric_limits<W>::digits;
      const W w = std::is_signed_v<T> ? static_cast<W>(static_cast<S>(v)) : static_cast<W>(v);
      const L fill = (std::is_signed_v<T> && v < 0) ? static_cast<L>(~L{0}) : L{0};
      for (std::size_t i = 0; i < N; ++i)
        Word[i] = (static_cast<int>(i) * limb_bits < wbits)
                ? static_cast<L>(w >> (static_cast<int>(i) * limb_bits)) : fill;
    }

    // Between widths: sign-extends a signed source, truncates a wider one.
    // Implicit only when it widens.
    template <std::size_t M, bool S2>
      requires (M != N || S2 != Signed)
    constexpr explicit(M >= N) wide_int(wide_int<M, S2, L> const& o) noexcept
    {
      const L fill = o.negative() ? static_cast<L>(~L{0}) : L{0};
      for (std::size_t i = 0; i < N; ++i) Word[i] = (i < M) ? o.Word[i] : fill;
    }

    [[nodiscard]] constexpr bool negative() const noexcept
    { return Signed && (Word[N - 1] >> (limb_bits - 1)) != 0; }

    [[nodiscard]] constexpr bool is_zero() const noexcept
    {
      for (std::size_t i = 0; i < N; ++i)
        if (Word[i] != 0) return false;
      return true;
    }

    constexpr explicit operator bool() const noexcept { return !is_zero(); }

    // To a builtin integer: keeps the low bits (two's complement), as the
    // builtin narrowing conversions do.
    template <std::integral T>
    constexpr explicit operator T() const noexcept
    {
#if defined(__SIZEOF_INT128__)
      using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
      using W = std::uint64_t;
#endif
      constexpr int wbits = std::numeric_limits<W>::digits;
      W acc = 0;
      for (std::size_t i = 0; i < N && static_cast<int>(i) * limb_bits < wbits; ++i)
        acc |= static_cast<W>(Word[i]) << (static_cast<int>(i) * limb_bits);
      if constexpr (bits < wbits)
        if (negative()) acc |= ~W{0} << bits;
      return static_cast<T>(acc);
    }

    // Nearest double (ties to even).
    constexpr explicit operator double() const noexcept
    {
      if (negative()) return -static_cast<double>(wide_int<N, false, L>(-*this));   // −min reads as 2^(bits−1)
      const int bw = bit_width_of(*this);
      if (bw <= 64) return static_cast<double>(static_cast<std::uint64_t>(*this));
      // Keep the top 64 bits and fold the rest into a sticky bit: 64 > 53 + 2,
      // so the one uint64 → double rounding is the correct one.
      const int sh = bw - 64;
      std::uint64_t top = static_cast<std::uint64_t>(lshr(*this, sh));
      if (!(lshr(shl(*this, bits - sh), bits - sh)).is_zero()) top |= 1u;
      return ldexp(static_cast<double>(top), sh);
    }

    // ---- arithmetic (wraps modulo 2^bits) -------------------------------
    friend constexpr wide_int operator+(wide_int a, wide_int const& b) noexcept
    {
      L c = 0;
      for (std::size_t i = 0; i < N; ++i) a.Word[i] = limb::add_carry(a.Word[i], b.Word[i], c);
      return a;
    }
    friend constexpr wide_int operator-(wide_int a, wide_int const& b) noexcept
    {
      L br = 0;
      for (std::size_t i = 0; i < N; ++i) a.Word[i] = limb::sub_borrow(a.Word[i], b.Word[i], br);
      return a;
    }
    friend constexpr wide_int operator-(wide_int const& a) noexcept { return wide_int{0} - a; }
    friend constexpr wide_int operator+(wide_int const& a) noexcept { return a; }

    friend constexpr wide_int operator*(wide_int const& a, wide_int const& b) noexcept
    {
      wide_int r{0};
      for (std::size_t i = 0; i < N; ++i)
      {
        L carry = 0;
        for (std::size_t j = 0; i + j < N; ++j)
        {
          const limb::pair<L> p = limb::mul(a.Word[i], b.Word[j]);
          L c1 = 0, c2 = 0;
          L s = limb::add_carry(r.Word[i + j], p.Lo, c1);
          s = limb::add_carry(s, carry, c2);
          r.Word[i + j] = s;
          carry = static_cast<L>(p.Hi + c1 + c2);       // fits: (b−1)² + 2(b−1) < b²
        }
      }
      return r;
    }

    struct divmod_result;
    // Truncating division, as the builtin operators. Division by zero fails
    // the build in constant evaluation and reports errc::division_by_zero at
    // runtime.
    [[nodiscard]] static constexpr divmod_result divmod(wide_int const& a, wide_int const& b) noexcept;

    friend constexpr wide_int operator/(wide_int const& a, wide_int const& b) noexcept
    { return divmod(a, b).Quotient; }
    friend constexpr wide_int operator%(wide_int const& a, wide_int const& b) noexcept
    { return divmod(a, b).Remainder; }

    // ---- bitwise ---------------------------------------------------------
    friend constexpr wide_int operator~(wide_int a) noexcept
    {
      for (std::size_t i = 0; i < N; ++i) a.Word[i] = static_cast<L>(~a.Word[i]);
      return a;
    }
    friend constexpr wide_int operator&(wide_int a, wide_int const& b) noexcept
    { for (std::size_t i = 0; i < N; ++i) a.Word[i] &= b.Word[i]; return a; }
    friend constexpr wide_int operator|(wide_int a, wide_int const& b) noexcept
    { for (std::size_t i = 0; i < N; ++i) a.Word[i] |= b.Word[i]; return a; }
    friend constexpr wide_int operator^(wide_int a, wide_int const& b) noexcept
    { for (std::size_t i = 0; i < N; ++i) a.Word[i] ^= b.Word[i]; return a; }

    // Shifts by s >= bits give 0 (<<) or the sign fill (>>); negative s is a
    // precondition violation, as for builtins.
    friend constexpr wide_int operator<<(wide_int const& a, int s) noexcept { return shl(a, s); }
    friend constexpr wide_int operator>>(wide_int const& a, int s) noexcept
    {
      const L fill = a.negative() ? static_cast<L>(~L{0}) : L{0};
      return shr(a, s, fill);
    }

    // ---- comparison --------------------------------------------------------
    friend constexpr bool operator==(wide_int const&, wide_int const&) noexcept = default;
    friend constexpr std::strong_ordering operator<=>(wide_int const& a, wide_int const& b) noexcept
    {
      if (a.negative() != b.negative())
        return a.negative() ? std::strong_ordering::less : std::strong_ordering::greater;
      for (std::size_t i = N; i-- > 0;)
        if (a.Word[i] != b.Word[i]) return a.Word[i] <=> b.Word[i];
      return std::strong_ordering::equal;
    }

    // ---- compound forms ----------------------------------------------------
    constexpr wide_int& operator+=(wide_int const& b) noexcept { return *this = *this + b; }
    constexpr wide_int& operator-=(wide_int const& b) noexcept { return *this = *this - b; }
    constexpr wide_int& operator*=(wide_int const& b) noexcept { return *this = *this * b; }
    constexpr wide_int& operator/=(wide_int const& b) noexcept { return *this = *this / b; }
    constexpr wide_int& operator%=(wide_int const& b) noexcept { return *this = *this % b; }
    constexpr wide_int& operator&=(wide_int const& b) noexcept { return *this = *this & b; }
    constexpr wide_int& operator|=(wide_int const& b) noexcept { return *this = *this | b; }
    constexpr wide_int& operator^=(wide_int const& b) noexcept { return *this = *this ^ b; }
    constexpr wide_int& operator<<=(int s) noexcept { return *this = *this << s; }
    constexpr wide_int& operator>>=(int s) noexcept { return *this = *this >> s; }
    constexpr wide_int& operator++() noexcept { return *this += wide_int{1}; }
    constexpr wide_int& operator--() noexcept { return *this -= wide_int{1}; }
    constexpr wide_int operator++(int) noexcept { wide_int t = *this; ++*this; return t; }
    constexpr wide_int operator--(int) noexcept { wide_int t = *this; --*this; return t; }

    // Number of significant bits of a non-negative value (0 for 0).
    [[nodiscard]] friend constexpr int bit_width_of(wide_int const& a) noexcept
    {
      for (std::size_t i = N; i-- > 0;)
        if (a.Word[i] != 0)
          return static_cast<int>(i) * limb_bits + (limb_bits - std::countl_zero(a.Word[i]));
      return 0;
    }

  private:
    static constexpr wide_int shl(wide_int const& a, int s) noexcept
    {
      wide_int r{0};
      if (s >= bits) return r;
      const std::size_t ws = static_cast<std::size_t>(s / limb_bits);
      const int bs = s % limb_bits;
      for (std::size_t i = ws; i < N; ++i)
      {
        L w = static_cast<L>(a.Word[i - ws] << bs);
        if (bs != 0 && i > ws) w |= static_cast<L>(a.Word[i - ws - 1] >> (limb_bits - bs));
        r.Word[i] = w;
      }
      return r;
    }
    static constexpr wide_int shr(wide_int const& a, int s, L fill) noexcept
    {
      wide_int r;
      if (s >= bits)
      {
        for (std::size_t i = 0; i < N; ++i) r.Word[i] = fill;
        return r;
      }
      const std::size_t ws = static_cast<std::size_t>(s / limb_bits);
      const int bs = s % limb_bits;
      const auto word = [&](std::size_t k) { return k < N ? a.Word[k] : fill; };
      for (std::size_t i = 0; i < N; ++i)
      {
        L w = static_cast<L>(word(i + ws) >> bs);
        if (bs != 0) w |= static_cast<L>(word(i + ws + 1) << (limb_bits - bs));
        r.Word[i] = w;
      }
      return r;
    }
    static constexpr wide_int lshr(wide_int const& a, int s) noexcept { return shr(a, s, L{0}); }
  };

  template <std::size_t N, bool Signed, std::unsigned_integral L>
  struct wide_int<N, Signed, L>::divmod_result { wide_int Quotient; wide_int Remainder; };

  template <std::size_t N, bool Signed, std::unsigned_integral L>
  constexpr auto wide_int<N, Signed, L>::divmod(wide_int const& a, wide_int const& b) noexcept
    -> divmod_result
  {
    if (b.is_zero())
    {
      if consteval { constexpr_error<"wide_int: division by zero">(); }
      raise(errc::division_by_zero, "wide_int: division by zero");
    }
    // Divide the magnitudes; −min reads correctly as an unsigned magnitude.
    const bool na = a.negative(), nb = b.negative();
    const wide_int ua = na ? -a : a, ub = nb ? -b : b;
    std::size_t m = N, n = N;
    while (m > 0 && ua.Word[m - 1] == 0) --m;
    while (ub.Word[n - 1] == 0) --n;
    divmod_result res{wide_int{0}, wide_int{0}};
    if (m < n)
      res.Remainder = ua;
    else
    {
      L un[N + 1]{}, vn[N]{};
      limb::divmod(ua.Word, m, ub.Word, n, res.Quotient.Word, res.Remainder.Word, un, vn);
    }
    if (na != nb) res.Quotient = -res.Quotient;
    if (na) res.Remainder = -res.Remainder;
    return res;
  }

  template <std::size_t N, std::unsigned_integral L = umax>
  using wide_uint = wide_int<N, false, L>;
  template <std::size_t N, std::unsigned_integral L = umax>
  using wide_sint = wide_int<N, true, L>;

  template <typename T>
  inline constexpr bool is_wide_int_v = false;
  template <std::size_t N, bool S, std::unsigned_integral L>
  inline constexpr bool is_wide_int_v<wide_int<N, S, L>> = true;
} // namespace beman::inside::detail

template <std::size_t N, bool Signed, std::unsigned_integral L>
struct std::numeric_limits<beman::inside::detail::wide_int<N, Signed, L>>
{
private:
  using W = beman::inside::detail::wide_int<N, Signed, L>;
  static constexpr W top_bit() noexcept { return W{1} << (W::bits - 1); }
public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = Signed;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = !Signed;
  static constexpr int  radix = 2;
  static constexpr int  digits = W::bits - (Signed ? 1 : 0);
  static constexpr int  digits10 = digits * 30103 / 100000;   // ⌊digits·log10 2⌋
  static constexpr W min() noexcept { return Signed ? top_bit() : W{0}; }
  static constexpr W lowest() noexcept { return min(); }
  static constexpr W max() noexcept { return Signed ? ~top_bit() : ~W{0}; }
};




namespace beman::inside::detail
{
  [[nodiscard]] constexpr umax abs_den(imax d) noexcept { return (d >= 0) ? static_cast<umax>(d) : umax{0} - static_cast<umax>(d); }

  //---------------------------------------------------------------------------
  // trim
  //---------------------------------------------------------------------------
  inline constexpr void trim(umax& numerator, imax& denominator)
  {
    umax ad = abs_den(denominator);
    if (ad <= 1)
      return;
    auto g = std::gcd(numerator, ad);
    if (g <= 1)
      return;
    numerator /= g;
    ad /= g;
    denominator = (denominator < 0) ? -ad : ad;
  }

  inline constexpr void trim(umax& a, umax& b)
  {
    if (a <= 1 || b <= 1)
      return;
    auto g = std::gcd(a, b);
    if (g <= 1)
      return;
    a /= g;
    b /= g;
  }

  [[nodiscard]] constexpr std::expected<rational, errc> operator+(rational const&, rational const&);
  [[nodiscard]] constexpr std::expected<rational, errc> operator/(rational const&, rational const&);
  [[nodiscard]] constexpr std::expected<rational, errc> operator-(rational const&, rational const&);

  [[nodiscard]] constexpr std::expected<rational, errc> operator*(rational const&, rational const&);
  [[nodiscard]] constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

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
  struct rational
  {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    constexpr rational(std::floating_point  auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1)
     :Numerator{num}, Denominator{den}
    { canonicalize(Numerator, Denominator); }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. Numerator is unsigned anyway; the cast on `den` is
    // safe because the unsigned-integral concept exclude negative inputs.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den)
     :Numerator{num}, Denominator{static_cast<imax>(den)}
    { canonicalize(Numerator, Denominator); }

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
    [[nodiscard]] constexpr bool operator==(T value) const { return operator==(rational{value}); }

    [[nodiscard]] constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr std::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        raise(errc::domain_error, "cannot convert negative rational to unsigned");
      return Numerator / abs_den(Denominator);
    }

    template <std::signed_integral T>
    explicit constexpr operator T () const
    {
      umax q = Numerator / abs_den(Denominator);
      return (Denominator < 0) ? -q : q;
    }

    template <std::floating_point T>
    explicit constexpr operator T () const
    {
      T q = static_cast<T>(Numerator) / static_cast<T>(abs_den(Denominator));
      return (Denominator < 0) ? -q : q;
    }

    // allow unary+ for generic programming
    [[nodiscard]] constexpr rational operator+() const { return *this; }

    // Compound-assign: forward to the checked binary op and unwrap via .value()
    // — overflow surfaces as std::bad_expected_access (no error channel here).
    constexpr rational& operator+=(rational const& rhs);
    constexpr rational& operator-=(rational const& rhs);
    constexpr rational& operator*=(rational const& rhs);
    constexpr rational& operator/=(rational const& rhs);

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero operand for div_unchecked / inv_unchecked).
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);
    static constexpr rational inv_unchecked(rational);

    static constexpr std::expected<rational, errc> add(rational a, rational b)
    { return a + b; }
    static constexpr std::expected<rational, errc> inv(rational);

    // Shared algorithm bodies. Checked=true returns expected<rational, errc>,
    // reporting overflow / division_by_zero (a compile error at compile time,
    // std::unexpected at runtime); Checked=false
    // silently overflows — the caller must guarantee its absence.
    template <bool Checked, bool Loud = true> static constexpr auto add_impl(rational const&, rational const&);
    template <bool Checked, bool Loud = true> static constexpr auto mul_impl(rational const&, rational const&);
    template <bool Checked, bool Loud = true> static constexpr auto div_impl(rational const&, rational const&);
    template <bool Checked, bool Loud = true> static constexpr auto inv_impl(rational const&);

  private:
    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    // Two domain errors: Denominator == 0 (undefined)
    // and Denominator == imax_min (cannot be negated without UB, which every
    // sign-flip in the file assumes is well-defined).
    static constexpr void canonicalize(umax& num, imax& den)
    {
      if (den == 0)
        raise(errc::domain_error, "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
      if (num == 0) den = 1;
      trim(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB (the ctor-body canonicalize() would catch
    // these too, but only after the mem-init already evaluated `-den`).
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den)
    {
      if (den == 0)
        raise(errc::domain_error, "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
      return (num < 0) ? -den : den;
    }
  };


  [[nodiscard]] constexpr std::expected<rational, errc> gcd(rational const&, rational const&);
  [[nodiscard]] constexpr rational abs(rational);

  [[nodiscard]] constexpr bool divides_evenly(rational const&, rational const&);

  //---------------------------------------------------------------------------
  // sign / named integer reductions — free functions over the public
  // numerator/denominator (structural type), siblings of abs/gcd. Reductions
  // are explicit, lossy alternatives to `static_cast`:
  // trunc → 0; floor → -inf; ceil → +inf; round → half-away-from-zero.
  //---------------------------------------------------------------------------
  // -1 / 0 / +1 — single source of truth for the sign convention (sign lives in
  // Denominator; canonical zero is {0, 1}).
  [[nodiscard]] constexpr int sign(rational v) noexcept
  {
    if (v.Numerator == 0) return 0;
    return (v.Denominator < 0) ? -1 : 1;
  }

  // A rational from already-canonical parts (magnitude, signed denominator):
  // no gcd, no domain checks.
  [[nodiscard]] constexpr rational make_raw(umax num, imax den) noexcept
  {
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
  constexpr std::unexpected<errc> fail(errc code)
  {
    if constexpr (Loud)
      if consteval { constexpr_error<Msg>(); }
    return std::unexpected{code};
  }

  // The numerator with the value's sign, as imax (callers ensure it fits).
  [[nodiscard]] constexpr imax signed_numerator(rational v) noexcept
  {
    const imax n = static_cast<imax>(v.Numerator);
    return (v.Denominator < 0) ? -n : n;
  }

  [[nodiscard]] constexpr imax trunc(rational v)
  {
    umax q = v.Numerator / abs_den(v.Denominator);
    return (v.Denominator < 0) ? -q : q;
  }

  [[nodiscard]] constexpr imax floor(rational v)
  {
    umax ad = abs_den(v.Denominator);
    umax q = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // negative with non-zero remainder: step one further toward -inf
    if (v.Denominator < 0 && rem != 0)
      return -q - 1;
    return (v.Denominator < 0) ? -q : q;
  }

  [[nodiscard]] constexpr imax ceil(rational v)
  {
    umax ad  = abs_den(v.Denominator);
    umax q   = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // negative value: ceiling toward +inf coincides with truncation toward zero
    if (v.Denominator < 0)
      return -q;
    // positive with non-zero remainder: step one further toward +inf
    return q + (rem != 0 ? 1 : 0);
  }

  [[nodiscard]] constexpr imax round(rational v)
  {
    umax ad = abs_den(v.Denominator);
    umax q = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // half-away-from-zero: bump magnitude when 2*rem >= ad
    if (rem * 2 >= ad) ++q;
    return (v.Denominator < 0) ? -q : q;
  }

  //---------------------------------------------------------------------------
  // abs
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr rational abs(rational v)
  { if (v.Denominator < 0) v.Denominator = -v.Denominator; return v; }

  //---------------------------------------------------------------------------
  // gcd
  //---------------------------------------------------------------------------
  // Returns errc::overflow if the combined denominator lcm = (a/gcd)·b would exceed
  // imax_max (sign-bit reservation) — traps the mul_overflow then range-checks.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr std::expected<rational, errc> gcd(rational const& lhs, rational const& rhs)
  {
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
   :Numerator{safe_abs(num)},
    Denominator{ signed_den_from(num, den) }
  { canonicalize(Numerator, Denominator); }

  constexpr rational::rational(std::floating_point auto value)
  {
    if (not is_finite(value))
      raise(errc::not_finite, "non-finite double");

    if (value == 0.0)
    {
      Numerator = 0;
      Denominator = 1;
      return;
    }

    bool neg = (value < 0.0);
    if (neg) value = -value;

    auto [num, den] = abs_fraction(value);
    Numerator = num;
    Denominator = neg ? -den : den;
    // trim not needed, because abs_fraction already trims in its special case
  }

  //---------------------------------------------------------------------------
  // to
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  constexpr std::expected<T, errc> rational::to() const
  {
    if (Denominator < 0) return std::unexpected{errc::domain_error};
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
    constexpr int parse_digit(char c, int base)
    {
      if (c >= '0' && c <= '9')
      {
        int d = c - '0';
        return d < base ? d : -1;
      }
      if (base == 16)
      {
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      }
      return -1;
    }

    // Failure kinds of the number parser. The literals turn each into a named
    // compile-time diagnostic; runtime parsing maps them onto errc.
    enum class parse_fail : unsigned char
    {
      none, empty, invalid_exponent, multiple_dots, dot_in_binary, invalid_digit,
      numerator_overflow, denominator_overflow, hex_fraction_too_long,
      p_exponent_too_large, p_exponent_denominator_overflow,
      e_exponent_numerator_overflow, e_exponent_denominator_overflow,
    };

    struct parsed_number { rational Value; parse_fail Fail; };

    // Unsigned number in [first, last): the grammar of the _ins / _r literals.
    constexpr parsed_number parse_number(const char* first, const char* last)
    {
      const std::size_t N = static_cast<std::size_t>(last - first);
      auto fail = [](parse_fail f) { return parsed_number{rational{}, f}; };

      // Detect radix prefix.
      int base = 10;
      std::size_t i = 0;
      if (N >= 2 && first[0] == '0')
      {
        if (first[1] == 'x' || first[1] == 'X') { base = 16; i = 2; }
        else if (first[1] == 'b' || first[1] == 'B') { base = 2; i = 2; }
      }

      umax num = 0;
      int frac_len = 0;
      bool in_frac = false;
      bool seen_digit = false;
      int exp = 0;
      bool exp_neg = false;
      bool has_p_exp = false;  // 2^exp (hex floats)
      bool has_e_exp = false;  // 10^exp (decimal scientific)
      bool in_exp = false;
      bool exp_seen_digit = false;
      int frac_zeros = 0;      // deferred fractional zeros (see the digit loop)

      for (; i < N; ++i)
      {
        const char c = first[i];
        if (c == '\'') continue;

        if (in_exp)
        {
          if (!exp_seen_digit && (c == '+' || c == '-'))
          {
            exp_neg = (c == '-');
            continue;
          }
          if (c >= '0' && c <= '9')
          {
            if (exp < 100000) exp = exp * 10 + (c - '0');   // past 10^5 it overflows anyway
            exp_seen_digit = true;
            continue;
          }
          return fail(parse_fail::invalid_exponent);
        }

        if (c == '.')
        {
          if (in_frac) return fail(parse_fail::multiple_dots);
          if (base == 2) return fail(parse_fail::dot_in_binary);
          in_frac = true;
          continue;
        }

        if ((c == 'p' || c == 'P') && base == 16)
        {
          has_p_exp = true;
          in_exp = true;
          continue;
        }

        if ((c == 'e' || c == 'E') && base == 10)
        {
          has_e_exp = true;
          in_exp = true;
          continue;
        }

        const int d = parse_digit(c, base);
        if (d < 0) return fail(parse_fail::invalid_digit);
        seen_digit = true;

        const umax base_u = static_cast<umax>(base);
        // A fractional zero only matters if a non-zero digit follows: defer it,
        // so trailing zeros ("1.000…0") cannot overflow num / den.
        if (in_frac && d == 0) { ++frac_zeros; continue; }
        for (; frac_zeros > 0; --frac_zeros, ++frac_len)
        {
          if (num > (~umax{0}) / base_u)
            return fail(parse_fail::numerator_overflow);
          num *= base_u;
        }
        if (num > (~umax{0} - static_cast<umax>(d)) / base_u)
          return fail(parse_fail::numerator_overflow);
        num = num * base_u + static_cast<umax>(d);
        if (in_frac) ++frac_len;
      }
      if (!seen_digit) return fail(parse_fail::empty);
      if (in_exp && !exp_seen_digit) return fail(parse_fail::invalid_exponent);

      // Build denominator from fractional part.
      // For decimal: den = 10^frac_len. For hex: den = 2^(4*frac_len).
      umax den = 1;
      if (base == 10)
      {
        for (int k = 0; k < frac_len; ++k)
        {
          if (den > (~umax{0}) / 10u) return fail(parse_fail::denominator_overflow);
          den *= 10u;
        }
      }
      else if (base == 16)
      {
        const int shift = 4 * frac_len;
        if (shift >= 64) return fail(parse_fail::hex_fraction_too_long);
        den <<= shift;
      }

      // Zero stays zero whatever the exponent: skip the scaling (which could
      // only overflow).
      if (num == 0) return parsed_number{rational{umax{0}}, parse_fail::none};

      // Apply binary exponent (hex floats, `p`).
      if (has_p_exp)
      {
        if (exp >= 63) return fail(parse_fail::p_exponent_too_large);
        if (!exp_neg)
        {
          if (num > (~umax{0}) >> exp) return fail(parse_fail::numerator_overflow);
          num <<= exp;
        }
        else
        {
          if (den > (~umax{0}) >> exp) return fail(parse_fail::p_exponent_denominator_overflow);
          den <<= exp;
        }
      }

      // Apply decimal exponent (decimal scientific, `e`).
      if (has_e_exp)
      {
        for (int k = 0; k < exp; ++k)
        {
          if (!exp_neg)
          {
            if (num > (~umax{0}) / 10u) return fail(parse_fail::e_exponent_numerator_overflow);
            num *= 10u;
          }
          else
          {
            if (den > (~umax{0}) / 10u) return fail(parse_fail::e_exponent_denominator_overflow);
            den *= 10u;
          }
        }
      }

      if (den > static_cast<umax>(std::numeric_limits<imax>::max()))
        return fail(parse_fail::denominator_overflow);
      return parsed_number{rational{num, den}, parse_fail::none};
    }

    template<char... Chars>
    consteval rational parse_ins_literal()
    {
      constexpr char src[] = { Chars..., '\0' };
      constexpr parsed_number r = parse_number(src, src + sizeof...(Chars));
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
    constexpr std::expected<rational, errc> parse_text(const char* first, const char* last)
    {
      bool neg = false;
      if (first != last && (*first == '+' || *first == '-')) { neg = (*first == '-'); ++first; }
      const char* slash = first;
      while (slash != last && *slash != '/') ++slash;

      auto one = [](const char* f, const char* l) -> std::expected<rational, errc>
      {
        const parsed_number r = parse_number(f, l);
        switch (r.Fail)
        {
          case parse_fail::none: return r.Value;
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
      if (v && slash != last)
      {
        const auto d = one(slash + 1, last);
        if (!d) return d;
        if (d->Numerator == 0) return std::unexpected{errc::division_by_zero};
        v = *v / *d;
      }
      if (v && neg) v = -*v;
      return v;
    }

  template<char... Chars>
  constexpr rational operator ""_r() { return parse_ins_literal<Chars...>(); }

  // per<D> and frac<N, D> are defined publicly in `namespace beman::inside` (see the
  // re-export block at the end of this header) so consumers spell grid values
  // without naming the internal representation type.

  //---------------------------------------------------------------------------
  // add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
  //---------------------------------------------------------------------------
  template <bool Checked, bool Loud>
  inline constexpr auto rational::add_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    // (a == −b needs no test: equal denominators, opposite signs → num == 0 below.)
    if (a.Numerator == 0) return ret_t{b};
    if (b.Numerator == 0) return ret_t{a};

    bool a_neg = a.Denominator < 0;
    bool b_neg = b.Denominator < 0;
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == b_ad)
    {
      if (a_neg == b_neg)
      {
        umax numerator;
        if constexpr (Checked)
        {
          if (add_overflow(a.Numerator, b.Numerator, &numerator))
          { return ret_t{fail<"rational +: numerator overflow (same denominator)", Loud>(errc::overflow)}; }
        }
        else
          numerator = a.Numerator + b.Numerator;

        rational r;
        r.Numerator = numerator;
        r.Denominator = a.Denominator;
        trim(r.Numerator, r.Denominator);
        return ret_t{r};
      }

      umax num = (a.Numerator > b.Numerator) ? (a.Numerator - b.Numerator)
                                              : (b.Numerator - a.Numerator);
      bool r_neg = a_neg ? (a.Numerator > b.Numerator) : (b.Numerator > a.Numerator);
      if (num == 0) return ret_t{0_r};
      rational r;
      r.Numerator = num;
      r.Denominator = r_neg ? -a_ad : a_ad;
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    // Common denominator = lcm(a_ad, b_ad) = a_ad·(b_ad/g), not a_ad·b_ad: the
    // reduced cofactors (g = gcd) overflow far less often than the raw product.
    umax g     = std::gcd(a_ad, b_ad);
    umax a_ad_r = a_ad / g;       // = a_ad / gcd; coprime with b_ad_r
    umax b_ad_r = b_ad / g;

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked)
    {
      if (mul_overflow(a_ad, b_ad_r, &denominator)    ||   // = lcm(a_ad, b_ad)
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational +: denominator overflow", Loud>(errc::overflow)}; }
      if (mul_overflow(a.Numerator, b_ad_r, &A) ||
          mul_overflow(b.Numerator, a_ad_r, &B))
      {
        // Mixed signs: |A − B| can fit umax even when a cross-product alone
        // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
        // subtraction brings it back in range — the dbl-engine store path hit
        // exactly this). Retry the difference in 128-bit before giving up.
        if (a_neg != b_neg)
        {
          const limb::pair<umax> A2 = limb::mul(a.Numerator, b_ad_r);
          const limb::pair<umax> B2 = limb::mul(b.Numerator, a_ad_r);
          const bool a_bigger = A2.Hi != B2.Hi ? A2.Hi > B2.Hi : A2.Lo > B2.Lo;
          const limb::pair<umax> big = a_bigger ? A2 : B2, small = a_bigger ? B2 : A2;
          if (big.Hi - small.Hi - (big.Lo < small.Lo ? 1u : 0u) == 0)
          {
            rational r;
            r.Numerator   = big.Lo - small.Lo;
            r.Denominator = (a_neg ? a_bigger : !a_bigger) ? -denominator
                                                           :  denominator;
            trim(r.Numerator, r.Denominator);
            return ret_t{r};
          }
        }
        return ret_t{fail<"rational +: cross-multiplication overflow", Loud>(errc::overflow)};
      }
    }
    else
    {
      denominator = a_ad * b_ad_r;
      A = a.Numerator * b_ad_r;
      B = b.Numerator * a_ad_r;
    }

    if (a_neg == b_neg)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (add_overflow(A, B, &numerator))
        { return ret_t{fail<"rational +: numerator sum overflow", Loud>(errc::overflow)}; }
      }
      else
        numerator = A + B;

      // num, den both > 0 here, so the ctor's domain/zero checks are dead;
      // assemble directly and trim.
      rational r;
      r.Numerator   = numerator;
      r.Denominator = a_neg ? -denominator
                             :  denominator;
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which the `a == -b` early-return at the top of
    // add_impl already handles for canonical inputs.
    umax numerator = (A > B) ? (A - B) : (B - A);
    bool r_neg = a_neg ? (A > B) : (B > A);
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator
                           :  denominator;
    trim(r.Numerator, r.Denominator);
    return ret_t{r};
  }

  template <bool Checked, bool Loud>
  inline constexpr auto rational::mul_impl(rational const& a_in, rational const& b_in)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0) return ret_t{0_r};

    bool r_neg = (a.Denominator < 0) != (b.Denominator < 0);
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == 1 && b_ad == 1)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (mul_overflow(a.Numerator, b.Numerator, &numerator))
        { return ret_t{fail<"rational *: numerator overflow", Loud>(errc::overflow)}; }
      }
      else
        numerator = a.Numerator * b.Numerator;

      return ret_t{make_raw(numerator, r_neg ? imax{-1} : imax{1})};
    }

    trim(a.Numerator, b_ad);
    trim(b.Numerator, a_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked)
    {
      if (mul_overflow(a.Numerator, b.Numerator, &numerator) ||
          mul_overflow(a_ad, b_ad, &denominator)             ||
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational *: numerator or denominator overflow", Loud>(errc::overflow)}; }
    }
    else
    {
      numerator = a.Numerator * b.Numerator;
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
  inline constexpr auto rational::inv_impl(rational const& a)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked)
    {
      // a.Numerator goes into the result's Denominator slot, so it must fit in
      // imax (else the umax→imax conversion wraps and a later -Denominator is UB).
      if (a.Numerator == 0)
      { return ret_t{fail<"rational inv: division by zero", Loud>(errc::division_by_zero)}; }
      if (a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational inv: numerator out of denominator range", Loud>(errc::overflow)}; }
    }

    return ret_t{make_raw(abs_den(a.Denominator), signed_numerator(a))};
  }

  // div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
  // the b.Numerator-fits-in-imax check (added there) propagates here too;
  // the unchecked path skips it (caller's contract).
  template <bool Checked, bool Loud>
  inline constexpr auto rational::div_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked)
    {
      auto inv_b = inv_impl<true, Loud>(b);
      if (!inv_b.has_value()) return ret_t{std::unexpected{inv_b.error()}};
      return mul_impl<true, Loud>(a, *inv_b);
    }
    else
      return mul_impl<false>(a, inv_impl<false>(b));
  }

  //---------------------------------------------------------------------------
  // unchecked rational arithmetic — caller guarantees: no umax overflow on the
  // products, no zero divisor/numerator, and the result Denominator fits in imax.
  // The checked variants enforce all three; unchecked skips them.
  //---------------------------------------------------------------------------
  inline constexpr rational rational::add_unchecked(rational a, rational b)
  { return add_impl<false>(a, b); }

  inline constexpr rational rational::mul_unchecked(rational a, rational b)
  { return mul_impl<false>(a, b); }

  inline constexpr rational rational::div_unchecked(rational a, rational b)
  { return div_impl<false>(a, b); }

  inline constexpr rational rational::inv_unchecked(rational a)
  { return inv_impl<false>(a); }

  inline constexpr std::expected<rational, errc> rational::inv(rational a)
  { return inv_impl<true>(a); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr rational rational::operator-() const
  {
    if (Numerator == 0)
      return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    return make_raw(Numerator, -Denominator);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
  {
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
    if (lhs_ad == 1 && rhs_ad == 1)
    {
      if (lhs_neg)
        return rhs.Numerator <=> lhs.Numerator;
      else
        return lhs.Numerator <=> rhs.Numerator;
    }

    // One side is an integer: compare via divmod instead of cross-multiply.
    // Cross-multiplying would otherwise overflow when the non-integer side
    // has a huge denominator (e.g. doubles like 19.99 stored as N/2^48).
    if (rhs_ad == 1)
    {
      umax q = lhs.Numerator / lhs_ad;
      umax r = lhs.Numerator % lhs_ad;
      auto cmp = (q == rhs.Numerator) ? (r == 0 ? std::strong_ordering::equal
                                                : std::strong_ordering::greater)
                                      : (q <=> rhs.Numerator);
      return lhs_neg ? (0 <=> cmp) : cmp;
    }
    if (lhs_ad == 1)
    {
      umax q = rhs.Numerator / rhs_ad;
      umax r = rhs.Numerator % rhs_ad;
      auto cmp = (q == lhs.Numerator) ? (r == 0 ? std::strong_ordering::equal
                                                : std::strong_ordering::less)
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
  [[nodiscard]] inline constexpr auto operator<=>(std::expected<T, errc> const& lhs, const rational& rhs)
  { return rational{lhs.value()} <=> rhs; }

  template <typename T>
  [[nodiscard]] inline constexpr auto operator<=>(rational const& lhs, std::expected<T, errc> const& rhs)
  { return lhs <=> rational{rhs.value()}; }

  template <arithmetic T>
  [[nodiscard]] inline constexpr auto operator<=>(T lhs, const rational& rhs)
  { return rational{lhs} <=> rhs; }

  template <arithmetic T>
  [[nodiscard]] inline constexpr auto operator<=>(rational const& lhs, T rhs)
  { return lhs <=> rational{rhs}; }

  //---------------------------------------------------------------------------
  // Expected-lifting operators — one generic overload per arithmetic operator
  // that engages when an operand is a std::expected, both unwrap to arithmetic,
  // and at least one to rational. Gating on `arithmetic` (not `insidable`, which
  // isn't visible this low) excludes inside operands, so inside-involving expected
  // expressions partition cleanly to arithmetic.hpp's generic instead.
  //---------------------------------------------------------------------------
  template <class L, class R>
  concept rational_lift_operands =
       (is_expected_v<L> || is_expected_v<R>)
    && arithmetic<unwrap_t<L>> && arithmetic<unwrap_t<R>>
    && (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

  //---------------------------------------------------------------------------
  // Binary operators: the checked rational ⋈ rational cores, then per operator
  // the arithmetic-operand forms (direct construction, no lift overhead), the
  // expected-operand form (propagates via lift), and the compound assignments.
  // Compound assignments unwrap with .value() — std::bad_expected_access on
  // overflow; callers needing a non-throwing path use the binary operators.
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<rational, errc> operator+(rational const& lhs, rational const& rhs)
  { return rational::add_impl<true>(lhs, rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator-(rational const& lhs, rational const& rhs)
  { return operator+(lhs, -rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator*(rational const& lhs, rational const& rhs)
  { return rational::mul_impl<true>(lhs, rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator/(rational const& lhs, rational const& rhs)
  { return rational::div_impl<true>(lhs, rhs); }

  // Quiet checked ops: like the operators, but an overflow is an error value
  // even in constant evaluation (the operators make it a compile error there).
  // For compile-time code that asks whether a result fits.
  [[nodiscard]] inline constexpr std::expected<rational, errc> try_add(rational const& a, rational const& b)
  { return rational::add_impl<true, false>(a, b); }
  [[nodiscard]] inline constexpr std::expected<rational, errc> try_sub(rational const& a, rational const& b)
  { return rational::add_impl<true, false>(a, -b); }
  [[nodiscard]] inline constexpr std::expected<rational, errc> try_mul(rational const& a, rational const& b)
  { return rational::mul_impl<true, false>(a, b); }
  [[nodiscard]] inline constexpr std::expected<rational, errc> try_div(rational const& a, rational const& b)
  { return rational::div_impl<true, false>(a, b); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator-(std::expected<rational, errc> const& v)
  { return lift([](rational r){ return -r; }, v); }

#define BEMAN_INSIDE_RATIONAL_OP(op)                                                   \
  template <arithmetic T>                                                              \
  [[nodiscard]] inline constexpr auto operator op(T lhs, rational const& rhs)          \
  { return rational{lhs} op rhs; }                                                     \
  template <arithmetic T>                                                              \
  [[nodiscard]] inline constexpr auto operator op(rational const& lhs, T rhs)          \
  { return lhs op rational{rhs}; }                                                     \
  template <class L, class R> requires rational_lift_operands<L, R>                    \
  [[nodiscard]] inline constexpr auto operator op(L const& lhs, R const& rhs)          \
  { return lift([](auto const& a, auto const& b){ return a op b; }, lhs, rhs); }       \
  inline constexpr rational& rational::operator op##=(rational const& rhs)             \
  { *this = (*this op rhs).value(); return *this; }                                    \
  template <arithmetic T>                                                              \
  inline constexpr rational& operator op##=(rational& lhs, T rhs)                      \
  { return lhs op##= rational{rhs}; }                                                  \
  inline constexpr rational& operator op##=(rational& lhs, std::expected<rational, errc> const& rhs) \
  { return lhs op##= rhs.value(); }

  BEMAN_INSIDE_RATIONAL_OP(+)
  BEMAN_INSIDE_RATIONAL_OP(-)
  BEMAN_INSIDE_RATIONAL_OP(*)
  BEMAN_INSIDE_RATIONAL_OP(/)
#undef BEMAN_INSIDE_RATIONAL_OP

  //---------------------------------------------------------------------------
  // divides_evenly
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr bool divides_evenly(rational const& dividend, rational const& divisor)
  {
    if (divisor == 0) return true;            // convention: everything divides 0 evenly
    if (dividend.Numerator == 0) return true; // 0 / anything is the integer 0

    // dividend/divisor ∈ ℤ without forming the (possibly umax-overflowing)
    // quotient numerator. In lowest terms dividend = p/q, divisor = r/s; the
    // quotient p·s/(q·r) is integral iff q | s and r | p (rationals are
    // canonicalized, so gcd(p,q)=gcd(r,s)=1). All checks are on single fields.
    const umax p = dividend.Numerator, q = abs_den(dividend.Denominator);
    const umax r = divisor.Numerator,  s = abs_den(divisor.Denominator);
    return (s % q == 0) && (p % r == 0);
  }

} // namespace beman::inside::detail

namespace beman::inside::detail
{
  // Checked builders for the public helpers below: a static_assert here fires at
  // the user's spelling. Denominators are unsigned, so a sign can only sit in
  // frac's numerator.
  template <umax D>
  consteval rational make_per()
  {
    static_assert(D >= 1, "per<D> is the positive step 1/D; a continuous grid is spelled 0");
    static_assert(D <= static_cast<umax>(std::numeric_limits<imax>::max()), "per<D>: denominator too large");
    return rational{umax{1}, D};
  }

  template <imax N, umax D>
  consteval rational make_frac()
  {
    static_assert(D >= 1, "frac<N, D>: the denominator must be at least 1");
    static_assert(D <= static_cast<umax>(std::numeric_limits<imax>::max()), "frac<N, D>: denominator too large");
    return rational{N, static_cast<imax>(D)};
  }
}

namespace beman::inside
{
  // `rational` is internal, but the grid-building helpers are public — they
  // never name the type:
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
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/detail/grid_rational.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


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
#  if defined(__cpp_impl_reflection) && __has_include(<meta>)
#    define BEMAN_INSIDE_BIG_GRIDS 1
#  else
#    define BEMAN_INSIDE_BIG_GRIDS 0
#  endif
#endif

#if BEMAN_INSIDE_BIG_GRIDS
#  define BEMAN_INSIDE_GRID_ABI big_grids_v1
#else
#  define BEMAN_INSIDE_GRID_ABI small_grids_v1
#endif

namespace beman::inside::detail
{
  // Phase 0: both modes still use the 64-bit rational; big_rational follows.
  using grid_rational = rational;
}



namespace beman::inside
{
  //---------------------------------------------------------------------------
  // interval — structural NTTP type (public members only) with inclusive Lower
  // and Upper bounds. Like `grid`, its operator+/-/*// computes result intervals
  // at compile time; division returns errc::division_by_zero when the divisor straddles zero
  // (grid::operator/ re-runs on the two zero-free halves and unions them).
  //---------------------------------------------------------------------------
  inline namespace BEMAN_INSIDE_GRID_ABI {
  struct interval
  {
    detail::rational Lower;
    detail::rational Upper;

    interval() = default;

    constexpr interval(detail::rational lower, detail::rational upper)
     :Lower{lower}, Upper{upper} { }
    constexpr interval(detail::arithmetic auto lower, detail::arithmetic auto upper)
     :Lower{lower}, Upper{upper} { }

    template <auto I>
    static constexpr bool validate()
    {
      static_assert(I.Lower <= I.Upper);
      return true;
    }

    [[nodiscard]] constexpr bool operator==(const interval& rhs) const = default;
    [[nodiscard]] constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    // A span past the 64-bit rational range (an interval reaching past int64 on
    // both sides) is tested endpoint by endpoint: equal residues mod notch.
    [[nodiscard]] constexpr bool divides_evenly(const detail::rational& notch) const
    {
      if (const auto span = detail::try_sub(Upper, Lower))
        return detail::divides_evenly(*span, notch);
      return detail::divides_evenly(Lower, notch) && detail::divides_evenly(Upper, notch);
    }

    [[nodiscard]] constexpr std::expected<detail::rational, errc> operator/(const detail::rational& notch) const
    { return (Upper - Lower) / notch; }
  };
  }

  // Containment / disjointness — free functions over the public endpoints
  // (siblings of the binary interval operators below).
  [[nodiscard]] constexpr bool includes(interval const& iv, interval const& rhs) noexcept
  { return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, detail::rational const& r) noexcept
  { return iv.Lower <= r && r <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, detail::arithmetic auto a) noexcept
  { return includes(iv, detail::rational{a}); }

  // `excludes` means *strictly disjoint* — the intervals share no value.
  // `!includes()` is weaker: it only rules out total containment, so two
  // overlapping intervals are `!includes` AND `!excludes`.
  [[nodiscard]] constexpr bool excludes(interval const& iv, interval const& rhs) noexcept
  { return rhs.Upper < iv.Lower || iv.Upper < rhs.Lower; }

  // The `includes(rhs, iv)` clause catches rhs wholly containing iv (where
  // neither rhs endpoint lands in iv, so the other checks would miss it).
  [[nodiscard]] constexpr bool overlaps(interval const& iv, interval const& rhs) noexcept
  { return includes(rhs, iv) || includes(iv, rhs.Lower) || includes(iv, rhs.Upper); }

  // The min/max hull of four endpoint combinations — the result interval of an
  // interval product or quotient (interval arithmetic's four-corner rule).
  namespace detail
  {
    [[nodiscard]] constexpr interval corner_hull(rational a, rational b, rational c, rational d) noexcept
    {
      const rational lo1 = a < b ? a : b, hi1 = a < b ? b : a;
      const rational lo2 = c < d ? c : d, hi2 = c < d ? d : c;
      return interval{lo1 < lo2 ? lo1 : lo2, hi1 < hi2 ? hi2 : hi1};
    }
  }

  [[nodiscard]] constexpr std::expected<interval, errc> operator+  (const interval&, const interval&);
  [[nodiscard]] constexpr std::expected<interval, errc> operator-  (const interval&, const interval&);
  [[nodiscard]] constexpr std::expected<interval, errc> operator*  (const interval&, const interval&);
  [[nodiscard]] constexpr std::expected<interval, errc> operator/  (const interval&, const interval&);
  [[nodiscard]] constexpr auto                          operator<=>(const interval&, const interval&) -> std::partial_ordering;

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<interval, errc> operator+(const interval& lhs, const interval& rhs)
  {
    return detail::lift(
      [](detail::rational l, detail::rational u){ return interval{l, u}; },
      lhs.Lower + rhs.Lower, lhs.Upper + rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<interval, errc> operator-(const interval& lhs, const interval& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<interval, errc> operator*(const interval& lhs, const interval& rhs)
  {
    return detail::lift(detail::corner_hull,
      lhs.Lower * rhs.Lower, lhs.Lower * rhs.Upper,
      lhs.Upper * rhs.Lower, lhs.Upper * rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<interval, errc> operator/(const interval& lhs, const interval& rhs)
  {
    if (includes(rhs, 0))
      return std::unexpected{errc::division_by_zero};

    return detail::lift(detail::corner_hull,
      lhs.Lower / rhs.Lower, lhs.Lower / rhs.Upper,
      lhs.Upper / rhs.Lower, lhs.Upper / rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering
  {
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
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// BEMAN_INSIDE_MATH_NO_FP — no hardware floating point anywhere: the f64/f32
// storage flags fall back to deduced integer storage, and the double/float math
// engines compile out (the integer/CORDIC engine carries every transcendental).
// Resolved here, in a header every other one includes, so storage selection and
// the math headers always agree. Define it to force the FP-free build; it is
// auto-enabled on freestanding targets (__STDC_HOSTED__ == 0) and by
// BEMAN_INSIDE_MATH_CORDIC. Public API and grid deduction are unchanged.
#if !defined(BEMAN_INSIDE_MATH_NO_FP)
#  if defined(BEMAN_INSIDE_MATH_CORDIC) || (defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0)
#    define BEMAN_INSIDE_MATH_NO_FP
#  endif
#endif

namespace beman::inside
{
  //---------------------------------------------------------------------------
  // policy_flag
  //---------------------------------------------------------------------------
  using policy_flag = unsigned long long;

  // Check model: compile-time checks always run. When success can't be proven
  // statically, compilation fails unless the matching ignore flag is set; else a
  // runtime check is inserted that throws (or reports via an error_code param).
  // Binary operations OR the flags of both operands.
  inline constexpr policy_flag none         {0ull};
  inline constexpr policy_flag ignore_zero  {1ull << 1};
  inline constexpr policy_flag ignore_range{1ull << 2};
  // `snap` — an off-notch value is rounded to fit the grid instead of
  // rejected; on its own truncate-toward-zero. Without it, an off-notch value is
  // a compile/runtime error and div/mod fall through to exact-rational results.
  inline constexpr policy_flag snap     {1ull << 4};
  inline constexpr policy_flag round_nearest {(1ull << 5) | snap};
  // Rounding modes each pick a unique bit and OR in `snap`. Conceptually
  // exclusive; combining two is allowed but dispatch (assignment.hpp) picks the
  // first match: nearest → floor → ceil → half_even → trunc.
  inline constexpr policy_flag round_floor     {(1ull << 6) | snap};
  inline constexpr policy_flag round_ceil      {(1ull << 7) | snap};
  inline constexpr policy_flag round_half_even {(1ull << 8) | snap};

  // runtime checking — on unless the policy carries `unsafe` (see is_checked).
  // Spelling `checked` re-enables the checks alongside `unsafe`.
  inline constexpr policy_flag checked{1ull << 34}; // runtime range/notch/overflow checks

  // unary — mutually exclusive
  inline constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic

  // Representation flags — select raw storage. Without one, storage is deduced
  // from the grid (notch-0 → rational; unit notch at/below 0 → integer value;
  // else 0-based index). Binary ops OR operand policies; storage resolves
  // widest-wins: exact > f64 > f32 > {width} > direct > indexed > deduced.
  // ({width} = the fixed-width integer flags i8..u64 declared below; they pin the
  // exact backing type rather than letting deduction pick the smallest fit.)
  //
  // `f64` — math operand, binary64-backed storage under the default engine (value
  // held as IEEE-754 double, notch nominal); an ordinary round_nearest integer
  // inside under BEMAN_INSIDE_MATH_CORDIC. Power-of-2 notch + dyadic Lower required so
  // on-grid values are exact in double (see `double_exact`).
  inline constexpr policy_flag f64{(1ull << 37) | round_nearest};

  // `f32` — binary32-backed storage (raw held as IEEE-754 float, notch nominal);
  // the single-precision sibling of `f64`, for float-only FPUs (Cortex-M4F) and
  // the `flt` engine. Power-of-2 notch + dyadic Lower required AND every on-grid
  // value must fit float's 24-bit significand (see `float_exact`). Like `f64` it
  // is an ordinary round_nearest integer inside under BEMAN_INSIDE_MATH_CORDIC.
  inline constexpr policy_flag f32{(1ull << 41) | round_nearest};


  // Fixed-width integer raw storage — pin the exact backing type instead of
  // letting deduction pick the smallest fit. A bare width flag means *value*
  // storage (raw == value, like `direct`, so Notch == 1 and the value range must
  // fit the type); OR in `indexed` for 0-based notch-index storage. `storage_pick`
  // static_asserts the type is big enough for the grid (no silent widening). One
  // width flag at a time. Unlike `f32`/`f64` these carry no `round_nearest` — they
  // are plain integer storage, like `direct`/`indexed`.
  inline constexpr policy_flag i8 {1ull << 42};
  inline constexpr policy_flag u8 {1ull << 43};
  inline constexpr policy_flag i16{1ull << 44};
  inline constexpr policy_flag u16{1ull << 45};
  inline constexpr policy_flag i32{1ull << 46};
  inline constexpr policy_flag u32{1ull << 47};
  inline constexpr policy_flag i64{1ull << 48};
  inline constexpr policy_flag u64{1ull << 49};

  // OR of every fixed-width flag — lets storage_pick test "any width pinned" and
  // count set bits (exactly one allowed) in a single mask.
  inline constexpr policy_flag raw_width_mask
    {i8 | u8 | i16 | u16 | i32 | u32 | i64 | u64};

  // `exact` — force rational raw storage on any grid. Values still obey the grid;
  // exact fractions, no notch-count limit, no double. Slowest; overflow-checked
  // rational math. Identical under both engines.
  inline constexpr policy_flag exact{1ull << 38};

  // `direct` — force raw == value (plain integer) where deduction would pick a
  // 0-based index (inside<{5,100}> stores 5..100). Wire/debugger value for interop.
  // Requires Notch == 1.
  inline constexpr policy_flag direct{1ull << 39};

  // `indexed` — force raw == 0-based notch index where deduction would pick
  // direct storage (inside<{-5,5}> stores 0..10). Dense unsigned layout. Requires
  // Notch != 0.
  inline constexpr policy_flag indexed{1ull << 40};

  // opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
  // out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
  // check). Includes `snap` so notch-incompatible assigns compile.
  namespace detail { inline constexpr policy_flag unsafe_marker{1ull << 36}; }
  inline constexpr policy_flag unsafe
    {detail::unsafe_marker | ignore_range | snap | ignore_zero};

  //---------------------------------------------------------------------------
  // Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
  // of `flag` is present in `set` — reads better than the raw `(set & flag) ==
  // flag` and is correct for composite flags (e.g. `round_nearest` carries
  // `snap`, `f64` carries `round_nearest`), where a bare `set & flag`
  // truthy test would misfire. `has_any_flag` tests for any overlap.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept
  { return (set & flag) == flag; }

  [[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept
  { return (set & flags) != none; }

  // Runtime checks run unless the policy opts out with `unsafe`; an explicit
  // `checked` wins over `unsafe`. So `inside<G, round_nearest>` and
  // `inside<G, f64>` are checked, exactly like the default `inside<G>`.
  [[nodiscard]] constexpr bool is_checked(policy_flag set) noexcept
  { return has_flag(set, checked) || !has_flag(set, detail::unsafe_marker); }

  namespace detail
  {
    // The rounding mode a flag set selects — the ONE precedence every rounding
    // path uses (integer, rational and fp storage, division, math stores).
    // An explicit directional or half-even mode beats round_nearest (which f64 /
    // f32 carry by default, so `f64 | round_floor` floors); `snap` alone, or no
    // rounding flag at all, truncates toward zero. Ties of `nearest` go half
    // away from zero.
    enum class round_mode { trunc, nearest, floor, ceil, half_even };

    [[nodiscard]] constexpr round_mode rounding_of(policy_flag f) noexcept
    {
      if (has_flag(f, round_floor))     return round_mode::floor;
      if (has_flag(f, round_ceil))      return round_mode::ceil;
      if (has_flag(f, round_half_even)) return round_mode::half_even;
      if (has_flag(f, round_nearest))   return round_mode::nearest;
      return round_mode::trunc;
    }
  }

  //---------------------------------------------------------------------------
  // no_action — zero-overhead default for overflow callbacks
  //---------------------------------------------------------------------------
  struct no_action {};

  //---------------------------------------------------------------------------
  // tagged actions — opt-in callbacks for each failure path.
  // The lambda receives the inside by mutable reference as its first argument,
  // so the handler can override the value the policy was about to store.
  //---------------------------------------------------------------------------
  template<typename F> struct on_clamp_t    { [[no_unique_address]] F Fn; };
  template<typename F> struct on_wrap_t     { [[no_unique_address]] F Fn; };
  template<typename F> struct on_error_t    { [[no_unique_address]] F Fn; };
  template<typename F> struct on_overflow_t { [[no_unique_address]] F Fn; };

  //---------------------------------------------------------------------------
  // CTAD-style factories — drop the on_overflow_t{lambda} brace-init.
  //---------------------------------------------------------------------------
  template<typename F> [[nodiscard]] constexpr auto on_clamp(F&& fn)
  { return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_wrap(F&& fn)
  { return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_error(F&& fn)
  { return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_overflow(F&& fn)
  { return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }

  namespace detail
  {
  // Action detection: the `*Pred` struct is the primary detector; the concept
  // derives from it and strips cvref so the ref form matches the value form.
  template<typename T> struct is_clamp_action    : std::false_type {};
  template<typename F> struct is_clamp_action<on_clamp_t<F>>    : std::true_type {};
  template<typename T> struct is_wrap_action     : std::false_type {};
  template<typename F> struct is_wrap_action<on_wrap_t<F>>     : std::true_type {};
  template<typename T> struct is_error_action    : std::false_type {};
  template<typename F> struct is_error_action<on_error_t<F>>    : std::true_type {};
  template<typename T> struct is_overflow_action : std::false_type {};
  template<typename F> struct is_overflow_action<on_overflow_t<F>> : std::true_type {};

  template<typename T> concept clamp_action    = is_clamp_action   <std::remove_cvref_t<T>>::value;
  template<typename T> concept wrap_action     = is_wrap_action    <std::remove_cvref_t<T>>::value;
  template<typename T> concept error_action    = is_error_action   <std::remove_cvref_t<T>>::value;
  template<typename T> concept overflow_action = is_overflow_action<std::remove_cvref_t<T>>::value;

  //---------------------------------------------------------------------------
  // implied_flags<A> — single source of truth for "this action requires these
  // policy bits". Used by inside::on_* and the action-first free-fn overloads.
  //---------------------------------------------------------------------------
  template<typename T> inline constexpr policy_flag implied_flags = none;
  template<typename F> inline constexpr policy_flag implied_flags<on_clamp_t<F>>    = clamp;
  template<typename F> inline constexpr policy_flag implied_flags<on_wrap_t<F>>     = wrap;
  template<typename F> inline constexpr policy_flag implied_flags<on_error_t<F>>    = checked;
  template<typename F> inline constexpr policy_flag implied_flags<on_overflow_t<F>> = checked;

  //---------------------------------------------------------------------------
  // Pack helpers — let policy_ref/assignment/arithmetic accept Actions... packs.
  // The `*Pred` structs are reused as template-template parameters (concepts
  // can't be passed as such in C++23).
  //---------------------------------------------------------------------------

  // True if any element of the pack matches the trait.
  template<template<typename> class Trait, typename... As>
  inline constexpr bool has_action = (Trait<std::remove_cvref_t<As>>::value || ... || false);

  // How many pack elements match.
  template<template<typename> class Trait, typename... As>
  inline constexpr unsigned count_action_matches =
    (0u + ... + (Trait<std::remove_cvref_t<As>>::value ? 1u : 0u));

  // OR of implied_flags<plain_t<A>> across the pack.
  template<typename... As>
  inline constexpr policy_flag merged_implied_flags =
    (none | ... | implied_flags<std::remove_cvref_t<As>>);

  // pick_action<Trait>(actions...) returns a reference to the first pack element
  // matching the trait, or a static `no_action` fallback if none does. Conflict
  // diagnostics elsewhere ensure at most one match.
    template<template<typename> class Trait>
    inline no_action& pick_action_fallback()
    { static no_action n; return n; }

    template<template<typename> class Trait, typename A, typename... Rest>
    constexpr auto& pick_action_impl(A& a, Rest&... rest)
    {
      if constexpr (Trait<std::remove_cvref_t<A>>::value) return a;
      else if constexpr (sizeof...(Rest) > 0) return pick_action_impl<Trait>(rest...);
      else return pick_action_fallback<Trait>();
    }

  template<template<typename> class Trait, typename... As>
  constexpr auto& pick_action(As&... as)
  {
    if constexpr (sizeof...(As) == 0) return pick_action_fallback<Trait>();
    else return pick_action_impl<Trait>(as...);
  }

  // Same, but operating on a tuple (lvalue or rvalue ref).
  template<template<typename> class Trait, typename Tuple>
  constexpr auto& pick_action_in(Tuple& t)
  {
    return std::apply(
      [](auto&... as) -> auto& { return pick_action<Trait>(as...); },
      t);
  }

  } // namespace detail
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/int_for_bits.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// int_for_bits — one rule for every integer the library stores or computes
// with: the smallest builtin integer of at least Bits value bits, else a
// wide_int of enough 64-bit limbs. Raws and intermediates both size by it, so
// a grid's bit count fully decides its integer types.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  constexpr std::size_t limbs_for_bits(int bits) noexcept
  { return static_cast<std::size_t>((bits + 63) / 64); }

  // Bits counts value bits; a signed type spends one more on the sign.
  template <int Bits, bool Signed>
  struct int_for_bits
  {
    static constexpr int total = Bits + (Signed ? 1 : 0);
    using type =
      std::conditional_t<(total <= 8),  std::conditional_t<Signed, std::int8_t,  std::uint8_t>,
      std::conditional_t<(total <= 16), std::conditional_t<Signed, std::int16_t, std::uint16_t>,
      std::conditional_t<(total <= 32), std::conditional_t<Signed, std::int32_t, std::uint32_t>,
      std::conditional_t<(total <= 64), std::conditional_t<Signed, std::int64_t, std::uint64_t>,
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
}




namespace beman::inside::detail
{
  // Wide enough for every exact grid computation on 64-bit grid numbers: a
  // product of three 64-bit magnitudes plus a sign.
  using grid_wide = wide_sint<4>;

  // A grid number's signed numerator and positive denominator, widened.
  constexpr grid_wide wide_numerator(rational const& r) noexcept
  {
    const grid_wide n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
  }
  constexpr grid_wide wide_denominator(rational const& r) noexcept
  { return grid_wide{abs_den(r.Denominator)}; }
}

namespace beman::inside
{
  //---------------------------------------------------------------------------
  // grid — structural NTTP type (public members only). Discretizes its interval
  // into notch-sized steps (interval must divide evenly by notch; Notch == 0
  // allows every rational, raw not offset). Its operator+/-/*// is the engine of
  // compile-time result-grid inference: every inside arithmetic operator computes
  // its result grid here, so the result interval contains every reachable value.
  //---------------------------------------------------------------------------
  inline namespace BEMAN_INSIDE_GRID_ABI {
  struct grid
  {
    interval Interval;
    detail::rational Notch;

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `inside` / `just<>` (via its implicit `operator rational()`), so an inside can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `inside`.
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper,
                   std::convertible_to<detail::rational> auto notch)
      :grid{interval{lower, upper}, notch} { }
    // Two limits: the notch is derived — gcd(1, Lower, Upper), the coarsest
    // step 1/k that keeps every integer and both limits on the lattice. Integer
    // limits give 1; {0.5, 10} gives 1/2; {frac<-6,5>, frac<3,5>} gives 1/5.
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper)
      :grid{interval{lower, upper}, derive_notch(lower, upper)} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower)
      :grid{interval{lower, lower}, detail::rational{0}} { }
    constexpr grid(interval val, detail::rational notch):Interval{val}, Notch{notch} { }

  private:
    // A combined denominator past imax has no rational notch: fall back to a
    // continuous grid (notch 0), which is always valid.
    static constexpr detail::rational derive_notch(auto lower, auto upper)
    {
      check_short_binary(lower);
      check_short_binary(upper);
      const detail::rational lo{lower}, hi{upper};
      return detail::gcd(detail::rational{1}, lo)
          .and_then([&](detail::rational g) { return detail::gcd(g, hi); })
          .value_or(detail::rational{0});
    }

    // A floating-point limit is taken as its exact binary value, so 0.1 would
    // derive a 2^-55 notch. Past 1/1024 the literal almost surely meant a
    // decimal: reject it at compile time and point to the exact spellings.
    template <typename T>
    static constexpr void check_short_binary([[maybe_unused]] T v)
    {
      if constexpr (std::floating_point<T>)
        if (std::is_constant_evaluated() && detail::abs_den(detail::rational{v}.Denominator) > 1024)
          detail::constexpr_error<
            // Clang prints only the first ~34 characters: lead with the fix.
            "float limit: use _r literal (0.1_r) or give a notch {{lo, hi}, per<D>}; "
            "grid{lo, hi} derives a notch from a floating-point limit only down to "
            "1/1024 (0.1 is not 1/10 in binary)">();
    }

  public:

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
      // Decoding is Lower + raw·Notch: a negative notch would count downward.
      static_assert(G.Notch >= 0, "grid: the notch must be non-negative");
      static_assert(G.Interval.divides_evenly(G.Notch));
      // Lower must sit on the notch lattice. divides_evenly avoids forming the
      // (possibly umax-overflowing) Lower/Notch quotient, so a grid finer than
      // uint64 index space is still valid (it stores as rational).
      static_assert(G.Notch == 0 || detail::divides_evenly(G.Interval.Lower, G.Notch));

      return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be an inside<G,P> template argument.
    [[nodiscard]] static constexpr std::expected<grid, errc>
    try_make(interval iv, detail::rational notch)
    {
      if (iv.Lower > iv.Upper)
        return std::unexpected{errc::domain_error};
      if (notch < 0)
        return std::unexpected{errc::domain_error};
      if (!iv.divides_evenly(notch))
        return std::unexpected{errc::rounding_error};
      if (notch != 0 && !detail::divides_evenly(iv.Lower, notch))
        return std::unexpected{errc::rounding_error};
      return grid{iv, notch};
    }

    // Exact slot count (Upper − Lower)/Notch, however large: with Upper = a/b,
    // Lower = c/d and Notch = e/f it is (a·d − c·b)·f / (b·d·e), exact on a
    // valid grid. 0 for a continuous grid.
    [[nodiscard]] constexpr detail::grid_wide slot_count() const noexcept
    {
      using detail::wide_numerator, detail::wide_denominator;
      if (Notch == 0) return detail::grid_wide{0};
      const auto& U = Interval.Upper;
      const auto& L = Interval.Lower;
      const detail::grid_wide num =
          (wide_numerator(U) * wide_denominator(L) - wide_numerator(L) * wide_denominator(U))
        * wide_denominator(Notch);
      return num / (wide_denominator(U) * wide_denominator(L) * wide_numerator(Notch));
    }

    // Bits needed to hold every slot index 0..slot_count().
    [[nodiscard]] constexpr int slot_bits() const noexcept { return bit_width_of(slot_count()); }

    // The slot count as a umax; false (out = 0) when it needs more than 64
    // bits — such a grid stores a wide_int index.
    [[nodiscard]] constexpr bool max_index_checked(umax& out) const
    {
      const detail::grid_wide c = slot_count();
      const bool fits = !(detail::grid_wide{std::numeric_limits<umax>::max()} < c);
      out = fits ? static_cast<umax>(c) : umax{0};
      return fits;
    }

    // Index-storage slot count (0 on overflow; the over-flow branch of storage_min
    // is discarded for such grids, which pick rational storage instead).
    [[nodiscard]] constexpr umax max_index() const { umax c = 0; (void)max_index_checked(c); return c; }

    // True when the slot count fits umax (index storage is possible). False ⇒ the
    // grid is still valid but stores its value as a rational, never an index.
    [[nodiscard]] constexpr bool max_index_representable() const { umax c = 0; return max_index_checked(c); }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_ins`) regardless of whole-range mapping.
    [[nodiscard]] constexpr bool representable(detail::rational v) const noexcept
    {
      if (!includes(Interval, v)) return false;
      if (Notch == 0) return true;
      auto diff = v - Interval.Lower;            // expected<rational, errc>
      if (!diff) return false;
      auto off = diff.value() / Notch;           // expected<rational, errc>
      return off.has_value() && detail::abs_den(off->Denominator) == 1;
    }

    // operator== be default for structural type
    [[nodiscard]] constexpr bool operator==(const grid& rhs) const = default;
    [[nodiscard]] constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy inside has an unsigned raw that IS the value.)
  };
  }

  namespace detail
  {
  // Snap a double onto the (dyadic) grid G by rounding mode M — the same rule
  // as integer storage (rounding_of; ties of `nearest` half away from zero). On
  // an fp grid the notch is a power of two, so v/notch is the exact signed value
  // index. A continuous grid (notch 0) has nothing to snap to. |index| >= 2^52 is
  // already integral, so the imax narrowing below is always safe. G and M are
  // template parameters so each store compiles to its own branch-free rounding.
  // AnySign: v may lie below a grid that starts at 0 or higher (the wrap path
  // rounds out-of-range values); otherwise that half of the tie test is dead.
  template <grid G, round_mode M = round_mode::nearest, bool AnySign = (G.Interval.Lower < 0)>
  [[nodiscard]] constexpr double snap_double(double v) noexcept
  {
    if constexpr (G.Notch == rational{0})
      return v;
    else
    {
      constexpr double nd = static_cast<double>(G.Notch);
      const double q = v / nd;
      if ((q < 0 ? -q : q) >= 4503599627370496.0)        // 2^52
        return v;
      const imax   t = static_cast<imax>(q);              // toward zero
      const double f = q - static_cast<double>(t);        // exact, sign of q, |f| < 1
      imax k = t;
      if constexpr (M == round_mode::nearest)
      {
        k += (f >= 0.5);
        if constexpr (AnySign) k -= (f <= -0.5);
      }
      else if constexpr (M == round_mode::floor)     k -= (f < 0);
      else if constexpr (M == round_mode::ceil)      k += (f > 0);
      else if constexpr (M == round_mode::half_even) k += (f > 0.5  || (f ==  0.5 && (t & 1)))
                                                       - (f < -0.5 || (f == -0.5 && (t & 1)));
      return static_cast<double>(k) * nd;
    }
  }
  }

  // Raw of a point grid (Lower == Upper): its value lives in the type, so the
  // raw is empty. It acts as index slot 0 — constructible from any index,
  // converting to integer 0 — so the index-storage decode (Lower + raw·Notch)
  // yields the point's value without special cases. Declared
  // [[no_unique_address]] in inside, a point member of another struct (also
  // marked [[no_unique_address]]) takes no space.
  namespace detail
  {
  struct point_slot
  {
    constexpr point_slot() = default;
    template <typename T> requires std::is_arithmetic_v<T>
    constexpr point_slot(T) noexcept {}                     // any index: the only slot
    constexpr point_slot(rational const&) noexcept {}       // any value: the type holds it
    constexpr operator imax() const noexcept { return 0; }   // reads as index 0
    constexpr bool operator==(point_slot const&) const = default;
    constexpr auto operator<=>(point_slot const&) const = default;
  };
  }

  // Both endpoints lie in imax — the signed-direct candidates (and every
  // `trunc(endpoint)` constant) are only meaningful then.
  namespace detail
  {
  constexpr bool fits_imax(interval const& iv) noexcept
  {
    return iv.Lower >= rational{std::numeric_limits<imax>::min()}
        && iv.Upper <= rational{std::numeric_limits<imax>::max()};
  }
  }

  // Smallest raw type holding every reachable index in G. Order: point →
  // empty point_slot; notch-zero → rational (no integer index space); more
  // than 2^64 slots → a wide_int index; signed-direct fits Lower < 0 with
  // notch 1; unsigned-offset (max_index slots) otherwise.
  namespace detail
  {
  // Unsigned index raw for G's slots: a builtin up to 64 bits, else wide.
  template <grid G>
  using index_raw_for_t =
    std::conditional_t<G.max_index_representable(), smallest_uint_for_t<G.max_index()>,
                       int_for_bits_t<G.slot_bits(), false>>;

  template <grid G>
  using storage_min_t =
    std::conditional_t<(G.Interval.Lower == G.Interval.Upper), point_slot,
    std::conditional_t<(G.Notch == 0), detail::rational,
    std::conditional_t<(!G.max_index_representable()), index_raw_for_t<G>,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1 && fits_imax(G.Interval)),
      smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
      smallest_uint_for_t<G.max_index()>>>>>;

  // Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
  // on-grid value is exactly representable in IEEE-754 `double`. Precondition
  // for double-backed (`f64`) storage.
  template <grid G>
  inline constexpr bool dyadic_grid =
       G.Notch.Numerator != 0
    && std::has_single_bit(detail::abs_den(G.Notch.Denominator))
    && std::has_single_bit(detail::abs_den(G.Interval.Lower.Denominator));

  // log2 of a power-of-two magnitude (>= 1); 0 for 1.
  constexpr int log2_pow2_mag(umax d) noexcept { return std::countr_zero(d); }

  // |r · 2^f| as an integer. On a dyadic grid every endpoint's denominator is a
  // power of two dividing 2^f, so r·2^f is integral. Writes |N| and returns true
  // when it fits in umax; returns false on overflow (which already means ≥ 2^53).
  constexpr bool scaled_numerator(const rational& r, int f, umax& out) noexcept
  {
    if (r.Numerator == 0) { out = 0; return true; }
    const int sh = f - log2_pow2_mag(abs_den(r.Denominator));   // 0 <= sh <= f
    if (sh >= 64) return false;
    if (r.Numerator > (~umax{0} >> sh)) return false;           // Numerator << sh overflows
    out = r.Numerator << sh;
    return true;
  }

  // `double`-exactness of a dyadic grid: the IEEE-754 double path equals the
  // exact grid arithmetic iff, at the coarsest-magnitude end, the value's ULP is
  // no coarser than the notch. Writing v = N·2^(−f) with f = log2(den(Notch)),
  // that is |N| < 2^53 (53-bit significand) AND f ≤ 1022 (notch ≥ smallest
  // normal, so no on-grid value is subnormal). The 2^1024 overflow ceiling is
  // unreachable once |N| < 2^53. Necessary precondition for `f64` storage.
  template <grid G>
  constexpr bool compute_double_exact() noexcept
  {
    if constexpr (!dyadic_grid<G>) return false;
    else
    {
      constexpr int f = log2_pow2_mag(abs_den(G.Notch.Denominator));
      if (f > 1022) return false;
      umax nlo = 0, nhi = 0;
      if (!scaled_numerator(G.Interval.Lower, f, nlo)) return false;
      if (!scaled_numerator(G.Interval.Upper, f, nhi)) return false;
      constexpr umax lim = umax{1} << 53;
      return nlo < lim && nhi < lim;
    }
  }

  template <grid G>
  inline constexpr bool double_exact = compute_double_exact<G>();

  // `float`-exactness: the binary32 analogue of double_exact. Every on-grid value
  // v = N·2^(−f) must fit float's 24-bit significand (|N| < 2^24) with f ≤ 126
  // (notch ≥ float's smallest normal, so no on-grid value is subnormal).
  // Necessary precondition for `f32` (binary32-backed) storage.
  template <grid G>
  constexpr bool compute_float_exact() noexcept
  {
    if constexpr (!dyadic_grid<G>) return false;
    else
    {
      constexpr int f = log2_pow2_mag(abs_den(G.Notch.Denominator));
      if (f > 126) return false;
      umax nlo = 0, nhi = 0;
      if (!scaled_numerator(G.Interval.Lower, f, nlo)) return false;
      if (!scaled_numerator(G.Interval.Upper, f, nhi)) return false;
      constexpr umax lim = umax{1} << 24;
      return nlo < lim && nhi < lim;
    }
  }

  template <grid G>
  inline constexpr bool float_exact = compute_float_exact<G>();

  // Fixed-width raw storage (policy_flag.hpp i8..u64) — pin the exact backing
  // type instead of letting storage_min pick the smallest fit.
  //
  // has_width_flag / width_flag_count: detect "a width is pinned" and enforce
  // exactly one (combining two width flags is a misuse, caught in storage_pick).
  constexpr bool has_width_flag(policy_flag P) noexcept
  { return (P & raw_width_mask) != none; }

  constexpr int width_flag_count(policy_flag P) noexcept
  { return std::popcount(P & raw_width_mask); }

  // Map the single set width bit to its C++ type (only valid when has_width_flag).
  template <policy_flag P>
  using raw_type_of_t =
    std::conditional_t<(P & i8 ) == i8 , std::int8_t,
    std::conditional_t<(P & u8 ) == u8 , std::uint8_t,
    std::conditional_t<(P & i16) == i16, std::int16_t,
    std::conditional_t<(P & u16) == u16, std::uint16_t,
    std::conditional_t<(P & i32) == i32, std::int32_t,
    std::conditional_t<(P & u32) == u32, std::uint32_t,
    std::conditional_t<(P & i64) == i64, std::int64_t,
                                                    std::uint64_t>>>>>>>;

  // Does raw type R hold every reachable raw value of grid G under the given
  // encoding? Index storage runs 0..max_index (unsigned); value storage runs
  // Lower..Upper. The full range of R is usable, matching smallest_uint_for /
  // smallest_int_for.
  template <grid G, typename R, bool Index>
  constexpr bool storage_fits() noexcept
  {
    using lim = std::numeric_limits<R>;
    if constexpr (Index)
      return G.max_index_representable()
          && G.max_index() <= static_cast<umax>(lim::max());
    else if constexpr (std::is_unsigned_v<R>)
      return G.Interval.Lower >= 0
          && G.Interval.Upper <= rational{static_cast<umax>(lim::max())};
    else
      return G.Interval.Lower >= rational{static_cast<imax>(lim::min())}
          && G.Interval.Upper <= rational{static_cast<imax>(lim::max())};
  }

  // Storage for an inside<G, P>: representation flags pick the raw type, widest-wins
  // (exact > f64 > f32 > {width} > direct > indexed > deduced).
  //   exact   → rational raw on any grid.
  //   f64     → double-backed under the default engine, on a dyadic or notch-0
  //             grid; elided under BEMAN_INSIDE_MATH_CORDIC (falls through to deduced).
  //   f32     → float-backed when float holds the grid, else widened to double.
  //   {width} → the pinned i8..u64 type, value or (with `indexed`) index storage.
  //   direct  → raw == value, plain integer (Notch == 1).
  //   indexed → raw == 0-based notch index (Notch != 0).
  //   none    → storage_min deduction.
  template <grid G, policy_flag P>
  constexpr auto storage_pick()
  {
    // A point's value is its type: empty raw whatever the representation flag,
    // unless a width flag pins a wire layout.
    if constexpr (G.Interval.Lower == G.Interval.Upper && !has_width_flag(P))
      return point_slot{};
    else if constexpr (has_flag(P, exact))
      return detail::rational{};
#ifndef BEMAN_INSIDE_MATH_NO_FP
    else if constexpr (has_flag(P, f64)
                    && (double_exact<G> || G.Notch == 0))
      return double{};
    else if constexpr (has_flag(P, f64) && dyadic_grid<G>)
    {
      // `f64` explicitly requested on a dyadic grid double can't represent
      // exactly (max |value·2^f| ≥ 2^53, or notch below the smallest normal).
      // Arithmetic drops the flag before reaching here, so this is direct misuse.
      static_assert(double_exact<G>,
        "f64 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return double{};   // unreachable; fixes the deduced return type
    }
    else if constexpr (has_flag(P, f32)
                    && (float_exact<G> || G.Notch == 0))
      return float{};
    else if constexpr (has_flag(P, f32) && double_exact<G>)
      // `f32` requested on a grid too fine for float but representable in double:
      // WIDEN the storage to binary64. This makes a deduced f32 output (a cmath
      // result inheriting the operand's flag) whose grid overflows float store its
      // value in double rather than hard-erroring — the value stays exact. The f32
      // POLICY bit remains (harmless; storage is raw-driven via fp_raw).
      return double{};
    else if constexpr (has_flag(P, f32) && dyadic_grid<G>)
    {
      // Too fine for double too → genuinely unrepresentable as fp storage.
      static_assert(double_exact<G>,
        "f32 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return float{};    // unreachable; fixes the deduced return type
    }
#endif
    else if constexpr (has_width_flag(P))
    {
      // User-pinned raw width (i8..u64). Encoding follows `indexed` (0-based
      // notch index) else value storage (raw == value, Notch == 1 like `direct`).
      // No silent widening — a type too small for the grid is a hard error.
      static_assert(width_flag_count(P) == 1,
        "storage: pick a single fixed-width flag (e.g. `u16`), not several");
      using R = raw_type_of_t<P>;
      constexpr bool idx = (P & indexed) == indexed;
      // A point (notch 0) has one value: value storage holds it, index storage
      // holds slot 0 — the notch requirement does not apply.
      static_assert(G.Interval.Lower == G.Interval.Upper
                    || (idx ? (G.Notch != 0) : (G.Notch == 1)),
        "fixed-width storage: value storage needs Notch == 1 — add `indexed` to "
        "store a notched grid's 0-based index instead");
      static_assert(storage_fits<G, R, idx>(),
        "fixed-width storage: the chosen raw type is too small for this grid — "
        "widen the flag, coarsen the grid/notch, or use `exact`");
      return R{};
    }
    else if constexpr ((P & direct) == direct && G.Notch == 1)
    {
      static_assert(G.Interval.Lower >= 0 || fits_imax(G.Interval),
        "direct storage: a negative grid must fit int64 — drop `direct` (index storage) or use `exact`");
      return std::conditional_t<(G.Interval.Lower < 0),
          smallest_int_for_t<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
          smallest_uint_for_t<static_cast<umax>(trunc(G.Interval.Upper))>>{};
    }
    else if constexpr ((P & indexed) == indexed && G.Notch != 0)
      return index_raw_for_t<G>{};
    else
      return storage_min_t<G>{};
  }

  template <grid G, policy_flag P>
  using storage_for_t = decltype(storage_pick<G, P>());
  }

  [[nodiscard]] constexpr std::expected<grid, errc> operator+(const grid&, const grid&);
  [[nodiscard]] constexpr std::expected<grid, errc> operator-(const grid&, const grid&);
  [[nodiscard]] constexpr std::expected<grid, errc> operator*(const grid&, const grid&);
  [[nodiscard]] constexpr std::expected<grid, errc> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator+(const grid& lhs, const grid& rhs)
  {
    // gcd returns expected — lift it so a notch-denominator overflow produces
    // errc::overflow rather than a silently wrapped result grid.
    return detail::lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, detail::gcd(lhs.Notch, rhs.Notch));
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator-(const grid& lhs, const grid& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator*(const grid& lhs, const grid& rhs)
  {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous (rational-backed).
    const bool lp = lhs.Interval.Lower == lhs.Interval.Upper;
    const bool rp = rhs.Interval.Lower == rhs.Interval.Upper;
    const detail::rational ln = (lp && !rp) ? detail::abs(lhs.Interval.Lower) : lhs.Notch;
    const detail::rational rn = (rp && !lp) ? detail::abs(rhs.Interval.Lower) : rhs.Notch;
    return detail::lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, ln * rn);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> operator/(const grid& lhs, const grid& rhs)
  {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
      return grid{*d, detail::rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
      return std::unexpected{errc::division_by_zero};

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    detail::rational step = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : detail::rational{1};
    bool has_pos = 0 < rhs.Interval.Upper;
    bool has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg)
    {
      return detail::lift(
        [](interval pos, interval neg){
          return grid{interval{neg.Lower < pos.Lower ? neg.Lower : pos.Lower,
                               neg.Upper < pos.Upper ? pos.Upper : neg.Upper}, detail::rational{0}};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return detail::lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return detail::lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
  }

  //---------------------------------------------------------------------------
  // hull
  //---------------------------------------------------------------------------
  // The smallest grid that represents every value of both operands exactly:
  // interval hull + notch gcd. A valid grid anchors Lower on a multiple of its
  // notch, so both lattices are sub-lattices of the gcd lattice — no offset
  // term is needed, and the hull is a valid grid by construction. A continuous
  // operand (Notch 0) makes the hull continuous. errc::overflow when the notch gcd's
  // combined denominator exceeds the representable rational range.
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<grid, errc> hull(const grid& lhs, const grid& rhs)
  {
    const interval iv{lhs.Interval.Lower < rhs.Interval.Lower ? lhs.Interval.Lower : rhs.Interval.Lower,
                      lhs.Interval.Upper < rhs.Interval.Upper ? rhs.Interval.Upper : lhs.Interval.Upper};
    if (lhs.Notch == 0 || rhs.Notch == 0)
      return grid{iv, detail::rational{0}};
    return detail::lift([iv](detail::rational g){ return grid{iv, g}; },
                detail::gcd(lhs.Notch, rhs.Notch));
  }
} // namespace beman::inside



//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`grid_of<B>`, `policy_of<B>`, `Lower/Upper/notch_of<B>`,
// `interval_of<B>`) plus the `insidable`/`numeric`/`inside_assignable` concepts; the
// storage-shape predicates and raw/value converters are internal (`beman::inside::detail`).
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <grid G = grid{{0, 0}, 0}, policy_flag P = checked> struct inside;

  template <class>                 inline constexpr bool is_inside_v = false;
  template <grid G, policy_flag P> inline constexpr bool is_inside_v<inside<G, P>> = true;

  template <typename B>
  concept insidable = is_inside_v<std::remove_cvref_t<B>>;

  //---------------------------------------------------------------------------
  // Public grid/policy introspection — extract an inside's template parameters.
  // These mirror std::numeric_limits: they report what the grid is, used
  // opaquely (the rational return type is never named by callers).
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <typename B> struct inside_params;
    template <grid G, policy_flag P> struct inside_params<inside<G, P>>
    {
      static constexpr grid        grid_v   = G;
      static constexpr policy_flag policy_v = P;
    };
  }

  template <insidable B>
  inline constexpr grid grid_of = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

  template <insidable B>
  inline constexpr policy_flag policy_of = detail::inside_params<std::remove_cvref_t<B>>::policy_v;

  template <typename T>
  inline constexpr interval interval_of = {0,0};

  template <insidable B>
  inline constexpr interval interval_of<B> = grid_of<B>.Interval;

  template <std::integral I>
  inline constexpr interval interval_of<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <insidable B> inline constexpr detail::rational lower_of = grid_of<B>.Interval.Lower;
  template <insidable B> inline constexpr detail::rational upper_of = grid_of<B>.Interval.Upper;
  template <insidable B> inline constexpr detail::rational notch_of = grid_of<B>.Notch;

  template <typename N>
  concept numeric = insidable<N> or detail::arithmetic<N>;

  //---------------------------------------------------------------------------
  // Internal plumbing — storage shape, raw/value conversion, dispatch.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>
    using plain_t = std::remove_cvref_t<T>;

    // Always-false but template-dependent: lets a `static_assert` inside a
    // template body fire only when that template is actually instantiated
    // (e.g. the guidance overloads that make `inside + 1` ill-formed).
    template<typename...>
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
    //   raw_from_offset<B>(o)  index → raw_t<B>     adds raw_lo for direct storage; identity for index
    //-------------------------------------------------------------------------

    // Uniform rational view of a scalar or inside (rational{v} / operator rational()).
    template <numeric N>
    [[nodiscard]] constexpr rational as_rational(N v)
    {
      if constexpr (arithmetic<N>) return rational{v};
      else                         return v;
    }

    // Canonical-zero test for a divisor. rational stores zero as {0, 1}, so
    // Numerator == 0 catches it regardless of representation; other types compare
    // against their own zero.
    template <typename T>
    [[nodiscard]] constexpr bool is_canonical_zero(T const& v)
    {
      if constexpr (std::same_as<T, rational>) return v.Numerator == 0;
      else                                      return v == T{0};
    }

    template <insidable B>
    using raw_t = typename B::raw_type;

    // How an inside's value lives in its raw storage — four disjoint encodings
    // (selected by policy flags or deduced; see grid.hpp storage_pick):
    //   rational_raw — raw IS the value, as a rational.
    //   f64_raw      — raw IS the value, as an IEEE-754 double (dyadic grids only).
    //   f32_raw      — raw IS the value, as an IEEE-754 float  (dyadic grids only).
    //   value_raw    — raw IS the value, as a plain integer.
    //   index_raw    — raw is a 0-based notch index; value = Lower + raw*Notch.
    template <insidable B>
    inline constexpr bool f64_raw = std::is_same_v<raw_t<B>, double>;

    template <insidable B>
    inline constexpr bool f32_raw = std::is_same_v<raw_t<B>, float>;

    // fp_raw — value held directly in a floating-point raw (f64 or f32). These
    // share every value-path branch: read/store/compare/arithmetic compute in
    // double, narrowing to the raw type on store (lossless on an fp-exact grid).
    template <insidable B>
    inline constexpr bool fp_raw = f64_raw<B> || f32_raw<B>;

    template <insidable B>
    inline constexpr bool rational_raw = std::is_same_v<raw_t<B>, rational>;

    // point_raw — a point grid's empty raw (point_slot): index storage at slot 0.
    template <insidable B>
    inline constexpr bool point_raw = std::is_same_v<raw_t<B>, point_slot>;

    // wide_raw — an index raw wider than any builtin integer (more than 2^64
    // slots). Its values need the exact wide paths (detail/wide_value.hpp): a
    // 64-bit rational or imax cannot hold them.
    template <insidable B>
    inline constexpr bool wide_raw = is_wide_int_v<raw_t<B>>;

    template <insidable B>
    inline constexpr bool value_raw =
         !fp_raw<B> && !rational_raw<B> && !point_raw<B> && !wide_raw<B>
      && ((policy_of<B> & direct) == direct
          // A pinned width flag without `indexed` is value storage (raw == value)
          // regardless of Lower's sign — storage_pick checked the range fits.
          || (has_width_flag(policy_of<B>)
              && (policy_of<B> & indexed) != indexed)
          || ((policy_of<B> & indexed) != indexed
              && notch_of<B> == 1
              && (lower_of<B> == 0 || std::signed_integral<raw_t<B>>)));

    template <insidable B>
    inline constexpr bool index_raw =
         !fp_raw<B> && !rational_raw<B> && !value_raw<B>;

    // Same raw type AND same encoding (value vs index): only then does one
    // inside's raw mean the same as another's on the same grid. A grid alone
    // does not fix the encoding — `indexed` / `direct` / `f64` / a width flag
    // pick it per policy.
    template <insidable L, insidable R>
    inline constexpr bool same_encoding =
         std::is_same_v<raw_t<L>, raw_t<R>> && index_raw<L> == index_raw<R>;

    // Ungated double view of any inside, for the `f64` arithmetic arms (the
    // public operator double() is gated on a rounding flag; this is always
    // available). Everything but index storage holds the value verbatim; an
    // index decodes through the grid.
    struct exact_frac;
    template <insidable B> constexpr exact_frac exact_of(B const& b);

    template <insidable B>
    [[nodiscard]] constexpr double as_double(B const& b) noexcept
    {
      if constexpr (point_raw<B>)
        return static_cast<double>(lower_of<B>);
      else if constexpr (wide_raw<B>)
        return static_cast<double>(exact_of(b));
      else if constexpr (!index_raw<B>)
        return static_cast<double>(b.raw());
      else
        return static_cast<double>((*(b.raw() * notch_of<B>) + lower_of<B>).value());
    }

    // True when R's interval cannot contain zero — so `a / b` can return a plain
    // `inside` instead of `expected<inside, errc>` (see detail/division.hpp). A point
    // grid at 0 is *not* excluded.
    template <insidable R>
    inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

    // Storage-agnostic int truncation of interval endpoints — intent-revealing
    // `static_cast<imax>(lower_of<B>)`. Used by from_value, raw_lo, the fast paths.
    template <insidable B>
    inline constexpr imax lower_imax = trunc(lower_of<B>);

    template <insidable B>
    inline constexpr imax upper_imax = trunc(upper_of<B>);

    // Slot count via grid::max_index (overflow-safe: 0 when it doesn't fit umax,
    // for grids that store as rational and never use the index).
    template <insidable B>
    inline constexpr umax max_index_v = grid_of<B>.max_index();

    // Every value — and, for index storage, every slot — fits imax. Gates the
    // integer fast paths that work in imax (raw_imax / to_value / raw_lo /
    // raw_hi); a grid reaching past int64 (e.g. {0, 2^64−1} in a uint64) takes
    // the exact rational / umax paths instead.
    template <insidable B>
    inline constexpr bool values_fit_imax =
         !wide_raw<B> && fits_imax(interval_of<B>)
      && (!index_raw<B> || max_index_v<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

    //-------------------------------------------------------------------------
    // grid_value_bounds / rational_mul_is_safe / rational_add_is_safe
    //
    // Conservative compile-time inside on the (numerator, denominator) of any
    // canonical value on a grid, and derived "can the rational op of two grid
    // values overflow imax" predicates — letting checked exact arithmetic drop
    // the expected wrapper when the grids prove no overflow is reachable.
    //
    // For a notched grid every value v = lo + k·notch over the common denominator
    // dC = |lo.den|·|hi.den|·|notch.den| is linear in k, so the max scaled
    // numerator is at an endpoint. A continuous grid (Notch == 0, non-point) has
    // unbounded denominators — nothing provable, so the helpers return false.
    //-------------------------------------------------------------------------
    constexpr bool grid_value_bounds(grid g, umax& max_num, umax& max_den) noexcept
    {
      if (g.Notch.Numerator == 0 && !(g.Interval.Lower == g.Interval.Upper))
        return false;                          // continuous: dens unbounded

      umax d_lo = abs_den(g.Interval.Lower.Denominator);
      umax d_hi = abs_den(g.Interval.Upper.Denominator);
      umax d_no = (g.Notch.Numerator == 0) ? umax{1} : abs_den(g.Notch.Denominator);

      umax d_common;
      if (mul_overflow(d_lo, d_hi, &d_common)) return false;
      if (mul_overflow(d_common, d_no, &d_common)) return false;

      umax lo_scaled, hi_scaled;
      if (mul_overflow(g.Interval.Lower.Numerator, d_common / d_lo, &lo_scaled)) return false;
      if (mul_overflow(g.Interval.Upper.Numerator, d_common / d_hi, &hi_scaled)) return false;

      max_num = lo_scaled > hi_scaled ? lo_scaled : hi_scaled;
      max_den = d_common;
      return true;
    }

    constexpr bool rational_mul_is_safe(grid g_l, grid g_r) noexcept
    {
      umax n_l, d_l, n_r, d_r;
      if (!grid_value_bounds(g_l, n_l, d_l)) return false;
      if (!grid_value_bounds(g_r, n_r, d_r)) return false;

      umax num_prod, den_prod;
      if (mul_overflow(n_l, n_r, &num_prod)) return false;
      if (mul_overflow(d_l, d_r, &den_prod)) return false;
      if (den_prod > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
      return true;
    }

    // add_impl's worst case over the conservative common denominator
    // D = d_l*d_r: scaled numerators A <= n_l*d_r and B <= n_r*d_l, sum
    // A + B. (The same-denominator and lcm-reduced paths only shrink these;
    // mixed signs subtract magnitudes.)
    constexpr bool rational_add_is_safe(grid g_l, grid g_r) noexcept
    {
      umax n_l, d_l, n_r, d_r;
      if (!grid_value_bounds(g_l, n_l, d_l)) return false;
      if (!grid_value_bounds(g_r, n_r, d_r)) return false;

      umax den, a, b, sum;
      if (mul_overflow(d_l, d_r, &den)) return false;
      if (den > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
      if (mul_overflow(n_l, d_r, &a)) return false;
      if (mul_overflow(n_r, d_l, &b)) return false;
      if (add_overflow(a, b, &sum)) return false;
      return true;
    }

    // Notch is a non-zero integer (denominator 1) — the grid is notch-aligned,
    // so values map 1:1 to integers. Gates the implicit imax/size_t conversions.
    template <grid G>
    inline constexpr bool notch_is_unit_integer =
      abs_den(G.Notch.Denominator) == 1 && G.Notch.Numerator != 0;

    // ONLY type conversion, NO value representation conversion calculation
    template <insidable B>
    [[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept
    {
      return static_cast<raw_t<B>>(value);
    }

    template <insidable B>
    [[nodiscard]] constexpr raw_t<B> raw_cast(rational value) noexcept
    {
      if constexpr (rational_raw<B>)
        return value;
      else
        return value.to<raw_t<B>>().value_or(0);
    }

    // Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
    // grids where raw is an index rather than a value — naming separates the
    // two intents that today both spell `static_cast<imax>`.
    template <insidable B>
    constexpr imax raw_imax(B b) noexcept
    {
      static_assert(!wide_raw<B>, "raw_imax: a wide raw does not fit imax — use the exact wide path");
      return static_cast<imax>(b.raw());
    }

    //-------------------------------------------------------------------------
    // Q-format integer fast path: for grids with integer Lower, unit-numerator
    // Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
    // by operator rational(), from_value, and assignment::store.
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr bool has_qformat_fast_path =
        abs_den(lower_of<B>.Denominator) == 1
        && notch_of<B>.Numerator == 1
        && !rational_raw<B>
        && values_fit_imax<B>;          // Lower·nd and the raw both in imax

    // value → raw, integer math only. Pre: has_qformat_fast_path<B>.
    template <insidable B>
    constexpr raw_t<B> q_format_encode(imax value) noexcept
    {
      constexpr imax nd = abs_den(notch_of<B>.Denominator);
      return raw_cast<B>((value - lower_imax<B>) * nd);
    }

    // raw → rational, integer math only. Pre: has_qformat_fast_path<B>.
    template <insidable B>
    constexpr rational q_format_decode(B b) noexcept
    {
      constexpr imax nd = abs_den(notch_of<B>.Denominator);
      return rational{raw_imax(b) + lower_imax<B> * nd, nd};
    }

    // Library-internal extraction helper. Always succeeds (returns `imax`
    // unconditionally) but does not check the value fits in any narrower
    // target. User code should prefer `b.to<T>()`, which carries a typed
    // overflow error.
    template <insidable B>
    [[nodiscard]] constexpr imax to_value(B b) noexcept
    {
      if constexpr (!index_raw<B>)
        return raw_imax(b);
      else if constexpr (abs_den(notch_of<B>.Denominator) == 1 && abs_den(lower_of<B>.Denominator) == 1)
        return lower_imax<B> + raw_imax(b) * static_cast<imax>(notch_of<B>.Numerator);
      else if constexpr (has_qformat_fast_path<B>)
      {
        constexpr imax nd = abs_den(notch_of<B>.Denominator);
        return (raw_imax(b) + lower_imax<B> * nd) / nd;   // q_format_decode, truncated
      }
      else // index storage, generic rational path
        return trunc(as_rational(b));
    }

    template <insidable B>
    constexpr void from_value(B& b, imax val)
    {
      if constexpr (!index_raw<B>)
        b = B::from_raw(raw_cast<B>(val));
      else if constexpr (abs_den(notch_of<B>.Denominator) == 1 && abs_den(lower_of<B>.Denominator) == 1)
        b = B::from_raw(raw_cast<B>((val - lower_imax<B>) / static_cast<imax>(notch_of<B>.Numerator)));
      else if constexpr (has_qformat_fast_path<B>)
        b = B::from_raw(q_format_encode<B>(val));
      else // index storage, generic rational path
      {
        auto offset = (rational{val} - lower_of<B>) / notch_of<B>;
        b = B::from_raw(raw_cast<B>(offset.value().Numerator));
      }
    }

    //-------------------------------------------------------------------------
    // raw_lo / raw_hi / raw_from_offset — map interval endpoints to raw space. For
    // notch-offset storage the raw is a 0-based index (raw_lo == 0); for direct
    // storage the raw IS the value (raw_lo == lower_imax<B>), so an offset needs
    // raw_lo<L> added back before storing.
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr imax raw_lo = !index_raw<B> ? lower_imax<B> : 0;

    template <insidable B>
    inline constexpr imax raw_hi = !index_raw<B> ? upper_imax<B> : static_cast<imax>(max_index_v<B>);

    // The exact raw range: 0 .. slot count for index storage, Lower .. Upper
    // for value storage (integers there). Sizes the work types below.
    template <insidable B>
    inline constexpr grid_wide raw_lo_exact = index_raw<B> ? grid_wide{0} : wide_numerator(lower_of<B>);
    template <insidable B>
    inline constexpr grid_wide raw_hi_exact = index_raw<B> ? grid_of<B>.slot_count() : wide_numerator(upper_of<B>);

    // Value bits a signed integer needs to hold every value in [lo, hi].
    constexpr int signed_value_bits(grid_wide const& lo, grid_wide const& hi) noexcept
    {
      auto mag = [](grid_wide const& v) { return bit_width_of(v.negative() ? -(v + grid_wide{1}) : v); };
      const int a = mag(lo), b = mag(hi);
      return a > b ? a : b;
    }

    // Value bits for every value in a list (their min .. max).
    constexpr int signed_value_bits_of(std::initializer_list<grid_wide> vals) noexcept
    {
      grid_wide mn = *vals.begin(), mx = *vals.begin();
      for (const grid_wide& v : vals) { if (v < mn) mn = v; if (mx < v) mx = v; }
      return signed_value_bits(mn, mx);
    }

    // Signed work type for an exact intermediate of Bits value bits: imax for
    // everything within int64 (the builtin fast paths keep their codegen),
    // a wide_int beyond.
    template <int Bits>
    using work_int_t = int_for_bits_t<(Bits < 63 ? 63 : Bits), true>;

    template <insidable L>
    constexpr raw_t<L> raw_from_offset(umax offset) noexcept
    {
      // Add in umax: the bits are the same, but a value raw of a grid
      // reaching past int64 (offset + Lower ≥ 2^63) must not overflow imax.
      if constexpr (!index_raw<L>)
        return raw_cast<L>(offset + static_cast<umax>(raw_lo<L>));
      else
        return raw_cast<L>(offset);
    }

    template <insidable L>
    constexpr raw_t<L> raw_from_offset(imax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(static_cast<umax>(offset) + static_cast<umax>(raw_lo<L>));
      else
        return raw_cast<L>(static_cast<umax>(offset));
    }

    //-------------------------------------------------------------------------
    // is_integer_interval vs is_integer_aligned — easy to confuse, both needed.
    //   is_integer_interval<B>: Lower and Upper integer (Notch may be fractional,
    //     e.g. inside<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
    //   is_integer_aligned<B>: Notch and Lower integer ⇒ is_integer_interval (not the
    //     converse). Precondition for native integer raw arithmetic (Raw == value).
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr bool is_integer_interval =
        abs_den(lower_of<B>.Denominator) == 1 && abs_den(upper_of<B>.Denominator) == 1;

    template <insidable B>
    inline constexpr bool is_integer_aligned =
        abs_den(notch_of<B>.Denominator) == 1 && abs_den(lower_of<B>.Denominator) == 1;

    // Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
    // unit numerator with integer denominator > 1, Lower is an integer at 0.
    // Value = Raw / Notch.Denominator. Used to gate the integer fast path for
    // fixed-point division, which would otherwise fall into the slow rational
    // route because Notch.Denominator > 1 disqualifies is_integer_aligned.
    template <insidable B>
    inline constexpr bool is_qformat =
           !rational_raw<B>
        && notch_of<B>.Numerator == 1
        && abs_den(notch_of<B>.Denominator) > 1
        && abs_den(lower_of<B>.Denominator) == 1
        && lower_of<B> == 0;

    // Policy test: checks both type-level and per-operation policy.
    // Composite flags (e.g. round_nearest = bit5 | snap) require all
    // their bits set — having a subset like just `snap` does NOT match.
    template <insidable B, typename P, policy_flag F>
    inline constexpr bool has_policy = has_flag(policy_of<B>, F) || plain_t<P>::test(F);

    // rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
    template <insidable L, typename P>
    inline constexpr round_mode rounding_for =
        has_policy<L, P, round_floor>     ? round_mode::floor
      : has_policy<L, P, round_ceil>      ? round_mode::ceil
      : has_policy<L, P, round_half_even> ? round_mode::half_even
      : has_policy<L, P, round_nearest>   ? round_mode::nearest
      :                                    round_mode::trunc;

    // v rounded onto L's lattice {k·Notch} by rounding_for<L, P> (value index,
    // ties half away from zero) — not limited to [Lower, Upper], so wrap can
    // round first and fold an on-lattice value after. Lower/Notch is an integer
    // on every valid grid, so the lattice points are exactly the grid's.
    template <insidable L, typename P>
    [[nodiscard]] constexpr rational round_to_lattice(rational v)
    {
      if constexpr (notch_of<L> == 0)
        return v;
      else
      {
        const rational qv = (v / notch_of<L>).value();
        constexpr round_mode m = rounding_for<L, P>;
        imax k;
        if constexpr (m == round_mode::nearest)    k = round(qv);
        else if constexpr (m == round_mode::floor) k = floor(qv);
        else if constexpr (m == round_mode::ceil)  k = ceil(qv);
        else if constexpr (m == round_mode::half_even)
        {
          const imax f = floor(qv);
          const rational frac = (qv - rational{f}).value();
          const rational half{1, 2};
          k = frac > half ? f + 1 : frac < half ? f : ((f & 1) ? f + 1 : f);
        }
        else                                       k = trunc(qv);
        return (rational{k} * notch_of<L>).value();
      }
    }

    // Round, then range-check. Lower and Upper are lattice points, so rounding
    // an in-range value keeps it in range; only an out-of-range value can change
    // outcome. When the policy may round (snap), rounds_into_range rounds such a
    // value and reports whether it lands inside the interval (`out` = the
    // rounded value). Only values within one notch of the interval can, which
    // also keeps round_to_lattice's division bounded for huge sources.
    template <insidable L, typename P>
    inline constexpr bool rounds_before_range_check =
        notch_of<L> != 0 && has_policy<L, P, snap>;

    template <insidable L, typename P>
    [[nodiscard]] constexpr bool rounds_into_range(rational v, rational& out)
    {
      if constexpr (!rounds_before_range_check<L, P>)
        return false;
      else
      {
        constexpr rational lo = (lower_of<L> - notch_of<L>).value_or(lower_of<L>);
        constexpr rational hi = (upper_of<L> + notch_of<L>).value_or(upper_of<L>);
        if (v <= lo || v >= hi)
          return false;
        out = round_to_lattice<L, P>(v);
        return includes(interval_of<L>, out);
      }
    }

    // Store-side form for the assignment paths: when v rounds inside, the raw of
    // the rounded lattice point (an exact in-range point: an index, rational or
    // double raw, no further rounding). Cold and out of line, and it returns the
    // raw in registers instead of writing through the caller's inside:
    //   - a second call site of the large store functions stops GCC inlining
    //     them into the hot path (~40 instructions per in-range store);
    //   - an escaping `lhs` address turns on the stack protector there (~3).
    template <insidable L> struct rounded_raw { raw_t<L> Raw; bool Ok; };

    template <insidable L, typename P>
    [[gnu::cold, gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v)
    {
      rational r;
      if (!rounds_into_range<L, P>(v, r))
        return {raw_t<L>{}, false};
      if constexpr (fp_raw<L>)
        return {static_cast<raw_t<L>>(static_cast<double>(r)), true};   // exact: fp-exact grid
      else if constexpr (rational_raw<L>)
        return {r, true};
      else
        return {raw_from_offset<L>(((r - lower_of<L>).value() / notch_of<L>).value().Numerator), true};
    }

    // Rounds the split offset quotient q + r/den (r < den ≤ imax_max) per L's
    // rounding policy — q/r form so no expression can overflow umax
    // (num + den/2 could, for num near umax). round_quotient's offset rule.
    template <insidable L, typename P>
    [[nodiscard]] constexpr umax round_offset(umax q, umax r, umax den) noexcept
    {
      constexpr round_mode m = rounding_for<L, P>;
      if constexpr (m == round_mode::nearest)        return (r * 2 >= den) ? q + 1 : q;
      else if constexpr (m == round_mode::floor)     return q;
      else if constexpr (m == round_mode::ceil)      return (r != 0) ? q + 1 : q;
      else if constexpr (m == round_mode::half_even)
      {
        if (r * 2 < den) return q;
        if (r * 2 > den) return q + 1;
        return (q & 1) ? q + 1 : q;
      }
      else                                                 return q;
    }

    // Round the non-negative offset quotient num/den (den >= 1) to an integer
    // notch index per L's rounding policy.
    //
    // Tie/sign rules are in VALUE space, not offset space, so assigning a value
    // rounds it the same way dividing down to it does (detail::div_rounded is the
    // reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
    // we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
    // it like div_rounded, and return the offset J - m. m is integral on every
    // dyadic/integer-aligned/Q-format grid; otherwise fall back to offset rounding.
    template <insidable L, typename P>
    [[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept
    {
      constexpr rational zl =
          (notch_of<L> == rational{0})
            ? rational{0}
            : (lower_of<L> / notch_of<L>).value_or(rational{0});
      constexpr bool vidx = (zl.Denominator == 1 || zl.Denominator == -1);
      constexpr imax m = vidx
          ? signed_numerator(zl)
          : imax{0};

      if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
      else
      {
        // Round the signed value-index NUM/di exactly like detail::div_rounded.
        // A numerator or m·di beyond imax (fp-derived sources on grids with
        // large |Lower·count|) cannot rebuild the signed index — fall back to
        // the offset rule, which differs only at exact ties on negative values.
        const imax di = static_cast<imax>(den);
        imax mdi, NUM;
        if (num > static_cast<umax>(std::numeric_limits<imax>::max())
            || mul_overflow(m, di, &mdi)
            || add_overflow(mdi, static_cast<imax>(num), &NUM)) [[unlikely]]
          return round_offset<L, P>(num / den, num % den, den);
        const imax t   = NUM / di;                 // C++ truncation toward zero
        const imax rr  = NUM % di;                 // sign of NUM, |rr| < di
        imax J;
        if (rr == 0)
          J = t;
        else
        {
          const bool neg = NUM < 0;
          const umax ar  = (rr < 0) ? ~static_cast<umax>(rr) + 1u
                                    :  static_cast<umax>(rr);
          const umax ab  = static_cast<umax>(di);  // ab - ar safe: 0 < ar < ab
          constexpr round_mode mode = rounding_for<L, P>;
          if constexpr (mode == round_mode::nearest)        // half away from zero
            J = (ar >= ab - ar) ? (neg ? t - 1 : t + 1) : t;
          else if constexpr (mode == round_mode::floor)     // toward -inf
            J = neg ? t - 1 : t;
          else if constexpr (mode == round_mode::ceil)      // toward +inf
            J = neg ? t : t + 1;
          else if constexpr (mode == round_mode::half_even) // tie -> even value
          {
            if      (ar < ab - ar) J = t;
            else if (ar > ab - ar) J = neg ? t - 1 : t + 1;
            else                   J = (t & 1) == 0 ? t : (neg ? t - 1 : t + 1);
          }
          else                                                 // snap: toward zero
            J = t;
        }
        return static_cast<umax>(J - m);           // offset index k = J - m (>= 0)
      }
    }

    // Forward decl — defined in assignment.hpp
    template <typename L, typename R> struct assignment;

    // A single-point source (Lower == Upper) carries one value, so its notch
    // question is only whether that value lies on L's lattice — admitting
    // `3_ins` into `{{0,9},3}` while rejecting `1_ins`. (Range is the interval
    // check's job, so clamp/wrap still take an out-of-range point.) The raw
    // Factor says nothing here: a point's notch is 0.
    template <typename L, typename R>
    inline constexpr bool point_on_lattice =
      notch_of<L> == 0
      || abs_den(((lower_of<R> - lower_of<L>).value() / notch_of<L>).value().Denominator) == 1;

    // The notch half of inside_assignable for an insidable R.
    template <typename L, typename R>
    inline constexpr bool notches_compatible = [] {
      if constexpr (lower_of<R> == upper_of<R>) return point_on_lattice<L, R>;
      else return abs_den(assignment<L, R>::Factor.Denominator) == 1;
    }();

    // Tail of the policy cascade: checked reports.
    // Returns true if a policy handled the failure (caller should return).
    // Cheap default — reports through the static category message (no string).
    template <insidable B, typename P>
    constexpr bool range_fail([[maybe_unused]] B& b, P&& policy)
    {
      if (policy.range_check())
      {
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
      || ((policy_of<L> | P) & (wrap | clamp)) != 0
      || not excludes(interval_of<L>, interval_of<R>);

    template <typename L, typename R, policy_flag P>
    concept assign_notch_ok =
      !insidable<R> || ((policy_of<L> | P) & snap) != 0 || notches_compatible<L, R>;
  } // namespace detail

  // Compile-time prerequisites for L = R, gating three failure modes at the call
  // site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
  // skipped for float/rational, which have no static interval); (3) integer
  // notch ratio or snap set (else R's notch doesn't divide L's; opt into
  // rounding). Named `inside_assignable` to avoid shadowing std::assignable_from.
  template <typename L, typename R, policy_flag P = checked>
  concept inside_assignable =
    numeric<R>
    && detail::assign_intervals_ok<L, R, P>
    && detail::assign_notch_ok<L, R, P>;

  // Diagnostic helper: instantiating `inside_assignable_why<L,R,P>` fires a named
  // static_assert per failed clause, so a developer can see which tripped. Backs
  // both the default-build diagnostic fallbacks in `inside` (core.hpp, gated by
  // `BEMAN_INSIDE_STRICT_SFINAE`) and the public `why_assignable` probe below.
  template <typename L, typename R, policy_flag P = checked>
  struct inside_assignable_why
  {
    // Collapse each clause to a plain bool *before* the static_assert. Asserting on
    // a concept-id makes GCC dump the whole satisfaction tree ("constraints not
    // satisfied / no operand of the disjunction…") on top of the message; a bool
    // condition prints just the message. Each clause is self-guarding (the inner
    // disjunctions gate `assignment<L,R>::Factor` on `insidable<R>`), so evaluating
    // all three unconditionally is safe even when R is not numeric.
    static constexpr bool is_numeric   = numeric<R>;
    static constexpr bool intervals_ok = detail::assign_intervals_ok<L, R, P>;
    static constexpr bool notch_ok     = detail::assign_notch_ok<L, R, P>;
    static_assert(is_numeric,
      "inside_assignable: rhs is not numeric (must be an inside or arithmetic type)");
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
  inline constexpr bool why_assignable =
    inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/wide_value.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// wide_value — exact values of insides whose raw is a wide index (more than
// 2^64 slots), where the 64-bit `rational` cannot hold every value.
//
// On a valid grid Lower/Notch is an integer m (slot_base), so a value is its
// value index J = m + raw times Notch, exactly. exact_frac carries a value as
// an unreduced fraction of wide integers; comparisons cross-multiply, and a
// store divides by the target notch and rounds by the policy's mode.
//
// exact_int is sized for 64-bit grid numbers: a value numerator J·Notch.num
// stays under ~200 bits, so products and cross terms fit 512 bits.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  using exact_int = wide_sint<8>;

  struct exact_frac
  {
    exact_int Num;
    exact_int Den;                                      // > 0; not reduced

    friend constexpr std::strong_ordering operator<=>(exact_frac const& a, exact_frac const& b) noexcept
    { return a.Num * b.Den <=> b.Num * a.Den; }
    friend constexpr bool operator==(exact_frac const& a, exact_frac const& b) noexcept
    { return a.Num * b.Den == b.Num * a.Den; }

    friend constexpr exact_frac operator+(exact_frac const& a, exact_frac const& b) noexcept
    { return {a.Num * b.Den + b.Num * a.Den, a.Den * b.Den}; }
    friend constexpr exact_frac operator*(exact_frac const& a, exact_frac const& b) noexcept
    { return {a.Num * b.Num, a.Den * b.Den}; }
    // Pre: b != 0.
    friend constexpr exact_frac operator/(exact_frac const& a, exact_frac const& b) noexcept
    {
      const exact_frac q{a.Num * b.Den, a.Den * b.Num};
      return q.Den.negative() ? exact_frac{-q.Num, -q.Den} : q;
    }

    constexpr explicit operator double() const noexcept
    { return static_cast<double>(Num) / static_cast<double>(Den); }
  };

  constexpr exact_frac exact_of(rational const& r) noexcept
  { return {exact_int{wide_numerator(r)}, exact_int{wide_denominator(r)}}; }

  template <std::integral T>
  constexpr exact_frac exact_of(T v) noexcept { return {exact_int{v}, exact_int{1}}; }

  // m = Lower/Notch, the value index of slot 0 (0 for a continuous grid).
  template <insidable B>
  inline constexpr grid_wide slot_base = []{
    if constexpr (notch_of<B> == 0)
      return grid_wide{0};
    else
      return wide_numerator(lower_of<B>) * wide_denominator(notch_of<B>)
           / (wide_denominator(lower_of<B>) * wide_numerator(notch_of<B>));
  }();

  template <insidable B>
  constexpr exact_frac exact_of(B const& b)
  {
    if constexpr (wide_raw<B>)
    {
      const exact_int j = exact_int{slot_base<B>} + exact_int{b.raw()};
      return {j * exact_int{wide_numerator(notch_of<B>)}, exact_int{wide_denominator(notch_of<B>)}};
    }
    else
      return exact_of(as_rational(b));
  }

  // Truncation toward zero, as an integer.
  constexpr exact_int trunc(exact_frac const& f) noexcept { return f.Num / f.Den; }

  // The reduced value as a 64-bit rational, or overflow when it does not fit.
  constexpr std::expected<rational, errc> try_rational(exact_frac const& f) noexcept
  {
    const bool neg = f.Num.negative();
    exact_int a = neg ? -f.Num : f.Num, b = f.Den;
    exact_int x = a, y = b;
    while (!y.is_zero()) { const exact_int t = x % y; x = y; y = t; }
    if (!x.is_zero()) { a /= x; b /= x; }
    if (a > exact_int{std::numeric_limits<umax>::max()} || b > exact_int{std::numeric_limits<imax>::max()})
      return std::unexpected{errc::overflow};
    const imax den = static_cast<imax>(b);
    return rational{static_cast<umax>(a), neg ? -den : den};
  }

  // Text → exact value, for wide grids whose values outgrow the 64-bit
  // rational parse (from_chars falls back here on errc::overflow). Accepts
  // [+-]digits[.digits][e[+-]digits] and two of those joined by '/'.
  // Anything the 512-bit exact_int cannot hold reports overflow.
  constexpr std::expected<exact_frac, errc> parse_exact(const char* first, const char* last) noexcept
  {
    constexpr exact_int ten{10};
    constexpr exact_int limit = std::numeric_limits<exact_int>::max() / exact_int{100};
    auto one = [&](const char* f, const char* l) -> std::expected<exact_frac, errc> {
      bool neg = false;
      if (f != l && (*f == '+' || *f == '-')) { neg = (*f == '-'); ++f; }
      exact_int num{0}, den{1};
      bool digits = false, point = false;
      for (; f != l && ((*f >= '0' && *f <= '9') || (*f == '.' && !point)); ++f)
      {
        if (*f == '.') { point = true; continue; }
        if (num > limit || den > limit) return std::unexpected{errc::overflow};
        num = num * ten + exact_int{*f - '0'};
        if (point) den = den * ten;
        digits = true;
      }
      if (!digits) return std::unexpected{errc::invalid_format};
      if (f != l && (*f == 'e' || *f == 'E'))
      {
        ++f;
        bool eneg = false;
        if (f != l && (*f == '+' || *f == '-')) { eneg = (*f == '-'); ++f; }
        if (f == l) return std::unexpected{errc::invalid_format};
        int e = 0;
        for (; f != l && *f >= '0' && *f <= '9'; ++f)
          if ((e = e * 10 + (*f - '0')) > 150) return std::unexpected{errc::overflow};
        for (; e > 0; --e)
        {
          exact_int& t = eneg ? den : num;
          if (t > limit) return std::unexpected{errc::overflow};
          t = t * ten;
        }
      }
      if (f != l) return std::unexpected{errc::invalid_format};
      return exact_frac{neg ? -num : num, den};
    };
    const char* slash = first;
    while (slash != last && *slash != '/') ++slash;
    auto v = one(first, slash);
    if (v && slash != last)
    {
      const auto d = one(slash + 1, last);
      if (!d) return d;
      if (d->Num.is_zero()) return std::unexpected{errc::division_by_zero};
      v = *v / *d;
    }
    return v;
  }

  // n / d (d != 0) rounded to an integer by M; the sign rules of div_rounded.
  template <round_mode M>
  constexpr exact_int rounded_div(exact_int const& n, exact_int const& d) noexcept
  {
    auto [q, r] = exact_int::divmod(n, d);                                 // toward zero
    if (r.is_zero()) return q;
    const bool neg = n.negative() != d.negative();
    const exact_int ar = r.negative() ? -r : r, ad = d.negative() ? -d : d;
    const exact_int away = neg ? q - exact_int{1} : q + exact_int{1};
    const exact_int r2 = ar * exact_int{2};
    if constexpr (M == round_mode::floor)          { if (neg) q = away; }
    else if constexpr (M == round_mode::ceil)      { if (!neg) q = away; }
    else if constexpr (M == round_mode::nearest)   { if (r2 >= ad) q = away; }
    else if constexpr (M == round_mode::half_even)
    { if (r2 > ad || (r2 == ad && (q.Word[0] & 1u) != 0)) q = away; }
    return q;
  }

  // The value index of f on L's lattice (f / Notch) rounded by M, minus the
  // slot base: L's slot offset. Exact is false when f lies between notches.
  struct exact_index_result { exact_int Index; bool Exact; };

  template <insidable L, round_mode M>
  constexpr exact_index_result exact_index(exact_frac const& f) noexcept
  {
    const exact_int n = f.Num * exact_int{wide_denominator(notch_of<L>)};
    const exact_int d = f.Den * exact_int{wide_numerator(notch_of<L>)};   // > 0
    const bool exact = (n % d).is_zero();
    return {rounded_div<M>(n, d) - exact_int{slot_base<L>}, exact};
  }

  // The raw of slot offset `index` (0 .. slot count) in L's encoding.
  template <insidable L>
  constexpr raw_t<L> raw_of_index(exact_int const& index) noexcept
  {
    if constexpr (point_raw<L>)
      return raw_t<L>{};
    else if constexpr (index_raw<L>)
      return static_cast<raw_t<L>>(index);
    else                                                   // value raw: raw == J
      return static_cast<raw_t<L>>(index + exact_int{slot_base<L>});
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
  template <insidable X, rational Unit>
  inline constexpr grid_wide units_lo = exact_quotient(lower_of<X>, Unit);
  template <insidable X, rational Unit>
  inline constexpr grid_wide units_hi = exact_quotient(upper_of<X>, Unit);

  // The work type of an integer + or ×: signed imax when every value index
  // involved (each operand in its unit, the result in its notch, and the
  // result's slot offsets) provably fits — then nothing wraps, and the
  // compiler keeps the value ranges, as the builtin paths always did — else
  // the wrapping type.
  template <insidable Result, insidable L, rational UL, insidable R, rational UR>
  using index_work_t = std::conditional_t<
      signed_value_bits_of({units_lo<L, UL>, units_hi<L, UL>, units_lo<R, UR>, units_hi<R, UR>,
                            units_lo<Result, notch_of<Result>>, units_hi<Result, notch_of<Result>>,
                            grid_of<Result>.slot_count()}) <= 63,
      imax, wrap_work_t<Result>>;

  // Integer raws: neither fp nor rational (a point's empty raw counts).
  template <insidable B>
  inline constexpr bool integer_raw = !fp_raw<B> && !rational_raw<B>;

  // a / b for grid numbers, known at compile time to be an integer.
  constexpr grid_wide exact_quotient(rational const& a, rational const& b) noexcept
  { return wide_numerator(a) * wide_denominator(b) / (wide_denominator(a) * wide_numerator(b)); }

  template <typename W, insidable X>
  constexpr W value_index(X const& x) noexcept
  {
    if constexpr (index_raw<X>)
      return static_cast<W>(slot_base<X>) + static_cast<W>(x.raw());
    else
      return static_cast<W>(x.raw());
  }

  // x's value in units of the notch `unit` (an integer: the unit divides
  // x's notch, or x's value for a point).
  template <typename W, rational Unit, insidable X>
  constexpr W value_in_units(X const& x) noexcept
  {
    // A point (Lower == Upper) holds its value in the type — even under a
    // width flag, whose raw stores it again.
    if constexpr (lower_of<X> == upper_of<X>)
      return static_cast<W>(exact_quotient(lower_of<X>, Unit));
    else
    {
      constexpr grid_wide scale = exact_quotient(notch_of<X>, Unit);
      if constexpr (scale == grid_wide{1}) return value_index<W>(x);
      else                                 return value_index<W>(x) * static_cast<W>(scale);
    }
  }

  // The Result whose value index is j (taken modulo 2^bits).
  template <insidable Result, typename W>
  constexpr Result from_value_index(W const& j) noexcept
  {
    if constexpr (point_raw<Result>)
      return Result::from_raw(raw_t<Result>{});
    else if constexpr (index_raw<Result>)
      return Result::from_raw(static_cast<raw_t<Result>>(j - static_cast<W>(slot_base<Result>)));
    else
      return Result::from_raw(static_cast<raw_t<Result>>(j));
  }

  // Exact result of grid arithmetic: the value is on the result lattice and
  // inside its interval by construction, so it maps straight to a raw.
  template <insidable Result>
  constexpr Result exact_result(exact_frac const& v) noexcept
  { return Result::from_raw(raw_of_index<Result>(exact_index<Result, round_mode::trunc>(v).Index)); }
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/policy.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/detail/assignment.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


namespace beman::inside::detail
{
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
         clamp_action   <plain_t<A>>
      || wrap_action    <plain_t<A>>
      || error_action   <plain_t<A>>
      || has_policy<L, P, clamp>
      || has_policy<L, P, wrap>
      || ((plain_t<P>::test(checked)
           || is_checked(policy_of<L> | (plain_t<P>::test(detail::unsafe_marker) ? detail::unsafe_marker : none)))
          && !has_policy<L, P, ignore_range>);

  // Shared out-of-range policy cascade. Order: clamp/wrap/error *actions*, then
  // clamp/wrap *policy* bits, then `range_fail`. The three caller-supplied
  // callables cover how clamp/wrap store and the error-message rhs view. `Wrappable` is false on the fractional
  // path (no wrap *action* branch). Returns true when a handler resolved the write.
  template <bool Wrappable, insidable L, typename P, typename A,
            typename DoClamp, typename DoWrap, typename MsgView>
  constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action,
                                       DoClamp do_clamp, DoWrap do_wrap,
                                       [[maybe_unused]] MsgView msg_view)
  {
    using PA = plain_t<A>;
    if constexpr (clamp_action<PA>)
    { do_clamp(); return true; }
    else if constexpr (Wrappable && wrap_action<PA>)
    { do_wrap(); return true; }
    else if constexpr (error_action<PA>)
    {
      action.Fn(lhs, errc::overflow, errc_message(errc::overflow));
      return true;
    }
    else if constexpr (has_policy<L, P, clamp>)
    { do_clamp(); return true; }
    else if constexpr (has_policy<L, P, wrap>)
    { do_wrap(); return true; }
    else
      return range_fail(lhs, policy);
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
  constexpr grid wrap_carry_grid()
  {
    constexpr imax kMin = std::numeric_limits<imax>::min();
    constexpr imax kMax = std::numeric_limits<imax>::max();
    // A range past the rational range (a grid spanning 2^64 or more) has
    // carries in {−1, 0, 1} at most: the full imax grid holds them.
    constexpr auto span_r = try_sub(upper_of<L>, lower_of<L>);
    if constexpr (!span_r || !try_add(*span_r, notch_of<L>))
      return grid{kMin, kMax};
    else if constexpr (std::integral<R> || insidable<R>)
    {
      // An integral source spans its type's limits, an inside its interval.
      constexpr rational src_lo = [] {
        if constexpr (insidable<R>) return lower_of<R>;
        else                        return rational{std::numeric_limits<R>::min()};
      }();
      constexpr rational src_hi = [] {
        if constexpr (insidable<R>) return upper_of<R>;
        else                        return rational{std::numeric_limits<R>::max()};
      }();
      const rational range = *try_add(*span_r, notch_of<L>);
      auto carry_of = [&](rational v, imax fallback) -> imax
      {
        const auto off = try_sub(v, lower_of<L>);
        if (!off) return fallback;
        const auto q = try_div(*off, range);
        if (!q || *q < rational{kMin} || *q > rational{kMax}) return fallback;
        return floor(*q);
      };
      return grid{carry_of(src_lo, kMin), carry_of(src_hi, kMax)};
    }
    else
      return grid{kMin, kMax};
  }

  template <insidable L, typename R>
  constexpr auto make_wrap_carry(imax q)
  {
    beman::inside::inside<wrap_carry_grid<L, R>()> carry;
    from_value(carry, q);                // in the carry grid by construction
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
  struct unit_fold
  {
    static constexpr grid_wide lo = wide_numerator(lower_of<L>);
    static constexpr grid_wide hi = wide_numerator(upper_of<L>);
    using W = work_int_t<signed_value_bits_of(
        {SrcLo, SrcHi, lo, hi, SrcLo - hi, SrcHi - lo, hi - lo + grid_wide{1}})>;

    static constexpr W lower = static_cast<W>(lo);
    static constexpr W upper = static_cast<W>(hi);
    static constexpr W span  = static_cast<W>(hi - lo);

    // d saturated to imax (a clamp overshoot or a wrap carry).
    static constexpr imax saturate(W const& d) noexcept
    {
      constexpr W kMin = static_cast<W>(std::numeric_limits<imax>::min());
      constexpr W kMax = static_cast<W>(std::numeric_limits<imax>::max());
      return static_cast<imax>(d < kMin ? kMin : kMax < d ? kMax : d);
    }

    // The raw of Lower + offset (0 ≤ offset ≤ span) in L's encoding.
    static constexpr raw_t<L> raw_at(W const& offset) noexcept
    {
      if constexpr (point_raw<L>)
        return raw_t<L>{};                              // a point has one slot
      else if constexpr (index_raw<L>)
        return static_cast<raw_t<L>>(offset);
      else if constexpr (rational_raw<L> || fp_raw<L>)
      {
        const W v = lower + offset;                     // |v| < 2^64: a grid value
        const rational r = v < W{0} ? -rational{static_cast<umax>(-v)} : rational{static_cast<umax>(v)};
        if constexpr (rational_raw<L>) return r;
        else                           return static_cast<raw_t<L>>(static_cast<double>(r));
      }
      else
        return static_cast<raw_t<L>>(lower + offset);
    }

    // v = Lower + carry·(span + 1) + offset with 0 ≤ offset ≤ span.
    struct folded { imax Carry; W Offset; };
    static constexpr folded fold(W const& v) noexcept
    {
      constexpr W range = span + W{1};
      const W t = v - lower;
      W q = t / range, w = t % range;
      if (w < W{0}) { w += range; q -= W{1}; }
      return {saturate(q), w};
    }
  };

  // The integer range of a source: its type's limits, or ±2^64 (every
  // integer a 64-bit rational or a grid value can be).
  template <typename R>
  inline constexpr grid_wide source_lo = [] {
    if constexpr (std::integral<R>) return grid_wide{std::numeric_limits<R>::min()};
    else                            return -(grid_wide{1} << 64);
  }();
  template <typename R>
  inline constexpr grid_wide source_hi = [] {
    if constexpr (std::integral<R>) return grid_wide{std::numeric_limits<R>::max()};
    else                            return grid_wide{1} << 64;
  }();

  // An integer-valued rational (|v| < 2^64) as a grid_wide.
  constexpr grid_wide integer_wide(rational const& v) noexcept { return wide_numerator(v); }

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
  template <typename R, insidable L, typename P, typename A>
  constexpr L& assign_exact(L& lhs, exact_frac const& v, P&& policy, A&& action)
  {
    auto fail = [&](errc code) {
      if constexpr (error_action<plain_t<A>>) action.Fn(lhs, code, errc_message(code));
      else                                    policy.report(code);
    };
    if constexpr (rational_raw<L> || fp_raw<L>)
    {
      // L holds 64-bit values: narrow through the rational (a value that does
      // not fit lies outside every such grid).
      const auto r = try_rational(v);
      if (!r) [[unlikely]] { fail(errc::overflow); return lhs; }
      if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
        static_assert(dependent_false<A>,
          "on_clamp / on_wrap: a source past the 64-bit rational into a rational or fp inside is not supported");
      return assignment<L, rational>::assign(lhs, *r, policy, std::forward<A>(action));
    }
    else
    {
      // (Plain variables, not a structured binding: Clang rejects a binding
      // captured by the lambdas below in constant evaluation.)
      const exact_index_result slot = exact_index<L, rounding_for<L, plain_t<P>>>(v);
      const exact_int index = slot.Index;
      if (!slot.Exact && !has_policy<L, P, snap> && policy.round_check()) [[unlikely]]
      { fail(errc::rounding_error); return lhs; }
      const exact_int count{grid_of<L>.slot_count()};
      if (index.negative() || index > count) [[unlikely]]
      {
        auto saturate = [](exact_int const& d) -> imax {
          constexpr imax kMin = std::numeric_limits<imax>::min(), kMax = std::numeric_limits<imax>::max();
          return d < exact_int{kMin} ? kMin : exact_int{kMax} < d ? kMax : static_cast<imax>(d);
        };
        if (dispatch_out_of_range<true>(lhs, policy, action,
              [&]{
                const bool low = index.negative();
                lhs = L::from_raw(raw_of_index<L>(low ? exact_int{0} : count));
                if constexpr (clamp_action<plain_t<A>>)
                {
                  // The overshoot rhs − bound, shaped like the builtin paths'.
                  const exact_frac over = v + exact_frac{exact_int{-1}, exact_int{1}}
                                        * exact_of(low ? lower_of<L> : upper_of<L>);
                  if constexpr (insidable<R>)
                    action.Fn(lhs, exact_result<beman::inside::inside<(grid_of<R> - grid_of<L>).value()>>(over));
                  else if constexpr (std::integral<R>)
                    action.Fn(lhs, saturate(trunc(over)));
                  else if constexpr (std::floating_point<R>)
                    action.Fn(lhs, static_cast<R>(static_cast<double>(over)));
                  else
                    action.Fn(lhs, try_rational(over).value_or(rational{0}));
                }
              },
              [&]{
                const exact_int range = count + exact_int{1};
                auto [q, w] = exact_int::divmod(index, range);
                if (w.negative()) { w += range; q -= exact_int{1}; }
                lhs = L::from_raw(raw_of_index<L>(w));
                if constexpr (wrap_action<plain_t<A>>)
                  action.Fn(lhs, make_wrap_carry<L, R>(saturate(q)));
              },
              [&]{ return 0; }))
          return lhs;
      }
      lhs = L::from_raw(raw_of_index<L>(index));
      return lhs;
    }
  }

  //---------------------------------------------------------------------------
  // assign(insidable, integral)
  //---------------------------------------------------------------------------
  template <insidable L, std::integral R>
  struct assignment<L,R>
  {
    private:
      // A 64-bit unsigned source can exceed imax: `static_cast<imax>(rhs)` would
      // turn 2^64−1 into −1. Compare such a source in umax instead; every
      // narrower or signed source is exact in imax and keeps the plain cast.
      static constexpr bool wide_unsigned =
          std::is_unsigned_v<R> && sizeof(R) >= sizeof(imax);

      // Exact integer arithmetic for the bounds and the cold clamp/wrap paths:
      // an integer interval may reach past int64 on either side (e.g.
      // {0, 2^64−1}), and rhs − Lower may need 65 bits.
      using fold = unit_fold<L, source_lo<R>, source_hi<R>>;
      using W = typename fold::W;

      // Hot-path range test. Bounds within imax compare as plain integers;
      // bounds past int64 compare exactly in the fold's work type.
      static constexpr bool out_of_range(R rhs) noexcept
      {
        if constexpr (fits_imax(interval_of<L>))
        {
          constexpr imax lower = lower_imax<L>, upper = upper_imax<L>;
          if constexpr (wide_unsigned)
            return (lower > 0 && static_cast<umax>(rhs) < static_cast<umax>(lower))
                || upper < 0 || static_cast<umax>(rhs) > static_cast<umax>(upper);
          else
            return static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper;
        }
        else
        {
          const W v = static_cast<W>(rhs);
          return v < fold::lower || fold::upper < v;
        }
      }

      template<typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, A&& action)
      {
        // Pre: rhs is out of [Lower, Upper] (only called from handle_out_of_range),
        // so the two-way pick is the full clamp.
        const W v = static_cast<W>(rhs);
        const bool low = v < fold::lower;
        lhs = L::from_raw(fold::raw_at(low ? W{0} : fold::span));
        if constexpr (clamp_action<plain_t<A>>)
          action.Fn(lhs, fold::saturate(v - (low ? fold::lower : fold::upper)));
      }

      template<typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, A&& action)
      {
        // Modular wrap on the exact offset rhs − Lower into span + 1 slots. The
        // carry saturates at imax, like the carry grid (wrap_carry_grid).
        const auto [carry, w] = fold::fold(static_cast<W>(rhs));
        lhs = L::from_raw(fold::raw_at(w));
        if constexpr (wrap_action<plain_t<A>>)
          action.Fn(lhs, make_wrap_carry<L, R>(carry));
      }

      template<typename P, typename A>
      static constexpr bool handle_out_of_range(L& lhs, R rhs, P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, action); },
          [&]{ apply_wrap (lhs, rhs, action); },
          [&]{ return rhs; });
      }

      static constexpr void store(L& lhs, R rhs)
      {
        if constexpr (!index_raw<L>)
          lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (lower_of<L> == upper_of<L>)
          lhs = L::from_raw(0);   // notch_storage point grid: 0 is the only offset
        else if constexpr (has_qformat_fast_path<L>)
          lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage on a notch 1/K grid: the offset is an exact integer
        {
          rational raw = ((rhs - interval_of<L>.Lower)/notch_of<L>).value();
          lhs = L::from_raw(raw_cast<L>(raw.Numerator));
        }
      }

    public:
      // An integer lands on L's grid whenever the notch is 1/K over an integer
      // Lower (or the grid is continuous); otherwise it may fall between notches
      // and must round or report exactly like the same value given as a rational.
      static constexpr bool integers_on_grid =
          notch_of<L> == 0 || (notch_of<L>.Numerator == 1 && abs_den(lower_of<L>.Denominator) == 1);

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        // wrap/clamp bring any value into range (matches assign_intervals_ok).
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp>
                      || not excludes(interval_of<L>, interval_of<R>),
          "rhs type's range lies entirely outside lhs interval and the policy cannot bring it into range");

        if constexpr (wide_raw<L>)
          return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
        else if constexpr (!integers_on_grid)
          return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
        else
        {
          // The out-of-range check runs unconditionally — clamp/wrap
          // policies handle it via apply_*, which is constexpr-clean. Only the
          // unhandled-checked path winds up calling `policy.report`, which
          // contains its own `std::is_constant_evaluated()` guard.
          if constexpr (not includes(interval_of<L>, interval_of<R>))
          {
            if constexpr (is_integer_interval<L>)
            {
              // Skip the runtime range branch entirely when every handler would
              // be dead anyway — the dead branch otherwise inhibits autovec.
              if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>)
              {
                if (out_of_range(rhs)) [[unlikely]]
                {
                  // The integer clamp/wrap formulas need consecutive integers to be
                  // adjacent grid points (notch 1); a finer notch wraps modulo
                  // span + notch on the rational path.
                  if constexpr (notch_of<L> == 1)
                  {
                    if (handle_out_of_range(lhs, rhs, policy, action)) return lhs;
                  }
                  else
                    return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                }
              }
            }
            else if (not includes(interval_of<L>, rhs))
            {
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
  // assign(insidable, floating_point | rational)
  //---------------------------------------------------------------------------
  template <insidable L, typename R>
    requires fractional<R>
  struct assignment<L,R>
  {
    private:
      // A floating source with |rhs| ≥ 2^64 lies outside every grid and has no
      // rational form (rational(double) would fail): decide such values by sign.
      static constexpr bool huge(R const& rhs) noexcept
      {
        if constexpr (std::floating_point<R>)
          return !(rhs < 0x1p64 && rhs > -0x1p64);
        else
          return false;
      }

      static constexpr bool below_lower(R const& rhs)
      { return huge(rhs) ? rhs < 0 : rhs < lower_of<L>; }

      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action)
      {
        const bool low = below_lower(rhs);
        R clamped = low ? static_cast<R>(lower_of<L>) : static_cast<R>(upper_of<L>);
        R overshoot;
        if constexpr (std::same_as<R, rational>)
          overshoot = (rhs - clamped).value_or(rational{0});
        else
          overshoot = rhs - clamped;

        // The clamp target is an interval endpoint — a grid point — so the slot is 0
        // or max_index_v, no rounding. f64 takes the endpoint as a double, rational
        // the exact constant (a double round-trip would lose non-dyadic endpoints);
        // raw_from_offset<L> adds Lower back for direct-encoded storage.
        if constexpr (fp_raw<L>)
          lhs = L::from_raw(low ? static_cast<double>(lower_of<L>)
                                : static_cast<double>(upper_of<L>));
        else if constexpr (rational_raw<L>)
          lhs = L::from_raw(low ? lower_of<L> : upper_of<L>);
        else
          lhs = L::from_raw(raw_from_offset<L>(
              low ? umax{0} : max_index_v<L>));

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
      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
      {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        // The fold's quotient q is an imax: a floating source of 2^63 or more
        // cannot be wrapped with a deliverable carry (nor converted exactly).
        if constexpr (std::floating_point<R>)
          if (!(rhs < 0x1p63 && rhs > -0x1p63)) [[unlikely]]
          {
            policy.report(errc::overflow);
            return;
          }
        rational rhs_r{rhs};
        if constexpr (has_policy<L, P, snap>)
          rhs_r = round_to_lattice<L, P>(rhs_r);
        imax q;
        rational wrapped;
        if (abs_den(rhs_r.Denominator) == 1 && notch_of<L> == 1 && abs_den(lower_of<L>.Denominator) == 1)
        {
          // Integer value on a unit lattice: fold exactly (the grid may span
          // 2^64 values, past the rational range).
          using fold = unit_fold<L, source_lo<rational>, source_hi<rational>>;
          using W = typename fold::W;
          const auto [qq, w] = fold::fold(static_cast<W>(integer_wide(rhs_r)));
          const W v = fold::lower + w;
          q = qq;
          wrapped = v < W{0} ? -rational{static_cast<umax>(-v)} : rational{static_cast<umax>(v)};
        }
        else
        {
          // q = floor((rhs - lower) / range), wrapped = rhs - q * range. A step
          // past the 64-bit rational range, or a fold count past imax, cannot
          // be computed exactly: report it rather than store a wrapped guess.
          const auto span  = try_sub(upper_of<L>, lower_of<L>);
          const auto range = span ? try_add(*span, notch_of<L>) : span;
          const auto off   = try_sub(rhs_r, lower_of<L>);
          const auto quot  = (range && off) ? try_div(*off, *range) : off;
          if (!range || !quot || *quot < rational{std::numeric_limits<imax>::min()}
                              || *quot > rational{std::numeric_limits<imax>::max()}) [[unlikely]]
          {
            policy.report(errc::overflow);
            return;
          }
          q = floor(*quot);
          const auto qr = try_mul(rational{q}, *range);
          const auto w  = qr ? try_sub(rhs_r, *qr) : qr;
          if (!w) [[unlikely]] { policy.report(errc::overflow); return; }
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
      struct wide_slot_result { umax Slot; bool Exact; };
      template <typename P>
      [[gnu::cold, gnu::noinline]] static constexpr wide_slot_result wide_slot(rational const& v)
      {
        const exact_index_result r = exact_index<L, rounding_for<L, P>>(exact_of(v));
        return {static_cast<umax>(r.Index), r.Exact};   // in range: fits 64 bits
      }

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {})
      {
        if constexpr (rational_raw<L> && notch_of<L> == 0)
        { lhs = L::from_raw(rhs); return true; }   // continuous: store verbatim
        else if constexpr (fp_raw<L>)
        {
          // f64 target: raw IS the value — snap to the dyadic grid (range handling
          // already ran in the assign cascade; finite guard mirrors store_f64's).
          const double v = static_cast<double>(rhs);
          if (!(v - v == 0)) [[unlikely]]                  // assign() screens these first
          { policy.report(errc::not_finite); return false; }
          lhs = L::from_raw(snap_double<grid_of<L>, rounding_for<L, P>>(v));
          return true;
        }
        else if constexpr (lower_of<L> == upper_of<L>)
        {
          // Singleton grid: offset encoding → Raw=0; rational/direct → Raw = Lower.
          if constexpr (rational_raw<L>)
            lhs = L::from_raw(lower_of<L>);
          else if constexpr (!index_raw<L>)
            lhs = L::from_raw(raw_cast<L>(raw_lo<L>));
          else
            lhs = L::from_raw(0);
          return true;
        }
        else
        {
          // Store the k-th notch slot: rational storage holds the snapped value;
          // raw_from_offset<L> covers offset- and direct-encoded integers.
          auto store_slot = [&](auto k)
          {
            if constexpr (rational_raw<L>)
              lhs = L::from_raw((lower_of<L> + (rational{k} * notch_of<L>).value()).value());
            else
              lhs = L::from_raw(raw_from_offset<L>(k));
          };

          constexpr bool has_round_flag =
               has_policy<L, P, round_nearest> || has_policy<L, P, round_floor>
            || has_policy<L, P, round_ceil>    || has_policy<L, P, round_half_even>
            || has_policy<L, P, snap>;

          // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
          // (num − Lo·aden)·(K/g) / (aden/g), g = gcd(aden, K) — one gcd + integer ops
          // instead of two rational ops. round_quotient is invariant under reduction,
          // so the slot is bit-identical to the rational path. Oversized denominators
          // fall through (the kMaxDen guard keeps every product inside imax).
          if constexpr (has_qformat_fast_path<L> && !fp_raw<L> && notch_of<L> != 0)
          {
            constexpr imax K  = abs_den(notch_of<L>.Denominator);
            constexpr imax Lo = lower_imax<L>;
            constexpr umax kKM = []{
              // 2 · K · M with saturation (M bounds |value| and the offset span)
              umax k = static_cast<umax>(K);
              umax m = static_cast<umax>(
                  ceil(((detail::abs(lower_of<L>) > detail::abs(upper_of<L>)
                      ? detail::abs(lower_of<L>) : detail::abs(upper_of<L>))
                   ))) * 2 + 2;
              if (k > std::numeric_limits<umax>::max() / m)
                return std::numeric_limits<umax>::max();
              umax km = k * m;
              return (km > std::numeric_limits<umax>::max() / 2)
                       ? std::numeric_limits<umax>::max() : km * 2;
            }();
            constexpr umax kMaxDen =
                static_cast<umax>(std::numeric_limits<imax>::max()) / kKM;

            const rational rv{rhs};                       // exact (copy for rational R)
            const umax aden = abs_den(rv.Denominator);
            if (kMaxDen != 0 && aden <= kMaxDen)
            {
              const umax g    = std::gcd(aden, static_cast<umax>(K));
              const umax den2 = aden / g;
              const imax k2   = K / static_cast<imax>(g);
              const imax num  = signed_numerator(rv);
              const umax onum =                          // ≥ 0: rhs ≥ Lower (in range)
                  static_cast<umax>((num - Lo * static_cast<imax>(aden)) * k2);
              if (den2 == 1)
              { store_slot(onum); return true; }
              if constexpr (has_round_flag)
              { store_slot(round_quotient<L, P>(onum, den2)); return true; }
              // strict policy, off-notch: fall through to the rational path for
              // the error message / action plumbing (cold).
            }
          }

          // The exact quotient can overflow the 64-bit rational range (huge
          // source denominator × fine notch): take the slot from the exact wide
          // index instead (rhs is in range, so it fits L's raw). Rounding is
          // the value-space rule, as everywhere else.
          const auto quotient = (rhs - lower_of<L>)/notch_of<L>;
          if (!quotient.has_value()) [[unlikely]]
          {
            const wide_slot_result slot = wide_slot<P>(rational{rhs});
            if constexpr (!has_round_flag)
              if (!slot.Exact && policy.round_check()) [[unlikely]]
              {
                if constexpr (error_action<plain_t<A>>)
                { action.Fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
                policy.report(errc::rounding_error);
                return false;
              }
            store_slot(slot.Slot);
            return true;
          }
          rational raw = *quotient;
          umax den = static_cast<umax>(raw.Denominator);
          if (den == 1)
          { store_slot(raw.Numerator); return true; }

          if constexpr (has_round_flag)
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          else if (policy.round_check()) [[unlikely]]
          {
            if constexpr (error_action<plain_t<A>>)
            { action.Fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
            policy.report(errc::rounding_error);
            return false;
          }
          else
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          return true;
        }
      }

    private:
      // Range test for the source value. A floating source compares in double when
      // both endpoints are exact doubles (then the comparison is exact), instead of
      // converting the value to a rational first.
      static constexpr bool double_bounds_exact =
          std::floating_point<R>
          && rational{static_cast<double>(lower_of<L>)} == lower_of<L>
          && rational{static_cast<double>(upper_of<L>)} == upper_of<L>;

      // A rational source on integer endpoints compares by multiplying the
      // endpoint by the denominator (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d
      // exceeds any n) — exact, and no division.
      static constexpr bool integer_bounds =
          std::same_as<R, rational>
          && abs_den(lower_of<L>.Denominator) == 1 && abs_den(upper_of<L>.Denominator) == 1;

      static constexpr bool out_of_interval(R const& rhs)
      {
        if (huge(rhs))
          return true;
        if constexpr (double_bounds_exact)
        {
          constexpr double lo = static_cast<double>(lower_of<L>);
          constexpr double hi = static_cast<double>(upper_of<L>);
          return rhs < lo || rhs > hi;
        }
        else if constexpr (integer_bounds)
        {
          constexpr imax lo = signed_numerator(lower_of<L>);
          constexpr imax hi = signed_numerator(upper_of<L>);
          const umax n = rhs.Numerator, d = abs_den(rhs.Denominator);
          auto le = [&](umax m) { umax p; return mul_overflow(m, d, &p) || n <= p; };
          auto ge = [&](umax m) { umax p; return !mul_overflow(m, d, &p) && n >= p; };
          if (rhs.Denominator < 0 && n != 0)            // value −n/d < 0
          {
            bool in_lo, in_hi;
            if constexpr (lo >= 0) in_lo = false; else in_lo = le(safe_abs(lo));
            if constexpr (hi >= 0) in_hi = true;  else in_hi = ge(safe_abs(hi));
            return !(in_lo && in_hi);
          }
          bool in_lo, in_hi;                            // value n/d ≥ 0
          if constexpr (lo <= 0) in_lo = true;  else in_lo = ge(static_cast<umax>(lo));
          if constexpr (hi < 0)  in_hi = false; else in_hi = le(static_cast<umax>(hi));
          return !(in_lo && in_hi);
        }
        else
          return not includes(interval_of<L>, rhs);
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        if constexpr (wide_raw<L>)
        {
          // NaN / ±inf first, as below; a finite |rhs| ≥ 2^64 lies outside
          // every grid, so any value past ±2^64 stands in for it.
          if constexpr (std::floating_point<R>)
            if (!(rhs - rhs == 0) || huge(rhs)) [[unlikely]]
            {
              if (rhs != rhs)
              {
                if constexpr (error_action<plain_t<A>>) action.Fn(lhs, errc::not_finite, errc_message(errc::not_finite));
                else policy.report(errc::not_finite);
                return lhs;
              }
              if (!(rhs - rhs == 0) && !has_policy<L, P, clamp>)
              {
                if constexpr (error_action<plain_t<A>>) action.Fn(lhs, errc::not_finite, errc_message(errc::not_finite));
                else policy.report(errc::not_finite);
                return lhs;
              }
              const exact_int past = exact_int{1} << 65;
              return assign_exact<R>(lhs, exact_frac{rhs < 0 ? -past : past, exact_int{1}},
                                  policy, std::forward<A>(action));
            }
          return assign_exact<R>(lhs, exact_of(rational{rhs}), policy, std::forward<A>(action));
        }
        else
          return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
      }

    private:
      template<typename P, typename A>
      static constexpr L& assign_builtin(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        // NaN / ±inf: no rational value to round or range-check. clamp saturates
        // an infinity; everything else reports not_finite through the policy.
        if constexpr (std::floating_point<R>)
          if (!(rhs - rhs == 0)) [[unlikely]]
          {
            if constexpr (has_policy<L, P, clamp>)
              if (rhs == rhs)
                return assignment<L, rational>::assign(lhs, rhs > 0 ? upper_of<L> : lower_of<L>, policy);
            if constexpr (error_action<plain_t<A>>)
              action.Fn(lhs, errc::not_finite, errc_message(errc::not_finite));
            else
              policy.report(errc::not_finite);
            return lhs;
          }

        if (out_of_interval(rhs)) [[unlikely]]
        {
          // Round first: a value just outside may round onto an endpoint.
          if constexpr (rounds_before_range_check<L, plain_t<P>>)
            if (!huge(rhs))
              if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(rational{rhs}); rr.Ok)
              { lhs = L::from_raw(rr.Raw); return lhs; }
          // Fractional path has no wrap *action* branch (Wrappable = false).
          if (dispatch_out_of_range<false>(lhs, policy, action,
                [&]{ apply_clamp(lhs, rhs, policy, action); },
                [&]{ apply_wrap (lhs, rhs, policy, action); },
                [&]{ return rhs; }))
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
  struct assignment<L,R>
  {
    private:
      // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`.
      // Branches: L rational (pass value through), R rational (pre-divide by
      // notch_of<L>), both integer (the hot path, collapses to integer math).
      static constexpr rational calcOffset()
      {
        if constexpr (rational_raw<L>)
          return lower_of<R>;
        else if constexpr (notch_of<L> == 0)
          // Continuous fp_raw L: no grid to land on, mapping unused (store
          // routes through snap_double). 0 avoids the /notch_of<L> divide-by-zero.
          return rational{0};
        else if constexpr (rational_raw<R>)
          return -(lower_of<L>/notch_of<L>).value();
        else
          return ((lower_of<R> - lower_of<L>)/notch_of<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (rational_raw<L>)
          return notch_of<R>;
        else if constexpr (notch_of<L> == 0)
          // Continuous fp_raw L (see calcOffset). A denominator-1 Factor also
          // makes assign_notch_ok vacuously true (any value representable).
          return rational{0};
        else if constexpr (rational_raw<R>)
          return (rational{1}/notch_of<L>).value();
        else if constexpr (point_raw<R>)
          return rational{0};          // raw is always 0: the mapping is Offset alone
        else
          return (notch_of<R>/notch_of<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space integer-only mapping — requires integer raw storage on both
      // sides (not rational, not f64).
      // It also needs every raw and every mapped raw in imax: map_raw's L-raw
      // range is [Offset, Offset + Factor·max_index<R>] (+ Lower for value storage).
      static constexpr bool is_integer_mapping = [] {
        if constexpr (rational_raw<L> || rational_raw<R> || fp_raw<L> || fp_raw<R>
                      || abs_den(Factor.Denominator) != 1 || abs_den(Offset.Denominator) != 1
                      || !values_fit_imax<L> || !values_fit_imax<R>)
          return false;
        else
        {
          const rational base = index_raw<L> ? rational{0} : lower_of<L>;
          const auto lo = Offset + base;
          const auto span = Factor * rational{max_index_v<R>};
          if (!lo || !span) return false;
          const auto hi = *lo + *span;
          return hi.has_value()
              && *lo >= rational{std::numeric_limits<imax>::min()}
              && *hi <= rational{std::numeric_limits<imax>::max()};
        }
      }();

      // Non-integer mapping folded to one integer multiply-add:
      //   Offset + Factor·raw = (o_s·f_d + raw·f_n·o_d) / (o_d·f_d)
      // with every coefficient compile-time. round_quotient is invariant under
      // fraction reduction, so rounding the unreduced pair is bit-identical to
      // reducing through the two rational ops first. ok gates on every product
      // (including the worst-case runtime numerator over R's raw range)
      // provably fitting imax; mul/add/den are zeroed when not ok.
      struct affine_map_t { imax Mul; imax Add; imax Den; bool Ok; };
      static constexpr affine_map_t affine_map = []{
        constexpr affine_map_t no{0, 0, 0, false};
        if constexpr (rational_raw<L> || rational_raw<R> || fp_raw<L> || fp_raw<R>
                      || notch_of<L> == 0 || is_integer_mapping
                      || !values_fit_imax<L> || !values_fit_imax<R>)
          return no;
        else
        {
          constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
          if (Factor.Numerator > cap || Offset.Numerator > cap)
            return no;
          const imax f_n = static_cast<imax>(Factor.Numerator);  // Factor > 0
          const imax f_d = abs_den(Factor.Denominator);
          const imax o_s = signed_numerator(Offset);
          const imax o_d = abs_den(Offset.Denominator);
          affine_map_t m{0, 0, 0, true};
          if (mul_overflow(f_n, o_d, &m.Mul) || mul_overflow(o_s, f_d, &m.Add)
              || mul_overflow(o_d, f_d, &m.Den))
            return no;
          // worst-case |numerator| over R's offset range [0, max_index]
          if (max_index_v<R> > cap)
            return no;
          const imax rmax = static_cast<imax>(max_index_v<R>);
          imax term, num;
          if (mul_overflow(rmax, m.Mul, &term)
              || add_overflow(term, m.Add < 0 ? -m.Add : m.Add, &num))
            return no;
          // round_quotient equivalence: rounding is reduction-invariant, but
          // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
          // imax — mirror those checks for the unreduced den so both forms
          // take the same branch (ties on negatives differ across branches).
          constexpr auto zl = (lower_of<L> / notch_of<L>).value_or(rational{0});
          if (abs_den(zl.Denominator) == 1)
          {
            if (zl.Numerator > cap)
              return no;
            const imax mbias = signed_numerator(zl);
            imax mdi, total;
            if (mul_overflow(mbias, m.Den, &mdi) || add_overflow(mdi, num, &total))
              return no;
          }
          return m;
        }
      }();

      // Map rhs.Raw into L's raw space (requires is_integer_mapping). The
      // Offset/Factor formula assumes offset encoding both sides; for direct
      // storage, subtract lower_of<R> first (R-value → R-offset) and add lower_of<L>
      // after (raw_from_offset<L>). All integer (is_integer_mapping guarantees it).
      static constexpr imax map_raw(auto rhs_raw)
      {
        imax r_offset = rhs_raw;
        if constexpr (!index_raw<R>)
          r_offset -= raw_lo<R>;

        // Offset is an exact integer here, so trunc(Offset) is a constexpr constant.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + trunc(Offset);

        if constexpr (!index_raw<L>)
          return l_offset + raw_lo<L>;
        else
          return l_offset;
      }

    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action)
      {
        // raw_lo/raw_hi are already the correct Raw (no raw_from_offset). Real storage
        // takes the endpoint as a double (raw_lo/Hi truncate fractional dyadic endpoints).
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((as_rational(rhs) < lower_of<L>)
            ? static_cast<double>(lower_of<L>) : static_cast<double>(upper_of<L>));
        else
          lhs = L::from_raw((as_rational(rhs) < lower_of<L>)
            ? raw_cast<L>(raw_lo<L>) : raw_cast<L>(raw_hi<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are insides, so the overshoot is too. It is always
        // in-grid and on-notch for grid_of<R> − grid_of<L>, so the construction is exact.
        if constexpr (clamp_action<plain_t<A>>)
        {
          constexpr grid OG = (grid_of<R> - grid_of<L>).value();
          beman::inside::inside<OG> overshoot{ (as_rational(rhs) - as_rational(lhs)).value() };
          action.Fn(lhs, overshoot);
        }
      }

      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        // The integer modular wrap (range = Upper - Lower + 1, integer values) is
        // only correct on a unit-integer grid — notch 1 with integer bounds, so
        // consecutive integers are adjacent grid points — and for a source whose
        // values are integers (no rounding to do). Anything else routes through
        // the rational modular wrap, which rounds by the policy first.
        if constexpr (is_integer_interval<L> && abs_den(notch_of<L>.Denominator) == 1
                      && notch_of<L>.Numerator == 1 && !fp_raw<R> && is_integer_aligned<R>)
        {
          // Unit-integer fast path: modular wrap on the integer value, exact
          // (either grid may reach past int64; the span can be 2^64−1).
          using fold = unit_fold<L, wide_numerator(lower_of<R>), wide_numerator(upper_of<R>)>;
          using W = typename fold::W;
          const auto [excess, w] = fold::fold(static_cast<W>(integer_wide(as_rational(rhs))));
          lhs = L::from_raw(fold::raw_at(w));
          if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, make_wrap_carry<L, R>(excess));   // carry as an inside
        }
        else if constexpr (wrap_action<plain_t<A>>)
        {
          // Fractional destination with a wrap action: reuse the rational modular-wrap
          // path for the store/rounding, but wrap its imax carry `q` into an inside before
          // handing it to the user action.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy,
            beman::inside::on_wrap([&](auto& self, imax q){
              action.Fn(self, make_wrap_carry<L, R>(q));
            }));
        }
        else
        {
          // Fractional destination, no wrap action: delegate unchanged.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
        }
      }

      template<typename P, typename A>
      static constexpr bool try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, action); },
          [&]{ apply_wrap (lhs, rhs, policy, action); },
          [&]{ return as_rational(rhs); });
      }

      template<typename P>
      static constexpr void store(L& lhs, R const& rhs, P&& policy)
      {
        if constexpr (fp_raw<L>)
          // f64 target: raw IS the value — decode the source and snap to the dyadic
          // grid (the offset machinery below mis-encodes a double raw).
          lhs = L::from_raw(snap_double<grid_of<L>, rounding_for<L, P>>(as_double(rhs)));
        else if constexpr (rational_raw<L>)
          // rational target: raw IS the value — snap the decoded source through
          // the rational-rhs store (the offset machinery below would round the
          // VALUE to a notch index and store that number as the raw).
          assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy,
                                                 no_action{});
        else if constexpr (is_integer_mapping)
        {
          // exact: Factor and Offset have integer denominators, no rounding ambiguity
          if constexpr (Offset == 0 && Factor == 1 && same_encoding<L, R>)
            lhs = L::from_raw(raw_cast<L>(rhs.raw()));
          else
            lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
        }
        else if constexpr (affine_map.Ok)
        {
          // Folded non-integer mapping: one multiply-add, then the same
          // round_quotient (invariant under reduction — bit-identical to the
          // rational chain below).
          // Offset/Factor map R's 0-based offset: a value raw counts from Lower.
          const imax r_offset = static_cast<imax>(rhs.raw()) - (index_raw<R> ? imax{0} : raw_lo<R>);
          const imax num = affine_map.Add + r_offset * affine_map.Mul;
          const umax q = round_quotient<L, P>(
              static_cast<umax>(num < 0 ? -num : num),
              static_cast<umax>(affine_map.Den));
          lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<imax>(q))
                                    : raw_from_offset<L>(q));
        }
        else
        {
          // Offset/Factor map R's 0-based offset (a rational raw is the value,
          // which calcOffset/calcFactor already account for).
          const rational r_offset = [&] {
            if constexpr (rational_raw<R> || index_raw<R>) return rational{rhs.raw()};
            else return (rational{rhs.raw()} - lower_of<R>).value();
          }();
          rational rat = *(Offset + *(Factor * r_offset));
          umax ad = static_cast<umax>(abs_den(rat.Denominator));
          // Round the L-offset to a notch index in VALUE space via round_quotient
          // (same as the scalar path), honouring every rounding mode.
          umax q = round_quotient<L, P>(rat.Numerator, ad);
          // rat is the L-offset; raw_from_offset<L> adds lower_of<L> back for direct storage.
          lhs = L::from_raw((rat.Denominator < 0)
            ? raw_from_offset<L>(-static_cast<imax>(q))
            : raw_from_offset<L>(q));
        }
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        // A wide raw on either side: the exact wide path.
        if constexpr (wide_raw<L> || wide_raw<R>)
        {
          static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp>
                        || not excludes(interval_of<L>, interval_of<R>),
            "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
          static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
            "incompatible notches: use with_snap() or policy<snap>() to allow rounding");
          return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
        }
        else
          return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
      }

    private:
      template<typename P, typename A>
      static constexpr L& assign_builtin(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
        // for them (matches the integral-rhs path); only strict policies reject it.
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp>
                      || not excludes(interval_of<L>, interval_of<R>),
          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(notches_compatible<L, R> || has_policy<L, P, snap>,
          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        // A `f64` source holds its value as a double raw, which the raw-mapping
        // formulas below would misread as an index: take the double path.
        if constexpr (fp_raw<R>)
          return assignment<L, double>::assign(lhs, as_double(rhs), policy, std::forward<A>(action));
        else if constexpr (not includes(interval_of<L>, interval_of<R>))
        {
          if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>)
          {
            if constexpr (is_integer_mapping)
            {
              if (imax mapped = map_raw(rhs.raw()); mapped < raw_lo<L> || mapped > raw_hi<L>)
                if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
            }
            else if (const rational v = as_rational(rhs); not includes(interval_of<L>, v))
            {
              // Round first: a value just outside may round onto an endpoint.
              // (The integer mapping above lands on the lattice: nothing to round.)
              if constexpr (rounds_before_range_check<L, plain_t<P>>)
                if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(v); rr.Ok)
                { lhs = L::from_raw(rr.Raw); return lhs; }
              if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
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
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // policy — derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
  // `policy(ec)` makes E == error_ref carrying a `beman::inside::errc&`. No virtuals,
  // resolved at compile time. (The error-code channel reports `errc` directly —
  // there is no <system_error> dependency.)
  //---------------------------------------------------------------------------
  namespace detail
  {
    struct empty_ref{ };
    struct error_ref
    {
      constexpr error_ref(errc& ec):Code{ec} {}
      errc& Code;
    };
  }

  template<policy_flag W = none, typename E = detail::empty_ref>
  struct policy: E
  {
    constexpr policy() = default;
    constexpr policy(errc& ec) requires std::same_as<E, detail::error_ref>
    :E(ec) { }

    static constexpr bool test(policy_flag w)
    { return has_flag(W, w); }

    static constexpr bool range_check()
    {
      if (std::is_constant_evaluated()) return true;
      return is_checked(W) && not test(ignore_range);
    }

    static constexpr bool round_check()
    {
      if (std::is_constant_evaluated()) return true;
      return is_checked(W) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error) — also during constant evaluation, so
    // try_make / from_chars / tan_into return error values in constexpr code.
    // Throw mode funnels through the installed handler via an outlined cold
    // helper; at compile time it names a fixed-string diagnostic instead of
    // "non-constexpr function called".
    constexpr void report(errc code)
    {
      if constexpr (std::is_same_v<E, detail::error_ref>)
        E::Code = E::Code != errc{} ? E::Code : code;
      else
      {
        if (std::is_constant_evaluated())
          detail::constexpr_error<
            "inside: value out of range during constant evaluation "
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
  namespace detail
  {
    template<typename T>             inline constexpr bool is_policy = false;
    template<policy_flag F, typename E> inline constexpr bool is_policy<policy<F,E>> = true;

    // Concept form of is_policy — pulls cvref off so the constraint matches
    // forwarded `policy<F,E>` references in template parameters.
    template<typename T>
    concept policy_like = is_policy<std::remove_cvref_t<T>>;

    // policy_flags_of<T> — the flag-set a one-shot `policy<F,E>` carries (else
    // `none`). Lets the value+policy constructor and policy_ref's conversion fold
    // the per-call flags into their `inside_assignable` check, so a one-shot
    // clamp/round actually relaxes the constraint it enables.
    template<typename T>                inline constexpr policy_flag policy_flags_of = none;
    template<policy_flag F, typename E> inline constexpr policy_flag policy_flags_of<policy<F,E>> = F;

    // True for policy specializations that carry a beman::inside::errc& reference.
    // Free-fn arithmetic uses this to decide whether to call policy.report on
    // failure (which sets ec) vs. returning a silent std::unexpected (no-arg form).
    template<typename T>             inline constexpr bool uses_error_ref = false;
    template<policy_flag F>          inline constexpr bool uses_error_ref<policy<F, error_ref>> = true;
  }

  //---------------------------------------------------------------------------
  // make_policy
  //---------------------------------------------------------------------------
  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy()
  { return policy<F,detail::empty_ref>{}; }

  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy(errc& ec)
  { return policy<F,detail::error_ref>{ec}; }

  //---------------------------------------------------------------------------
  // report_or_unexpected — uniform "rational arithmetic failed" handler shared
  // by addition/multiplication/division/modulo. Three compile-time behaviors:
  // overflow_action<A> → fire it on a default Result; uses_error_ref<P> →
  // policy.report then std::unexpected{code}; plain throw-policy →
  // std::unexpected{code}.
  //---------------------------------------------------------------------------
  namespace detail
  {
  template <insidable Result, typename A, typename P>
  constexpr auto report_or_unexpected(A&& action, P&& policy, errc code,
                                      [[maybe_unused]] const char* what)
    -> std::conditional_t<overflow_action<A>, Result, std::expected<Result, errc>>
  {
    if constexpr (overflow_action<A>)
    {
      Result res;
      action.Fn(res, code);
      return res;
    }
    else
    {
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
  namespace detail
  {
  // Shared assignment dispatch: store `src` into `dst` under `policy` + the single
  // matching action from `actions` (at most one assignment-time tag is present).
  // Backs both policy_ref (dst = the wrapped inside) and policy_buffer (dst = a fresh
  // target), so the conversion/assignment logic lives in exactly one place.
  template <insidable Dst, numeric C, typename P, typename... As>
  constexpr Dst& dispatch_assign(Dst& dst, C const& src, P& policy, std::tuple<As...>& actions)
  {
    if constexpr (has_action<is_clamp_action, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<is_clamp_action>(actions));
    else if constexpr (has_action<is_wrap_action, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<is_wrap_action>(actions));
    else if constexpr (has_action<is_error_action, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<is_error_action>(actions));
    else
      return assignment<Dst, C>::assign(dst, src, policy);
  }

  // policy_buffer — the rvalue-receiver sibling of policy_ref. `with_snap()` etc.
  // on a *temporary* return this instead: it OWNS the inside by value (the temporary
  // is moved in), so the snapped value can be returned/stored without dangling —
  // `auto square(small n){ return (n*n).with_snap(); }` is safe. Value read-out only
  // (no operator=: assigning into a throwaway is meaningless). Same constrained
  // conversion as policy_ref, so it stays SFINAE-friendly.
  template<insidable B, typename P, typename... As>
  struct policy_buffer
  {
    B Owned;
    P Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    template <insidable Target>
      requires inside_assignable<Target, B, policy_of<Target> | policy_flags_of<P>>
    constexpr operator Target()
    {
      Target r;
      dispatch_assign(r, Owned, Policy, Actions);
      return r;
    }
  };

  template<insidable B, typename P, typename... As>
  struct policy_ref
  {
    private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned ClampCount    = count_action_matches<is_clamp_action,    As...>;
    static constexpr unsigned WrapCount     = count_action_matches<is_wrap_action,     As...>;
    static constexpr unsigned ErrorCount    = count_action_matches<is_error_action,    As...>;
    static constexpr unsigned OverflowCount = count_action_matches<is_overflow_action, As...>;

    static_assert(ClampCount + WrapCount + ErrorCount <= 1,
      "on_clamp / on_wrap / on_error are mutually exclusive in a single policy_ref");
    static_assert(ClampCount    <= 1, "duplicate on_clamp");
    static_assert(WrapCount     <= 1, "duplicate on_wrap");
    static_assert(ErrorCount    <= 1, "duplicate on_error");
    static_assert(OverflowCount <= 1, "duplicate on_overflow");

    public:
    B& Ref;
    P Policy;
    // `[[no_unique_address]]` is load-bearing: each captureless action lambda
    // is an empty type, and without this attribute the tuple would pad each
    // one out to a byte. With it, `policy_ref<B, P>` carrying no actions has
    // the same size as `policy_ref<B, P, no_action>`.
    [[no_unique_address]] std::tuple<As...> Actions;

    private:
    // Pre-pick the assignment-time action that matches the policy_ref's pack,
    // and forward to the single-action assignment::assign. At most one of the
    // four assignment-time tags is in the pack (enforced by static_assert), so
    // exactly one branch fires; the rest fall through to no-action.
    // Generic assignment: store `src` into `dst` under this ref's Policy + picked
    // action. `operator=` uses it with dst = Ref (the inside this ref wraps); the
    // conversion operator below uses it with a fresh target, so a one-shot snap can
    // be read out as a value (`(a * b).with_snap()`), not only assigned.
    template <insidable Dst, numeric C>
    constexpr Dst& assign_into(Dst& dst, C const& src)
    { return dispatch_assign(dst, src, Policy, Actions); }

    template <numeric C>
    constexpr B& assign_with_picked(C const& other)
    { return assign_into(Ref, other); }

    public:
    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assign_with_picked(other); }

    // Value read-out: a one-shot policy ref converts to any inside the assignment
    // could satisfy, applying the target's own policy (range) plus this ref's
    // carried flags (notch/rounding) via has_policy's merge. Makes
    // `Target t = (a * b).with_snap();` / `return (a * b).with_snap();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <insidable Target>
      requires inside_assignable<Target, B, policy_of<Target> | policy_flags_of<P>>
    constexpr operator Target()
    {
      Target r;
      assign_into(r, Ref);
      return r;
    }

    // expected<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    template <numeric C>
    constexpr B& operator=(std::expected<C, errc> const& other)
    { return assign_with_picked(other.value()); }

    private:
    constexpr void report_zero(errc code, const char* what)
    {
      if constexpr (has_action<is_error_action, As...>)
        pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
      else if constexpr (!has_policy<B, P, ignore_zero>)
        Policy.report(code);
    }

    public:
    //-------------------------------------------------------------------------
    // insidable RHS overloads — route through the inside's arithmetic, then
    // assign via assign_with_picked so callbacks fire on the narrowing back to B.
    // An expected<inside> result carrying an error (rational-raw overflow,
    // division by zero) surfaces its errc through on_overflow if registered,
    // else report.
    //-------------------------------------------------------------------------
    private:
    template <typename R>
    constexpr B& finalise_arith(R&& result, [[maybe_unused]] const char* msg)
    {
      if constexpr (is_expected_v<R>)
      {
        if (!result.has_value()) [[unlikely]]
        {
          if (result.error() == errc::division_by_zero)
            report_zero(errc::division_by_zero, msg);       // on_error / ignore_zero, like rational /=
          else if constexpr (has_action<is_overflow_action, As...>)
            pick_action_in<is_overflow_action>(Actions).Fn(Ref, result.error());
          else
            Policy.report(result.error());
          return Ref;
        }
        return assign_with_picked(result.value());
      }
      else
        return assign_with_picked(std::forward<R>(result));
    }

    // Shared body for the rational `+=`/`-=`/`*=`/`/=` operators: lift Ref to
    // rational and route the checked result through `finalise_arith`.
    template <typename RatOp>
    constexpr B& rational_assign(rational const& rhs, RatOp rat_op, const char* msg)
    { return finalise_arith(rat_op(rational{Ref}, rhs), msg); }
    public:

    template <insidable C>
    constexpr B& operator+=(C const& rhs)
    { return finalise_arith(add(Ref, rhs, Policy), "policy_ref::operator+= overflow"); }

    template <insidable C>
    constexpr B& operator-=(C const& rhs)
    { return finalise_arith(sub(Ref, rhs, Policy), "policy_ref::operator-= overflow"); }

    template <insidable C>
    constexpr B& operator*=(C const& rhs)
    { return finalise_arith(mul(Ref, rhs, Policy), "policy_ref::operator*= overflow"); }

    // A zero divisor is reported (on_error / ignore_zero) and leaves Ref
    // unchanged, like inside::operator/=: div/mod under ignore_zero skip their
    // own check, so dividing here would be a division by zero.
    template <insidable C>
    constexpr B& operator/=(C const& rhs)
    {
      if (rhs == 0)
      {
        if constexpr (!has_flag(policy_of<C>, ignore_zero))   // either operand silences it
          report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }
      return finalise_arith(div(Ref, rhs, Policy), "policy_ref::operator/= division/overflow");
    }

    template <insidable C>
    constexpr B& operator%=(C const& rhs)
    {
      if (rhs == 0)
      {
        if constexpr (!has_flag(policy_of<C>, ignore_zero))   // either operand silences it
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
    constexpr B& operator+=(C const& rhs)
    {
      return rational_assign(rhs, [](rational a, rational b){ return a + b; }, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(C const& rhs)
    {
      return rational_assign(rhs, [](rational a, rational b){ return a - b; }, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(C const& rhs)
    {
      return rational_assign(rhs, [](rational a, rational b){ return a * b; }, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(C const& rhs)
    {
      if (is_canonical_zero(rhs))
      {
        report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }
      return rational_assign(rhs, [](rational a, rational b){ return a / b; }, "policy_ref::operator/= division/overflow");
    }
  };
  } // namespace detail

} // namespace beman::inside


// ======================================================================
//  beman/inside/detail/addition.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/detail/rep.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// rep — representation-flag propagation for binary arithmetic results, shared
// by addition/multiplication/division. Propagate fp storage only when the
// result grid stays exactly representable in the chosen width; otherwise
// demote (f32→f64) or drop it so storage_pick deduces an exact representation
// (the fp result would diverge from the exact result — see grid::double_exact
// / float_exact). Widest-wins: prefer f32 only when both operands are
// f32-only and the result fits float; an f64 operand or a too-fine-for-float
// result widens to f64; too fine for double → exact. Division sets
// AllowContinuous: a continuous result (Notch 0) keeps fp regardless — the
// raw stores the quotient verbatim, so there is no grid to land on.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  template <insidable Lhs, insidable Rhs, grid ResultGrid, bool AllowContinuous = false>
  struct fp_rep
  {
    static constexpr bool any_f64 =
        has_flag(policy_of<Lhs>, f64) || has_flag(policy_of<Rhs>, f64);
    static constexpr bool any_f32 =
        has_flag(policy_of<Lhs>, f32) || has_flag(policy_of<Rhs>, f32);
    static constexpr bool continuous_ok = AllowContinuous && ResultGrid.Notch == 0;
    static constexpr bool keep_f32 =
        any_f32 && !any_f64 && (continuous_ok || float_exact<ResultGrid>);
    static constexpr bool keep_f64 =
        !keep_f32 && (any_f64 || any_f32) && (continuous_ok || double_exact<ResultGrid>);
    static constexpr bool dropped_fp = (any_f64 || any_f32) && !keep_f64 && !keep_f32;
    // Carry both operands' representation flags (widest-wins at storage selection).
    // `direct` needs notch 1 and `indexed` a non-zero notch; a result grid
    // that cannot hold them drops them (storage is then deduced).
    static constexpr policy_flag carried =
        (policy_of<Lhs> | policy_of<Rhs>)
        & (exact | (ResultGrid.Notch == 1 ? direct : none) | (ResultGrid.Notch != 0 ? indexed : none));
    static constexpr policy_flag rep =
        carried
        | (keep_f64 ? f64 : none) | (keep_f32 ? f32 : none);
    // The result inside's policy: the propagated representation, checked when
    // either operand is (a plain result is always checked); a representation
    // carried from two `unsafe` operands stays unchecked.
    static constexpr policy_flag result_policy =
        rep | ((rep == none || is_checked(policy_of<Lhs>) || is_checked(policy_of<Rhs>))
               ? checked : detail::unsafe_marker);
  };
}

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = grid_of<L> + grid_of<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on rational-raw results.
// Specialises on the storage shapes: rational result, mixed rational/integer,
// direct integer-space add, or both notch-offset (scale via lhs/rhs_widen).
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  template <insidable L, insidable R = L>
  struct addition
  {
    static_assert((grid_of<L> + grid_of<R>).has_value(),
      "addition: result grid's notch/interval exceeds the representable rational "
      "range — coarsen the operand grids");
    static constexpr grid result_grid = (grid_of<L> + grid_of<R>).value();
    // fp / representation propagation — shared rule in detail/rep.hpp.
    using rep_t = fp_rep<L, R, result_grid>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)
            || has_any_flag(F | policy_of<L> | policy_of<R>, exact))
        && !rational_add_is_safe(grid_of<L>, grid_of<R>);

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>,
                                        result, std::expected<result, errc>>;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> return_t<F, A>
  {
    result res;
    if constexpr (fp_raw<result>)
    {
      // Exact by construction, no snap: fp storage is kept only when the
      // result grid is double/float-exact (fp_rep), and grid values are notch
      // multiples, so the sum is itself a representable result-grid point and
      // the double add is exact. (Division still snaps — a quotient is not a
      // grid point.)
      res = result::from_raw(raw_cast<result>(as_double(lhs) + as_double(rhs)));
    }
    else if constexpr (rational_raw<result>)
    {
      static_assert(!wide_raw<L> && !wide_raw<R>,
        "addition: a wide-index operand with a continuous result is not supported yet");
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = rational::add(lhs,rhs);
        if (!sum) [[unlikely]]
          return report_or_unexpected<result>(action, policy, errc::overflow,
                                              "rational overflow in add");
        res = result::from_raw(*sum);
      }
      else
        res = result::from_raw(rational::add_unchecked(lhs, rhs));
    }
    else if constexpr (point_raw<result>)
      res = result::from_raw(raw_t<result>{});        // point + point: a point
    else if constexpr (integer_raw<L> && integer_raw<R>)
    {
      // Integer raws: add the value indices in result-notch units (the result
      // notch is gcd(N_L, N_R), so it divides both), in imax or by wrapping
      // arithmetic (wide_value.hpp). Exact for every grid, at any width.
      using W = index_work_t<result, L, notch_of<result>, R, notch_of<result>>;
      res = from_value_index<result>(value_in_units<W, notch_of<result>>(lhs)
                                   + value_in_units<W, notch_of<result>>(rhs));
    }
    else if constexpr (wide_raw<result>)
      // An fp or rational operand into a result with more than 2^64 slots.
      res = exact_result<result>(exact_of(lhs) + exact_of(rhs));
    else
    {
      // An fp or rational operand into an integer result: the exact rational
      // sum, converted to the result's raw.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - lower_of<result>) / notch_of<result>).value().Numerator));
    }
    return res;
  }
  };
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/detail/multiplication.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> inside<grid_of<L> * grid_of<R>>`. The
// integer hot path branches on which corner of the four-quadrant product hits
// `lower_of<result>`, doing the arithmetic as `umax * umax` (no signed overflow)
// plus integer offset corrections. Rational-result and all-integer-aligned
// cases come first.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  template <insidable L, insidable R = L>
  struct multiplication
  {
    static_assert((grid_of<L> * grid_of<R>).has_value(),
      "multiplication: result grid's notch/interval exceeds the representable "
      "rational range — coarsen the operand grids");
    static constexpr grid result_grid = (grid_of<L> * grid_of<R>).value();
    // fp / representation propagation — shared rule in detail/rep.hpp. The product
    // grid (notch = N_L·N_R) is finer, so demotion/dropping is the common case.
    using rep_t = fp_rep<L, R, result_grid>;
    static constexpr bool dropped_fp = rep_t::dropped_fp;
    using result = inside<result_grid, rep_t::result_policy>;

    // The dropped-fp case lands on a rational result when the product grid outgrows
    // uint index space; its product numerator can exceed `umax`, so check it (the
    // result carries `checked`) rather than wrap.
    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)
            || has_any_flag(F | policy_of<L> | policy_of<R>, exact) || dropped_fp)
        && !rational_mul_is_safe(grid_of<L>, grid_of<R>);

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>,
                                        result, std::expected<result, errc>>;

    // `x * just<c>` (c != 0): the result lattice is x's lattice scaled by c
    // (see grid operator*), so the result offset IS x's offset — counted from
    // the far end when c < 0. No multiply at all.
    template <insidable Point, insidable X>
    static constexpr bool point_scale =
        lower_of<Point> == upper_of<Point> && lower_of<Point> != 0
        && !rational_raw<X> && !fp_raw<X> && notch_of<X> != 0
        && !rational_raw<result> && !fp_raw<result>
        && !wide_raw<X> && !wide_raw<result>;

    // An operand's unit in the product grid (grid operator*): its notch, or
    // |c| for a point c.
    template <insidable X>
    static constexpr rational unit_of = (lower_of<X> == upper_of<X>) ? abs(lower_of<X>) : notch_of<X>;

    template <bool Negate, insidable X>
    static constexpr result scale_by_point(X const& x)
    {
      static_assert(max_index_v<result> == max_index_v<X>);
      umax off;
      if constexpr (index_raw<X>) off = static_cast<umax>(x.raw());
      else                        off = static_cast<umax>(x.raw()) - static_cast<umax>(raw_lo<X>);
      return result::from_raw(raw_from_offset<result>(Negate ? max_index_v<X> - off : off));
    }

    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> return_t<policy_flags_of<plain_t<P>>, A>
  {
    if constexpr (fp_raw<result>)
    {
      // Exact by construction, no snap (see addition.hpp): operands are notch
      // multiples, the product index |ia·ib| stays under the double_exact 2^53
      // gate, so the double multiply is exact and on the result lattice.
      return result::from_raw(raw_cast<result>(as_double(lhs) * as_double(rhs)));
    }
    else if constexpr (point_scale<R, L>)
      return scale_by_point<(lower_of<R> < 0)>(lhs);
    else if constexpr (point_scale<L, R>)
      return scale_by_point<(lower_of<L> < 0)>(rhs);
    else if constexpr (rational_raw<result>)
    {
      static_assert(!wide_raw<L> && !wide_raw<R>,
        "multiplication: a wide-index operand with a continuous result is not supported yet");
      if constexpr (needs_overflow_check<policy_flags_of<plain_t<P>>>)
      {
        auto prod = as_rational(lhs) * as_rational(rhs);
        if (!prod) [[unlikely]]
          return report_or_unexpected<result>(action, policy, errc::overflow,
                                              "rational overflow in mul");
        return result::from_raw(raw_cast<result>(*prod));
      }
      else
        return result::from_raw(raw_cast<result>(rational::mul_unchecked(
            as_rational(lhs), as_rational(rhs))));
    }
    else if constexpr (point_raw<result>)
      return result::from_raw(raw_t<result>{});     // a product with 0: the point 0
    else if constexpr (integer_raw<L> && integer_raw<R>)
    {
      // Integer raws: multiply the operands' values in their own units, in
      // imax or by wrapping arithmetic (wide_value.hpp). The product notch is the product
      // of those units (a notch, or |c| for a point c), so the product of the
      // unit counts is the result's value index — exact for every grid and
      // sign, at any width.
      using W = index_work_t<result, L, unit_of<L>, R, unit_of<R>>;
      static_assert(exact_quotient((unit_of<L> * unit_of<R>).value(), notch_of<result>) == grid_wide{1},
        "multiplication: the product notch is the product of the operand units");
      return from_value_index<result>(value_in_units<W, unit_of<L>>(lhs)
                                    * value_in_units<W, unit_of<R>>(rhs));
    }
    else if constexpr (wide_raw<result>)
      // An fp or rational operand into a result with more than 2^64 slots.
      return exact_result<result>(exact_of(lhs) * exact_of(rhs));
    else
    {
      // An fp or rational operand into an integer result (reached when `f64`
      // was dropped from a result grid that is not double-exact): the exact
      // rational product, converted to the result's raw.
      auto prod = rational::mul_unchecked(as_rational(lhs), as_rational(rhs));
      return result::from_raw(raw_from_offset<result>(
          ((prod - lower_of<result>) / notch_of<result>).value().Numerator));
    }
  }
  };
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/detail/division.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// division / modulo. `division::div` returns expected<result, errc> (division by zero
// is always runtime-possible). Two paths: native (integer-aligned grids +
// snap → native integer division) and rational (exact, can overflow under
// checked). `modulo::mod` is integer-only — non-integer remainders aren't
// well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  // Both operands are plain integer grids and the caller accepted integer
  // truncation (snap) — the prerequisite for native integer div / mod.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool integer_ops =
      ((F | policy_of<L> | policy_of<R>) & snap)
      && !rational_raw<L> && !rational_raw<R>
      && is_integer_aligned<L> && is_integer_aligned<R>;

  // ...and every value fits imax, so the builtin integer division applies.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool integer_native_ops =
      integer_ops<L, R, F> && values_fit_imax<L> && values_fit_imax<R>;

  //---------------------------------------------------------------------------
  // Rounding mode for the native div & mod paths (fire when `snap` is set).
  // Decided from the combined flags by rounding_of (policy_flag.hpp), the one
  // precedence all rounding paths share; `snap` alone is truncate-toward-zero.
  // The runtime quotient and the compile-time grid endpoints MUST agree on the
  // mode (both read div_round_mode), or a result could escape its own grid.
  //---------------------------------------------------------------------------
  constexpr round_mode div_round_mode(policy_flag eff) noexcept { return rounding_of(eff); }

  // Round the signed exact quotient a/b (b != 0) to an integer per `m`.
  template <std::signed_integral T>
  constexpr T div_rounded(T a, T b, round_mode m) noexcept
  {
    using U = std::make_unsigned_t<T>;
    const T t = a / b;                        // C++ truncation toward zero
    const T r = a % b;                        // sign of a, |r| < |b|
    if (r == 0 || m == round_mode::trunc) return t;
    const bool neg = (a < 0) != (b < 0);      // exact quotient is negative
    // |r|, |b| in U (safe for T::min); ab - ar is safe: 0 < ar < ab
    const U ar = r < 0 ? U(~U(r) + 1u) : U(r);
    const U ab = b < 0 ? U(~U(b) + 1u) : U(b);
    const T away = neg ? T(t - 1) : T(t + 1);
    switch (m)
    {
      case round_mode::floor:   return neg ? away : t;
      case round_mode::ceil:    return neg ? t : away;
      case round_mode::nearest: return (ar >= ab - ar) ? away : t;   // half away from zero
      case round_mode::half_even:
        if (ar != ab - ar) return (ar < ab - ar) ? t : away;
        return (t & 1) == 0 ? t : away;                           // tie → even
      default:                  return t;
    }
  }

  // The narrowest signed type in which native div/mod of L by R is exact: int32
  // when both value ranges fit (excluding INT32_MIN, so a / -1 cannot overflow),
  // else imax. A 32-bit divide is markedly cheaper than a 64-bit one.
  template <insidable L, insidable R>
  using native_div_t = std::conditional_t<
      (lower_imax<L> > std::numeric_limits<std::int32_t>::min()
       && upper_imax<L> <= std::numeric_limits<std::int32_t>::max()
       && lower_imax<R> > std::numeric_limits<std::int32_t>::min()
       && upper_imax<R> <= std::numeric_limits<std::int32_t>::max()),
      std::int32_t, imax>;

  // Round a non-negative quotient num/den (den != 0) per `m`. Used by the
  // Q-format path, whose raws are non-negative (Lower == 0).
  template <std::unsigned_integral U>
  constexpr U round_uquotient(U num, U den, round_mode m) noexcept
  {
    const U t = num / den, r = num % den;
    if (r == 0 || m == round_mode::trunc) return t;
    switch (m)
    {
      case round_mode::floor:   return t;             // non-negative: floor == trunc
      case round_mode::ceil:    return t + 1;
      case round_mode::nearest: return (r >= den - r) ? t + 1 : t;
      case round_mode::half_even:
        if (r < den - r) return t;
        if (r > den - r) return t + 1;
        return (t & 1) == 0 ? t : t + 1;
      default:                  return t;
    }
  }

  // Compile-time rounding of a quotient-interval endpoint to an integer index.
  // lo/hi differ only for half_even, where the endpoint is bracketed by
  // [floor, ceil] rather than reproducing the parity rule at compile time.
  constexpr imax round_rat_lo(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return round(q);
      case round_mode::floor:     return floor(q);
      case round_mode::ceil:      return ceil(q);
      case round_mode::half_even: return floor(q);
      default:                    return trunc(q);
    }
  }
  constexpr imax round_rat_hi(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return round(q);
      case round_mode::floor:     return floor(q);
      case round_mode::ceil:      return ceil(q);
      case round_mode::half_even: return ceil(q);
      default:                    return trunc(q);
    }
  }

  template <insidable L, insidable R = L, policy_flag F = none>
  struct division
  {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_qformat — both same Q-format (Notch = 1/N, Lower = 0); formula
    //                        `(a·N)/b` (the native `(a << log2 N)/b` idiom).
    // Otherwise the exact-rational path returns inside<rational>.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    static constexpr bool native_div_qformat =
        ((F | policy_of<L> | policy_of<R>) & snap)
        && is_qformat<L> && is_qformat<R>
        && notch_of<L> == notch_of<R>
        // raw·N must fit umax (the scaled dividend below)
        && !wide_raw<L> && !wide_raw<R>
        && max_index_v<L> <= ~umax{0} / abs_den(notch_of<L>.Denominator);

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode =
        div_round_mode(F | policy_of<L> | policy_of<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (grid_of<L> / grid_of<R>).has_value(),
      "division: result grid not representable (notch/interval exceeds the "
      "representable rational range) — coarsen the operand grids");
    static_assert(!native_div_qformat || (upper_of<L> / notch_of<R>).has_value(),
      "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid =
        native_div_integer
            ? grid{round_rat_lo((*(grid_of<L> / grid_of<R>)).Interval.Lower, rmode),
                   round_rat_hi((*(grid_of<L> / grid_of<R>)).Interval.Upper, rmode)}
      : native_div_qformat
            ? grid{interval{rational{0}, (upper_of<L> / notch_of<R>).value()}, notch_of<L>}
            : *(grid_of<L> / grid_of<R>);

    // fp / representation propagation — shared rule in detail/rep.hpp.
    // AllowContinuous: a continuous quotient (Notch 0) keeps fp verbatim.
    using rep_t = fp_rep<L, R, result_grid, /*AllowContinuous=*/true>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)
        || has_any_flag(G | F | policy_of<L> | policy_of<R>, exact);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>.
    // A wide-index operand's quotient may outgrow the 64-bit rational
    // whatever the policy, so that path always reports.
    static constexpr bool may_overflow_nonzero =
        !native_div && !fp_raw<result>
        && (needs_overflow_check<F> != 0 || wide_raw<L> || wide_raw<R>);

    // Real division can still fail on a zero divisor, so it uses the same
    // return-type rule as the rest: plain `result` when the op cannot fail
    // (overflow-action, or the divisor grid excludes zero with no rational
    // overflow), else expected<result, errc>. Real has no rational overflow, so
    // may_overflow_nonzero is false for it (above).
    template <typename A>
    using return_t = std::conditional_t<
        overflow_action<plain_t<A>> || (divisor_excludes_zero<R> && !may_overflow_nonzero),
        result,
        std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<insidable L, insidable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto division<L,R,F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> return_t<A>
  {
    // `fail` must stay well-formed even when return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    // Shared by the f64 and non-f64 paths (f64 fails only on a zero divisor).
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> return_t<A> {
      if constexpr (overflow_action<plain_t<A>>)
        return report_or_unexpected<result>(action, policy, code, what);   // -> result
      else if constexpr (!divisor_excludes_zero<R> || may_overflow_nonzero)
        return report_or_unexpected<result>(action, policy, code, what);   // -> expected<result, errc>
      else
        return result{};   // unreachable: divisor excludes zero, op cannot fail
    };

    // Div-by-zero check elided when R's grid excludes zero, or `ignore_zero` is
    // set (zero divisor is then UB, matching the `/= 0` no-op). The fail arms stay
    // keyed on divisor_excludes_zero (which narrows the return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = divisor_excludes_zero<R>
        || (((G | F | policy_of<L> | policy_of<R>) & ignore_zero) != 0);

    if constexpr (fp_raw<result>)
    {
      // Real division reports zero like every other path (throw / report /
      // action / unexpected). Finite operands keep the quotient finite, so no
      // non-finite ever reaches storage.
      if constexpr (!zero_unchecked)
        if (as_double(rhs) == 0.0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(raw_cast<result>(snap_double<grid_of<result>, rmode>(as_double(lhs) / as_double(rhs))));
    }
    else if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (lower_of<R> == 0). Formula folds to
      // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
      if constexpr (!zero_unchecked)
        if (rhs.raw() == 0) return fail(errc::division_by_zero, "division by zero in div");
      constexpr umax N = abs_den(notch_of<L>.Denominator);
      // 32-bit divide when the scaled dividend fits (Q8.8, Q16.15, ...).
      using U = std::conditional_t<(max_index_v<L> <= std::numeric_limits<std::uint32_t>::max() / N),
                                   std::uint32_t, umax>;
      return result::from_raw(raw_cast<result>(round_uquotient<U>(
          static_cast<U>(static_cast<U>(lhs.raw()) * U{N}), static_cast<U>(rhs.raw()), rmode)));
    }
    else if constexpr (native_div_integer)
    {
      using T = native_div_t<L, R>;
      const T rhs_val = static_cast<T>(to_value(rhs));
      if constexpr (!zero_unchecked)
        if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, imax{div_rounded(static_cast<T>(to_value(lhs)), rhs_val, rmode)});
      return res;
    }
    else if constexpr (wide_raw<L> || wide_raw<R>)
    {
      // A wide-index operand: the exact quotient, narrowed to the rational raw.
      const exact_frac d = exact_of(rhs);
      if constexpr (!zero_unchecked)
        if (d.Num.is_zero()) return fail(errc::division_by_zero, "division by zero in div");
      const auto q = try_rational(exact_of(lhs) / d);
      if (!q) [[unlikely]] return fail(errc::overflow, "rational overflow in div");
      return result::from_raw(*q);
    }
    else if constexpr (needs_overflow_check<G>)
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      auto q = as_rational(lhs) / rhs_r;
      if (!q) [[unlikely]] return fail(errc::overflow, "rational overflow in div");
      return result::from_raw(*q);
    }
    else
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
  }
  //---------------------------------------------------------------------------
  // modulo (requires integer-valued grids + snap)
  //---------------------------------------------------------------------------
  template <insidable L, insidable R, policy_flag F = none>
  struct modulo
  {
    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(integer_ops<L, R, F>, "modulo requires integer-valued grids and snap");

    // Builtin division when every value fits imax; else exact wide integers.
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    static constexpr rational max_rem =
        ((abs(lower_of<R>) > abs(upper_of<R>) ? abs(lower_of<R>) : abs(upper_of<R>)) - rational{1}).value();

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode =
        div_round_mode(F | policy_of<L> | policy_of<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::trunc && lower_of<L> >= 0)
        ? grid{rational{0}, max_rem}
        : grid{-max_rem, max_rem};

    using result = inside<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
    template <typename A>
    using return_t = std::conditional_t<
        overflow_action<plain_t<A>> || divisor_excludes_zero<R>,
        result,
        std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
  };

  template<insidable L, insidable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto modulo<L,R,F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> return_t<A>
  {
    if constexpr (!native_mod)
    {
      // Integer values past imax: r = a − round(a/b)·b in exact wide integers.
      constexpr bool zero_unchecked = divisor_excludes_zero<R>
          || (((G | F | policy_of<L> | policy_of<R>) & ignore_zero) != 0);
      const exact_int b = trunc(exact_of(rhs));
      if constexpr (!zero_unchecked)
        if (b.is_zero())
          return report_or_unexpected<result>(action, policy, errc::division_by_zero,
                                              "division by zero in mod");
      const exact_int a = trunc(exact_of(lhs));
      return exact_result<result>(exact_frac{a - rounded_div<rmode>(a, b) * b, exact_int{1}});
    }
    else
    {
      using T = native_div_t<L, R>;
      const T rhs_val = static_cast<T>(to_value(rhs));
      // Zero check elided when R's grid excludes zero (return_t is plain
      // `result`) or `ignore_zero` is set (zero divisor is then UB, matching `%= 0`).
      constexpr bool zero_unchecked = divisor_excludes_zero<R>
          || (((G | F | policy_of<L> | policy_of<R>) & ignore_zero) != 0);
      if constexpr (!zero_unchecked)
        if (rhs_val == 0)
          return report_or_unexpected<result>(action, policy, errc::division_by_zero,
                                              "division by zero in mod");
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
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   conversion_overflows<B>(v) — v falls outside B's interval.
//   conversion_rounds<B>(v)    — v is in-range but off-notch (would round).
//   conversion_is_lossy<B>(v)  — either of the two.
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool conversion_overflows(A value) noexcept
  {
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return true;   // NaN / ±inf fit no grid (and must not raise here)
    if constexpr (std::floating_point<A>)
      if (!(value < 0x1p64 && value > -0x1p64)) return true;   // beyond every grid, no rational form
    const detail::rational r = detail::as_rational(value);
    if (includes(interval_of<B>, r))
      return false;
    // B's policy rounds before it range-checks: a value that rounds onto the
    // grid does not overflow.
    detail::rational rounded;
    return !detail::rounds_into_range<B, policy<>>(r, rounded);
  }

  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool conversion_rounds(A value) noexcept
  {
    if constexpr (notch_of<B> == 0)
      return false;                       // continuous grid: no notch to miss
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return false;   // non-finite — overflow, not truncation
    if constexpr (std::floating_point<A>)
      if (!(value < 0x1p64 && value > -0x1p64)) return false;   // overflow, not truncation
    detail::rational r = detail::as_rational(value);
    if (not includes(interval_of<B>, r))
    {
      // Out of range: a rounding policy that brings it onto the grid rounds;
      // anything else is overflow, not rounding.
      detail::rational rounded;
      return detail::rounds_into_range<B, policy<>>(r, rounded);
    }
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - lower_of<B>) / notch_of<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
  }

  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool conversion_is_lossy(A value) noexcept
  {
    return conversion_overflows<B>(value)
        || conversion_rounds<B>(value);
  }
} // namespace beman::inside



// Deducing `this` (P0847) folds the lvalue/rvalue overload pairs below.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
#  error "beman.inside requires GCC 14 or newer (deducing this)"
#endif

// Forward-declare the `beman::inside::math` entry points used in-class, so the bodies
// pass `-Wtemplate-body` without pulling cmath.hpp in unconditionally (its
// definitions live there).
namespace beman::inside::math
{
  template <insidable Out, insidable In> constexpr Out floor_into(In x);
  template <insidable Out, insidable In> constexpr Out ceil_into (In x);
  template <insidable Out, insidable In> constexpr Out round_into(In x);
  template <insidable Out, insidable In> constexpr Out trunc_into(In x);
  template <insidable Out, insidable In> constexpr Out abs_into  (In x);
  template <insidable In> constexpr auto floor(In x);
  template <insidable In> constexpr auto ceil (In x);
  template <insidable In> constexpr auto round(In x);
  template <insidable In> constexpr auto trunc(In x);
  template <insidable In> constexpr auto abs  (In x);
}

//---------------------------------------------------------------------------
// inside — defines `inside<G, P>` and its member operators. Free-function
// arithmetic (arithmetic.hpp), casts (casts.hpp) and `inside_range` (range.hpp)
// follow in the umbrella. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap), and
// generic.hpp/policy.hpp (traits + policy machinery).
//---------------------------------------------------------------------------
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // inside
  //---------------------------------------------------------------------------
  template<grid G, policy_flag P>
  struct inside
  {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");
#ifndef BEMAN_INSIDE_MATH_NO_FP
    // Under the default (double) engine the `f64` policy is double-backed, and
    // its value snaps to the grid (Lower + k·Notch). That snap is only exact
    // when the grid is dyadic — power-of-two notch and Lower — so grid points
    // are representable in IEEE-754 double. A continuous grid (Notch == 0) has
    // no grid to snap to. Anything else is rejected here rather than silently
    // demoted to integer storage.
    static_assert(!has_flag(P, f64) || detail::dyadic_grid<G> || G.Notch == 0,
                  "inside: the `f64` policy requires a dyadic grid (power-of-two "
                  "notch and Lower, so values are exactly representable in double)");
    static_assert(!has_flag(P, f32) || detail::dyadic_grid<G> || G.Notch == 0,
                  "inside: the `f32` policy requires a dyadic grid (power-of-two notch "
                  "and Lower); values must also fit float's 24-bit significand "
                  "(checked at storage selection — see `float_exact`)");
#endif
    // Representation flags vs grid shape (exact has no requirement; a result
    // policy may carry several flags — storage selection resolves widest-wins,
    // so no mutual-exclusion asserts here).
    static_assert(!has_flag(P, direct) || G.Notch == 1,
                  "inside: the `direct` policy (raw == value as a plain integer) "
                  "requires Notch == 1");
    static_assert(!has_flag(P, indexed) || G.Notch != 0,
                  "inside: the `indexed` policy (raw == 0-based notch index) "
                  "requires a notch (Notch != 0)");

    using negative = inside<-G, P>;
    using raw_type = detail::storage_for_t<G, P>;

    private:
    [[no_unique_address]] raw_type Raw;   // empty for a point grid

    public:
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only an inside that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr raw_type const& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw()       noexcept
      requires (has_flag(P, unsafe)) { return Raw; }

    // Trivial default ctor — Raw is left uninitialized, like a built-in scalar: a
    // default-constructed inside has no value until assigned. (A previous checked
    // overload zero-filled Raw, which decoded to an out-of-range value or an invalid
    // {0,0} rational for grids not containing 0 — a defined-but-invalid footgun.
    // Value-init `inside{}` still zero-fills where a zero raw is genuinely wanted.)
    constexpr inside() = default;

    private:
    // Snap a value onto fp storage: lossless on the (fp-exact) dyadic grid — the
    // snap is computed in double and narrowed to the raw type (double or float),
    // which is exact because every grid point fits the raw's significand. Out-of-
    // range values run the same policy cascade as the fractional path (clamp →
    // wrap → checked-report → store as-is), with Pol's one-shot flags merged in.
    template <typename Pol>
    constexpr void store_fp(double v, Pol& pol)
    {
      constexpr policy_flag F = P | detail::policy_flags_of<std::remove_cvref_t<Pol>>;
      const double lo = static_cast<double>(G.Interval.Lower);
      const double hi = static_cast<double>(G.Interval.Upper);
      // NaN/±inf (`v - v` is NaN exactly then): clamp saturates an infinity,
      // anything else reports not_finite through the policy, like the
      // rational path.
      if (!(v - v == 0)) [[unlikely]]
      {
        if constexpr (has_flag(F, clamp))
          if (v == v) { Raw = static_cast<raw_type>(v > 0 ? hi : lo); return; }
        pol.report(errc::not_finite);
        return;
      }
      if (v < lo || v > hi)
      {
        // Round, then range-check (as assignment does): a value less than one
        // notch outside may snap onto an endpoint.
        if constexpr (G.Notch != 0 && has_flag(F, snap))
        {
          constexpr double nd = static_cast<double>(G.Notch);
          if (v > lo - nd && v < hi + nd)
          {
            const double s = detail::snap_double<G, detail::rounding_of(F), true>(v);
            if (s >= lo && s <= hi) { Raw = static_cast<raw_type>(s); return; }
          }
        }
        if constexpr (has_flag(F, clamp))
          v = v < lo ? lo : hi;
        else if constexpr (has_flag(F, wrap))
        {
          // Round onto the lattice first, as the rational path does, so the
          // folded value is a grid point and cannot round up past Upper.
          v = detail::snap_double<G, detail::rounding_of(F), /*AnySign=*/true>(v);
          // Fold into [Lower, Lower + range), range = span + notch — the same
          // convention as the fractional apply_wrap. floor(q) without an
          // unguarded imax cast: for |q| >= 2^52 the double is already integral
          // (floor(q) == q), else narrow to imax (safe) and adjust toward -inf.
          const double range = hi - lo + static_cast<double>(G.Notch);
          const double q = (v - lo) / range;
          const double aq = q < 0 ? -q : q;
          double kd;
          if (aq >= 4503599627370496.0)            // 2^52
            kd = q;
          else
          {
            const imax k = static_cast<imax>(q);   // |q| < 2^52 < imax
            kd = static_cast<double>(k);
            if (q < 0 && kd != q) kd -= 1.0;        // floor toward -inf
          }
          v -= kd * range;
        }
        else if (detail::range_fail(*this, pol))
          return;            // reported (error_code mode)
        // no handler (unchecked policy): fall through and store snapped as-is
      }
      Raw = static_cast<raw_type>(detail::snap_double<G, detail::rounding_of(F)>(v));   // float for f32: lossless
    }

    // The one store every constructor and assignment goes through; fp storage
    // takes the value as a double.
    template <numeric A, typename Pol>
    constexpr void store_value(A const& value, Pol&& pol)
    {
      if constexpr (!detail::fp_raw<inside>)
        detail::assignment<inside, A>::assign(*this, value, pol);
      else if constexpr (std::is_arithmetic_v<A>)
        store_fp(static_cast<double>(value), pol);
      else
        store_fp(static_cast<double>(detail::as_rational(value)), pol);
    }

    template <numeric A>
    constexpr void store_value(A const& value) { store_value(value, make_policy<P>()); }
    public:

    template <numeric A>
      requires inside_assignable<inside, A, P>
    constexpr inside(A value)
    { store_value(value); }

    // One-shot policy: `pol`'s flags widen the assignable check (a clamp/round
    // relaxes the interval/notch clause, e.g. clamp_round<B>(some_inside)) and
    // apply to this store. If it reports an error (ec mode), Raw is ill-defined.
    template <numeric A, typename Pol>
      requires inside_assignable<inside, A, P | detail::policy_flags_of<std::remove_cvref_t<Pol>>>
    constexpr inside(A value, Pol&& pol)
    { store_value(value, pol); }

    // No error-code constructor: construction that can fail is `try_make(value)`,
    // which returns expected<inside, errc>. This overload only turns
    // `inside x(value, ec)` into a readable error (an errc& would otherwise bind
    // the Pol&& constructor above).
    template <numeric A>
    constexpr inside(A, errc&)
    {
      static_assert(detail::dependent_false<A>,
          "inside(value, errc&) was removed: use `auto r = B::try_make(value);` "
          "(expected<B, errc>), or `b.policy(ec) = value` to assign with an error code");
    }

    // expected<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws
    // std::bad_expected_access on an error.
    template <numeric A>
      requires inside_assignable<inside, A, P>
    constexpr inside(std::expected<A, errc> const& value)
    { store_value(value.value()); }

    template <numeric B>
      requires inside_assignable<inside, B, P>
    constexpr inside& operator=(B const& other)
    { store_value(other); return *this; }

    template <numeric B>
      requires inside_assignable<inside, B, P>
    constexpr inside& operator=(std::expected<B, errc> const& other)
    { store_value(other.value()); return *this; }

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
      requires (!inside_assignable<inside, A, P>)
    constexpr inside(A)
    { static_assert(inside_assignable_why<inside, A, P>::value,
        "inside: cannot construct this inside from the value — see the per-clause notes above"); }

    template <numeric A>
      requires (!inside_assignable<inside, A, P>)
    constexpr inside(std::expected<A, errc> const&)
    { static_assert(inside_assignable_why<inside, A, P>::value,
        "inside: cannot construct this inside from the expected's value — see the per-clause notes above"); }

    template <numeric B>
      requires (!inside_assignable<inside, B, P>)
    constexpr inside& operator=(B const&)
    { static_assert(inside_assignable_why<inside, B, P>::value,
        "inside: cannot assign this value to this inside — see the per-clause notes above"); return *this; }

    template <numeric B>
      requires (!inside_assignable<inside, B, P>)
    constexpr inside& operator=(std::expected<B, errc> const&)
    { static_assert(inside_assignable_why<inside, B, P>::value,
        "inside: cannot assign this expected's value to this inside — see the per-clause notes above"); return *this; }
#endif

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr inside from_raw(raw_type r) noexcept
    { inside b; b.Raw = r; return b; }

    // Conversion summary:
    //   operator imax     — implicit, when the grid is notch-aligned and fits in
    //                       int64 (else use `to<imax>()`). Also the `vec[b]` index
    //                       path. No second implicit integer operator (would make
    //                       `imax_var += b` ambiguous).
    //   operator rational — implicit; lossless and exact.
    //   operator double   — implicit for an `f64` inside (dyadic grid → lossless);
    //                       explicit otherwise and gated on a rounding flag.
    //                       A strict inside opts in via `to<double>().value()`.
    //   to<T>()           — typed-error narrowing/widening → `expected<T, errc>`
    //                       (overflow / domain_error).
    //   as<T>()           — non-expected sibling; throws on error. For known-
    //                       in-range sites (array indexing). FP shares the gate.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    constexpr operator imax() const
      requires (detail::notch_is_unit_integer<G>
             && G.Interval.Lower >= detail::rational{std::numeric_limits<imax>::min()}
             && G.Interval.Upper <= detail::rational{std::numeric_limits<imax>::max()})
    { return detail::to_value(*this); }

    constexpr explicit(!has_flag(P, f64) && !has_flag(P, f32)) operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return detail::as_double(*this); }

    // Unavailable on a wide-index grid: its values outgrow the 64-bit rational
    // (compare it, or read it with to<T>()).
    constexpr operator detail::rational() const
      requires (!detail::is_wide_int_v<raw_type>)
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;
      else if constexpr (!detail::index_raw<inside>)
        return Raw;
      // Q-format-with-integer-Lower fast path skips the generic path's three
      // rational ops. Falls through to the rational path when the raw is too wide
      // to widen safely (e.g. uint64 from a Q16.16 × Q16.16 result type).
      else if constexpr (detail::has_qformat_fast_path<inside>)
        return detail::q_format_decode(*this);
      else
        return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    // to<T>() — typed-error scalar extraction (mirrors rational::to<T>, extended
    // to signed and floating point). Returns `errc::overflow` (out of T's range;
    // `errc::domain_error` for a negative value into unsigned T); fractional
    // truncation is silent. Bounds the grid already satisfies are not checked.
    template <std::integral T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      using lim = std::numeric_limits<T>;
      constexpr bool check_lo = lower_of<inside> < detail::rational{lim::min()};
      constexpr bool check_hi = upper_of<inside> > detail::rational{lim::max()};
      if constexpr (!check_lo && !check_hi && detail::values_fit_imax<inside>)
        return static_cast<T>(detail::to_value(*this));
      else if constexpr (detail::wide_raw<inside>)
      {
        const detail::exact_frac v = detail::exact_of(*this);
        if (check_lo && v < detail::exact_of(lim::min()))
          return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
        if (check_hi && v > detail::exact_of(lim::max()))
          return std::unexpected{errc::overflow};
        return static_cast<T>(trunc(v));
      }
      else
      {
        const auto r = detail::as_rational(*this);
        if (check_lo && r < detail::rational{lim::min()})
          return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
        if (check_hi && r > detail::rational{lim::max()})
          return std::unexpected{errc::overflow};
        return static_cast<T>(trunc(r));
      }
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict inside rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
      requires (!std::floating_point<T>
             || (P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return to<T>().value(); }

    // numerator() / denominator() — the exact value of a fractional inside as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
    // Gated on every value fitting imax (a grid reaching past int64 has no imax
    // numerator for its largest values — read it as a rational instead).
    [[nodiscard]] constexpr imax numerator() const
      requires detail::values_fit_imax<inside> { return fraction().first; }
    [[nodiscard]] constexpr imax denominator() const
      requires detail::values_fit_imax<inside> { return fraction().second; }

    private:
    // The reduced exact value as {numerator, positive denominator}. Integer
    // grids need no division; dyadic Q-format grids reduce by shifting out
    // common factors of two instead of a gcd.
    constexpr std::pair<imax, imax> fraction() const
    {
      if constexpr (detail::index_raw<inside> && detail::is_integer_aligned<inside>)
        return {detail::to_value(*this), 1};
      else if constexpr (detail::index_raw<inside> && detail::has_qformat_fast_path<inside>
                         && std::has_single_bit(detail::abs_den(G.Notch.Denominator)))
      {
        constexpr imax nd = detail::abs_den(G.Notch.Denominator);
        constexpr int  k  = std::countr_zero(static_cast<umax>(nd));
        const imax num = detail::raw_imax(*this) + detail::lower_imax<inside> * nd;
        const int  tz  = std::countr_zero(static_cast<umax>(num));   // num == 0: 64
        const int  s   = tz < k ? tz : k;
        return {num >> s, nd >> s};
      }
      else
      {
        auto r = detail::as_rational(*this);
        return {signed_numerator(r),
                static_cast<imax>(detail::abs_den(r.Denominator))};
      }
    }
    public:

    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `beman::inside::math` — `beman::inside::math::floor(b)` etc. (auto-deduced Out)
    // or `beman::inside::math::floor_into<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<beman/inside/cmath.hpp>`.

    [[nodiscard]] constexpr negative operator-() const
    {
      negative neg;
      if constexpr (detail::point_raw<inside>)
        neg = negative::from_raw({});                  // −point is a point: no raw
      else if constexpr (detail::fp_raw<inside>)
        neg = negative::from_raw(-Raw);
      else if constexpr (detail::rational_raw<inside>)
        neg = negative::from_raw(-(Raw));
      else
      {
        // Integer raws: the negated value index is −J (wide_value.hpp), in imax
        // when the bounds allow, else by wrapping. Index storage on both sides
        // counts the slot from the opposite end instead.
        using W = detail::index_work_t<negative, inside, G.Notch, inside, G.Notch>;
        if constexpr (detail::index_raw<inside> && detail::index_raw<negative>)
          neg = negative::from_raw(static_cast<detail::raw_t<negative>>(
              static_cast<W>(G.slot_count()) - static_cast<W>(Raw)));
        else
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
    [[nodiscard]] constexpr auto policy(this Self&& self)
    {
      auto pol = make_policy<P | F>();
      if constexpr (std::is_lvalue_reference_v<Self>)
        return detail::policy_ref<inside, decltype(pol)>{self, pol};
      else
        return detail::policy_buffer<inside, decltype(pol)>{std::move(self), pol};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(errc& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return detail::policy_ref<inside, decltype(pol)>{*this, pol};
    }

    // with_snap<Mode>() — opt this assignment into snapping with the given rounding
    // mode. Bare `with_snap()` is truncate-toward-zero (Mode == snap); pass an
    // explicit mode for the others: with_snap<round_nearest>(), <round_floor>,
    // <round_ceil>, <round_half_even>. Like policy<F>(), a temporary receiver
    // yields a value-owning policy_buffer.
    template <policy_flag Mode = snap, typename Self>
    [[nodiscard]] constexpr auto with_snap(this Self&& self)
    {
      static_assert(has_flag(Mode, snap),
        "with_snap<Mode>: Mode must be a snapping mode — snap (truncate), round_nearest, "
        "round_floor, round_ceil, or round_half_even");
      return std::forward<Self>(self).template policy<Mode>();
    }
    template <typename Self>
    [[nodiscard]] constexpr auto with_clamp(this Self&& self) { return std::forward<Self>(self).template policy<clamp>(); }
    template <typename Self>
    [[nodiscard]] constexpr auto with_wrap(this Self&& self)  { return std::forward<Self>(self).template policy<wrap>(); }

    private:
    // Shared builder for the single-action fluent hooks below (internal). Merges
    // the tag's implied policy flag, then returns a policy_ref bound to *this
    // carrying the tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action)
    {
       using tag = Tag<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | detail::implied_flags<tag>>();
       return detail::policy_ref<inside, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }
    public:

    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& a)     { return make_action_ref<on_wrap_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& a)    { return make_action_ref<on_clamp_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& a)    { return make_action_ref<on_error_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_overflow(A&& a) { return make_action_ref<on_overflow_t>(std::forward<A>(a)); }

    // Multi-action entry point: combine N tagged actions into one policy_ref.
    // policy_ref rejects mutually exclusive combinations at compile time. E.g.
    // `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs` — the arithmetic fires λ1,
    // the narrowing back into b fires λ2.
    template <typename... Actions>
    [[nodiscard]] constexpr auto with(Actions&&... actions)
    {
       constexpr policy_flag merged = detail::merged_implied_flags<Actions...>;
       auto pol = make_policy<P | merged>();
       return detail::policy_ref<inside, decltype(pol), std::remove_cvref_t<Actions>...>{
         *this, pol,
         std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

    private:
    // Raw-space fast paths of += and -=. Each adds a delta to the raw: a
    // compile-time constant (point rhs), the rhs raw, or −rhs raw − bias.
    //   point_delta<R>   — rhs is one whole number of notches: the delta.
    //   raw_add_ok<R>    — rhs raw adds directly (direct storage, or both
    //                      offset-encoded at Lower 0).
    //   raw_sub_ok<R>    — rhs raw subtracts with a constant bias.
    // When every raw and every new raw fits imax the add runs in imax
    // (store_raw); otherwise — a grid reaching past int64, or a sum that could
    // overflow — it runs exactly in 128 bits (store_raw_wide).
    template <insidable R>
    static constexpr bool point_delta_ok =
        !detail::rational_raw<inside> && !detail::fp_raw<inside> && notch_of<inside> != 0
        && lower_of<R> == upper_of<R>
        && (lower_of<R> / notch_of<inside>).has_value()
        && detail::abs_den((*(lower_of<R> / notch_of<inside>)).Denominator) == 1;

    template <insidable R>
    static constexpr bool raw_add_ok =
        !detail::rational_raw<inside> && !detail::rational_raw<R>
        && !detail::fp_raw<inside> && !detail::fp_raw<R>
        && notch_of<inside> == notch_of<R>
        && (!detail::index_raw<R> || (lower_of<inside> == 0 && lower_of<R> == 0));

    template <insidable R>
    static constexpr bool raw_sub_ok =
        !detail::rational_raw<inside> && !detail::rational_raw<R>
        && !detail::fp_raw<inside> && !detail::fp_raw<R>
        && notch_of<inside> != 0 && notch_of<inside> == notch_of<R>
        && (!detail::index_raw<R>
            || ((lower_of<R> / notch_of<inside>).has_value()
                && detail::abs_den((*(lower_of<R> / notch_of<inside>)).Denominator) == 1));

    // The rhs raw's exact range, as a delta: +raw for +=, −raw − bias for -=
    // (the bias is lower_of<R>/Notch for an index-raw rhs, else 0).
    template <insidable R>
    static constexpr detail::grid_wide point_delta = [] {
      const auto q = *(lower_of<R> / notch_of<inside>);
      return detail::wide_numerator(q);
    }();
    template <insidable R>
    static constexpr detail::grid_wide sub_bias = [] {
      if constexpr (detail::index_raw<R>) return point_delta<R>;
      else                                return detail::grid_wide{0};
    }();

    // Work type of a raw-space add whose delta lies in [Dlo, Dhi]: it holds
    // every raw of this grid, the delta, the new raw (in [raw_lo + Dlo,
    // raw_hi + Dhi]) and the wrap range raw_hi − raw_lo + 1, so the add cannot
    // overflow. imax for every grid within int64.
    // (A variable template, not a function: Clang would instantiate a plain
    // member function's body while the class is still incomplete.)
    template <detail::grid_wide Dlo, detail::grid_wide Dhi>
    static constexpr int raw_work_bits = [] {
      using W = detail::grid_wide;
      constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
      return detail::signed_value_bits_of({lo, hi, Dlo, Dhi, lo + Dlo, hi + Dhi, hi - lo + W{1}});
    }();
    template <detail::grid_wide Dlo, detail::grid_wide Dhi>
    using raw_work_t = detail::work_int_t<raw_work_bits<Dlo, Dhi>>;

    // The rhs raw's exact range (0 .. slot count, or Lower .. Upper).
    template <insidable R> static constexpr detail::grid_wide raw_min_of = detail::raw_lo_exact<R>;
    template <insidable R> static constexpr detail::grid_wide raw_max_of = detail::raw_hi_exact<R>;
    public:

    template <insidable R>
    constexpr inside& operator+=(R const& rhs)
    {
      // Point-inside rhs (just<v> / 1_ins / ++) whose value is a whole number of
      // this grid's notches: the raw delta is a compile-time constant and the
      // raw encoding cancels every Lower term (raw(v+d) = raw(v) + d/Notch for
      // offset and direct storage alike), so this compiles to one integer add.
      if constexpr (point_delta_ok<R>)
      {
        using W = raw_work_t<point_delta<R>, point_delta<R>>;
        constexpr W delta = static_cast<W>(point_delta<R>);
        return store_raw<W>(static_cast<W>(Raw) + delta);
      }
      // Fast path: raw-level integer addition, safe when raw_a + raw_b is the raw
      // of value_a + value_b — direct storage, or offset encoding with Lower==0 both.
      else if constexpr (raw_add_ok<R>)
      {
        using W = raw_work_t<raw_min_of<R>, raw_max_of<R>>;
        return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(rhs.raw()));
      }
      else
        return assign_op_result(*this + rhs);
    }

    private:
    // Store a raw computed by the raw-space fast paths of += and -=, in their
    // work type W. Under clamp/wrap/checked an out-of-range raw is clamped,
    // wrapped or reported.
    template <typename W>
    constexpr inside& store_raw(W new_raw)
    {
      constexpr W lo = static_cast<W>(detail::raw_lo_exact<inside>);
      constexpr W hi = static_cast<W>(detail::raw_hi_exact<inside>);
      if constexpr (has_any_flag(P, clamp | wrap)
                    || (is_checked(P) && !has_flag(P, ignore_range)))
        if (new_raw < lo || new_raw > hi)
        {
          if constexpr (P & clamp)
            new_raw = new_raw < lo ? lo : hi;
          else if constexpr (P & wrap)
          {
            constexpr W range = hi - lo + W{1};
            W w = (new_raw - lo) % range;
            if (w < W{0}) w += range;
            new_raw = lo + w;
          }
          else
          {
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
    constexpr inside& assign_op_result(Result const& r)
    {
      if constexpr (detail::is_expected_v<Result>)
      {
        // A failed op is reported through this type's policy (throw / handler);
        // *this stays unchanged. `*r` (not value()): no bad_expected_access.
        if (r.has_value())
          *this = *r;
        else
          make_policy<P>().report(r.error());
      }
      else
        *this = r;
      return *this;
    }

    // A zero divisor in /= or %=; ignore_zero on either operand silences it,
    // as it does for div/mod.
    template <typename R>
    constexpr inside& report_div_by_zero()
    {
      if constexpr (!has_flag(P | policy_of<R>, ignore_zero))
        make_policy<P>().report(errc::division_by_zero);
      return *this;
    }
    public:

    // Only a `rational` (a library type) may join an inside in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_ins` /
    // `just<1>` / `inside<{lo,hi}>{n}`), mirroring the binary operators.
    template <std::same_as<detail::rational> A>
    constexpr inside& operator+=(A const& rhs)
    { return assign_op_result(detail::rational{*this} + rhs); }

    template <insidable R>
    constexpr inside& operator-=(R const& rhs)
    {
      // Raw-space fast path, the subtraction mirror of +='s: with equal
      // notches, raw(v_l − v_r) = raw_l − raw_r − bias, where the bias is
      // lower_of<R>/Notch for an index-raw rhs (its raw is Lower-relative) and 0
      // for a value-raw rhs. Delegating to `+= (-rhs)` instead shifts R's
      // Lower by negation and defeats +='s raw path for index-backed grids.
      if constexpr (raw_sub_ok<R>)
      {
        using W = raw_work_t<-raw_max_of<R> - sub_bias<R>, -raw_min_of<R> - sub_bias<R>>;
        constexpr W bias = static_cast<W>(sub_bias<R>);
        return store_raw<W>(static_cast<W>(Raw) - static_cast<W>(rhs.raw()) - bias);
      }
      else
        return *this += (-rhs);
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator-=(A const& rhs)
    { return assign_op_result(detail::rational{*this} - rhs); }

    template <insidable R>
    constexpr inside& operator*=(R const& rhs)
    { return assign_op_result(*this * rhs); }

    // The outer zero check is semantic, not redundant: the binary `a / b`
    // yields an error value on a zero divisor (expected vocabulary), so the compound
    // form's report comes from here. (Measured perf-neutral to remove.)
    template <insidable R>
    constexpr inside& operator/=(R const& rhs)
    {
      if (rhs == 0)
        return report_div_by_zero<R>();
      return assign_op_result(*this / rhs);
    }

    template <insidable R>
    constexpr inside& operator%=(R const& rhs)
    {
      if (rhs == 0)
        return report_div_by_zero<R>();
      return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator*=(A const& rhs)
    { return assign_op_result(detail::rational{*this} * rhs); }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator/=(A const& rhs)
    {
      if (detail::is_canonical_zero(rhs))
        return report_div_by_zero<inside>();
      return assign_op_result(detail::rational{*this} / rhs);
    }

    // expected<inside> RHS (e.g. `x += a / b`): unwrap once, reporting an error
    // through this type's policy like any other failed compound op.
    template <insidable R>
    constexpr inside& operator+=(std::expected<R, errc> const& rhs) { return rhs ? (*this += *rhs) : report_error(rhs.error()); }
    template <insidable R>
    constexpr inside& operator-=(std::expected<R, errc> const& rhs) { return rhs ? (*this -= *rhs) : report_error(rhs.error()); }
    template <insidable R>
    constexpr inside& operator*=(std::expected<R, errc> const& rhs) { return rhs ? (*this *= *rhs) : report_error(rhs.error()); }
    template <insidable R>
    constexpr inside& operator/=(std::expected<R, errc> const& rhs) { return rhs ? (*this /= *rhs) : report_error(rhs.error()); }
    template <insidable R>
    constexpr inside& operator%=(std::expected<R, errc> const& rhs) { return rhs ? (*this %= *rhs) : report_error(rhs.error()); }

    private:
    constexpr inside& report_error(errc e) { make_policy<P>().report(e); return *this; }
    public:

    // ++/-- add the point inside `just<±1>` through the insidable += (which has
    // the raw-level integer fast path) instead of the rational round-trip,
    // which decodes to rational and re-stores through the full quotient/
    // rounding machinery (~30× the instructions on an integer grid). `just`
    // itself is declared after the class, so spell the point inside directly.
    constexpr inside& operator++()
    {
      // constexpr local: the point inside is materialised at compile time (the
      // ctor's error path otherwise blocks constant folding at -O3).
      constexpr auto kOne = inside<grid{detail::rational{1}}>{detail::rational{1}};
      return *this += kOne;
    }
    constexpr inside  operator++(int) { inside t = *this; ++*this; return t; }
    constexpr inside& operator--()
    {
      constexpr auto kMinusOne =
          inside<grid{detail::rational{-1}}>{detail::rational{-1}};
      return *this += kMinusOne;
    }
    constexpr inside  operator--(int) { inside t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value)
    {
      errc ec{};
      inside result;
      result.store_value(value, make_policy<P>(ec));   // the constructors' store
      if (ec != errc{}) return std::unexpected{ec};
      return result;
    }
  };

  //---------------------------------------------------------------------------
  // to<T>(b) / as<T>(b) — free-function forms, for generic code that would
  // otherwise need the `.template` disambiguator. Same semantics as the members.
  //---------------------------------------------------------------------------
  template <typename T, insidable B>
  [[nodiscard]] constexpr auto to(B const& b)
    requires requires { b.template to<T>(); }
  { return b.template to<T>(); }

  template <typename T, insidable B>
  [[nodiscard]] constexpr T as(B const& b)
    requires requires { b.template as<T>(); }
  { return b.template as<T>(); }

  //---------------------------------------------------------------------------
  // from_chars<B>(first, last) — text → B, exactly (no double round-trip). The
  // whole range must be one number: an optional sign, then the literal grammar
  // (1'000, 1.25, 1.5e2, 0xff, 0b1010, 0x1.8p3) or a fraction N/D. Malformed text
  // is errc::invalid_format; the value then goes through B::try_make, so B's
  // policy rounds, clamps or wraps it and reports overflow / rounding_error.
  // (io.hpp adds a std::string_view overload and operator>>.)
  //---------------------------------------------------------------------------
  template <insidable B>
  [[nodiscard]] constexpr std::expected<B, errc> from_chars(const char* first, const char* last)
  {
    const auto v = detail::parse_text(first, last);
    if constexpr (detail::wide_raw<B>)
      if (!v && v.error() == errc::overflow)
      {
        // A value past the 64-bit rational: parse it exactly instead.
        const auto w = detail::parse_exact(first, last);
        if (!w) return std::unexpected{w.error()};
        errc ec{};
        B b;
        detail::assign_exact<detail::rational>(b, *w, make_policy<policy_of<B>>(ec), no_action{});
        if (ec != errc{}) return std::unexpected{ec};
        return b;
      }
    if (!v) return std::unexpected{v.error()};
    return B::try_make(*v);
  }

  //---------------------------------------------------------------------------
  // comparison
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Integer value-index comparison eligibility: an integer-backed inside
    // whose value indices (value/Notch — integral by the grid anchor
    // invariant) fit imax, so two same-notch insides compare as
    // `bias + raw` without a rational decode.
    template <insidable B>
    inline constexpr bool index_cmp_fits = []{
      if constexpr (rational_raw<B> || fp_raw<B> || notch_of<B> == 0 || !values_fit_imax<B>)
        return false;
      else
      {
        constexpr auto lo = lower_of<B> / notch_of<B>;
        constexpr auto hi = upper_of<B> / notch_of<B>;
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        return lo.has_value() && hi.has_value()
            && (*lo).Numerator <= cap && (*hi).Numerator <= cap;
      }
    }();

    // Signed value index of Raw == 0: Lower/Notch for offset (index) storage,
    // 0 for direct storage (raw is already the value == the index at notch 1).
    template <insidable B>
    inline constexpr imax index_cmp_bias = []{
      if constexpr (index_raw<B>)
      {
        constexpr auto lo = *(lower_of<B> / notch_of<B>);
        return signed_numerator(lo);
      }
      else
        return imax{0};
    }();
  }

  namespace detail
  {
    inline constexpr auto three_way = [](auto const& a, auto const& b) { return a <=> b; };
    inline constexpr auto equal_to  = [](auto const& a, auto const& b) { return a == b; };

    // Every value of B is exactly a double (fp storage, or a double-exact grid).
    template <insidable B>
    inline constexpr bool exact_in_double = fp_raw<B> || double_exact<grid_of<B>>;

    // inside ⋈ inside (⋈ = `cmp`: <=> or ==) in the cheapest exact form the two
    // storage shapes allow.
    template <insidable L, insidable R, class Cmp>
    constexpr auto compare(L const& lhs, R const& rhs, Cmp cmp)
    {
      // same grid and encoding: Raw is monotonically ordered and comparable
      if constexpr (grid_of<L> == grid_of<R> && same_encoding<L, R>)
        return cmp(lhs.raw(), rhs.raw());
      // a wide-index operand: exact wide fractions
      else if constexpr (wide_raw<L> || wide_raw<R>)
        return cmp(exact_of(lhs), exact_of(rhs));
      // an fp-backed operand: compare in double when both sides' values are
      // exact in double (raw_imax would truncate the fp raw); otherwise the
      // rational fallback below keeps the comparison exact.
      else if constexpr ((fp_raw<L> || fp_raw<R>) && exact_in_double<L> && exact_in_double<R>)
        return cmp(as_double(lhs), as_double(rhs));
      // both integer-direct (notch=1, Raw==value): compare as integers
      else if constexpr (value_raw<L> && value_raw<R> && values_fit_imax<L> && values_fit_imax<R>)
        return cmp(raw_imax(lhs), raw_imax(rhs));
      // same nonzero notch, integer-backed: compare signed value indices
      // (compile-time bias + raw) — e.g. two same-Q-format fixed-point types
      // with different intervals, without the rational decode.
      else if constexpr (notch_of<L> == notch_of<R> && index_cmp_fits<L> && index_cmp_fits<R>)
        return cmp(index_cmp_bias<L> + raw_imax(lhs), index_cmp_bias<R> + raw_imax(rhs));
      else
        return cmp(as_rational(lhs), as_rational(rhs));
    }
  }

  template <insidable L, insidable R>
  [[nodiscard]] constexpr auto operator<=>(L const& lhs, R const& rhs) { return detail::compare(lhs, rhs, detail::three_way); }

  template <insidable L, insidable R>
  [[nodiscard]] constexpr bool operator==(L const& lhs, R const& rhs) { return detail::compare(lhs, rhs, detail::equal_to); }

  namespace detail
  {
    // inside ⋈ integral scalar without the rational decode: with the positive
    // notch n/d, value ⋈ c ⟺ (bias + raw)·n ⋈ c·d — both sides exact
    // integers (bias + raw is the signed value index, exact ordering AND
    // equality since c·d is exact too). Eligible when both cross terms
    // provably fit imax for every representable c of type A.
    template <insidable B, typename A>
    inline constexpr bool scalar_index_cmp_fits = []{
      if constexpr (!std::integral<A> || !index_raw<B> || !index_cmp_fits<B>)
        return false;
      else
      {
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        constexpr umax notch_num = notch_of<B>.Numerator;
        constexpr umax notch_den = static_cast<umax>(notch_of<B>.Denominator); // Notch > 0
        constexpr umax index_mag = []{
          constexpr auto lo = *(lower_of<B> / notch_of<B>);
          constexpr auto hi = *(upper_of<B> / notch_of<B>);
          return lo.Numerator > hi.Numerator ? lo.Numerator : hi.Numerator;
        }();
        constexpr umax scalar_mag = []{
          umax mag = static_cast<umax>(std::numeric_limits<A>::max());
          if constexpr (std::signed_integral<A>)
          {
            umax min_mag = static_cast<umax>(
                -(std::numeric_limits<A>::min() + 1)) + 1;
            if (min_mag > mag) mag = min_mag;
          }
          return mag;
        }();
        umax product;
        return !mul_overflow(index_mag, notch_num, &product) && product <= cap
            && !mul_overflow(scalar_mag, notch_den, &product) && product <= cap;
      }
    }();
  }

  namespace detail
  {
    // inside ⋈ arithmetic scalar. Integer storage compares as integers when the
    // scalar's type fits imax, and in double when it is floating point and the
    // grid's values are exact in double; everything else goes through rational.
    template <insidable B, arithmetic A, class Cmp>
    constexpr auto compare_scalar(B const& lhs, A rhs, Cmp cmp)
    {
      constexpr bool imax_scalar = std::signed_integral<A> || (std::unsigned_integral<A> && sizeof(A) < sizeof(imax));
      constexpr bool double_exact_values = lower_of<B> >= rational{-(imax{1} << 53)} && upper_of<B> <= rational{imax{1} << 53};
      if constexpr (wide_raw<B>)
      {
        // Every grid number lies strictly inside ±2^64; so does a wide grid.
        if constexpr (std::floating_point<A>)
          if (rhs == rhs && !(rhs < 0x1p64 && rhs > -0x1p64))
            return cmp(exact_of(0), exact_of(rhs < 0 ? -1 : 1));
        return cmp(exact_of(lhs), exact_of(as_rational(rhs)));
      }
      else if constexpr (value_raw<B> && values_fit_imax<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
      else if constexpr (value_raw<B> && values_fit_imax<B> && std::floating_point<A> && double_exact_values)
        return cmp(static_cast<double>(raw_imax(lhs)), static_cast<double>(rhs));
      else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(notch_of<B>.Numerator),
                   static_cast<imax>(rhs) * notch_of<B>.Denominator);
      else
      {
        // |rhs| ≥ 2^64 (or infinite) has no rational form, and every grid value
        // lies strictly inside ±2^64: the sign of rhs decides.
        if constexpr (std::floating_point<A>)
          if (rhs == rhs && !(rhs < 0x1p64 && rhs > -0x1p64))
            return cmp(rational{0}, rational{rhs < 0 ? -1 : 1});
        return cmp(as_rational(lhs), rational{rhs});
      }
    }
  }

  template <insidable B, detail::arithmetic A>
  [[nodiscard]] constexpr auto operator<=>(B const& lhs, A rhs) { return detail::compare_scalar(lhs, rhs, detail::three_way); }

  template <insidable B, detail::arithmetic A>
  [[nodiscard]] constexpr bool operator==(B const& lhs, A rhs) { return detail::compare_scalar(lhs, rhs, detail::equal_to); }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = inside<grid{value}>{value};

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
  template<char... Chars>
  constexpr auto operator""_ins() { return just<detail::parse_ins_literal<Chars...>()>; }

} // namespace beman::inside


// ======================================================================
//  beman/inside/casts.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Free-function casts complementing the constructors. Unlike a direct B{value}
// call, these read naturally in algorithm callbacks and make the intent (clamp
// vs. wrap vs. throw vs. trust) explicit at the call site.
//---------------------------------------------------------------------------
namespace beman::inside
{
  // Each cast constructs via the value+policy constructor, passing a one-shot
  // policy that overrides B's declared one for this conversion only.
  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_cast(N value)
  { return B{value, make_policy<clamp>()}; }

  // `wrap_cast` — modular semantics: the input is reduced into the target grid's
  // interval rather than clipped. For integer-style wraparound (angles, indices).
  template <insidable B, numeric N>
  [[nodiscard]] constexpr B wrap_cast(N value)
  { return B{value, make_policy<wrap>()}; }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round — compose `clamp` with a rounding
  // mode: the canonical "double in, bounded integer out, never throw" pipeline.
  //---------------------------------------------------------------------------
  namespace detail
  {
  template <insidable B, policy_flag RoundMode, numeric N>
  [[nodiscard]] constexpr B clamp_with_rounding(N value)
  { return B{value, make_policy<clamp | RoundMode>()}; }
  }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_floor(N value)
  { return detail::clamp_with_rounding<B, round_floor>(value); }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_ceil(N value)
  { return detail::clamp_with_rounding<B, round_ceil>(value); }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_round(N value)
  { return detail::clamp_with_rounding<B, round_nearest>(value); }

  // `checked_cast` — throws (via the installed handler) when the value would not
  // fit exactly: errc::overflow out of the interval (as to<T> and the predicate
  // name it), errc::rounding_error off the notch. Any numeric source, insides
  // included; once both checks pass the store is exact.
  template <insidable B, numeric A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (conversion_overflows<B>(value))
      detail::raise(errc::overflow, "checked_cast: value out of inside interval");
    if (conversion_rounds<B>(value))
      detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value, make_policy<snap>()};
  }

  // `unchecked_cast` routes through `inside<G, unsafe>` so the compiler elides
  // every domain/round check. UB if the value is actually out of range.
  template <insidable B, numeric A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    // Keep B's representation flags so the twin's raw layout is B's.
    constexpr policy_flag representation =
        policy_of<B> & (exact | f64 | f32 | direct | indexed | raw_width_mask);
    using twin = inside<grid_of<B>, unsafe | representation>;
    return B::from_raw(twin{value}.raw());   // same grid → identical raw layout
  }

} // namespace beman::inside


// ======================================================================
//  beman/inside/arithmetic.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Free-function arithmetic — wraps detail::addition/multiplication/division/
// modulo with caller-friendly overloads:
//   add(l, r) / add(l, r, policy<F>{}) / add(l, r, on_overflow(λ)) /
//   add(l, r, ec) / l + r
// Plus the variadic folds add_all/mul_all and *_into<Target>, and the
// std::expected operator overloads (an error operand propagates through).
//---------------------------------------------------------------------------
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // add / sub / mul / div / mod — each takes one of three trailing forms:
  //   op(l, r [, policy [, action]])   explicit per-call policy (+ action)
  //   op(l, r, on_overflow(λ), ...)    tagged actions (policy = their implied flags)
  //   op(l, r, ec [, action])          error-code form (checked, reports into ec)
  // detail::arith normalises the form to (policy, action) for the op's core.
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Arithmetic fires only on_overflow; any other action would be silently
    // ignored, so it is rejected.
    template <class A>
    inline constexpr bool arith_action =
        std::same_as<std::remove_cvref_t<A>, no_action> || overflow_action<std::remove_cvref_t<A>>;

    template <class Op, class L, class R, policy_like P = policy<>, class A = no_action>
    constexpr auto arith(Op op, L const& l, R const& r, P&& pol = {}, A&& act = {})
    {
      static_assert(arith_action<A>,
        "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
        "apply to assignment — use them with b.with(...) or a cast");
      return op(l, r, std::forward<P>(pol), std::forward<A>(act));
    }

    // Action-first form: on_overflow actions (the only kind arithmetic fires;
    // any other tag is rejected below with a message, not a bare mismatch).
    template <class A>
    inline constexpr bool is_action_tag =
        is_overflow_action<A>::value || is_clamp_action<A>::value
        || is_wrap_action<A>::value || is_error_action<A>::value;

    template <class Op, class L, class R, class... Actions>
      requires (sizeof...(Actions) >= 1)
            && (is_action_tag<std::remove_cvref_t<Actions>> && ...)
    constexpr auto arith(Op op, L const& l, R const& r, Actions&&... acts)
    {
      static_assert((overflow_action<std::remove_cvref_t<Actions>> && ...),
        "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
        "apply to assignment — use them with b.with(...) or a cast");
      return op(l, r, make_policy<merged_implied_flags<Actions...>>(),
                pick_action<is_overflow_action>(acts...));
    }

    template <class Op, class L, class R, class A = no_action>
    constexpr auto arith(Op op, L const& l, R const& r, errc& ec, A&& act = {})
    {
      static_assert(arith_action<A>,
        "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error "
        "apply to assignment — use them with b.with(...) or a cast");
      return op(l, r, make_policy<checked>(ec), std::forward<A>(act));
    }

    template <class P> inline constexpr policy_flag flags_of = policy_flags_of<std::remove_cvref_t<P>>;

    struct add_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return addition<L, R>::add(l, r, std::forward<P>(p), std::forward<A>(a)); }
    };
    struct sub_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return add_op{}(l, -r, std::forward<P>(p), std::forward<A>(a)); }
    };
    struct mul_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return multiplication<L, R>::mul(l, r, std::forward<P>(p), std::forward<A>(a)); }
    };
    struct div_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return division<L, R, flags_of<P>>::div(l, r, p, std::forward<A>(a)); }
    };
    struct mod_op
    {
      template <class L, class R, class P, class A>
      constexpr auto operator()(L const& l, R const& r, P&& p, A&& a) const
      { return modulo<L, R, flags_of<P>>::mod(l, r, p, std::forward<A>(a)); }
    };
  }

#define BEMAN_INSIDE_ARITH_FN(name, op)                                              \
  template <insidable L, insidable R, class... Args>                                 \
    requires requires(L const& l, R const& r, Args&&... args)                        \
      { detail::arith(detail::op{}, l, r, std::forward<Args>(args)...); }            \
  [[nodiscard]] constexpr auto name(L const& lhs, R const& rhs, Args&&... args)      \
  { return detail::arith(detail::op{}, lhs, rhs, std::forward<Args>(args)...); }

  BEMAN_INSIDE_ARITH_FN(add, add_op)
  BEMAN_INSIDE_ARITH_FN(sub, sub_op)
  BEMAN_INSIDE_ARITH_FN(mul, mul_op)
  BEMAN_INSIDE_ARITH_FN(div, div_op)
  BEMAN_INSIDE_ARITH_FN(mod, mod_op)
#undef BEMAN_INSIDE_ARITH_FN

  // Binary operators: +, -, * use the default policy; / and % carry the
  // operands' own policies (snap/rounding select the native integer paths).
  [[nodiscard]] constexpr auto operator+(insidable auto lhs, insidable auto rhs)
  { return beman::inside::add(lhs, rhs); }

  [[nodiscard]] constexpr auto operator-(insidable auto lhs, insidable auto rhs)
  { return beman::inside::sub(lhs, rhs); }

  [[nodiscard]] constexpr auto operator*(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mul(lhs, rhs); }

  [[nodiscard]] constexpr auto operator/(insidable auto lhs, insidable auto rhs)
  { return beman::inside::div(lhs, rhs, make_policy<policy_of<decltype(lhs)> | policy_of<decltype(rhs)>>()); }

  [[nodiscard]] constexpr auto operator%(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mod(lhs, rhs, make_policy<policy_of<decltype(lhs)> | policy_of<decltype(rhs)>>()); }

  //---------------------------------------------------------------------------
  // add_all / mul_all — variadic folds (pairwise widening, same as `a + b + c`
  // but reads cleaner; matches Chromium's `CheckAdd(a, b, c)`).
  //---------------------------------------------------------------------------
  template <insidable First, insidable... Rest>
  [[nodiscard]] constexpr auto add_all(First const& first, Rest const&... rest)
  { return (first + ... + rest); }

  template <insidable First, insidable... Rest>
  [[nodiscard]] constexpr auto mul_all(First const& first, Rest const&... rest)
  { return (first * ... * rest); }

  //---------------------------------------------------------------------------
  // sum<Target> — bulk reduction with ONE deferred range check. Per-element
  // `target += b` re-validates every step (blocks vectorization); this
  // accumulates raws in imax and applies Target's policy once to the total
  // (semantic difference: the *total* is validated, not every prefix). Fast
  // path: ≤32-bit integer raws, flushed to a rational every 2^30 elements so the
  // accumulator can't overflow; wider/rational/f64 take the per-element fold.
  //---------------------------------------------------------------------------
  template <insidable Target, std::ranges::input_range Rng>
    requires insidable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
  [[nodiscard]] constexpr Target sum(Rng&& r)
  {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    using detail::rational;
    rational total{0};

    if constexpr ((detail::value_raw<B> || detail::index_raw<B>)
                  && sizeof(detail::raw_t<B>) <= 4)
    {
      auto flush = [&](imax acc, imax cnt)
      {
        // value storage: raw IS the value. index: Σvalue = cnt·Lower + Σraw·Notch.
        rational part = [&]
        {
          if constexpr (detail::index_raw<B>)
            return ((rational{acc} * notch_of<B>).value()
                    + (rational{cnt} * lower_of<B>).value()).value();
          else
            return rational{acc};
        }();
        total = (total + part).value();
      };
      auto it  = std::ranges::begin(r);
      auto end = std::ranges::end(r);
      while (it != end)
      {
        // Branch-free inner block (≤ 2^30 elements keeps the imax accumulator
        // overflow-free) — the loop that vectorizes.
        imax acc = 0, cnt = 0;
        if constexpr (std::ranges::random_access_range<Rng>)
        {
          const imax block =
              std::min<imax>(end - it, imax{1} << 30);
          for (imax j = 0; j < block; ++j)
            acc += detail::raw_imax(it[j]);
          it += block;
          cnt = block;
        }
        else
        {
          for (; it != end && cnt < (imax{1} << 30); ++it, ++cnt)
            acc += detail::raw_imax(*it);
        }
        flush(acc, cnt);
      }
    }
    else
    {
      for (auto const& b : r)
        total = (total + detail::as_rational(b)).value();
    }
    return Target{total};
  }

  //---------------------------------------------------------------------------
  // dot / cross / lerp — 2-D inside-space vector helpers. Each widens its result
  // grid like the underlying `+`/`*`, so no overflow and the result is a plain
  // `inside`. (cross is the z-component, useful for "which side" tests.)
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto dot(insidable auto ax, insidable auto ay,
                                   insidable auto bx, insidable auto by)
  { return ax * bx + ay * by; }

  [[nodiscard]] constexpr auto cross(insidable auto ax, insidable auto ay,
                                     insidable auto bx, insidable auto by)
  { return ax * by - ay * bx; }

  // lerp(a, b, t) = a + (b - a) * t. `t` is itself an inside (typically a
  // [0, 1] fixed-point grid), so the interpolation never leaves inside-space.
  [[nodiscard]] constexpr auto lerp(insidable auto a, insidable auto b,
                                    insidable auto t)
  { return a + (b - a) * t; }

  //---------------------------------------------------------------------------
  // common_inside — the "hull" type able to hold every value of L and R exactly:
  // interval hull + notch gcd (grid `hull`), representation propagated by the
  // same widest-wins rule as arithmetic results (detail::fp_rep). Backs the
  // std::common_type specialisation (numeric_limits.hpp) and mixed-grid
  // min/max below. The primary has no `type` when the hull grid is
  // unrepresentable, so common_type_t SFINAEs away instead of erroring.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <insidable Lhs, insidable Rhs>
    struct common_inside {};

    // Same type stays itself (policy included) — mirrors std::common_type<T, T>.
    template <insidable Same>
    struct common_inside<Same, Same> { using type = Same; };

    template <insidable Lhs, insidable Rhs>
      requires (!std::same_as<Lhs, Rhs>) && (hull(grid_of<Lhs>, grid_of<Rhs>).has_value())
    struct common_inside<Lhs, Rhs>
    {
      static constexpr grid hull_grid = *hull(grid_of<Lhs>, grid_of<Rhs>);
      using type = inside<hull_grid,
                         fp_rep<Lhs, Rhs, hull_grid, /*AllowContinuous=*/true>::result_policy>;
    };
  }

  template <insidable Lhs, insidable Rhs>
  using common_inside_t = typename detail::common_inside<Lhs, Rhs>::type;

  //---------------------------------------------------------------------------
  // std-vocabulary helpers — ADL-found `min` / `max` / `midpoint` for generic
  // code. min/max mirror std; midpoint returns the *exact* average on a refined
  // grid (so, unlike std::midpoint, it neither rounds nor overflows). There is
  // no free `beman::inside::clamp` (the name is the policy flag — use clamp_cast<Target>).
  //---------------------------------------------------------------------------
  template <insidable T>
  [[nodiscard]] constexpr T min(T a, T b) { return (b < a) ? b : a; }

  template <insidable T>
  [[nodiscard]] constexpr T max(T a, T b) { return (a < b) ? b : a; }

  // Mixed-grid forms return the common hull type (both operands convert
  // losslessly — the hull is assignable from each by construction).
  template <insidable Lhs, insidable Rhs> requires (!std::same_as<Lhs, Rhs>)
  [[nodiscard]] constexpr auto min(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs>
  { common_inside_t<Lhs, Rhs> ca{a}, cb{b}; return (cb < ca) ? cb : ca; }

  template <insidable Lhs, insidable Rhs> requires (!std::same_as<Lhs, Rhs>)
  [[nodiscard]] constexpr auto max(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs>
  { common_inside_t<Lhs, Rhs> ca{a}, cb{b}; return (ca < cb) ? cb : ca; }

  template <insidable T>
  [[nodiscard]] constexpr auto midpoint(T a, T b) { return (a + b) * just<frac<1, 2>>; }

  // Mixed grids: the exact average on the refined sum grid, like the same-type form.
  template <insidable Lhs, insidable Rhs> requires (!std::same_as<Lhs, Rhs>)
  [[nodiscard]] constexpr auto midpoint(Lhs a, Rhs b) { return (a + b) * just<frac<1, 2>>; }

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
  namespace detail
  {
    template <class L, class R>
    concept expected_operands =
        (is_expected_v<L> || is_expected_v<R>)
        && (insidable<unwrap_t<L>> || insidable<unwrap_t<R>>);
  }

#define BEMAN_INSIDE_LIFT_OP(op)                                                     \
  template <class L, class R>                                                        \
    requires detail::expected_operands<L, R>                                         \
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l op r; }      \
  [[nodiscard]] constexpr auto operator op(L const& lhs, R const& rhs)               \
  { return detail::lift([](auto const& l, auto const& r) { return l op r; }, lhs, rhs); }

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
  // on a f64 call.
  //---------------------------------------------------------------------------
  template <typename A> concept raw_scalar = std::integral<A> || std::floating_point<A>;

#define BEMAN_INSIDE_SCALAR_MSG                                                      \
  "an inside cannot be combined with a raw scalar: give the scalar a grid — "        \
  "`1_ins`, `just<1>`, `one`, or `inside<{lo,hi}>{n}` for a runtime value with a "   \
  "known range"
#define BEMAN_INSIDE_NO_SCALAR(op)                                                   \
  template <insidable B, raw_scalar A> B operator op(B const&, A)                    \
  { static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); }            \
  template <raw_scalar A, insidable B> B operator op(A, B const&)                    \
  { static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); }            \
  template <insidable B, raw_scalar A> B& operator op##=(B&, A)                      \
  { static_assert(detail::dependent_false<B>, BEMAN_INSIDE_SCALAR_MSG); }

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
template <beman::inside::grid G1, beman::inside::policy_flag P1,
          beman::inside::grid G2, beman::inside::policy_flag P2>
struct std::common_type<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>>
  : beman::inside::detail::common_inside<beman::inside::inside<G1, P1>,
                                         beman::inside::inside<G2, P2>> {};


// ======================================================================
//  beman/inside/range.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// inside_range — random-access range over a grid. Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. Models random_access_range + sized_range (so std::ranges algorithms
// work directly). iterator_category is input_iterator_tag because operator*
// returns by value; iterator_concept carries the f64 random-access capability.
//---------------------------------------------------------------------------
namespace beman::inside
{
  namespace detail
  {
    // enumerate_view — C++20 stand-in for std::views::enumerate (C++23), yielding
    // pair<index, value> by value (all indexed() needs).
    template <class R>
    struct enumerate_view
    {
      R Base;

      struct iterator
      {
        std::ranges::iterator_t<const R> It{};
        std::size_t Index{0};

        using value_type      = std::pair<std::size_t, std::ranges::range_value_t<R>>;
        using difference_type  = std::ptrdiff_t;

        [[nodiscard]] constexpr value_type operator*() const { return {Index, *It}; }
        constexpr iterator& operator++() { ++It; ++Index; return *this; }
        constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
        [[nodiscard]] constexpr bool operator==(iterator const& o) const { return It == o.It; }
      };

      constexpr iterator begin() const { return {std::ranges::begin(Base), 0}; }
      constexpr iterator end()   const { return {std::ranges::end(Base), 0}; }
    };

    // stride_view — stand-in for std::views::stride (likewise). Visits every
    // `step`-th element; forward-only, and the advance checks `end` so a length
    // that isn't a multiple of the stride still terminates.
    template <class R>
    struct stride_view
    {
      R Base;
      std::size_t Step{1};

      struct iterator
      {
        std::ranges::iterator_t<const R> It{};
        std::ranges::iterator_t<const R> End{};
        std::size_t Step{1};

        using value_type      = std::ranges::range_value_t<R>;
        using difference_type = std::ptrdiff_t;

        [[nodiscard]] constexpr value_type operator*() const { return *It; }
        constexpr iterator& operator++()
        {
          for (std::size_t k = 0; k < Step && It != End; ++k) ++It;
          return *this;
        }
        constexpr iterator operator++(int) { auto t = *this; ++*this; return t; }
        [[nodiscard]] constexpr bool operator==(iterator const& o) const { return It == o.It; }
      };

      constexpr iterator begin() const
      { return {std::ranges::begin(Base), std::ranges::end(Base), Step}; }
      constexpr iterator end() const
      { return {std::ranges::end(Base), std::ranges::end(Base), Step}; }
    };
  } // namespace detail

  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0)
  struct inside_range
  {
    using value_type = inside<G, P>;
    static_assert(G.max_index_representable()
                  && detail::max_index_v<value_type> < std::numeric_limits<umax>::max(),
                  "inside_range: the grid has more points than a 64-bit count can hold");
    static constexpr umax slot_count = detail::max_index_v<value_type> + 1;

    struct iterator
    {
      using iterator_concept  = std::random_access_iterator_tag;
      using iterator_category = std::input_iterator_tag;
      using value_type        = inside<G, P>;
      using difference_type   = imax;

      umax Start {0};   // slot of the first element (the range wraps past the top)
      imax Pos   {0};   // position in [0, slot_count]; the loop variable

      constexpr iterator() = default;
      constexpr iterator(umax s, imax p) : Start{s}, Pos{p} {}

      // Grid slot of this position: Start + Pos, wrapped once (no overflow).
      constexpr umax slot() const
      {
        const umax p = static_cast<umax>(Pos);
        return p < slot_count - Start ? Start + p : p - (slot_count - Start);
      }

      [[nodiscard]] constexpr value_type operator*() const
      {
        // value = Lower + index * Notch (always exact: lies on the grid).
        // Integer-backed storages decode without the rational/assignment
        // engine: for index storage the iterator index IS the raw (it stays in
        // [0, max_index_v], which the raw type holds); integer-grid value
        // storage is a multiply-add in raw space. Rational/fp raws keep the exact generic path.
        if constexpr (detail::index_raw<value_type>)
          return value_type::from_raw(
              static_cast<typename value_type::raw_type>(slot()));
        else if constexpr (detail::value_raw<value_type>
                           && detail::abs_den(notch_of<value_type>.Denominator) == 1
                           && detail::abs_den(lower_of<value_type>.Denominator) == 1)
        {
          constexpr imax notch_step = static_cast<imax>(notch_of<value_type>.Numerator);
          return value_type::from_raw(static_cast<typename value_type::raw_type>(
              detail::lower_imax<value_type>
              + static_cast<imax>(slot()) * notch_step));
        }
        else
        {
          detail::rational val = (G.Interval.Lower
                          + (detail::rational{slot()} * G.Notch).value()).value();
          return value_type{val};
        }
      }

      [[nodiscard]] constexpr value_type operator[](difference_type n) const
      { return *(*this + n); }

      constexpr iterator& operator++() { ++Pos; return *this; }
      constexpr iterator  operator++(int) { auto t = *this; ++Pos; return t; }
      constexpr iterator& operator--() { --Pos; return *this; }
      constexpr iterator  operator--(int) { auto t = *this; --Pos; return t; }
      constexpr iterator& operator+=(difference_type n) { Pos += n; return *this; }
      constexpr iterator& operator-=(difference_type n) { Pos -= n; return *this; }

      [[nodiscard]] constexpr iterator operator+(difference_type n) const { auto t = *this; t += n; return t; }
      [[nodiscard]] constexpr iterator operator-(difference_type n) const { auto t = *this; t -= n; return t; }
      [[nodiscard]] friend constexpr iterator operator+(difference_type n, iterator it) { return it + n; }

      [[nodiscard]] constexpr difference_type operator-(iterator o) const { return Pos - o.Pos; }
      [[nodiscard]] constexpr bool operator==(iterator o) const { return Pos == o.Pos; }
      [[nodiscard]] constexpr auto operator<=>(iterator o) const { return Pos <=> o.Pos; }
    };

    umax StartIndex;

    constexpr inside_range() : StartIndex{0} {}

    constexpr inside_range(value_type start)
    {
      // Map a grid value back to its notch index: (start - Lower) / Notch.
      // Same storage split as iterator::operator* — index raw already is the
      // notch index; integer-grid value raw divides out the (integer) step.
      if constexpr (detail::index_raw<value_type>)
        StartIndex = static_cast<umax>(start.raw());
      else if constexpr (detail::value_raw<value_type>
                         && detail::abs_den(notch_of<value_type>.Denominator) == 1
                         && detail::abs_den(lower_of<value_type>.Denominator) == 1)
      {
        constexpr imax notch_step = static_cast<imax>(notch_of<value_type>.Numerator);
        StartIndex = static_cast<umax>(
            (static_cast<imax>(start.raw()) - detail::lower_imax<value_type>)
            / notch_step);
      }
      else
      {
        // The result has integer denominator (start is on the grid) so the
        // numerator is the index directly.
        auto offset = ((detail::as_rational(start) - G.Interval.Lower)
                       / G.Notch).value();
        StartIndex = offset.Numerator;
      }
    }

    constexpr iterator begin() const { return {StartIndex, 0}; }
    constexpr iterator end() const   { return {StartIndex, static_cast<imax>(slot_count)}; }

    constexpr std::size_t size() const { return slot_count; }

    // `indexed()` pairs each value with its zero-based position (≈ C++23
    // std::views::enumerate), via detail::enumerate_view for C++20.
    constexpr auto indexed() const { return detail::enumerate_view<inside_range>{*this}; }

    // `strided(step)` visits every `step`-th grid value (≈ C++23 std::views::
    // stride). `std::views::reverse` already works directly, so there's no reverse().
    constexpr auto strided(std::size_t step) const
    { return detail::stride_view<inside_range>{*this, step}; }
  };

} // namespace beman::inside



// ======================================================================
//  beman/inside/cmath.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  beman/inside/cmath_double.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// beman::inside::math double engine — a small, reproducible libm in `double`. Bit-identical
// on every IEEE-754 binary64 platform compiled without `-ffast-math`, via:
//   * NO <cmath> transcendentals — sin/cos/exp/log are fixed polynomials here;
//     only std::fma/sqrt/nearbyint (well-defined) and the constexpr ldexp.
//   * Horner evaluation with explicit std::fma (immune to FMA-contraction).
//   * Cody-Waite range reduction for full-precision args.
// The default engine; `BEMAN_INSIDE_MATH_CORDIC` selects the integer CORDIC engine instead.
//---------------------------------------------------------------------------


// BEMAN_INSIDE_MATH_NO_FP is resolved in policy_flag.hpp (included via inside.hpp).

#ifndef BEMAN_INSIDE_MATH_NO_FP   // ===== FP engine present (needs <cmath> + an FPU) =====

#include <cmath>            // std::fma, std::sqrt, std::nearbyint ONLY

// `BEMAN_INSIDE_FP_FN`: the engine cores become `constexpr` on C++26 toolchains with
// constexpr <cmath> (P1383). Inert otherwise — see BEMAN_INSIDE_MATH_FN in cmath.hpp.
#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
#  define BEMAN_INSIDE_FP_FN constexpr
#else
#  define BEMAN_INSIDE_FP_FN
#endif

namespace beman::inside::math::dbl::detail
{
  using std::fma;

  // c0·z^n + c1·z^(n-1) + … + cn as an fma chain from the highest coefficient
  // down (Horner) — the same operation order as writing the chain out by hand.
  template <std::floating_point T, typename... C>
  [[gnu::always_inline]] inline BEMAN_INSIDE_FP_FN T horner(T z, T c0, C... cs)
  {
    T p = c0;
    ((p = fma(p, z, static_cast<T>(cs))), ...);
    return p;
  }

  inline constexpr double kHalfPiHi = 0x1.921fb54442d18p+0;   // π/2  high
  inline constexpr double kHalfPiLo = 0x1.1a62633145c07p-54;  // π/2  low
  inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;  // 2/π
  inline constexpr double kLn2Hi    = 0x1.62e42fee00000p-1;   // ln2  high
  inline constexpr double kLn2Lo    = 0x1.a39ef35793c76p-33;  // ln2  low
  inline constexpr double kLog2e    = 0x1.71547652b82fep+0;   // 1/ln2

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²), P = Σ (−1)ᵏ zᵏ/(2k+1)! to z⁷ (r¹⁵).
  inline BEMAN_INSIDE_FP_FN double sin_poly(double r)
  {
    double z = r * r;
    double p = horner(z,
                      -1.0 / 1307674368000.0, 1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0,
                      -1.0 / 5040.0, 1.0 / 120.0, -1.0 / 6.0, 1.0);
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²), Q = Σ (−1)ᵏ zᵏ/(2k)! to z⁸ (r¹⁶).
  inline BEMAN_INSIDE_FP_FN double cos_poly(double r)
  {
    double z = r * r;
    return horner(z,
                  1.0 / 20922789888000.0, -1.0 / 87178291200.0, 1.0 / 479001600.0, -1.0 / 3628800.0,
                  1.0 / 40320.0, -1.0 / 720.0, 1.0 / 24.0, -1.0 / 2.0,
                  1.0);
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r¹².
  inline BEMAN_INSIDE_FP_FN double exp_poly(double r)
  {
    return horner(r,
                  1.0 / 479001600.0, 1.0 / 39916800.0, 1.0 / 3628800.0, 1.0 / 362880.0,
                  1.0 / 40320.0, 1.0 / 5040.0, 1.0 / 720.0, 1.0 / 120.0,
                  1.0 / 24.0, 1.0 / 6.0, 1.0 / 2.0, 1.0,
                  1.0);
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BEMAN_INSIDE_FP_FN double reduce_quadrant(double x, long& q)
  {
    double k = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r = fma(-k, kHalfPiLo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BEMAN_INSIDE_FP_FN double fp_sin(double x)
  {
    long q; double r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  inline BEMAN_INSIDE_FP_FN double fp_cos(double x)
  {
    long q; double r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return cos_poly(r);
      case 1:  return -sin_poly(r);
      case 2:  return -cos_poly(r);
      default: return sin_poly(r);
    }
  }

  // tan from one reduction: s/c in even quadrants, −c/s in odd ones. False on
  // a pole (odd quadrant with s == 0).
  inline BEMAN_INSIDE_FP_FN bool fp_tan(double x, double& t)
  {
    long q; double r = reduce_quadrant(x, q);
    const double s = sin_poly(r), c = cos_poly(r);
    if (q & 1)
    {
      if (s == 0.0) return false;
      t = -c / s;
    }
    else
      t = s / c;
    return true;
  }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BEMAN_INSIDE_FP_FN double fp_exp(double x)
  {
    double k = std::nearbyint(x * kLog2e);
    double r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return beman::inside::detail::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BEMAN_INSIDE_FP_FN double fp_sqrt(double x) { return std::sqrt(x); }   // correctly rounded

  inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1; // √½

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln(x) = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1) ∈ [−0.18,0.18] (atanh series converges fast). Pre: x > 0.
  inline BEMAN_INSIDE_FP_FN double fp_log(double x)
  {
    int e;
    double m = beman::inside::detail::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    double f  = (m - 1.0) / (m + 1.0);
    double f2 = f * f;
    double p = horner(f2,
                      1.0 / 17.0, 1.0 / 15.0, 1.0 / 13.0, 1.0 / 11.0,
                      1.0 / 9.0, 1.0 / 7.0, 1.0 / 5.0, 1.0 / 3.0,
                      1.0);
    double logm = 2.0 * f * p;
    double r = fma(static_cast<double>(e), kLn2Hi, logm);
    return fma(static_cast<double>(e), kLn2Lo, r);
  }

  inline constexpr double kLn2Full  = 0x1.62e42fefa39efp-1;  // ln2
  inline constexpr double kLog10e   = 0x1.bcb7b1526e50ep-2;  // 1/ln10

  // Compositions on the validated primitives.
  inline BEMAN_INSIDE_FP_FN double fp_exp2(double x)  { return fp_exp(x * kLn2Full); }
  inline BEMAN_INSIDE_FP_FN double fp_log2(double x)  { return fp_log(x) * kLog2e; }
  inline BEMAN_INSIDE_FP_FN double fp_log10(double x) { return fp_log(x) * kLog10e; }
  inline BEMAN_INSIDE_FP_FN double fp_pow(double b, double e) { return fp_exp(e * fp_log(b)); }
  inline BEMAN_INSIDE_FP_FN double fp_cbrt(double x)
  {
    if (x == 0.0) return 0.0;
    double m = fp_exp(fp_log(x < 0 ? -x : x) * (1.0 / 3.0));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN double fp_sinh(double x) { double e = fp_exp(x); return (e - 1.0 / e) * 0.5; }
  inline BEMAN_INSIDE_FP_FN double fp_cosh(double x) { double e = fp_exp(x); return (e + 1.0 / e) * 0.5; }
  // Inverse hyperbolics from fp_log / fp_sqrt, on |x| (odd functions) so every
  // log argument is ≥ 1; |x| > 1 and acosh use ln a + ln(1 + √(1 ∓ 1/a²)).
  inline BEMAN_INSIDE_FP_FN double fp_asinh(double x)
  {
    const double a = x < 0 ? -x : x;
    const double m = a <= 1.0 ? fp_log(a + fp_sqrt(a * a + 1.0))
                            : fp_log(a) + fp_log(1.0 + fp_sqrt(1.0 + 1.0 / (a * a)));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN double fp_acosh(double x)
  { return fp_log(x) + fp_log(1.0 + fp_sqrt(1.0 - 1.0 / (x * x))); }
  inline BEMAN_INSIDE_FP_FN double fp_atanh(double x)
  {
    const double a = x < 0 ? -x : x;
    const double m = 0.5 * fp_log((1.0 + a) / (1.0 - a));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN double fp_tanh(double x)
  {
    double e = fp_exp(x + x);            // e^{2x}
    return (e - 1.0) / (e + 1.0);
  }
  // √(x²+y²). The public domain caps |x|,|y| ≤ 2^20, so x²+y² ≤ 2^41 — no
  // overflow, no scaling needed; the correctly-rounded √ keeps it accurate.
  inline BEMAN_INSIDE_FP_FN double fp_hypot(double x, double y) { return fp_sqrt(x * x + y * y); }

  inline constexpr double kPi      = 0x1.921fb54442d18p+1;   // π
  inline constexpr double kPiHalf  = 0x1.921fb54442d18p+0;   // π/2
  inline constexpr double kPiSixth = 0x1.0c152382d7366p-1;   // π/6
  inline constexpr double kInvSqrt3 = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
  inline constexpr double kTanPi12 = 0x1.126145e9ecd56p-2;   // tan(π/12) ≈ 0.2679

  // atan(x). Reduce |x|>1 via reciprocal (π/2 − atan(1/x)); then |a|>tan(π/12)
  // via the π/6 addition formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BEMAN_INSIDE_FP_FN double fp_atan(double x)
  {
    bool neg = x < 0; double a = neg ? -x : x;
    bool inv = a > 1.0; if (inv) a = 1.0 / a;
    double off = 0.0;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0); off = kPiSixth; }
    double z = a * a;
    double p = horner(z,
                      -1.0 / 23.0, 1.0 / 21.0, -1.0 / 19.0, 1.0 / 17.0,
                      -1.0 / 15.0, 1.0 / 13.0, -1.0 / 11.0, 1.0 / 9.0,
                      -1.0 / 7.0, 1.0 / 5.0, -1.0 / 3.0, 1.0);
    double r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BEMAN_INSIDE_FP_FN double fp_atan2(double y, double x)
  {
    if (x > 0.0) return fp_atan(y / x);
    if (x < 0.0) return fp_atan(y / x) + (y >= 0.0 ? kPi : -kPi);
    if (y > 0.0) return kPiHalf;
    if (y < 0.0) return -kPiHalf;
    return 0.0;
  }

  inline BEMAN_INSIDE_FP_FN double fp_asin(double x) { return fp_atan(x / fp_sqrt((1.0 - x) * (1.0 + x))); }
  inline BEMAN_INSIDE_FP_FN double fp_acos(double x) { return kPiHalf - fp_asin(x); }
} // namespace beman::inside::math::dbl::detail

namespace beman::inside::math::dbl::detail
{
  // Engine cores: `f64` (double-backed) inside in → `double` math → inside out.
  // The inside I/O is a plain double read/store (operator double / Out{double}),
  // so the cost is the polynomial itself. These plug into the shared public
  // surface as `fn_core` under the default build.
  template <typename Out>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out store(double d)
  {
    // The double assignment applies Out's whole policy: it rounds onto the
    // grid, clamps / wraps / reports an out-of-range or non-finite result. (A
    // rational conversion first would fail outside that policy for a result
    // of 2^64 or more, or a non-finite one.) An fp-backed Out (f64 or f32)
    // stores the value as its raw — lossless on its fp-exact grid.
    return Out{d};
  }

  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sin_core(In x)  { return store<Out>(detail::fp_sin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cos_core(In x)  { return store<Out>(detail::fp_cos(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out exp_core(In x)  { return store<Out>(detail::fp_exp(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sqrt_core(In x) { return store<Out>(detail::fp_sqrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log_core(In x)  { return store<Out>(detail::fp_log(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out exp2_core(In x) { return store<Out>(detail::fp_exp2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log2_core(In x) { return store<Out>(detail::fp_log2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log10_core(In x){ return store<Out>(detail::fp_log10(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cbrt_core(In x) { return store<Out>(detail::fp_cbrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sinh_core(In x) { return store<Out>(detail::fp_sinh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cosh_core(In x) { return store<Out>(detail::fp_cosh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out tanh_core(In x) { return store<Out>(detail::fp_tanh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out asinh_core(In x) { return store<Out>(detail::fp_asinh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out acosh_core(In x) { return store<Out>(detail::fp_acosh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atanh_core(In x) { return store<Out>(detail::fp_atanh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan_core(In x) { return store<Out>(detail::fp_atan(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out asin_core(In x) { return store<Out>(detail::fp_asin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out acos_core(In x) { return store<Out>(detail::fp_acos(static_cast<double>(x))); }
  template <typename Out, typename InY, typename InX>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan2_core(InY y, InX x)
  { return store<Out>(detail::fp_atan2(static_cast<double>(y), static_cast<double>(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::fp_hypot(static_cast<double>(x), static_cast<double>(y))); }
} // namespace beman::inside::math::dbl

#endif // !BEMAN_INSIDE_MATH_NO_FP


// ======================================================================
//  beman/inside/cmath_float.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// beman::inside::math float engine — a small, reproducible libm in `float` (binary32).
// Bit-identical on every IEEE-754 binary32 platform compiled without
// `-ffast-math`, via the same recipe as the double engine but in single
// precision, for single-precision-only FPUs (Cortex-M4F etc.) and size/speed:
//   * NO <cmath> transcendentals — own fixed polynomials evaluated with the
//     correctly-rounded std::fma(float) (immune to FMA-contraction).
//   * Cody-Waite range reduction whose hi/lo split constants are derived at
//     compile time (mask the low mantissa bits of the float-rounded constant),
//     no external codegen — honoring the bit-exact contract.
//   * Correctly-rounded std::sqrt(float).
// Float is a THIRD value set: float ≠ double ≠ cordic, each ≤ a few notches of
// truth where the grid permits (table-maker's dilemma — see determinism.md).
// Present only when FP is available; compiled out under BEMAN_INSIDE_MATH_NO_FP.
//---------------------------------------------------------------------------


#ifndef BEMAN_INSIDE_MATH_NO_FP

#include <bit>     // std::bit_cast (constexpr Cody-Waite split derivation)
#include <cmath>   // std::fma, std::sqrt, std::nearbyint, std::ldexp, std::frexp (float)

namespace beman::inside::math::flt::detail
{
  using std::fma;
  using beman::inside::math::dbl::detail::horner;

  // Constexpr Cody-Waite split of a high-precision (double) reference into a
  // float `hi` with `keep` significant mantissa bits (low bits zeroed, so k·hi
  // stays exact for modest k) plus a float `lo` carrying the remainder. Pure
  // bit manipulation — identical on every IEEE-754 target.
  inline constexpr float split_hi(double full, int keep) noexcept
  {
    float f = static_cast<float>(full);
    std::uint32_t b = std::bit_cast<std::uint32_t>(f);
    b &= ~((std::uint32_t{1} << (23 - keep)) - 1);
    return std::bit_cast<float>(b);
  }
  inline constexpr float split_lo(double full, int keep) noexcept
  {
    return static_cast<float>(full - static_cast<double>(split_hi(full, keep)));
  }

  // High-precision references (double literals; only their float projections are
  // used at runtime). 12 kept bits hold accuracy across the shared ±2^20 domain.
  inline constexpr double kPiD    = 3.14159265358979323846;
  inline constexpr double kPiO2D  = kPiD / 2;
  inline constexpr double kLn2D   = 0.69314718055994530942;

  inline constexpr float kPio2Hi    = split_hi(kPiO2D, 12);
  inline constexpr float kPio2Lo    = split_lo(kPiO2D, 12);
  inline constexpr float kTwoOverPi  = static_cast<float>(2.0 / kPiD);
  inline constexpr float kLn2Hi     = split_hi(kLn2D, 12);
  inline constexpr float kLn2Lo     = split_lo(kLn2D, 12);
  inline constexpr float kLn2Full   = static_cast<float>(kLn2D);
  inline constexpr float kLog2e     = static_cast<float>(1.44269504088896340736);
  inline constexpr float kLog10e    = static_cast<float>(0.43429448190325182765);
  inline constexpr float kSqrtHalf  = 0x1.6a09e6p-1f;             // √½
  inline constexpr float kPi        = static_cast<float>(kPiD);
  inline constexpr float kPiHalf    = static_cast<float>(kPiO2D);
  inline constexpr float kPiSixth   = static_cast<float>(kPiD / 6);
  inline constexpr float kInvSqrt3  = 0x1.279a74p-1f;             // 1/√3 = tan(π/6)
  inline constexpr float kTanPi12   = 0x1.126146p-2f;             // tan(π/12)

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²) to r¹¹ (float-sufficient).
  inline BEMAN_INSIDE_FP_FN float sin_poly(float r)
  {
    float z = r * r;
    float p = horner(z,
                     -1.0f / 39916800.0f, 1.0f / 362880.0f, -1.0f / 5040.0f, 1.0f / 120.0f,
                     -1.0f / 6.0f, 1.0f);
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²) to r¹⁰.
  inline BEMAN_INSIDE_FP_FN float cos_poly(float r)
  {
    float z = r * r;
    return horner(z,
                  -1.0f / 3628800.0f, 1.0f / 40320.0f, -1.0f / 720.0f, 1.0f / 24.0f,
                  -1.0f / 2.0f, 1.0f);
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r⁷.
  inline BEMAN_INSIDE_FP_FN float exp_poly(float r)
  {
    return horner(r,
                  1.0f / 5040.0f, 1.0f / 720.0f, 1.0f / 120.0f, 1.0f / 24.0f,
                  1.0f / 6.0f, 1.0f / 2.0f, 1.0f, 1.0f);
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BEMAN_INSIDE_FP_FN float reduce_quadrant(float x, long& q)
  {
    float k = std::nearbyint(x * kTwoOverPi);
    float r = fma(-k, kPio2Hi, x);
    r = fma(-k, kPio2Lo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BEMAN_INSIDE_FP_FN float fp_sin(float x)
  {
    long q; float r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  // cos via quadrant reduction (NOT sin(x+π/2): shifting the float input would
  // lose bits for large x — the double engine can afford that, float cannot).
  inline BEMAN_INSIDE_FP_FN float fp_cos(float x)
  {
    long q; float r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return cos_poly(r);
      case 1:  return -sin_poly(r);
      case 2:  return -cos_poly(r);
      default: return sin_poly(r);
    }
  }

  // tan from one reduction: s/c in even quadrants, −c/s in odd ones. False on
  // a pole (odd quadrant with s == 0).
  inline BEMAN_INSIDE_FP_FN bool fp_tan(float x, float& t)
  {
    long q; float r = reduce_quadrant(x, q);
    const float s = sin_poly(r), c = cos_poly(r);
    if (q & 1)
    {
      if (s == 0.0f) return false;
      t = -c / s;
    }
    else
      t = s / c;
    return true;
  }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BEMAN_INSIDE_FP_FN float fp_exp(float x)
  {
    float k = std::nearbyint(x * kLog2e);
    float r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return std::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BEMAN_INSIDE_FP_FN float fp_sqrt(float x) { return std::sqrt(x); }   // correctly rounded

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1). Pre: x > 0.
  inline BEMAN_INSIDE_FP_FN float fp_log(float x)
  {
    int e;
    float m = std::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    float f  = (m - 1.0f) / (m + 1.0f);
    float f2 = f * f;
    float p = horner(f2, 1.0f / 9.0f, 1.0f / 7.0f, 1.0f / 5.0f, 1.0f / 3.0f, 1.0f);
    float logm = 2.0f * f * p;
    float r = fma(static_cast<float>(e), kLn2Hi, logm);
    return fma(static_cast<float>(e), kLn2Lo, r);
  }

  // Compositions on the validated primitives (same shapes as the double engine).
  inline BEMAN_INSIDE_FP_FN float fp_exp2(float x)  { return fp_exp(x * kLn2Full); }
  inline BEMAN_INSIDE_FP_FN float fp_log2(float x)  { return fp_log(x) * kLog2e; }
  inline BEMAN_INSIDE_FP_FN float fp_log10(float x) { return fp_log(x) * kLog10e; }
  inline BEMAN_INSIDE_FP_FN float fp_pow(float b, float e) { return fp_exp(e * fp_log(b)); }
  inline BEMAN_INSIDE_FP_FN float fp_cbrt(float x)
  {
    if (x == 0.0f) return 0.0f;
    float m = fp_exp(fp_log(x < 0 ? -x : x) * (1.0f / 3.0f));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN float fp_sinh(float x) { float e = fp_exp(x); return (e - 1.0f / e) * 0.5f; }
  inline BEMAN_INSIDE_FP_FN float fp_cosh(float x) { float e = fp_exp(x); return (e + 1.0f / e) * 0.5f; }
  // Inverse hyperbolics from fp_log / fp_sqrt, on |x| (odd functions) so every
  // log argument is ≥ 1; |x| > 1 and acosh use ln a + ln(1 + √(1 ∓ 1/a²)).
  inline BEMAN_INSIDE_FP_FN float fp_asinh(float x)
  {
    const float a = x < 0 ? -x : x;
    const float m = a <= 1.0f ? fp_log(a + fp_sqrt(a * a + 1.0f))
                            : fp_log(a) + fp_log(1.0f + fp_sqrt(1.0f + 1.0f / (a * a)));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN float fp_acosh(float x)
  { return fp_log(x) + fp_log(1.0f + fp_sqrt(1.0f - 1.0f / (x * x))); }
  inline BEMAN_INSIDE_FP_FN float fp_atanh(float x)
  {
    const float a = x < 0 ? -x : x;
    const float m = 0.5f * fp_log((1.0f + a) / (1.0f - a));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_FP_FN float fp_tanh(float x)
  {
    float e = fp_exp(x + x);             // e^{2x}
    return (e - 1.0f) / (e + 1.0f);
  }
  inline BEMAN_INSIDE_FP_FN float fp_hypot(float x, float y) { return fp_sqrt(x * x + y * y); }

  // atan(x): reduce |x|>1 via reciprocal; |a|>tan(π/12) via the π/6 addition
  // formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BEMAN_INSIDE_FP_FN float fp_atan(float x)
  {
    bool neg = x < 0; float a = neg ? -x : x;
    bool inv = a > 1.0f; if (inv) a = 1.0f / a;
    float off = 0.0f;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0f); off = kPiSixth; }
    float z = a * a;
    float p = horner(z,
                     1.0f / 13.0f, -1.0f / 11.0f, 1.0f / 9.0f, -1.0f / 7.0f,
                     1.0f / 5.0f, -1.0f / 3.0f, 1.0f);
    float r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BEMAN_INSIDE_FP_FN float fp_atan2(float y, float x)
  {
    if (x > 0.0f) return fp_atan(y / x);
    if (x < 0.0f) return fp_atan(y / x) + (y >= 0.0f ? kPi : -kPi);
    if (y > 0.0f) return kPiHalf;
    if (y < 0.0f) return -kPiHalf;
    return 0.0f;
  }

  inline BEMAN_INSIDE_FP_FN float fp_asin(float x) { return fp_atan(x / fp_sqrt((1.0f - x) * (1.0f + x))); }
  inline BEMAN_INSIDE_FP_FN float fp_acos(float x) { return kPiHalf - fp_asin(x); }
} // namespace beman::inside::math::flt::detail

namespace beman::inside::math::flt::detail
{
  // Engine cores: inside in → `float` math → inside out. Storing the float result:
  // an fp-backed Out (f32 OR f64) stores the value directly via its float/double
  // raw (the natural pairing for `flt` is `f32` — no rational, no double round-
  // trip on the result); any other snap grid assigns through the rational path,
  // snapping via Out's round policy.
  template <typename Out>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out store(float f)
  {
    // Through the double assignment, like the double engine's store: Out's
    // policy handles rounding, range and non-finite results.
    return Out{static_cast<double>(f)};
  }

  // Read an input inside as `float`. An f32-backed operand IS a binary32 raw, so
  // read it directly — no double hop (keeps the whole flt+f32 path in hardware
  // float on a single-precision FPU). Any other storage decodes via double then
  // narrows; float→double→float round-trips to the same float, so this is a pure
  // optimization with no value change.
  template <typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN float to_float(In x)
  {
    if constexpr (beman::inside::detail::f32_raw<In>) return x.raw();
    else                                    return static_cast<float>(static_cast<double>(x));
  }

  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sin_core(In x)  { return store<Out>(detail::fp_sin(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cos_core(In x)  { return store<Out>(detail::fp_cos(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out exp_core(In x)  { return store<Out>(detail::fp_exp(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sqrt_core(In x) { return store<Out>(detail::fp_sqrt(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log_core(In x)  { return store<Out>(detail::fp_log(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out exp2_core(In x) { return store<Out>(detail::fp_exp2(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log2_core(In x) { return store<Out>(detail::fp_log2(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out log10_core(In x){ return store<Out>(detail::fp_log10(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cbrt_core(In x) { return store<Out>(detail::fp_cbrt(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out sinh_core(In x) { return store<Out>(detail::fp_sinh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out cosh_core(In x) { return store<Out>(detail::fp_cosh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out tanh_core(In x) { return store<Out>(detail::fp_tanh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out asinh_core(In x) { return store<Out>(detail::fp_asinh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out acosh_core(In x) { return store<Out>(detail::fp_acosh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atanh_core(In x) { return store<Out>(detail::fp_atanh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan_core(In x) { return store<Out>(detail::fp_atan(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out asin_core(In x) { return store<Out>(detail::fp_asin(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out acos_core(In x) { return store<Out>(detail::fp_acos(to_float(x))); }
  template <typename Out, typename InY, typename InX>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan2_core(InY y, InX x)
  { return store<Out>(detail::fp_atan2(to_float(y), to_float(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BEMAN_INSIDE_FP_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::fp_hypot(to_float(x), to_float(y))); }
} // namespace beman::inside::math::flt

#endif // !BEMAN_INSIDE_MATH_NO_FP




// BEMAN_INSIDE_MATH_FN: the integer/CORDIC engine (selected by BEMAN_INSIDE_MATH_NO_FP,
// resolved in cmath_double.hpp and implied by BEMAN_INSIDE_MATH_CORDIC) is always
// `constexpr`. The FP engines become `constexpr` only on a C++26 toolchain with
// constexpr <cmath> (P1383; __cpp_lib_constexpr_cmath) — that branch is inert and
// untested until such a toolchain exists. Decision 2026-06-12: no compile-time
// softfloat emulation — wait for the standard.
#if defined(BEMAN_INSIDE_MATH_NO_FP) \
    || (defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L)
#  define BEMAN_INSIDE_MATH_FN constexpr
#else
#  define BEMAN_INSIDE_MATH_FN
#endif

//---------------------------------------------------------------------------
// beman::inside::math — one transcendental API, three interchangeable engines selected by
// the `BEMAN_INSIDE_MATH_CORDIC` / `BEMAN_INSIDE_MATH_FLOAT` macros. All are feature-equivalent (same functions,
// signatures, domains):
//
//   * DEFAULT — double engine (`cmath_double.hpp`): hardware `double`
//     polynomials on `f64` insides. Bit-identical on any IEEE-754 binary64
//     platform built without `-ffast-math`. Fast (~ns); needs an FPU; runtime.
//   * `BEMAN_INSIDE_MATH_CORDIC` — integer/CORDIC engine (this file): FPU-free, constexpr,
//     UNCONDITIONALLY bit-identical (any platform/flags). For embedded/portability.
//   * `BEMAN_INSIDE_MATH_FLOAT` — float (binary32) engine (`cmath_float.hpp`): like the
//     double engine but single precision, for single-precision-only FPUs.
//
// The macro picks only which engine the UNQUALIFIED `beman::inside::math::fn` uses; all
// engines are always reachable by namespace (`cordic::`/`dbl::`/`flt::`). Default
// selection (the dispatch below): `BEMAN_INSIDE_MATH_NO_FP`→cordic, else
// `BEMAN_INSIDE_MATH_FLOAT`→flt, else dbl.
//
// `BEMAN_INSIDE_MATH_NO_FP` (implied by `BEMAN_INSIDE_MATH_CORDIC`, auto-enabled when
// `__STDC_HOSTED__ == 0`) compiles the double AND float engines and their
// `<cmath>` out entirely, leaving the integer engine — so the library, including
// the single header, builds with no hardware floating point.
//
// =====================================================================
//   INTEGER (CORDIC) ENGINE — BIT-EXACT REPRODUCIBILITY CONTRACT
// =====================================================================
// Every function below produces bit-identical output for the same input across
// compiler, platform, optimisation level, and FP flags — relied on for fuzzing
// corpora, record-and-replay, deterministic simulation, regression testing.
// (The double engine's contract lives atop cmath_double.hpp.) Requirements:
//   1. NO `<cmath>`/FPU/intrinsics; hot paths are integer-only over int64.
//   2. NO runtime-derived tables — coefficients derived at compile time from
//      `rational` literals, quantized to integer Q-format via constexpr rounding.
//   3. NO external code generators; derivation is constexpr C++.
//   4. C++20+ (well-defined signed right-shift semantics).
//   5. Each transcendental ships checked-in `static_assert` vectors pinning its
//      bit-exact output.
//
// Pattern per function: pick a working scale 2^W from the output grid
// (`working_bits`), range-reduce with integer ops, evaluate via the shared
// shift-add CORDIC (or Newton for sqrt/cbrt) using the portable wide `fmul`,
// then quantize onto `Out`'s grid through its assignment policy.
//---------------------------------------------------------------------------
namespace beman::inside::math
{
  using beman::inside::detail::rational;

  namespace detail
  {
    using namespace beman::inside::detail;

    // Exact rational source for the irrational constants — the fixed-point cores
    // need the exact form; bit-identical across platforms.
    inline constexpr rational kPiRat{1068966896, 340262731};
    inline constexpr rational kTwoPiRat = 2 * kPiRat;

    // Policy of an auto-deduced output: the input's, minus any fixed-width
    // storage flag (i8 … u64) — the output range differs, as for arithmetic.
    template <insidable In>
    inline constexpr policy_flag out_policy = policy_of<In> & ~raw_width_mask;

    // Output notch for a two-input function: the gcd of both input notches (0
    // if either is continuous), so swapping or mixing input types is symmetric.
    template <insidable A, insidable B>
    inline constexpr rational gcd_notch =
        (notch_of<A> == 0 || notch_of<B> == 0) ? rational{0} : *gcd(notch_of<A>, notch_of<B>);

    // Input-domain checks, one per function, shared by every engine (cordic,
    // dbl, flt) so all three accept exactly the same input grids. The limits are
    // the CORDIC working-scale envelope, which also computes every engine's
    // auto-deduced output grid.
    template <insidable In>
    constexpr void domain_acos() noexcept
    { static_assert(lower_of<In> >= -1 && upper_of<In> <= 1, "beman::inside::math::acos: input must be in [-1, 1]"); }
    template <insidable In>
    constexpr void domain_asin() noexcept
    { static_assert(lower_of<In> >= -1 && upper_of<In> <= 1, "beman::inside::math::asin: input must be in [-1, 1]"); }
    template <insidable In>
    constexpr void domain_atan() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::atan: input magnitudes must be \u2264 2^20 for the working-scale envelope"); }
    template <insidable In>
    constexpr void domain_atan2() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::atan2: input magnitudes must be \u2264 2^20 for the working-scale envelope"); }
    template <insidable In>
    constexpr void domain_cbrt() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::cbrt: input magnitude must be ≤ 2^20 for the working-scale envelope"); }
    template <insidable In>
    constexpr void domain_cos() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::cos: input magnitudes must be \u2264 2^20 rad"); }
    template <insidable In>
    constexpr void domain_cosh() noexcept
    { static_assert(lower_of<In> >= -10 && upper_of<In> <= 10, "beman::inside::math::cosh: input must be in [-10, 10]"); }
    template <insidable In>
    constexpr void domain_exp() noexcept
    { static_assert(lower_of<In> >= -20 && upper_of<In> <= 20, "beman::inside::math::exp: input must be in [-20, 20]"); }
    template <insidable In>
    constexpr void domain_exp2() noexcept
    { static_assert(lower_of<In> >= -30 && upper_of<In> <= 30, "beman::inside::math::exp2: input must be in [-30, 30]"); }
    template <insidable In>
    constexpr void domain_log() noexcept
    { static_assert(lower_of<In> > 0, "beman::inside::math::log: input must be strictly positive"); }
    template <insidable In>
    constexpr void domain_log10() noexcept
    { static_assert(lower_of<In> > 0, "beman::inside::math::log10: input must be strictly positive"); }
    template <insidable In>
    constexpr void domain_log2() noexcept
    { static_assert(lower_of<In> > 0, "beman::inside::math::log2: input must be strictly positive"); }
    template <insidable In>
    constexpr void domain_sin() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::sin: input magnitudes must be \u2264 2^20 rad"); }
    template <insidable In>
    constexpr void domain_sinh() noexcept
    { static_assert(lower_of<In> >= -10 && upper_of<In> <= 10, "beman::inside::math::sinh: input must be in [-10, 10]"); }
    template <insidable In>
    constexpr void domain_tan() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::tan: input magnitudes must be \u2264 2^20 rad"); }
    template <insidable In>
    constexpr void domain_asinh() noexcept
    { static_assert(lower_of<In> >= -(imax{1} << 20) && upper_of<In> <= (imax{1} << 20), "beman::inside::math::asinh: input magnitudes must be \u2264 2^20 for the working-scale envelope"); }
    template <insidable In>
    constexpr void domain_acosh() noexcept
    { static_assert(lower_of<In> >= 1 && upper_of<In> <= (imax{1} << 20), "beman::inside::math::acosh: input must be in [1, 2^20]"); }
    // atanh(±1) is infinite; the 2^-30 margin keeps (1+|x|)/(1−|x|) inside the
    // working scale.
    template <insidable In>
    constexpr void domain_atanh() noexcept
    { static_assert(lower_of<In> >= -1 + rational{1, imax{1} << 30} && upper_of<In> <= 1 - rational{1, imax{1} << 30}, "beman::inside::math::atanh: input must be in (-1, 1), at least 2^-30 from \u00b11"); }
    template <insidable In>
    constexpr void domain_tanh() noexcept
    { static_assert(lower_of<In> >= -10 && upper_of<In> <= 10, "beman::inside::math::tanh: input must be in [-10, 10]"); }
    template <insidable InX, insidable InY>
    constexpr void domain_hypot() noexcept
    { static_assert(lower_of<InX> >= -(imax{1} << 20) && upper_of<InX> <= (imax{1} << 20)
               && lower_of<InY> >= -(imax{1} << 20) && upper_of<InY> <= (imax{1} << 20), "beman::inside::math::hypot: input magnitudes must be ≤ 2^20 for the working-scale envelope"); }

    // pow_base<Base>(x) stays inside the 2^±30 envelope pow uses (the CORDIC
    // working scale): Base^x ≤ 2^30 for x up to Upper and ≥ 2^-30 down to
    // Lower. Checked with integer powers at the rounded-out exponents.
    consteval bool pow_within_2_30(imax base, rational x) noexcept
    {
      if (x <= 0) return true;
      const imax e = ceil(x);
      umax p = 1;
      for (imax k = 0; k < e; ++k)
      {
        if (p > (umax{1} << 30) / static_cast<umax>(base)) return false;
        p *= static_cast<umax>(base);
      }
      return true;
    }
    template <imax Base, insidable In>
    inline constexpr bool pow_base_domain_ok =
        Base >= 2 && pow_within_2_30(Base, upper_of<In>) && pow_within_2_30(Base, -lower_of<In>);

    template <imax Base, insidable In>
    constexpr void domain_pow_base() noexcept
    { static_assert(pow_base_domain_ok<Base, In>, "beman::inside::math::pow_base: Base must be ≥ 2 and Base^x must stay within [2^-30, 2^30] over the input interval"); }

    // pow_base_into<Out> outside the 2^±30 envelope: a clamp Out saturates,
    // anything else reports errc::overflow through Out's policy (pow_into's rule).
    template <insidable Out>
    constexpr Out pow_envelope_fail(bool high)
    {
      if constexpr (has_flag(policy_of<Out>, clamp))
        return Out{high ? upper_of<Out> : lower_of<Out>};
      else
      {
        make_policy<policy_of<Out>>().report(errc::overflow);
        return Out{lower_of<Out>};       // reached only if the handler returns
      }
    }

    // Out's interval endpoints as F (via double, like the runtime value), folded
    // at compile time for the FP engines' range checks.
    template <typename F, insidable Out>
    inline constexpr F lower_fp = static_cast<F>(static_cast<double>(lower_of<Out>));
    template <typename F, insidable Out>
    inline constexpr F upper_fp = static_cast<F>(static_cast<double>(upper_of<Out>));

    // Every transcendental operand must permit rounding (the `snap` bit, carried by
    // `f64`/`f32` and every `round_*` mode): the result is rounded onto the output
    // grid, which inherits the operand's policy. Pure grid ops (abs/floor/ceil/
    // round/trunc/fmod) round nothing and don't require it.
    template <insidable In>
    consteval bool require_snap() noexcept
    {
      static_assert(has_flag(policy_of<In>, snap),
          "beman::inside::math: a transcendental result is rounded onto the grid — its "
          "operand must permit rounding. Declare it with `round_nearest` (or "
          "`snap` / a `round_*` mode / `f64`).");
      return true;
    }
  }

  // Public irrational constants as point insides, so they compose directly in
  // inside-space (`angle * math::pi`) with no rational on the surface.
  inline constexpr auto pi     = just<detail::kPiRat>;
  inline constexpr auto two_pi = just<detail::kTwoPiRat>;

  namespace detail
  {
    using namespace beman::inside::detail;

    // Internal turn-phase shape: Q.N turns, period implicit in the unsigned raw's
    // modular wrap. Public sin/cos/tan take radians and route through this shape;
    // callers don't construct it directly (see examples/oscillator.cpp).
    template <int N>
    using turns_t = inside<{0, rational{(imax{1} << N) - 1, imax{1} << N},
                           per<(imax{1} << N)>}>;


    // log2(d) for a power-of-2 imax d. Constexpr loop; cheap at compile time.
    constexpr int log2_pow2(imax d) noexcept
    {
      int n = 0;
      while (d > 1) { d >>= 1; ++n; }
      return n;
    }

    // Extract N from a turns-shaped input inside (notch denominator is 2^N).
    // Alias templates can't be reverse-deduced, so sin/cos take a `insidable In`
    // and derive N from its grid.
    template <insidable In>
    inline constexpr int turn_bits = []{
      static_assert(lower_of<In> == 0,
                    "beman::inside::math: turn-phase input must have Lower == 0");
      static_assert(notch_of<In>.Numerator == 1,
                    "beman::inside::math: turn-phase input must have notch 1/2^N");
      return log2_pow2(abs_den(notch_of<In>.Denominator));
    }();

    // Forward declarations of the CORDIC engine pieces the turn-input workers
    // rely on (the engine is defined below, after the radians sin/cos).
    template <insidable Out> constexpr int working_bits() noexcept;
    template <int W, int N> constexpr rational sin_from_turn_fixed(imax turn_w) noexcept;
    template <int W, int N> constexpr rational cos_from_turn_fixed(imax turn_w) noexcept;
    template <insidable Out> constexpr Out store_grid(rational r);

    // sin (turn-input, internal). Q.N turn-phase → amplitude on `Out`'s grid via
    // the CORDIC engine: rescale the phase to the working scale 2^W and run the
    // shared `sin_from_turn_fixed` reducer.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr Out sin_turn_impl(In phase)
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "beman::inside::math: turn-phase N must be in [2, 30]");
      static_assert(lower_of<Out> <= -1 && upper_of<Out> >= 1,
                    "beman::inside::math: Out must cover [-1, 1]");

      constexpr int W = working_bits<Out>();
      imax raw    = raw_imax(phase);                       // Q.N turn
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));       // → Q.W
      return store_grid<Out>(sin_from_turn_fixed<W, W>(turn_w));
    }

    // cos (turn-input, internal). cos(x) = sin(x + π/2) — shift the phase
    // by one quarter-turn (modular wrap on the raw) and reuse sin. The
    // shift is integer-exact, no precision cost at this tier.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr Out cos_turn_impl(In phase)
    {
      constexpr int  N            = turn_bits<In>;
      constexpr imax full_mask    = (imax{1} << N) - 1;
      constexpr imax quarter_turn = imax{1} << (N - 2);

      In shifted = In::from_raw(raw_cast<In>(
          (raw_imax(phase) + quarter_turn) & full_mask));
      return sin_turn_impl<Out>(shifted);
    }

    //=========================================================================
    // Grid-scaled CORDIC engine. Values cross the API as `rational`; internally
    // we work in fixed-point at a scale 2^W chosen from the output grid
    // (`working_bits`), so precision follows the grid. The iteration is pure
    // shift-add (overflow-free); the only multiplies (table/gain derivation,
    // input scaling) use the wide `fmul`, bit-identical on every toolchain.
    //=========================================================================

    // (a·b) >> W via the full 128-bit product, magnitude-truncating (toward zero).
    constexpr imax fmul(imax a, imax b, int W) noexcept
    {
      bool neg = (a < 0) ^ (b < 0);
      umax ua = (a < 0) ? umax{0} - static_cast<umax>(a) : static_cast<umax>(a);
      umax ub = (b < 0) ? umax{0} - static_cast<umax>(b) : static_cast<umax>(b);
      const limb::pair<umax> p = limb::mul(ua, ub);
      const umax r = (W == 0) ? p.Lo
                   : (W < 64) ? ((p.Lo >> W) | (p.Hi << (64 - W)))
                   :            (p.Hi >> (W - 64));
      return neg ? -static_cast<imax>(r) : static_cast<imax>(r);
    }

    // round(v · 2^W) — scale-W marshalling (parametric Q.W). The product num·2^W
    // is formed at 128 bits, so a reduced numerator near 2^63 cannot wrap.
    // Rounding is half-away-from-zero, matching rational::round().
    constexpr imax to_fixed(rational v, int W) noexcept
    {
      const umax n = v.Numerator;
      const umax d = abs_den(v.Denominator);
      const auto sign = [&](umax q) { return (v.Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q); };
      // Power-of-two denominator (every core result and fixed_to_rational):
      // a shift, with round-half-up as the last shifted-out bit.
      if (std::has_single_bit(d))
      {
        const int D = std::countr_zero(d);
        if (W >= D) return sign(n << (W - D));
        const int sh = D - W;
        return sign((n >> sh) + ((n >> (sh - 1)) & 1u));
      }
      // n·2^W + d/2 below 2^64: one 64-bit divide (d < 2^63).
      if (n < (umax{1} << (63 - W)))
        return sign(((n << W) + d / 2) / d);
      // 128-bit (hi:lo) dividend n·2^W + d/2; the quotient fits umax.
      const umax half = d / 2;
      umax hi = (W == 0) ? 0 : (n >> (64 - W));
      umax lo = n << W;
      lo += half;
      hi += (lo < half);
      const umax q = limb::div(limb::div(umax{0}, hi, d).Lo, lo, d).Hi;
      return sign(q);
    }
    constexpr rational fixed_to_rational(imax x, int W) noexcept
    { return rational{x, imax{1} << W}; }

    // Grids eligible for the GCD-free store: unit-numerator notch, non-rational
    // storage, raw fits imax, assigned with round_nearest. (Unlike the Q-format
    // fast path this does NOT require integer Lower — Lower·K is an exact
    // integer by the grid invariant regardless.) The math results all carry a
    // power-of-two denominator, so the value index is formed with integer
    // shifts, rounded half away from zero — the same rule as the rational
    // assignment path (round_quotient), minus `(value−Lower)/Notch`'s GCDs.
    template <insidable Out>
    inline constexpr bool grid_fast_store =
        notch_of<Out>.Numerator == 1
        && !rational_raw<Out>
        // `f64` storage holds the VALUE, not an offset index, so route it
        // through the rational fallback `Out{r}` (same guard as fmod_int_fast).
        && !fp_raw<Out>
        && rounding_of(policy_of<Out>) == round_mode::nearest
        && (std::signed_integral<raw_t<Out>>
            || max_index_v<Out>
                 <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // Store a power-of-two-denominator result (the shape every core returns)
    // onto Out's grid. Fast path: pure integer. Fallback: the general rational
    // assignment (handles non-fast grids, clamp/wrap on out-of-range, etc).
    template <insidable Out>
    constexpr Out store_grid(rational r)
    {
      if constexpr (grid_fast_store<Out>)
      {
        umax den = abs_den(r.Denominator);
        if ((den & (den - 1)) == 0)                       // power-of-two denom
        {
          int  D   = std::countr_zero(den);
          imax num = signed_numerator(r);
          constexpr imax K = abs_den(notch_of<Out>.Denominator);  // 1/notch
          constexpr imax m = trunc((lower_of<Out> * rational{K}).value()); // Lower·K (exact int)
          // K·num + half must fit imax (a wide-denominator r, e.g. hypot's
          // 2^46, would wrap K·num and silently store `value mod 2^k`).
          constexpr imax lim = std::numeric_limits<imax>::max() / 2 / K;
          if (-lim <= num && num <= lim)
          {
            // value index round(value·K), ties half away from zero like the
            // assignment path: round the magnitude, then restore the sign.
            const imax half = (D > 0) ? (imax{1} << (D - 1)) : 0;
            const imax x    = K * num;
            const imax idx  = x >= 0 ? (x + half) >> D : -((-x + half) >> D);
            // Round, then range-check, like assignment: the rounded index must
            // be a slot; anything else goes to the policy cascade below.
            const imax off  = idx - m;
            if (off >= 0 && off <= static_cast<imax>(max_index_v<Out>))
              return Out::from_raw(raw_from_offset<Out>(static_cast<umax>(off)));
          }
        }
      }
      return Out{r};
    }

    // Working scale for an output grid: fractional bits to resolve the notch,
    // plus integer bits of the largest output magnitude (error ~V·2^-W, so large
    // outputs like pow's 10^k need headroom), plus CORDIC guard bits. Capped at 31.
    template <insidable Out>
    constexpr int working_bits() noexcept
    {
      constexpr int kGuard = 6;
      umax den        = abs_den(notch_of<Out>.Denominator);   // 1/notch
      int  notch_bits = (den <= 1) ? 0 : std::bit_width(den - 1);
      imax hi  = ceil(abs(upper_of<Out>));
      imax lo  = ceil(abs(lower_of<Out>));
      imax mag = (hi > lo) ? hi : lo;
      int  int_bits = (mag <= 1) ? 0 : std::bit_width(static_cast<umax>(mag));
      int  W = notch_bits + int_bits + kGuard;
      return (W < 12) ? 12 : (W > 31) ? 31 : W;
    }

    // Working scale for the composed endpoint functions (asin, tanh, log10,
    // cbrt, ...): several fixed-point stages each add error, so they need more
    // guard bits than one CORDIC pass; capped at the reference scale.
    inline constexpr int kEndpointGuard = 4;
    template <insidable Out>
    constexpr int endpoint_bits() noexcept
    {
      constexpr int W = working_bits<Out>() + kEndpointGuard;
      return W < 30 ? W : 30;
    }

    // atan(2^-i) in RADIANS at scale 2^W. i=0 is π/4 (exact, from kPiRat); i≥1
    // uses the fast-converging series atan(z)=z−z³/3+z⁵/5−… for tiny z=2^-i.
    constexpr imax atan_pow2_fixed(int i, int W) noexcept
    {
      if (i == 0)
        return to_fixed(kPiRat / 4, W);
      if (W - i < 1) return 0;
      imax z = imax{1} << (W - i);
      imax z2 = fmul(z, z, W), term = z, acc = 0;
      for (int k = 0; k < 64; ++k) {
        imax t = term / (2 * k + 1);
        acc += (k & 1) ? -t : t;
        if (z2 == 0) break;
        term = fmul(term, z2, W);
        if (term == 0) break;
      }
      return acc;
    }

    // 1/sqrt(a) at scale 2^W for a ∈ [1,2], division-free Newton (y←y(3−ay²)/2)
    // from y = 1, `iters` steps. Compile-time use (CORDIC gains, the seed table);
    // the runtime sqrt path seeds from a table instead (rsqrt_seeded).
    constexpr imax rsqrt_fixed(imax a, int W, int iters) noexcept
    {
      imax one = imax{1} << W, three = 3 * one, y = one;
      for (int k = 0; k < iters; ++k) {
        imax ay2 = fmul(a, fmul(y, y, W), W);
        y = fmul(y, three - ay2, W) >> 1;
      }
      return y;
    }

    // 1/√m for m ∈ [1, 2) at scale 2^30, sampled at the midpoints of 16 cells
    // (top 4 fraction bits of m): ≤ 1.1% error, a ~6.6-bit Newton start.
    inline constexpr auto rsqrt_seed_tbl = []{
      std::array<imax, 16> t{};
      for (int i = 0; i < 16; ++i)
      {
        const imax m_mid = (imax{1} << 30) + (((2 * imax{i} + 1)) << 25);   // 1 + (i+½)/16
        t[static_cast<std::size_t>(i)] = rsqrt_fixed(m_mid, 30, 12);
      }
      return t;
    }();

    // 1/√m at scale 2^W for m·2^W ∈ [2^W, 2^(W+1)): table seed, then Newton
    // (y ← y(3−my²)/2, error squares each step): 2 steps reach ~24 bits, 3 ~47.
    template <int W>
    constexpr imax rsqrt_seeded(imax m_w) noexcept
    {
      static_assert(W >= 4 && W <= 31);
      constexpr int iters = (W <= 20) ? 2 : 3;
      const imax seed = rsqrt_seed_tbl[static_cast<std::size_t>((m_w >> (W - 4)) & 15)];
      imax y = (W <= 30) ? (seed >> (30 - W)) : (seed << (W - 30));
      constexpr imax three = imax{3} << W;
      for (int k = 0; k < iters; ++k)
        y = fmul(y, three - fmul(m_w, fmul(y, y, W), W), W) >> 1;
      return y;
    }

    // √2 as a rational (literal source, like kPiRat / kLn2Rat), for sqrt's odd-
    // exponent step.
    inline constexpr rational kSqrt2Rat{1414213562, 1000000000};

    // √a at scale 2^W, a_w = a·2^W ≥ 0. Reduce a = m·2^e, m ∈ [1,2);
    // √a = √m · 2^(e/2), √m = m·(1/√m) via rsqrt; odd e multiplies in √2.
    // Templated on W so the √2 constant and iteration count fold at compile time.
    template <int W>
    constexpr imax sqrt_fixed(imax a_w) noexcept
    {
      if (a_w <= 0) return 0;
      int  lead = 63 - std::countl_zero(static_cast<umax>(a_w));
      int  e    = lead - W;
      imax m_w  = (e >= 0) ? (a_w >> e) : (a_w << (-e));    // m·2^W ∈ [2^W, 2^(W+1))
      imax sm   = fmul(m_w, rsqrt_seeded<W>(m_w), W);                     // √m · 2^W
      if (e & 1) { constexpr imax sqrt2_w = to_fixed(kSqrt2Rat, W); sm = fmul(sm, sqrt2_w, W); }
      int h = e >> 1;                                       // floor(e/2)
      return (h >= 0) ? (sm << h) : (sm >> (-h));
    }

    // CORDIC circular gain 1/K = ∏ 1/√(1+4^-i) at scale 2^W.
    constexpr imax cordic_invgain(int W, int N) noexcept
    {
      imax one = imax{1} << W, invK = one;
      for (int i = 0; i < N; ++i) {
        if (2 * i > W - 1) break;
        invK = fmul(invK, rsqrt_fixed(one + (one >> (2 * i)), W, 12), W);
      }
      return invK;
    }

    // Per-<W,N> compile-time atan table + prescaled gain (one per instantiation).
    template <int W, int N>
    inline constexpr auto cordic_atan_tbl = []{
      std::array<imax, static_cast<std::size_t>(N)> t{};
      for (int i = 0; i < N; ++i) t[i] = atan_pow2_fixed(i, W);
      return t;
    }();
    template <int W, int N>
    inline constexpr imax cordic_invgain_v = cordic_invgain(W, N);

    // Circular rotation: sin/cos of z (radians at scale 2^W, |z| ≤ ~π/2).
    template <int W, int N>
    constexpr void cordic_sincos(imax z, imax& sin_out, imax& cos_out) noexcept
    {
      imax x = cordic_invgain_v<W, N>, y = 0;
      for (int i = 0; i < N; ++i) {
        imax d  = (z >= 0) ? 1 : -1;
        imax xn = x - d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atan_tbl<W, N>[i];
        x = xn; y = yn;
      }
      sin_out = y; cos_out = x;
    }

    // sin(x) as a rational, x given as a Q.W turn-phase (one turn = 2^W). Reduces
    // to the first quadrant in turns (exact powers of two), then CORDICs the
    // residual converted to radians. cos = sin(+¼ turn).
    template <int W, int N>
    constexpr rational sin_from_turn_fixed(imax turn_w) noexcept
    {
      imax one_turn = imax{1} << W, half = imax{1} << (W - 1), quarter = imax{1} << (W - 2);
      turn_w &= (one_turn - 1);                      // wrap into [0,1) turn
      bool flip = (turn_w & half) != 0;
      turn_w &= (half - 1);
      if (turn_w > quarter) turn_w = half - turn_w;  // reflect about π/4
      // Exact zero at multiples of a half-turn: CORDIC leaves a ~1-ULP residual
      // at angle 0, but sin(kπ) must be exactly 0 (pole detection in tan relies
      // on it). Quadrant peaks (turn_w == quarter) round to ±1 on the grid.
      if (turn_w == 0) return rational{0};
      // Bound as constexpr so the 128-bit divide inside to_fixed is guaranteed
      // compile-time (same pattern as sqrt2_w) — args are all constants.
      constexpr imax two_pi_w = to_fixed(kTwoPiRat, W);
      imax rad = fmul(turn_w, two_pi_w, W);
      imax s, c;
      cordic_sincos<W, N>(rad, s, c);
      return fixed_to_rational(flip ? -s : s, W);
    }
    template <int W, int N>
    constexpr rational cos_from_turn_fixed(imax turn_w) noexcept
    { return sin_from_turn_fixed<W, N>(turn_w + (imax{1} << (W - 2))); }

    // tan(x) for a Q.W turn-phase as a Q.W fixed-point value, from ONE rotation:
    // reduce to quadrant q and r ∈ [0, ¼ turn], take sin r / cos r from a single
    // cordic_sincos pass, and divide in fixed point. Exact zeros at r == 0 and
    // r == ¼ turn keep the poles exact. Returns false on a pole.
    template <int W, int N>
    constexpr bool tan_from_turn_fixed(imax turn_w, imax& tan_w) noexcept
    {
      constexpr imax one_turn = imax{1} << W, quarter = imax{1} << (W - 2);
      turn_w &= (one_turn - 1);                        // wrap into [0, 1) turn
      const imax q = turn_w / quarter;                 // quadrant 0..3
      const imax r = turn_w - q * quarter;             // [0, ¼ turn)
      imax s = 0, c = imax{1} << W;                    // r == 0: sin 0, cos 1
      if (r != 0)
      {
        constexpr imax two_pi_w = to_fixed(kTwoPiRat, W);
        cordic_sincos<W, N>(fmul(r, two_pi_w, W), s, c);
      }
      // tan = sin/cos per quadrant: q0 s/c, q1 −c/s, q2 s/c, q3 −c/s.
      const imax num = (q & 1) ? -c : s;
      const imax den = (q & 1) ?  s : c;
      if (den == 0) return false;                      // pole (q odd, r == 0)
      // round-half-away((num << W) / den); |num| ≤ 2^W so num·2^W fits imax.
      const imax n = num * (imax{1} << W);
      const imax ad = den < 0 ? -den : den;
      const imax an = n < 0 ? -n : n;
      const imax mag = (an + ad / 2) / ad;
      tan_w = ((n < 0) != (den < 0)) ? -mag : mag;
      return true;
    }

    // 1/(2π) as a rational, for radians→turn reduction at any scale.
    inline constexpr rational kInvTwoPiRat =
      (rational{1} / kTwoPiRat).value();

    // radians → turn at scale 2^W. The single-term product's error (~2^-(W+1))
    // scales with |a|, capping the envelope at ±1024 rad — grids within it keep
    // that expression verbatim (bit-identical). Wider grids (up to ±2^20 rad) use
    // a two-term hi+lo split of 1/2π (lo carried at scale 2^(W+24)) to recover
    // ~2^-W turn accuracy, combined through the 128-bit fmul.
    template <int W, insidable In>
    constexpr imax rad_to_turn_w(rational a) noexcept
    {
      const imax a_w = to_fixed(a, W);
      if constexpr (lower_of<In> >= -1024 && upper_of<In> <= 1024)
      {
        constexpr imax inv_two_pi_w = to_fixed(kInvTwoPiRat, W);
        return fmul(a_w, inv_two_pi_w, W);
      }
      else
      {
        constexpr int  S    = W + 24;                 // ≤ 55 for W ≤ 31
        constexpr imax hi_w = to_fixed(kInvTwoPiRat, W);
        constexpr rational lo = kInvTwoPiRat - fixed_to_rational(hi_w, W);
        constexpr imax lo_s = to_fixed(lo, S);
        return fmul(a_w, hi_w, W) + fmul(a_w, lo_s, S);
      }
    }

    // CORDIC circular vectoring: atan2(y, x) in RADIANS at scale 2^W. Pre: x > 0
    // (caller pre-rotates other quadrants). Rotates (x, y) toward the +x axis,
    // accumulating the atan table; the gain cancels in y/x, so no prescale.
    template <int W, int N>
    constexpr imax cordic_atan2_rad(imax y, imax x) noexcept
    {
      imax z = 0;
      for (int i = 0; i < N; ++i) {
        imax dx = y >> i, dy = x >> i;
        if (y >= 0) { x += dx; y -= dy; z += cordic_atan_tbl<W, N>[i]; }
        else        { x -= dx; y += dy; z -= cordic_atan_tbl<W, N>[i]; }
      }
      return z;
    }

    //----- hyperbolic CORDIC (exp via sinh+cosh, ln via atanh-vectoring) ------

    // Reference precision for compile-time interval derivation and composed
    // endpoints (sinh/cosh/tanh/log10/cbrt/pow). Runtime impls use working_bits<Out>
    // so the value still follows the grid; this only bounds the derived intervals.
    inline constexpr int kRefBits = 30;

    // ln 2 as a rational (10-digit literal), plus its reciprocal — for exp/log
    // range reduction and base changes.
    inline constexpr rational kLn2Rat{6931471806, 10000000000};
    inline constexpr rational kInvLn2Rat = (rational{1} / kLn2Rat).value();

    // atanh(2^-i) at scale 2^W (series; 2^-i ≤ ½ ⇒ converges). i ≥ 1 only.
    constexpr imax atanh_pow2_fixed(int i, int W) noexcept
    {
      if (W - i < 1) return 0;
      imax z = imax{1} << (W - i);
      imax z2 = fmul(z, z, W), term = z, acc = 0;
      for (int k = 0; k < 64; ++k) {
        acc += term / (2 * k + 1);
        if (z2 == 0) break;
        term = fmul(term, z2, W);
        if (term == 0) break;
      }
      return acc;
    }

    // Hyperbolic CORDIC shift schedule with the convergence repeats at
    // i = 4, 13, 40, … (each 3·prev+1). Length L covers W + guard distinct bits.
    template <int L>
    constexpr std::array<int, static_cast<std::size_t>(L)> hyp_seq() noexcept
    {
      std::array<int, static_cast<std::size_t>(L)> s{};
      int idx = 0, i = 1, rep = 4;
      while (idx < L) {
        s[idx++] = i;
        if (i == rep && idx < L) { s[idx++] = i; rep = 3 * rep + 1; }
        ++i;
      }
      return s;
    }

    constexpr int hyp_len(int W) noexcept { return W + 6; }

    template <int W, int L>
    inline constexpr auto cordic_atanh_tbl = []{
      constexpr auto seq = hyp_seq<L>();
      std::array<imax, static_cast<std::size_t>(L)> t{};
      for (int j = 0; j < L; ++j)
        t[j] = atanh_pow2_fixed(seq[j], W);
      return t;
    }();

    // Hyperbolic gain 1/Kh = ∏ 1/√(1−4^-i) over the schedule, at scale 2^W.
    template <int W, int L>
    inline constexpr imax cordic_hyp_invgain_v = []{
      constexpr auto seq = hyp_seq<L>();
      imax one = imax{1} << W, invK = one;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        if (2 * i > W - 1) continue;
        invK = fmul(invK, rsqrt_fixed(one - (one >> (2 * i)), W, 12), W);   // ×1/√(1−4^-i)
      }
      return invK;
    }();

    // Rotation: sinh/cosh of z (scale 2^W, |z| ≤ ~1.11). exp(z) = sinh+cosh.
    template <int W, int L>
    constexpr void cordic_sinhcosh(imax z, imax& sh, imax& ch) noexcept
    {
      constexpr auto seq = hyp_seq<L>();
      imax x = cordic_hyp_invgain_v<W, L>, y = 0;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        imax d  = (z >= 0) ? 1 : -1;
        imax xn = x + d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atanh_tbl<W, L>[j];
        x = xn; y = yn;
      }
      sh = y; ch = x;
    }

    // Vectoring: atanh(y/x) at scale 2^W (drives y → 0). ln(m) = 2·atanh((m−1)/(m+1)).
    template <int W, int L>
    constexpr imax cordic_atanh_vec(imax x, imax y) noexcept
    {
      constexpr auto seq = hyp_seq<L>();
      imax z = 0;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        imax d  = (y < 0) ? 1 : -1;
        imax xn = x + d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atanh_tbl<W, L>[j];
        x = xn; y = yn;
      }
      return z;
    }

    // 2^(x_w / 2^W) as a rational, x_w a fixed-point exponent at scale 2^W.
    // Split x = k + f (k integer, f ∈ [−½,½]); 2^x = 2^k · e^(f·ln2). The 2^k
    // lives in the rational's power-of-two num/den so large |x| never overflows.
    // Pure fixed-point — composing this from a log result (pow/cbrt) never
    // stacks rational denominators.
    template <int W>
    constexpr rational exp2_from_fixed(imax x_w) noexcept
    {
      imax k   = (x_w + (imax{1} << (W - 1))) >> W;        // round to nearest int
      imax f_w = x_w - (k << W);                            // ∈ [−2^(W−1), 2^(W−1)]
      constexpr imax ln2_w = to_fixed(kLn2Rat, W);            // compile-time constant
      imax fr_w = fmul(f_w, ln2_w, W);                      // f·ln2 (natural)
      imax er_w;
      if (fr_w == 0) er_w = imax{1} << W;                   // 2^k exactly
      else { imax sh, ch; cordic_sinhcosh<W, hyp_len(W)>(fr_w, sh, ch); er_w = sh + ch; }
      if (k <= W) return rational{static_cast<umax>(er_w), imax{1} << (W - k)};
      return rational{static_cast<umax>(er_w) << (k - W), 1};
    }

    // ln(w) at scale 2^W as a fixed-point imax. Leading-bit reduce w = 2^e·m,
    // m ∈ [1,2); ln(w) = e·ln2 + 2·atanh((m−1)/(m+1)). Pre: w > 0.
    template <int W>
    constexpr imax log_to_fixed(rational w) noexcept
    {
      imax w_w  = to_fixed(w, W);
      int  lead = 63 - std::countl_zero(static_cast<umax>(w_w));
      int  e    = lead - W;
      imax one  = imax{1} << W;
      imax m_w  = (e >= 0) ? (w_w >> e) : (w_w << (-e));   // m·2^W ∈ [2^W, 2^(W+1))
      imax z    = cordic_atanh_vec<W, hyp_len(W)>(m_w + one, m_w - one);
      constexpr imax ln2_w = to_fixed(kLn2Rat, W);           // compile-time constant
      return e * ln2_w + 2 * z;
    }

    // log2(x) at scale 2^W as imax: ln(x)·log2(e).
    template <int W>
    constexpr imax log2_to_fixed(rational x) noexcept
    {
      constexpr imax inv_ln2_w = to_fixed(kInvLn2Rat, W);   // compile-time constant
      return fmul(log_to_fixed<W>(x), inv_ln2_w, W);
    }

    // e^(v_w / 2^W) as a rational: 2^(v·log2 e).
    template <int W>
    constexpr rational exp_from_fixed(imax v_w) noexcept
    {
      constexpr imax inv_ln2_w = to_fixed(kInvLn2Rat, W);   // compile-time constant
      return exp2_from_fixed<W>(fmul(v_w, inv_ln2_w, W));
    }

    // Rational-input wrappers (inputs are small-denominator values; fine to
    // marshal through to_fixed). pow/cbrt compose via the *_fixed primitives
    // above instead, to avoid rational-denominator blow-up.
    template <int W>
    constexpr rational exp_rat(rational v) noexcept
    { return exp_from_fixed<W>(to_fixed(v, W)); }
    template <int W>
    constexpr rational log_rat(rational w) noexcept
    { return fixed_to_rational(log_to_fixed<W>(w), W); }
    template <int W>
    constexpr rational exp2_rat(rational x) noexcept
    { return exp2_from_fixed<W>(to_fixed(x, W)); }
    template <int W>
    constexpr rational log2_rat(rational x) noexcept
    { return fixed_to_rational(log2_to_fixed<W>(x), W); }

    // 1/ln10 at scale 2^W — for log10 = ln·(1/ln10), composed in fixed-point.
    template <int W>
    constexpr imax inv_ln10_fixed() noexcept
    {
      // ln10 ≈ 2.302585 (small, no overflow): derive the rational once, marshal.
      return to_fixed((rational{1} /
                       fixed_to_rational(log_to_fixed<W>(rational{10}), W)).value(), W);
    }
  } // namespace detail

  // sin: radians-valued inside → amplitude on the auto-deduced output grid.
  // Converts to a turn (× 1/(2π)) at the grid-derived working scale, then runs
  // the circular-CORDIC reducer. Inputs up to |angle| ≤ 2^20 rad (see
  // rad_to_turn_w for the reduction split beyond ±1024).
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sin_into(In angle)
  {
    detail::domain_sin<In>();
    static_assert(lower_of<Out> <= -1 && upper_of<Out> >= 1,
                  "beman::inside::math::sin: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::sin_from_turn_fixed<W, W>(turn_w));
  }
  } // namespace cordic

  // cos: radians-valued inside → amplitude. cos(x) = sin(x + π/2) — add a
  // quarter-turn before the quadrant reducer, same precision as sin.
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cos_into(In angle)
  {
    detail::domain_cos<In>();
    static_assert(lower_of<Out> <= -1 && upper_of<Out> >= 1,
                  "beman::inside::math::cos: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::cos_from_turn_fixed<W, W>(turn_w));
  }
  } // namespace cordic

  namespace detail
  {
    using namespace beman::inside::detail;

    // Shared tail of tan_into / tan_turn_impl: pole → division_by_zero; outside
    // Out → overflow (a clamp Out saturates in the store instead).
    template <insidable Out, int W>
    constexpr std::expected<Out, errc> tan_store(imax turn_w)
    {
      imax t_w;
      if (!tan_from_turn_fixed<W, W>(turn_w, t_w))
        return std::unexpected(errc::division_by_zero);
      if constexpr (!has_flag(policy_of<Out>, clamp))
      {
        // t_w/2^W ∈ [lo, hi]  ⇔  ⌈lo·2^W⌉ ≤ t_w ≤ ⌊hi·2^W⌋
        constexpr imax lo_w = ceil ((lower_of<Out> * rational{imax{1} << W}).value());
        constexpr imax hi_w = floor((upper_of<Out> * rational{imax{1} << W}).value());
        if (t_w < lo_w || t_w > hi_w)
          return std::unexpected(errc::overflow);
      }
      return store_grid<Out>(fixed_to_rational(t_w, W));
    }

    // tan (turn-input, internal). sin/cos from the grid-scaled engine, divided
    // with a pole guard. Returns `unexpected(errc::division_by_zero)` when the
    // phase lands on a pole (cos == 0) and `unexpected(errc::overflow)` when the
    // result exceeds Out's range.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr std::expected<Out, errc> tan_turn_impl(In phase)
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "beman::inside::math: turn-phase N must be in [2, 30]");

      constexpr int W = working_bits<Out>();
      imax raw    = raw_imax(phase);
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));

      return tan_store<Out, W>(turn_w);
    }
  } // namespace detail

  // tan: radians-valued inside → amplitude, with pole guard. sin/cos from the
  // radians input, divided. Returns `unexpected(division_by_zero)` if cos rounds
  // to 0 (input on a pole), `unexpected(overflow)` if the result exceeds Out.
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> tan_into(In angle)
  {
    detail::domain_tan<In>();

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);

    return detail::tan_store<Out, W>(turn_w);
  }
  } // namespace cordic


  // log2: positive inside → inside. log2(x) = ln(x)·log2(e) via the grid-scaled
  // hyperbolic-CORDIC `log2_rat` core (leading-bit reduction + atanh vectoring).
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log2_into(In x)
  {
    detail::domain_log2<In>();

    return detail::store_grid<Out>(detail::log2_rat<detail::working_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  // exp2: inside → inside, returning 2^x. 2^x = e^(x·ln2) via the grid-scaled
  // hyperbolic-CORDIC `exp2_rat` core (integer/fractional split + sinh/cosh).
  //
  // Restrict |x| ≤ 30 so the rational denominator 2^(30 - k) fits in int63.
  // The output `Out` must include non-negative values and cover at least
  // [2^lower_of<In>, 2^upper_of<In>] — anything narrower needs `clamp` to absorb
  // overflow at the assignment.
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out exp2_into(In x)
  {
    detail::domain_exp2<In>();
    static_assert(lower_of<Out> >= 0,
                  "beman::inside::math::exp2: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp2_rat<detail::working_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  // exp: thin wrapper. exp(x) = exp2(x · log2(e)). The scaling factor
  // log2(e) ≈ 1.4427, so x must stay inside [-30/log2(e), 30/log2(e)] ≈
  // [-20.79, 20.79] for exp2's denominator-shift envelope. We use [-20, 20].
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out exp_into(In x)
  {
    detail::domain_exp<In>();
    static_assert(lower_of<Out> >= 0,
                  "beman::inside::math::exp: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp_rat<detail::working_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  // log: thin wrapper. log(x) = log2(x) · ln(2). Result precision matches
  // log2 minus 1-2 ULP from the final fixed-point scaling.
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log_into(In x)
  {
    detail::domain_log<In>();

    return detail::store_grid<Out>(detail::log_rat<detail::working_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  // pow_base<Base>(x) = Base^x for compile-time-known integer Base ≥ 2.
  // Implemented as exp2(x · log2(Base)) with log2(Base) from the grid-scaled
  // `log2_to_fixed` core — no hand-typed magic constants.
  // For Base = 10 it builds decibel → linear gain (see examples/decibels.cpp).
  namespace cordic {
  template <insidable Out, imax Base, insidable In>
  [[nodiscard]] constexpr Out pow_base_into(In x)
  {
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be ≥ 2");
    static_assert(lower_of<Out> >= 0,
                  "beman::inside::math::pow_base: Out must be non-negative");

    constexpr int W = detail::working_bits<Out>();
    constexpr imax lb_w = detail::log2_to_fixed<W>(rational{Base});   // log2(Base)·2^W
    imax sc_w = detail::fmul(detail::to_fixed(rational{x}, W), lb_w, W);
    // x·log2(Base) beyond ±30: past the envelope exp2_from_fixed can scale.
    constexpr imax env = imax{30} << W;
    if (sc_w > env || sc_w < -env) [[unlikely]]
      return detail::pow_envelope_fail<Out>(sc_w > 0);
    return detail::store_grid<Out>(detail::exp2_from_fixed<W>(sc_w));
  }
  } // namespace cordic


  // atan2: signed inside, signed inside → radians ∈ [-π, π], via CORDIC vectoring
  // with quadrant pre-rotation. CORDIC depends only on y/x, so inputs beyond
  // magnitude 1 are normalized by the larger magnitude (exact rational division);
  // inputs already in [-1, 1] skip it.
  namespace cordic {
  template <insidable Out, insidable InY, insidable InX>
  [[nodiscard]] constexpr Out atan2_into(InY y, InX x)
  {
    detail::domain_atan2<InY>();
    detail::domain_atan2<InX>();
    static_assert(lower_of<Out> <= -detail::kPiRat && upper_of<Out> >= detail::kPiRat,
                  "beman::inside::math::atan2: Out must cover [-π, π]");

    constexpr int W = detail::working_bits<Out>();
    rational yv = y, xv = x;
    {
      rational ay = beman::inside::detail::abs(yv);
      rational ax = beman::inside::detail::abs(xv);
      rational m  = (ax > ay) ? ax : ay;
      if (m > rational{1})
      {
        yv = yv / m;
        xv = xv / m;
      }
    }
    imax y_w = detail::to_fixed(yv, W);
    imax x_w = detail::to_fixed(xv, W);

    // atan2(0, 0) is undefined; convention is 0. Without the guard CORDIC
    // accumulates the angle table on zero x,y and produces garbage.
    if (x_w == 0 && y_w == 0) return Out{0};

    // Quadrant pre-rotation: CORDIC requires x > 0. For x < 0, rotate the
    // vector by ±π/2 (in radians, at scale W) to land in the right half-plane
    // and add the rotation back at the end.
    //   Q2 (x<0, y≥0): (x',y') = (y, −x),  θ = CORDIC + π/2.
    //   Q3 (x<0, y<0): (x',y') = (−y, x),  θ = CORDIC − π/2.
    constexpr imax half_pi_w = detail::to_fixed(detail::kPiRat / 2, W);
    imax pre_rotation = 0;
    if (x_w < 0) {
      if (y_w >= 0) { imax nx = y_w;  imax ny = -x_w; x_w = nx; y_w = ny; pre_rotation =  half_pi_w; }
      else          { imax nx = -y_w; imax ny =  x_w; x_w = nx; y_w = ny; pre_rotation = -half_pi_w; }
    }

    imax rad = detail::cordic_atan2_rad<W, W>(y_w, x_w) + pre_rotation;   // radians, scale W
    return detail::store_grid<Out>(detail::fixed_to_rational(rad, W));
  }
  } // namespace cordic

  namespace detail
  {
    // max(|lower_of<In>|, |upper_of<In>|) as a constexpr rational. Used to size
    // the auto-deduced abs output.
    template <insidable In>
    inline constexpr rational abs_auto_upper =
      (abs(lower_of<In>) > abs(upper_of<In>))
        ? abs(lower_of<In>) : abs(upper_of<In>);

    template <insidable In>
    using abs_auto_t = inside<{{rational{0}, abs_auto_upper<In>},
                              notch_of<In>}, out_policy<In>>;

    // sign(x) ∈ {sign(Lower) … sign(Upper)}, integer notch.
    template <insidable In>
    using sign_auto_t = inside<{rational{sign(lower_of<In>)}, rational{sign(upper_of<In>)}},
                               out_policy<In>>;

    // copysign(mag, sgn): |mag| with sgn's possible signs. |mag| ranges over
    // [m_lo, m_hi] (m_lo = 0 when mag's interval spans 0); a valid grid's Lower is
    // a multiple of its notch, so ±|mag| stays on mag's lattice.
    template <insidable Mag>
    inline constexpr rational abs_auto_lower =
      (lower_of<Mag> <= 0 && upper_of<Mag> >= 0) ? rational{0}
      : (abs(lower_of<Mag>) < abs(upper_of<Mag>)) ? abs(lower_of<Mag>) : abs(upper_of<Mag>);

    template <insidable Mag, insidable Sgn>
    using copysign_auto_t = inside<{{
        lower_of<Sgn> < 0 ? -abs_auto_upper<Mag> : abs_auto_lower<Mag>,
        upper_of<Sgn> >= 0 ? abs_auto_upper<Mag> : -abs_auto_lower<Mag>},
        notch_of<Mag>}, out_policy<Mag>>;

    template <insidable In>
    using floor_auto_t = inside<{{rational{floor(lower_of<In>)},
                                  rational{floor(upper_of<In>)}},
                                 1}, out_policy<In>>;

    template <insidable In>
    using ceil_auto_t = inside<{{rational{ceil(lower_of<In>)},
                                 rational{ceil(upper_of<In>)}},
                                1}, out_policy<In>>;

    template <insidable In>
    using round_auto_t = inside<{{rational{round(lower_of<In>)},
                                  rational{round(upper_of<In>)}},
                                 1}, out_policy<In>>;

    template <insidable In>
    using trunc_auto_t = inside<{{rational{trunc(lower_of<In>)},
                                  rational{trunc(upper_of<In>)}},
                                 1}, out_policy<In>>;

    // Double-backed fast path for the algebraic tier. |x| and the integer
    // roundings of a grid value are exact in double (|x| < 2^53 on a
    // double_exact grid, so the imax cast cannot overflow), and the
    // auto-deduced Out holds every result by construction, so the result is
    // stored as the raw without the rational round-trip.
    template <insidable Out, insidable AutoOut, insidable In>
    inline constexpr bool fp_direct =
        std::same_as<Out, AutoOut> && fp_raw<In> && fp_raw<Out>;

    template <insidable Out, insidable In, typename F>
    constexpr Out fp_direct_store(In x, F f) noexcept
    { return Out::from_raw(raw_cast<Out>(f(static_cast<double>(x.raw())))); }

    constexpr double fp_trunc(double v) noexcept { return static_cast<double>(static_cast<imax>(v)); }
    constexpr double fp_floor(double v) noexcept { const double t = fp_trunc(v); return t > v ? t - 1 : t; }
    constexpr double fp_ceil (double v) noexcept { const double t = fp_trunc(v); return t < v ? t + 1 : t; }
    constexpr double fp_round(double v) noexcept   // half away from zero, like rational round()
    {
      const double t = fp_trunc(v), f = v - t;     // exact: v and t share the grid
      return f >= 0.5 ? t + 1 : f <= -0.5 ? t - 1 : t;
    }
  }

  //---------------------------------------------------------------------------
  // Algebraic tier — exact, no polynomial machinery. Each function wraps the
  // corresponding `rational` operation and routes through `Out`'s assignment.
  //---------------------------------------------------------------------------

  // |x|. Output Lower must be ≥ 0 (the result is always non-negative).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out abs_into(In x)
  {
    static_assert(lower_of<Out> <= 0,
                  "beman::inside::math::abs: Out must include 0");
    if constexpr (detail::fp_direct<Out, detail::abs_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, [](double v) { return v < 0 ? -v : v; });
    else
      return detail::store_grid<Out>(beman::inside::detail::abs(rational{x}));
  }

  // sign(x) ∈ {−1, 0, 1}, by exact comparison (no decode).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sign_into(In x)
  { return Out{imax{(x > 0) - (x < 0)}}; }

  // copysign(mag, sgn) — |mag| with the sign of sgn; sgn == 0 counts as positive.
  template <insidable Out, insidable Mag, insidable Sgn>
  [[nodiscard]] constexpr Out copysign_into(Mag mag, Sgn sgn)
  {
    const rational a = beman::inside::detail::abs(rational{mag});
    return detail::store_grid<Out>(sgn < 0 ? -a : a);
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out floor_into(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::floor_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_floor);
    else
      return detail::store_grid<Out>(floor(rational{x}));
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out ceil_into(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::ceil_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_ceil);
    else
      return detail::store_grid<Out>(ceil(rational{x}));
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out round_into(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::round_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_round);
    else
      return detail::store_grid<Out>(round(rational{x}));
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out trunc_into(In x)
  {
    if constexpr (detail::fp_direct<Out, detail::trunc_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_trunc);
    else
      return detail::store_grid<Out>(trunc(rational{x}));
  }

  namespace detail
  {
    using namespace beman::inside::detail;

    // Gate for fmod's integer fast path. When both operands and Out are
    // integer-backed on commensurable notches, fmod collapses to ONE integer
    // remainder in units of g = gcd(notch_of<InX>, notch_of<InY>): with x = a·g and
    // y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ %
    // is truncated division, the same convention). Conditions:
    //   * integer raws only (rational/double raws keep the rational path),
    //   * non-zero notches, g on Out's grid (g / notch_of<Out> integer),
    //   * divisor grid excludes zero (no runtime zero check needed),
    //   * Out's interval covers ±max|y| (result magnitude is < |y|),
    //   * all unit counts fit comfortably in imax (headroom 4).
    template <insidable Out, insidable InX, insidable InY>
    inline constexpr bool fmod_int_fast = []{
      if (rational_raw<InX> || fp_raw<InX>
       || rational_raw<InY> || fp_raw<InY>
       || rational_raw<Out> || fp_raw<Out>)
        return false;
      if (notch_of<InX> == 0 || notch_of<InY> == 0 || notch_of<Out> == 0)
        return false;
      if (!divisor_excludes_zero<InY>)
        return false;
      auto go = gcd(notch_of<InX>, notch_of<InY>);
      if (!go.has_value()) return false;
      rational g = *go;
      auto qo = g / notch_of<Out>;
      if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
      rational maxx =
          abs(lower_of<InX>) > abs(upper_of<InX>)
            ? abs(lower_of<InX>) : abs(upper_of<InX>);
      rational maxy =
          abs(lower_of<InY>) > abs(upper_of<InY>)
            ? abs(lower_of<InY>) : abs(upper_of<InY>);
      if (lower_of<Out> > -maxy || upper_of<Out> < maxy)
        return false;
      constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
      auto ux = maxx / g;  auto uy = maxy / g;  auto uo = maxy / notch_of<Out>;
      return ux.has_value() && uy.has_value() && uo.has_value()
          && ux->Numerator <= lim && uy->Numerator <= lim && uo->Numerator <= lim;
    }();
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y != 0 (fmod_into checks it).
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y)
  {
    if constexpr (detail::fmod_int_fast<Out, InX, InY>)
    {
      // One integer remainder in g-units; bit-identical to the rational path.
      constexpr rational g = *beman::inside::detail::gcd(notch_of<InX>, notch_of<InY>);
      constexpr imax wx  = trunc((notch_of<InX> / g).value());
      constexpr imax wy  = trunc((notch_of<InY> / g).value());
      constexpr imax wo  = trunc((g / notch_of<Out>).value());
      constexpr imax lox = trunc((lower_of<InX> / g).value());   // exact: grid invariant
      constexpr imax loy = trunc((lower_of<InY> / g).value());
      constexpr imax loo = trunc((lower_of<Out> / notch_of<Out>).value());
      const imax a = beman::inside::detail::raw_imax(x) * wx
                   + (beman::inside::detail::index_raw<InX> ? lox : 0);
      const imax b = beman::inside::detail::raw_imax(y) * wy
                   + (beman::inside::detail::index_raw<InY> ? loy : 0);
      const imax r = a % b;                                    // |r| < |b|, in Out's range
      return Out::from_raw(beman::inside::detail::raw_from_offset<Out>(r * wo - loo));
    }
    else
    {
      rational xv = x;
      rational yv = y;
      rational q  = xv / yv;
      imax     qt = trunc(q);
      rational qy = qt * yv;
      rational r  = xv - qy;
      return detail::store_grid<Out>(r);
    }
  }

  // Like `/`: a plain Out when y's grid excludes 0, else expected<Out, errc>
  // with division_by_zero for y == 0.
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod_into(InX x, InY y)
  {
    if constexpr (beman::inside::detail::divisor_excludes_zero<InY>)
      return fmod_nonzero<Out>(x, y);
    else
    {
      if (y == 0)
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      return std::expected<Out, errc>{fmod_nonzero<Out>(x, y)};
    }
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — algebraic tier.
  //
  // Each `fn_into<Out>(x)` has an auto form `fn(x)` that derives `Out` from `In`
  // and delegates to it. Notch policy: abs/fmod inherit `notch_of<In>`; floor/ceil/round/trunc
  // deduce notch 1 since their outputs are integer-valued.
  //---------------------------------------------------------------------------

  //---------------------------------------------------------------------------
  // pown<E> — compile-time integer powers, pure grid arithmetic
  //---------------------------------------------------------------------------
  // Repeated squaring in inside-space: every multiply widens the result grid
  // corner-correctly, so the result is exact for exact inputs and negative
  // bases are fine. No engine, no `f64` requirement — works on any inside
  // (like abs/floor/fmod). Checked rational raws may return
  // std::expected<inside, errc> per the usual arithmetic vocabulary. Negative
  // exponents are deferred (they need the division error story).
  template <imax E, insidable In>
    requires (E >= 0)
  [[nodiscard]] constexpr auto pown(In x) noexcept
  {
    if constexpr (E == 0)      { (void)x; return just<1>; }
    else if constexpr (E == 1) return x;
    else if constexpr (E % 2)  return x * pown<E - 1>(x);
    else                       { auto h = pown<E / 2>(x); return h * h; }
  }

  template <insidable In>
  [[nodiscard]] constexpr auto abs(In x) { return abs_into<detail::abs_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto sign(In x) { return sign_into<detail::sign_auto_t<In>>(x); }

  template <insidable Mag, insidable Sgn>
  [[nodiscard]] constexpr auto copysign(Mag mag, Sgn sgn)
  { return copysign_into<detail::copysign_auto_t<Mag, Sgn>>(mag, sgn); }

  template <insidable In>
  [[nodiscard]] constexpr auto floor(In x) { return floor_into<detail::floor_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto ceil(In x) { return ceil_into<detail::ceil_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto round(In x) { return round_into<detail::round_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto trunc(In x) { return trunc_into<detail::trunc_auto_t<In>>(x); }

  // sqrt: non-negative inside → inside. Newton-Raphson on grid-scaled integer math
  // with a leading-bit initial guess; input must have Lower == 0. The mixed-sign
  // overload below accepts Lower < 0 and errors on a negative runtime value.
  namespace cordic {
  template <insidable Out, insidable In>
    requires (lower_of<In> == rational{0})
  [[nodiscard]] constexpr Out sqrt_into(In x)
  {
    static_assert(lower_of<Out> <= 0,
                  "beman::inside::math::sqrt: Out must include 0");

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(rational{x}, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }
  } // namespace cordic

  // Mixed-sign sqrt: accepts inputs whose interval crosses zero. Returns
  // `unexpected(errc::domain_error)` on a negative runtime value, else same as
  // sqrt_into.
  namespace cordic {
  template <insidable Out, insidable In>
    requires (lower_of<In> < rational{0})
  [[nodiscard]] constexpr std::expected<Out, errc> sqrt_into(In x)
  {
    static_assert(lower_of<Out> <= 0,
                  "beman::inside::math::sqrt: Out must include 0");

    rational v = beman::inside::detail::as_rational(x);
    if (v < rational{0})
      return std::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(v, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }
  } // namespace cordic

  //---------------------------------------------------------------------------
  // Auto-deducing forms — monotonic transcendental tier. Each derives Out from
  // In: Lower/Upper from running the engine cores on the input endpoints at
  // compile time, rounded outward to notch_of<In> so the deduced inside covers the
  // true range even for irrational endpoints; notch and policy inherited from In.
  //---------------------------------------------------------------------------
  namespace detail
  {
    using namespace beman::inside::detail;

    // Round a rational down to the nearest multiple of `notch`.
    constexpr rational floor_to_notch(rational x, rational notch) noexcept
    {
      rational q = x / notch;
      imax n  = floor(q);
      return n * notch;
    }

    // Round a rational up to the nearest multiple of `notch`.
    constexpr rational ceil_to_notch(rational x, rational notch) noexcept
    {
      rational q = x / notch;
      imax n = ceil(q);
      return n * notch;
    }

    // Helpers: evaluate the engine cores on a compile-time-known
    // rational endpoint and return the result as a rational.
    constexpr rational sqrt_endpoint(rational v) noexcept
    {
      if (v == 0) return rational{0};
      return fixed_to_rational(sqrt_fixed<kRefBits>(to_fixed(v, kRefBits)), kRefBits);
    }

    constexpr rational exp2_endpoint(rational v) noexcept
    { return exp2_rat<kRefBits>(v); }

    constexpr rational log2_endpoint(rational v) noexcept
    { return log2_rat<kRefBits>(v); }

    constexpr rational exp_endpoint(rational v) noexcept
    { return exp_rat<kRefBits>(v); }

    constexpr rational log_endpoint(rational v) noexcept
    { return log_rat<kRefBits>(v); }

    template <imax Base>
    constexpr rational pow_base_endpoint(rational v) noexcept
    {
      imax sc_w = fmul(to_fixed(v, kRefBits),
                       log2_to_fixed<kRefBits>(rational{Base}), kRefBits);
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // Deduction aliases. Each rounds endpoints outward to notch_of<In> and adds
    // `round_nearest` — the cores emit sub-notch drift, so the assignment needs
    // a rounding rule to land on the grid.
    template <insidable In>
    using sqrt_auto_t = inside<{{rational{0},
                                ceil_to_notch(sqrt_endpoint(upper_of<In>), notch_of<In>)},
                               notch_of<In>}, out_policy<In> | round_nearest>;

    // Mixed-sign sqrt: Upper of the result is sqrt of the larger absolute
    // endpoint, since the runtime value can be anywhere in [Lower, Upper].
    template <insidable In>
    inline constexpr rational sqrt_signed_upper =
        (abs(lower_of<In>) > abs(upper_of<In>))
            ? abs(lower_of<In>) : abs(upper_of<In>);

    template <insidable In>
    using sqrt_signed_auto_t = inside<{{rational{0},
                                       ceil_to_notch(sqrt_endpoint(sqrt_signed_upper<In>),
                                                     notch_of<In>)},
                                      notch_of<In>}, out_policy<In> | round_nearest>;

    // Auto output grid for results in [lo, hi]: the endpoints rounded outward
    // to In's notch, with In's notch and policy (plus round_nearest).
    template <insidable In, rational Lo, rational Hi>
    using outward_t = inside<{{floor_to_notch(Lo, notch_of<In>), ceil_to_notch(Hi, notch_of<In>)},
                              notch_of<In>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using exp2_auto_t = outward_t<In, exp2_endpoint(lower_of<In>), exp2_endpoint(upper_of<In>)>;

    template <insidable In>
    using log2_auto_t = outward_t<In, log2_endpoint(lower_of<In>), log2_endpoint(upper_of<In>)>;

    template <insidable In>
    using exp_auto_t = outward_t<In, exp_endpoint(lower_of<In>), exp_endpoint(upper_of<In>)>;

    template <insidable In>
    using log_auto_t = outward_t<In, log_endpoint(lower_of<In>), log_endpoint(upper_of<In>)>;

    template <imax Base, insidable In>
    using pow_base_auto_t = outward_t<In, pow_base_endpoint<Base>(lower_of<In>), pow_base_endpoint<Base>(upper_of<In>)>;
  } // namespace detail

  //---------------------------------------------------------------------------
  // Auto-deducing forms — trig + atan2 + tan + fmod.
  //
  // sin / cos default to the full amplitude range [-1, 1]; atan2 defaults to
  // the full angle range [-π, π] radians. tan defaults to [-1024, 1024]
  // (covers all phases >1 slot from a pole; closer-to-pole phases trip the
  // overflow branch of the returned `expected`). fmod inherits sign from x.
  // Notch is inherited from input throughout.
  //---------------------------------------------------------------------------
  namespace detail
  {
    using namespace beman::inside::detail;

    template <insidable In>
    using sin_auto_t = inside<{{-rational{1}, rational{1}},
                               notch_of<In>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using cos_auto_t = sin_auto_t<In>;

    // Output covers [-π, π] rounded outward to notch multiples — the exact
    // ±π endpoints are irrational and would violate the grid's divides-evenly
    // invariant against a rational notch.
    template <insidable In, insidable InX = In>
    using atan2_auto_t = inside<{{floor_to_notch(-kPiRat, gcd_notch<In, InX>),
                                  ceil_to_notch ( kPiRat, gcd_notch<In, InX>)},
                                 gcd_notch<In, InX>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using tan_auto_t = inside<{{-rational{1024}, rational{1024}},
                               notch_of<In>}, out_policy<In> | round_nearest>;

    // fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
    // both notches (x − k·y lies on that lattice, so the result is exact).
    template <insidable B>
    inline constexpr rational max_abs = abs(lower_of<B>) > abs(upper_of<B>) ? abs(lower_of<B>) : abs(upper_of<B>);

    template <insidable InX, insidable InY>
    inline constexpr rational fmod_bound =
        max_abs<InX> < max_abs<InY> ? max_abs<InX> : max_abs<InY>;


    template <insidable InX, insidable InY>
    using fmod_auto_t = inside<{{(lower_of<InX> < 0 ? -fmod_bound<InX, InY> : rational{0}),
                                 (upper_of<InX> > 0 ?  fmod_bound<InX, InY> : rational{0})},
                                gcd_notch<InX, InY>}, out_policy<InX> | round_nearest>;
  } // namespace detail

  template <insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod(InX x, InY y)
  { return fmod_into<detail::fmod_auto_t<InX, InY>>(x, y); }

  //---------------------------------------------------------------------------
  // amp<K> — amplitude grid [-1, 1] at 1/K resolution: a ready-made explicit
  // output for sin / cos (`math::sin_into<math::amp<32768>>(angle)`), decoupling
  // the output precision from the angle's grid. K must be a power of two (f64).
  // All angles are radians, as in <cmath>.
  //---------------------------------------------------------------------------
  template <std::uint64_t K>
  using amp = inside<{{rational{-1}, rational{1}},
                     per<K>}, f64>;

  //===========================================================================
  // Extended transcendentals — inverse trig, hyperbolic, log10, pow, cbrt,
  // hypot. Each composes the CORDIC / Newton cores defined above; no new
  // polynomial machinery. Outputs follow the beman::inside::math conventions: angles in
  // radians, runtime-conditional failures via `std::expected<Out, errc>`,
  // statically-knowable domain limits via `static_assert`.
  //===========================================================================
  namespace detail
  {
    using namespace beman::inside::detail;

    // --- inverse trig (radians) -------------------------------------------
    // atan(v) in radians at scale 2^W: atan2(v, 1) — x = 1 > 0, so the
    // vectoring CORDIC runs with no pre-rotation. Grid-scaled (no Q.30).
    // Full domain: |v| > 1 reduces via atan(v) = sign(v)·(π/2 − atan(1/|v|)),
    // keeping the CORDIC argument inside its [-1, 1] window. Inputs with
    // |v| ≤ 1 take the original branch unchanged (bit-identical results).
    template <int W>
    constexpr rational atan_rat(rational v) noexcept
    {
      rational av = abs(v);
      if (av <= rational{1})
      {
        imax rad = cordic_atan2_rad<W, W>(to_fixed(v, W), imax{1} << W);
        return fixed_to_rational(rad, W);
      }
      rational inv = 1 / av;
      imax rad = cordic_atan2_rad<W, W>(to_fixed(inv, W), imax{1} << W);
      rational mag = kPiRat / 2 - fixed_to_rational(rad, W);
      return (v < rational{0}) ? -mag : mag;
    }

    // asin(v) = atan2(v, sqrt(1 − v²)); v ∈ [−1, 1] → result ∈ [−π/2, π/2].
    template <int W = kRefBits>
    constexpr rational asin_endpoint(rational v) noexcept
    {
      imax one = imax{1} << W;
      imax v_w = to_fixed(v, W);
      imax c_w = sqrt_fixed<W>(one - fmul(v_w, v_w, W));   // √(1−v²) ≥ 0
      if (c_w == 0) {                                                    // v = ±1 → ±π/2
        rational half_pi = kPiRat / 2;
        return (v < rational{0}) ? -half_pi : half_pi;
      }
      imax rad = cordic_atan2_rad<W, W>(v_w, c_w);        // x = c_w > 0
      return fixed_to_rational(rad, W);
    }

    // acos(v) = π/2 − asin(v); v ∈ [−1, 1] → result ∈ [0, π].
    template <int W = kRefBits>
    constexpr rational acos_endpoint(rational v) noexcept
    {
      rational half_pi = kPiRat / 2;
      return half_pi - asin_endpoint<W>(v);
    }

    // --- hyperbolic (from e^x via the exp core) ---------------------------
    // sinh/cosh = (e^v ∓ e^-v)/2, combined in fixed-point at W, not as
    // rationals: e^v and e^-v have wildly different denominators and the rational
    // cross-multiply overflows imax. At scale kRefBits each term is one scaled
    // integer (|v| ≤ 10 ⇒ e^|v|·2^30 ≤ 2.4e13, well inside int63).
    // e^|v| and e^-|v| at scale W from ONE exponential: the small term is the
    // rounded fixed-point reciprocal of the large one (2^2W ≤ 2^60 fits imax).
    template <int W>
    constexpr void exp_pair(rational v, imax& big, imax& small) noexcept
    {
      big   = to_fixed(exp_rat<W>(abs(v)), W);
      small = ((imax{1} << (2 * W)) + big / 2) / big;
    }

    template <int W = kRefBits>
    constexpr rational sinh_endpoint(rational v) noexcept
    {
      imax big, small;
      exp_pair<W>(v, big, small);
      const imax h = (big - small) / 2;
      return fixed_to_rational(v < rational{0} ? -h : h, W);
    }

    template <int W = kRefBits>
    constexpr rational cosh_endpoint(rational v) noexcept
    {
      imax big, small;
      exp_pair<W>(v, big, small);
      return fixed_to_rational((big + small) / 2, W);
    }

    // tanh via the overflow-safe form tanh(x) = (1 − e^-2|x|)/(1 + e^-2|x|),
    // odd-extended for x < 0. With u = e^-2|x| ∈ (0, 1] at scale W, the
    // quotient `((1−u)·2^W)/(1+u)` keeps the dividend bounded.
    template <int W = kRefBits>
    constexpr rational tanh_endpoint(rational v) noexcept
    {
      constexpr imax one = imax{1} << W;
      rational av = abs(v);
      imax u = to_fixed(exp_rat<W>(av * -2), W);
      imax t = ((one - u) << W) / (one + u);
      return (v < rational{0}) ? fixed_to_rational(-t, W)
                                            : fixed_to_rational(t, W);
    }

    // --- log10, cbrt ------------------------------------------------------
    template <int W>
    inline constexpr imax inv_ln10_w = inv_ln10_fixed<W>();

    template <int W = kRefBits>
    constexpr rational log10_endpoint(rational v) noexcept
    { return fixed_to_rational(fmul(log_to_fixed<W>(v), inv_ln10_w<W>, W), W); }

    // --- inverse hyperbolic (from ln via the atanh-vectoring core) ----------
    // All three are odd or one-sided, so they work on |v| and every log argument
    // is ≥ 1. asinh for |v| > 1 and acosh use ln a + ln(1 + √(1 ∓ 1/a²)): the
    // radicand stays in [0, 2] at scale W instead of squaring a (|a| ≤ 2^20).
    // Outside the domain they return a finite placeholder: the auto output type
    // is formed before the domain static_assert runs, which then reports.
    template <int W = kRefBits>
    constexpr rational asinh_endpoint(rational v) noexcept
    {
      if (v == rational{0}) return rational{0};
      const rational a = abs(v);
      if (a > rational{imax{1} << 20}) return v;                       // outside the domain
      constexpr imax one = imax{1} << W;
      imax r_w;
      if (a <= rational{1})
      {
        const imax a_w = to_fixed(a, W);
        const imax s_w = sqrt_fixed<W>(one + fmul(a_w, a_w, W));       // √(a²+1) ∈ [1, √2]
        r_w = log_to_fixed<W>(fixed_to_rational(a_w + s_w, W));
      }
      else
      {
        const imax i_w = to_fixed(rational{rational{1} / a}, W);       // 1/a ∈ (0, 1)
        const imax t_w = sqrt_fixed<W>(one + fmul(i_w, i_w, W));
        r_w = log_to_fixed<W>(a) + log_to_fixed<W>(fixed_to_rational(one + t_w, W));
      }
      const rational mag = fixed_to_rational(r_w, W);
      return (v < rational{0}) ? -mag : mag;
    }

    // acosh(v), v ≥ 1.
    template <int W = kRefBits>
    constexpr rational acosh_endpoint(rational v) noexcept
    {
      if (v <= rational{1} || v > rational{imax{1} << 20}) return rational{0};   // 1, or outside
      constexpr imax one = imax{1} << W;
      const imax i_w = to_fixed(rational{rational{1} / v}, W);          // 1/v ∈ (0, 1)
      const imax t_w = sqrt_fixed<W>(one - fmul(i_w, i_w, W));
      return fixed_to_rational(log_to_fixed<W>(v) + log_to_fixed<W>(fixed_to_rational(one + t_w, W)), W);
    }

    // atanh(v) = ½·ln((1+|v|)/(1−|v|)), odd-extended. The ratio is formed exactly
    // as a rational: a log of a tiny 1−|v| at scale W would lose its precision.
    template <int W = kRefBits>
    constexpr rational atanh_endpoint(rational v) noexcept
    {
      if (v == rational{0}) return rational{0};
      const rational a = abs(v);
      if (a > rational{1} - rational{1, imax{1} << 30}) return v;     // outside the domain
      const rational q = rational{(rational{1} + a) / (rational{1} - a)};
      const rational mag = fixed_to_rational(log_to_fixed<W>(q) / 2, W);
      return (v < rational{0}) ? -mag : mag;
    }

    // cbrt(v) = sign(v)·e^(ln|v|/3); cbrt(0) = 0.
    template <int W = kRefBits>
    constexpr rational cbrt_endpoint(rational v) noexcept
    {
      if (v == rational{0}) return rational{0};
      rational av = abs(v);
      rational mag = exp_from_fixed<W>(log_to_fixed<W>(av) / 3);
      return (v < rational{0}) ? -mag : mag;
    }

    // hypot(x, y) = sqrt(x²+y²), computed as m·sqrt((x/m)²+(y/m)²) with
    // m = max(|x|,|y|) so the radicand stays in [1, 2]. Exact rational scaling;
    // reuses the grid-scaled sqrt_fixed.
    template <int W = kRefBits>
    constexpr rational hypot_endpoint(rational x,
                                                   rational y) noexcept
    {
      rational ax = abs(x);
      rational ay = abs(y);
      rational m  = (ax > ay) ? ax : ay;
      if (m == rational{0}) return rational{0};
      // Form the radicand (x/m)²+(y/m)² ∈ [1, 2] at scale kRefBits — keeping it
      // a rational overflows imax (the squared numerators cross-multiply to
      // ~1e24). Each ratio is ≤ 1, so its fixed-point square fits comfortably.
      imax rx = to_fixed(x / m, W);
      imax ry = to_fixed(y / m, W);
      imax s_w = fmul(rx, rx, W) + fmul(ry, ry, W);
      const imax root_w = sqrt_fixed<W>(s_w);
      // Exact rational product when it fits; a wide-denominator m can push m·root
      // past imax, so fall back to the fixed-point form (|m| ≤ 2^20 envelope).
      if (auto exact = m * fixed_to_rational(root_w, W))
        return *exact;
      return fixed_to_rational(fmul(to_fixed(m, W), root_w, W),
                               W);
    }

    // pow(b, e) = 2^(e·log2(b)), b > 0. The exponent e·log2(b) is saturated into
    // exp2's [−30, 30] envelope so the power-of-two denominator never UB-shifts;
    // the runtime impl reports envelope overflow via `expected`.
    constexpr rational pow_endpoint(rational b,
                                                 rational e) noexcept
    {
      imax sc_w = fmul(to_fixed(e, kRefBits), log2_to_fixed<kRefBits>(b), kRefBits);
      constexpr imax lim = imax{30} << kRefBits;
      sc_w = (sc_w > lim) ? lim : (sc_w < -lim) ? -lim : sc_w;
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // --- deduction aliases ------------------------------------------------
    // Monotonic-increasing functions round endpoints outward like the exp/log
    // family. acos is decreasing; cosh is even (min at 0 if the interval
    // spans it). round_nearest lands sub-notch drift onto the grid.
    template <insidable In>
    using atan_auto_t = outward_t<In, atan_rat<working_bits<In>()>(lower_of<In>), atan_rat<working_bits<In>()>(upper_of<In>)>;

    template <insidable In>
    using asin_auto_t = outward_t<In, asin_endpoint(lower_of<In>), asin_endpoint(upper_of<In>)>;

    template <insidable In>
    using acos_auto_t = outward_t<In, acos_endpoint(upper_of<In>), acos_endpoint(lower_of<In>)>;

    template <insidable In>
    using sinh_auto_t = outward_t<In, sinh_endpoint(lower_of<In>), sinh_endpoint(upper_of<In>)>;

    template <insidable In>
    inline constexpr rational cosh_auto_lo =
      (lower_of<In> <= rational{0} && upper_of<In> >= rational{0})
        ? rational{1}
        : (cosh_endpoint(lower_of<In>) < cosh_endpoint(upper_of<In>)
             ? cosh_endpoint(lower_of<In>) : cosh_endpoint(upper_of<In>));

    template <insidable In>
    inline constexpr rational cosh_auto_hi =
      (cosh_endpoint(lower_of<In>) > cosh_endpoint(upper_of<In>))
        ? cosh_endpoint(lower_of<In>) : cosh_endpoint(upper_of<In>);

    template <insidable In>
    using cosh_auto_t = outward_t<In, cosh_auto_lo<In>, cosh_auto_hi<In>>;

    template <insidable In>
    using tanh_auto_t = outward_t<In, tanh_endpoint(lower_of<In>), tanh_endpoint(upper_of<In>)>;

    // asinh / acosh / atanh are increasing: the output spans the endpoint images.
    template <insidable In>
    using asinh_auto_t = outward_t<In, asinh_endpoint(lower_of<In>), asinh_endpoint(upper_of<In>)>;
    template <insidable In>
    using acosh_auto_t = outward_t<In, acosh_endpoint(lower_of<In>), acosh_endpoint(upper_of<In>)>;
    template <insidable In>
    using atanh_auto_t = outward_t<In, atanh_endpoint(lower_of<In>), atanh_endpoint(upper_of<In>)>;

    template <insidable In>
    using log10_auto_t = outward_t<In, log10_endpoint(lower_of<In>), log10_endpoint(upper_of<In>)>;

    template <insidable In>
    using cbrt_auto_t = outward_t<In, cbrt_endpoint(lower_of<In>), cbrt_endpoint(upper_of<In>)>;

    // hypot output: non-negative, Upper at the largest-magnitude corner.
    template <insidable InX, insidable InY>
    inline constexpr rational hypot_auto_hi =
      hypot_endpoint(
        (abs(lower_of<InX>) > abs(upper_of<InX>)
           ? abs(lower_of<InX>) : abs(upper_of<InX>)),
        (abs(lower_of<InY>) > abs(upper_of<InY>)
           ? abs(lower_of<InY>) : abs(upper_of<InY>)));

    template <insidable InX, insidable InY>
    using hypot_auto_t = inside<{{rational{0},
                                 ceil_to_notch(hypot_auto_hi<InX, InY>, gcd_notch<InX, InY>)},
                                gcd_notch<InX, InY>}, out_policy<InX> | round_nearest>;

    // pow output: extrema of b^e over the input rectangle occur at corners
    // (monotone in each argument for b > 0). Min and max of the 4 corners.
    template <insidable InB, insidable InE>
    inline constexpr rational pow_corner[4] = {
      pow_endpoint(lower_of<InB>, lower_of<InE>), pow_endpoint(lower_of<InB>, upper_of<InE>),
      pow_endpoint(upper_of<InB>, lower_of<InE>), pow_endpoint(upper_of<InB>, upper_of<InE>),
    };

    template <insidable InB, insidable InE>
    inline constexpr rational pow_auto_lo = []{
      rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] < m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <insidable InB, insidable InE>
    inline constexpr rational pow_auto_hi = []{
      rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] > m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <insidable InB, insidable InE>
    using pow_auto_t = inside<{{floor_to_notch(pow_auto_lo<InB, InE>, notch_of<InB>),
                               ceil_to_notch (pow_auto_hi<InB, InE>, notch_of<InB>)},
                              notch_of<InB>}, out_policy<InB> | round_nearest>;
  } // namespace detail

  // --- explicit-Out impls -------------------------------------------------
  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out atan_into(In x)
  {
    detail::domain_atan<In>();
    return detail::store_grid<Out>(detail::atan_rat<detail::working_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out asin_into(In x)
  {
    detail::domain_asin<In>();
    return detail::store_grid<Out>(detail::asin_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out acos_into(In x)
  {
    detail::domain_acos<In>();
    return detail::store_grid<Out>(detail::acos_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sinh_into(In x)
  {
    detail::domain_sinh<In>();
    return detail::store_grid<Out>(detail::sinh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out asinh_into(In x)
  {
    detail::domain_asinh<In>();
    return detail::store_grid<Out>(detail::asinh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out acosh_into(In x)
  {
    detail::domain_acosh<In>();
    return detail::store_grid<Out>(detail::acosh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out atanh_into(In x)
  {
    detail::domain_atanh<In>();
    return detail::store_grid<Out>(detail::atanh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cosh_into(In x)
  {
    detail::domain_cosh<In>();
    static_assert(lower_of<Out> <= rational{1},
                  "beman::inside::math::cosh: Out must include 1 (cosh ≥ 1)");
    return detail::store_grid<Out>(detail::cosh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out tanh_into(In x)
  {
    detail::domain_tanh<In>();
    return detail::store_grid<Out>(detail::tanh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log10_into(In x)
  {
    detail::domain_log10<In>();
    return detail::store_grid<Out>(detail::log10_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cbrt_into(In x)
  {
    detail::domain_cbrt<In>();
    return detail::store_grid<Out>(detail::cbrt_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }
  } // namespace cordic

  namespace cordic {
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out hypot_into(InX x, InY y)
  {
    detail::domain_hypot<InX, InY>();
    static_assert(lower_of<Out> <= 0, "beman::inside::math::hypot: Out must include 0");
    return detail::store_grid<Out>(detail::hypot_endpoint<detail::endpoint_bits<Out>()>(rational{x}, rational{y}));
  }
  } // namespace cordic

  // pow: b^e for runtime base b > 0. Returns `expected` — `overflow` when
  // e·log2(b) leaves exp2's [-30, 30] envelope or the result leaves Out's
  // interval. The auto form requires lower_of<InB> > 0 (so b > 0 is guaranteed
  // and the output range is bounded for deduction).
  namespace cordic {
  template <insidable Out, insidable InB, insidable InE>
  [[nodiscard]] constexpr std::expected<Out, errc> pow_into(InB base, InE exp)
  {
    rational bv = base;
    if (bv <= rational{0})
      return std::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax sc_w = detail::fmul(detail::to_fixed(rational{exp}, W),
                             detail::log2_to_fixed<W>(bv), W);     // e·log2(b), scale 2^W
    constexpr imax lim = imax{30} << W;
    if (sc_w > lim || sc_w < -lim)                      // outside 2^±30: every engine
    {                                                   // reports, or clamp saturates
      if constexpr (has_flag(policy_of<Out>, clamp))
        return detail::store_grid<Out>(sc_w > 0 ? upper_of<Out> : lower_of<Out>);
      else
        return std::unexpected(errc::overflow);
    }

    rational r = detail::exp2_from_fixed<W>(sc_w);
    if constexpr (!has_flag(policy_of<Out>, clamp))   // clamp Out: saturate below
      if (r < lower_of<Out> || r > upper_of<Out>)
        return std::unexpected(errc::overflow);
    return detail::store_grid<Out>(r);
  }
  } // namespace cordic

  //===========================================================================
  // Explicit engine namespaces — call a chosen engine regardless of the build
  // default. `cordic::fn` (integer/CORDIC, ALWAYS present) and `dbl::fn` (the
  // double engine, present unless BEMAN_INSIDE_MATH_NO_FP) expose the SAME public-shaped
  // API as the top-level `beman::inside::math::fn` — same signatures, domains, auto-deduced
  // output grids, and domain static_asserts. The unqualified `beman::inside::math::fn` is
  // an alias for whichever engine the build selects (see the #ifdef dispatch
  // above); these let a single binary mix both — e.g. `cordic::sin` on a
  // determinism-critical path and `dbl::sin` on a hot one.
  //
  // The engines are independent approximations: a grid-snapped value can differ
  // between them by up to one notch on rounding ties (table-maker's dilemma).
  // Don't compare outputs across engines — see determinism.md.
  //===========================================================================
  namespace cordic
  {
    template <insidable In>
      requires (lower_of<In> == rational{0})
    [[nodiscard]] constexpr auto sqrt(In x)
    { static_assert(detail::require_snap<In>()); return sqrt_into<detail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (lower_of<In> < rational{0})
    [[nodiscard]] constexpr auto sqrt(In x)
    { static_assert(detail::require_snap<In>()); return sqrt_into<detail::sqrt_signed_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto exp2(In x)
    { static_assert(detail::require_snap<In>()); return exp2_into<detail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log2(In x)
    {
      static_assert(detail::require_snap<In>());
      static_assert(lower_of<In> > 0, "beman::inside::math::cordic::log2: input must be strictly positive");
      return log2_into<detail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto exp(In x)
    { static_assert(detail::require_snap<In>()); return exp_into<detail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log(In x)
    {
      static_assert(detail::require_snap<In>());
      static_assert(lower_of<In> > 0, "beman::inside::math::cordic::log: input must be strictly positive");
      return log_into<detail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] constexpr auto pow_base(In x)
    {
      static_assert(detail::require_snap<In>());
      detail::domain_pow_base<Base, In>();
      if constexpr (detail::pow_base_domain_ok<Base, In>)   // no deduction outside the domain
        return pow_base_into<detail::pow_base_auto_t<Base, In>, Base>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto sin(In angle)
    { static_assert(detail::require_snap<In>()); return sin_into<detail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto cos(In angle)
    { static_assert(detail::require_snap<In>()); return cos_into<detail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto tan(In angle)
    { static_assert(detail::require_snap<In>()); return tan_into<detail::tan_auto_t<In>>(angle); }

    template <insidable InY, insidable InX>
    [[nodiscard]] constexpr auto atan2(InY y, InX x)
    {
      static_assert(detail::require_snap<InY>() && detail::require_snap<InX>());
      return atan2_into<detail::atan2_auto_t<InY, InX>>(y, x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto atan(In x)
    { static_assert(detail::require_snap<In>()); return atan_into<detail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto asin(In x)
    { static_assert(detail::require_snap<In>()); return asin_into<detail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto acos(In x)
    { static_assert(detail::require_snap<In>()); return acos_into<detail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto sinh(In x)
    { static_assert(detail::require_snap<In>()); return sinh_into<detail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto cosh(In x)
    { static_assert(detail::require_snap<In>()); return cosh_into<detail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto tanh(In x)
    { static_assert(detail::require_snap<In>()); return tanh_into<detail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto asinh(In x)
    { static_assert(detail::require_snap<In>()); return asinh_into<detail::asinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto acosh(In x)
    { static_assert(detail::require_snap<In>()); return acosh_into<detail::acosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto atanh(In x)
    { static_assert(detail::require_snap<In>()); return atanh_into<detail::atanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log10(In x)
    {
      static_assert(detail::require_snap<In>());
      static_assert(lower_of<In> > 0, "beman::inside::math::cordic::log10: input must be strictly positive");
      return log10_into<detail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto cbrt(In x)
    { static_assert(detail::require_snap<In>()); return cbrt_into<detail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] constexpr auto hypot(InX x, InY y)
    {
      static_assert(detail::require_snap<InX>() && detail::require_snap<InY>());
      return hypot_into<detail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (lower_of<InB> > rational{0})
    [[nodiscard]] constexpr auto pow(InB base, InE exp)
    {
      static_assert(detail::require_snap<InB>() && detail::require_snap<InE>());
      return pow_into<detail::pow_auto_t<InB, InE>>(base, exp);
    }
  } // namespace cordic

  // The shared deduction/helpers, as seen from the engine namespaces (where a
  // bare `detail::` names the engine's own cores).
  namespace mdetail = beman::inside::math::detail;

#ifndef BEMAN_INSIDE_MATH_NO_FP
  //---------------------------------------------------------------------------
  // The FP engines' public API, written once for both float types. Each
  // function has an explicit-output form `fn_into<Out>` (domain-checked) and
  // the auto form `fn`, which deduces Out and calls it. Inside each namespace
  // `fp_t` is the engine's float type, `to_fp` reads an inside as one, `detail::`
  // names the engine's own cores and `mdetail::` the shared helpers.
  //---------------------------------------------------------------------------
#define BEMAN_INSIDE_FP_UNARY(fn)                                                         \
    template <insidable Out, insidable In>                                                \
    [[nodiscard]] BEMAN_INSIDE_FP_FN Out fn##_into(In x)                                 \
    { mdetail::domain_##fn<In>(); return detail::fn##_core<Out>(x); }                             \
    template <insidable In>                                                               \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto fn(In x)                                       \
    { static_assert(mdetail::require_snap<In>()); return fn##_into<mdetail::fn##_auto_t<In>>(x); }

#define BEMAN_INSIDE_FP_ENGINE_API                                                        \
    BEMAN_INSIDE_FP_UNARY(exp2)  BEMAN_INSIDE_FP_UNARY(log2)  BEMAN_INSIDE_FP_UNARY(exp)  \
    BEMAN_INSIDE_FP_UNARY(log)   BEMAN_INSIDE_FP_UNARY(log10) BEMAN_INSIDE_FP_UNARY(sin)  \
    BEMAN_INSIDE_FP_UNARY(cos)   BEMAN_INSIDE_FP_UNARY(atan)  BEMAN_INSIDE_FP_UNARY(asin) \
    BEMAN_INSIDE_FP_UNARY(acos)  BEMAN_INSIDE_FP_UNARY(sinh)  BEMAN_INSIDE_FP_UNARY(cosh) \
    BEMAN_INSIDE_FP_UNARY(tanh)  BEMAN_INSIDE_FP_UNARY(cbrt)                              \
    BEMAN_INSIDE_FP_UNARY(asinh) BEMAN_INSIDE_FP_UNARY(acosh) BEMAN_INSIDE_FP_UNARY(atanh) \
                                                                                          \
    template <insidable Out, insidable In> requires (lower_of<In> == rational{0})         \
    [[nodiscard]] BEMAN_INSIDE_FP_FN Out sqrt_into(In x) { return detail::sqrt_core<Out>(x); }   \
    template <insidable Out, insidable In> requires (lower_of<In> < rational{0})          \
    [[nodiscard]] BEMAN_INSIDE_FP_FN std::expected<Out, errc> sqrt_into(In x)            \
    {                                                                                     \
      const fp_t v = to_fp(x);                                                            \
      if (v < fp_t{0}) return std::unexpected(errc::domain_error);                        \
      return detail::store<Out>(detail::fp_sqrt(v));                                               \
    }                                                                                     \
    template <insidable In> requires (lower_of<In> == rational{0})                        \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto sqrt(In x)                                     \
    { static_assert(mdetail::require_snap<In>()); return sqrt_into<mdetail::sqrt_auto_t<In>>(x); } \
    template <insidable In> requires (lower_of<In> < rational{0})                         \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto sqrt(In x)                                     \
    { static_assert(mdetail::require_snap<In>()); return sqrt_into<mdetail::sqrt_signed_auto_t<In>>(x); } \
                                                                                          \
    template <insidable Out, insidable In>                                                \
    [[nodiscard]] BEMAN_INSIDE_FP_FN std::expected<Out, errc> tan_into(In angle)         \
    {                                                                                     \
      mdetail::domain_tan<In>();                                                          \
      fp_t t;                                                                             \
      if (!detail::fp_tan(to_fp(angle), t)) return std::unexpected(errc::division_by_zero); \
      if constexpr (!has_flag(policy_of<Out>, clamp))   /* clamp Out: saturate below */   \
        if (t < mdetail::lower_fp<fp_t, Out> || t > mdetail::upper_fp<fp_t, Out>)         \
          return std::unexpected(errc::overflow);                                         \
      return detail::store<Out>(t);                                                               \
    }                                                                                     \
    template <insidable In>                                                               \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto tan(In angle)                                  \
    { static_assert(mdetail::require_snap<In>()); return tan_into<mdetail::tan_auto_t<In>>(angle); } \
                                                                                          \
    template <insidable Out, imax Base, insidable In>                                     \
    [[nodiscard]] BEMAN_INSIDE_FP_FN Out pow_base_into(In x)                             \
    {                                                                                     \
      static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be ≥ 2");        \
      const fp_t r = detail::fp_pow(static_cast<fp_t>(Base), to_fp(x));                   \
      if (!(r <= static_cast<fp_t>(0x1p30) && r >= static_cast<fp_t>(0x1p-30)))           \
        return mdetail::pow_envelope_fail<Out>(r > fp_t{1});  /* the 2^±30 envelope */    \
      return detail::store<Out>(r);                                                       \
    }                                                                                     \
    template <imax Base, insidable In>                                                    \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto pow_base(In x)                                 \
    {                                                                                     \
      static_assert(mdetail::require_snap<In>());                                         \
      mdetail::domain_pow_base<Base, In>();                                               \
      if constexpr (mdetail::pow_base_domain_ok<Base, In>)                                \
        return pow_base_into<mdetail::pow_base_auto_t<Base, In>, Base>(x);                \
    }                                                                                     \
                                                                                          \
    template <insidable Out, insidable InY, insidable InX>                                \
    [[nodiscard]] BEMAN_INSIDE_FP_FN Out atan2_into(InY y, InX x)                        \
    { mdetail::domain_atan2<InY>(); mdetail::domain_atan2<InX>(); return detail::atan2_core<Out>(y, x); } \
    template <insidable InY, insidable InX>                                               \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto atan2(InY y, InX x)                            \
    {                                                                                     \
      static_assert(mdetail::require_snap<InY>() && mdetail::require_snap<InX>());        \
      return atan2_into<mdetail::atan2_auto_t<InY, InX>>(y, x);                           \
    }                                                                                     \
                                                                                          \
    template <insidable Out, insidable InX, insidable InY>                                \
    [[nodiscard]] BEMAN_INSIDE_FP_FN Out hypot_into(InX x, InY y)                        \
    { mdetail::domain_hypot<InX, InY>(); return detail::hypot_core<Out>(x, y); }                  \
    template <insidable InX, insidable InY>                                               \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto hypot(InX x, InY y)                            \
    {                                                                                     \
      static_assert(mdetail::require_snap<InX>() && mdetail::require_snap<InY>());        \
      return hypot_into<mdetail::hypot_auto_t<InX, InY>>(x, y);                           \
    }                                                                                     \
                                                                                          \
    template <insidable Out, insidable InB, insidable InE>                                \
    [[nodiscard]] BEMAN_INSIDE_FP_FN std::expected<Out, errc> pow_into(InB base, InE exp) \
    {                                                                                     \
      const fp_t b = to_fp(base);                                                         \
      if (b <= fp_t{0}) return std::unexpected(errc::domain_error);                       \
      const fp_t r = detail::fp_pow(b, to_fp(exp));                                        \
      if (!(r <= static_cast<fp_t>(0x1p30) && r >= static_cast<fp_t>(0x1p-30)))           \
      {                                       /* the 2^±30 envelope, as cordic */         \
        if constexpr (has_flag(policy_of<Out>, clamp))                                    \
          return detail::store<Out>(r > fp_t{1} ? mdetail::upper_fp<fp_t, Out> : mdetail::lower_fp<fp_t, Out>); \
        else                                                                              \
          return std::unexpected(errc::overflow);                                         \
      }                                                                                   \
      if constexpr (!has_flag(policy_of<Out>, clamp))   /* clamp Out: saturate below */   \
        if (r < mdetail::lower_fp<fp_t, Out> || r > mdetail::upper_fp<fp_t, Out>)         \
          return std::unexpected(errc::overflow);                                         \
      return detail::store<Out>(r);                                                               \
    }                                                                                     \
    template <insidable InB, insidable InE> requires (lower_of<InB> > rational{0})        \
    [[nodiscard]] BEMAN_INSIDE_FP_FN auto pow(InB base, InE exp)                         \
    {                                                                                     \
      static_assert(mdetail::require_snap<InB>() && mdetail::require_snap<InE>());        \
      return pow_into<mdetail::pow_auto_t<InB, InE>>(base, exp);                          \
    }

  namespace dbl
  {
    using fp_t = double;
    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_FP_FN fp_t to_fp(In x) { return static_cast<double>(x); }
    BEMAN_INSIDE_FP_ENGINE_API
  } // namespace dbl

  namespace flt
  {
    using fp_t = float;
    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_FP_FN fp_t to_fp(In x) { return detail::to_float(x); }
    BEMAN_INSIDE_FP_ENGINE_API
  } // namespace flt
#undef BEMAN_INSIDE_FP_ENGINE_API
#undef BEMAN_INSIDE_FP_UNARY
#endif // !BEMAN_INSIDE_MATH_NO_FP

  //---------------------------------------------------------------------------
  // The unqualified API (`beman::inside::math::sin` etc.) is the build's default
  // engine: CORDIC under BEMAN_INSIDE_MATH_NO_FP, float under
  // BEMAN_INSIDE_MATH_FLOAT, else double. Every engine stays reachable by name.
  // Trig takes radians (std::sin-shaped; the turn-input workers are internal).
  // sqrt of a mixed-sign input, tan and pow return std::expected<inside, errc>
  // (domain_error / division_by_zero / overflow) instead of UB.
  //---------------------------------------------------------------------------
#if defined(BEMAN_INSIDE_MATH_NO_FP)
  namespace default_engine = cordic;
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
  namespace default_engine = flt;
#else
  namespace default_engine = dbl;
#endif
  using default_engine::sqrt;
  using default_engine::sqrt_into;
  using default_engine::exp2_into;
  using default_engine::log2_into;
  using default_engine::exp_into;
  using default_engine::log_into;
  using default_engine::pow_base_into;
  using default_engine::sin_into;
  using default_engine::cos_into;
  using default_engine::tan_into;
  using default_engine::atan2_into;
  using default_engine::atan_into;
  using default_engine::asin_into;
  using default_engine::acos_into;
  using default_engine::sinh_into;
  using default_engine::cosh_into;
  using default_engine::tanh_into;
  using default_engine::asinh_into;
  using default_engine::acosh_into;
  using default_engine::atanh_into;
  using default_engine::log10_into;
  using default_engine::cbrt_into;
  using default_engine::hypot_into;
  using default_engine::pow_into;
  using default_engine::exp2;
  using default_engine::log2;
  using default_engine::exp;
  using default_engine::log;
  using default_engine::pow_base;
  using default_engine::sin;
  using default_engine::cos;
  using default_engine::tan;
  using default_engine::atan2;
  using default_engine::atan;
  using default_engine::asin;
  using default_engine::acos;
  using default_engine::sinh;
  using default_engine::cosh;
  using default_engine::tanh;
  using default_engine::asinh;
  using default_engine::acosh;
  using default_engine::atanh;
  using default_engine::log10;
  using default_engine::cbrt;
  using default_engine::hypot;
  using default_engine::pow;
}


// ======================================================================
//  beman/inside/formats.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


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
namespace beman::inside
{
  //-------------------------------------------------------------------------
  // Native integer widths — direct storage (Raw == value), `checked`.
  // Full native range.
  //-------------------------------------------------------------------------
  using byte  = inside<{0, 255}>;                         // uint8
  using word  = inside<{0, 65535}>;                       // uint16
  using dword = inside<{0, 4294967295}>;                  // uint32
  // qword reaches past int64, so it has no implicit `operator imax`; read it
  // with `to<std::uint64_t>()`. A difference of qwords spans 2^65 values and
  // gets a wide-integer index; a sum's upper bound passes the 64-bit grid
  // numbers, so it needs C++26 big grids.
  using qword = inside<{0, 18446744073709551615u}>;       // uint64

  using sbyte  = inside<{-128, 127}>;                      // int8
  using sword  = inside<{-32768, 32767}>;                  // int16
  using sdword = inside<{-2147483648, 2147483647}>;        // int32
  // sqword stays symmetric: the internal value path is `imax`, and -2^63
  // has no negation in int64.
  using sqword = inside<{-9223372036854775807, 9223372036854775807}>; // int64

  //-------------------------------------------------------------------------
  // Unsigned normalized (UNORM) — [0, 1] at N-bit resolution, `round_nearest`.
  // The notch denominator is the type max, so the index 0..max fills the native
  // width; both endpoints (0 and 1) are exactly representable.
  //-------------------------------------------------------------------------
  using unorm8  = inside<{{0, 1}, per<255>},        round_nearest>; // uint8
  using unorm16 = inside<{{0, 1}, per<65535>},      round_nearest>; // uint16
  using unorm32 = inside<{{0, 1}, per<4294967295>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
  // full natural range. `round_nearest`.
  //-------------------------------------------------------------------------
  using q4_4   = inside<{{0, 15},    per<16>},    round_nearest>; // uint8
  using q8_8   = inside<{{0, 255},   per<256>},   round_nearest>; // uint16
  using q16_16 = inside<{{0, 65535}, per<65536>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Counters — a counter is an inside over [0, Max] whose overflow policy says
  // what `++` does at the ceiling (the boundary behavior is in the type).
  //-------------------------------------------------------------------------
  // Saturating counter: `++` caps at Max (never throws or wraps) — the honest
  // "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
  template <umax Max> using counter      = inside<{0, Max}, clamp>;
  // Modular / ring counter: `++` wraps Max → 0 (sequence numbers, epochs).
  template <umax Max> using ring_counter = inside<{0, Max}, wrap>;

} // namespace beman::inside


#ifndef BEMAN_INSIDE_NO_STRING

// ======================================================================
//  beman/inside/io.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
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

namespace beman::inside
{
  //-------------------------------------------------------------------------
  // to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
  // for plain arithmetic types and the exact-rational form for insidables.
  //-------------------------------------------------------------------------
  [[nodiscard]] inline std::string to_string(beman::inside::detail::rational r)
  {
    std::string str;
    if (r.Denominator < 0)
      str = "-";

    umax ad = detail::abs_den(r.Denominator);
    if (ad == 1)
      return str += std::to_string(r.Numerator);

    // power-of-2 or power-of-10: decimal output
    // find smallest 10^k divisible by ad
    umax pow10 = 1;
    unsigned digits = 0;
    bool is_decimal = false;
    for (unsigned k = 0; k < 20; ++k)
    {
      if (pow10 % ad == 0)
      { is_decimal = true; digits = k; break; }
      pow10 *= 10;
    }

    if (is_decimal)
    {
      umax scale = pow10 / ad;
      umax total;
      if (!mul_overflow(r.Numerator, scale, &total))
      {
        umax int_part = total / pow10;
        umax frac_part = total % pow10;
        str += std::to_string(int_part);
        if (digits > 0)
        {
          str += ".";
          auto frac_str = std::to_string(frac_part);
          // zero-pad
          for (unsigned i = 0; i < digits - frac_str.size(); ++i)
            str += "0";
          str += frac_str;
        }
        return str;
      }
      // Decimal expansion would overflow the umax scratch buffer. Fall back
      // silently to the mixed-number/fraction form — `to_string` must always
      // produce *some* readable output, never an error.
    }

    // mixed number for improper fractions
    umax int_part = r.Numerator / ad;
    umax remainder = r.Numerator % ad;
    if (int_part > 0)
    {
      str += std::to_string(int_part);
      if (remainder > 0)
      {
        str += " ";
        str += std::to_string(remainder);
        str += "/";
        str += std::to_string(ad);
      }
    }
    else
    {
      str += std::to_string(r.Numerator);
      str += "/";
      str += std::to_string(ad);
    }
    return str;
  }

  [[nodiscard]] inline std::string to_string(interval ival)
  {
    std::string str{"["};

    str += beman::inside::to_string(ival.Lower);
    str += "..";
    str += beman::inside::to_string(ival.Upper);
    str += "]";
    return str;
  }

  [[nodiscard]] inline std::string to_string(grid g)
  {
    std::string str{"{"};

    str += beman::inside::to_string(g.Interval);
    str += ", ";
    str += beman::inside::to_string(g.Notch);
    str += "}";
    return str;
  }

  // delegate to std::to_string
  template <typename V>
  [[nodiscard]] auto to_string(V value)
  { return std::to_string(value); }

  // `f64` (double-backed) and `exact` (rational-backed) insides: render the
  // exact rational form. (Without this overload a f64 inside would fall to the
  // generic `std::to_string(double)` and print a lossy 6-digit form, and a
  // rational-raw inside has no std::to_string at all.) A continuous (Notch == 0)
  // f64 inside prints the double.
  template <insidable B>
    requires (detail::fp_raw<B> || detail::rational_raw<B>)
  [[nodiscard]] inline std::string to_string(B b)
  {
    if constexpr (detail::fp_raw<B> && notch_of<B> == beman::inside::detail::rational{0})
      return std::to_string(detail::as_double(b));
    else
      return to_string(beman::inside::detail::as_rational(b));
  }

  //-------------------------------------------------------------------------
  // type_name<T>() — short raw-type label for to_string_debug. Lives here (not
  // in the core math header) so the core never pulls <string_view>.
  //-------------------------------------------------------------------------
  namespace detail
  {
    template <typename T>
    constexpr std::string_view type_name()
    {
      if constexpr (std::is_same_v<T, std::uint8_t>)  return "uint8_t";
      if constexpr (std::is_same_v<T, std::uint16_t>) return "uint16_t";
      if constexpr (std::is_same_v<T, std::uint32_t>) return "uint32_t";
      if constexpr (std::is_same_v<T, std::uint64_t>) return "uint64_t";
      if constexpr (std::is_same_v<T, std::int8_t>)   return "int8_t";
      if constexpr (std::is_same_v<T, std::int16_t>)  return "int16_t";
      if constexpr (std::is_same_v<T, std::int32_t>)  return "int32_t";
      if constexpr (std::is_same_v<T, std::int64_t>)  return "int64_t";
      if constexpr (std::is_same_v<T, rational>) return "rational";
      if constexpr (std::is_same_v<T, point_slot>) return "point";
      if constexpr (std::is_same_v<T, wide_uint<2>>) return "wide_uint<2>";
      if constexpr (std::is_same_v<T, wide_uint<3>>) return "wide_uint<3>";
      if constexpr (is_wide_int_v<T>) return "wide_int";
      return "unknown";
    }
  } // namespace detail

  //-------------------------------------------------------------------------
  // to_string / to_string_debug / operator<< for insidables and rational.
  // The debug form also prints the raw value, raw type, and grid — useful when
  // inspecting failing tests or storage choices.
  //-------------------------------------------------------------------------
  namespace detail
  {
    // Decimal digits of a wide integer: 19 digits per division by 10^19.
    template <std::size_t N, bool S>
    std::string wide_to_decimal(wide_int<N, S> v)
    {
      const bool neg = v.negative();
      wide_int<N, false> u(neg ? -v : v);
      const wide_int<N, false> chunk{10'000'000'000'000'000'000ull};
      std::string out;
      do
      {
        const auto [q, r] = wide_int<N, false>::divmod(u, chunk);
        std::string part = std::to_string(static_cast<umax>(r));
        if (!q.is_zero()) part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
        u = q;
      } while (!u.is_zero());
      return neg ? "-" + out : out;
    }

    // A wide-index value: the usual rational form when it fits, else the
    // reduced fraction num/den in decimal.
    inline std::string exact_to_string(exact_frac f)
    {
      if (const auto r = try_rational(f)) return beman::inside::to_string(*r);
      exact_int x = f.Num.negative() ? -f.Num : f.Num, y = f.Den;
      while (!y.is_zero()) { const exact_int t = x % y; x = y; y = t; }
      const exact_int num = f.Num / x, den = f.Den / x;
      return den == exact_int{1} ? wide_to_decimal(num) : wide_to_decimal(num) + "/" + wide_to_decimal(den);
    }
  }

  template <std::size_t N, bool S>
  [[nodiscard]] inline std::string to_string(detail::wide_int<N, S> v) { return detail::wide_to_decimal(v); }

  template <insidable B>
  [[nodiscard]] inline std::string to_string(B b)
  {
    if constexpr (detail::wide_raw<B>) return detail::exact_to_string(detail::exact_of(b));
    else                               return beman::inside::to_string(detail::as_rational(b));
  }

  template <insidable B>
  [[nodiscard]] inline std::string to_string_debug(B b)
  {
    std::string str;
    str += beman::inside::to_string(b);
    str += " {";
    str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name<detail::raw_t<B>>());
    str += " Max:" + beman::inside::to_string(grid_of<B>.slot_count()) + "] ";
    str += beman::inside::to_string(grid_of<B>);
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, beman::inside::detail::rational r)
  {
    stream << beman::inside::to_string(r);
    return stream;
  }

  template <insidable B>
  inline std::ostream& operator<<(std::ostream& stream, B b)
  {
    stream << beman::inside::to_string(b);
    return stream;
  }

  // from_chars<B>(text) — the std::string_view form of from_chars<B>(first, last).
  template <insidable B>
  [[nodiscard]] constexpr std::expected<B, errc> from_chars(std::string_view text)
  { return from_chars<B>(text.data(), text.data() + text.size()); }

  // Reads one whitespace-delimited token and parses it with from_chars<B>. On an
  // error the stream's failbit is set and `b` is left unchanged.
  template <insidable B>
  inline std::istream& operator>>(std::istream& stream, B& b)
  {
    std::string token;
    if (!(stream >> token)) return stream;
    if (const auto r = from_chars<B>(token); r) b = *r;
    else stream.setstate(std::ios_base::failbit);
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

namespace beman::inside::detail
{
  // Shared spec handling: an empty `{}` is left to the derived format() (exact
  // to_string); a non-empty spec is parsed and applied by `Numeric`.
  template <class Inner>
  struct numeric_spec_formatter
  {
    Inner Numeric{};
    bool  HasSpec = false;

    constexpr auto parse(std::format_parse_context& ctx)
    {
      auto it = ctx.begin();
      if (it != ctx.end() && *it != '}')
      {
        HasSpec = true;
        return Numeric.parse(ctx);
      }
      return it;
    }
  };
}

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
  : beman::inside::detail::numeric_spec_formatter<
      std::conditional_t<beman::inside::detail::is_integer_aligned<beman::inside::inside<G, P>> && G.Notch != 0
                         && beman::inside::detail::values_fit_imax<beman::inside::inside<G, P>>,
                         std::formatter<beman::inside::imax>,
                         std::formatter<double>>>
{
  using B = beman::inside::inside<G, P>;
  // Integer formatting only for a notched integer grid: a continuous grid
  // (notch 0) holds fractions even between integer bounds.
  static constexpr bool integer_path = beman::inside::detail::is_integer_aligned<B> && G.Notch != 0
                                    && beman::inside::detail::values_fit_imax<B>;

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return this->Numeric.format(beman::inside::detail::to_value(b), ctx);
    else if (this->HasSpec)
      return this->Numeric.format(beman::inside::detail::as_double(b), ctx);
    else
      return std::format_to(ctx.out(), "{}", beman::inside::to_string(b));
  }
};

template <>
struct std::formatter<beman::inside::detail::rational>
  : beman::inside::detail::numeric_spec_formatter<std::formatter<double>>
{
  template <typename Ctx>
  auto format(beman::inside::detail::rational const& r, Ctx& ctx) const
  {
    if (HasSpec)
      return Numeric.format(static_cast<double>(r), ctx);
    return std::format_to(ctx.out(), "{}", beman::inside::to_string(r));
  }
};

#endif // __cpp_lib_format


#endif // BEMAN_INSIDE_NO_STRING

// ======================================================================
//  beman/inside/numeric_limits.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specialisations for inside<G, P>.
// numeric_limits reports the *grid* bounds (Lower/Upper), not the raw type's
// limits. std::hash hashes the Raw member (rational raw: Numerator+Denominator,
// boost-style combine). (std::common_type lives in arithmetic.hpp, always on.)
//---------------------------------------------------------------------------

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::numeric_limits<beman::inside::inside<G, P>>
{
  using B = beman::inside::inside<G, P>;

  static constexpr bool is_specialized = true;
  static constexpr bool is_signed      = (G.Interval.Lower < beman::inside::detail::rational{0});
  // Every value is an integer: a non-zero integer notch over an integer Lower.
  static constexpr bool is_integer     = beman::inside::detail::is_integer_aligned<B> && G.Notch != 0;
  static constexpr bool is_exact       = true;     // rational + integer raw are both exact
  static constexpr bool is_bounded     = true;
  static constexpr bool is_modulo      = (P & beman::inside::wrap) != 0;
  static constexpr bool has_infinity   = false;
  static constexpr bool has_quiet_NaN  = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr bool traps          = beman::inside::is_checked(P);
  static constexpr bool is_iec559      = false;
  static constexpr int  radix          = 2;
  // The mode stores round by (rounding_of, the one precedence every path uses).
  static constexpr std::float_round_style round_style = []{
    using enum beman::inside::detail::round_mode;
    switch (beman::inside::detail::rounding_of(P))
    {
      case floor:     return std::round_toward_neg_infinity;
      case ceil:      return std::round_toward_infinity;
      case nearest:
      case half_even: return std::round_to_nearest;
      default:        return std::round_toward_zero;
    }
  }();

  // digits / digits10 forward to the raw type so generic algorithms see the
  // storage size, not the rational interval count.
  static constexpr int digits   = std::numeric_limits<beman::inside::detail::raw_t<B>>::digits;
  static constexpr int digits10 = std::numeric_limits<beman::inside::detail::raw_t<B>>::digits10;

  static constexpr B min()    noexcept { return B{G.Interval.Lower}; }
  static constexpr B max()    noexcept { return B{G.Interval.Upper}; }
  static constexpr B lowest() noexcept { return B{G.Interval.Lower}; }
  // Exact types have no rounding noise — epsilon and round_error are 0 when
  // 0 is on the grid (it always is when 0 ∈ interval, since the grid is
  // validated such that Lower is an integer multiple of Notch). When 0 is
  // outside the interval, fall back to the grid minimum — the closest
  // representable stand-in for "no error" the type can express.
  static constexpr B epsilon() noexcept
  {
    if constexpr (G.Interval.Lower <= beman::inside::detail::rational{0}
               && beman::inside::detail::rational{0} <= G.Interval.Upper)
      return B{beman::inside::detail::rational{0}};
    else
      return B{G.Interval.Lower};
  }
  static constexpr B round_error() noexcept { return epsilon(); }
};

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::hash<beman::inside::inside<G, P>>
{
  using B = beman::inside::inside<G, P>;

  constexpr std::size_t operator()(B const& b) const noexcept
  {
    if constexpr (beman::inside::detail::rational_raw<B>)
    {
      // Boost-style hash combine over (Numerator, Denominator).
      auto h1 = std::hash<beman::inside::umax>{}(b.raw().Numerator);
      auto h2 = std::hash<beman::inside::imax>{}(b.raw().Denominator);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
    else if constexpr (beman::inside::detail::wide_raw<B>)
    {
      // Same combine over the limbs of a wide index.
      std::size_t h = 0;
      for (auto w : b.raw().Word)
        h ^= std::hash<beman::inside::umax>{}(w) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
      return h;
    }
    else
      return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
  }
};


#if __STDC_HOSTED__ && !defined(BEMAN_INSIDE_MATH_NO_FP)

// ======================================================================
//  beman/inside/random.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Opt-in: uniform sampling over a grid. `uniform<B>(rng)` returns a B drawn
// uniformly from the grid's slots (Lower, Lower + Notch, …, Upper) — exact, any
// storage. Kept out of the umbrella because <random> is heavy, hosted-only and
// pulls <cmath> (so the single header drops it under BEMAN_INSIDE_MATH_NO_FP).
//---------------------------------------------------------------------------


#include <random>

namespace beman::inside
{
  template <insidable B, std::uniform_random_bit_generator G>
  [[nodiscard]] B uniform(G& g)
  {
    static_assert(notch_of<B> != 0 || lower_of<B> == upper_of<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    if constexpr (detail::wide_raw<B>)
    {
      // More than 2^64 slots: draw limbs uniformly, masked to the slot count's
      // bit width, and reject draws past the count (accepts > 1/2 of draws).
      using W = detail::raw_t<B>;
      const W count{grid_of<B>.slot_count()};
      constexpr int top_bits = grid_of<B>.slot_bits() - 64 * (static_cast<int>(sizeof(W) / 8) - 1);
      std::uniform_int_distribution<umax> limb;
      for (;;)
      {
        W k;
        for (auto& w : k.Word) w = limb(g);
        if constexpr (top_bits < 64) k.Word[sizeof(W) / 8 - 1] &= (umax{1} << top_bits) - 1;
        if (!(k > count)) return B::from_raw(k);
      }
    }
    else
    {
      std::uniform_int_distribution<umax> pick(0, detail::max_index_v<B>);
      const umax k = pick(g);
      if constexpr (detail::fp_raw<B> || detail::rational_raw<B>)
      {
        const detail::rational v = (lower_of<B> + (detail::rational{k} * notch_of<B>).value()).value();
        if constexpr (detail::fp_raw<B>)
          return B::from_raw(static_cast<detail::raw_t<B>>(static_cast<double>(v)));   // exact: fp-exact grid
        else
          return B::from_raw(v);
      }
      else
        return B::from_raw(detail::raw_from_offset<B>(k));   // index or value storage
    }
  }
} // namespace beman::inside


#endif // __STDC_HOSTED__ && !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_SINGLE_HEADER_HPP
