//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmultiplicationHPP
#define BNDmultiplicationHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct multiplication
  {
    using result = bound<get_grid(L{}) * get_grid(R{})>;

    static constexpr result to_result(auto raw)
    { result res; res.Raw = static_cast<raw_t<result>>(raw); return res; }

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
    if constexpr (get_notch(result{}).Sign == sign::zero)
      return to_result(lhs.to_rational() * rhs.to_rational());
    else
    {
      if constexpr (get_lower(result{}) == get_lower(L{}) * get_lower(R{}))
      {
        // low_per_notch is always positive in this case
        return to_result
        (
          lhs.Raw * rhs.Raw +
          lhs.Raw * get_grid(R{}).low_per_notch().Numerator +
          rhs.Raw * get_grid(L{}).low_per_notch().Numerator
        );
      }

      if constexpr (get_lower(result{}) == get_upper(L{}) * get_upper(R{}))
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (get_lower(result{}) == get_upper(L{}) * get_lower(R{}))
      {
        raw_t<L> negRaw = static_cast<raw_t<L>>(L::Grid.max_notch()) - lhs.Raw;
        return to_result
        (
          (negRaw * R::Grid.low_per_notch().Numerator + rhs.Raw * L::Grid.up_per_notch().Numerator) -
          (negRaw * rhs.Raw)
        );
      }

      if constexpr (get_lower(result{}) == get_lower(L{}) * get_upper(R{}))
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      throw "multiplication: internal logic error";
    }
  }
} // namespace bnd

#endif // BNDmultiplicationHPP
