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

  namespace 
  {
  // older gcc workaround (gcc 12.2)
  [[noreturn]] void throw_overflow_rt(const char* what)
  {
      throw std::overflow_error{what};
  }
  } 

  constexpr void OVERFLOW_trap(const char* what)
  {
    if consteval
    { throw_overflow_rt(what); } // not constexpr → compile error
    else
    { throw_overflow_rt(what); } // runtime throw
  }
/*
  constexpr void OVERFLOW_trap(char const* what = "overflow") 
  { 
    if not conste
    throw std::overflow_error{what}; 
  }
*/
} // namespace bnd

#endif // BNDdebugHPP
