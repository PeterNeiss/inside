//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdebugHPP
#define BNDdebugHPP

#include <stdexcept>
#include <cstdint>

#ifndef NDEBUG
    #include <source_location>
    namespace bnd
    {
      using diag_location = std::source_location;
    }
#else
    namespace bnd
    {
      struct diag_location 
      {
        static constexpr diag_location current() noexcept { return {}; }
        constexpr const char* file_name() const noexcept { return ""; }
        constexpr const char* function_name() const noexcept { return ""; }
        constexpr std::uint_least32_t line() const noexcept { return 0; }
        constexpr std::uint_least32_t column() const noexcept { return 0; }
      };
    }
#endif

namespace bnd
{
  template <typename... Ts>
  struct print_types 
  {
      static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
  };

  template <auto...N>
  struct print_values;

  constexpr void OVERFLOW_trap(const char* what)
  {
    if (what) 
      throw std::overflow_error{what};
  }

  constexpr void DIV_ZERO_trap(const char* what)
  {
    if (what) 
      throw std::domain_error{what};
  }
} // namespace bnd

#endif // BNDdebugHPP
