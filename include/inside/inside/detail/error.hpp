// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_ERROR_HPP
#define INSIDE_INSIDE_DETAIL_ERROR_HPP

#include <inside/inside/detail/config.hpp>
#include <algorithm>
#include <atomic> // the handler (freestanding)

// <stdexcept> backs inside_error, the compile-time diagnostic of a failed check in a constant expression,
// and is included only when exceptions are available; nothing throws at run time.
// A hosted build's default handler prints with <cstdio> and stops with <cstdlib>; a freestanding one traps.
// Otherwise errors stay static const char* messages: no <string>, <string_view> or <system_error>
// (text output is in the opt-in "inside/inside/io.hpp").
#if INSIDE_HAS_EXCEPTIONS // (config.hpp)
  #include <stdexcept>
#endif
#if __STDC_HOSTED__
  #include <cstdio>
  #include <cstdlib>
#endif

// Error codes, the replaceable failure handler and diagnostic helpers.
// A failure a program can expect is a value (a std::expected, an error&, an on_error hook),
// and a store the grids cannot prove does not compile without one.
// What is left goes through detail::raise:
// a precondition (an mdspan index past a dynamic extent, a cursor past its end) or a library bug.
// raise calls the installed error_handler, by default one that prints the message and aborts (hosted)
// or traps (freestanding); set_error_handler lets a bare-metal target send failures to a reset or a log.
// In a constant expression a failed check is a compile error that names it.
namespace ins
{
  enum class error
  {
    domain_error = 1, // an argument outside a function's mathematical domain
    division_by_zero, // a zero divisor
    overflow,         // a value past its destination's range (an inside's interval, a native type, a rational's
                      // 64-bit fields)
    rounding_error,   // a value off the grid (between notches)
    not_finite,       // a non-finite double input (NaN, Inf)
    invalid_format,   // text that is not a number (parse, operator>>)
    missing_unit,     // a quantity's text without its unit ("12.5" for metres)
    unknown_unit,     // a unit's text that names no unit visible at the call, or does not read as one
    incompatible_unit // a unit that does not convert exactly to the quantity's (another dimension or kind,
                      // a factor of pi, another absolute origin)
  };

  // The static, allocation-free message of a code, the one source for every error path:
  // a null-terminated literal, so it doubles as the default handler's text and an on_error message.
  constexpr const char* error_message(error e) noexcept
  {
    switch (e)
    {
      case error::domain_error:
        return "argument outside the function's domain";
      case error::division_by_zero:
        return "division by zero";
      case error::overflow:
        return "value does not fit its range";
      case error::rounding_error:
        return "value is not on the grid";
      case error::not_finite:
        return "non-finite floating-point value";
      case error::invalid_format:
        return "malformed number";
      case error::missing_unit:
        return "number without its unit";
      case error::unknown_unit:
        return "unknown unit";
      case error::incompatible_unit:
        return "unit does not convert exactly to the quantity's";
    }
    return "unknown inside error";
  }

#if INSIDE_HAS_EXCEPTIONS
  // inside_error is thrown only during constant evaluation,
  // where an uncaught exception is a compile error that prints its what():
  // the text of a failed check in a constant expression. Nothing throws it at run time.
  // It derives from std::runtime_error (the library's only <stdexcept> use) and carries its error.
  namespace detail
  {
    struct inside_error : std::runtime_error
    {
      error Code;
      constexpr explicit inside_error(error c) : std::runtime_error(error_message(c)), Code(c) {}
      constexpr inside_error(error c, const char* what)
        : std::runtime_error(what ? what : error_message(c)), Code(c) {}
    };
  } // namespace detail
#endif

  // The replaceable failure handler, for preconditions (and library bugs).
  // It must not return (abort, reset, longjmp, throw); if it does, raise traps to honour [[noreturn]].
  using error_handler_t = void (*)(error code, const char* what);

  namespace detail
  {
    [[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void default_error_handler(error code, const char* what)
    {
#if __STDC_HOSTED__
      std::fputs("ins: ", stderr);
      std::fputs(what ? what : error_message(code), stderr);
      std::fputs("\n", stderr);
      std::abort();
#else
      (void)code;
      (void)what;
      __builtin_trap();
#endif
    }

    // A header-only global, one per program.
    // Atomic, so one thread may install a handler while another raises;
    // default visibility, so shared libraries built with -fvisibility=hidden share it.
    // Only run time changes it (constant evaluation never reads it), so constexpr paths are unaffected.
    [[gnu::visibility("default")]] inline std::atomic<error_handler_t> g_error_handler{&default_error_handler};
  } // namespace detail

  // Installs a handler and returns the previous one; null restores the default.
  // Never throws; safe while other threads raise.
  inline error_handler_t set_error_handler(error_handler_t h) noexcept
  {
    return detail::g_error_handler.exchange(h ? h : &detail::default_error_handler, std::memory_order_acq_rel);
  }

  inline error_handler_t get_error_handler() noexcept
  {
    return detail::g_error_handler.load(std::memory_order_acquire);
  }

  namespace detail
  {
    // A failed check in a constant expression without exceptions:
    // a compile error naming the code (printing `what` would need exceptions).
    [[noreturn]] constexpr void constexpr_error_of(error code);

    // The outlined failure funnel, cold and not inlined, so the rare machinery is emitted once, off the hot path.
    // During constant evaluation it throws an inside_error (C++26 constexpr exceptions),
    // so a failed compile-time path is a compile error whose text is `what`; without exceptions, one naming the code.
    [[noreturn]] [[gnu::cold]] [[gnu::noinline]] constexpr void raise(error code, const char* what = nullptr)
    {
      if consteval
      {
#if INSIDE_HAS_EXCEPTIONS
        throw inside_error(code, what);
#else
        (void)what;
        constexpr_error_of(code);
#endif
      }
      g_error_handler.load(std::memory_order_acquire)(code, what ? what : error_message(code));
      __builtin_trap(); // the handler must not return; trap if it did
    }

    // The compile-time text of a failed checked store: what happened, and the fix.
    constexpr const char* store_failure_message(error e) noexcept
    {
      switch (e)
      {
        case error::overflow:
          return "inside: a checked store in a constant expression is outside the grid's interval - widen the "
                 "interval, or store with clamp or wrap";
        case error::rounding_error:
          return "inside: a checked store in a constant expression is not on the grid's notch - write the value "
                 "exactly (3.49_q, \"1/3\"_q), or store with a rounding policy";
        default:
          return error_message(e);
      }
    }

    // A compile-time-only diagnostic:
    // a fixed_string template argument carries the message into the type,
    // so a malformed literal or an overflow stops constant evaluation with the text in the instantiation.
    // No throw token, so it compiles under -fno-exceptions too.
    // Never reached at run time: every call is behind if consteval or in a consteval function.
    template <unsigned N>
    struct fixed_string
    {
      char Data[N]{};
      constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, Data); }
    };

    template <fixed_string Msg>
    [[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void constexpr_error()
    {
      raise(error::overflow, Msg.Data);
    }

    // A checked store that fails in a constant expression:
    // what happened, and the fix (store_failure_message's text, without exceptions).
    constexpr void store_constexpr_error(error code)
    {
      if (code == error::overflow)
        constexpr_error<"inside: a checked store in a constant expression is outside the grid's interval - widen "
                        "the interval, or store with clamp or wrap">();
      if (code == error::rounding_error)
        constexpr_error<"inside: a checked store in a constant expression is not on the grid's notch - write the "
                        "value exactly (3.49_q, \"1/3\"_q), or store with a rounding policy">();
      constexpr_error_of(code);
    }

    constexpr void constexpr_error_of(error code)
    {
      switch (code)
      {
        case error::overflow:
          constexpr_error<"inside: a value does not fit its range (overflow) in a constant expression">();
        case error::rounding_error:
          constexpr_error<"inside: a value is not on the grid's notch (rounding_error) in a constant expression">();
        case error::division_by_zero:
          constexpr_error<"inside: division by zero in a constant expression">();
        case error::not_finite:
          constexpr_error<"inside: a non-finite floating-point value (not_finite) in a constant expression">();
        case error::invalid_format:
          constexpr_error<"inside: malformed number text (invalid_format) in a constant expression">();
        case error::missing_unit:
          constexpr_error<"units: a number without its unit (missing_unit) in a constant expression">();
        case error::unknown_unit:
          constexpr_error<"units: an unknown unit (unknown_unit) in a constant expression">();
        case error::incompatible_unit:
          constexpr_error<"units: a unit that does not convert exactly (incompatible_unit) in a constant "
                          "expression">();
        default:
          constexpr_error<"inside: argument outside the function's domain (domain_error) in a constant expression">();
      }
    }

    // A checked path's hard failure:
    // Msg as the compile-time diagnostic during constant evaluation, else raised with the code.
    template <fixed_string Msg>
    [[noreturn]] constexpr void fail_hard(error code)
    {
#if !INSIDE_HAS_EXCEPTIONS
      if consteval
      {
        constexpr_error<Msg>();
      }
#endif
      raise(code, Msg.Data);
    }
  } // namespace detail
} // namespace ins

#endif // INSIDE_INSIDE_DETAIL_ERROR_HPP
