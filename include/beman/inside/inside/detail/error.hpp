// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_ERROR_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_ERROR_HPP

#include <beman/inside/inside/detail/config.hpp>
#include <algorithm>
#include <atomic> // the handler (freestanding)

// <stdexcept> backs inside_error, the compile-time diagnostic of a failed
// check in a constant expression; it is pulled in solely when exceptions are
// available. Nothing throws at run time. A hosted build's default handler
// prints with <cstdio> and stops with <cstdlib>; a freestanding one traps.
// Error reporting otherwise stays on static const-char* messages — no
// <string>, no <string_view>, no <system_error>. (General-purpose
// stringification lives in the opt-in "beman/inside/inside/io.hpp".)
#if BEMAN_INSIDE_HAS_EXCEPTIONS // (config.hpp)
    #include <stdexcept>
#endif
#if __STDC_HOSTED__
    #include <cstdio>
    #include <cstdlib>
#endif

//---------------------------------------------------------------------------
// debug — error codes, the replaceable failure handler, and diagnostic helpers.
// `error` enumerates the failure modes. A failure a program can expect is a
// value: a std::expected, an error&, an on_error hook; a store the grids cannot
// prove does not compile without one. What is left funnels through
// `detail::raise`: a precondition (an mdspan index past a dynamic extent, a
// cursor past its end) or a library bug. raise calls the installed
// `error_handler` — by default one that prints the message and aborts
// (hosted) or traps (freestanding). `set_error_handler` lets a bare-metal
// target redirect failures to a reset/log. In a constant expression a failed
// check is a compile error that names it.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// error codes
//---------------------------------------------------------------------------
enum class error {
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
// the default handler's text and as an on_error message.
constexpr const char* error_message(error e) noexcept {
    switch (e) {
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
    }
    return "unknown inside error";
}

#if BEMAN_INSIDE_HAS_EXCEPTIONS
//---------------------------------------------------------------------------
// inside_error — thrown only during constant evaluation, where an uncaught
// exception is a compile error that prints its what(): the text of a failed
// check in a constant expression. Nothing throws it at run time. Derives from
// std::runtime_error (the library's only <stdexcept> use) and carries the
// originating `error`.
//---------------------------------------------------------------------------
namespace detail {
struct inside_error : std::runtime_error {
    error Code;
    constexpr explicit inside_error(error c) : std::runtime_error(error_message(c)), Code(c) {}
    constexpr inside_error(error c, const char* what) : std::runtime_error(what ? what : error_message(c)), Code(c) {}
};
} // namespace detail
#endif

//---------------------------------------------------------------------------
// Replaceable failure handler, for preconditions (and library bugs). Contract:
// it must NOT return (abort / reset / longjmp / throw). If it does return,
// `raise` traps to honour [[noreturn]].
//---------------------------------------------------------------------------
using error_handler_t = void (*)(error code, const char* what);

namespace detail {
[[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void default_error_handler(error code, const char* what) {
#if __STDC_HOSTED__
    std::fputs("beman::inside: ", stderr);
    std::fputs(what ? what : error_message(code), stderr);
    std::fputs("\n", stderr);
    std::abort();
#else
    (void)code;
    (void)what;
    __builtin_trap();
#endif
}

// Header-only global: an inline variable, one per program. Atomic, so one
// thread may install a handler while another raises; default visibility, so
// shared libraries built with -fvisibility=hidden share it too. Mutating it is
// a runtime-only act (constant evaluation never reads it), so constexpr paths
// are unaffected.
[[gnu::visibility("default")]] inline std::atomic<error_handler_t> g_error_handler{&default_error_handler};
} // namespace detail

// Install a handler; returns the previous one. A null argument restores the
// default. Never throws; safe to call while other threads raise.
inline error_handler_t set_error_handler(error_handler_t h) noexcept {
    return detail::g_error_handler.exchange(h ? h : &detail::default_error_handler, std::memory_order_acq_rel);
}

inline error_handler_t get_error_handler() noexcept { return detail::g_error_handler.load(std::memory_order_acquire); }

namespace detail {
// A failed check in a constant expression without exceptions: a compile error
// naming the code (the text of `what` would need exceptions to print).
[[noreturn]] constexpr void constexpr_error_of(error code);

//-----------------------------------------------------------------------
// Outlined failure funnel. Cold + non-inline so the (rare) machinery is
// emitted once, off the hot path. During constant evaluation it throws an
// inside_error (C++26 constexpr exceptions), so a failed compile-time path is
// a compile error whose text is `what`; without exceptions, a compile error
// naming the code.
//-----------------------------------------------------------------------
[[noreturn]] [[gnu::cold]] [[gnu::noinline]] constexpr void raise(error code, const char* what = nullptr) {
    if consteval {
#if BEMAN_INSIDE_HAS_EXCEPTIONS
        throw inside_error(code, what);
#else
        (void)what;
        constexpr_error_of(code);
#endif
    }
    g_error_handler.load(std::memory_order_acquire)(code, what ? what : error_message(code));
    __builtin_trap(); // handler must not return; trap if it did
}

// The compile-time text of a failed checked store: what happened, and the fix.
constexpr const char* store_failure_message(error e) noexcept {
    switch (e) {
    case error::overflow:
        return "inside: a checked store in a constant expression is outside the grid's interval - widen the "
               "interval, or store with clamp or wrap";
    case error::rounding_error:
        return "inside: a checked store in a constant expression is not on the grid's notch - write the value "
               "exactly (3.49_ins, just<349, 100>), or store with a rounding policy";
    default:
        return error_message(e);
    }
}

//-----------------------------------------------------------------------
// Compile-time-only diagnostic. A fixed_string NTTP carries the message
// into the type, so a malformed literal / overflow aborts constant
// evaluation with the text in the instantiation — no `throw` token, so it
// also compiles under -fno-exceptions. Never reached at runtime (all call
// sites are guarded by if consteval or are consteval).
//-----------------------------------------------------------------------
template <unsigned N>
struct fixed_string {
    char Data[N]{};
    constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, Data); }
};

template <fixed_string Msg>
[[noreturn]] [[gnu::cold]] [[gnu::noinline]] inline void constexpr_error() {
    raise(error::overflow, Msg.Data);
}

// A checked store that fails in a constant expression: what happened, and the
// fix (store_failure_message's text, without exceptions).
constexpr void store_constexpr_error(error code) {
    if (code == error::overflow)
        constexpr_error<"inside: a checked store in a constant expression is outside the grid's interval - widen "
                        "the interval, or store with clamp or wrap">();
    if (code == error::rounding_error)
        constexpr_error<"inside: a checked store in a constant expression is not on the grid's notch - write the "
                        "value exactly (3.49_ins, just<349, 100>), or store with a rounding policy">();
    constexpr_error_of(code);
}

constexpr void constexpr_error_of(error code) {
    switch (code) {
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
    default:
        constexpr_error<"inside: argument outside the function's domain (domain_error) in a constant expression">();
    }
}

// A checked path's hard failure: Msg as the compile-time diagnostic during
// constant evaluation, else raised with code.
template <fixed_string Msg>
[[noreturn]] constexpr void fail_hard(error code) {
#if !BEMAN_INSIDE_HAS_EXCEPTIONS
    if consteval {
        constexpr_error<Msg>();
    }
#endif
    raise(code, Msg.Data);
}
} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_DETAIL_ERROR_HPP
