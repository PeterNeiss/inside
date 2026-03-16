//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/common.hpp"
#include "bound/policy/waiver.hpp"
#include "bound/policy/mitigation.hpp"
#include "bound/policy/promotion.hpp"
#include "bound/policy/detection.hpp"

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
    static constexpr interval Interval{Lower, Upper};

    static_assert(0 <= Notch); 
    static_assert(Interval.divides_evenly(Notch));
    static constexpr grid Grid{Interval, Notch};

    using raw_type = smallest_uint_for<Grid.max_notch()>; 
    static_assert(std::unsigned_integral<raw_type>);
    raw_type Raw;

    bound() = default; // trivial constructor
   ~bound() = default; // trivial destructor

    bound(bound const& other) noexcept :Raw{other.Raw} { }

    template <waiver_flag F = none>
    bound(arithmetic auto value, waiver_type<F> waiver = {})
     :Raw{detection<bound>::construct(value, waiver)} { }

    private:
      static void check_trival() { static_assert(std::is_trivial_v<bound>);}
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

*/

  template 
  <
    boundable L,
    boundable R, 
    waiver_type W = mitigation<L,R>::default_waiver_add,
    boundable Ret = promotion<L,R>::add_return_type
  >
  auto add(L const& lhs, R const& rhs) -> Ret
  {
    if constexpr (detection<L,R>::template never_addable<Ret>)
    {
      return mitigation<L,R>::template never_addable<Ret>(lhs, rhs);
    }
    else if constexpr(detection<L,R>::template always_addable<Ret>)
    {
      return raw_add<Ret>(lhs, rhs); 
    }
    else if constexpr(detection<L,R>::template maybe_addable<Ret>)
    {
      if constexpr(detection<L,R>::template check_runtime_addable<W>)
      {
        if (detection<L,R>::template runtime_check<Ret>(lhs, rhs))
          return unsafe_add<Ret>(lhs, rhs); 
        else
          return mitigation<L,R>::template runtime_addable_check_failed<Ret, W>(lhs, rhs);
      }
      else
        return mitigation<L,R>::template runtime_unsafe_add<Ret, W>(lhs, rhs);
    }

    std::unreachable();
  }

  template <typename L, typename R>
  auto operator+(L const& lhs, R const& rhs) -> promotion<R,L>::add_return_type 
  {
    return add(lhs, rhs);
  }
  //TODO wrap_bound, sat_bound
  //safe_loop, force_add,
  //
} // namespace bnd

#endif // BNDboundHPP
