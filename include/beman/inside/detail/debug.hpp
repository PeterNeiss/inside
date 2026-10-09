// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_DEBUG_HPP
#define BEMAN_INSIDE_DETAIL_DEBUG_HPP

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

#endif // BEMAN_INSIDE_DETAIL_DEBUG_HPP
