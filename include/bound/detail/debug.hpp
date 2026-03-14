//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdebugHPP
#define BNDdebugHPP

namespace bnd
{
  template <typename... Ts>
  struct print_types 
  {
      static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
  };

  template <auto...N>
  struct print_values;

} // namespace bnd

#endif // BNDdebugHPP
