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
  //---------------------------------------------------------------------------
  // bound 
  //---------------------------------------------------------------------------
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
    static_assert(0 == Notch || Interval.divides_evenly(Notch));
    static constexpr grid Grid{Interval, Notch};

    using raw_type = smallest_uint_for<Grid.max_notch()>; 
    static_assert(std::unsigned_integral<raw_type> || std::is_same_v<raw_type, rational>);
    raw_type Raw;

    constexpr bound() = default; // trivial constructor
    constexpr ~bound() = default; // trivial destructor

    constexpr bound(bound const& other) noexcept :Raw{other.Raw} { }

    template <waiver_flag F = detection<bound>::default_construct_flag>
    constexpr bound(arithmetic auto value, waiver_type<F> waiver = {})
     :Raw{detection<bound>::construct(value, waiver)} { }

    template <waiver_flag F = detection<bound>::default_construct_flag>
    constexpr bound(rational value, waiver_type<F> waiver = {})
     :Raw{detection<bound>::construct(value, waiver)} { }

    static bound from_raw(raw_type raw) { bound b; b.Raw = raw; return b; } 

    explicit operator double() const
    { return Grid.raw_to_double(Raw); }

    private:
      static void check_trival() { static_assert(std::is_trivial_v<bound>);}
  };

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template 
  <
    boundable L,
    boundable R, 
    waiver_flag F = detection<L,R>::default_add_flag,
    boundable Ret = promotion<L,R>::add_return
  >
  auto add(L const& lhs, R const& rhs, waiver_type<F> waiver = {}) -> Ret
  {
    return detection<L,R>::template add<Ret>(lhs, rhs, waiver);
  }

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  auto operator+(L const& lhs, R const& rhs) 
    -> promotion<L,R>::add_return 
  {
    return add(lhs, rhs);
  }

  //TODO wrap_bound, sat_bound
  //safe_loop, force_add,
} // namespace bnd

#endif // BNDboundHPP
