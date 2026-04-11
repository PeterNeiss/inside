//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmultiplicationHPP
#define BNDmultiplicationHPP

#include "bound/common.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct multiplication
  {
    using result = bound<(Grid<L> * Grid<R>).value()>;

    static constexpr result to_result(auto raw)
    { result res; res.Raw = raw_cast<result>(raw); return res; }

    template <typename P>
    static constexpr result mul(L, R, P&&);
  };

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  template <typename P>
  constexpr auto multiplication<L,R>::mul(L lhs, R rhs, P&& policy) -> result
  {
    if (!lhs.Raw.has_value() || !rhs.Raw.has_value())
    {
      result res;
      res.Raw = slim::nullopt;
      return res;
    }

    if constexpr (Notch<result>.Numerator == 0)
      return to_result((lhs.to_rational() * rhs.to_rational()).value());
    else
    {
      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        // low_per_notch is always positive in this case
        return to_result
        (lhs.Raw.value() * rhs.Raw.value() + lhs.Raw.value() * OffsetLower<R> + rhs.Raw.value() * OffsetLower<L>);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        raw_t<L> negRaw = raw_cast<L>(MaxNotch<L> - lhs.Raw.value());
        return to_result
        (*negRaw * OffsetLower<R> + *rhs.Raw * OffsetUpper<L> - (*negRaw * *rhs.Raw));
      }

      if constexpr (Lower<result> == (Lower<L> * Upper<R>).value())
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      throw "multiplication: internal logic error";
    }
  }
} // namespace bnd

#endif // BNDmultiplicationHPP
