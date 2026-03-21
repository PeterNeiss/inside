//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/common.hpp"
#include "bound/waiver.hpp"
#include "bound/construction.hpp"
#include "bound/addition.hpp"
#include "bound/multiplication.hpp"

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
    static_assert(0 == Notch || (Lower/Notch).Denominator == 1,
      "Zero must coincide with (extended) grid. No displacement allowed");
    static constexpr grid Grid{Interval, Notch};

    using raw_type = smallest_uint_for<Grid.max_notch()>; 
    static_assert(std::unsigned_integral<raw_type> || std::is_same_v<raw_type, rational>);
    // check logically (Notch == 0) equivalent (raw_type == rational)
    static_assert(0 != Notch || std::is_same_v<raw_type, rational>);
    static_assert(not std::is_same_v<raw_type, rational> || 0 == Notch);
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
    constexpr rational to_rational() const 
    {
      if constexpr (std::is_same_v<raw_type, rational>)
        return Raw;
      else
        return Raw * Grid.Notch + Interval.Lower; 
    }

    constexpr auto operator-() const
    {
      using neg = bound<-Upper, -Lower, Notch>;
      if constexpr (std::is_same_v<raw_type, rational>)
        return neg::from_raw(-Raw);
      else
        return neg::from_raw
        (static_cast<neg::raw_type>(Grid.max_notch() - Raw));
    }

    private:
      static void check_trival() { static_assert(std::is_trivial_v<bound>);}
  };

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, waiver_flag F = none>
  constexpr auto add(L const& lhs, R const& rhs, waiver_type<F> waiver = {})
  { return addition<L,R>::add(lhs, rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  constexpr auto operator+(boundable auto lhs, boundable auto rhs) 
  { return add(lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, waiver_flag F = none>
  constexpr auto sub(L const& lhs, R const& rhs, waiver_type<F> waiver = {})
  { return add(lhs, -rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  constexpr auto operator-(boundable auto lhs, boundable auto rhs) 
  { return sub(lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, waiver_flag F = none>
  constexpr auto mul(L const& lhs, R const& rhs, waiver_type<F> waiver = {})
  { return multiplication<L,R>::mul(lhs, rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  constexpr auto operator*(boundable auto lhs, boundable auto rhs) 
  { return mul(lhs, rhs); }

} // namespace bnd

#endif // BNDboundHPP
