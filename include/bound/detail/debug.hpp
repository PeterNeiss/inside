//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdebugHPP
#define BNDdebugHPP

#include <stdexcept>

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
} // namespace bnd

#endif // BNDdebugHPP
