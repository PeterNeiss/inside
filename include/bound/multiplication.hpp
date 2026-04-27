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
    else if constexpr (abs_den(Notch<L>.Denominator) == 1 && abs_den(Lower<L>.Denominator) == 1
                     && abs_den(Notch<R>.Denominator) == 1 && abs_den(Lower<R>.Denominator) == 1
                     && abs_den(Notch<result>.Denominator) == 1 && abs_den(Lower<result>.Denominator) == 1)
    {
      result res;
      from_value(res, to_value(lhs) * to_value(rhs));
      return res;
    }
    else
    {
      auto to_result = [](auto raw)
      { result res; res.Raw = raw_cast<result>(raw); return res; };

      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        return to_result
        (lhs.Raw * rhs.Raw + lhs.Raw * LowerIndex<R> + rhs.Raw * LowerIndex<L>);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        raw_t<L> negRaw = raw_cast<L>(NotchCount<L> - lhs.Raw);
        return to_result
        (negRaw * LowerIndex<R> + rhs.Raw * UpperIndex<L> - (negRaw * rhs.Raw));
      }

      if constexpr (Lower<result> == (Lower<L> * Upper<R>).value())
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      static_assert(Lower<result> == (Lower<L> * Lower<R>).value()
                 || Lower<result> == (Upper<L> * Upper<R>).value()
                 || Lower<result> == (Upper<L> * Lower<R>).value()
                 || Lower<result> == (Lower<L> * Upper<R>).value(),
                 "multiplication: internal logic error");
    }
  }
} // namespace bnd

#endif // BNDmultiplicationHPP
