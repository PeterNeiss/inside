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
    static_assert(!(P & sentinel) || !(P & clamp), "sentinel and clamp are mutually exclusive");
    static_assert(!(P & sentinel) || !(P & wrap), "sentinel and wrap are mutually exclusive");

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
      else
        return static_cast<rational>(*this);
    }

    constexpr operator imax() const
      requires (abs_den(G.Notch.Denominator) == 1 && G.Notch.Numerator != 0)
    { return to_value(*this); }

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
      else
        return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    constexpr negative operator-() const
    {
      negative neg;
      if constexpr (is_raw_rational<bound>)
        neg.Raw = -(Raw);
      else if constexpr (is_direct_storage<bound> || is_direct_storage<negative>)
        from_value(neg, -to_value(*this));
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

    auto with_round()         { return policy<ignore_round>(); }
    auto with_round_nearest() { return policy<round_nearest>(); }
    auto with_clamp()         { return policy<clamp>(); }
    auto with_wrap()          { return policy<wrap>(); }

    template <boundable R>
    constexpr bound& operator+=(R const& rhs)
    {
      if constexpr (not is_raw_rational<bound> && not is_raw_rational<R>
                    && Notch<bound> == Notch<R>
                    && is_direct_storage<R>)
      {
        if constexpr (P & (clamp | wrap | checked | sentinel))
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
            else if constexpr (P & sentinel)
            {
              Raw = sentinel_raw<bound>();
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
      return assignment<bound, imax>::assign(*this,
        to_value(*this) + static_cast<imax>(rhs), make_policy<P>());
    }

    template <boundable R>
    constexpr bound& operator-=(R const& rhs)
    { return *this += (-rhs); }

    template <arithmetic A>
    constexpr bound& operator-=(A rhs)
    { return *this += (-rhs); }

    template <arithmetic A>
    constexpr bound& operator*=(A rhs)
    {
      return assignment<bound, imax>::assign(*this,
        to_value(*this) * static_cast<imax>(rhs), make_policy<P>());
    }

    template <arithmetic A>
    constexpr bound& operator/=(A rhs)
    {
      if (rhs == 0) { make_policy<P>().domain_error("operator/= division by zero"); return *this; }
      return assignment<bound, imax>::assign(*this,
        to_value(*this) / static_cast<imax>(rhs), make_policy<P>());
    }

    template <arithmetic A>
    constexpr bound& operator%=(A rhs)
    {
      if (rhs == 0) { make_policy<P>().domain_error("operator%= division by zero"); return *this; }
      return assignment<bound, imax>::assign(*this,
        to_value(*this) % static_cast<imax>(rhs), make_policy<P>());
    }

    constexpr bound& operator++()    { return *this += 1; }
    constexpr bound  operator++(int) { bound t = *this; ++*this; return t; }
    constexpr bound& operator--()    { return *this -= 1; }
    constexpr bound  operator--(int) { bound t = *this; --*this; return t; }

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
  // comparison
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  constexpr auto operator<=>(L const& lhs, R const& rhs)
  { return static_cast<rational>(lhs) <=> static_cast<rational>(rhs); }

  template <boundable L, boundable R>
  constexpr bool operator==(L const& lhs, R const& rhs)
  { return static_cast<rational>(lhs) == static_cast<rational>(rhs); }

  template <boundable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs)
  { return static_cast<rational>(lhs) <=> rational{rhs}; }

  template <boundable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs)
  { return static_cast<rational>(lhs) == rational{rhs}; }

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>>
  constexpr auto add(L const& lhs, R const& rhs, P&& policy = {})
  { return addition<L,R>::add(lhs, rhs, std::forward<P>(policy)); }

  //---------------------------------------------------------------------------
  // detail::lift — optional propagation for binary operators
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <typename T>
    constexpr auto as_optional(T&& val)
    {
      if constexpr (requires { typename std::remove_cvref_t<T>::value_type; })
        return std::forward<T>(val);
      else
        return slim::optional<std::remove_cvref_t<T>>{std::forward<T>(val)};
    }

    template <boundable L, boundable R, typename Op>
    constexpr auto lift(slim::optional<L> lhs, R rhs, Op op)
    { if (!lhs) return decltype(as_optional(op(*lhs, rhs))){slim::nullopt}; return as_optional(op(*lhs, rhs)); }

    template <boundable L, boundable R, typename Op>
    constexpr auto lift(L lhs, slim::optional<R> rhs, Op op)
    { if (!rhs) return decltype(as_optional(op(lhs, *rhs))){slim::nullopt}; return as_optional(op(lhs, *rhs)); }

    template <boundable L, boundable R, typename Op>
    constexpr auto lift(slim::optional<L> lhs, slim::optional<R> rhs, Op op)
    { if (!lhs || !rhs) return decltype(as_optional(op(*lhs, *rhs))){slim::nullopt}; return as_optional(op(*lhs, *rhs)); }
  }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  constexpr auto operator+(boundable auto lhs, boundable auto rhs)
  { return add(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, R rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l + r; }); }

  template <boundable L, boundable R>
  constexpr auto operator+(L lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l + r; }); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l + r; }); }

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
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l - r; }); }

  template <boundable L, boundable R>
  constexpr auto operator-(L lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l - r; }); }

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l - r; }); }

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
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l * r; }); }

  template <boundable L, boundable R>
  constexpr auto operator*(L lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l * r; }); }

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l * r; }); }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  constexpr auto div(L lhs, R rhs, policy<F> pol = {})
  { return division<L, R, F>::div(lhs, rhs, pol); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  constexpr auto operator/(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::div(lhs, rhs, make_policy<F>());
  }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, R rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l / r; }); }

  template <boundable L, boundable R>
  constexpr auto operator/(L lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l / r; }); }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, slim::optional<R> rhs)
  { return detail::lift(lhs, rhs, [](auto l, auto r){ return l / r; }); }

  //---------------------------------------------------------------------------
  // mod
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  constexpr auto mod(L lhs, R rhs, policy<F> pol = {})
  { return modulo<L, R, F>::mod(lhs, rhs, pol); }

  //---------------------------------------------------------------------------
  // operator%
  //---------------------------------------------------------------------------
  constexpr auto operator%(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::mod(lhs, rhs, make_policy<F>());
  }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = bound<grid{value}>{value};

  //---------------------------------------------------------------------------
  // operator++ / operator-- for slim::optional<bound>
  //---------------------------------------------------------------------------
  template <boundable B>
    requires ((BoundPolicy<B> & sentinel) != 0)
  constexpr slim::optional<B>& operator++(slim::optional<B>& opt)
  {
    if (opt) ++(*opt);
    return opt;
  }

  template <boundable B>
    requires ((BoundPolicy<B> & sentinel) != 0)
  constexpr slim::optional<B> operator++(slim::optional<B>& opt, int)
  { auto t = opt; ++opt; return t; }

  template <boundable B>
    requires ((BoundPolicy<B> & sentinel) != 0)
  constexpr slim::optional<B>& operator--(slim::optional<B>& opt)
  {
    if (opt) --(*opt);
    return opt;
  }

  template <boundable B>
    requires ((BoundPolicy<B> & sentinel) != 0)
  constexpr slim::optional<B> operator--(slim::optional<B>& opt, int)
  { auto t = opt; --opt; return t; }

  //---------------------------------------------------------------------------
  // bound_range — range-based for loop support
  //---------------------------------------------------------------------------
  template <grid G, policy_flag P = none>
  struct bound_range
  {
    using value_type = bound<G, P>;
    using wrap_type = bound<G, (P & ~(clamp | sentinel)) | wrap>;

    struct iterator
    {
      wrap_type pos;
      imax remaining;

      constexpr value_type operator*() const { return value_type(static_cast<imax>(pos)); }
      constexpr iterator& operator++() { --remaining; ++pos; return *this; }
      constexpr bool operator!=(iterator o) const { return remaining != o.remaining; }
    };

    wrap_type start_;
    imax count_;

    constexpr bound_range()
      : start_(static_cast<imax>(Lower<value_type>))
      , count_(static_cast<imax>(Upper<value_type>) - static_cast<imax>(Lower<value_type>) + 1) {}

    constexpr bound_range(value_type start)
      : start_(static_cast<imax>(start))
      , count_(static_cast<imax>(Upper<value_type>) - static_cast<imax>(Lower<value_type>) + 1) {}

    constexpr iterator begin() const { return {start_, count_}; }
    constexpr iterator end() const { return {start_, 0}; }
  };

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
