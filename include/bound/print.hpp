//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDprintHPP
#define BNDprintHPP

#include "bound/bound.hpp"
#include "bound/format.hpp"

#include <ostream>

//---------------------------------------------------------------------------
// print — `to_string`, `to_string_debug`, and `operator<<` for boundables
// and `rational`. The debug form also prints the raw value, raw type, and
// grid — useful when inspecting failing tests or storage choices. Separate
// from `format.hpp` so test/benchmark binaries can opt out of `<ostream>`.
//---------------------------------------------------------------------------
namespace bnd
{
  template <boundable B>
  inline std::string to_string(B b)
  { return bnd::to_string(as_rational(b)); }

  template <boundable B>
  inline std::string to_string_debug(B b)
  {
    std::string str;
    str += bnd::to_string(as_rational(b));
    str += " {";
    str += bnd::to_string(+b.raw());
    str += "[" + std::string(type_name<raw_t<B>>());
    str += " Max:" + bnd::to_string(+NotchCount<B>) + "] ";
    str += bnd::to_string(Grid<B>);
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, bnd::detail::rational r)
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
