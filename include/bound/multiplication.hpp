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

    template <typename P>
    static constexpr bool needs_overflow_check =
        is_raw_rational<result>
        && (((BoundPolicy<L> | BoundPolicy<R>) & checked) || plain<P>::test(checked));

    template <typename P>
    using return_type_for = std::conditional_t<needs_overflow_check<P>,
                                               slim::optional<result>,
                                               result>;

    template <typename P, typename A>
    using mul_return_t = std::conditional_t<is_overflow_action<plain<A>>,
                                            result,
                                            return_type_for<P>>;

    template <typename P, typename A = no_action>
    static constexpr mul_return_t<P, A> mul(L, R, P&&, A&& = {});
  };

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  template <typename P, typename A>
  constexpr auto multiplication<L,R>::mul(L lhs, R rhs, P&& policy, A&& action) -> mul_return_t<P, A>
  {
    if constexpr (is_raw_rational<result>)
    {
      if constexpr (needs_overflow_check<P>)
      {
        auto prod = static_cast<rational>(lhs) * static_cast<rational>(rhs);
        if (!prod)
        {
          if constexpr (is_overflow_action<plain<A>>)
          { result res; action.fn(res, errc::overflow); return res; }
          else
            return slim::nullopt;
        }
        result res; res.Raw = raw_cast<result>(*prod); return res;
      }
      else
      {
        result res;
        res.Raw = raw_cast<result>(rational::mul_unchecked(
            static_cast<rational>(lhs), static_cast<rational>(rhs)));
        return res;
      }
    }
    else if constexpr (is_integer_aligned<L> && is_integer_aligned<R> && is_integer_aligned<result>)
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
