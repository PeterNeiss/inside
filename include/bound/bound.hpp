//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/generic.hpp"
#include "bound/lift.hpp"
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

    constexpr bound() requires ((P & checked) == 0) = default; // trivial when not checked
    constexpr bound() requires ((P & checked) != 0) : Raw{} {} // zero-init under checked

    template <numeric A>
      requires assignable_from<bound, A, P>
    constexpr bound(A value)
    { assignment<bound, A>::assign(*this, value, make_policy<P>()); }

    template <numeric A, typename Pol>
      requires assignable_from<bound, A, P>
    constexpr bound(A value, Pol&& pol)
    { assignment<bound, A>::assign(*this, value, pol); }

    template <numeric B>
      requires assignable_from<bound, B, P>
    constexpr bound& operator=(B const& other)
    { return assignment<bound, B>::assign(*this, other, make_policy<P>()); }

    [[nodiscard]] constexpr auto value() const
    {
      if constexpr (IsRawRational<bound>)
        return Raw;
      else if constexpr (IsDirectStorage<bound>)
        return static_cast<std::common_type_t<raw_type, int>>(Raw);
      else
        return static_cast<rational>(*this);
    }

    constexpr operator imax() const
      requires (abs_den(G.Notch.Denominator) == 1 && G.Notch.Numerator != 0)
    { return to_value(*this); }

    constexpr explicit operator double() const { return G.raw_to_double(Raw); }

    constexpr operator rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (IsDirectStorage<bound>)
        return Raw;

      return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    [[nodiscard]] constexpr negative operator-() const
    {
      negative neg;
      if constexpr (IsRawRational<bound>)
        neg.Raw = -(Raw);
      else if constexpr (IsDirectStorage<bound> || IsDirectStorage<negative>)
        from_value(neg, -to_value(*this));
      else
        neg.Raw = raw_cast<negative>(NotchCount<bound> - Raw);
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

    template <typename A>
    auto on_wrap(A&& action)
    {
       using tag = on_wrap_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    auto on_clamp(A&& action)
    {
       using tag = on_clamp_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    auto on_error(A&& action)
    {
       using tag = on_error_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    auto on_sentinel(A&& action)
    {
       using tag = on_sentinel_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    auto on_overflow(A&& action)
    {
       using tag = on_overflow_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    // Multi-action entry point: combine N tagged actions into one policy_ref.
    // The merged implied flags drive the policy; conflict diagnostics in
    // policy_ref reject mutually exclusive combinations (e.g. on_clamp + on_wrap)
    // at compile time. Use case: `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs`
    // — the imax-overflow probe fires λ1, the post-probe narrowing fires λ2.
    template <typename... Actions>
    auto with(Actions&&... actions)
    {
       constexpr policy_flag merged = merged_implied_flags<Actions...>;
       auto pol = make_policy<P | merged>();
       return policy_ref<bound, decltype(pol), std::remove_cvref_t<Actions>...>{
         *this, pol,
         std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

    template <boundable R>
    constexpr bound& operator+=(R const& rhs)
    {
      if constexpr (not IsRawRational<bound> && not IsRawRational<R>
                    && Notch<bound> == Notch<R>
                    && IsDirectStorage<R>)
      {
        if constexpr (P & (clamp | wrap | checked | sentinel))
        {
          imax new_raw = static_cast<imax>(Raw) + static_cast<imax>(rhs.Raw);
          if (new_raw < RawLo<bound> || new_raw > RawHi<bound>)
          {
            if constexpr (P & clamp)
            {
              Raw = raw_cast<bound>(new_raw < RawLo<bound> ? RawLo<bound> : RawHi<bound>);
              return *this;
            }
            else if constexpr (P & wrap)
            {
              constexpr imax range = RawHi<bound> - RawLo<bound> + 1;
              new_raw = ((new_raw - RawLo<bound>) % range + range) % range + RawLo<bound>;
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
              make_policy<P>().report(errc::domain_error, "operator+= result out of range");
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
      if constexpr (std::integral<A>)
      {
        imax l = to_value(*this), r = static_cast<imax>(rhs), result;
        if constexpr (P & checked)
        {
          if (add_overflow(l, r, &result))
          {
            make_policy<P>().report(errc::overflow, "operator+= overflow");
            return *this;
          }
        }
        else
          result = static_cast<imax>(static_cast<umax>(l) + static_cast<umax>(r));
        return assignment<bound, imax>::assign(*this, result, make_policy<P>());
      }
      else
        return assignment<bound, imax>::assign(*this,
          to_value(*this) + static_cast<imax>(rhs), make_policy<P>());
    }

    template <boundable R>
    constexpr bound& operator-=(R const& rhs)
    { return *this += (-rhs); }

    template <arithmetic A>
    constexpr bound& operator-=(A rhs)
    {
      if constexpr (std::integral<A>)
      {
        imax l = to_value(*this), r = static_cast<imax>(rhs), result;
        if constexpr (P & checked)
        {
          if (sub_overflow(l, r, &result))
          {
            make_policy<P>().report(errc::overflow, "operator-= overflow");
            return *this;
          }
        }
        else
          result = static_cast<imax>(static_cast<umax>(l) - static_cast<umax>(r));
        return assignment<bound, imax>::assign(*this, result, make_policy<P>());
      }
      else
        return *this += (-rhs);
    }

    template <boundable R>
    constexpr bound& operator*=(R const& rhs)
    { return assign_op_result(*this * rhs); }

    template <boundable R>
    constexpr bound& operator/=(R const& rhs)
    { return assign_op_result(*this / rhs); }

    template <boundable R>
    constexpr bound& operator%=(R const& rhs)
    { return assign_op_result(mod(*this, rhs, make_policy<P>())); }

    private:
    template <typename Result>
    constexpr bound& assign_op_result(Result const& r)
    {
      if constexpr (requires { typename Result::value_type; })
        *this = r.value();
      else
        *this = r;
      return *this;
    }
    public:

    template <arithmetic A>
    constexpr bound& operator*=(A rhs)
    {
      if constexpr (std::integral<A>)
      {
        imax l = to_value(*this), r = static_cast<imax>(rhs), result;
        if constexpr (P & checked)
        {
          if (mul_overflow(l, r, &result))
          {
            make_policy<P>().report(errc::overflow, "operator*= overflow");
            return *this;
          }
        }
        else
          result = static_cast<imax>(static_cast<umax>(l) * static_cast<umax>(r));
        return assignment<bound, imax>::assign(*this, result, make_policy<P>());
      }
      else
        return assignment<bound, imax>::assign(*this,
          to_value(*this) * static_cast<imax>(rhs), make_policy<P>());
    }

    template <arithmetic A>
    constexpr bound& operator/=(A rhs)
    {
      if (rhs == 0)
      {
        if constexpr (!(P & ignore_zero))
          make_policy<P>().report(errc::division_by_zero, "operator/= division by zero");
        return *this;
      }
      return assignment<bound, imax>::assign(*this,
        to_value(*this) / static_cast<imax>(rhs), make_policy<P>());
    }

    template <arithmetic A>
    constexpr bound& operator%=(A rhs)
    {
      if (rhs == 0)
      {
        if constexpr (!(P & ignore_zero))
          make_policy<P>().report(errc::division_by_zero, "operator%= division by zero");
        return *this;
      }
      return assignment<bound, imax>::assign(*this,
        to_value(*this) % static_cast<imax>(rhs), make_policy<P>());
    }

    constexpr bound& operator++()    { return *this += 1; }
    constexpr bound  operator++(int) { bound t = *this; ++*this; return t; }
    constexpr bound& operator--()    { return *this -= 1; }
    constexpr bound  operator--(int) { bound t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr slim::optional<bound> try_make(A value)
    {
      std::error_code ec;
      bound result;
      assignment<bound, A>::assign(result, value, make_policy<P>(ec));
      if (ec) return slim::nullopt;
      return result;
    }
  };

  //---------------------------------------------------------------------------
  // comparison
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  constexpr auto operator<=>(L const& lhs, R const& rhs)
  {
    // same grid: Raw is monotonically ordered regardless of storage kind
    if constexpr (Grid<L> == Grid<R>)
      return lhs.Raw <=> rhs.Raw;
    // both integer-direct (notch=1, Raw==value): compare as integers
    else if constexpr (!IsRawRational<L> && !IsRawRational<R>
                       && IsDirectStorage<L> && IsDirectStorage<R>)
      return static_cast<imax>(lhs.Raw) <=> static_cast<imax>(rhs.Raw);
    else
      return static_cast<rational>(lhs) <=> static_cast<rational>(rhs);
  }

  template <boundable L, boundable R>
  constexpr bool operator==(L const& lhs, R const& rhs)
  {
    if constexpr (Grid<L> == Grid<R>)
      return lhs.Raw == rhs.Raw;
    else if constexpr (!IsRawRational<L> && !IsRawRational<R>
                       && IsDirectStorage<L> && IsDirectStorage<R>)
      return static_cast<imax>(lhs.Raw) == static_cast<imax>(rhs.Raw);
    else
      return static_cast<rational>(lhs) == static_cast<rational>(rhs);
  }

  template <boundable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs)
  {
    if constexpr (!IsRawRational<B> && IsDirectStorage<B>)
      return static_cast<imax>(lhs.Raw) <=> static_cast<imax>(rhs);
    else
      return static_cast<rational>(lhs) <=> rational{rhs};
  }

  template <boundable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs)
  {
    if constexpr (!IsRawRational<B> && IsDirectStorage<B>)
      return static_cast<imax>(lhs.Raw) == static_cast<imax>(rhs);
    else
      return static_cast<rational>(lhs) == rational{rhs};
  }

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>, typename A = no_action>
    requires is_policy_v<plain<P>>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return addition<L,R>::add(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  // Action-first form: 1+ tagged actions, at least one of which is on_overflow
  // (the only kind arithmetic itself fires; others are kept for forward-compat).
  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, Actions&&... actions)
  { return addition<L,R>::add(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator+(boundable auto lhs, boundable auto rhs)
  { return add(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l + r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l + r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator+(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l + r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>, typename A = no_action>
    requires is_policy_v<plain<P>>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return add(lhs, -rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, Actions&&... actions)
  { return add(lhs, -rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator-(boundable auto lhs, boundable auto rhs)
  { return sub(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l - r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l - r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator-(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l - r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, typename P = policy<>, typename A = no_action>
    requires is_policy_v<plain<P>>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, Actions&&... actions)
  { return multiplication<L,R>::mul(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator*(boundable auto lhs, boundable auto rhs)
  { return bnd::mul(lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l * r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l * r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator*(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l * r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return division<L, R, F>::div(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, Actions&&... actions)
  { return division<L, R, merged_implied_flags<Actions...>>::div(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator/(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::div(lhs, rhs, make_policy<F>());
  }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, R rhs)
  { return lift([](auto l, auto r){ return l / r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator/(L lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l / r; }, lhs, rhs); }

  template <boundable L, boundable R>
  constexpr auto operator/(slim::optional<L> lhs, slim::optional<R> rhs)
  { return lift([](auto l, auto r){ return l / r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mod
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return modulo<L, R, F>::mod(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, Actions&&... actions)
  { return modulo<L, R, merged_implied_flags<Actions...>>::mod(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  //---------------------------------------------------------------------------
  // operator%
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator%(boundable auto lhs, boundable auto rhs)
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
  //
  // Iteration uses bound::operator++ (which adds 1 to the *value*), so the grid
  // must have notch 1 and integer-valued lower bound. That keeps every step on
  // the grid and makes `count_ = upper - lower + 1` exact.
  //---------------------------------------------------------------------------
  template <grid G, policy_flag P = checked>
    requires (G.Notch == 1_r && abs_den(G.Interval.Lower.Denominator) == 1)
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
    s.Raw = bnd::sentinel_raw<bnd::bound<G, P>>();
    return s;
  }

  template <bnd::grid G, bnd::policy_flag P>
  constexpr bool sentinel_traits<bnd::bound<G, P>>::is_sentinel(const bnd::bound<G, P>& v) noexcept
  {
    using raw = typename bnd::bound<G, P>::raw_type;
    // Rational uses a broader check (any zero denominator) than equality
    // against the canonical {1, 0} sentinel.
    if constexpr (std::is_same_v<raw, bnd::rational>)
      return v.Raw.Denominator == 0;
    else
      return v.Raw == bnd::sentinel_raw<bnd::bound<G, P>>();
  }
} // namespace slim

#endif // BNDboundHPP
