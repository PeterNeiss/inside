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
#include "bound/predicates.hpp"

//---------------------------------------------------------------------------
// bound — public-facing struct that ties everything together.
//
// This header is the one users include. It defines `bound<G, P>` and its
// per-instance operators (assignment, negation, `value()`, `policy()`,
// `with_clamp/wrap/round/...`, increment, compare). The free-function
// arithmetic (`add`, `sub`, `mul`, `div`, `mod` with policy/action overloads)
// and the `bound_range` iterator helper also live here.
//
// Heavy lifting is delegated: `addition.hpp` / `multiplication.hpp` /
// `division.hpp` carry the per-operator code; `assignment.hpp` does
// narrowing/clamp/wrap/sentinel; `generic.hpp` and `policy.hpp` supply the
// type-level traits and the policy machinery used throughout.
//---------------------------------------------------------------------------
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
      requires bound_assignable<bound, A, P>
    constexpr bound(A value)
    { assignment<bound, A>::assign(*this, value, make_policy<P>()); }

    template <numeric A, typename Pol>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value, Pol&& pol)
    { assignment<bound, A>::assign(*this, value, pol); }

    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value, std::error_code& ec)
    { assignment<bound, A>::assign(*this, value, make_policy<P | checked>(ec)); }

    template <numeric B>
      requires bound_assignable<bound, B, P>
    constexpr bound& operator=(B const& other)
    { return assignment<bound, B>::assign(*this, other, make_policy<P>()); }

    [[nodiscard]] constexpr auto value() const
    {
      if constexpr (IsRawRational<bound>)
        return Raw;
      else if constexpr (IsDirectStorage<bound>)
        return static_cast<std::common_type_t<raw_type, int>>(Raw);
      else
        return as_rational(*this);
    }

    // Public sentinel probe — the canonical "is this slot empty?" check
    // under `sentinel` policy, matching the raw layout used by both the
    // policy machinery and `slim::optional<bound>`.
    [[nodiscard]] constexpr bool is_sentinel() const noexcept
    {
      if constexpr (IsRawRational<bound>)
        return Raw.Denominator == 0;
      else
        return Raw == sentinel_raw<bound>();
    }

    constexpr operator imax() const
      requires (abs_den(G.Notch.Denominator) == 1 && G.Notch.Numerator != 0)
    { return to_value(*this); }

    // Explicit so `bound + double` doesn't silently demote arithmetic to
    // floating-point and lose the exact-rational guarantees. Callers must
    // opt in with `double(b)` when they actually want float output.
    constexpr explicit operator double() const { return G.raw_to_double(Raw); }

    constexpr operator rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (IsDirectStorage<bound>)
        return Raw;

      // Q-format-with-integer-Lower fast path: value = (Raw + Lower*N) / N.
      // Skips the three rational ops (multiply, add, optional-unwrap) of the
      // generic path — useful for any fixed-point bound that goes through
      // the rational route (e.g. exact division when `ignore_round` is off).
      // The signed-integer rational ctor handles negative results.
      // Gated on `RawFitsInImax`: wide unsigned raws (e.g. uint64 from a
      // Q16.16 × Q16.16 result type) would wrap on the signed_raw widening,
      // so those fall through to the rational path below.
      if constexpr (G.Notch.Numerator == 1
                 && abs_den(G.Interval.Lower.Denominator) == 1
                 && RawFitsInImax<bound>)
      {
        constexpr imax N = abs_den(G.Notch.Denominator);
        imax v = signed_raw(*this) + LowerImax<bound> * N;
        return rational{v, N};
      }

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
        // Unsigned-offset fast path: with offset encoding `value = Raw*Notch + Lower`,
        // negating the value is equivalent to indexing from the opposite end of the
        // grid — i.e. `NotchCount - Raw`. No rational arithmetic in the hot path.
        //
        // NOTE: not a sibling of the IsDirectStorage encoding bugs — this branch is
        // unreachable when either operand uses direct storage, so `Raw` here is
        // guaranteed to be an offset.
        neg.Raw = raw_cast<negative>(NotchCount<bound> - Raw);
      return neg;
    }

    template <policy_flag F = none>
    constexpr auto policy()
    {
       auto pol = make_policy<P | F>();
       return policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none>
    constexpr auto policy(std::error_code& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none, typename A>
    constexpr auto policy(A&& action)
    {
       auto pol = make_policy<P | F>();
       return policy_ref<bound, decltype(pol), std::remove_cvref_t<A>>{
         *this, pol, std::forward<A>(action)};
    }

    constexpr auto with_round()           { return policy<ignore_round>(); }
    constexpr auto with_round_nearest()   { return policy<round_nearest>(); }
    constexpr auto with_floor()           { return policy<round_floor>(); }
    constexpr auto with_ceil()            { return policy<round_ceil>(); }
    constexpr auto with_round_half_even() { return policy<round_half_even>(); }
    constexpr auto with_clamp()           { return policy<clamp>(); }
    constexpr auto with_wrap()            { return policy<wrap>(); }

    template <typename A>
    constexpr auto on_wrap(A&& action)
    {
       using tag = on_wrap_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    constexpr auto on_clamp(A&& action)
    {
       using tag = on_clamp_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    constexpr auto on_error(A&& action)
    {
       using tag = on_error_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    constexpr auto on_sentinel(A&& action)
    {
       using tag = on_sentinel_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    constexpr auto on_overflow(A&& action)
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
    constexpr auto with(Actions&&... actions)
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
      // Fast path: raw-level integer addition. Safe when both sides share an
      // encoding where raw_a + raw_b is the raw of value_a + value_b — either
      // direct storage (Raw == value) or offset encoding with Lower==0 on
      // both (Raw == value/notch, so raws sum to (sum)/notch).
      if constexpr (not IsRawRational<bound> && not IsRawRational<R>
                    && Notch<bound> == Notch<R>
                    && (IsDirectStorage<R>
                        || (Lower<bound> == 0_r && Lower<R> == 0_r)))
      {
        if constexpr (P & (clamp | wrap | checked | sentinel))
        {
          imax new_raw = signed_raw(*this) + signed_raw(rhs);
          if (new_raw < RawLo<bound> || new_raw > RawHi<bound>)
          {
            if constexpr (P & clamp)
            {
              // `RawLo`/`RawHi` are raw-space constants by construction
              // (Lower/Upper for direct, 0/NotchCount for notch-offset), so
              // they're already the correct Raw — no `raw_from_offset` needed.
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
      // For `sentinel` policy types, an out-of-range write silently sets
      // result.Raw to the sentinel; converting that bound into the returned
      // optional would otherwise trip validate_not_sentinel.
      if constexpr ((P & sentinel) != 0)
        if (result.Raw == sentinel_raw<bound>()) return slim::nullopt;
      return result;
    }
  };

  //---------------------------------------------------------------------------
  // saturated_cast / checked_cast / unchecked_cast
  //
  // Free-function casts that complement the constructors. Unlike a direct
  // `B{value}` call, these read naturally in algorithm callbacks
  // (`std::transform`, `std::ranges::views::transform`) and make the
  // intent — clamp vs. throw vs. trust — explicit at the call site.
  //---------------------------------------------------------------------------
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B saturated_cast(A value)
  {
    B b{};                  // default-init; immediately overwritten below
    b.with_clamp() = value; // clamp regardless of B's declared policy
    return b;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr B saturated_cast(A value)
  {
    B b{};
    b.with_clamp() = value;
    return b;
  }

  // `wrap_cast` rounds out the named-cast trio with modular semantics.
  // Mirrors `saturated_cast` but uses `with_wrap()` so the input value is
  // reduced into the target grid's interval rather than clipped at the
  // boundaries. Useful where the caller wants integer-style wraparound
  // (sequence numbers, angles, ring-buffer indices) inside an
  // `std::transform` or other algorithm callback.
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B wrap_cast(A value)
  {
    B b{};
    b.with_wrap() = value;
    return b;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr B wrap_cast(A value)
  {
    B b{};
    b.with_wrap() = value;
    return b;
  }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round
  //
  // Compose `with_clamp` and one of the rounding modes — the canonical
  // pipeline for "double in, bounded integer out, never throw" used in
  // audio, graphics, and DSP code. Without these helpers the caller would
  // chain `with_clamp().policy<round_floor>()` etc. by hand.
  //---------------------------------------------------------------------------
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B clamp_floor(A value)
  {
    B b{};
    b.template policy<clamp | round_floor>() = value;
    return b;
  }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B clamp_ceil(A value)
  {
    B b{};
    b.template policy<clamp | round_ceil>() = value;
    return b;
  }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B clamp_round(A value)
  {
    B b{};
    b.template policy<clamp | round_nearest>() = value;
    return b;
  }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (will_conversion_overflow<B>(value))
      throw std::system_error(make_error_code(errc::domain_error),
                              "checked_cast: value out of bound interval");
    if (will_conversion_truncate<B>(value))
      throw std::system_error(make_error_code(errc::rounding_error),
                              "checked_cast: value does not land on notch");
    return B{value};
  }

  // `unchecked_cast` is the strong sibling of constructing via the `unsafe`
  // policy: it routes through `bound<G, unsafe>` so the compiler can elide
  // every domain/round check. UB if the caller's value is actually out of
  // range — same contract as bounded::integer's `assume_in_range`.
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    using twin = bound<Grid<B>, unsafe>;
    twin t{value};
    B out;
    out.Raw = t.Raw;        // same grid → identical raw layout
    return out;
  }

  // Inline rational view of a value or bound. Use where `auto r = x;` would
  // copy the bound and `rational{x}` would not compile because `x` is a bound
  // (the implicit conversion via `operator rational()` does the work). The
  // arithmetic overload exists so the same name covers both call sites.
  template <arithmetic A>
  [[nodiscard]] constexpr rational as_rational(A v) { return rational{v}; }

  template <boundable B>
  [[nodiscard]] constexpr rational as_rational(B b) { return b; }

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
      return signed_raw(lhs) <=> signed_raw(rhs);
    else
      return as_rational(lhs) <=> as_rational(rhs);
  }

  template <boundable L, boundable R>
  constexpr bool operator==(L const& lhs, R const& rhs)
  {
    if constexpr (Grid<L> == Grid<R>)
      return lhs.Raw == rhs.Raw;
    else if constexpr (!IsRawRational<L> && !IsRawRational<R>
                       && IsDirectStorage<L> && IsDirectStorage<R>)
      return signed_raw(lhs) == signed_raw(rhs);
    else
      return as_rational(lhs) == as_rational(rhs);
  }

  template <boundable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs)
  {
    if constexpr (!IsRawRational<B> && IsDirectStorage<B>)
      return signed_raw(lhs) <=> static_cast<imax>(rhs);
    else
      return as_rational(lhs) <=> rational{rhs};
  }

  template <boundable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs)
  {
    if constexpr (!IsRawRational<B> && IsDirectStorage<B>)
      return signed_raw(lhs) == static_cast<imax>(rhs);
    else
      return as_rational(lhs) == rational{rhs};
  }

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_like P = policy<>, typename A = no_action>
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

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return addition<L,R>::add(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

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
  template <boundable L, boundable R, policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return add(lhs, -rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, Actions&&... actions)
  { return add(lhs, -rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return add(lhs, -rhs, make_policy<checked>(ec), std::forward<A>(action)); }

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
  template <boundable L, boundable R, policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && has_action<is_overflow_action_pred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, Actions&&... actions)
  { return multiplication<L,R>::mul(lhs, rhs,
      make_policy<merged_implied_flags<Actions...>>(),
      pick_action<is_overflow_action_pred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs,
                                   std::error_code& ec, A&& action = {})
  { return multiplication<L,R>::mul(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

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
  // add_all / mul_all — variadic folds
  //
  // `a + b + c + d` works today via pairwise `add`, threading four grid
  // widenings. `add_all(a, b, c, d)` is the variadic equivalent — same
  // pairwise widening, but the fold reads cleaner and matches Chromium's
  // `CheckAdd(a, b, c)` idiom.
  //---------------------------------------------------------------------------
  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto add_all(First const& first, Rest const&... rest)
  { return (first + ... + rest); }

  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto mul_all(First const& first, Rest const&... rest)
  { return (first * ... * rest); }

  // `add_all_into<Target>` / `mul_all_into<Target>` — variadic fold that
  // collapses the widening intermediate back into a caller-chosen target
  // grid via `saturated_cast<Target>`. The standard audio-mix / sensor-sum
  // idiom: pairwise widen for exactness, then clip to the bus.
  template <boundable Target, boundable First, boundable... Rest>
  [[nodiscard]] constexpr Target add_all_into(First const& first, Rest const&... rest)
  {
    auto sum = (first + ... + rest);
    if constexpr (requires { typename decltype(sum)::value_type; })
      return saturated_cast<Target>(sum.value());
    else
      return saturated_cast<Target>(sum);
  }

  template <boundable Target, boundable First, boundable... Rest>
  [[nodiscard]] constexpr Target mul_all_into(First const& first, Rest const&... rest)
  {
    auto prod = (first * ... * rest);
    if constexpr (requires { typename decltype(prod)::value_type; })
      return saturated_cast<Target>(prod.value());
    else
      return saturated_cast<Target>(prod);
  }

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

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs,
                                   std::error_code& ec, A&& action = {})
  { return division<L, R, checked>::div(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

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

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs,
                                   std::error_code& ec, A&& action = {})
  { return modulo<L, R, checked>::mod(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

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
  // _b literal — compile-time bound<{N, N}> from an integer literal.
  //   auto five = 5_b;            // bound<{5, 5}>
  //   auto x    = 10_b + my_bnd;  // grid widening via just<N> + bound
  //---------------------------------------------------------------------------
  namespace _detail
  {
    template<char... Chars>
    consteval imax parse_b_literal()
    {
      // The literal operator strips any `'` digit-separators, leaves digits
      // only; sign is unary `-` applied to the result by the parser. We only
      // accept decimal digits — `0x`/`0b` prefixes would need a different
      // parser and aren't needed for grid bounds.
      imax v = 0;
      ((Chars >= '0' && Chars <= '9'
          ? (v = v * 10 + (Chars - '0'), 0)
          : 0), ...);
      return v;
    }
  }

  template<char... Chars>
  constexpr auto operator""_b() { return just<_detail::parse_b_literal<Chars...>()>; }

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
  // Iteration walks the grid by notch index, so any grid with a non-zero
  // notch works (integer or fractional). Each step computes the value
  // `Lower + index * Notch`, which is exact by construction. The iterator
  // wraps modulo the slot count so a mid-range start visits every slot
  // exactly once before terminating.
  //---------------------------------------------------------------------------
  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0_r)
  struct bound_range
  {
    using value_type = bound<G, P>;
    static constexpr umax slot_count = NotchCount<value_type> + 1;

    struct iterator
    {
      umax index;
      imax remaining;

      constexpr value_type operator*() const
      {
        // value = Lower + index * Notch  (always exact: lies on the grid).
        rational val = (G.Interval.Lower
                        + (rational{index} * G.Notch).value()).value();
        return value_type{val};
      }
      constexpr iterator& operator++()
      {
        --remaining;
        index = (index + 1) % slot_count;
        return *this;
      }
      constexpr bool operator!=(iterator o) const { return remaining != o.remaining; }
    };

    umax start_index_;

    constexpr bound_range() : start_index_{0} {}

    constexpr bound_range(value_type start)
    {
      // Map a grid value back to its notch index: (start - Lower) / Notch.
      // The result has integer denominator (start is on the grid) so the
      // numerator is the index directly.
      auto offset = ((as_rational(start) - G.Interval.Lower)
                     / G.Notch).value();
      start_index_ = offset.Numerator;
    }

    constexpr iterator begin() const
    { return {start_index_, static_cast<imax>(slot_count)}; }
    constexpr iterator end() const
    { return {start_index_, 0}; }
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
