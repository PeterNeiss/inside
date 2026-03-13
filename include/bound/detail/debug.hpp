//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdebugHPP
#define BNDdebugHPP

namespace bnd
{
  template <typename... Ts>
  struct print_types {
      static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
  };
} // namespace bnd

#endif // BNDdebugHPP
