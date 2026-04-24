//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmultiplicationHPP
#define BNDmultiplicationHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct multiplication
  {
    using result = bound<(Grid<L> * Grid<R>).value()>;

    using return_type = std::conditional_t<is_raw_rational<result>,
                                           slim::optional<result>,
                                           result>;

    static constexpr result to_result(auto raw)
    { result res; res.Raw = raw_cast<result>(raw); return res; }

    template <typename P>
    static constexpr return_type mul(L, R, P&&);
  };

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  template <typename P>
  constexpr auto multiplication<L,R>::mul(L lhs, R rhs, P&& policy) -> return_type
  {
    if constexpr (is_raw_rational<result>)
    {
      auto prod = static_cast<rational>(lhs) * static_cast<rational>(rhs);
      if (!prod) return slim::nullopt;
      result res; res.Raw = raw_cast<result>(*prod); return res;
    }
    else if constexpr (is_direct_storage<L> || is_direct_storage<R> || is_direct_storage<result>)
    {
      // At least one operand or result uses signed direct storage.
      // Compute through actual values for correctness.
      imax lhs_val = static_cast<imax>(static_cast<rational>(lhs));
      imax rhs_val = static_cast<imax>(static_cast<rational>(rhs));
      imax prod = lhs_val * rhs_val;
      result res;
      if constexpr (is_direct_storage<result>)
        res.Raw = raw_cast<result>(prod);
      else
        res.Raw = raw_cast<result>(prod - static_cast<imax>(Lower<result>));
      return res;
    }
    else
    {
      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        return to_result
        (lhs.Raw * rhs.Raw + lhs.Raw * OffsetLower<R> + rhs.Raw * OffsetLower<L>);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        raw_t<L> negRaw = raw_cast<L>(MaxNotch<L> - lhs.Raw);
        return to_result
        (negRaw * OffsetLower<R> + rhs.Raw * OffsetUpper<L> - (negRaw * rhs.Raw));
      }

      if constexpr (Lower<result> == (Lower<L> * Upper<R>).value())
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      throw "multiplication: internal logic error";
    }
  }
} // namespace bnd

#endif // BNDmultiplicationHPP
