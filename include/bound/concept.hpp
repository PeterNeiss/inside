//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDconceptHPP
#define BNDconceptHPP

namespace bnd
{
  template<typename T>
  concept arithmetic = std::integral<T> || std::floating_point<T>;
} // namespace bnd

#endif // BNDconceptHPP
