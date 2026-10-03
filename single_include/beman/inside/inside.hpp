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
    domain_error = 1,   // value outside interval
    division_by_zero,   // divisor is zero
    overflow,           // rational arithmetic overflow
    rounding_error,     // notch incompatibility
    not_finite,         // non-finite double input (NaN/Inf)
  };

  // Static, allocation-free message per code. The single source of truth for
  // every error path; returns a null-terminated string literal so it doubles as
  // the default exception's what() and as an on_error message.
  constexpr const char* errc_message(errc e) noexcept
  {
    switch (e)
    {
      case errc::domain_error:     return "value outside interval";
      case errc::division_by_zero: return "division by zero";
      case errc::overflow:         return "rational arithmetic overflow";
      case errc::rounding_error:   return "notch incompatibility";
      case errc::not_finite:       return "non-finite floating-point value";
    }
    return "unknown inside error";
  }

#if BEMAN_INSIDE_HAS_EXCEPTIONS
  //---------------------------------------------------------------------------
  // inside_error — the exception thrown by the default handler. Derives from
  // std::runtime_error (the library's only <stdexcept> use) and carries the
  // originating `errc` so `catch (inside_error& e) { e.code; }` replaces the old
  // `e.code() == make_error_code(...)` idiom.
  //---------------------------------------------------------------------------
  struct inside_error : std::runtime_error
  {
    errc code;
    explicit inside_error(errc c)
      : std::runtime_error(errc_message(c)), code(c) {}
    inside_error(errc c, const char* what)
      : std::runtime_error(what ? what : errc_message(c)), code(c) {}
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
      char data[N]{};
      constexpr fixed_string(const char (&s)[N])
      { for (unsigned i = 0; i < N; ++i) data[i] = s[i]; }
    };

    template <fixed_string Msg>
    [[noreturn]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE
    inline void constexpr_error()
    { raise(errc::overflow, Msg.data); }
  } // namespace detail

  //---------------------------------------------------------------------------
  // diagnostics
  //---------------------------------------------------------------------------
  template <typename... Ts>
  struct print_types
  {
      static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
  };
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
  // lift(op, args...) — call op on the unwrapped args → expected<result, errc>;
  // the first erroneous arg (left to right) short-circuits with its error. An op
  // already returning expected<R, errc> passes through.
  //---------------------------------------------------------------------------
  template <class Op, class... Args>
  constexpr auto lift(Op op, Args&&... args)
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

  template<typename T>
  concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<detail::rational,T>;

  namespace detail
  {

  // Smallest unsigned type whose range holds every index 0..N.
  template <std::uintmax_t N>
  using smallest_uint_for =
    std::conditional_t<(N == 0), rational,
    std::conditional_t<(N <= UINT8_MAX),  std::uint8_t,
    std::conditional_t<(N <= UINT16_MAX), std::uint16_t,
    std::conditional_t<(N <= UINT32_MAX), std::uint32_t,
                                           std::uint64_t>>>>;

  // Smallest signed type whose range holds Low..High.
  template <std::intmax_t Low, std::intmax_t High>
  using smallest_int_for =
    std::conditional_t<(Low >= INT8_MIN  && High <= INT8_MAX),  std::int8_t,
    std::conditional_t<(Low >= INT16_MIN && High <= INT16_MAX), std::int16_t,
    std::conditional_t<(Low >= INT32_MIN && High <= INT32_MAX), std::int32_t,
                                                                 std::int64_t>>>;

  // (type_name<T>() — used only by the debug stringifier — lives in
  // "beman/inside/io.hpp" so the core stays free of <string_view>.)

  // Subset of arithmetic excluding integrals — the rhs types that need the
  // rational-arithmetic assignment path. (Named to avoid clashing with `real`.)
  template<typename T>
  concept fractional = std::floating_point<T> || std::same_as<rational, T>;

  template <std::signed_integral V>
  constexpr umax safe_abs(V value)
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
      return {significand << exp2, 1};

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




namespace beman::inside::detail
{
  constexpr umax abs_den(imax d) { return (d >= 0) ? static_cast<umax>(d) : umax{0} - static_cast<umax>(d); }

  // 64×64 → 128-bit unsigned product, as {hi, lo}. Native where the target has
  // unsigned __int128; else a schoolbook 32-bit split (32-bit targets) — the
  // same construction trusted in cmath.hpp's fmul, so both are bit-exact.
  struct u128 { umax hi; umax lo; };
  constexpr u128 umul(umax a, umax b)
  {
#if defined(__SIZEOF_INT128__)
    const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
    return {static_cast<umax>(p >> 64), static_cast<umax>(p)};
#endif
    umax al = a & 0xffffffffu, ah = a >> 32;
    umax bl = b & 0xffffffffu, bh = b >> 32;
    umax ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    umax mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
    return { hh + (lh >> 32) + (hl >> 32) + (mid >> 32),
             (ll & 0xffffffffu) | (mid << 32) };
  }
  constexpr std::strong_ordering cmp128(u128 a, u128 b)
  { return (a.hi != b.hi) ? (a.hi <=> b.hi) : (a.lo <=> b.lo); }

  // 128×64 product with an overflow flag (result beyond 128 bits).
  struct mul128_result { u128 value; bool overflowed; };
  constexpr mul128_result mul128(u128 a, umax b)
  {
    const u128 low  = umul(a.lo, b);
    const u128 high = umul(a.hi, b);
    const umax hi_sum = high.lo + low.hi;
    return {u128{hi_sum, low.lo}, high.hi != 0 || hi_sum < low.hi};
  }

  // Quotient/remainder of a 128-bit dividend by a 64-bit divisor. Requires
  // 1 <= d <= imax_max (the rational-denominator domain) so the portable
  // partial remainder can never overflow when shifted.
  struct divmod128_result { u128 quotient; umax remainder; };
  constexpr divmod128_result divmod128(u128 n, umax d)
  {
#if defined(__SIZEOF_INT128__)
    using u128n = unsigned __int128;
    const u128n wide = (static_cast<u128n>(n.hi) << 64) | n.lo;
    const u128n q = wide / d;
    return {u128{static_cast<umax>(q >> 64), static_cast<umax>(q)},
            static_cast<umax>(wide % d)};
#else
    // Portable (no __int128, 32-bit targets): restoring shift-subtract divide — the same construction
    // as cmath.hpp's to_fixed fallback.
    u128 q{0, 0};
    umax r = 0;
    for (int i = 127; i >= 0; --i)
    {
      r = (r << 1) | ((i >= 64 ? (n.hi >> (i - 64)) : (n.lo >> i)) & 1u);
      q.hi = (q.hi << 1) | (q.lo >> 63);
      q.lo <<= 1;
      if (r >= d) { r -= d; q.lo |= 1; }
    }
    return {q, r};
#endif
  }

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

  constexpr std::expected<rational, errc> operator+(rational const&, rational const&);
  constexpr std::expected<rational, errc> operator/(rational const&, rational const&);
  constexpr std::expected<rational, errc> operator-(rational const&, rational const&);

  constexpr std::expected<rational, errc> operator*(rational const&, rational const&);
  constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

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
    constexpr bool operator==(const rational&) const = default;
    template <arithmetic T>
    constexpr bool operator==(T value) const { return operator==(rational{value}); }

    constexpr rational operator-() const;

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
    constexpr rational operator+() const { return *this; }

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
    template <bool Checked> static constexpr auto add_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto mul_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto div_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto inv_impl(rational const&);

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
  template <fixed_string Msg>
  constexpr std::unexpected<errc> fail(errc code)
  {
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
    return static_cast<T>(Numerator / abs_den(Denominator));
  }

  //---------------------------------------------------------------------------
  // _ins / _r literal parser — shared between inside.hpp's `_ins` and `_r` below.
  // Accepts:
  //   integer:           5, 1'000
  //   decimal:           1.25, .5
  //   decimal scientific 1.5e2, 2.5e-1
  //   hex integer:       0xff
  //   binary integer:    0b1010
  //   hex float (Q-fmt): 0x1p15, 0x1p-15, 0x1.8p3
  // Exact (no double round-trip). Overflow -> consteval throw.
  //---------------------------------------------------------------------------
  namespace _detail
  {
    consteval int parse_digit(char c, int base)
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

    template<char... Chars>
    consteval rational parse_ins_literal()
    {
      constexpr char src[] = { Chars..., '\0' };
      constexpr std::size_t N = sizeof...(Chars);

      // Detect radix prefix.
      int base = 10;
      std::size_t i = 0;
      if (N >= 2 && src[0] == '0')
      {
        if (src[1] == 'x' || src[1] == 'X') { base = 16; i = 2; }
        else if (src[1] == 'b' || src[1] == 'B') { base = 2; i = 2; }
      }

      umax num = 0;
      int frac_len = 0;
      bool in_frac = false;
      int exp = 0;
      bool exp_neg = false;
      bool has_p_exp = false;  // 2^exp (hex floats)
      bool has_e_exp = false;  // 10^exp (decimal scientific)
      bool in_exp = false;
      bool exp_seen_digit = false;

      for (; i < N; ++i)
      {
        char c = src[i];
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
            exp = exp * 10 + (c - '0');
            exp_seen_digit = true;
            continue;
          }
          constexpr_error<"_ins/_r literal: invalid char in exponent">();
        }

        if (c == '.')
        {
          if (in_frac) constexpr_error<"_ins/_r literal: multiple '.'">();
          if (base == 2) constexpr_error<"_ins/_r literal: '.' not allowed in binary">();
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

        int d = parse_digit(c, base);
        if (d < 0) constexpr_error<"_ins/_r literal: invalid digit for radix">();

        umax base_u = base;
        if (num > (~umax{0} - d) / base_u)
          constexpr_error<"_ins/_r literal: numerator overflow">();
        num = num * base_u + d;
        if (in_frac) ++frac_len;
      }

      // Build denominator from fractional part.
      // For decimal: den = 10^frac_len. For hex: den = 2^(4*frac_len).
      umax den = 1;
      if (base == 10)
      {
        for (int k = 0; k < frac_len; ++k)
        {
          if (den > (~umax{0}) / 10u)
            constexpr_error<"_ins/_r literal: denominator overflow">();
          den *= 10u;
        }
      }
      else if (base == 16)
      {
        int shift = 4 * frac_len;
        if (shift >= 64) constexpr_error<"_ins/_r literal: hex fraction too long">();
        den <<= shift;
      }

      // Apply binary exponent (hex floats, `p`).
      if (has_p_exp)
      {
        if (exp >= 63) constexpr_error<"_ins/_r literal: p exponent too large">();
        if (!exp_neg) num <<= exp;
        else
        {
          if (den > (~umax{0}) >> exp)
            constexpr_error<"_ins/_r literal: p exponent denominator overflow">();
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
            if (num > (~umax{0}) / 10u)
              constexpr_error<"_ins/_r literal: e exponent numerator overflow">();
            num *= 10u;
          }
          else
          {
            if (den > (~umax{0}) / 10u)
              constexpr_error<"_ins/_r literal: e exponent denominator overflow">();
            den *= 10u;
          }
        }
      }

      return rational{num, den};
    }
  }

  template<char... Chars>
  constexpr rational operator ""_r() { return _detail::parse_ins_literal<Chars...>(); }

  // notch<N, D> is defined publicly in `namespace beman::inside` (see the re-export block
  // at the end of this header) so consumers spell it without naming the
  // internal representation type.

  //---------------------------------------------------------------------------
  // add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
  //---------------------------------------------------------------------------
  template <bool Checked>
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
          { return ret_t{fail<"rational +: numerator overflow (same denominator)">(errc::overflow)}; }
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
      { return ret_t{fail<"rational +: denominator overflow">(errc::overflow)}; }
      if (mul_overflow(a.Numerator, b_ad_r, &A) ||
          mul_overflow(b.Numerator, a_ad_r, &B))
      {
        // Mixed signs: |A − B| can fit umax even when a cross-product alone
        // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
        // subtraction brings it back in range — the dbl-engine store path hit
        // exactly this). Retry the difference in 128-bit before giving up.
        if (a_neg != b_neg)
        {
          const u128 A128 = umul(a.Numerator, b_ad_r);
          const u128 B128 = umul(b.Numerator, a_ad_r);
          const bool a_bigger = cmp128(A128, B128) > 0;
          const u128 big   = a_bigger ? A128 : B128;
          const u128 small = a_bigger ? B128 : A128;
          const u128 diff{big.hi - small.hi - (big.lo < small.lo ? 1u : 0u),
                          big.lo - small.lo};
          if (diff.hi == 0)
          {
            rational r;
            r.Numerator   = diff.lo;
            r.Denominator = (a_neg ? a_bigger : !a_bigger) ? -denominator
                                                           :  denominator;
            trim(r.Numerator, r.Denominator);
            return ret_t{r};
          }
        }
        return ret_t{fail<"rational +: cross-multiplication overflow">(errc::overflow)};
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
        { return ret_t{fail<"rational +: numerator sum overflow">(errc::overflow)}; }
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

  template <bool Checked>
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
        { return ret_t{fail<"rational *: numerator overflow">(errc::overflow)}; }
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
      { return ret_t{fail<"rational *: numerator or denominator overflow">(errc::overflow)}; }
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
  template <bool Checked>
  inline constexpr auto rational::inv_impl(rational const& a)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked)
    {
      // a.Numerator goes into the result's Denominator slot, so it must fit in
      // imax (else the umax→imax conversion wraps and a later -Denominator is UB).
      if (a.Numerator == 0)
      { return ret_t{fail<"rational inv: division by zero">(errc::division_by_zero)}; }
      if (a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational inv: numerator out of denominator range">(errc::overflow)}; }
    }

    return ret_t{make_raw(abs_den(a.Denominator), signed_numerator(a))};
  }

  // div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
  // the b.Numerator-fits-in-imax check (added there) propagates here too;
  // the unchecked path skips it (caller's contract).
  template <bool Checked>
  inline constexpr auto rational::div_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked)
    {
      auto inv_b = inv_impl<true>(b);
      if (!inv_b.has_value()) return ret_t{std::unexpected{inv_b.error()}};
      return mul_impl<true>(a, *inv_b);
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
  inline constexpr rational rational::operator-() const
  {
    if (Numerator == 0)
      return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    return make_raw(Numerator, -Denominator);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
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
#if defined(__SIZEOF_INT128__)
    using u128n = unsigned __int128;
    u128n A = static_cast<u128n>(lhs.Numerator) * rhs_ad;
    u128n B = static_cast<u128n>(rhs.Numerator) * lhs_ad;
    return lhs_neg ? (B <=> A) : (A <=> B);
#else
    // Portable path (no __int128): form each product as {hi, lo} and compare lexically.
    const u128 A = umul(lhs.Numerator, rhs_ad);
    const u128 B = umul(rhs.Numerator, lhs_ad);
    return lhs_neg ? cmp128(B, A) : cmp128(A, B);
#endif
  }

  template <typename T>
  inline constexpr auto operator<=>(std::expected<T, errc> const& lhs, const rational& rhs)
  { return rational{lhs.value()} <=> rhs; }

  template <typename T>
  inline constexpr auto operator<=>(rational const& lhs, std::expected<T, errc> const& rhs)
  { return lhs <=> rational{rhs.value()}; }

  template <arithmetic T>
  inline constexpr auto operator<=>(T lhs, const rational& rhs)
  { return rational{lhs} <=> rhs; }

  template <arithmetic T>
  inline constexpr auto operator<=>(rational const& lhs, T rhs)
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
  inline constexpr std::expected<rational, errc> operator+(rational const& lhs, rational const& rhs)
  { return rational::add_impl<true>(lhs, rhs); }

  inline constexpr std::expected<rational, errc> operator-(rational const& lhs, rational const& rhs)
  { return operator+(lhs, -rhs); }

  inline constexpr std::expected<rational, errc> operator*(rational const& lhs, rational const& rhs)
  { return rational::mul_impl<true>(lhs, rhs); }

  inline constexpr std::expected<rational, errc> operator/(rational const& lhs, rational const& rhs)
  { return rational::div_impl<true>(lhs, rhs); }

  inline constexpr std::expected<rational, errc> operator-(std::expected<rational, errc> const& v)
  { return lift([](rational r){ return -r; }, v); }

#define BEMAN_INSIDE_RATIONAL_OP(op)                                                   \
  template <arithmetic T>                                                              \
  inline constexpr auto operator op(T lhs, rational const& rhs)                        \
  { return rational{lhs} op rhs; }                                                     \
  template <arithmetic T>                                                              \
  inline constexpr auto operator op(rational const& lhs, T rhs)                        \
  { return lhs op rational{rhs}; }                                                     \
  template <class L, class R> requires rational_lift_operands<L, R>                    \
  inline constexpr auto operator op(L const& lhs, R const& rhs)                        \
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

namespace beman::inside
{
  // `rational` is internal, but the grid-building `notch<N,D>` / `frac<N,D>`
  // literals are public — they never expose the type.
  template <umax N, imax D = 1>
  inline constexpr detail::rational notch = detail::rational{N, D};

  // frac<N, D> — exact fractional grid value (signed numerator), companion to
  // notch<N,D> for non-dyadic endpoints not writable as a float literal
  // (e.g. `frac<-6, 5>` for -1.2).
  template <imax N, imax D = 1>
  inline constexpr detail::rational frac = detail::rational{N, D};
} // namespace beman::inside



// ======================================================================
//  beman/inside/interval.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



namespace beman::inside
{
  //---------------------------------------------------------------------------
  // interval — structural NTTP type (public members only) with inclusive Lower
  // and Upper bounds. Like `grid`, its operator+/-/*// computes result intervals
  // at compile time; division returns errc::division_by_zero when the divisor straddles zero
  // (grid::operator/ re-runs on the two zero-free halves and unions them).
  //---------------------------------------------------------------------------
  struct interval
  {
    detail::rational Lower;
    detail::rational Upper;

    interval() = default;

    constexpr interval(detail::rational lower, detail::rational upper)
     :Lower{lower}, Upper{upper} { }
    constexpr interval(arithmetic auto lower, arithmetic auto upper)
     :Lower{lower}, Upper{upper} { }

    template <auto I>
    static constexpr bool validate()
    {
      static_assert(I.Lower <= I.Upper);
      return true;
    }

    constexpr bool operator==(const interval& rhs) const = default;
    constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    constexpr bool divides_evenly(const detail::rational& notch) const
    { return detail::divides_evenly((Upper - Lower).value(), notch); }

    constexpr std::expected<detail::rational, errc> operator/(const detail::rational& notch) const
    { return (Upper - Lower) / notch; }
  };

  // Containment / disjointness — free functions over the public endpoints
  // (siblings of the binary interval operators below).
  [[nodiscard]] constexpr bool includes(interval const& iv, interval const& rhs)
  { return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, detail::rational const& r)
  { return iv.Lower <= r && r <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, arithmetic auto a)
  { return includes(iv, detail::rational{a}); }

  // `excludes` means *strictly disjoint* — the intervals share no value.
  // `!includes()` is weaker: it only rules out total containment, so two
  // overlapping intervals are `!includes` AND `!excludes`.
  [[nodiscard]] constexpr bool excludes(interval const& iv, interval const& rhs)
  { return rhs.Upper < iv.Lower || iv.Upper < rhs.Lower; }

  // The `includes(rhs, iv)` clause catches rhs wholly containing iv (where
  // neither rhs endpoint lands in iv, so the other checks would miss it).
  [[nodiscard]] constexpr bool overlaps(interval const& iv, interval const& rhs)
  { return includes(rhs, iv) || includes(iv, rhs.Lower) || includes(iv, rhs.Upper); }

  // The min/max hull of four endpoint combinations — the result interval of an
  // interval product or quotient (interval arithmetic's four-corner rule).
  namespace detail
  {
    constexpr interval corner_hull(rational a, rational b, rational c, rational d) noexcept
    {
      const rational lo1 = a < b ? a : b, hi1 = a < b ? b : a;
      const rational lo2 = c < d ? c : d, hi2 = c < d ? d : c;
      return interval{lo1 < lo2 ? lo1 : lo2, hi1 < hi2 ? hi2 : hi1};
    }
  }

  constexpr std::expected<interval, errc> operator+  (const interval&, const interval&);
  constexpr std::expected<interval, errc> operator-  (const interval&, const interval&);
  constexpr std::expected<interval, errc> operator*  (const interval&, const interval&);
  constexpr std::expected<interval, errc> operator/  (const interval&, const interval&);
  constexpr auto                          operator<=>(const interval&, const interval&) -> std::partial_ordering;

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr std::expected<interval, errc> operator+(const interval& lhs, const interval& rhs)
  {
    return lift(
      [](detail::rational l, detail::rational u){ return interval{l, u}; },
      lhs.Lower + rhs.Lower, lhs.Upper + rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr std::expected<interval, errc> operator-(const interval& lhs, const interval& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr std::expected<interval, errc> operator*(const interval& lhs, const interval& rhs)
  {
    return lift(detail::corner_hull,
      lhs.Lower * rhs.Lower, lhs.Lower * rhs.Upper,
      lhs.Upper * rhs.Lower, lhs.Upper * rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr std::expected<interval, errc> operator/(const interval& lhs, const interval& rhs)
  {
    if (includes(rhs, 0))
      return std::unexpected{errc::division_by_zero};

    return lift(detail::corner_hull,
      lhs.Lower / rhs.Lower, lhs.Lower / rhs.Upper,
      lhs.Upper / rhs.Lower, lhs.Upper / rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering
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
// BEMAN_INSIDE_MATH_FIXED. Public API and grid deduction are unchanged.
#if !defined(BEMAN_INSIDE_MATH_NO_FP)
#  if defined(BEMAN_INSIDE_MATH_FIXED) || (defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0)
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
  inline constexpr policy_flag ignore_domain{1ull << 2};
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

  // runtime checking — opt-in
  inline constexpr policy_flag checked{1ull << 34}; // enable runtime domain/overflow checks

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
  // inside under BEMAN_INSIDE_MATH_FIXED. Power-of-2 notch + dyadic Lower required so
  // on-grid values are exact in double (see `double_exact`).
  inline constexpr policy_flag f64{(1ull << 37) | round_nearest};

  // `f32` — binary32-backed storage (raw held as IEEE-754 float, notch nominal);
  // the single-precision sibling of `f64`, for float-only FPUs (Cortex-M4F) and
  // the `flt` engine. Power-of-2 notch + dyadic Lower required AND every on-grid
  // value must fit float's 24-bit significand (see `float_exact`). Like `f64` it
  // is an ordinary round_nearest integer inside under BEMAN_INSIDE_MATH_FIXED. Widest-wins
  // storage order: exact > f64 > f32 > direct > indexed > deduced.
  inline constexpr policy_flag f32{(1ull << 41) | round_nearest};

  // `real` — deprecated spelling of `f64`, kept as an alias for one release. New
  // code should use `f64` (binary64 storage) or `f32` (binary32). The flag is
  // purely a storage choice — transcendentals gate on `snap`, not on this.
  inline constexpr policy_flag real = f64;

  // Fixed-width integer raw storage — pin the exact backing type instead of
  // letting deduction pick the smallest fit. A bare width flag means *value*
  // storage (raw == value, like `direct`, so Notch == 1 and the value range must
  // fit the type); OR in `indexed` for 0-based notch-index storage. `storage_pick`
  // static_asserts the type is big enough for the grid (no silent widening). One
  // width flag at a time. Unlike `f32`/`f64` these carry no `round_nearest` — they
  // are plain integer storage, like `direct`/`indexed`. Widest-wins storage order:
  // exact > f64 > f32 > {width} > direct > indexed > deduced.
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
  inline constexpr policy_flag unsafe
    {(1ull << 36) | ignore_domain | snap | ignore_zero};

  //---------------------------------------------------------------------------
  // Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
  // of `flag` is present in `set` — reads better than the raw `(set & flag) ==
  // flag` and is correct for composite flags (e.g. `round_nearest` carries
  // `snap`, `real` carries `round_nearest`), where a bare `set & flag`
  // truthy test would misfire. `has_any_flag` tests for any overlap.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept
  { return (set & flag) == flag; }

  [[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept
  { return (set & flags) != none; }

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
  template<typename F> struct on_clamp_t    { [[no_unique_address]] F fn; };
  template<typename F> struct on_wrap_t     { [[no_unique_address]] F fn; };
  template<typename F> struct on_error_t    { [[no_unique_address]] F fn; };
  template<typename F> struct on_overflow_t { [[no_unique_address]] F fn; };

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
  template<typename T> struct IsClampActionPred    : std::false_type {};
  template<typename F> struct IsClampActionPred<on_clamp_t<F>>    : std::true_type {};
  template<typename T> struct IsWrapActionPred     : std::false_type {};
  template<typename F> struct IsWrapActionPred<on_wrap_t<F>>     : std::true_type {};
  template<typename T> struct IsErrorActionPred    : std::false_type {};
  template<typename F> struct IsErrorActionPred<on_error_t<F>>    : std::true_type {};
  template<typename T> struct IsOverflowActionPred : std::false_type {};
  template<typename F> struct IsOverflowActionPred<on_overflow_t<F>> : std::true_type {};

  template<typename T> concept clamp_action    = IsClampActionPred   <std::remove_cvref_t<T>>::value;
  template<typename T> concept wrap_action     = IsWrapActionPred    <std::remove_cvref_t<T>>::value;
  template<typename T> concept error_action    = IsErrorActionPred   <std::remove_cvref_t<T>>::value;
  template<typename T> concept overflow_action = IsOverflowActionPred<std::remove_cvref_t<T>>::value;

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

  // OR of implied_flags<plain<A>> across the pack.
  template<typename... As>
  inline constexpr policy_flag merged_implied_flags =
    (none | ... | implied_flags<std::remove_cvref_t<As>>);

  // pick_action<Trait>(actions...) returns a reference to the first pack element
  // matching the trait, or a static `no_action` fallback if none does. Conflict
  // diagnostics elsewhere ensure at most one match.
  namespace _detail
  {
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
  }

  template<template<typename> class Trait, typename... As>
  constexpr auto& pick_action(As&... as)
  {
    if constexpr (sizeof...(As) == 0) return _detail::pick_action_fallback<Trait>();
    else return _detail::pick_action_impl<Trait>(as...);
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




namespace beman::inside
{
  //---------------------------------------------------------------------------
  // grid — structural NTTP type (public members only). Discretizes its interval
  // into notch-sized steps (interval must divide evenly by notch; Notch == 0
  // allows every rational, raw not offset). Its operator+/-/*// is the engine of
  // compile-time result-grid inference: every inside arithmetic operator computes
  // its result grid here, so the result interval contains every reachable value.
  //---------------------------------------------------------------------------
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
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper)
      :grid{interval{lower, upper}, detail::rational{1}} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower)
      :grid{interval{lower, lower}, detail::rational{0}} { }
    constexpr grid(interval val, detail::rational notch):Interval{val}, Notch{notch} { }

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
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
      if (!iv.divides_evenly(notch))
        return std::unexpected{errc::rounding_error};
      if (notch != 0 && !detail::divides_evenly(iv.Lower, notch))
        return std::unexpected{errc::rounding_error};
      return grid{iv, notch};
    }

    // Notch-slot count = (Upper-Lower)/Notch, computed WITHOUT the rational
    // division (which throws at constant-eval on overflow). A valid grid is
    // notch-aligned, so with span = p/q and Notch = r/s the count is exactly
    // (p/r)·(s/q); mul_overflow flags when it exceeds umax. Returns false (and
    // count is meaningless) on overflow — such a grid stores as rational, never
    // an index, so the count is never used.
    constexpr bool notch_count(umax& out) const
    {
      if (Notch == 0) { out = 0; return true; }
      const detail::rational span = (Interval.Upper - Interval.Lower).value();
      const umax p = span.Numerator,  q = detail::abs_den(span.Denominator);
      const umax r = Notch.Numerator, s = detail::abs_den(Notch.Denominator);
      if (r == 0 || p % r != 0 || s % q != 0) { out = 0; return false; }
      return !mul_overflow(p / r, s / q, &out);
    }

    // Index-storage slot count (0 on overflow; the over-flow branch of storage_min
    // is discarded for such grids, which pick rational storage instead).
    constexpr umax max_notch() const { umax c = 0; (void)notch_count(c); return c; }

    // True when the slot count fits umax (index storage is possible). False ⇒ the
    // grid is still valid but stores its value as a rational, never an index.
    constexpr bool notch_count_representable() const { umax c = 0; return notch_count(c); }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_ins`) regardless of whole-range mapping.
    constexpr bool representable(detail::rational v) const
    {
      if (!includes(Interval, v)) return false;
      if (Notch == 0) return true;
      auto diff = v - Interval.Lower;            // expected<rational, errc>
      if (!diff) return false;
      auto off = diff.value() / Notch;           // expected<rational, errc>
      return off.has_value() && detail::abs_den(off->Denominator) == 1;
    }

    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy inside has an unsigned raw that IS the value.)
  };

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

  // Smallest raw type holding every reachable index in G. Order: notch-zero →
  // rational (no integer index space); index count too large for any integer →
  // rational (store the value's fraction directly, no index); signed-direct fits
  // Lower < 0 with notch 1; unsigned-offset (max_notch slots) otherwise.
  namespace detail
  {
  template <grid G>
  using storage_min =
    std::conditional_t<(G.Notch == 0), detail::rational,
    std::conditional_t<(!G.notch_count_representable()), detail::rational,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1),
      smallest_int_for<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
      smallest_uint_for<G.max_notch()>>>>;

  // Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
  // on-grid value is exactly representable in IEEE-754 `double`. Precondition
  // for double-backed (`real`) storage.
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
  // unreachable once |N| < 2^53. Necessary precondition for `real` storage.
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
  using raw_type_of =
    std::conditional_t<(P & i8 ) == i8 , std::int8_t,
    std::conditional_t<(P & u8 ) == u8 , std::uint8_t,
    std::conditional_t<(P & i16) == i16, std::int16_t,
    std::conditional_t<(P & u16) == u16, std::uint16_t,
    std::conditional_t<(P & i32) == i32, std::int32_t,
    std::conditional_t<(P & u32) == u32, std::uint32_t,
    std::conditional_t<(P & i64) == i64, std::int64_t,
                                                    std::uint64_t>>>>>>>;

  // Does raw type R hold every reachable raw value of grid G under the given
  // encoding? Index storage runs 0..max_notch (unsigned); value storage runs
  // Lower..Upper. The full range of R is usable, matching smallest_uint_for /
  // smallest_int_for.
  template <grid G, typename R, bool Index>
  constexpr bool storage_fits() noexcept
  {
    using lim = std::numeric_limits<R>;
    if constexpr (Index)
      return G.notch_count_representable()
          && G.max_notch() <= static_cast<umax>(lim::max());
    else if constexpr (std::is_unsigned_v<R>)
      return G.Interval.Lower >= 0
          && G.Interval.Upper <= rational{static_cast<umax>(lim::max())};
    else
      return G.Interval.Lower >= rational{static_cast<imax>(lim::min())}
          && G.Interval.Upper <= rational{static_cast<imax>(lim::max())};
  }

  // Demote an fp STORAGE flag a result grid can't represent — for DEDUCED policies
  // (cmath auto-outputs, which inherit the operand's storage flag), so a deduced
  // f32 output whose grid overflows binary32 silently widens instead of hard-
  // erroring. (A grid a user spells `f32` on directly still static_asserts in
  // storage_pick — that's deliberate misuse, not deduction.) f32 needs float_exact,
  // f64 needs double_exact (Notch == 0 continuous fits either). When the flag
  // doesn't fit: widen f32→f64 if double holds the grid, else drop the fp flag so
  // storage is deduced. The snap/round bits are preserved.
  // Storage for an inside<G, P>: representation flags pick the raw type, widest-wins
  // (exact > real > direct > indexed > deduced).
  //   exact   → rational raw on any grid.
  //   real    → double-backed under the default engine, on a dyadic or notch-0
  //             grid; elided under BEMAN_INSIDE_MATH_FIXED (falls through to deduced).
  //   direct  → raw == value, plain integer (Notch == 1).
  //   indexed → raw == 0-based notch index (Notch != 0).
  //   none    → storage_min deduction.
  template <grid G, policy_flag P>
  constexpr auto storage_pick()
  {
    if constexpr (has_flag(P, exact))
      return detail::rational{};
#ifndef BEMAN_INSIDE_MATH_NO_FP
    else if constexpr (has_flag(P, real)
                    && (double_exact<G> || G.Notch == 0))
      return double{};
    else if constexpr (has_flag(P, real) && dyadic_grid<G>)
    {
      // `real`/`f64` explicitly requested on a dyadic grid double can't represent
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
      using R = raw_type_of<P>;
      constexpr bool idx = (P & indexed) == indexed;
      static_assert(idx ? (G.Notch != 0) : (G.Notch == 1),
        "fixed-width storage: value storage needs Notch == 1 — add `indexed` to "
        "store a notched grid's 0-based index instead");
      static_assert(storage_fits<G, R, idx>(),
        "fixed-width storage: the chosen raw type is too small for this grid — "
        "widen the flag, coarsen the grid/notch, or use `exact`");
      return R{};
    }
    else if constexpr ((P & direct) == direct && G.Notch == 1)
      return std::conditional_t<(G.Interval.Lower < 0),
          smallest_int_for<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
          smallest_uint_for<static_cast<umax>(trunc(G.Interval.Upper))>>{};
    else if constexpr ((P & indexed) == indexed && G.Notch != 0)
      return smallest_uint_for<G.max_notch()>{};
    else
      return storage_min<G>{};
  }

  template <grid G, policy_flag P>
  using storage_for = decltype(storage_pick<G, P>());
  }

  constexpr std::expected<grid, errc> operator+(const grid&, const grid&);
  constexpr std::expected<grid, errc> operator-(const grid&, const grid&);
  constexpr std::expected<grid, errc> operator*(const grid&, const grid&);
  constexpr std::expected<grid, errc> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr std::expected<grid, errc> operator+(const grid& lhs, const grid& rhs)
  {
    // gcd returns expected — lift it so a notch-denominator overflow produces
    // errc::overflow rather than a silently wrapped result grid.
    return lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, detail::gcd(lhs.Notch, rhs.Notch));
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr std::expected<grid, errc> operator-(const grid& lhs, const grid& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr std::expected<grid, errc> operator*(const grid& lhs, const grid& rhs)
  {
    // A point operand c (notch 0) scales the other lattice exactly: its notch
    // becomes N·|c|, so `x * just<c>` keeps integer storage instead of turning
    // continuous (rational-backed).
    const bool lp = lhs.Interval.Lower == lhs.Interval.Upper;
    const bool rp = rhs.Interval.Lower == rhs.Interval.Upper;
    const detail::rational ln = (lp && !rp) ? detail::abs(lhs.Interval.Lower) : lhs.Notch;
    const detail::rational rn = (rp && !lp) ? detail::abs(rhs.Interval.Lower) : rhs.Notch;
    return lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, ln * rn);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr std::expected<grid, errc> operator/(const grid& lhs, const grid& rhs)
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
      return lift(
        [](interval pos, interval neg){
          return grid{interval{neg.Lower < pos.Lower ? neg.Lower : pos.Lower,
                               neg.Upper < pos.Upper ? pos.Upper : neg.Upper}, detail::rational{0}};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return lift([](interval i){ return grid{i, detail::rational{0}}; },
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
  inline constexpr std::expected<grid, errc> hull(const grid& lhs, const grid& rhs)
  {
    const interval iv{lhs.Interval.Lower < rhs.Interval.Lower ? lhs.Interval.Lower : rhs.Interval.Lower,
                      lhs.Interval.Upper < rhs.Interval.Upper ? rhs.Interval.Upper : lhs.Interval.Upper};
    if (lhs.Notch == 0 || rhs.Notch == 0)
      return grid{iv, detail::rational{0}};
    return lift([iv](detail::rational g){ return grid{iv, g}; },
                detail::gcd(lhs.Notch, rhs.Notch));
  }
} // namespace beman::inside


//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`Grid<B>`, `InsidePolicy<B>`, `Lower/Upper/Notch<B>`,
// `Interval<B>`) plus the `insidable`/`numeric`/`inside_assignable` concepts; the
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
  inline constexpr grid Grid = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

  template <insidable B>
  inline constexpr policy_flag InsidePolicy = detail::inside_params<std::remove_cvref_t<B>>::policy_v;

  template <typename T>
  inline constexpr interval Interval = {0,0};

  template <insidable B>
  inline constexpr interval Interval<B> = Grid<B>.Interval;

  template <std::integral I>
  inline constexpr interval Interval<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <insidable B> inline constexpr detail::rational Lower = Grid<B>.Interval.Lower;
  template <insidable B> inline constexpr detail::rational Upper = Grid<B>.Interval.Upper;
  template <insidable B> inline constexpr detail::rational Notch = Grid<B>.Notch;

  template <typename N>
  concept numeric = insidable<N> or arithmetic<N>;

  //---------------------------------------------------------------------------
  // Internal plumbing — storage shape, raw/value conversion, dispatch.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>
    using plain = std::remove_cvref_t<T>;

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
    //   raw_from_offset<B>(o)  index → raw_t<B>     adds RawLo for direct storage; identity for index
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

    template <insidable B>
    inline constexpr bool value_raw =
         !fp_raw<B> && !rational_raw<B>
      && ((InsidePolicy<B> & direct) == direct
          // A pinned width flag without `indexed` is value storage (raw == value)
          // regardless of Lower's sign — storage_pick checked the range fits.
          || (has_width_flag(InsidePolicy<B>)
              && (InsidePolicy<B> & indexed) != indexed)
          || ((InsidePolicy<B> & indexed) != indexed
              && Notch<B> == 1
              && (Lower<B> == 0 || std::signed_integral<raw_t<B>>)));

    template <insidable B>
    inline constexpr bool index_raw =
         !fp_raw<B> && !rational_raw<B> && !value_raw<B>;

    // Ungated double view of any inside, for the `real` arithmetic arms (the
    // public operator double() is gated on a rounding flag; this is always
    // available). Everything but index storage holds the value verbatim; an
    // index decodes through the grid.
    template <insidable B>
    [[nodiscard]] constexpr double as_double(B const& b) noexcept
    {
      if constexpr (!index_raw<B>)
        return static_cast<double>(b.raw());
      else
        return static_cast<double>((*(b.raw() * Notch<B>) + Lower<B>).value());
    }

    template <insidable B>
    using negative = inside<-Grid<B>, InsidePolicy<B>>;

    // True when R's interval cannot contain zero — so `a / b` can return a plain
    // `inside` instead of `expected<inside, errc>` (see detail/division.hpp). A point
    // grid at 0 is *not* excluded.
    template <insidable R>
    inline constexpr bool DivisorExcludesZero = (Lower<R> > 0) || (Upper<R> < 0);

    // Storage-agnostic int truncation of interval endpoints — intent-revealing
    // `static_cast<imax>(Lower<B>)`. Used by from_value, RawLo, the fast paths.
    template <insidable B>
    inline constexpr imax LowerImax = trunc(Lower<B>);

    template <insidable B>
    inline constexpr imax UpperImax = trunc(Upper<B>);

    // Slot count via grid::max_notch (overflow-safe: 0 when it doesn't fit umax,
    // for grids that store as rational and never use the index).
    template <insidable B>
    inline constexpr umax NotchCount = Grid<B>.max_notch();

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
    constexpr raw_t<B> raw_cast(auto value)
    {
      return static_cast<raw_t<B>>(value);
    }

    template <insidable B>
    constexpr raw_t<B> raw_cast(rational value)
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
    constexpr imax raw_imax(B b) noexcept { return static_cast<imax>(b.raw()); }

    //-------------------------------------------------------------------------
    // Q-format integer fast path: for grids with integer Lower, unit-numerator
    // Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
    // by operator rational(), from_value, and assignment::store.
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr bool HasQFormatFastPath =
        abs_den(Lower<B>.Denominator) == 1
        && Notch<B>.Numerator == 1
        && !rational_raw<B>
        && (std::signed_integral<raw_t<B>>
            || NotchCount<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // value → raw, integer math only. Pre: HasQFormatFastPath<B>.
    template <insidable B>
    constexpr raw_t<B> q_format_encode(imax value) noexcept
    {
      constexpr imax nd = abs_den(Notch<B>.Denominator);
      return raw_cast<B>((value - LowerImax<B>) * nd);
    }

    // raw → rational, integer math only. Pre: HasQFormatFastPath<B>.
    template <insidable B>
    constexpr rational q_format_decode(B b) noexcept
    {
      constexpr imax nd = abs_den(Notch<B>.Denominator);
      return rational{raw_imax(b) + LowerImax<B> * nd, nd};
    }

    // Library-internal extraction helper. Always succeeds (returns `imax`
    // unconditionally) but does not check the value fits in any narrower
    // target. User code should prefer `b.to<T>()`, which carries a typed
    // overflow error.
    template <insidable B>
    constexpr imax to_value(B b)
    {
      if constexpr (!index_raw<B>)
        return raw_imax(b);
      else if constexpr (abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1)
        return LowerImax<B> + raw_imax(b) * static_cast<imax>(Notch<B>.Numerator);
      else if constexpr (HasQFormatFastPath<B>)
      {
        constexpr imax nd = abs_den(Notch<B>.Denominator);
        return (raw_imax(b) + LowerImax<B> * nd) / nd;   // q_format_decode, truncated
      }
      else // index storage, generic rational path
        return trunc(as_rational(b));
    }

    template <insidable B>
    constexpr void from_value(B& b, imax val)
    {
      if constexpr (!index_raw<B>)
        b = B::from_raw(raw_cast<B>(val));
      else if constexpr (abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1)
        b = B::from_raw(raw_cast<B>((val - LowerImax<B>) / static_cast<imax>(Notch<B>.Numerator)));
      else if constexpr (HasQFormatFastPath<B>)
        b = B::from_raw(q_format_encode<B>(val));
      else // index storage, generic rational path
      {
        auto offset = (rational{val} - Lower<B>) / Notch<B>;
        b = B::from_raw(raw_cast<B>(offset.value().Numerator));
      }
    }

    // x mod m into [0, m) for m > 0 — one division (vs `((x % m) + m) % m`).
    [[nodiscard]] constexpr imax euclid_mod(imax x, imax m) noexcept
    { const imax r = x % m; return r < 0 ? r + m : r; }

    //-------------------------------------------------------------------------
    // RawLo / RawHi / raw_from_offset — map interval endpoints to raw space. For
    // notch-offset storage the raw is a 0-based index (RawLo == 0); for direct
    // storage the raw IS the value (RawLo == LowerImax<B>), so an offset needs
    // RawLo<L> added back before storing.
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr imax RawLo = !index_raw<B> ? LowerImax<B> : 0;

    template <insidable B>
    inline constexpr imax RawHi = !index_raw<B> ? UpperImax<B> : static_cast<imax>(NotchCount<B>);

    template <insidable L>
    constexpr raw_t<L> raw_from_offset(umax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(static_cast<imax>(offset) + RawLo<L>);
      else
        return raw_cast<L>(offset);
    }

    template <insidable L>
    constexpr raw_t<L> raw_from_offset(imax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(offset + RawLo<L>);
      else
        return raw_cast<L>(static_cast<umax>(offset));
    }

    //-------------------------------------------------------------------------
    // IsIntegerInterval vs IsIntegerAligned — easy to confuse, both needed.
    //   IsIntegerInterval<B>: Lower and Upper integer (Notch may be fractional,
    //     e.g. inside<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
    //   IsIntegerAligned<B>: Notch and Lower integer ⇒ IsIntegerInterval (not the
    //     converse). Precondition for native integer raw arithmetic (Raw == value).
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr bool IsIntegerInterval =
        abs_den(Lower<B>.Denominator) == 1 && abs_den(Upper<B>.Denominator) == 1;

    template <insidable B>
    inline constexpr bool IsIntegerAligned =
        abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1;

    // Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
    // unit numerator with integer denominator > 1, Lower is an integer at 0.
    // Value = Raw / Notch.Denominator. Used to gate the integer fast path for
    // fixed-point division, which would otherwise fall into the slow rational
    // route because Notch.Denominator > 1 disqualifies IsIntegerAligned.
    template <insidable B>
    inline constexpr bool IsQFormat =
           !rational_raw<B>
        && Notch<B>.Numerator == 1
        && abs_den(Notch<B>.Denominator) > 1
        && abs_den(Lower<B>.Denominator) == 1
        && Lower<B> == 0;

    // Policy test: checks both type-level and per-operation policy.
    // Composite flags (e.g. round_nearest = bit5 | snap) require all
    // their bits set — having a subset like just `snap` does NOT match.
    template <insidable B, typename P, policy_flag F>
    inline constexpr bool HasPolicy = has_flag(InsidePolicy<B>, F) || plain<P>::test(F);

    // rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
    template <insidable L, typename P>
    inline constexpr round_mode rounding_for =
        HasPolicy<L, P, round_floor>     ? round_mode::floor
      : HasPolicy<L, P, round_ceil>      ? round_mode::ceil
      : HasPolicy<L, P, round_half_even> ? round_mode::half_even
      : HasPolicy<L, P, round_nearest>   ? round_mode::nearest
      :                                    round_mode::trunc;

    // v rounded onto L's lattice {k·Notch} by rounding_for<L, P> (value index,
    // ties half away from zero) — not limited to [Lower, Upper], so wrap can
    // round first and fold an on-lattice value after. Lower/Notch is an integer
    // on every valid grid, so the lattice points are exactly the grid's.
    template <insidable L, typename P>
    [[nodiscard]] constexpr rational round_to_lattice(rational v)
    {
      if constexpr (Notch<L> == 0)
        return v;
      else
      {
        const rational qv = (v / Notch<L>).value();
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
        return (rational{k} * Notch<L>).value();
      }
    }

    // Rounds the split offset quotient q + r/den (r < den ≤ imax_max) per L's
    // rounding policy — q/r form so no expression can overflow umax
    // (num + den/2 could, for num near umax). Shared by round_quotient's
    // offset rule and the 128-bit wide store (assignment.hpp).
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
          (Notch<L> == rational{0})
            ? rational{0}
            : (Lower<L> / Notch<L>).value_or(rational{0});
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

    // A single-point source (Lower == Upper) carries one value, so the only
    // question is whether it lands on L's grid — admitting e.g. `3_ins` into
    // `{{0,9},3}` while rejecting `1_ins` and out-of-range points.
    template <typename L, typename R>
    inline constexpr bool point_exactly_assignable =
      (Lower<R> == Upper<R>) && Grid<L>.representable(Lower<R>);

    // Tail of the policy cascade: checked reports.
    // Returns true if a policy handled the failure (caller should return).
    // Cheap default — reports through the static category message (no string).
    template <insidable B, typename P>
    constexpr bool domain_fail([[maybe_unused]] B& b, P&& policy)
    {
      if (policy.domain_check())
      {
        policy.report(errc::domain_error);
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
      || ((InsidePolicy<L> | P) & (wrap | clamp)) != 0
      || not excludes(Interval<L>, Interval<R>);

    template <typename L, typename R, policy_flag P>
    concept assign_notch_ok =
      !insidable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
      || ((InsidePolicy<L> | P) & snap) != 0
      || point_exactly_assignable<L, R>;
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
  template <typename Dst, typename Src, policy_flag P = InsidePolicy<Dst>>
  inline constexpr bool why_assignable =
    inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace beman::inside


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
  // needs_runtime_domain_check<L, P, A>: true iff any out-of-range handler would
  // fire (an action, a clamp/wrap bit, or default-throw under checked).
  // When false (typically `unsafe`, no action) the runtime range branch in
  // `assign` is dead code and skipped, letting the autovectorizer kick in.
  //---------------------------------------------------------------------------
  template <insidable L, typename P, typename A>
  inline constexpr bool needs_runtime_domain_check =
         clamp_action   <plain<A>>
      || wrap_action    <plain<A>>
      || error_action   <plain<A>>
      || HasPolicy<L, P, clamp>
      || HasPolicy<L, P, wrap>
      || (HasPolicy<L, P, checked> && !HasPolicy<L, P, ignore_domain>);

  // Shared out-of-range policy cascade. Order: clamp/wrap/error *actions*, then
  // clamp/wrap *policy* bits, then `domain_fail`. The three caller-supplied
  // callables cover how clamp/wrap store and the error-message rhs view. `Wrappable` is false on the fractional
  // path (no wrap *action* branch). Returns true when a handler resolved the write.
  template <bool Wrappable, insidable L, typename P, typename A,
            typename DoClamp, typename DoWrap, typename MsgView>
  constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action,
                                       DoClamp do_clamp, DoWrap do_wrap,
                                       [[maybe_unused]] MsgView msg_view)
  {
    using PA = plain<A>;
    if constexpr (clamp_action<PA>)
    { do_clamp(); return true; }
    else if constexpr (Wrappable && wrap_action<PA>)
    { do_wrap(); return true; }
    else if constexpr (error_action<PA>)
    {
      action.fn(lhs, errc::domain_error, errc_message(errc::domain_error));
      return true;
    }
    else if constexpr (HasPolicy<L, P, clamp>)
    { do_clamp(); return true; }
    else if constexpr (HasPolicy<L, P, wrap>)
    { do_wrap(); return true; }
    else
      return domain_fail(lhs, policy);
  }

  //---------------------------------------------------------------------------
  // assignment
  //---------------------------------------------------------------------------
  template <typename L, typename R>
  struct assignment;

  //---------------------------------------------------------------------------
  // assign(insidable, integral)
  //---------------------------------------------------------------------------
  template <insidable L, std::integral R>
  struct assignment<L,R>
  {
    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, imax lower, imax upper, A&& action)
      {
        // Pre: rhs is out of [lower, upper] (only called from handle_out_of_range),
        // so the two-way pick is the full clamp.
        imax clamped = static_cast<imax>(rhs) < lower ? lower : upper;
        imax overshoot = static_cast<imax>(rhs) - clamped;
        from_value(lhs, clamped);
        if constexpr (clamp_action<plain<A>>)
          action.fn(lhs, overshoot);
      }

      template<typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, imax lower, imax upper, A&& action)
      {
        // Overflow-safe modular wrap: `upper-lower+1` and `rhs-lower` can exceed imax,
        // so the reduction runs in umax (both fit umax for any valid grid; the result
        // lands back in [lower, upper] ⊂ imax).
        const umax urange = static_cast<umax>(upper) - static_cast<umax>(lower) + 1u;
        const imax ri = static_cast<imax>(rhs);
        if (urange == 0)                              // span == 2^64−1: wrap is identity
        {
          from_value(lhs, ri);
          if constexpr (wrap_action<plain<A>>) action.fn(lhs, imax{0});
          return;
        }
        umax w;
        imax excess;
        if (ri >= lower)
        {
          const umax dist = static_cast<umax>(ri) - static_cast<umax>(lower);  // true, ≥ 0
          w = dist % urange;
          excess = static_cast<imax>(dist / urange);
        }
        else
        {
          const umax dist = static_cast<umax>(lower) - static_cast<umax>(ri);  // true, > 0
          const umax m = dist % urange;
          w = (m == 0) ? 0u : (urange - m);
          excess = -static_cast<imax>((dist + urange - 1u) / urange);          // −ceil(dist/range)
        }
        from_value(lhs, static_cast<imax>(static_cast<umax>(lower) + w));
        if constexpr (wrap_action<plain<A>>)
          action.fn(lhs, excess);
      }

      template<typename P, typename A>
      static constexpr bool handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, lower, upper, action); },
          [&]{ apply_wrap (lhs, rhs, lower, upper, action); },
          [&]{ return rhs; });
      }

      static constexpr void store(L& lhs, R rhs)
      {
        if constexpr (!index_raw<L>)
          lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (Lower<L> == Upper<L>)
          lhs = L::from_raw(0);   // notch_storage point grid: 0 is the only offset
        else if constexpr (HasQFormatFastPath<L>)
          lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage on a notch 1/K grid: the offset is an exact integer
        {
          rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
          lhs = L::from_raw(raw_cast<L>(raw.Numerator));
        }
      }

    public:
      // An integer lands on L's grid whenever the notch is 1/K over an integer
      // Lower (or the grid is continuous); otherwise it may fall between notches
      // and must round or report exactly like the same value given as a rational.
      static constexpr bool integers_on_grid =
          Notch<L> == 0 || (Notch<L>.Numerator == 1 && abs_den(Lower<L>.Denominator) == 1);

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        static_assert(not excludes(Interval<L>, Interval<R>));

        if constexpr (!integers_on_grid)
          return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
        else
        {
          // The out-of-range check runs unconditionally — clamp/wrap
          // policies handle it via apply_*, which is constexpr-clean. Only the
          // unhandled-checked path winds up calling `policy.report`, which
          // contains its own `std::is_constant_evaluated()` guard.
          if constexpr (not includes(Interval<L>, Interval<R>))
          {
            if constexpr (IsIntegerInterval<L>)
            {
              // Skip the runtime range branch entirely when every handler would
              // be dead anyway — the dead branch otherwise inhibits autovec.
              if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
              {
                constexpr imax lower = LowerImax<L>;
                constexpr imax upper = UpperImax<L>;
                if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper) [[unlikely]]
                {
                  // The integer clamp/wrap formulas need consecutive integers to be
                  // adjacent grid points (notch 1); a finer notch wraps modulo
                  // span + notch on the rational path.
                  if constexpr (Notch<L> == 1)
                  {
                    if (handle_out_of_range(lhs, rhs, lower, upper, policy, action)) return lhs;
                  }
                  else
                    return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                }
              }
            }
            else if (not includes(Interval<L>, rhs))
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
      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action)
      {
        R clamped = (rhs < Lower<L>) ? static_cast<R>(Lower<L>) : static_cast<R>(Upper<L>);
        R overshoot;
        if constexpr (std::same_as<R, rational>)
          overshoot = (rhs - clamped).value_or(rational{0});
        else
          overshoot = rhs - clamped;

        // The clamp target is an interval endpoint — a grid point — so the slot is 0
        // or NotchCount, no rounding. real takes the endpoint as a double, rational
        // the exact constant (a double round-trip would lose non-dyadic endpoints);
        // raw_from_offset<L> adds Lower back for direct-encoded storage.
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((rhs < Lower<L>) ? static_cast<double>(Lower<L>)
                                             : static_cast<double>(Upper<L>));
        else if constexpr (rational_raw<L>)
          lhs = L::from_raw((rhs < Lower<L>) ? Lower<L> : Upper<L>);
        else
          lhs = L::from_raw(raw_from_offset<L>(
              (rhs < Lower<L>) ? umax{0} : NotchCount<L>));

        if constexpr (clamp_action<plain<A>>)
          action.fn(lhs, overshoot);
      }

    public:
      // Exposed (not private) so the insidable-rhs wrap path can reuse the
      // rational specialization's modular wrap on fractional/notch grids, and
      // so the wrap path can reuse store_checked after computing the wrapped
      // value.
      //
      // apply_wrap for real R — modular reduction into [Lower, Lower + range)
      // followed by store_checked so the rounding policy still applies if rhs
      // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
      {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        rational rhs_r{rhs};
        if constexpr (HasPolicy<L, P, snap>)
          rhs_r = round_to_lattice<L, P>(rhs_r);
        rational lower_r = Lower<L>;
        rational range   = ((Upper<L> - lower_r).value() + Notch<L>).value();
        // q = floor((rhs - lower) / range), wrapped = rhs - q * range
        rational shifted = (rhs_r - lower_r).value();
        imax q = floor((shifted / range).value());
        rational wrapped = (rhs_r - (rational{q} * range).value()).value();

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

        if constexpr (wrap_action<plain<A>>)
          action.fn(lhs, q);
      }

      // 128-bit rounded store — the offset slot of an in-range rhs computed
      // directly in wide arithmetic when the exact 64-bit rational formation
      // of (rhs − Lower)/Notch overflows (full-mantissa fp-derived sources on
      // grids with large |Lower|):
      //     slot + remainder/divisor = (rhs − Lower)·d_n / (a_dr·a_dl·n_n)
      // The compile-time divisor factors are gcd-reduced first, so the only
      // wide operations are one 128×64 multiply and one 128÷64 divide.
      // ok == false when the reduced divisor or dividend exceeds the 128-bit
      // envelope (or rhs is out of range — callers check range first).
      struct wide_quotient { umax slot; umax remainder; umax divisor; bool ok; };

      static constexpr wide_quotient wide_offset_quotient(rational const& rv)
      {
        if constexpr (Notch<L> == 0)
          return {};                       // continuous grids never index slots
        else
        {
          constexpr umax n_l     = Lower<L>.Numerator;
          constexpr umax a_dl    = abs_den(Lower<L>.Denominator);
          constexpr bool low_neg = Lower<L>.Denominator < 0;
          constexpr umax n_n     = Notch<L>.Numerator;   // Notch > 0: d_n > 0
          constexpr umax d_n     = static_cast<umax>(Notch<L>.Denominator);

          // Compile-time divisor part; a grid whose a_dl·n_n cannot fit umax
          // is beyond the wide envelope entirely.
          constexpr umax den_ct = []{
            umax p;
            return mul_overflow(a_dl, n_n, &p) ? umax{0} : p;
          }();
          if constexpr (den_ct == 0)
            return {};
          else
          {
            constexpr umax g1   = std::gcd(d_n, den_ct);
            constexpr umax d_n1 = d_n / g1;
            constexpr umax den1 = den_ct / g1;

            const umax a_dr    = abs_den(rv.Denominator);
            const bool rhs_neg = rv.Denominator < 0;

            // Offset numerator over the common denominator a_dr·a_dl:
            //   s_r·n_r·a_dl − s_l·n_l·a_dr  (≥ 0 for in-range rhs).
            // Each product is < 2^127 (numerator < 2^64, denominator ≤ imax),
            // so the same-sign sum below cannot carry out of 128 bits.
            const u128 val = umul(rv.Numerator, a_dl);
            const u128 low = umul(n_l, a_dr);
            u128 offset;
            if (!rhs_neg && low_neg)
              offset = u128{val.hi + low.hi + (val.lo + low.lo < val.lo ? 1u : 0u),
                            val.lo + low.lo};
            else if (!rhs_neg && !low_neg)
            {
              if (cmp128(val, low) < 0) return {};         // rhs < Lower
              offset = u128{val.hi - low.hi - (val.lo < low.lo ? 1u : 0u),
                            val.lo - low.lo};
            }
            else if (rhs_neg && low_neg)
            {
              if (cmp128(low, val) < 0) return {};         // rhs < Lower
              offset = u128{low.hi - val.hi - (low.lo < val.lo ? 1u : 0u),
                            low.lo - val.lo};
            }
            else
              return {};                                   // rhs < 0 ≤ Lower

            const umax g2    = std::gcd(d_n1, a_dr);
            const umax d_n2  = d_n1 / g2;
            const umax a_dr1 = a_dr / g2;

            umax divisor;
            if (mul_overflow(a_dr1, den1, &divisor)
                || divisor > static_cast<umax>(std::numeric_limits<imax>::max()))
              return {};

            const mul128_result dividend = mul128(offset, d_n2);
            if (dividend.overflowed)
              return {};

            const divmod128_result qr = divmod128(dividend.value, divisor);
            if (qr.quotient.hi != 0)
              return {};                    // slot beyond any 64-bit index space
            return {qr.quotient.lo, qr.remainder, divisor, true};
          }
        }
      }

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {})
      {
        if constexpr (rational_raw<L> && Notch<L> == 0)
        { lhs = L::from_raw(rhs); return true; }   // continuous: store verbatim
        else if constexpr (fp_raw<L>)
        {
          // real target: raw IS the value — snap to the dyadic grid (range handling
          // already ran in the assign cascade; finite guard mirrors store_f64's).
          const double v = static_cast<double>(rhs);
          if (!(v - v == 0))
            detail::raise(errc::not_finite, "non-finite double");
          lhs = L::from_raw(snap_double<Grid<L>, rounding_for<L, P>>(v));
          return true;
        }
        else if constexpr (Lower<L> == Upper<L>)
        {
          // Singleton grid: offset encoding → Raw=0; rational/direct → Raw = Lower.
          if constexpr (rational_raw<L>)
            lhs = L::from_raw(Lower<L>);
          else if constexpr (!index_raw<L>)
            lhs = L::from_raw(raw_cast<L>(RawLo<L>));
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
              lhs = L::from_raw((Lower<L> + (rational{k} * Notch<L>).value()).value());
            else
              lhs = L::from_raw(raw_from_offset<L>(k));
          };

          constexpr bool has_round_flag =
               HasPolicy<L, P, round_nearest> || HasPolicy<L, P, round_floor>
            || HasPolicy<L, P, round_ceil>    || HasPolicy<L, P, round_half_even>
            || HasPolicy<L, P, snap>;

          // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
          // (num − Lo·aden)·(K/g) / (aden/g), g = gcd(aden, K) — one gcd + integer ops
          // instead of two rational ops. round_quotient is invariant under reduction,
          // so the slot is bit-identical to the rational path. Oversized denominators
          // fall through (the kMaxDen guard keeps every product inside imax).
          if constexpr (HasQFormatFastPath<L> && !fp_raw<L> && Notch<L> != 0)
          {
            constexpr imax K  = abs_den(Notch<L>.Denominator);
            constexpr imax Lo = LowerImax<L>;
            constexpr umax kKM = []{
              // 2 · K · M with saturation (M bounds |value| and the offset span)
              umax k = static_cast<umax>(K);
              umax m = static_cast<umax>(
                  ceil(((detail::abs(Lower<L>) > detail::abs(Upper<L>)
                      ? detail::abs(Lower<L>) : detail::abs(Upper<L>))
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
          // source denominator × fine notch). Recompute the slot directly in
          // 128-bit (wide_offset_quotient above); only a result beyond even
          // that envelope reports errc::overflow — never an unchecked expected deref,
          // which would escape noexcept callers (the math engines) as
          // terminate. Rounding here is the offset rule (round_offset), the
          // same semantics round_quotient falls back to past 64 bits.
          const auto quotient = (rhs - Lower<L>)/Notch<L>;
          if (!quotient.has_value()) [[unlikely]]
          {
            const wide_quotient wide = wide_offset_quotient(rational{rhs});
            if (!wide.ok)
            {
              if constexpr (error_action<plain<A>>)
              { action.fn(lhs, errc::overflow, errc_message(errc::overflow)); return false; }
              policy.report(errc::overflow);
              return false;
            }
            if (wide.remainder == 0)
            { store_slot(wide.slot); return true; }
            if constexpr (has_round_flag)
            { store_slot(round_offset<L, P>(wide.slot, wide.remainder, wide.divisor)); return true; }
            if (policy.round_check()) [[unlikely]]
            {
              if constexpr (error_action<plain<A>>)
              { action.fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
              policy.report(errc::rounding_error);
              return false;
            }
            store_slot(round_offset<L, P>(wide.slot, wide.remainder, wide.divisor));
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
            if constexpr (error_action<plain<A>>)
            { action.fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
            policy.report(errc::rounding_error);
            return false;
          }
          else
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          return true;
        }
      }

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        if (not includes(Interval<L>, rhs)) [[unlikely]]
        {
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
      // Notch<L>), both integer (the hot path, collapses to integer math).
      static constexpr rational calcOffset()
      {
        if constexpr (rational_raw<L>)
          return Lower<R>;
        else if constexpr (Notch<L> == 0)
          // Continuous fp_raw L: no grid to land on, mapping unused (store
          // routes through snap_double). 0 avoids the /Notch<L> divide-by-zero.
          return rational{0};
        else if constexpr (rational_raw<R>)
          return -(Lower<L>/Notch<L>).value();
        else
          return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (rational_raw<L>)
          return Notch<R>;
        else if constexpr (Notch<L> == 0)
          // Continuous fp_raw L (see calcOffset). A denominator-1 Factor also
          // makes assign_notch_ok vacuously true (any value representable).
          return rational{0};
        else if constexpr (rational_raw<R>)
          return (rational{1}/Notch<L>).value();
        else
          return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space integer-only mapping — requires integer raw storage on both
      // sides (not rational, not real).
      static constexpr bool is_integer_mapping =
          !rational_raw<L> && !rational_raw<R>
          && !fp_raw<L> && !fp_raw<R>
          && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1;

      // Non-integer mapping folded to one integer multiply-add:
      //   Offset + Factor·raw = (o_s·f_d + raw·f_n·o_d) / (o_d·f_d)
      // with every coefficient compile-time. round_quotient is invariant under
      // fraction reduction, so rounding the unreduced pair is bit-identical to
      // reducing through the two rational ops first. ok gates on every product
      // (including the worst-case runtime numerator over R's raw range)
      // provably fitting imax; mul/add/den are zeroed when not ok.
      struct affine_map_t { imax mul; imax add; imax den; bool ok; };
      static constexpr affine_map_t affine_map = []{
        constexpr affine_map_t no{0, 0, 0, false};
        if constexpr (rational_raw<L> || rational_raw<R> || fp_raw<L> || fp_raw<R>
                      || Notch<L> == 0 || is_integer_mapping)
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
          if (mul_overflow(f_n, o_d, &m.mul) || mul_overflow(o_s, f_d, &m.add)
              || mul_overflow(o_d, f_d, &m.den))
            return no;
          // worst-case |numerator| over R's raw range
          constexpr imax hi_mag = RawHi<R> < 0 ? -RawHi<R> : RawHi<R>;
          constexpr imax lo_mag = RawLo<R> < 0 ? -RawLo<R> : RawLo<R>;
          const imax rmax = hi_mag > lo_mag ? hi_mag : lo_mag;
          imax term, num;
          if (mul_overflow(rmax, m.mul, &term)
              || add_overflow(term, m.add < 0 ? -m.add : m.add, &num))
            return no;
          // round_quotient equivalence: rounding is reduction-invariant, but
          // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
          // imax — mirror those checks for the unreduced den so both forms
          // take the same branch (ties on negatives differ across branches).
          constexpr auto zl = (Lower<L> / Notch<L>).value_or(rational{0});
          if (abs_den(zl.Denominator) == 1)
          {
            if (zl.Numerator > cap)
              return no;
            const imax mbias = signed_numerator(zl);
            imax mdi, total;
            if (mul_overflow(mbias, m.den, &mdi) || add_overflow(mdi, num, &total))
              return no;
          }
          return m;
        }
      }();

      // Map rhs.Raw into L's raw space (requires is_integer_mapping). The
      // Offset/Factor formula assumes offset encoding both sides; for direct
      // storage, subtract Lower<R> first (R-value → R-offset) and add Lower<L>
      // after (raw_from_offset<L>). All integer (is_integer_mapping guarantees it).
      static constexpr imax map_raw(auto rhs_raw)
      {
        imax r_offset = rhs_raw;
        if constexpr (!index_raw<R>)
          r_offset -= RawLo<R>;

        // Offset is an exact integer here, so trunc(Offset) is a constexpr constant.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + trunc(Offset);

        if constexpr (!index_raw<L>)
          return l_offset + RawLo<L>;
        else
          return l_offset;
      }

    private:
      // Grid of the wrap "excess"/carry handed to an on_wrap action:
      // floor((value − Lower) / range) for value ∈ R's interval (range = span + notch).
      // Both operands are bounds, so — like the clamp overshoot — the carry has a
      // known range and is delivered as an inside, not a raw imax.
      static constexpr grid wrap_excess_grid()
      {
        constexpr rational range = ((Upper<L> - Lower<L>).value() + Notch<L>).value();
        return grid{ floor(((Lower<R> - Lower<L>).value() / range).value()),
                     floor(((Upper<R> - Lower<L>).value() / range).value()) };
      }

      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action)
      {
        // RawLo/RawHi are already the correct Raw (no raw_from_offset). Real storage
        // takes the endpoint as a double (RawLo/Hi truncate fractional dyadic endpoints).
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((as_rational(rhs) < Lower<L>)
            ? static_cast<double>(Lower<L>) : static_cast<double>(Upper<L>));
        else
          lhs = L::from_raw((as_rational(rhs) < Lower<L>)
            ? raw_cast<L>(RawLo<L>) : raw_cast<L>(RawHi<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are bounds, so the overshoot is too. It is always
        // in-grid and on-notch for Grid<R> − Grid<L>, so the construction is exact.
        if constexpr (clamp_action<plain<A>>)
        {
          constexpr grid OG = (Grid<R> - Grid<L>).value();
          beman::inside::inside<OG> overshoot{ (as_rational(rhs) - as_rational(lhs)).value() };
          action.fn(lhs, overshoot);
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
        if constexpr (IsIntegerInterval<L> && abs_den(Notch<L>.Denominator) == 1
                      && Notch<L>.Numerator == 1 && !fp_raw<R> && IsIntegerAligned<R>)
        {
          // Unit-integer fast path: modular wrap on the integer value.
          imax rhs_imax = trunc(as_rational(rhs));
          constexpr imax lower = LowerImax<L>;
          constexpr imax upper = UpperImax<L>;
          imax range = upper - lower + 1;
          imax shifted = rhs_imax - lower;
          // floor division: one divide yields both the wrap and the carry
          imax excess  = shifted / range;
          imax wrapped = shifted % range;
          if (wrapped < 0) { wrapped += range; --excess; }
          from_value(lhs, wrapped + lower);
          if constexpr (wrap_action<plain<A>>)
            action.fn(lhs, beman::inside::inside<wrap_excess_grid()>{excess});   // carry as an inside
        }
        else if constexpr (wrap_action<plain<A>>)
        {
          // Fractional destination with a wrap action: reuse the rational modular-wrap
          // path for the store/rounding, but wrap its imax carry `q` into an inside before
          // handing it to the user action.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy,
            beman::inside::on_wrap([&](auto& self, imax q){
              action.fn(self, beman::inside::inside<wrap_excess_grid()>{q});
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
          // real target: raw IS the value — decode the source and snap to the dyadic
          // grid (the offset machinery below mis-encodes a double raw).
          lhs = L::from_raw(snap_double<Grid<L>, rounding_for<L, P>>(as_double(rhs)));
        else if constexpr (rational_raw<L>)
          // rational target: raw IS the value — snap the decoded source through
          // the rational-rhs store (the offset machinery below would round the
          // VALUE to a notch index and store that number as the raw).
          assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy,
                                                 no_action{});
        else if constexpr (is_integer_mapping)
        {
          // exact: Factor and Offset have integer denominators, no rounding ambiguity
          if constexpr (Offset == 0 && Factor == 1)
            lhs = L::from_raw(raw_cast<L>(rhs.raw()));
          else
            lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
        }
        else if constexpr (affine_map.ok)
        {
          // Folded non-integer mapping: one multiply-add, then the same
          // round_quotient (invariant under reduction — bit-identical to the
          // rational chain below).
          const imax num = affine_map.add
                         + static_cast<imax>(rhs.raw()) * affine_map.mul;
          const umax q = round_quotient<L, P>(
              static_cast<umax>(num < 0 ? -num : num),
              static_cast<umax>(affine_map.den));
          lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<imax>(q))
                                    : raw_from_offset<L>(q));
        }
        else
        {
          rational rat = *(Offset + *(Factor * rhs.raw()));
          umax ad = static_cast<umax>(abs_den(rat.Denominator));
          // Round the L-offset to a notch index in VALUE space via round_quotient
          // (same as the scalar path), honouring every rounding mode.
          umax q = round_quotient<L, P>(rat.Numerator, ad);
          // rat is the L-offset; raw_from_offset<L> adds Lower<L> back for direct storage.
          lhs = L::from_raw((rat.Denominator < 0)
            ? raw_from_offset<L>(-static_cast<imax>(q))
            : raw_from_offset<L>(q));
        }
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
        // for them (matches the integral-rhs path); only strict policies reject it.
        static_assert(HasPolicy<L, P, wrap> || HasPolicy<L, P, clamp>
                      || not excludes(Interval<L>, Interval<R>),
          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(abs_den(Factor.Denominator) == 1 || HasPolicy<L, P, snap>
                      || point_exactly_assignable<L, R>,
          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        // A `real` source holds its value as a double raw, which the raw-mapping
        // formulas below would misread as an index: take the double path.
        if constexpr (fp_raw<R>)
          return assignment<L, double>::assign(lhs, as_double(rhs), policy, std::forward<A>(action));
        else if constexpr (not includes(Interval<L>, Interval<R>))
        {
          if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
          {
            if constexpr (is_integer_mapping)
            {
              if (imax mapped = map_raw(rhs.raw()); mapped < RawLo<L> || mapped > RawHi<L>)
                if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
            }
            else if (not includes(Interval<L>, as_rational(rhs)))
              if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
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

    static constexpr bool domain_check()
    {
      if (std::is_constant_evaluated()) return true;
      return test(checked) && not test(ignore_domain);
    }

    static constexpr bool round_check()
    {
      if (std::is_constant_evaluated()) return true;
      return test(checked) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error); throw mode funnels through the
    // installed handler via an outlined cold helper. The constant-evaluation
    // guard names a fixed-string diagnostic for a clearer compile-time message
    // than "non-constexpr function called".
    constexpr void report(errc code)
    {
      if (std::is_constant_evaluated())
        detail::constexpr_error<
          "inside: value out of range during constant evaluation "
          "(checked policy hit; choose clamp/wrap or widen the interval)">();
      if constexpr (std::is_same_v<E, detail::error_ref>)
        E::Code = E::Code != errc{} ? E::Code : code;
      else
        detail::raise(code);
    }
  };

  policy(errc&) -> policy<none, detail::error_ref>;

  //---------------------------------------------------------------------------
  // IsPolicy — true for policy<F,E> specializations, false otherwise.
  // Used to gate free-fn overloads so they don't accidentally bind P = action tag.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>             inline constexpr bool IsPolicy = false;
    template<policy_flag F, typename E> inline constexpr bool IsPolicy<policy<F,E>> = true;

    // Concept form of IsPolicy — pulls cvref off so the constraint matches
    // forwarded `policy<F,E>` references in template parameters.
    template<typename T>
    concept policy_like = IsPolicy<std::remove_cvref_t<T>>;

    // policy_flags_of<T> — the flag-set a one-shot `policy<F,E>` carries (else
    // `none`). Lets the value+policy constructor and policy_ref's conversion fold
    // the per-call flags into their `inside_assignable` check, so a one-shot
    // clamp/round actually relaxes the constraint it enables.
    template<typename T>                inline constexpr policy_flag policy_flags_of = none;
    template<policy_flag F, typename E> inline constexpr policy_flag policy_flags_of<policy<F,E>> = F;

    // True for policy specializations that carry a beman::inside::errc& reference.
    // Free-fn arithmetic uses this to decide whether to call policy.report on
    // failure (which sets ec) vs. returning a silent std::unexpected (no-arg form).
    template<typename T>             inline constexpr bool UsesErrorRef = false;
    template<policy_flag F>          inline constexpr bool UsesErrorRef<policy<F, error_ref>> = true;
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
  // overflow_action<A> → fire it on a default Result; UsesErrorRef<P> →
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
      action.fn(res, code);
      return res;
    }
    else
    {
      if constexpr (UsesErrorRef<std::remove_cvref_t<P>>)
        policy.report(code);
      return std::unexpected{code};
    }
  }
  } // namespace detail

  //---------------------------------------------------------------------------
  // Named convenience policies — let user code skip `make_policy<F>()` entirely
  // for the per-call flag form on free arithmetic functions.
  //---------------------------------------------------------------------------
  inline constexpr auto truncated        = make_policy<snap>();
  inline constexpr auto round_to_nearest = make_policy<round_nearest>();
  inline constexpr auto clamped          = make_policy<clamp>();
  inline constexpr auto wrapped          = make_policy<wrap>();

  //---------------------------------------------------------------------------
  // policy_ref — variadic in actions (stores std::tuple<As...>). policy_ref
  // pre-picks the matching action per call path, so assignment/arithmetic keep
  // their single-A signatures. Payoff: the imax-probe and narrowing stages of a
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
    if constexpr (has_action<IsClampActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsClampActionPred>(actions));
    else if constexpr (has_action<IsWrapActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsWrapActionPred>(actions));
    else if constexpr (has_action<IsErrorActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsErrorActionPred>(actions));
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
      requires inside_assignable<Target, B, InsidePolicy<Target> | policy_flags_of<P>>
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
    static constexpr unsigned _clamp_count    = count_action_matches<IsClampActionPred,    As...>;
    static constexpr unsigned _wrap_count     = count_action_matches<IsWrapActionPred,     As...>;
    static constexpr unsigned _error_count    = count_action_matches<IsErrorActionPred,    As...>;
    static constexpr unsigned _overflow_count = count_action_matches<IsOverflowActionPred, As...>;

    static_assert(_clamp_count + _wrap_count + _error_count <= 1,
      "on_clamp / on_wrap / on_error are mutually exclusive in a single policy_ref");
    static_assert(_clamp_count    <= 1, "duplicate on_clamp");
    static_assert(_wrap_count     <= 1, "duplicate on_wrap");
    static_assert(_error_count    <= 1, "duplicate on_error");
    static_assert(_overflow_count <= 1, "duplicate on_overflow");

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
    // carried flags (notch/rounding) via HasPolicy's merge. Makes
    // `Target t = (a * b).with_snap();` / `return (a * b).with_snap();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <insidable Target>
      requires inside_assignable<Target, B, InsidePolicy<Target> | policy_flags_of<P>>
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
      if constexpr (has_action<IsErrorActionPred, As...>)
        pick_action_in<IsErrorActionPred>(Actions).fn(Ref, code, what);
      else if constexpr (!HasPolicy<B, P, ignore_zero>)
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
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, result.error());
          else
            Policy.report(result.error());
          return Ref;
        }
        return assign_with_picked(result.value());
      }
      else
        return assign_with_picked(std::forward<R>(result));
    }

    // Shared body for the fractional `+=`/`-=`/`*=`/`/=` operators: the rational
    // RHS lifts Ref to rational and routes the checked result through
    // `finalise_arith`; any other fractional RHS lifts both sides to double.
    template <fractional C, typename RatOp, typename DblOp>
    constexpr B& fractional_assign(C const& rhs, RatOp rat_op, DblOp dbl_op,
                                   const char* msg)
    {
      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rat_op(rational{Ref}, rhs), msg);
      else
        return assign_with_picked(dbl_op(static_cast<double>(Ref),
                                         static_cast<double>(rhs)));
    }
    public:

    template <insidable C>
    constexpr B& operator+=(C const& rhs)
    { return finalise_arith(Ref + rhs, "policy_ref::operator+= overflow"); }

    template <insidable C>
    constexpr B& operator-=(C const& rhs)
    { return finalise_arith(Ref - rhs, "policy_ref::operator-= overflow"); }

    template <insidable C>
    constexpr B& operator*=(C const& rhs)
    { return finalise_arith(Ref * rhs, "policy_ref::operator*= overflow"); }

    template <insidable C>
    constexpr B& operator/=(C const& rhs)
    { return finalise_arith(Ref / rhs, "policy_ref::operator/= division/overflow"); }

    template <insidable C>
    constexpr B& operator%=(C const& rhs)
    { return finalise_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow"); }

    //-------------------------------------------------------------------------
    // rational RHS overloads — the only non-inside operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_ins` / `just<1>` / `inside<{lo,hi}>{n}`).
    // Lift Ref to rational, checked op, finalise_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a + b; },
        [](double a, double b){ return a + b; }, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a - b; },
        [](double a, double b){ return a - b; }, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a * b; },
        [](double a, double b){ return a * b; }, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(C const& rhs)
    {
      if (is_canonical_zero(rhs))
      {
        report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }
      return fractional_assign(rhs, [](rational a, rational b){ return a / b; },
        [](double a, double b){ return a / b; }, "policy_ref::operator/= division/overflow");
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
        has_flag(InsidePolicy<Lhs>, real) || has_flag(InsidePolicy<Rhs>, real);
    static constexpr bool any_f32 =
        has_flag(InsidePolicy<Lhs>, f32) || has_flag(InsidePolicy<Rhs>, f32);
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
        (InsidePolicy<Lhs> | InsidePolicy<Rhs>)
        & (exact | (ResultGrid.Notch == 1 ? direct : none) | (ResultGrid.Notch != 0 ? indexed : none));
    static constexpr policy_flag rep =
        carried
        | (keep_f64 ? real : none) | (keep_f32 ? f32 : none);
    // The result inside's policy: the propagated representation plus the
    // operands' `checked` (a representation flag must not switch checking off),
    // or plain checked.
    static constexpr policy_flag result_policy =
        rep != none ? rep | ((InsidePolicy<Lhs> | InsidePolicy<Rhs>) & checked) : checked;
  };
}

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = Grid<L> + Grid<R>.
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
    static_assert((Grid<L> + Grid<R>).has_value(),
      "addition: result grid's notch/interval exceeds the representable rational "
      "range — coarsen the operand grids");
    static constexpr grid result_grid = (Grid<L> + Grid<R>).value();
    // fp / representation propagation — shared rule in detail/rep.hpp.
    using rep_t = fp_rep<L, R, result_grid>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && has_any_flag(F | InsidePolicy<L> | InsidePolicy<R>, checked | exact)
        && !rational_add_is_safe(Grid<L>, Grid<R>);

    template <policy_flag F = none>
    using return_type_for = std::conditional_t<needs_overflow_check<F>,
                                               std::expected<result, errc>,
                                               result>;

    template <policy_flag F, typename A>
    using add_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<F>>;

    // Mixed integer-aligned / notch-offset fast path: with a unit-numerator
    // result notch 1/d, both operand offsets in result-notch units are exact
    // integer math — (to_value − Lower)·d for the integer-aligned operand,
    // raw·widen for the notch-offset one (offsets compose because
    // Lower<result> = Lower<L> + Lower<R>). Gated on an index-raw result and
    // the result slot count fitting imax so no intermediate can overflow
    // (each operand contribution ≤ its own span/N ≤ the result slot count).
    static constexpr bool mixed_offset_ok = []{
      if constexpr (rational_raw<L> || rational_raw<R> || rational_raw<result>
                    || fp_raw<L> || fp_raw<R>          // double raws: no integer offset
                    || fp_raw<result> || !index_raw<result>
                    || (IsIntegerAligned<L> && IsIntegerAligned<R>)
                    || (index_raw<L> && index_raw<R>)
                    || Notch<result> == 0 || Notch<result>.Numerator != 1)
        return false;
      else
      {
        constexpr auto span = Upper<result> - Lower<result>;
        if (!span.has_value())
          return false;
        const auto slots = *span / Notch<result>;
        return slots.has_value()
            && (*slots).Numerator
                 <= static_cast<umax>(std::numeric_limits<imax>::max());
      }
    }();

    // One operand's offset in result-notch units (see mixed_offset_ok).
    template <insidable X>
    static constexpr imax mixed_offset_units(X const& x, imax widen)
    {
      if constexpr (IsIntegerAligned<X>)
      {
        constexpr imax den = static_cast<imax>(abs_den(Notch<result>.Denominator));
        return (to_value(x) - LowerImax<X>) * den;
      }
      else
        return raw_imax(x) * widen;
    }

    // Result notch is gcd(NL, NR); scale each raw up to it before adding —
    // lhs_widen = NL/Nresult, rhs_widen = NR/Nresult (exact, Nresult divides both).
    // A continuous result (Notch<result> == 0) has no widen (it takes the
    // rational path), so 1 stands in.
    static constexpr imax lhs_widen = (Notch<result> == 0) ? imax{1}
        : (Notch<L> / Notch<result>).value_or(rational{1}).Numerator;
    static constexpr imax rhs_widen = (Notch<result> == 0) ? imax{1}
        : (Notch<R> / Notch<result>).value_or(rational{1}).Numerator;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> add_return_t<F, A>
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
    else if constexpr (mixed_offset_ok)
    {
      // Mixed integer-aligned / notch-offset operands, pure integer offsets
      // (see mixed_offset_ok above).
      res = result::from_raw(raw_cast<result>(mixed_offset_units(lhs, lhs_widen)
                                            + mixed_offset_units(rhs, rhs_widen)));
    }
    else if constexpr (rational_raw<L> || rational_raw<R>
                       || !((IsIntegerAligned<L> && IsIntegerAligned<R>)
                            || (index_raw<L> && index_raw<R>)))
    {
      // Rational store: a rational-raw operand, or a mix the integer fast
      // paths can't express exactly (non-unit result notch numerator, or a
      // slot count past imax). Compute the exact rational sum and convert to
      // result's raw via raw_from_offset.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - Lower<result>) / Notch<result>).value().Numerator));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R>)
    {
      // Both operands are integer-valued (Notch and Lower integers), so the
      // value-space add is exact.
      from_value(res, to_value(lhs) + to_value(rhs));
    }
    else
    {
      // Both notch-offset: scale each raw to the result notch and add in offset
      // space (offsets compose because result Lower = Lower<L> + Lower<R>).
      res = result::from_raw(raw_cast<result>(raw_imax(lhs) * lhs_widen + raw_imax(rhs) * rhs_widen));
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
// multiplication — `mul(L, R, policy, action) -> inside<Grid<L> * Grid<R>>`. The
// integer hot path branches on which corner of the four-quadrant product hits
// `Lower<result>`, doing the arithmetic as `umax * umax` (no signed overflow)
// plus integer offset corrections. Rational-result and all-integer-aligned
// cases come first.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  template <insidable L, insidable R = L>
  struct multiplication
  {
    static_assert((Grid<L> * Grid<R>).has_value(),
      "multiplication: result grid's notch/interval exceeds the representable "
      "rational range — coarsen the operand grids");
    static constexpr grid result_grid = (Grid<L> * Grid<R>).value();
    // fp / representation propagation — shared rule in detail/rep.hpp. The product
    // grid (notch = N_L·N_R) is finer, so demotion/dropping is the common case.
    using rep_t = fp_rep<L, R, result_grid>;
    static constexpr bool dropped_fp = rep_t::dropped_fp;
    using result = inside<result_grid, rep_t::result_policy>;

    // The dropped-fp case lands on a rational result when the product grid outgrows
    // uint index space; its product numerator can exceed `umax`, so check it (the
    // result carries `checked`) rather than wrap.
    template <typename P>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && (has_any_flag(InsidePolicy<L> | InsidePolicy<R>, checked | exact)
            || plain<P>::test(checked) || dropped_fp)
        && !rational_mul_is_safe(Grid<L>, Grid<R>);

    template <typename P>
    using return_type_for = std::conditional_t<needs_overflow_check<P>,
                                               std::expected<result, errc>,
                                               result>;

    template <typename P, typename A>
    using mul_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<P>>;

    // `x * just<c>` (c != 0): the result lattice is x's lattice scaled by c
    // (see grid operator*), so the result offset IS x's offset — counted from
    // the far end when c < 0. No multiply at all.
    template <insidable Point, insidable X>
    static constexpr bool point_scale =
        Lower<Point> == Upper<Point> && Lower<Point> != 0
        && !rational_raw<X> && !fp_raw<X> && Notch<X> != 0
        && !rational_raw<result> && !fp_raw<result>;

    template <bool Negate, insidable X>
    static constexpr result scale_by_point(X const& x)
    {
      static_assert(NotchCount<result> == NotchCount<X>);
      umax off;
      if constexpr (index_raw<X>) off = static_cast<umax>(x.raw());
      else                        off = static_cast<umax>(raw_imax(x) - RawLo<X>);
      return result::from_raw(raw_from_offset<result>(Negate ? NotchCount<X> - off : off));
    }

    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> mul_return_t<P, A>
  {
    if constexpr (fp_raw<result>)
    {
      // Exact by construction, no snap (see addition.hpp): operands are notch
      // multiples, the product index |ia·ib| stays under the double_exact 2^53
      // gate, so the double multiply is exact and on the result lattice.
      return result::from_raw(raw_cast<result>(as_double(lhs) * as_double(rhs)));
    }
    else if constexpr (point_scale<R, L>)
      return scale_by_point<(Lower<R> < 0)>(lhs);
    else if constexpr (point_scale<L, R>)
      return scale_by_point<(Lower<L> < 0)>(rhs);
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<P>)
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
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R> && IsIntegerAligned<result>)
    {
      result res;
      from_value(res, to_value(lhs) * to_value(rhs));
      return res;
    }
    else if constexpr (fp_raw<L> || fp_raw<R> || rational_raw<L> || rational_raw<R>)
    {
      // An operand whose raw is a double/rational can't feed the integer
      // four-quadrant formula below (it reads the raw as an integer offset).
      // Combine exactly as rationals and convert to the result's storage —
      // mirrors addition's rational-mixed branch. Reached when `real` was
      // dropped from the result (grid not double-exact) but operands stay real.
      auto prod = rational::mul_unchecked(as_rational(lhs), as_rational(rhs));
      return result::from_raw(raw_from_offset<result>(
          ((prod - Lower<result>) / Notch<result>).value().Numerator));
    }
    else
    {
      // Result writes go through raw_from_offset so direct-storage results
      // get Lower<result> added back to recover the value.
      auto to_result = [](auto raw_offset)
      { return result::from_raw(raw_from_offset<result>(static_cast<umax>(raw_offset))); };

      // Normalize lhs.raw() / rhs.raw() to *offsets* regardless of L's / R's
      // storage shape. The formulas below all assume offset arithmetic.
      umax lhs_offset = !index_raw<L>
          ? static_cast<umax>(raw_imax(lhs) - RawLo<L>)
          : static_cast<umax>(lhs.raw());
      umax rhs_offset = !index_raw<R>
          ? static_cast<umax>(raw_imax(rhs) - RawLo<R>)
          : static_cast<umax>(rhs.raw());

      // Absolute notch index of each operand endpoint (Lower/Notch, Upper/Notch).
      constexpr umax idxLoL = (Lower<L>/Notch<L>).value_or(rational{0}).Numerator;
      constexpr umax idxLoR = (Lower<R>/Notch<R>).value_or(rational{0}).Numerator;
      constexpr umax idxHiL = (Upper<L>/Notch<L>).value_or(rational{0}).Numerator;

      // Integral promotion would make `raw * raw` an `int * int` (UB above
      // INT_MAX), so cast to umax to multiply in 64-bit unsigned space. The four
      // branches cover the sign quadrants: Lower<result> is one of the four
      // corner products; sign-flipped helpers (negative<L>/<R>) reduce each to
      // the all-positive formula. The static_assert guards the case analysis.
      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        return to_result(lhs_offset * rhs_offset
                         + lhs_offset * idxLoR
                         + rhs_offset * idxLoL);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        umax negLhs = NotchCount<L> - lhs_offset;
        return to_result(negLhs * idxLoR
                         + rhs_offset * idxHiL
                         - negLhs * rhs_offset);
      }

      if constexpr (Lower<result> == (Lower<L> * Upper<R>).value())
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      static_assert(Lower<result> == (Lower<L> * Lower<R>).value()
                 || Lower<result> == (Upper<L> * Upper<R>).value()
                 || Lower<result> == (Upper<L> * Lower<R>).value()
                 || Lower<result> == (Lower<L> * Upper<R>).value(),
                 "multiplication: internal logic error");
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
  inline constexpr bool integer_native_ops =
      ((F | InsidePolicy<L> | InsidePolicy<R>) & snap)
      && !rational_raw<L> && !rational_raw<R>
      && IsIntegerAligned<L> && IsIntegerAligned<R>;

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
      (LowerImax<L> > std::numeric_limits<std::int32_t>::min()
       && UpperImax<L> <= std::numeric_limits<std::int32_t>::max()
       && LowerImax<R> > std::numeric_limits<std::int32_t>::min()
       && UpperImax<R> <= std::numeric_limits<std::int32_t>::max()),
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
        ((F | InsidePolicy<L> | InsidePolicy<R>) & snap)
        && IsQFormat<L> && IsQFormat<R>
        && Notch<L> == Notch<R>;

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode =
        div_round_mode(F | InsidePolicy<L> | InsidePolicy<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (Grid<L> / Grid<R>).has_value(),
      "division: result grid not representable (notch/interval exceeds the "
      "representable rational range) — coarsen the operand grids");
    static_assert(!native_div_qformat || (Upper<L> / Notch<R>).has_value(),
      "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid =
        native_div_integer
            ? grid{round_rat_lo((*(Grid<L> / Grid<R>)).Interval.Lower, rmode),
                   round_rat_hi((*(Grid<L> / Grid<R>)).Interval.Upper, rmode)}
      : native_div_qformat
            ? grid{interval{rational{0}, (Upper<L> / Notch<R>).value()}, Notch<L>}
            : *(Grid<L> / Grid<R>);

    // fp / representation propagation — shared rule in detail/rep.hpp.
    // AllowContinuous: a continuous quotient (Notch 0) keeps fp verbatim.
    using rep_t = fp_rep<L, R, result_grid, /*AllowContinuous=*/true>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F | InsidePolicy<L> | InsidePolicy<R>, checked | exact);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>.
    static constexpr bool may_overflow_nonzero =
        !native_div && !fp_raw<result> && (needs_overflow_check<F> != 0);

    // Real division can still fail on a zero divisor, so it uses the same
    // return-type rule as the rest: plain `result` when the op cannot fail
    // (overflow-action, or the divisor grid excludes zero with no rational
    // overflow), else expected<result, errc>. Real has no rational overflow, so
    // may_overflow_nonzero is false for it (above).
    template <typename A>
    using div_return_t = std::conditional_t<
        overflow_action<plain<A>> || (DivisorExcludesZero<R> && !may_overflow_nonzero),
        result,
        std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr div_return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<insidable L, insidable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto division<L,R,F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> div_return_t<A>
  {
    // `fail` must stay well-formed even when div_return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    // Shared by the real and non-real paths (real fails only on a zero divisor).
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> div_return_t<A> {
      if constexpr (overflow_action<plain<A>>)
        return report_or_unexpected<result>(action, policy, code, what);   // -> result
      else if constexpr (!DivisorExcludesZero<R> || may_overflow_nonzero)
        return report_or_unexpected<result>(action, policy, code, what);   // -> expected<result, errc>
      else
        return result{};   // unreachable: divisor excludes zero, op cannot fail
    };

    // Div-by-zero check elided when R's grid excludes zero, or `ignore_zero` is
    // set (zero divisor is then UB, matching the `/= 0` no-op). The fail arms stay
    // keyed on DivisorExcludesZero (which narrows the return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | InsidePolicy<L> | InsidePolicy<R>) & ignore_zero) != 0);

    if constexpr (fp_raw<result>)
    {
      // Real division reports zero like every other path (throw / report /
      // action / unexpected). Finite operands keep the quotient finite, so no
      // non-finite ever reaches storage.
      if constexpr (!zero_unchecked)
        if (as_double(rhs) == 0.0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(raw_cast<result>(snap_double<Grid<result>, rmode>(as_double(lhs) / as_double(rhs))));
    }
    else if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (Lower<R> == 0). Formula folds to
      // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
      if constexpr (!zero_unchecked)
        if (rhs.raw() == 0) return fail(errc::division_by_zero, "division by zero in div");
      constexpr umax N = abs_den(Notch<L>.Denominator);
      // 32-bit divide when the scaled dividend fits (Q8.8, Q16.15, ...).
      using U = std::conditional_t<(NotchCount<L> <= std::numeric_limits<std::uint32_t>::max() / N),
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
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(native_mod, "modulo requires integer-valued grids and snap");

    static constexpr imax max_rem =
        (abs_den(LowerImax<R>) > abs_den(UpperImax<R>) ? abs_den(LowerImax<R>) : abs_den(UpperImax<R>)) - 1;

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode =
        div_round_mode(F | InsidePolicy<L> | InsidePolicy<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::trunc && LowerImax<L> >= 0)
        ? grid{imax{0}, max_rem}
        : grid{-max_rem, max_rem};

    using result = inside<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
    template <typename A>
    using mod_return_t = std::conditional_t<
        overflow_action<plain<A>> || DivisorExcludesZero<R>,
        result,
        std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr mod_return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
  };

  template<insidable L, insidable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto modulo<L,R,F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> mod_return_t<A>
  {
    using T = native_div_t<L, R>;
    const T rhs_val = static_cast<T>(to_value(rhs));
    // Zero check elided when R's grid excludes zero (mod_return_t is plain
    // `result`) or `ignore_zero` is set (zero divisor is then UB, matching `%= 0`).
    constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | InsidePolicy<L> | InsidePolicy<R>) & ignore_zero) != 0);
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
} // namespace beman::inside::detail


// ======================================================================
//  beman/inside/predicates.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or report an error:
//   will_conversion_overflow<B>(v) — v falls outside B's interval.
//   will_conversion_trunc<B>(v) — v is in-range but off-notch (would round).
//   is_conversion_lossy<B>(v)      — OR of the two.
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_overflow(A value) noexcept
  {
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return true;   // NaN / ±inf fit no grid (and must not raise here)
    return not includes(Interval<B>, detail::as_rational(value));
  }

  template <insidable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_trunc(A value) noexcept
  {
    if constexpr (detail::rational_raw<B> || Notch<B> == 0)
      return false;                       // rational raw / continuous grid: no notch to miss
    if constexpr (std::floating_point<A>)
      if (!(value - value == 0)) return false;   // non-finite — overflow, not truncation
    detail::rational r = detail::as_rational(value);
    if (not includes(Interval<B>, r))
      return false;                       // out-of-range — overflow, not truncation
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - Lower<B>) / Notch<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
  }

  template <insidable B, typename A>
  [[nodiscard]] constexpr bool is_conversion_lossy(A value) noexcept
  {
    return will_conversion_overflow<B>(value)
        || will_conversion_trunc<B>(value);
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
  template <insidable Out, insidable In> constexpr Out floor_impl(In x) noexcept;
  template <insidable Out, insidable In> constexpr Out ceil_impl (In x) noexcept;
  template <insidable Out, insidable In> constexpr Out round_impl(In x) noexcept;
  template <insidable Out, insidable In> constexpr Out trunc_impl(In x) noexcept;
  template <insidable Out, insidable In> constexpr Out abs_impl  (In x) noexcept;
  template <insidable In> constexpr auto floor(In x) noexcept;
  template <insidable In> constexpr auto ceil (In x) noexcept;
  template <insidable In> constexpr auto round(In x) noexcept;
  template <insidable In> constexpr auto trunc(In x) noexcept;
  template <insidable In> constexpr auto abs  (In x) noexcept;
}

//---------------------------------------------------------------------------
// inside — the public struct users include. Defines `inside<G, P>` and its
// per-instance operators; free-function arithmetic and `inside_range` also live
// here. Heavy lifting is delegated to addition/multiplication/division.hpp
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
    // Under the default (double) engine the `real` policy is double-backed, and
    // its value snaps to the grid (Lower + k·Notch). That snap is only exact
    // when the grid is dyadic — power-of-two notch and Lower — so grid points
    // are representable in IEEE-754 double. A continuous grid (Notch == 0) has
    // no grid to snap to. Anything else is rejected here rather than silently
    // demoted to integer storage.
    static_assert(!has_flag(P, real) || detail::dyadic_grid<G> || G.Notch == 0,
                  "inside: the `real`/`f64` policy requires a dyadic grid (power-of-two "
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
    using raw_type = detail::storage_for<G, P>;

    private:
    raw_type Raw;

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
      // NaN/±inf would reach snap_double's integer cast (UB); reject like the
      // non-real path. `v - v` is 0 for every finite v, NaN otherwise.
      if (!(v - v == 0))
        detail::raise(errc::not_finite, "non-finite double");
      const double lo = static_cast<double>(G.Interval.Lower);
      const double hi = static_cast<double>(G.Interval.Upper);
      if (v < lo || v > hi)
      {
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
        else if (detail::domain_fail(*this, pol))
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

    // Error-code construction: `inside x(value, ec)`. Needs its own overload (a raw
    // error_code would bind the Pol&& template above). On a reported (out-of-range)
    // error, ec is set and the inside's value is ill-defined — do not read it without
    // checking ec first.
    template <numeric A>
      requires inside_assignable<inside, A, P>
    constexpr inside(A value, errc& ec)
    { store_value(value, make_policy<P>(ec)); }

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
    //   operator double   — implicit for `real` bounds (dyadic grid → lossless);
    //                       explicit otherwise and gated on a rounding flag.
    //                       Strict bounds opt in via `to<double>().value()`.
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

    constexpr explicit(!has_flag(P, real) && !has_flag(P, f32)) operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return detail::as_double(*this); }

    constexpr operator detail::rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (!detail::index_raw<inside>)
        return Raw;

      // Q-format-with-integer-Lower fast path skips the generic path's three
      // rational ops. Falls through to the rational path when the raw is too wide
      // to widen safely (e.g. uint64 from a Q16.16 × Q16.16 result type).
      if constexpr (detail::HasQFormatFastPath<inside>)
        return detail::q_format_decode(*this);

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
      constexpr bool check_lo = Lower<inside> < detail::rational{lim::min()};
      constexpr bool check_hi = Upper<inside> > detail::rational{lim::max()};
      if constexpr (!check_lo && !check_hi)
        return static_cast<T>(detail::to_value(*this));
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
    [[nodiscard]] constexpr imax numerator() const   { return fraction().first; }
    [[nodiscard]] constexpr imax denominator() const { return fraction().second; }

    private:
    // The reduced exact value as {numerator, positive denominator}. Integer
    // grids need no division; dyadic Q-format grids reduce by shifting out
    // common factors of two instead of a gcd.
    constexpr std::pair<imax, imax> fraction() const
    {
      if constexpr (detail::index_raw<inside> && detail::IsIntegerAligned<inside>)
        return {detail::to_value(*this), 1};
      else if constexpr (detail::index_raw<inside> && detail::HasQFormatFastPath<inside>
                         && std::has_single_bit(detail::abs_den(G.Notch.Denominator)))
      {
        constexpr imax nd = detail::abs_den(G.Notch.Denominator);
        constexpr int  k  = std::countr_zero(static_cast<umax>(nd));
        const imax num = detail::raw_imax(*this) + detail::LowerImax<inside> * nd;
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
    // or `beman::inside::math::floor_impl<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<beman/inside/cmath.hpp>`.

    [[nodiscard]] constexpr negative operator-() const
    {
      negative neg;
      if constexpr (detail::fp_raw<inside>)
        neg = negative::from_raw(-Raw);
      else if constexpr (detail::rational_raw<inside>)
        neg = negative::from_raw(-(Raw));
      else if constexpr (!detail::index_raw<inside> || !detail::index_raw<negative>)
        detail::from_value(neg, -detail::to_value(*this));
      else
        // Unsigned-offset fast path: with `value = Raw*Notch + Lower`, negating
        // is `NotchCount - Raw` (index from the opposite end) — no rational ops.
        // Unreachable for direct storage, so `Raw` here is guaranteed an offset.
        neg = negative::from_raw(detail::raw_cast<negative>(detail::NotchCount<inside> - Raw));
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
    // `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs` — overflow probe fires λ1,
    // post-probe narrowing fires λ2.
    template <typename... Actions>
    [[nodiscard]] constexpr auto with(Actions&&... actions)
    {
       constexpr policy_flag merged = detail::merged_implied_flags<Actions...>;
       auto pol = make_policy<P | merged>();
       return detail::policy_ref<inside, decltype(pol), std::remove_cvref_t<Actions>...>{
         *this, pol,
         std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

    template <insidable R>
    constexpr inside& operator+=(R const& rhs)
    {
      // Point-inside rhs (just<v> / 1_ins / ++) whose value is a whole number of
      // this grid's notches: the raw delta is a compile-time constant and the
      // raw encoding cancels every Lower term (raw(v+d) = raw(v) + d/Notch for
      // offset and direct storage alike), so this compiles to one integer add.
      if constexpr (!detail::rational_raw<inside> && !detail::fp_raw<inside> && Notch<inside> != 0
                    && Lower<R> == Upper<R>
                    && (Lower<R> / Notch<inside>).has_value()
                    && detail::abs_den((*(Lower<R> / Notch<inside>)).Denominator) == 1)
      {
        constexpr imax delta = signed_numerator(*(Lower<R> / Notch<inside>));
        return store_raw(detail::raw_imax(*this) + delta);
      }
      // Fast path: raw-level integer addition, safe when raw_a + raw_b is the raw
      // of value_a + value_b — direct storage, or offset encoding with Lower==0 both.
      else if constexpr (!detail::rational_raw<inside> && !detail::rational_raw<R>
                    && !detail::fp_raw<inside> && !detail::fp_raw<R>
                    && Notch<inside> == Notch<R>
                    && (!detail::index_raw<R>
                        || (Lower<inside> == 0 && Lower<R> == 0)))
        return store_raw(detail::raw_imax(*this) + detail::raw_imax(rhs));
      else
        return *this = *this + rhs;
    }

    private:
    // Store a raw computed by the raw-space fast paths of += and -=. Under
    // clamp/wrap/checked an out-of-range raw is clamped, wrapped or reported.
    constexpr inside& store_raw(imax new_raw)
    {
      constexpr imax lo = detail::RawLo<inside>, hi = detail::RawHi<inside>;
      if constexpr (P & (clamp | wrap | checked))
        if (new_raw < lo || new_raw > hi)
        {
          if constexpr (P & clamp)
            new_raw = new_raw < lo ? lo : hi;
          else if constexpr (P & wrap)
            new_raw = detail::euclid_mod(new_raw - lo, hi - lo + 1) + lo;
          else
          {
            make_policy<P>().report(errc::domain_error);
            return *this;
          }
        }
      Raw = detail::raw_cast<inside>(new_raw);
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
        // A failed op (an error) has already been reported through the policy
        // channel; keep *this unchanged instead of dereferencing — a
        // non-throwing installed handler must not turn into
        // bad_expected_access here. `*r` (not value()): no second check.
        if (r.has_value())
          *this = *r;
      }
      else
        *this = r;
      return *this;
    }

    constexpr inside& report_div_by_zero()
    {
      if constexpr (!(P & ignore_zero))
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
      // Lower<R>/Notch for an index-raw rhs (its raw is Lower-relative) and 0
      // for a value-raw rhs. Delegating to `+= (-rhs)` instead shifts R's
      // Lower by negation and defeats +='s raw path for index-backed grids.
      if constexpr (!detail::rational_raw<inside> && !detail::rational_raw<R>
                    && !detail::fp_raw<inside> && !detail::fp_raw<R>
                    && Notch<inside> != 0 && Notch<inside> == Notch<R>
                    && (!detail::index_raw<R>
                        || ((Lower<R> / Notch<inside>).has_value()
                            && detail::abs_den((*(Lower<R> / Notch<inside>)).Denominator) == 1)))
      {
        constexpr imax bias = [] {
          if constexpr (detail::index_raw<R>) return signed_numerator(*(Lower<R> / Notch<inside>));
          else                                return imax{0};
        }();
        return store_raw(detail::raw_imax(*this) - detail::raw_imax(rhs) - bias);
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
        return report_div_by_zero();
      return assign_op_result(*this / rhs);
    }

    template <insidable R>
    constexpr inside& operator%=(R const& rhs)
    {
      if (rhs == 0)
        return report_div_by_zero();
      return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator*=(A const& rhs)
    { return assign_op_result(detail::rational{*this} * rhs); }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator/=(A const& rhs)
    {
      if (detail::is_canonical_zero(rhs))
        return report_div_by_zero();
      return assign_op_result(detail::rational{*this} / rhs);
    }

    // ++/-- add the point inside `just<±1>` through the insidable += (which has
    // the raw-level integer fast path) instead of the rational round-trip,
    // which decodes to rational and re-stores through the full quotient/
    // rounding machinery (~30× the instructions on an integer grid). `just`
    // itself is declared after the class, so spell the point inside directly.
    constexpr inside& operator++()
    {
      // constexpr local: the point inside is materialised at compile time (the
      // ctor's error path otherwise blocks constant folding at -O3).
      constexpr auto one_b = inside<grid{detail::rational{1}}>{detail::rational{1}};
      return *this += one_b;
    }
    constexpr inside  operator++(int) { inside t = *this; ++*this; return t; }
    constexpr inside& operator--()
    {
      constexpr auto minus_one_b =
          inside<grid{detail::rational{-1}}>{detail::rational{-1}};
      return *this += minus_one_b;
    }
    constexpr inside  operator--(int) { inside t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value)
    {
      errc ec{};
      inside result;
      detail::assignment<inside, A>::assign(result, value, make_policy<P>(ec));
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
  // comparison
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Integer value-index comparison eligibility: an integer-backed inside
    // whose value indices (value/Notch — integral by the grid anchor
    // invariant) fit imax, so two same-notch bounds compare as
    // `bias + raw` without a rational decode.
    template <insidable B>
    inline constexpr bool index_cmp_fits = []{
      if constexpr (rational_raw<B> || fp_raw<B> || Notch<B> == 0)
        return false;
      else
      {
        constexpr auto lo = Lower<B> / Notch<B>;
        constexpr auto hi = Upper<B> / Notch<B>;
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
        constexpr auto lo = *(Lower<B> / Notch<B>);
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
    inline constexpr bool exact_in_double = fp_raw<B> || double_exact<Grid<B>>;

    // inside ⋈ inside (⋈ = `cmp`: <=> or ==) in the cheapest exact form the two
    // storage shapes allow.
    template <insidable L, insidable R, class Cmp>
    constexpr auto compare(L const& lhs, R const& rhs, Cmp cmp)
    {
      // same grid: Raw is monotonically ordered regardless of storage kind
      if constexpr (Grid<L> == Grid<R>)
        return cmp(lhs.raw(), rhs.raw());
      // an fp-backed operand: compare in double when both sides' values are
      // exact in double (raw_imax would truncate the fp raw); otherwise the
      // rational fallback below keeps the comparison exact.
      else if constexpr ((fp_raw<L> || fp_raw<R>) && exact_in_double<L> && exact_in_double<R>)
        return cmp(as_double(lhs), as_double(rhs));
      // both integer-direct (notch=1, Raw==value): compare as integers
      else if constexpr (value_raw<L> && value_raw<R>)
        return cmp(raw_imax(lhs), raw_imax(rhs));
      // same nonzero notch, integer-backed: compare signed value indices
      // (compile-time bias + raw) — e.g. two same-Q-format fixed-point types
      // with different intervals, without the rational decode.
      else if constexpr (Notch<L> == Notch<R> && index_cmp_fits<L> && index_cmp_fits<R>)
        return cmp(index_cmp_bias<L> + raw_imax(lhs), index_cmp_bias<R> + raw_imax(rhs));
      else
        return cmp(as_rational(lhs), as_rational(rhs));
    }
  }

  template <insidable L, insidable R>
  constexpr auto operator<=>(L const& lhs, R const& rhs) { return detail::compare(lhs, rhs, detail::three_way); }

  template <insidable L, insidable R>
  constexpr bool operator==(L const& lhs, R const& rhs) { return detail::compare(lhs, rhs, detail::equal_to); }

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
        constexpr umax notch_num = Notch<B>.Numerator;
        constexpr umax notch_den = static_cast<umax>(Notch<B>.Denominator); // Notch > 0
        constexpr umax index_mag = []{
          constexpr auto lo = *(Lower<B> / Notch<B>);
          constexpr auto hi = *(Upper<B> / Notch<B>);
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
      constexpr bool double_exact_values = Lower<B> >= rational{-(imax{1} << 53)} && Upper<B> <= rational{imax{1} << 53};
      if constexpr (value_raw<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
      else if constexpr (value_raw<B> && std::floating_point<A> && double_exact_values)
        return cmp(static_cast<double>(raw_imax(lhs)), static_cast<double>(rhs));
      else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(Notch<B>.Numerator),
                   static_cast<imax>(rhs) * Notch<B>.Denominator);
      else
        return cmp(as_rational(lhs), rational{rhs});
    }
  }

  template <insidable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs) { return detail::compare_scalar(lhs, rhs, detail::three_way); }

  template <insidable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs) { return detail::compare_scalar(lhs, rhs, detail::equal_to); }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = inside<grid{value}>{value};

  //---------------------------------------------------------------------------
  // zero / one — universal exact constants. Single-point bounds that assign into
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
  //   0xff_b        // inside<{255}>             hex integer
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
  constexpr auto operator""_ins() { return just<detail::_detail::parse_ins_literal<Chars...>()>; }

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
  template <policy_flag RoundMode, insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_with_rounding(N value)
  { return B{value, make_policy<clamp | RoundMode>()}; }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_floor(N value)
  { return clamp_with_rounding<round_floor, B>(value); }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_ceil(N value)
  { return clamp_with_rounding<round_ceil, B>(value); }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_round(N value)
  { return clamp_with_rounding<round_nearest, B>(value); }

  template <insidable B, arithmetic A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (will_conversion_overflow<B>(value))
      detail::raise(errc::domain_error, "checked_cast: value out of inside interval");
    if (will_conversion_trunc<B>(value))
      detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value};
  }

  // `unchecked_cast` routes through `inside<G, unsafe>` so the compiler elides
  // every domain/round check. UB if the value is actually out of range.
  template <insidable B, arithmetic A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    // Keep B's representation flags so the twin's raw layout is B's.
    constexpr policy_flag representation =
        InsidePolicy<B> & (exact | f64 | f32 | direct | indexed | raw_width_mask);
    using twin = inside<Grid<B>, unsafe | representation>;
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
    template <class Op, class L, class R, policy_like P = policy<>, class A = no_action>
    constexpr auto arith(Op op, L const& l, R const& r, P&& pol = {}, A&& act = {})
    { return op(l, r, std::forward<P>(pol), std::forward<A>(act)); }

    // Action-first form: at least one on_overflow (the only kind arithmetic
    // fires; other tags are accepted for forward-compat).
    template <class Op, class L, class R, class... Actions>
      requires (sizeof...(Actions) >= 1)
            && has_action<IsOverflowActionPred, std::remove_cvref_t<Actions>...>
    constexpr auto arith(Op op, L const& l, R const& r, Actions&&... acts)
    { return op(l, r, make_policy<merged_implied_flags<Actions...>>(),
                pick_action<IsOverflowActionPred>(acts...)); }

    template <class Op, class L, class R, class A = no_action>
    constexpr auto arith(Op op, L const& l, R const& r, errc& ec, A&& act = {})
    { return op(l, r, make_policy<checked>(ec), std::forward<A>(act)); }

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
  { return add(lhs, rhs); }

  [[nodiscard]] constexpr auto operator-(insidable auto lhs, insidable auto rhs)
  { return sub(lhs, rhs); }

  [[nodiscard]] constexpr auto operator*(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mul(lhs, rhs); }

  [[nodiscard]] constexpr auto operator/(insidable auto lhs, insidable auto rhs)
  { return beman::inside::div(lhs, rhs, make_policy<InsidePolicy<decltype(lhs)> | InsidePolicy<decltype(rhs)>>()); }

  [[nodiscard]] constexpr auto operator%(insidable auto lhs, insidable auto rhs)
  { return beman::inside::mod(lhs, rhs, make_policy<InsidePolicy<decltype(lhs)> | InsidePolicy<decltype(rhs)>>()); }

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

  // add_all_into<Target> / mul_all_into<Target> — fold, then collapse the widened
  // intermediate into Target via clamp_cast (widen for exactness, then clip).
  template <insidable Target, insidable First, insidable... Rest>
  [[nodiscard]] constexpr Target add_all_into(First const& first, Rest const&... rest)
  {
    auto sum = (first + ... + rest);
    if constexpr (requires { typename decltype(sum)::value_type; })
      return clamp_cast<Target>(sum.value());
    else
      return clamp_cast<Target>(sum);
  }

  template <insidable Target, insidable First, insidable... Rest>
  [[nodiscard]] constexpr Target mul_all_into(First const& first, Rest const&... rest)
  {
    auto prod = (first * ... * rest);
    if constexpr (requires { typename decltype(prod)::value_type; })
      return clamp_cast<Target>(prod.value());
    else
      return clamp_cast<Target>(prod);
  }

  //---------------------------------------------------------------------------
  // sum<Target> — bulk reduction with ONE deferred range check. Per-element
  // `target += b` re-validates every step (blocks vectorization); this
  // accumulates raws in imax and applies Target's policy once to the total
  // (semantic difference: the *total* is validated, not every prefix). Fast
  // path: ≤32-bit integer raws, flushed to a rational every 2^30 elements so the
  // accumulator can't overflow; wider/rational/real take the per-element fold.
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
            return ((rational{acc} * Notch<B>).value()
                    + (rational{cnt} * Lower<B>).value()).value();
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
      requires (!std::same_as<Lhs, Rhs>) && (hull(Grid<Lhs>, Grid<Rhs>).has_value())
    struct common_inside<Lhs, Rhs>
    {
      static constexpr grid hull_grid = *hull(Grid<Lhs>, Grid<Rhs>);
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
  constexpr auto operator op(L const& lhs, R const& rhs)                             \
  { return lift([](auto const& l, auto const& r) { return l op r; }, lhs, rhs); }

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
  // Comparisons and compound assignment with raw scalars are unaffected.
  //
  // Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
  // so `requires { b + 1; }` stays well-formed and the static_assert fires only
  // on a real call.
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
// returns by value; iterator_concept carries the real random-access capability.
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
      R base_;

      struct iterator
      {
        std::ranges::iterator_t<const R> it{};
        std::size_t index{0};

        using value_type      = std::pair<std::size_t, std::ranges::range_value_t<R>>;
        using difference_type  = std::ptrdiff_t;

        constexpr value_type operator*() const { return {index, *it}; }
        constexpr iterator& operator++() { ++it; ++index; return *this; }
        constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
        constexpr bool operator==(iterator const& o) const { return it == o.it; }
      };

      constexpr iterator begin() const { return {std::ranges::begin(base_), 0}; }
      constexpr iterator end()   const { return {std::ranges::end(base_), 0}; }
    };

    // stride_view — C++20 stand-in for std::views::stride (C++23). Visits every
    // `step`-th element; forward-only, and the advance checks `end` so a length
    // that isn't a multiple of the stride still terminates.
    template <class R>
    struct stride_view
    {
      R base_;
      std::size_t step_{1};

      struct iterator
      {
        std::ranges::iterator_t<const R> it{};
        std::ranges::iterator_t<const R> end{};
        std::size_t step{1};

        using value_type      = std::ranges::range_value_t<R>;
        using difference_type = std::ptrdiff_t;

        constexpr value_type operator*() const { return *it; }
        constexpr iterator& operator++()
        {
          for (std::size_t k = 0; k < step && it != end; ++k) ++it;
          return *this;
        }
        constexpr iterator operator++(int) { auto t = *this; ++*this; return t; }
        constexpr bool operator==(iterator const& o) const { return it == o.it; }
      };

      constexpr iterator begin() const
      { return {std::ranges::begin(base_), std::ranges::end(base_), step_}; }
      constexpr iterator end() const
      { return {std::ranges::end(base_), std::ranges::end(base_), step_}; }
    };
  } // namespace detail

  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0)
  struct inside_range
  {
    using value_type = inside<G, P>;
    static constexpr umax slot_count = detail::NotchCount<value_type> + 1;

    struct iterator
    {
      using iterator_concept  = std::random_access_iterator_tag;
      using iterator_category = std::input_iterator_tag;
      using value_type        = inside<G, P>;
      using difference_type   = imax;

      umax start {0};   // slot of the first element (the range wraps past the top)
      imax pos   {0};   // position in [0, slot_count]; the loop variable

      constexpr iterator() = default;
      constexpr iterator(umax s, imax p) : start{s}, pos{p} {}

      // Grid slot of this position: start + pos, wrapped once (no overflow).
      constexpr umax slot() const
      {
        const umax p = static_cast<umax>(pos);
        return p < slot_count - start ? start + p : p - (slot_count - start);
      }

      constexpr value_type operator*() const
      {
        // value = Lower + index * Notch (always exact: lies on the grid).
        // Integer-backed storages decode without the rational/assignment
        // engine: for index storage the iterator index IS the raw (it stays in
        // [0, NotchCount], which the raw type holds); integer-grid value
        // storage is a multiply-add in raw space. Rational/fp raws keep the exact generic path.
        if constexpr (detail::index_raw<value_type>)
          return value_type::from_raw(
              static_cast<typename value_type::raw_type>(slot()));
        else if constexpr (detail::value_raw<value_type>
                           && detail::abs_den(Notch<value_type>.Denominator) == 1
                           && detail::abs_den(Lower<value_type>.Denominator) == 1)
        {
          constexpr imax notch_step = static_cast<imax>(Notch<value_type>.Numerator);
          return value_type::from_raw(static_cast<typename value_type::raw_type>(
              detail::LowerImax<value_type>
              + static_cast<imax>(slot()) * notch_step));
        }
        else
        {
          detail::rational val = (G.Interval.Lower
                          + (detail::rational{slot()} * G.Notch).value()).value();
          return value_type{val};
        }
      }

      constexpr value_type operator[](difference_type n) const
      { return *(*this + n); }

      constexpr iterator& operator++() { ++pos; return *this; }
      constexpr iterator  operator++(int) { auto t = *this; ++pos; return t; }
      constexpr iterator& operator--() { --pos; return *this; }
      constexpr iterator  operator--(int) { auto t = *this; --pos; return t; }
      constexpr iterator& operator+=(difference_type n) { pos += n; return *this; }
      constexpr iterator& operator-=(difference_type n) { pos -= n; return *this; }

      constexpr iterator operator+(difference_type n) const { auto t = *this; t += n; return t; }
      constexpr iterator operator-(difference_type n) const { auto t = *this; t -= n; return t; }
      friend constexpr iterator operator+(difference_type n, iterator it) { return it + n; }

      constexpr difference_type operator-(iterator o) const { return pos - o.pos; }
      constexpr bool operator==(iterator o) const { return pos == o.pos; }
      constexpr auto operator<=>(iterator o) const { return pos <=> o.pos; }
    };

    umax start_index_;

    constexpr inside_range() : start_index_{0} {}

    constexpr inside_range(value_type start)
    {
      // Map a grid value back to its notch index: (start - Lower) / Notch.
      // Same storage split as iterator::operator* — index raw already is the
      // notch index; integer-grid value raw divides out the (integer) step.
      if constexpr (detail::index_raw<value_type>)
        start_index_ = static_cast<umax>(start.raw());
      else if constexpr (detail::value_raw<value_type>
                         && detail::abs_den(Notch<value_type>.Denominator) == 1
                         && detail::abs_den(Lower<value_type>.Denominator) == 1)
      {
        constexpr imax notch_step = static_cast<imax>(Notch<value_type>.Numerator);
        start_index_ = static_cast<umax>(
            (static_cast<imax>(start.raw()) - detail::LowerImax<value_type>)
            / notch_step);
      }
      else
      {
        // The result has integer denominator (start is on the grid) so the
        // numerator is the index directly.
        auto offset = ((detail::as_rational(start) - G.Interval.Lower)
                       / G.Notch).value();
        start_index_ = offset.Numerator;
      }
    }

    constexpr iterator begin() const { return {start_index_, 0}; }
    constexpr iterator end() const   { return {start_index_, static_cast<imax>(slot_count)}; }

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
// The default engine; `BEMAN_INSIDE_MATH_FIXED` selects the integer CORDIC engine instead.
//---------------------------------------------------------------------------


// BEMAN_INSIDE_MATH_NO_FP is resolved in policy_flag.hpp (included via inside.hpp).

#ifndef BEMAN_INSIDE_MATH_NO_FP   // ===== FP engine present (needs <cmath> + an FPU) =====

#include <cmath>            // std::fma, std::sqrt, std::nearbyint ONLY

// `BEMAN_INSIDE_DBL_FN`: the engine cores become `constexpr` on C++26 toolchains with
// constexpr <cmath> (P1383). Inert otherwise — see BEMAN_INSIDE_MATH_FN in cmath.hpp.
#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
#  define BEMAN_INSIDE_DBL_FN constexpr
#else
#  define BEMAN_INSIDE_DBL_FN
#endif

namespace beman::inside::math::dbl::detail
{
  using std::fma;

  // c0·z^n + c1·z^(n-1) + … + cn as an fma chain from the highest coefficient
  // down (Horner) — the same operation order as writing the chain out by hand.
  template <std::floating_point T, typename... C>
  [[gnu::always_inline]] inline BEMAN_INSIDE_DBL_FN T horner(T z, T c0, C... cs)
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
  inline BEMAN_INSIDE_DBL_FN double sin_poly(double r)
  {
    double z = r * r;
    double p = horner(z,
                      -1.0 / 1307674368000.0, 1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0,
                      -1.0 / 5040.0, 1.0 / 120.0, -1.0 / 6.0, 1.0);
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²), Q = Σ (−1)ᵏ zᵏ/(2k)! to z⁸ (r¹⁶).
  inline BEMAN_INSIDE_DBL_FN double cos_poly(double r)
  {
    double z = r * r;
    return horner(z,
                  1.0 / 20922789888000.0, -1.0 / 87178291200.0, 1.0 / 479001600.0, -1.0 / 3628800.0,
                  1.0 / 40320.0, -1.0 / 720.0, 1.0 / 24.0, -1.0 / 2.0,
                  1.0);
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r¹².
  inline BEMAN_INSIDE_DBL_FN double exp_poly(double r)
  {
    return horner(r,
                  1.0 / 479001600.0, 1.0 / 39916800.0, 1.0 / 3628800.0, 1.0 / 362880.0,
                  1.0 / 40320.0, 1.0 / 5040.0, 1.0 / 720.0, 1.0 / 120.0,
                  1.0 / 24.0, 1.0 / 6.0, 1.0 / 2.0, 1.0,
                  1.0);
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BEMAN_INSIDE_DBL_FN double reduce_quadrant(double x, long& q)
  {
    double k = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r = fma(-k, kHalfPiLo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BEMAN_INSIDE_DBL_FN double d_sin(double x)
  {
    long q; double r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  inline BEMAN_INSIDE_DBL_FN double d_cos(double x)
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
  inline BEMAN_INSIDE_DBL_FN bool d_tan(double x, double& t)
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
  inline BEMAN_INSIDE_DBL_FN double d_exp(double x)
  {
    double k = std::nearbyint(x * kLog2e);
    double r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return beman::inside::detail::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BEMAN_INSIDE_DBL_FN double d_sqrt(double x) { return std::sqrt(x); }   // correctly rounded

  inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1; // √½

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln(x) = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1) ∈ [−0.18,0.18] (atanh series converges fast). Pre: x > 0.
  inline BEMAN_INSIDE_DBL_FN double d_log(double x)
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
  inline BEMAN_INSIDE_DBL_FN double d_exp2(double x)  { return d_exp(x * kLn2Full); }
  inline BEMAN_INSIDE_DBL_FN double d_log2(double x)  { return d_log(x) * kLog2e; }
  inline BEMAN_INSIDE_DBL_FN double d_log10(double x) { return d_log(x) * kLog10e; }
  inline BEMAN_INSIDE_DBL_FN double d_pow(double b, double e) { return d_exp(e * d_log(b)); }
  inline BEMAN_INSIDE_DBL_FN double d_cbrt(double x)
  {
    if (x == 0.0) return 0.0;
    double m = d_exp(d_log(x < 0 ? -x : x) * (1.0 / 3.0));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_DBL_FN double d_sinh(double x) { double e = d_exp(x); return (e - 1.0 / e) * 0.5; }
  inline BEMAN_INSIDE_DBL_FN double d_cosh(double x) { double e = d_exp(x); return (e + 1.0 / e) * 0.5; }
  inline BEMAN_INSIDE_DBL_FN double d_tanh(double x)
  {
    double e = d_exp(x + x);            // e^{2x}
    return (e - 1.0) / (e + 1.0);
  }
  // √(x²+y²). The public domain caps |x|,|y| ≤ 2^20, so x²+y² ≤ 2^41 — no
  // overflow, no scaling needed; the correctly-rounded √ keeps it accurate.
  inline BEMAN_INSIDE_DBL_FN double d_hypot(double x, double y) { return d_sqrt(x * x + y * y); }

  inline constexpr double kPi      = 0x1.921fb54442d18p+1;   // π
  inline constexpr double kPiHalf  = 0x1.921fb54442d18p+0;   // π/2
  inline constexpr double kPiSixth = 0x1.0c152382d7366p-1;   // π/6
  inline constexpr double kInvSqrt3 = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
  inline constexpr double kTanPi12 = 0x1.126145e9ecd56p-2;   // tan(π/12) ≈ 0.2679

  // atan(x). Reduce |x|>1 via reciprocal (π/2 − atan(1/x)); then |a|>tan(π/12)
  // via the π/6 addition formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BEMAN_INSIDE_DBL_FN double d_atan(double x)
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

  inline BEMAN_INSIDE_DBL_FN double d_atan2(double y, double x)
  {
    if (x > 0.0) return d_atan(y / x);
    if (x < 0.0) return d_atan(y / x) + (y >= 0.0 ? kPi : -kPi);
    if (y > 0.0) return kPiHalf;
    if (y < 0.0) return -kPiHalf;
    return 0.0;
  }

  inline BEMAN_INSIDE_DBL_FN double d_asin(double x) { return d_atan(x / d_sqrt((1.0 - x) * (1.0 + x))); }
  inline BEMAN_INSIDE_DBL_FN double d_acos(double x) { return kPiHalf - d_asin(x); }
} // namespace beman::inside::math::dbl::detail

namespace beman::inside::math::dbl
{
  // Engine cores: `real` (double-backed) inside in → `double` math → inside out.
  // The inside I/O is a plain double read/store (operator double / Out{double}),
  // so the cost is the polynomial itself. These plug into the shared public
  // surface as `fn_core` under the default build.
  template <typename Out>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out store(double d)
  {
    // An fp-backed Out (f64 or f32) stores the value directly via its raw (an f32
    // Out narrows double→float, lossless on its float-exact grid); a non-fp snap
    // grid assigns through the rational path, snapping via Out's round policy.
    if constexpr (beman::inside::detail::fp_raw<Out>) return Out{d};
    else { Out o{}; o = beman::inside::detail::rational{d}; return o; }
  }

  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out sin_core(In x)  { return store<Out>(detail::d_sin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out cos_core(In x)  { return store<Out>(detail::d_cos(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out exp_core(In x)  { return store<Out>(detail::d_exp(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out sqrt_core(In x) { return store<Out>(detail::d_sqrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out log_core(In x)  { return store<Out>(detail::d_log(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out exp2_core(In x) { return store<Out>(detail::d_exp2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out log2_core(In x) { return store<Out>(detail::d_log2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out log10_core(In x){ return store<Out>(detail::d_log10(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out cbrt_core(In x) { return store<Out>(detail::d_cbrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out sinh_core(In x) { return store<Out>(detail::d_sinh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out cosh_core(In x) { return store<Out>(detail::d_cosh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out tanh_core(In x) { return store<Out>(detail::d_tanh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out atan_core(In x) { return store<Out>(detail::d_atan(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out asin_core(In x) { return store<Out>(detail::d_asin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out acos_core(In x) { return store<Out>(detail::d_acos(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out atan2_core(In y, In x)
  { return store<Out>(detail::d_atan2(static_cast<double>(y), static_cast<double>(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::d_hypot(static_cast<double>(x), static_cast<double>(y))); }
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
  inline BEMAN_INSIDE_DBL_FN float sin_poly(float r)
  {
    float z = r * r;
    float p = horner(z,
                     -1.0f / 39916800.0f, 1.0f / 362880.0f, -1.0f / 5040.0f, 1.0f / 120.0f,
                     -1.0f / 6.0f, 1.0f);
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²) to r¹⁰.
  inline BEMAN_INSIDE_DBL_FN float cos_poly(float r)
  {
    float z = r * r;
    return horner(z,
                  -1.0f / 3628800.0f, 1.0f / 40320.0f, -1.0f / 720.0f, 1.0f / 24.0f,
                  -1.0f / 2.0f, 1.0f);
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r⁷.
  inline BEMAN_INSIDE_DBL_FN float exp_poly(float r)
  {
    return horner(r,
                  1.0f / 5040.0f, 1.0f / 720.0f, 1.0f / 120.0f, 1.0f / 24.0f,
                  1.0f / 6.0f, 1.0f / 2.0f, 1.0f, 1.0f);
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BEMAN_INSIDE_DBL_FN float reduce_quadrant(float x, long& q)
  {
    float k = std::nearbyint(x * kTwoOverPi);
    float r = fma(-k, kPio2Hi, x);
    r = fma(-k, kPio2Lo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BEMAN_INSIDE_DBL_FN float d_sin(float x)
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
  inline BEMAN_INSIDE_DBL_FN float d_cos(float x)
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
  inline BEMAN_INSIDE_DBL_FN bool d_tan(float x, float& t)
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
  inline BEMAN_INSIDE_DBL_FN float d_exp(float x)
  {
    float k = std::nearbyint(x * kLog2e);
    float r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return std::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BEMAN_INSIDE_DBL_FN float d_sqrt(float x) { return std::sqrt(x); }   // correctly rounded

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1). Pre: x > 0.
  inline BEMAN_INSIDE_DBL_FN float d_log(float x)
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
  inline BEMAN_INSIDE_DBL_FN float d_exp2(float x)  { return d_exp(x * kLn2Full); }
  inline BEMAN_INSIDE_DBL_FN float d_log2(float x)  { return d_log(x) * kLog2e; }
  inline BEMAN_INSIDE_DBL_FN float d_log10(float x) { return d_log(x) * kLog10e; }
  inline BEMAN_INSIDE_DBL_FN float d_pow(float b, float e) { return d_exp(e * d_log(b)); }
  inline BEMAN_INSIDE_DBL_FN float d_cbrt(float x)
  {
    if (x == 0.0f) return 0.0f;
    float m = d_exp(d_log(x < 0 ? -x : x) * (1.0f / 3.0f));
    return x < 0 ? -m : m;
  }
  inline BEMAN_INSIDE_DBL_FN float d_sinh(float x) { float e = d_exp(x); return (e - 1.0f / e) * 0.5f; }
  inline BEMAN_INSIDE_DBL_FN float d_cosh(float x) { float e = d_exp(x); return (e + 1.0f / e) * 0.5f; }
  inline BEMAN_INSIDE_DBL_FN float d_tanh(float x)
  {
    float e = d_exp(x + x);             // e^{2x}
    return (e - 1.0f) / (e + 1.0f);
  }
  inline BEMAN_INSIDE_DBL_FN float d_hypot(float x, float y) { return d_sqrt(x * x + y * y); }

  // atan(x): reduce |x|>1 via reciprocal; |a|>tan(π/12) via the π/6 addition
  // formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BEMAN_INSIDE_DBL_FN float d_atan(float x)
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

  inline BEMAN_INSIDE_DBL_FN float d_atan2(float y, float x)
  {
    if (x > 0.0f) return d_atan(y / x);
    if (x < 0.0f) return d_atan(y / x) + (y >= 0.0f ? kPi : -kPi);
    if (y > 0.0f) return kPiHalf;
    if (y < 0.0f) return -kPiHalf;
    return 0.0f;
  }

  inline BEMAN_INSIDE_DBL_FN float d_asin(float x) { return d_atan(x / d_sqrt((1.0f - x) * (1.0f + x))); }
  inline BEMAN_INSIDE_DBL_FN float d_acos(float x) { return kPiHalf - d_asin(x); }
} // namespace beman::inside::math::flt::detail

namespace beman::inside::math::flt
{
  // Engine cores: inside in → `float` math → inside out. Storing the float result:
  // an fp-backed Out (f32 OR f64) stores the value directly via its float/double
  // raw (the natural pairing for `flt` is `f32` — no rational, no double round-
  // trip on the result); any other snap grid assigns through the rational path,
  // snapping via Out's round policy.
  template <typename Out>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out store(float f)
  {
    if constexpr (beman::inside::detail::fp_raw<Out>) return Out{static_cast<double>(f)};
    else { Out o{}; o = beman::inside::detail::rational{static_cast<double>(f)}; return o; }
  }

  // Read an input inside as `float`. An f32-backed operand IS a binary32 raw, so
  // read it directly — no double hop (keeps the whole flt+f32 path in hardware
  // float on a single-precision FPU). Any other storage decodes via double then
  // narrows; float→double→float round-trips to the same float, so this is a pure
  // optimization with no value change.
  template <typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN float to_float(In x)
  {
    if constexpr (beman::inside::detail::f32_raw<In>) return x.raw();
    else                                    return static_cast<float>(static_cast<double>(x));
  }

  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out sin_core(In x)  { return store<Out>(detail::d_sin(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out cos_core(In x)  { return store<Out>(detail::d_cos(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out exp_core(In x)  { return store<Out>(detail::d_exp(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out sqrt_core(In x) { return store<Out>(detail::d_sqrt(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out log_core(In x)  { return store<Out>(detail::d_log(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out exp2_core(In x) { return store<Out>(detail::d_exp2(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out log2_core(In x) { return store<Out>(detail::d_log2(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out log10_core(In x){ return store<Out>(detail::d_log10(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out cbrt_core(In x) { return store<Out>(detail::d_cbrt(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out sinh_core(In x) { return store<Out>(detail::d_sinh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out cosh_core(In x) { return store<Out>(detail::d_cosh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out tanh_core(In x) { return store<Out>(detail::d_tanh(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out atan_core(In x) { return store<Out>(detail::d_atan(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out asin_core(In x) { return store<Out>(detail::d_asin(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out acos_core(In x) { return store<Out>(detail::d_acos(to_float(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out atan2_core(In y, In x)
  { return store<Out>(detail::d_atan2(to_float(y), to_float(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BEMAN_INSIDE_DBL_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::d_hypot(to_float(x), to_float(y))); }
} // namespace beman::inside::math::flt

#endif // !BEMAN_INSIDE_MATH_NO_FP




// The public beman::inside::math::* functions dispatch to the double engine (default) or
// the integer/CORDIC engine (`-DBEMAN_INSIDE_MATH_FIXED`). The integer engine is
// always `constexpr`; the double engine becomes `constexpr` automatically on
// C++26 toolchains where <cmath> is constexpr (P1383 — std::fma / std::sqrt /
// std::nearbyint; feature macro __cpp_lib_constexpr_cmath). That branch is
// inert (and untested) until such a toolchain exists. Decision 2026-06-12:
// no compile-time softfloat emulation — wait for the standard.
// BEMAN_INSIDE_MATH_NO_FP (resolved in cmath_double.hpp, included above) selects the
// integer/CORDIC engine and is implied by BEMAN_INSIDE_MATH_FIXED — so the integer engine
// is constexpr here. The double engine becomes constexpr only on a C++26 toolchain
// with constexpr <cmath> (P1383); that branch is inert until such a toolchain.
#if defined(BEMAN_INSIDE_MATH_NO_FP) \
    || (defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L)
#  define BEMAN_INSIDE_MATH_FN constexpr
#else
#  define BEMAN_INSIDE_MATH_FN
#endif

//---------------------------------------------------------------------------
// beman::inside::math — one transcendental API, two interchangeable engines selected by
// the `BEMAN_INSIDE_MATH_FIXED` macro. Both are feature-equivalent (same functions,
// signatures, domains):
//
//   * DEFAULT — double engine (`cmath_double.hpp`): hardware `double`
//     polynomials on `real` bounds. Bit-identical on any IEEE-754 binary64
//     platform built without `-ffast-math`. Fast (~ns); needs an FPU; runtime.
//   * `BEMAN_INSIDE_MATH_FIXED` — integer/CORDIC engine (this file): FPU-free, constexpr,
//     UNCONDITIONALLY bit-identical (any platform/flags). For embedded/portability.
//   * `BEMAN_INSIDE_MATH_FLOAT` — float (binary32) engine (`cmath_float.hpp`): like the
//     double engine but single precision, for single-precision-only FPUs.
//
// The macro picks only which engine the UNQUALIFIED `beman::inside::math::fn` uses; all
// engines are always reachable by namespace (`cordic::`/`dbl::`/`flt::`). Default
// selection (the dispatch below): `BEMAN_INSIDE_MATH_NO_FP`→cordic, else
// `BEMAN_INSIDE_MATH_FLOAT`→flt, else dbl.
//
// `BEMAN_INSIDE_MATH_NO_FP` (implied by `BEMAN_INSIDE_MATH_FIXED`, auto-enabled when
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
    inline constexpr rational pi_r{1068966896, 340262731};
    inline constexpr rational two_pi_r = 2 * pi_r;

    // Policy of an auto-deduced output: the input's, minus any fixed-width
    // storage flag (i8 … u64) — the output range differs, as for arithmetic.
    template <insidable In>
    inline constexpr policy_flag out_policy = InsidePolicy<In> & ~raw_width_mask;

    // Out's interval endpoints as F (via double, like the runtime value), folded
    // at compile time for the FP engines' range checks.
    template <typename F, insidable Out>
    inline constexpr F lower_fp = static_cast<F>(static_cast<double>(Lower<Out>));
    template <typename F, insidable Out>
    inline constexpr F upper_fp = static_cast<F>(static_cast<double>(Upper<Out>));

    // Every transcendental operand must carry the `real` policy flag: under the
    // default engine it selects double-backed dyadic storage, under BEMAN_INSIDE_MATH_FIXED
    // integer round_nearest. Requiring it keeps both engines' call sites identical
    // and avoids the slow integer-I/O path. Pure grid ops (abs/floor/ceil/round/
    // trunc/fmod) have no engine and don't require it.
    template <insidable In>
    consteval bool require_snap() noexcept
    {
      static_assert(has_flag(InsidePolicy<In>, snap),
          "beman::inside::math: a transcendental result is rounded onto the grid — its "
          "operand must permit rounding. Declare it with `round_nearest` (or "
          "`snap` / a `round_*` mode / `real`).");
      return true;
    }
  }

  // Public irrational constants as POINT-BOUNDS, so they compose directly in
  // inside-space (`angle * math::pi`) with no rational on the surface.
  inline constexpr auto pi     = just<detail::pi_r>;
  inline constexpr auto two_pi = just<detail::two_pi_r>;

  namespace detail
  {
    using namespace beman::inside::detail;

    // Internal turn-phase shape: Q.N turns, period implicit in the unsigned raw's
    // modular wrap. Public sin/cos/tan take radians and route through this shape;
    // callers don't construct it directly (see examples/oscillator.cpp).
    template <int N>
    using turns_t = inside<{0, rational{(imax{1} << N) - 1, imax{1} << N},
                           notch<1, (imax{1} << N)>}>;


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
      static_assert(Lower<In> == 0,
                    "beman::inside::math: turn-phase input must have Lower == 0");
      static_assert(Notch<In>.Numerator == 1,
                    "beman::inside::math: turn-phase input must have notch 1/2^N");
      return log2_pow2(abs_den(Notch<In>.Denominator));
    }();

    // Forward declarations of the CORDIC engine pieces the turn-input workers
    // rely on (the engine is defined below, after the radians sin/cos).
    template <insidable Out> constexpr int working_bits() noexcept;
    template <int W, int N> constexpr rational sin_from_turn_fixed(imax turn_w) noexcept;
    template <int W, int N> constexpr rational cos_from_turn_fixed(imax turn_w) noexcept;
    template <insidable Out> constexpr Out store_grid(rational r) noexcept;

    // sin (turn-input, internal). Q.N turn-phase → amplitude on `Out`'s grid via
    // the CORDIC engine: rescale the phase to the working scale 2^W and run the
    // shared `sin_from_turn_fixed` reducer.
    template <insidable Out, insidable In>
    [[nodiscard]] constexpr Out sin_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "beman::inside::math: turn-phase N must be in [2, 30]");
      static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
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
    [[nodiscard]] constexpr Out cos_turn_impl(In phase) noexcept
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
    // The two paths below are bit-for-bit equal by construction.
    constexpr imax fmul(imax a, imax b, int W) noexcept
    {
      bool neg = (a < 0) ^ (b < 0);
      umax ua = (a < 0) ? umax{0} - static_cast<umax>(a) : static_cast<umax>(a);
      umax ub = (b < 0) ? umax{0} - static_cast<umax>(b) : static_cast<umax>(b);
#if defined(__SIZEOF_INT128__)
      // gcc/clang: native 128-bit, constexpr-friendly.
      umax r = static_cast<umax>((static_cast<unsigned __int128>(ua) * ub) >> W);
#else
      // portable (no __int128): 32-bit split → 128-bit (hi:lo) → shift.
      umax al = ua & 0xffffffffu, ah = ua >> 32;
      umax bl = ub & 0xffffffffu, bh = ub >> 32;
      umax ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
      umax mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
      umax lo  = (ll & 0xffffffffu) | (mid << 32);
      umax hi  = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
      umax r   = (W == 0) ? lo
               : (W < 64) ? ((lo >> W) | (hi << (64 - W)))
               :            (hi >> (W - 64));
#endif
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
#if defined(__SIZEOF_INT128__)
      using u128 = unsigned __int128;
      const u128 t = (u128{n} << W) + d / 2;
      const umax q = static_cast<umax>(t / d);
#else
      // portable: 128-bit (hi:lo) dividend, restoring shift-subtract divide.
      // d < 2^63 (imax denominator), so the partial remainder fits umax.
      umax hi = (W == 0) ? 0 : (n >> (64 - W));
      umax lo = n << W;
      const umax half = d / 2;
      lo += half;
      hi += (lo < half);
      umax q = 0, r = 0;
      for (int i = 127; i >= 0; --i)
      {
        r = (r << 1) | ((i >= 64 ? (hi >> (i - 64)) : (lo >> i)) & 1u);
        q <<= 1;
        if (r >= d) { r -= d; q |= 1; }
      }
#endif
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
        Notch<Out>.Numerator == 1
        && !rational_raw<Out>
        // `real` storage holds the VALUE, not an offset index, so route it
        // through the rational fallback `Out{r}` (same guard as fmod_int_fast).
        && !fp_raw<Out>
        && rounding_of(InsidePolicy<Out>) == round_mode::nearest
        && (std::signed_integral<raw_t<Out>>
            || NotchCount<Out>
                 <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // Store a power-of-two-denominator result (the shape every core returns)
    // onto Out's grid. Fast path: pure integer. Fallback: the general rational
    // assignment (handles non-fast grids, clamp/wrap on out-of-range, etc).
    template <insidable Out>
    constexpr Out store_grid(rational r) noexcept
    {
      if constexpr (grid_fast_store<Out>)
      {
        umax den = abs_den(r.Denominator);
        if ((den & (den - 1)) == 0)                       // power-of-two denom
        {
          int  D   = std::countr_zero(den);
          imax num = signed_numerator(r);
          constexpr imax K = abs_den(Notch<Out>.Denominator);  // 1/notch
          constexpr imax m = trunc((Lower<Out> * rational{K}).value()); // Lower·K (exact int)
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
            const imax off  = idx - m;
            if (off >= 0 && static_cast<umax>(off) <= NotchCount<Out>)
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
      constexpr int GUARD = 6;
      umax den        = abs_den(Notch<Out>.Denominator);   // 1/notch
      int  notch_bits = (den <= 1) ? 0 : std::bit_width(den - 1);
      imax hi  = ceil(abs(Upper<Out>));
      imax lo  = ceil(abs(Lower<Out>));
      imax mag = (hi > lo) ? hi : lo;
      int  int_bits = (mag <= 1) ? 0 : std::bit_width(static_cast<umax>(mag));
      int  W = notch_bits + int_bits + GUARD;
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

    // atan(2^-i) in RADIANS at scale 2^W. i=0 is π/4 (exact, from pi_r); i≥1
    // uses the fast-converging series atan(z)=z−z³/3+z⁵/5−… for tiny z=2^-i.
    constexpr imax atan_pow2_fixed(int i, int W) noexcept
    {
      if (i == 0)
        return to_fixed(pi_r / 4, W);
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

    // √2 as a rational (literal source, like pi_r / ln2_r), for sqrt's odd-
    // exponent step.
    inline constexpr rational sqrt2_r{1414213562, 1000000000};

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
      if (e & 1) { constexpr imax sqrt2_w = to_fixed(sqrt2_r, W); sm = fmul(sm, sqrt2_w, W); }
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
      constexpr imax two_pi_w = to_fixed(two_pi_r, W);
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
        constexpr imax two_pi_w = to_fixed(two_pi_r, W);
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
    inline constexpr rational inv_two_pi =
      (rational{1} / two_pi_r).value();

    // radians → turn at scale 2^W. The single-term product's error (~2^-(W+1))
    // scales with |a|, capping the envelope at ±1024 rad — grids within it keep
    // that expression verbatim (bit-identical). Wider grids (up to ±2^20 rad) use
    // a two-term hi+lo split of 1/2π (lo carried at scale 2^(W+24)) to recover
    // ~2^-W turn accuracy, combined through the 128-bit fmul.
    template <int W, insidable In>
    constexpr imax rad_to_turn_w(rational a) noexcept
    {
      const imax a_w = to_fixed(a, W);
      if constexpr (Lower<In> >= -1024 && Upper<In> <= 1024)
      {
        constexpr imax inv_two_pi_w = to_fixed(inv_two_pi, W);
        return fmul(a_w, inv_two_pi_w, W);
      }
      else
      {
        constexpr int  S    = W + 24;                 // ≤ 55 for W ≤ 31
        constexpr imax hi_w = to_fixed(inv_two_pi, W);
        constexpr rational lo = inv_two_pi - fixed_to_rational(hi_w, W);
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
    inline constexpr rational ln2_r{6931471806, 10000000000};
    inline constexpr rational inv_ln2_r = (rational{1} / ln2_r).value();

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
      constexpr imax ln2_w = to_fixed(ln2_r, W);            // compile-time constant
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
    constexpr imax ln_to_fixed(rational w) noexcept
    {
      imax w_w  = to_fixed(w, W);
      int  lead = 63 - std::countl_zero(static_cast<umax>(w_w));
      int  e    = lead - W;
      imax one  = imax{1} << W;
      imax m_w  = (e >= 0) ? (w_w >> e) : (w_w << (-e));   // m·2^W ∈ [2^W, 2^(W+1))
      imax z    = cordic_atanh_vec<W, hyp_len(W)>(m_w + one, m_w - one);
      constexpr imax ln2_w = to_fixed(ln2_r, W);           // compile-time constant
      return e * ln2_w + 2 * z;
    }

    // log2(x) at scale 2^W as imax: ln(x)·log2(e).
    template <int W>
    constexpr imax log2_to_fixed(rational x) noexcept
    {
      constexpr imax inv_ln2_w = to_fixed(inv_ln2_r, W);   // compile-time constant
      return fmul(ln_to_fixed<W>(x), inv_ln2_w, W);
    }

    // e^(v_w / 2^W) as a rational: 2^(v·log2 e).
    template <int W>
    constexpr rational exp_from_fixed(imax v_w) noexcept
    {
      constexpr imax inv_ln2_w = to_fixed(inv_ln2_r, W);   // compile-time constant
      return exp2_from_fixed<W>(fmul(v_w, inv_ln2_w, W));
    }

    // Rational-input wrappers (inputs are small-denominator values; fine to
    // marshal through to_fixed). pow/cbrt compose via the *_fixed primitives
    // above instead, to avoid rational-denominator blow-up.
    template <int W>
    constexpr rational exp_fixed(rational v) noexcept
    { return exp_from_fixed<W>(to_fixed(v, W)); }
    template <int W>
    constexpr rational ln_fixed(rational w) noexcept
    { return fixed_to_rational(ln_to_fixed<W>(w), W); }
    template <int W>
    constexpr rational exp2_fixed(rational x) noexcept
    { return exp2_from_fixed<W>(to_fixed(x, W)); }
    template <int W>
    constexpr rational log2_fixed(rational x) noexcept
    { return fixed_to_rational(log2_to_fixed<W>(x), W); }

    // 1/ln10 at scale 2^W — for log10 = ln·(1/ln10), composed in fixed-point.
    template <int W>
    constexpr imax inv_ln10_fixed() noexcept
    {
      // ln10 ≈ 2.302585 (small, no overflow): derive the rational once, marshal.
      return to_fixed((rational{1} /
                       fixed_to_rational(ln_to_fixed<W>(rational{10}), W)).value(), W);
    }
  } // namespace detail

  // sin: radians-valued inside → amplitude on the auto-deduced output grid.
  // Converts to a turn (× 1/(2π)) at the grid-derived working scale, then runs
  // the circular-CORDIC reducer. Inputs up to |angle| ≤ 2^20 rad (see
  // rad_to_turn_w for the reduction split beyond ±1024).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sin_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::sin: input magnitudes must be \u2264 2^20 rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "beman::inside::math::sin: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::sin_from_turn_fixed<W, W>(turn_w));
  }

  // cos: radians-valued inside → amplitude. cos(x) = sin(x + π/2) — add a
  // quarter-turn before the quadrant reducer, same precision as sin.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cos_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::cos: input magnitudes must be \u2264 2^20 rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "beman::inside::math::cos: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::cos_from_turn_fixed<W, W>(turn_w));
  }

  namespace detail
  {
    using namespace beman::inside::detail;

    // Shared tail of tan_impl / tan_turn_impl: pole → division_by_zero; outside
    // Out → overflow (a clamp Out saturates in the store instead).
    template <insidable Out, int W>
    constexpr std::expected<Out, errc> tan_store(imax turn_w) noexcept
    {
      imax t_w;
      if (!tan_from_turn_fixed<W, W>(turn_w, t_w))
        return std::unexpected(errc::division_by_zero);
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))
      {
        // t_w/2^W ∈ [lo, hi]  ⇔  ⌈lo·2^W⌉ ≤ t_w ≤ ⌊hi·2^W⌋
        constexpr imax lo_w = ceil ((Lower<Out> * rational{imax{1} << W}).value());
        constexpr imax hi_w = floor((Upper<Out> * rational{imax{1} << W}).value());
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
    [[nodiscard]] constexpr std::expected<Out, errc> tan_turn_impl(In phase) noexcept
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
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> tan_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::tan: input magnitudes must be \u2264 2^20 rad");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);

    return detail::tan_store<Out, W>(turn_w);
  }


  // log2: positive inside → inside. log2(x) = ln(x)·log2(e) via the grid-scaled
  // hyperbolic-CORDIC `log2_fixed` core (leading-bit reduction + atanh vectoring).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log2_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "beman::inside::math::log2: input must be strictly positive");

    return detail::store_grid<Out>(detail::log2_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // exp2: inside → inside, returning 2^x. 2^x = e^(x·ln2) via the grid-scaled
  // hyperbolic-CORDIC `exp2_fixed` core (integer/fractional split + sinh/cosh).
  //
  // Restrict |x| ≤ 30 so the rational denominator 2^(30 - k) fits in int63.
  // The output `Out` must include non-negative values and cover at least
  // [2^Lower<In>, 2^Upper<In>] — anything narrower needs `clamp` to absorb
  // overflow at the assignment.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out exp2_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -30 && Upper<In> <= 30,
                  "beman::inside::math::exp2: input must be in [-30, 30]");
    static_assert(Lower<Out> >= 0,
                  "beman::inside::math::exp2: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp2_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // exp: thin wrapper. exp(x) = exp2(x · log2(e)). The scaling factor
  // log2(e) ≈ 1.4427, so x must stay inside [-30/log2(e), 30/log2(e)] ≈
  // [-20.79, 20.79] for exp2's denominator-shift envelope. We use [-20, 20].
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out exp_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -20 && Upper<In> <= 20,
                  "beman::inside::math::exp: input must be in [-20, 20]");
    static_assert(Lower<Out> >= 0,
                  "beman::inside::math::exp: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // log: thin wrapper. log(x) = log2(x) · ln(2). Result precision matches
  // log2 minus 1-2 ULP from the final fixed-point scaling.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "beman::inside::math::log: input must be strictly positive");

    return detail::store_grid<Out>(detail::ln_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  // pow_base<Base>(x) = Base^x for compile-time-known integer Base ≥ 2.
  // Implemented as exp2(x · log2(Base)) with log2(Base) from the grid-scaled
  // `log2_to_fixed` core — no hand-typed magic constants.
  // For Base = 10, this is the building block for `db_to_linear`.
  template <imax Base, insidable Out, insidable In>
  [[nodiscard]] constexpr Out pow_base_impl(In x) noexcept
  {
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be ≥ 2");
    static_assert(Lower<Out> >= 0,
                  "beman::inside::math::pow_base: Out must be non-negative");

    constexpr int W = detail::working_bits<Out>();
    constexpr imax lb_w = detail::log2_to_fixed<W>(rational{Base});   // log2(Base)·2^W
    imax sc_w = detail::fmul(detail::to_fixed(rational{x}, W), lb_w, W);
    return detail::store_grid<Out>(detail::exp2_from_fixed<W>(sc_w));
  }


  // atan2: signed inside, signed inside → radians ∈ [-π, π], via CORDIC vectoring
  // with quadrant pre-rotation. CORDIC depends only on y/x, so inputs beyond
  // magnitude 1 are normalized by the larger magnitude (exact rational division);
  // inputs already in [-1, 1] skip it.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out atan2_impl(In y, In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::atan2: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= -detail::pi_r && Upper<Out> >= detail::pi_r,
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
    constexpr imax half_pi_w = detail::to_fixed(detail::pi_r / 2, W);
    imax pre_rotation = 0;
    if (x_w < 0) {
      if (y_w >= 0) { imax nx = y_w;  imax ny = -x_w; x_w = nx; y_w = ny; pre_rotation =  half_pi_w; }
      else          { imax nx = -y_w; imax ny =  x_w; x_w = nx; y_w = ny; pre_rotation = -half_pi_w; }
    }

    imax rad = detail::cordic_atan2_rad<W, W>(y_w, x_w) + pre_rotation;   // radians, scale W
    return detail::store_grid<Out>(detail::fixed_to_rational(rad, W));
  }

  namespace detail
  {
    // max(|Lower<In>|, |Upper<In>|) as a constexpr rational. Used to size
    // the auto-deduced abs output.
    template <insidable In>
    inline constexpr rational abs_auto_upper =
      (abs(Lower<In>) > abs(Upper<In>))
        ? abs(Lower<In>) : abs(Upper<In>);

    template <insidable In>
    using abs_auto_t = inside<{{rational{0}, abs_auto_upper<In>},
                              Notch<In>}, out_policy<In>>;

    template <insidable In>
    using floor_auto_t = inside<{{rational{floor(Lower<In>)},
                                  rational{floor(Upper<In>)}},
                                 notch<1>}, out_policy<In>>;

    template <insidable In>
    using ceil_auto_t = inside<{{rational{ceil(Lower<In>)},
                                 rational{ceil(Upper<In>)}},
                                notch<1>}, out_policy<In>>;

    template <insidable In>
    using round_auto_t = inside<{{rational{round(Lower<In>)},
                                  rational{round(Upper<In>)}},
                                 notch<1>}, out_policy<In>>;

    template <insidable In>
    using trunc_auto_t = inside<{{rational{trunc(Lower<In>)},
                                  rational{trunc(Upper<In>)}},
                                 notch<1>}, out_policy<In>>;

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
  [[nodiscard]] constexpr Out abs_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "beman::inside::math::abs: Out must include 0");
    if constexpr (detail::fp_direct<Out, detail::abs_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, [](double v) { return v < 0 ? -v : v; });
    else
      return detail::store_grid<Out>(beman::inside::detail::abs(rational{x}));
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out floor_impl(In x) noexcept
  {
    if constexpr (detail::fp_direct<Out, detail::floor_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_floor);
    else
      return detail::store_grid<Out>(floor(rational{x}));
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out ceil_impl(In x) noexcept
  {
    if constexpr (detail::fp_direct<Out, detail::ceil_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_ceil);
    else
      return detail::store_grid<Out>(ceil(rational{x}));
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out round_impl(In x) noexcept
  {
    if constexpr (detail::fp_direct<Out, detail::round_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_round);
    else
      return detail::store_grid<Out>(round(rational{x}));
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out trunc_impl(In x) noexcept
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
    // remainder in units of g = gcd(Notch<InX>, Notch<InY>): with x = a·g and
    // y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ %
    // is truncated division, the same convention). Conditions:
    //   * integer raws only (rational/double raws keep the rational path),
    //   * non-zero notches, g on Out's grid (g / Notch<Out> integer),
    //   * divisor grid excludes zero (no runtime zero check needed),
    //   * Out's interval covers ±max|y| (result magnitude is < |y|),
    //   * all unit counts fit comfortably in imax (headroom 4).
    template <insidable Out, insidable InX, insidable InY>
    inline constexpr bool fmod_int_fast = []{
      if (rational_raw<InX> || fp_raw<InX>
       || rational_raw<InY> || fp_raw<InY>
       || rational_raw<Out> || fp_raw<Out>)
        return false;
      if (Notch<InX> == 0 || Notch<InY> == 0 || Notch<Out> == 0)
        return false;
      if (!DivisorExcludesZero<InY>)
        return false;
      auto go = gcd(Notch<InX>, Notch<InY>);
      if (!go.has_value()) return false;
      rational g = *go;
      auto qo = g / Notch<Out>;
      if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
      rational maxx =
          abs(Lower<InX>) > abs(Upper<InX>)
            ? abs(Lower<InX>) : abs(Upper<InX>);
      rational maxy =
          abs(Lower<InY>) > abs(Upper<InY>)
            ? abs(Lower<InY>) : abs(Upper<InY>);
      if (Lower<Out> > -maxy || Upper<Out> < maxy)
        return false;
      constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
      auto ux = maxx / g;  auto uy = maxy / g;  auto uo = maxy / Notch<Out>;
      return ux.has_value() && uy.has_value() && uo.has_value()
          && ux->Numerator <= lim && uy->Numerator <= lim && uo->Numerator <= lim;
    }();
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y != 0 (fmod_impl checks it).
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y) noexcept
  {
    if constexpr (detail::fmod_int_fast<Out, InX, InY>)
    {
      // One integer remainder in g-units; bit-identical to the rational path.
      constexpr rational g = *beman::inside::detail::gcd(Notch<InX>, Notch<InY>);
      constexpr imax wx  = trunc((Notch<InX> / g).value());
      constexpr imax wy  = trunc((Notch<InY> / g).value());
      constexpr imax wo  = trunc((g / Notch<Out>).value());
      constexpr imax lox = trunc((Lower<InX> / g).value());   // exact: grid invariant
      constexpr imax loy = trunc((Lower<InY> / g).value());
      constexpr imax loo = trunc((Lower<Out> / Notch<Out>).value());
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
  [[nodiscard]] constexpr auto fmod_impl(InX x, InY y)
  {
    if constexpr (beman::inside::detail::DivisorExcludesZero<InY>)
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
  // Each function gets a second overload that derives `Out` from `In` and
  // delegates to the explicit form. `f<Out>(x)` picks the explicit form, `f(x)`
  // the auto form (explicit Out can't be deduced from a parameter, so it drops
  // out). Notch policy: abs/fmod inherit `Notch<In>`; floor/ceil/round/trunc
  // deduce `notch<1>` since their outputs are integer-valued.
  //---------------------------------------------------------------------------

  //---------------------------------------------------------------------------
  // pown<E> — compile-time integer powers, pure grid arithmetic
  //---------------------------------------------------------------------------
  // Repeated squaring in inside-space: every multiply widens the result grid
  // corner-correctly, so the result is exact for exact inputs and negative
  // bases are fine. No engine, no `real` requirement — works on any inside
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

  namespace detail
  {
    using namespace beman::inside::detail;

    // (fmod has no auto form: with two insidable inputs, `fmod<X>(x, y)` is
    // ambiguous between the explicit-Out and auto overloads — partial ordering
    // can't tell them apart. The explicit form is the canonical entry point.)
  } // namespace detail

  template <insidable In>
  [[nodiscard]] constexpr auto abs(In x) noexcept { return abs_impl<detail::abs_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto floor(In x) noexcept { return floor_impl<detail::floor_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto ceil(In x) noexcept { return ceil_impl<detail::ceil_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto round(In x) noexcept { return round_impl<detail::round_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto trunc(In x) noexcept { return trunc_impl<detail::trunc_auto_t<In>>(x); }

  // sqrt: non-negative inside → inside. Newton-Raphson on grid-scaled integer math
  // with a leading-bit initial guess; input must have Lower == 0. The mixed-sign
  // overload below accepts Lower < 0 and errors on a negative runtime value.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sqrt_impl(In x) noexcept
  {
    static_assert(Lower<In> == 0,
                  "beman::inside::math::sqrt: input must start at 0 (use the mixed-sign overload)");
    static_assert(Lower<Out> <= 0,
                  "beman::inside::math::sqrt: Out must include 0");

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(rational{x}, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  // Mixed-sign sqrt: accepts inputs whose interval crosses zero. Returns
  // `unexpected(errc::domain_error)` on a negative runtime value, else same as
  // sqrt_impl.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr std::expected<Out, errc> sqrt_signed_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "beman::inside::math::sqrt: Out must include 0");

    rational v = beman::inside::detail::as_rational(x);
    if (v < rational{0})
      return std::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(v, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — monotonic transcendental tier. Each derives Out from
  // In: Lower/Upper from running the engine cores on the input endpoints at
  // compile time, rounded outward to Notch<In> so the deduced inside covers the
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
    { return exp2_fixed<kRefBits>(v); }

    constexpr rational log2_endpoint(rational v) noexcept
    { return log2_fixed<kRefBits>(v); }

    constexpr rational exp_endpoint(rational v) noexcept
    { return exp_fixed<kRefBits>(v); }

    constexpr rational log_endpoint(rational v) noexcept
    { return ln_fixed<kRefBits>(v); }

    template <imax Base>
    constexpr rational pow_base_endpoint(rational v) noexcept
    {
      imax sc_w = fmul(to_fixed(v, kRefBits),
                       log2_to_fixed<kRefBits>(rational{Base}), kRefBits);
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // Deduction aliases. Each rounds endpoints outward to Notch<In> and adds
    // `round_nearest` — the cores emit sub-notch drift, so the assignment needs
    // a rounding rule to land on the grid.
    template <insidable In>
    using sqrt_auto_t = inside<{{rational{0},
                                ceil_to_notch(sqrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, out_policy<In> | round_nearest>;

    // Mixed-sign sqrt: Upper of the result is sqrt of the larger absolute
    // endpoint, since the runtime value can be anywhere in [Lower, Upper].
    template <insidable In>
    inline constexpr rational sqrt_signed_upper =
        (abs(Lower<In>) > abs(Upper<In>))
            ? abs(Lower<In>) : abs(Upper<In>);

    template <insidable In>
    using sqrt_signed_auto_t = inside<{{rational{0},
                                       ceil_to_notch(sqrt_endpoint(sqrt_signed_upper<In>),
                                                     Notch<In>)},
                                      Notch<In>}, out_policy<In> | round_nearest>;

    // Auto output grid for results in [lo, hi]: the endpoints rounded outward
    // to In's notch, with In's notch and policy (plus round_nearest).
    template <insidable In, rational Lo, rational Hi>
    using outward_t = inside<{{floor_to_notch(Lo, Notch<In>), ceil_to_notch(Hi, Notch<In>)},
                              Notch<In>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using exp2_auto_t = outward_t<In, exp2_endpoint(Lower<In>), exp2_endpoint(Upper<In>)>;

    template <insidable In>
    using log2_auto_t = outward_t<In, log2_endpoint(Lower<In>), log2_endpoint(Upper<In>)>;

    template <insidable In>
    using exp_auto_t = outward_t<In, exp_endpoint(Lower<In>), exp_endpoint(Upper<In>)>;

    template <insidable In>
    using log_auto_t = outward_t<In, log_endpoint(Lower<In>), log_endpoint(Upper<In>)>;

    template <imax Base, insidable In>
    using pow_base_auto_t = outward_t<In, pow_base_endpoint<Base>(Lower<In>), pow_base_endpoint<Base>(Upper<In>)>;
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
                               Notch<In>}, out_policy<In> | round_nearest>;

    template <insidable In>
    using cos_auto_t = sin_auto_t<In>;

    // Output covers [-π, π] rounded outward to notch multiples — the exact
    // ±π endpoints are irrational and would violate the grid's divides-evenly
    // invariant against a rational notch.
    template <insidable In>
    using atan2_auto_t = outward_t<In, -pi_r, pi_r>;

    template <insidable In>
    using tan_auto_t = inside<{{-rational{1024}, rational{1024}},
                               Notch<In>}, out_policy<In> | round_nearest>;

    // fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
    // both notches (x − k·y lies on that lattice, so the result is exact).
    template <insidable B>
    inline constexpr rational max_abs = abs(Lower<B>) > abs(Upper<B>) ? abs(Lower<B>) : abs(Upper<B>);

    template <insidable InX, insidable InY>
    inline constexpr rational fmod_bound =
        max_abs<InX> < max_abs<InY> ? max_abs<InX> : max_abs<InY>;

    template <insidable InX, insidable InY>
    inline constexpr rational fmod_notch =
        (Notch<InX> == 0 || Notch<InY> == 0) ? rational{0} : *gcd(Notch<InX>, Notch<InY>);

    template <insidable InX, insidable InY>
    using fmod_auto_t = inside<{{(Lower<InX> < 0 ? -fmod_bound<InX, InY> : rational{0}),
                                 (Upper<InX> > 0 ?  fmod_bound<InX, InY> : rational{0})},
                                fmod_notch<InX, InY>}, out_policy<InX> | round_nearest>;
  } // namespace detail

  template <insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod(InX x, InY y)
  { return fmod_impl<detail::fmod_auto_t<InX, InY>>(x, y); }

  //===========================================================================
  // Grid-native periodic trig: circle<M> angle + caller-owned amplitude.
  //
  // A `circle<M>` is one revolution split into M equal slots, valued in DEGREES.
  // Degrees have an integer period (360), so a notch divides the circle exactly
  // and `wrap` is drift-free — unlike radians, whose 2π period no rational notch
  // divides. The raw is just the slot index 0..M-1.
  //
  // `sin`/`cos` write into a caller-supplied amplitude inside whose grid fixes the
  // output precision. The runtime path is a table lookup (no ×1/(2π), no
  // polynomial) into a first-quadrant table built at compile time by CORDIC.
  // Power-of-two M is optimal (reflection is a bitmask); any M%4==0 works.
  //===========================================================================
  template <std::uint64_t M>
  using circle = inside<{{rational{0},
                         rational{std::uint64_t{360} * (M - 1),
                                               static_cast<imax>(M)}},
                        notch<360, static_cast<imax>(M)>}, real | wrap>;

  // Amplitude output grid: [-1, 1] at 1/K resolution. The natural target for
  // `sin(circle<M>, amp<K>&)` — angle precision (M) and amplitude precision (K)
  // are chosen independently.
  template <std::uint64_t K>
  using amp = inside<{{rational{-1}, rational{1}},
                     notch<1, static_cast<imax>(K)>}, real>;

  namespace detail
  {
    using namespace beman::inside::detail;

    // First-quadrant sine table for an M-slot circle at working scale 2^W:
    // entry j = sin(j/M turn) for j ∈ [0, M/4], as a rational. Filled at
    // compile time by the grid-scaled CORDIC rotation (`sin_from_turn_fixed`),
    // never evaluated at runtime. Keyed by <M, W> so the entry precision tracks
    // the amplitude grid that selected W.
    template <imax M, int W>
    inline constexpr auto sin_quarter_table = []{
      std::array<rational, static_cast<std::size_t>(M / 4) + 1> t{};
      for (imax j = 0; j <= M / 4; ++j)
      {
        imax turn_w = ((j << W) + M / 2) / M;             // round(j/M · 2^W)
        t[j] = sin_from_turn_fixed<W, W>(turn_w);
      }
      return t;
    }();

    // sin(i/M turn) as a rational for any integer slot i (wraps mod M), by
    // quadrant reduction: sign from the half-turn, reflect about M/4. The table
    // holds first-quadrant magnitudes; this applies the sign. Power-of-two M
    // makes the half/quarter compares a bitmask.
    template <imax M, int W>
    constexpr rational sin_slot(imax i) noexcept
    {
      constexpr imax half = M / 2, quarter = M / 4;
      i = euclid_mod(i, M);                   // wrap into [0, M)
      bool flip = i >= half;
      if (flip) i -= half;                    // sin(π + x) = -sin(x)
      if (i > quarter) i = half - i;          // sin(π - x) =  sin(x)
      rational mag = sin_quarter_table<M, W>[i];
      return flip ? -mag : mag;
    }

    // Recover the slot count M from a circle-shaped angle: the degree period
    // 360 divided by the notch. The public entry points validate the shape.
    template <insidable DEG>
    inline constexpr imax circle_slots =
      round((rational{360} / Notch<DEG>).value());

    // Shared shape check for the circle-input entry points.
    template <insidable DEG>
    constexpr bool valid_circle() noexcept
    {
      static_assert(Lower<DEG> == 0,
                    "beman::inside::math: circle angle must have Lower 0 (degrees)");
      static_assert(has_flag(InsidePolicy<DEG>, wrap),
                    "beman::inside::math: circle angle must carry the wrap policy");
      static_assert(has_flag(InsidePolicy<DEG>, real),
                    "beman::inside::math: circle angle must carry the `real` policy "
                    "(circle<M> already does; custom angle bounds must add `| real`)");
      static_assert(circle_slots<DEG> % 4 == 0,
                    "beman::inside::math: circle slot count M must be divisible by 4");
      return true;
    }
  } // namespace detail

  // sin(angle) → out, on out's amplitude grid. angle is a circle<M>. Reference
  // output (not a return) lets AMP be deduced from the caller's object and reuses
  // its assignment policy for the final rounding.
  template <insidable DEG, insidable AMP>
  BEMAN_INSIDE_MATH_FN void sin(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BEMAN_INSIDE_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(beman::inside::detail::raw_imax(angle));
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
    out = flt::detail::d_sin(flt::to_float(angle) * (flt::detail::kPi / 180.0f));
#else
    out = dbl::detail::d_sin(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // cos(angle) → out. cos(x) = sin(x + ¼ turn): shift the slot by M/4.
  template <insidable DEG, insidable AMP>
  BEMAN_INSIDE_MATH_FN void cos(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BEMAN_INSIDE_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(beman::inside::detail::raw_imax(angle) + M / 4);
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
    out = flt::detail::d_cos(flt::to_float(angle) * (flt::detail::kPi / 180.0f));
#else
    out = dbl::detail::d_cos(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // tan(angle) → out = sin/cos. Returns false (and leaves out untouched) when
  // the angle lands exactly on a pole (cos == 0); overflow of the amplitude
  // grid is handled by out's own policy (e.g. clamp).
  template <insidable DEG, insidable AMP>
  [[nodiscard]] BEMAN_INSIDE_MATH_FN bool tan(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BEMAN_INSIDE_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    imax i = beman::inside::detail::raw_imax(angle);
    rational c = detail::sin_slot<M, W>(i + M / 4);
    if (c == 0) return false;                                  // pole
    out = (detail::sin_slot<M, W>(i) / c).value();             // sin / cos
    return true;
#elif defined(BEMAN_INSIDE_MATH_FLOAT)
    float t;
    if (!flt::detail::d_tan(flt::to_float(angle) * (flt::detail::kPi / 180.0f), t)) return false;   // pole
    out = t;
    return true;
#else
    double t;
    if (!dbl::detail::d_tan(static_cast<double>(angle) * (dbl::detail::kPi / 180.0), t)) return false;   // pole
    out = t;
    return true;
#endif
  }

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
    constexpr rational atan_fixed(rational v) noexcept
    {
      rational av = abs(v);
      if (av <= rational{1})
      {
        imax rad = cordic_atan2_rad<W, W>(to_fixed(v, W), imax{1} << W);
        return fixed_to_rational(rad, W);
      }
      rational inv = 1 / av;
      imax rad = cordic_atan2_rad<W, W>(to_fixed(inv, W), imax{1} << W);
      rational mag = pi_r / 2 - fixed_to_rational(rad, W);
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
        rational half_pi = pi_r / 2;
        return (v < rational{0}) ? -half_pi : half_pi;
      }
      imax rad = cordic_atan2_rad<W, W>(v_w, c_w);        // x = c_w > 0
      return fixed_to_rational(rad, W);
    }

    // acos(v) = π/2 − asin(v); v ∈ [−1, 1] → result ∈ [0, π].
    template <int W = kRefBits>
    constexpr rational acos_endpoint(rational v) noexcept
    {
      rational half_pi = pi_r / 2;
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
      big   = to_fixed(exp_fixed<W>(abs(v)), W);
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
      imax u = to_fixed(exp_fixed<W>(av * -2), W);
      imax t = ((one - u) << W) / (one + u);
      return (v < rational{0}) ? fixed_to_rational(-t, W)
                                            : fixed_to_rational(t, W);
    }

    // --- log10, cbrt ------------------------------------------------------
    template <int W>
    inline constexpr imax inv_ln10_w = inv_ln10_fixed<W>();

    template <int W = kRefBits>
    constexpr rational log10_endpoint(rational v) noexcept
    { return fixed_to_rational(fmul(ln_to_fixed<W>(v), inv_ln10_w<W>, W), W); }

    // cbrt(v) = sign(v)·e^(ln|v|/3); cbrt(0) = 0.
    template <int W = kRefBits>
    constexpr rational cbrt_endpoint(rational v) noexcept
    {
      if (v == rational{0}) return rational{0};
      rational av = abs(v);
      rational mag = exp_from_fixed<W>(ln_to_fixed<W>(av) / 3);
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
    using atan_auto_t = outward_t<In, atan_fixed<working_bits<In>()>(Lower<In>), atan_fixed<working_bits<In>()>(Upper<In>)>;

    template <insidable In>
    using asin_auto_t = outward_t<In, asin_endpoint(Lower<In>), asin_endpoint(Upper<In>)>;

    template <insidable In>
    using acos_auto_t = outward_t<In, acos_endpoint(Upper<In>), acos_endpoint(Lower<In>)>;

    template <insidable In>
    using sinh_auto_t = outward_t<In, sinh_endpoint(Lower<In>), sinh_endpoint(Upper<In>)>;

    template <insidable In>
    inline constexpr rational cosh_auto_lo =
      (Lower<In> <= rational{0} && Upper<In> >= rational{0})
        ? rational{1}
        : (cosh_endpoint(Lower<In>) < cosh_endpoint(Upper<In>)
             ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>));

    template <insidable In>
    inline constexpr rational cosh_auto_hi =
      (cosh_endpoint(Lower<In>) > cosh_endpoint(Upper<In>))
        ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>);

    template <insidable In>
    using cosh_auto_t = outward_t<In, cosh_auto_lo<In>, cosh_auto_hi<In>>;

    template <insidable In>
    using tanh_auto_t = outward_t<In, tanh_endpoint(Lower<In>), tanh_endpoint(Upper<In>)>;

    template <insidable In>
    using log10_auto_t = outward_t<In, log10_endpoint(Lower<In>), log10_endpoint(Upper<In>)>;

    template <insidable In>
    using cbrt_auto_t = outward_t<In, cbrt_endpoint(Lower<In>), cbrt_endpoint(Upper<In>)>;

    // hypot output: non-negative, Upper at the largest-magnitude corner.
    template <insidable InX, insidable InY>
    inline constexpr rational hypot_auto_hi =
      hypot_endpoint(
        (abs(Lower<InX>) > abs(Upper<InX>)
           ? abs(Lower<InX>) : abs(Upper<InX>)),
        (abs(Lower<InY>) > abs(Upper<InY>)
           ? abs(Lower<InY>) : abs(Upper<InY>)));

    template <insidable InX, insidable InY>
    using hypot_auto_t = inside<{{rational{0},
                                 ceil_to_notch(hypot_auto_hi<InX, InY>, Notch<InX>)},
                                Notch<InX>}, out_policy<InX> | round_nearest>;

    // pow output: extrema of b^e over the input rectangle occur at corners
    // (monotone in each argument for b > 0). Min and max of the 4 corners.
    template <insidable InB, insidable InE>
    inline constexpr rational pow_corner[4] = {
      pow_endpoint(Lower<InB>, Lower<InE>), pow_endpoint(Lower<InB>, Upper<InE>),
      pow_endpoint(Upper<InB>, Lower<InE>), pow_endpoint(Upper<InB>, Upper<InE>),
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
    using pow_auto_t = inside<{{floor_to_notch(pow_auto_lo<InB, InE>, Notch<InB>),
                               ceil_to_notch (pow_auto_hi<InB, InE>, Notch<InB>)},
                              Notch<InB>}, out_policy<InB> | round_nearest>;
  } // namespace detail

  // --- explicit-Out impls -------------------------------------------------
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out atan_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::atan: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::atan_fixed<detail::working_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out asin_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "beman::inside::math::asin: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::asin_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out acos_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "beman::inside::math::acos: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::acos_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sinh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "beman::inside::math::sinh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::sinh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cosh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "beman::inside::math::cosh: input must be in [-10, 10]");
    static_assert(Lower<Out> <= rational{1},
                  "beman::inside::math::cosh: Out must include 1 (cosh ≥ 1)");
    return detail::store_grid<Out>(detail::cosh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out tanh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "beman::inside::math::tanh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::tanh_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out log10_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "beman::inside::math::log10: input must be strictly positive");
    return detail::store_grid<Out>(detail::log10_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out cbrt_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "beman::inside::math::cbrt: input magnitude must be ≤ 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::cbrt_endpoint<detail::endpoint_bits<Out>()>(rational{x}));
  }

  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out hypot_impl(InX x, InY y) noexcept
  {
    static_assert(Lower<InX> >= -(imax{1} << 20) && Upper<InX> <= (imax{1} << 20)
               && Lower<InY> >= -(imax{1} << 20) && Upper<InY> <= (imax{1} << 20),
                  "beman::inside::math::hypot: input magnitudes must be ≤ 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= 0, "beman::inside::math::hypot: Out must include 0");
    return detail::store_grid<Out>(detail::hypot_endpoint<detail::endpoint_bits<Out>()>(rational{x}, rational{y}));
  }

  // pow: b^e for runtime base b > 0. Returns `expected` — `overflow` when
  // e·log2(b) leaves exp2's [-30, 30] envelope or the result leaves Out's
  // interval. The auto form requires Lower<InB> > 0 (so b > 0 is guaranteed
  // and the output range is bounded for deduction).
  template <insidable Out, insidable InB, insidable InE>
  [[nodiscard]] constexpr std::expected<Out, errc> pow_impl(InB base, InE exp) noexcept
  {
    rational bv = base;
    if (bv <= rational{0})
      return std::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax sc_w = detail::fmul(detail::to_fixed(rational{exp}, W),
                             detail::log2_to_fixed<W>(bv), W);     // e·log2(b), scale 2^W
    constexpr imax lim = imax{30} << W;
    if (sc_w > lim || sc_w < -lim)
      return std::unexpected(errc::overflow);

    rational r = detail::exp2_from_fixed<W>(sc_w);
    if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
      if (r < Lower<Out> || r > Upper<Out>)
        return std::unexpected(errc::overflow);
    return detail::store_grid<Out>(r);
  }

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
      requires (Lower<In> == rational{0})
    [[nodiscard]] constexpr auto sqrt(In x) noexcept
    { static_assert(detail::require_snap<In>()); return sqrt_impl<detail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (Lower<In> < rational{0})
    [[nodiscard]] constexpr auto sqrt(In x) noexcept
    { static_assert(detail::require_snap<In>()); return sqrt_signed_impl<detail::sqrt_signed_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto exp2(In x) noexcept
    { static_assert(detail::require_snap<In>()); return exp2_impl<detail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log2(In x) noexcept
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::cordic::log2: input must be strictly positive");
      return log2_impl<detail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto exp(In x) noexcept
    { static_assert(detail::require_snap<In>()); return exp_impl<detail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log(In x) noexcept
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::cordic::log: input must be strictly positive");
      return log_impl<detail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] constexpr auto pow_base(In x) noexcept
    { static_assert(detail::require_snap<In>()); return pow_base_impl<Base, detail::pow_base_auto_t<Base, In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto sin(In angle) noexcept
    { static_assert(detail::require_snap<In>()); return sin_impl<detail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto cos(In angle) noexcept
    { static_assert(detail::require_snap<In>()); return cos_impl<detail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto tan(In angle) noexcept
    { static_assert(detail::require_snap<In>()); return tan_impl<detail::tan_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] constexpr auto atan2(In y, In x) noexcept
    { static_assert(detail::require_snap<In>()); return atan2_impl<detail::atan2_auto_t<In>>(y, x); }

    template <insidable In>
    [[nodiscard]] constexpr auto atan(In x) noexcept
    { static_assert(detail::require_snap<In>()); return atan_impl<detail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto asin(In x) noexcept
    { static_assert(detail::require_snap<In>()); return asin_impl<detail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto acos(In x) noexcept
    { static_assert(detail::require_snap<In>()); return acos_impl<detail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto sinh(In x) noexcept
    { static_assert(detail::require_snap<In>()); return sinh_impl<detail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto cosh(In x) noexcept
    { static_assert(detail::require_snap<In>()); return cosh_impl<detail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto tanh(In x) noexcept
    { static_assert(detail::require_snap<In>()); return tanh_impl<detail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] constexpr auto log10(In x) noexcept
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::cordic::log10: input must be strictly positive");
      return log10_impl<detail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] constexpr auto cbrt(In x) noexcept
    { static_assert(detail::require_snap<In>()); return cbrt_impl<detail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] constexpr auto hypot(InX x, InY y) noexcept
    {
      static_assert(detail::require_snap<InX>() && detail::require_snap<InY>());
      return hypot_impl<detail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (Lower<InB> > rational{0})
    [[nodiscard]] constexpr auto pow(InB base, InE exp) noexcept
    {
      static_assert(detail::require_snap<InB>() && detail::require_snap<InE>());
      return pow_impl<detail::pow_auto_t<InB, InE>>(base, exp);
    }
  } // namespace cordic

  // The shared deduction/helpers, as seen from the engine namespaces (where a
  // bare `detail::` names the engine's own cores).
  namespace mdetail = beman::inside::math::detail;

#ifndef BEMAN_INSIDE_MATH_NO_FP
  namespace dbl
  {
    // Public-shaped double-engine entry points. `detail::` here would resolve to
    // beman::inside::math::dbl::detail (the engine cores), so the shared deduction/helpers
    // are qualified as `mdetail::`; the `*_core`/`store`/`d_*` names are
    // this namespace's own. Absent under BEMAN_INSIDE_MATH_NO_FP (no FP, no <cmath>).
    template <insidable In>
      requires (Lower<In> == rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return sqrt_core<mdetail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (Lower<In> < rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::sqrt_signed_auto_t<In>;
      double v = static_cast<double>(x);
      if (v < 0.0)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      return std::expected<Out, errc>{store<Out>(detail::d_sqrt(v))};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp2(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return exp2_core<mdetail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log2(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::dbl::log2: input must be strictly positive");
      return log2_core<mdetail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return exp_core<mdetail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::dbl::log: input must be strictly positive");
      return log_core<mdetail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow_base(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::pow_base_auto_t<Base, In>;
      return store<Out>(detail::d_pow(static_cast<double>(Base), static_cast<double>(x)));
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sin(In angle) noexcept
    { static_assert(mdetail::require_snap<In>()); return sin_core<mdetail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cos(In angle) noexcept
    { static_assert(mdetail::require_snap<In>()); return cos_core<mdetail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tan(In angle) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::tan_auto_t<In>;
      double t;
      if (!detail::d_tan(static_cast<double>(angle), t))
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (t < mdetail::lower_fp<double, Out> || t > mdetail::upper_fp<double, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(t)};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan2(In y, In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return atan2_core<mdetail::atan2_auto_t<In>>(y, x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return atan_core<mdetail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto asin(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return asin_core<mdetail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto acos(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return acos_core<mdetail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sinh(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return sinh_core<mdetail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cosh(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return cosh_core<mdetail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tanh(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return tanh_core<mdetail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log10(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::dbl::log10: input must be strictly positive");
      return log10_core<mdetail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cbrt(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return cbrt_core<mdetail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto hypot(InX x, InY y) noexcept
    {
      static_assert(mdetail::require_snap<InX>() && mdetail::require_snap<InY>());
      return hypot_core<mdetail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (Lower<InB> > rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow(InB base, InE exp) noexcept
    {
      static_assert(mdetail::require_snap<InB>() && mdetail::require_snap<InE>());
      using Out = mdetail::pow_auto_t<InB, InE>;
      double b = static_cast<double>(base);
      if (b <= 0.0)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      double r = detail::d_pow(b, static_cast<double>(exp));
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (r < mdetail::lower_fp<double, Out> || r > mdetail::upper_fp<double, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(r)};
    }
  } // namespace dbl

  namespace flt
  {
    // Public-shaped float-engine entry points — binary32 compute, same shapes as
    // dbl:: (qualify shared helpers as mdetail::; *_core/store/d_* are
    // this namespace's own). A third value set (float ≠ double ≠ cordic).
    template <insidable In>
      requires (Lower<In> == rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return sqrt_core<mdetail::sqrt_auto_t<In>>(x); }

    template <insidable In>
      requires (Lower<In> < rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sqrt(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::sqrt_signed_auto_t<In>;
      float v = flt::to_float(x);
      if (v < 0.0f)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      return std::expected<Out, errc>{store<Out>(detail::d_sqrt(v))};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp2(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return exp2_core<mdetail::exp2_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log2(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::flt::log2: input must be strictly positive");
      return log2_core<mdetail::log2_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto exp(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return exp_core<mdetail::exp_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::flt::log: input must be strictly positive");
      return log_core<mdetail::log_auto_t<In>>(x);
    }

    template <imax Base, insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow_base(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::pow_base_auto_t<Base, In>;
      return store<Out>(detail::d_pow(static_cast<float>(Base), flt::to_float(x)));
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sin(In angle) noexcept
    { static_assert(mdetail::require_snap<In>()); return sin_core<mdetail::sin_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cos(In angle) noexcept
    { static_assert(mdetail::require_snap<In>()); return cos_core<mdetail::cos_auto_t<In>>(angle); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tan(In angle) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      using Out = mdetail::tan_auto_t<In>;
      float t;
      if (!detail::d_tan(flt::to_float(angle), t))
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (t < mdetail::lower_fp<float, Out> || t > mdetail::upper_fp<float, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(t)};
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan2(In y, In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return atan2_core<mdetail::atan2_auto_t<In>>(y, x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto atan(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return atan_core<mdetail::atan_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto asin(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return asin_core<mdetail::asin_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto acos(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return acos_core<mdetail::acos_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto sinh(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return sinh_core<mdetail::sinh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cosh(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return cosh_core<mdetail::cosh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto tanh(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return tanh_core<mdetail::tanh_auto_t<In>>(x); }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto log10(In x) noexcept
    {
      static_assert(mdetail::require_snap<In>());
      static_assert(Lower<In> > 0, "beman::inside::math::flt::log10: input must be strictly positive");
      return log10_core<mdetail::log10_auto_t<In>>(x);
    }

    template <insidable In>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto cbrt(In x) noexcept
    { static_assert(mdetail::require_snap<In>()); return cbrt_core<mdetail::cbrt_auto_t<In>>(x); }

    template <insidable InX, insidable InY>
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto hypot(InX x, InY y) noexcept
    {
      static_assert(mdetail::require_snap<InX>() && mdetail::require_snap<InY>());
      return hypot_core<mdetail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <insidable InB, insidable InE>
      requires (Lower<InB> > rational{0})
    [[nodiscard]] BEMAN_INSIDE_DBL_FN auto pow(InB base, InE exp) noexcept
    {
      static_assert(mdetail::require_snap<InB>() && mdetail::require_snap<InE>());
      using Out = mdetail::pow_auto_t<InB, InE>;
      float b = flt::to_float(base);
      if (b <= 0.0f)
        return std::expected<Out, errc>{std::unexpected(errc::domain_error)};
      float r = detail::d_pow(b, flt::to_float(exp));
      if constexpr (!has_flag(InsidePolicy<Out>, clamp))   // clamp Out: saturate below
        if (r < mdetail::lower_fp<float, Out> || r > mdetail::upper_fp<float, Out>)
          return std::expected<Out, errc>{std::unexpected(errc::overflow)};
      return std::expected<Out, errc>{store<Out>(r)};
    }
  } // namespace flt
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
// (unsigned 8/16/32) and `sbyte`/`sword`/`sdword`/`sqword` (signed 8/16/32/64).
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
  // qword (unsigned 64) is intentionally absent: the library's internal value
  // path is `imax` (int64) — `to_value` returns `imax` — so unsigned values above
  // 2^63-1 cannot round-trip through arithmetic/compare. Use `sqword` or a
  // hand-rolled grid if you need 64-bit storage.

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
  using unorm8  = inside<{{0, 1}, notch<1, 255>},        round_nearest>; // uint8
  using unorm16 = inside<{{0, 1}, notch<1, 65535>},      round_nearest>; // uint16
  using unorm32 = inside<{{0, 1}, notch<1, 4294967295>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
  // full natural range. `round_nearest`.
  //-------------------------------------------------------------------------
  using q4_4   = inside<{{0, 15},    notch<1, 16>},    round_nearest>; // uint8
  using q8_8   = inside<{{0, 255},   notch<1, 256>},   round_nearest>; // uint16
  using q16_16 = inside<{{0, 65535}, notch<1, 65536>}, round_nearest>; // uint32

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
#include <ostream>
#include <version>

namespace beman::inside
{
  //-------------------------------------------------------------------------
  // to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
  // for plain arithmetic types and the exact-rational form for insidables.
  //-------------------------------------------------------------------------
  inline std::string to_string(beman::inside::detail::rational r)
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

  inline std::string to_string(interval ival)
  {
    std::string str{"["};

    str += beman::inside::to_string(ival.Lower);
    str += "..";
    str += beman::inside::to_string(ival.Upper);
    str += "]";
    return str;
  }

  inline std::string to_string(grid g)
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
  auto to_string(V value)
  { return std::to_string(value); }

  // `real` (double-backed) and `exact` (rational-backed) bounds: render the
  // exact rational form. (Without this overload a real inside would fall to the
  // generic `std::to_string(double)` and print a lossy 6-digit form, and a
  // rational-raw inside has no std::to_string at all.) A continuous (Notch == 0)
  // real inside prints the double.
  template <insidable B>
    requires (detail::fp_raw<B> || detail::rational_raw<B>)
  inline std::string to_string(B b)
  {
    if constexpr (detail::fp_raw<B> && Notch<B> == beman::inside::detail::rational{0})
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
      return "unknown";
    }
  } // namespace detail

  //-------------------------------------------------------------------------
  // to_string / to_string_debug / operator<< for insidables and rational.
  // The debug form also prints the raw value, raw type, and grid — useful when
  // inspecting failing tests or storage choices.
  //-------------------------------------------------------------------------
  template <insidable B>
  inline std::string to_string(B b)
  { return beman::inside::to_string(detail::as_rational(b)); }

  template <insidable B>
  inline std::string to_string_debug(B b)
  {
    std::string str;
    str += beman::inside::to_string(detail::as_rational(b));
    str += " {";
    str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name<detail::raw_t<B>>());
    str += " Max:" + beman::inside::to_string(+detail::NotchCount<B>) + "] ";
    str += beman::inside::to_string(Grid<B>);
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

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration is gated on a working <format> (libstdc++ ships it
// from GCC 13; GCC 12 / C++20 builds compile this as a no-op and rely on
// to_string()/operator<< instead).
//---------------------------------------------------------------------------
#ifdef __cpp_lib_format

#include <format>
#include <type_traits>

namespace beman::inside::detail
{
  // Shared spec handling: an empty `{}` is left to the derived format() (exact
  // to_string); a non-empty spec is parsed and applied by `numeric_`.
  template <class Inner>
  struct numeric_spec_formatter
  {
    Inner numeric_{};
    bool  has_spec_ = false;

    constexpr auto parse(std::format_parse_context& ctx)
    {
      auto it = ctx.begin();
      if (it != ctx.end() && *it != '}')
      {
        has_spec_ = true;
        return numeric_.parse(ctx);
      }
      return it;
    }
  };
}

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
  : beman::inside::detail::numeric_spec_formatter<
      std::conditional_t<beman::inside::detail::IsIntegerAligned<beman::inside::inside<G, P>> && G.Notch != 0,
                         std::formatter<beman::inside::imax>,
                         std::formatter<double>>>
{
  using B = beman::inside::inside<G, P>;
  // Integer formatting only for a notched integer grid: a continuous grid
  // (notch 0) holds fractions even between integer bounds.
  static constexpr bool integer_path = beman::inside::detail::IsIntegerAligned<B> && G.Notch != 0;

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return this->numeric_.format(beman::inside::detail::to_value(b), ctx);
    else if (this->has_spec_)
      return this->numeric_.format(
          static_cast<double>(beman::inside::detail::rational{b}), ctx);
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
    if (has_spec_)
      return numeric_.format(static_cast<double>(r), ctx);
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
// numeric_limits / hash / common_type — std:: specialisations for inside<G, P>.
// numeric_limits reports the *grid* bounds (Lower/Upper), not the raw type's
// limits. std::hash hashes the Raw member (rational raw: Numerator+Denominator,
// boost-style combine). std::common_type is the grid hull (see beman::inside::common_inside
// in arithmetic.hpp) so mixed-grid bounds interoperate with generic code.
//---------------------------------------------------------------------------

template <beman::inside::grid G1, beman::inside::policy_flag P1, beman::inside::grid G2, beman::inside::policy_flag P2>
struct std::common_type<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>>
  : beman::inside::detail::common_inside<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>> {};


template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::numeric_limits<beman::inside::inside<G, P>>
{
  using B = beman::inside::inside<G, P>;

  static constexpr bool is_specialized = true;
  static constexpr bool is_signed      = (G.Interval.Lower < beman::inside::detail::rational{0});
  static constexpr bool is_integer     = (G.Notch == beman::inside::detail::rational{1});
  static constexpr bool is_exact       = true;     // rational + integer raw are both exact
  static constexpr bool is_bounded     = true;
  static constexpr bool is_modulo      = (P & beman::inside::wrap) != 0;
  static constexpr bool has_infinity   = false;
  static constexpr bool has_quiet_NaN  = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr bool traps          = (P & beman::inside::checked) != 0;
  static constexpr bool is_iec559      = false;
  static constexpr int  radix          = 2;
  static constexpr std::float_round_style round_style =
      beman::inside::has_flag(P, beman::inside::round_nearest) ? std::round_to_nearest
                                                       : std::round_toward_zero;

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
    else
      return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
  }
};


#endif // BEMAN_INSIDE_SINGLE_HEADER_HPP
