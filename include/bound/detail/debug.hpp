//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdebugHPP
#define BNDdebugHPP

#include <system_error>
#include <string>
#include <string_view>

#ifdef BOUND_HAS_STACKTRACE
    #include <stacktrace>
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

  struct bound_category : std::error_category
  {
    const char* name() const noexcept override { return "bound"; }
    std::string message(int ev) const noexcept override
    {
      switch (static_cast<errc>(ev))
      {
        case errc::domain_error:     return "value outside interval";
        case errc::division_by_zero: return "division by zero";
        case errc::overflow:         return "rational arithmetic overflow";
        case errc::rounding_error:   return "notch incompatibility";
        case errc::not_a_value:      return "not a value (sentinel state)";
        case errc::not_finite:       return "non-finite floating-point value";
        default:                     return "unknown bound error";
      }
    }
  };

  inline const bound_category& bound_error_category()
  {
    static const bound_category cat;
    return cat;
  }

  inline std::error_code make_error_code(errc e)
  { return {static_cast<int>(e), bound_error_category()}; }

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
