//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/generic.hpp"
#include "bound/policy.hpp"
#include "bound/addition.hpp"
#include "bound/multiplication.hpp"
#include "bound/division.hpp"
#include "bound/assignment.hpp"

namespace slim
{
  template <bnd::grid G, bnd::policy_flag P>
  struct sentinel_traits<bnd::bound<G, P>>
  {
    protected:
      static constexpr bnd::bound<G, P> sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::bound<G, P>& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // bound
  //---------------------------------------------------------------------------
  template<grid G, policy_flag P>
  struct bound
  {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");

    using negative = bound<-G, P>;
    using raw_type = storage_min<G>;
    raw_type Raw;

    constexpr bound() = default; // trivial constructor

    template <numeric A>
    constexpr bound(A value)
    { assignment<bound, A>::assign(*this, value, make_policy<P>()); }

    template <numeric A, typename Pol>
    constexpr bound(A value, Pol&& pol)
    { assignment<bound, A>::assign(*this, value, pol); }

    template <numeric B>
    constexpr bound& operator=(B const& other)
    { return assignment<bound, B>::assign(*this, other, make_policy<P>()); }

    constexpr auto value() const
    {
      if constexpr (is_raw_rational<bound>)
        return Raw;
      else if constexpr (is_direct_storage<bound>)
        return static_cast<std::common_type_t<raw_type, int>>(Raw);
      else if constexpr (G.Interval.Lower == 0_r && G.Notch == 1_r)
        return static_cast<std::common_type_t<raw_type, unsigned int>>(Raw);
      else
        return static_cast<rational>(*this);
    }

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
      else if constexpr (is_direct_storage<bound>)
        return rational{Raw};
      else if constexpr (G.Interval.Lower == 0_r && G.Notch == 1_r)
        return rational{Raw};
      else
        return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    constexpr negative operator-() const
    {
      negative neg;
      if constexpr (is_raw_rational<bound>)
        neg.Raw = -(Raw);
      else if constexpr (is_direct_storage<bound> && is_direct_storage<negative>)
        neg.Raw = raw_cast<negative>(-static_cast<imax>(Raw));
      else if constexpr (is_direct_storage<negative>)
      {
        // Source is offset-encoded, result is direct storage
        imax value = static_cast<imax>(static_cast<rational>(*this));
        neg.Raw = raw_cast<negative>(-value);
      }
      else if constexpr (is_direct_storage<bound>)
      {
        // Source is direct storage, result is offset-encoded
        imax neg_val = -static_cast<imax>(Raw);
        rational raw = ((neg_val - Grid<negative>.Interval.Lower)/Grid<negative>.Notch).value();
        neg.Raw = raw_cast<negative>(raw.Numerator / static_cast<umax>(raw.Denominator));
      }
      else
        neg.Raw = raw_cast<negative>(MaxNotch<bound> - Raw);
      return neg;
    }

    template <policy_flag F = none>
    auto policy()
    {
       auto pol = make_policy<P | F>();
       return policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none>
    auto policy(std::error_code& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none, typename A>
    auto policy(A&& action)
    {
       auto pol = make_policy<P | F>();
       return policy_ref<bound, decltype(pol), std::remove_cvref_t<A>>{
         *this, pol, std::forward<A>(action)};
    }

    auto with_round() { return policy<ignore_round>(); }
    auto with_clamp() { return policy<clamp>(); }
    auto with_wrap()  { return policy<wrap>(); }

    template <boundable R>
    constexpr bound& operator+=(R const& rhs)
    {
      if constexpr (not is_raw_rational<bound> && not is_raw_rational<R>
                    && Notch<bound> == Notch<R>
                    && (Lower<R> == 0_r || is_direct_storage<R>))
      {
        if constexpr (P & (clamp | wrap | checked))
        {
          imax new_raw = static_cast<imax>(Raw) + static_cast<imax>(rhs.Raw);
          constexpr imax lo = is_direct_storage<bound>
            ? static_cast<imax>(Lower<bound>) : 0;
          constexpr imax hi = is_direct_storage<bound>
            ? static_cast<imax>(Upper<bound>) : static_cast<imax>(MaxNotch<bound>);
          if (new_raw < lo || new_raw > hi)
          {
            if constexpr (P & clamp)
            {
              Raw = raw_cast<bound>(new_raw < lo ? lo : hi);
              return *this;
            }
            else if constexpr (P & wrap)
            {
              constexpr imax range = hi - lo + 1;
              new_raw = ((new_raw - lo) % range + range) % range + lo;
              Raw = raw_cast<bound>(new_raw);
              return *this;
            }
            else
            {
              make_policy<P>().domain_error("operator+= result out of range");
              return *this;
            }
          }
          Raw = raw_cast<bound>(new_raw);
        }
        else
        {
          Raw += raw_cast<bound>(rhs.Raw);
        }
        return *this;
      }
      else
      {
        *this = *this + rhs;
        return *this;
      }
    }

    template <arithmetic A>
    constexpr bound& operator+=(A rhs)
    {
      if constexpr (is_direct_storage<bound> || (G.Interval.Lower == 0_r && G.Notch == 1_r))
        return assignment<bound, imax>::assign(*this,
          static_cast<imax>(Raw) + static_cast<imax>(rhs), make_policy<P>());
      else
        return assignment<bound, imax>::assign(*this,
          static_cast<imax>(static_cast<rational>(*this)) + static_cast<imax>(rhs),
          make_policy<P>());
    }

    template <numeric A>
    static constexpr slim::optional<bound> try_make(A value)
    {
      std::error_code ec;
      bound result;
      assignment<bound, A>::assign(result, value, make_policy<P>(ec));
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
  template <bnd::grid G, bnd::policy_flag P>
  constexpr bnd::bound<G, P> sentinel_traits<bnd::bound<G, P>>::sentinel() noexcept
  {
    bnd::bound<G, P> s;
    using raw = typename bnd::bound<G, P>::raw_type;
    if constexpr (std::is_same_v<raw, bnd::rational>)
      s.Raw = bnd::rational::make_sentinel();
    else if constexpr (std::signed_integral<raw>)
      s.Raw = std::numeric_limits<raw>::min();
    else
      s.Raw = std::numeric_limits<raw>::max();
    return s;
  }

  template <bnd::grid G, bnd::policy_flag P>
  constexpr bool sentinel_traits<bnd::bound<G, P>>::is_sentinel(const bnd::bound<G, P>& v) noexcept
  {
    using raw = typename bnd::bound<G, P>::raw_type;
    if constexpr (std::is_same_v<raw, bnd::rational>)
      return v.Raw.Denominator == 0;
    else if constexpr (std::signed_integral<raw>)
      return v.Raw == std::numeric_limits<raw>::min();
    else
      return v.Raw == std::numeric_limits<raw>::max();
  }
} // namespace slim

#endif // BNDboundHPP
