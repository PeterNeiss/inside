//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDformatterHPP
#define BNDformatterHPP

#include "bound/format.hpp"
#include "bound/print.hpp"

#include <format>

template <bnd::grid G, bnd::policy_flag P>
struct std::formatter<bnd::bound<G, P>> : std::formatter<std::string>
{
  template <typename Ctx>
  auto format(bnd::bound<G, P> const& b, Ctx& ctx) const
  { return std::formatter<std::string>::format(bnd::to_string(b), ctx); }
};

template <>
struct std::formatter<bnd::rational> : std::formatter<std::string>
{
  template <typename Ctx>
  auto format(bnd::rational const& r, Ctx& ctx) const
  { return std::formatter<std::string>::format(bnd::to_string(r), ctx); }
};

#endif // BNDformatterHPP
