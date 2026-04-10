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
  template<grid G>
  struct bound
  {
    static_assert(grid::validate<G>());

    using negative = bound<-G>;
    using raw_type = slim::optional<storage_min<G>>;
    raw_type Raw;

    constexpr bound() = default; // trivial constructor

    //TODO remove diag_location
    template <numeric A>
    constexpr bound(A value, diag_location loc = diag_location::current())
    { assignment<bound, A>::assign(*this, value, make_policy(loc)); }

    template <numeric A, typename P>
    constexpr bound(A value, P&& policy)
    { assignment<bound, A>::assign(*this, value, policy); }

    template <numeric B>
    constexpr bound& operator=(B const& other)
    { return assignment<bound, B>::assign(*this, other, make_policy(diag_location::current())); }

    constexpr explicit operator double() const { return G.raw_to_double(Raw); }

    constexpr explicit operator rational() const
    { return (std::is_same_v<raw_type, rational>) ? Raw.value() : (Raw * G.Notch + G.Interval.Lower); }

    constexpr negative operator-() const
    {
      negative neg;
      if (Raw.has_value())
      {
        if constexpr (is_raw_rational<bound>)
          neg.Raw = -(*Raw);
        else
          neg.Raw = raw_cast<negative>(MaxNotch<bound> - *Raw);
      }
      else
        neg.Raw = slim::nullopt;

      return neg;
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

    auto with_round() { return policy<ignore_round>(); }
    //auto without_clamp() { return this->policy<no_clamp>(); }
    //auto without_wrap() { return this->policy<no_wrap>(); }

    private:
      static void check_trival() { static_assert(std::is_trivial_v<bound>);}
  };

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>>
  constexpr auto add(L const& lhs, R const& rhs, P&& policy = {})
  { return addition<L,R>::add(lhs, rhs, std::forward<P>(policy)); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  constexpr auto operator+(boundable auto lhs, boundable auto rhs)
  { return add(lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>>
  constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {})
  { return add(lhs, -rhs, std::forward<P>(policy)); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  constexpr auto operator-(boundable auto lhs, boundable auto rhs)
  { return sub(lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>>
  constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {})
  { return multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy)); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  constexpr auto operator*(boundable auto lhs, boundable auto rhs)
  { return bnd::mul(lhs, rhs); }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>>
  constexpr auto div(L lhs, R rhs, P&& policy = {})
  { return division<L,R>::div(lhs, rhs, std::forward<P>(policy)); }

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
