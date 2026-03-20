//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/common.hpp"
#include "bound/waiver.hpp"
#include "bound/construction.hpp"
#include "bound/addition.hpp"

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

    template <waiver_flag F = construction<bound>::default_flag>
    constexpr bound(arithmetic auto value, waiver_type<F> waiver = {})
     :Raw{construction<bound>::from_value(value, waiver)} { }

    template <waiver_flag F = construction<bound>::default_flag>
    constexpr bound(rational value, waiver_type<F> waiver = {})
     :Raw{construction<bound>::from_value(value, waiver)} { }

    constexpr static bound from_raw(raw_type raw) { bound b; b.Raw = raw; return b; } 

    constexpr explicit operator double() const { return Grid.raw_to_double(Raw); }
    constexpr rational to_rational() const { return Raw * Grid.Notch + Interval.Lower; }

    private:
      static void check_trival() { static_assert(std::is_trivial_v<bound>);}
  };

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, waiver_flag F = addition<L,R>::default_flag>
  constexpr auto add(L const& lhs, R const& rhs, waiver_type<F> waiver = {})
  { return addition<L,R>::add(lhs, rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  constexpr auto operator+(boundable auto lhs, boundable auto rhs) 
  { return add(lhs, rhs); }

  //TODO wrap_bound, sat_bound
  //safe_loop, force_add,
} // namespace bnd

#endif // BNDboundHPP
