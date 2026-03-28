//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/common.hpp"
#include "bound/policy.hpp"
#include "bound/addition.hpp"
#include "bound/multiplication.hpp"
#include "bound/division.hpp"
#include "bound/assignment.hpp"

namespace bnd
{
  //---------------------------------------------------------------------------
  // bound 
  //---------------------------------------------------------------------------
  // error reporting strategy: Constructors throw
  //---------------------------------------------------------------------------
  template<grid G>
  struct bound
  {
    static_assert(G.validate());
    static constexpr grid Grid{G};

    using negative = bound<-G>;
    static constexpr interval Interval{G.Interval};
//    static constexpr rational Lower{Interval.Lower};
//    static constexpr rational Upper{Interval.Upper};

    using raw_type = smallest_uint_for<Grid.max_notch()>; 
    static_assert(std::unsigned_integral<raw_type> || std::is_same_v<raw_type, rational>);
    // check logically (Notch == 0) equivalent (raw_type == rational)
    static_assert(0_r != G.Notch || std::is_same_v<raw_type, rational>);
    static_assert(not std::is_same_v<raw_type, rational> || 0_r == G.Notch);
    raw_type Raw;

    constexpr bound() = default; // trivial constructor
    constexpr ~bound() = default; // trivial destructor

    constexpr bound(bound const& other) noexcept :Raw{other.Raw} { }

    template <arithmetic A>
    constexpr bound(A value, diag_location loc = diag_location::current())
    { assignment<bound, A>::assign(*this, value, make_policy(loc)); }

    template <arithmetic A, typename P>
    constexpr bound(A value, P&& policy)
    { assignment<bound, A>::assign(*this, value, policy); }

    constexpr static bound from_raw(raw_type raw) { bound b; b.Raw = raw; return b; } 

    constexpr explicit operator double() const { return Grid.raw_to_double(Raw); }
    constexpr rational to_rational() const 
    {
      if constexpr (std::is_same_v<raw_type, rational>)
        return Raw;
      else
        return Raw * Grid.Notch + Grid.Interval.Lower; 
    }

//    constexpr bound& operator=(boundable auto const& other)
//    { return assignment::assign(*this, other; }

    //TODO
    //template <typename T>
    // constexpr T to() const
    // constexpr raw_type to_raw() const
    // 

    //TODO
    //template <typename T>
    // static constexpr bound from(T)

    constexpr auto operator-() const
    {
      if constexpr (std::is_same_v<raw_type, rational>)
        return negative::from_raw(-Raw);
      else
        return negative::from_raw
        (static_cast<negative::raw_type>(Grid.max_notch() - Raw));
    }

    private:
      static void check_trival() { static_assert(std::is_trivial_v<bound>);}
  };

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  constexpr auto add(L const& lhs, R const& rhs, policy<F> waiver = {})
  { return addition<L,R>::add(lhs, rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  constexpr auto operator+(boundable auto lhs, boundable auto rhs) 
  { return add(lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  constexpr auto sub(L const& lhs, R const& rhs, policy<F> waiver = {})
  { return add(lhs, -rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  constexpr auto operator-(boundable auto lhs, boundable auto rhs) 
  { return sub(lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  constexpr auto mul(L const& lhs, R const& rhs, policy<F> waiver = {})
  { return multiplication<L,R>::mul(lhs, rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  constexpr auto operator*(boundable auto lhs, boundable auto rhs) 
  { return bnd::mul(lhs, rhs); }

  //---------------------------------------------------------------------------
  // div 
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  constexpr auto div(L lhs, R rhs, policy<F> waiver = {})
  { return division<L,R>::div(lhs, rhs, waiver); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  constexpr auto operator/(boundable auto lhs, boundable auto rhs) 
  { return bnd::div(lhs, rhs); }
} // namespace bnd

#endif // BNDboundHPP
