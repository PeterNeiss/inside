//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDprintHPP
#define BNDprintHPP

#include "bound/bound.hpp"
#include "bound/format.hpp"

#include <ostream>

namespace bnd
{
  template <boundable B>
  inline std::string to_string(B b)
  { return bnd::to_string(static_cast<rational>(b)); }

  template <boundable B>
  inline std::string to_string_debug(B b)
  {
    std::string str;
    str += bnd::to_string(static_cast<rational>(b));
    str += " {";
    str += bnd::to_string(+b.Raw);
    str += "[" + std::string(uint_type_name<raw_t<B>>());
    str += " Max:" + bnd::to_string(+MaxNotch<B>) + "] ";
    str += bnd::to_string(Grid<B>);
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, rational r)
  {
    stream << bnd::to_string(r);
    return stream;
  }

  template <boundable B>
  inline std::ostream& operator<<(std::ostream& stream, B b)
  {
    stream << bnd::to_string(b);
    return stream;
  }

} // namespace bnd

#endif // BNDprintHPP
