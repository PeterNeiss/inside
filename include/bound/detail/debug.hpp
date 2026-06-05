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
// debug — error codes, `std::error_category`, and a few diagnostic helpers.
//
// `errc` enumerates the four failure modes (domain, divzero, overflow,
// rounding). `bound_category` plus `make_error_code(errc)` plug into
// `<system_error>` so `bound`'s exceptions and the `error_code&` API both
// report uniformly. `overflow_trap` is the runtime sibling of the compile-time
// overflow `throw` in rational.hpp. `print_types` / `print_values` are static-assert
// based debug helpers — handy when chasing template instantiation issues.
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

  template <auto...N>
  struct print_values;

  constexpr void overflow_trap(const char* what)
  {
    if (what)
      throw std::system_error(make_error_code(errc::overflow), what);
  }
} // namespace bnd

namespace std
{
  template<> struct is_error_code_enum<bnd::errc> : true_type {};
}

#endif // BNDdebugHPP
