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

namespace slim
{
  template <bnd::grid G>
  struct sentinel_traits<bnd::bound<G>>
  {
    protected:
      static constexpr bnd::bound<G> sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::bound<G>& v) noexcept;
  };
} // namespace slim

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
    using raw_type = storage_min<G>;
    raw_type Raw;

    constexpr bound() = default; // trivial constructor

    template <numeric A>
    constexpr bound(A value)
    { assignment<bound, A>::assign(*this, value, make_policy()); }

    template <numeric A, typename P>
    constexpr bound(A value, P&& policy)
    { assignment<bound, A>::assign(*this, value, policy); }

    template <numeric B>
    constexpr bound& operator=(B const& other)
    { return assignment<bound, B>::assign(*this, other, make_policy()); }

    constexpr explicit operator double() const { return G.raw_to_double(Raw); }

    constexpr explicit operator rational() const
    {
      if constexpr (is_raw_rational<bound>)
      {
        if constexpr (G.Interval.Lower == G.Interval.Upper)
          return G.Interval.Lower;
        else
          return Raw;
      }
      else
        return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    constexpr negative operator-() const
    {
      negative neg;
      if constexpr (is_raw_rational<bound>)
        neg.Raw = -(Raw);
      else
        neg.Raw = raw_cast<negative>(MaxNotch<bound> - Raw);
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

    template <boundable R>
    constexpr bound& operator+=(R const& rhs)
    {
      if constexpr (not is_raw_rational<bound> && not is_raw_rational<R>
                    && Notch<bound> == Notch<R> && Lower<R> == 0_r)
      {
        umax new_raw = static_cast<umax>(Raw) + static_cast<umax>(rhs.Raw);
        if (new_raw > static_cast<umax>(MaxNotch<bound>))
        {
          make_policy().domain_error("operator+= result out of range");
          return *this;
        }
        Raw = raw_cast<bound>(new_raw);
        return *this;
      }
      else
      {
        *this = *this + rhs;
        return *this;
      }
    }
    //auto without_clamp() { return this->policy<no_clamp>(); }
    //auto without_wrap() { return this->policy<no_wrap>(); }

    template <numeric A>
    static constexpr slim::optional<bound> try_make(A value)
    {
      std::error_code ec;
      bound result;
      assignment<bound, A>::assign(result, value, make_policy(ec));
      if (ec) return slim::nullopt;
      return result;
    }

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

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, R rhs)
    -> slim::optional<typename addition<L,R>::result>
  { if (!lhs) return slim::nullopt; return add(*lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(L lhs, slim::optional<R> rhs)
    -> slim::optional<typename addition<L,R>::result>
  { if (!rhs) return slim::nullopt; return add(lhs, *rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, slim::optional<R> rhs)
    -> slim::optional<typename addition<L,R>::result>
  { if (!lhs || !rhs) return slim::nullopt; return add(*lhs, *rhs); }

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

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, R rhs)
    -> slim::optional<typename addition<L, typename R::negative>::result>
  { if (!lhs) return slim::nullopt; return sub(*lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(L lhs, slim::optional<R> rhs)
    -> slim::optional<typename addition<L, typename R::negative>::result>
  { if (!rhs) return slim::nullopt; return sub(lhs, *rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, slim::optional<R> rhs)
    -> slim::optional<typename addition<L, typename R::negative>::result>
  { if (!lhs || !rhs) return slim::nullopt; return sub(*lhs, *rhs); }

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

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, R rhs)
    -> slim::optional<typename multiplication<L,R>::result>
  { if (!lhs) return slim::nullopt; return bnd::mul(*lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(L lhs, slim::optional<R> rhs)
    -> slim::optional<typename multiplication<L,R>::result>
  { if (!rhs) return slim::nullopt; return bnd::mul(lhs, *rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, slim::optional<R> rhs)
    -> slim::optional<typename multiplication<L,R>::result>
  { if (!lhs || !rhs) return slim::nullopt; return bnd::mul(*lhs, *rhs); }

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

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, R rhs)
    -> slim::optional<typename division<L,R>::result>
  { if (!lhs) return slim::nullopt; return bnd::div(*lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator/(L lhs, slim::optional<R> rhs)
    -> slim::optional<typename division<L,R>::result>
  { if (!rhs) return slim::nullopt; return bnd::div(lhs, *rhs); }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, slim::optional<R> rhs)
    -> slim::optional<typename division<L,R>::result>
  { if (!lhs || !rhs) return slim::nullopt; return bnd::div(*lhs, *rhs); }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = bound<grid{value}>{value};

} // namespace bnd

namespace slim
{
  template <bnd::grid G>
  constexpr bnd::bound<G> sentinel_traits<bnd::bound<G>>::sentinel() noexcept
  {
    bnd::bound<G> s;
    using raw = typename bnd::bound<G>::raw_type;
    if constexpr (std::is_same_v<raw, bnd::rational>)
      s.Raw = bnd::rational::make_sentinel();
    else
      s.Raw = std::numeric_limits<raw>::max();
    return s;
  }

  template <bnd::grid G>
  constexpr bool sentinel_traits<bnd::bound<G>>::is_sentinel(const bnd::bound<G>& v) noexcept
  {
    using raw = typename bnd::bound<G>::raw_type;
    if constexpr (std::is_same_v<raw, bnd::rational>)
      return v.Raw.Denominator == 0;
    else
      return v.Raw == std::numeric_limits<raw>::max();
  }
} // namespace slim

#endif // BNDboundHPP
