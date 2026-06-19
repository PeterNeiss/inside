//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdebugHPP
#define BNDdebugHPP

#include <system_error>
#include <string_view>
// <string> is pulled in only when rich (value/interval) error messages are
// enabled (-DBND_RICH_MESSAGES). The cheap default reports through a static
// category message and never builds an std::string of its own. (std::string
// itself remains reachable via <system_error>, which needs it for
// std::error_category::message — gating the explicit include just keeps it off
// the cheap default's *intentional* surface.)
#ifdef BND_RICH_MESSAGES
    #include <string>
#endif

#ifdef BOUND_HAS_STACKTRACE
    #include <stacktrace>
#endif

//---------------------------------------------------------------------------
// Attribute shims. Error/throw paths are marked cold + non-inline so the
// optimiser keeps them out of the hot path (and out of the inlined body of
// otherwise-trivial assignment/arithmetic). Standard [[noreturn]] / [[unlikely]]
// are used directly at the throw sites and branches.
//---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#  define BND_COLD     [[gnu::cold]]
#  define BND_NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
#  define BND_COLD
#  define BND_NOINLINE __declspec(noinline)
#else
#  define BND_COLD
#  define BND_NOINLINE
#endif

//---------------------------------------------------------------------------
// debug — error codes, std::error_category, and diagnostic helpers. `errc`
// enumerates the failure modes; bound_category + make_error_code plug into
// <system_error>. `print_types` is a static_assert debug helper for template
// instantiation issues.
//---------------------------------------------------------------------------
namespace bnd
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
    not_a_value,        // operand is in its sentinel (NaN-like) state
    not_finite,         // non-finite double input (NaN/Inf)
  };

  // Static, allocation-free message per code. The single source of truth for
  // both bound_category::message and the cheap error path; returns a view onto a
  // string literal, so it needs only <string_view>.
  constexpr std::string_view errc_message(errc e) noexcept
  {
    switch (e)
    {
      case errc::domain_error:     return "value outside interval";
      case errc::division_by_zero: return "division by zero";
      case errc::overflow:         return "rational arithmetic overflow";
      case errc::rounding_error:   return "notch incompatibility";
      case errc::not_a_value:      return "not a value (sentinel state)";
      case errc::not_finite:       return "non-finite floating-point value";
    }
    return "unknown bound error";
  }

  struct bound_category : std::error_category
  {
    const char* name() const noexcept override { return "bound"; }
    std::string message(int ev) const noexcept override
    { return std::string(errc_message(static_cast<errc>(ev))); }
  };

  inline const bound_category& bound_error_category()
  {
    static const bound_category cat;
    return cat;
  }

  inline std::error_code make_error_code(errc e)
  { return {static_cast<int>(e), bound_error_category()}; }

  //---------------------------------------------------------------------------
  // Outlined throw helpers. Cold + non-inline so the (rare) throw machinery is
  // emitted once, off the hot path, rather than inlined into every assignment.
  //---------------------------------------------------------------------------
  namespace detail
  {
    [[noreturn]] BND_COLD BND_NOINLINE
    inline void throw_bound_error(errc code)
    { throw std::system_error(make_error_code(code)); }   // static category message

#ifdef BND_RICH_MESSAGES
    [[noreturn]] BND_COLD BND_NOINLINE
    inline void throw_bound_error(errc code, std::string what)
    {
#  ifdef BOUND_HAS_STACKTRACE
      what += ": \n" + std::to_string(std::stacktrace::current());
#  endif
      throw std::system_error(make_error_code(code), std::move(what));
    }
#endif
  } // namespace detail

  //---------------------------------------------------------------------------
  // diagnostics
  //---------------------------------------------------------------------------
  template <typename... Ts>
  struct print_types
  {
      static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
  };
} // namespace bnd

namespace std
{
  template<> struct is_error_code_enum<bnd::errc> : true_type {};
}

#endif // BNDdebugHPP
