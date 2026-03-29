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
//    template <policy_flag H>
//    using flag = bound<G, F | H>
    using raw_type = storage_min<G>; 
    raw_type Raw;

    constexpr bound() = default; // trivial constructor
    constexpr ~bound() = default; // trivial destructor
    constexpr bound(bound const&) = default; 
    constexpr bound(bound&&) = default; 
    constexpr bound& operator=(bound const&) = default;

    template <numeric A>
    constexpr bound(A value, diag_location loc = diag_location::current())
    { assignment<bound, A>::assign(*this, value, make_policy(loc)); }

    template <numeric A, typename P>
    constexpr bound(A value, P&& policy)
    { assignment<bound, A>::assign(*this, value, policy); }

    template <numeric B>
    constexpr bound& operator=(B const& other)
    {
      return assignment<bound, B>::assign
      (*this, other, make_policy(diag_location::current())); 
    }

    // TODO remove
    constexpr static bound from_raw(raw_type raw) { bound b; b.Raw = raw; return b; } 

    constexpr explicit operator double() const { return Grid.raw_to_double(Raw); }
    // TODO rename explict operator rational
    constexpr rational to_rational() const 
    {
      if constexpr (std::is_same_v<raw_type, rational>)
        return Raw;
      else
        return Raw * Grid.Notch + Grid.Interval.Lower; 
    }

    constexpr auto operator-() const
    {
      if constexpr (std::is_same_v<raw_type, rational>)
        return negative::from_raw(-Raw);
      else
        return negative::from_raw
        (static_cast<negative::raw_type>(Grid.max_notch() - Raw));
    }

    template <policy_flag F = none>
    auto policy() 
    { 
       auto policy = make_policy<F>();
       return policy_ref<bound, decltype(policy)>{*this, policy}; 
    }

    template <policy_flag F = none>
    auto policy(std::error_code& ec) 
    { 
       auto policy = make_policy<F>(ec);
       return policy_ref<bound, decltype(policy)>{*this, policy}; 
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

  //---------------------------------------------------------------------------
  // just 
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = bound<grid{value}>{value}; 

} // namespace bnd

#endif // BNDboundHPP
