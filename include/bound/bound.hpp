//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/common.hpp"

namespace bnd
{
  template
  <
    rational Lower,
    rational Upper,
    rational Notch
  >
  struct bound
  {
    static_assert(Lower <= Upper);
    static_assert(0 <= Notch); 
    static_assert(divides_evenly((Upper - Lower), Notch));

    static constexpr interval Interval{Lower, Upper};
    static constexpr grid Grid{Interval, Notch};

    using raw_type = smallest_uint_for<Grid.max_notch()>; 
    raw_type Raw;
  };
/*
  template <boundable B, typename Ret = promotion_policy<B>::add_return_type>
  auto unsafe_add(B const& lhs, B const& rhs) -> Ret
  {
    using raw_type = bigger<Ret::raw_type, B::raw_type>;
    // this can overflow on addition and underflow on subtraction, without detection
    raw_type raw = B::unsafe_remove_zero<raw_type>(static_cast<raw_type>(lhs.raw) + static_cast<raw_type>(rhs.raw)) 
    return {from_raw{}, raw};
  }

  template <typename L, typename R, typename Ret = promotion_policy<R,L>::add_return_type>
  auto unsafe_add(L const& lhs, R const& rhs) -> Ret
  {
        using other_step_ratio = typename epn<OTrait>::step_ratio;

        // We have to convert OStep into our Steps
        using step_conversion = detail::ratio_div<other_step_ratio, step_ratio>;
        Steps += detail::ratio_multiply<step_conversion>(other.Steps - epn<OTrait>::zero_steps::num);
    return lhs.raw
  }
  
  template 
  <
    typename L, typename R, 
    waiver W = mitigation_policy<L,R>::waiver_addable 
    typename Ret = promotion_policy<L,R>::add_return_type,
  >
  auto add(L const& lhs, R const& rhs) -> Ret
  {
    if constexpr (detection_policy<L,R>::never_addable<Ret>)
    {
      return mitigation_policy<L,R>::never_addable<Ret>(lhs, rhs);
    }
    else if constexpr(detection_policy<L,R>::always_addable<Ret>)
    {
      return raw_add<Ret>(lhs, rhs); 
    }
    else if constexpr(detection_policy<L,R>::maybe_addable)
    {
      if constexpr(detection_policy<L,R>::check_runtime_addable<W>)
      {
        if (detection_policy::runtime_check<Ret>(lhs, rhs))
          return unsafe_add<Ret>(lhs, rhs); 
        else
          return mitigation_policy<L,R>::runtime_addable_check_failed<Ret, W>(lhs, rhs);
      }
      else
        return mitigation_policy<L,R>::runtime_unsafe_add<Ret, W>(lhs, rhs);
    }

    std::unreachable
  }

  template <typename L, typename R>
  auto operator+(L const& lhs, R const& rhs) -> promotion_policy<R,L>::add_return_type 
  {
    return add(lhs, rhs);
  }
*/
  //TODO wrap_bound, sat_bound
  //safe_loop, force_add,
} // namespace bnd

#endif // BNDboundHPP
