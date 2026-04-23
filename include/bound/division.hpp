//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct division
  {
    static constexpr interval div_interval()
    {
      constexpr auto d = Interval<L> / Interval<R>;
      if constexpr (d.has_value())
        return *d;
      else
      {
        // Divisor interval includes zero — exclude zero for type computation.
        // The runtime div() already returns nullopt for actual division by zero.
        static_assert(!(Interval<R>.Lower == 0_r && Interval<R>.Upper == 0_r),
            "divisor type holds only zero; division is always undefined");

        constexpr auto rhs_ival = Interval<R>;
        constexpr rational step = (Notch<R> != 0_r) ? abs(Notch<R>) : 1_r;
        constexpr bool has_pos = 0_r < rhs_ival.Upper;
        constexpr bool has_neg = 0_r > rhs_ival.Lower;

        if constexpr (has_pos && has_neg)
        {
          constexpr auto pos = Interval<L> / interval{step, rhs_ival.Upper};
          constexpr auto neg = Interval<L> / interval{rhs_ival.Lower, -step};
          static_assert(pos.has_value() && neg.has_value());
          return interval{
            std::min(neg.value().Lower, pos.value().Lower),
            std::max(neg.value().Upper, pos.value().Upper)
          };
        }
        else if constexpr (has_pos)
        {
          constexpr auto r = Interval<L> / interval{step, rhs_ival.Upper};
          static_assert(r.has_value());
          return r.value();
        }
        else
        {
          constexpr auto r = Interval<L> / interval{rhs_ival.Lower, -step};
          static_assert(r.has_value());
          return r.value();
        }
      }
    }

    using result = bound<{div_interval(), 0}>;
    static_assert(std::is_same_v<typename result::raw_type, rational>);

    template <policy_flag F = none>
    static constexpr slim::optional<result> div(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto division<L,R>::div(L lhs, R rhs, policy<F>) -> slim::optional<result>
  {
    auto q = static_cast<rational>(lhs) / static_cast<rational>(rhs);
    if (!q) return slim::nullopt;
    result res; res.Raw = *q; return res;
  }
} // namespace bnd

#endif // BNDdivisionHPP
