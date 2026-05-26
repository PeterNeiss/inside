//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/generic.hpp"
#include "bound/lift.hpp"
#include "bound/policy.hpp"
#include "bound/detail/addition.hpp"
#include "bound/detail/multiplication.hpp"
#include "bound/detail/division.hpp"
#include "bound/assignment.hpp"
#include "bound/predicates.hpp"

#include <expected>

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

    // Conversion summary:
    //   operator imax     — implicit. Available only when the grid is
    //                       notch-aligned AND statically fits in
    //                       [INT64_MIN, INT64_MAX]. Pathological wide
    //                       grids must use `b.to<imax>()`.
    //   operator size_t   — implicit. Available when Lower >= 0, the grid
    //                       is notch-aligned, and Upper <= SIZE_MAX. Lets
    //                       a bound serve as an array/vector index without
    //                       the `.to<std::size_t>().value()` ceremony.
    //   operator rational — implicit. Lossless and mathematically exact,
    //                       so no risk in letting it happen silently.
    //   operator double   — *explicit*, AND gated by policy: only
    //                       available when P includes a rounding flag
    //                       (round_floor/ceil/nearest/half_even or
    //                       ignore_round). Strict-policy bounds must
    //                       use `b.to<double>().value()` to opt in.
    //   to<T>()           — typed-error narrowing/widening, returns
    //                       `std::expected<T, errc>` for any
    //                       unsigned/signed integral, floating point,
    //                       or `rational` target. Reports overflow,
    //                       domain_error (negative into unsigned),
    //                       and sentinel-state via errc.
    //   as<T>()           — non-expected sibling. Returns T directly; asserts
    //                       on sentinel state. Use when the value is known
    //                       in range (the common case at array-index sites).
    constexpr operator imax() const
      requires (abs_den(G.Notch.Denominator) == 1
             && G.Notch.Numerator != 0
             && G.Interval.Lower >= rational{std::numeric_limits<imax>::min()}
             && G.Interval.Upper <= rational{std::numeric_limits<imax>::max()})
    { return to_value(*this); }

    // Implicit size_t conversion for index-shaped bounds. Lets
    // `vec[bound_idx]` compile without `.as<std::size_t>()` and without
    // tripping `-Wsign-conversion` on the imax → size_t path. Upper is
    // capped at imax_max (not size_t_max) so wide bounds — those whose
    // Upper already exceeds imax — don't gain a back-door imax conversion
    // via size_t → imax integer narrowing.
    constexpr operator std::size_t() const
      requires (abs_den(G.Notch.Denominator) == 1
             && G.Notch.Numerator != 0
             && G.Interval.Lower >= 0
             && G.Interval.Upper <= rational{std::numeric_limits<imax>::max()})
    { return static_cast<std::size_t>(to_value(*this)); }

    constexpr explicit operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | ignore_round)) != 0)
    { return G.raw_to_double(Raw); }

    constexpr operator rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (IsDirectStorage<bound>)
        return Raw;

      // Q-format-with-integer-Lower fast path skips the three rational ops
      // (multiply, add, optional-unwrap) of the generic path. Shared with
      // `from_value` and `assignment::store` via `q_format_decode`. When the
      // raw is too wide to widen safely (e.g. uint64 from a Q16.16 × Q16.16
      // result type), we fall through to the rational path below.
      if constexpr (HasQFormatFastPath<bound>)
        return q_format_decode(*this);

      return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    // to<T>() — typed-error scalar extraction. Mirrors rational::to<T>
    // and extends it to signed integers, floating point, and rational.
    // Unlike the always-succeed `to_value(b)` / `operator T()` paths,
    // this returns `errc::overflow` (value out of T's range, or
    // sentinel-state) and `errc::domain_error` (negative into unsigned T).
    // Silent fractional truncation matches rational::to<T>.
    template <std::unsigned_integral T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      if constexpr ((P & sentinel) != 0)
        if (Raw == sentinel_raw<bound>())
          return std::unexpected{errc::overflow};

      constexpr bool needs_neg_check = (Lower<bound> < 0);
      constexpr bool needs_max_check =
          (Upper<bound> > rational{std::numeric_limits<T>::max()});

      if constexpr (!needs_neg_check && !needs_max_check)
        return static_cast<T>(to_value(*this));
      else
      {
        rational r = *this;
        if constexpr (needs_neg_check)
          if (r < 0) return std::unexpected{errc::domain_error};
        if constexpr (needs_max_check)
          if (r > rational{std::numeric_limits<T>::max()})
            return std::unexpected{errc::overflow};
        return static_cast<T>(r.trunc());
      }
    }

    template <std::signed_integral T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      if constexpr ((P & sentinel) != 0)
        if (Raw == sentinel_raw<bound>())
          return std::unexpected{errc::overflow};

      constexpr bool needs_min_check =
          (Lower<bound> < rational{std::numeric_limits<T>::min()});
      constexpr bool needs_max_check =
          (Upper<bound> > rational{std::numeric_limits<T>::max()});

      if constexpr (!needs_min_check && !needs_max_check)
        return static_cast<T>(to_value(*this));
      else
      {
        rational r = *this;
        if constexpr (needs_min_check)
          if (r < rational{std::numeric_limits<T>::min()})
            return std::unexpected{errc::overflow};
        if constexpr (needs_max_check)
          if (r > rational{std::numeric_limits<T>::max()})
            return std::unexpected{errc::overflow};
        return static_cast<T>(r.trunc());
      }
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      if constexpr ((P & sentinel) != 0)
        if (Raw == sentinel_raw<bound>())
          return std::unexpected{errc::overflow};
      return static_cast<T>(G.raw_to_double(Raw));
    }

    template <std::same_as<rational> T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      if constexpr ((P & sentinel) != 0)
        if (Raw == sentinel_raw<bound>())
          return std::unexpected{errc::overflow};
      return rational{*this};
    }

    // as<T>() — non-expected sibling of to<T>(). Returns T directly and lets
    // any error (sentinel-state, out of T's range, negative-into-unsigned)
    // surface as bad_expected_access from `to<T>().value()`. The shorthand
    // is for call sites where the user knows the value is in range — most
    // notably array indexing, capacity arithmetic, and rational extraction
    // for `.round()` / `.trunc()`.
    template <typename T>
    [[nodiscard]] constexpr T as() const { return to<T>().value(); }

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
    [[nodiscard]] constexpr auto policy()
    {
       auto pol = make_policy<P | F>();
       return policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(std::error_code& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return policy_ref<bound, decltype(pol)>{*this, pol};
    }

    [[nodiscard]] constexpr auto with_truncate()        { return policy<ignore_round>(); }
    [[nodiscard]] constexpr auto with_round_nearest()   { return policy<round_nearest>(); }
    [[nodiscard]] constexpr auto with_floor()           { return policy<round_floor>(); }
    [[nodiscard]] constexpr auto with_ceil()            { return policy<round_ceil>(); }
    [[nodiscard]] constexpr auto with_round_half_even() { return policy<round_half_even>(); }
    [[nodiscard]] constexpr auto with_clamp()           { return policy<clamp>(); }
    [[nodiscard]] constexpr auto with_wrap()            { return policy<wrap>(); }

    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& action)
    {
       using tag = on_wrap_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& action)
    {
       using tag = on_clamp_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& action)
    {
       using tag = on_error_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    [[nodiscard]] constexpr auto on_sentinel(A&& action)
    {
       using tag = on_sentinel_t<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | implied_flags<tag>>();
       return policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    [[nodiscard]] constexpr auto on_overflow(A&& action)
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
    [[nodiscard]] constexpr auto with(Actions&&... actions)
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
                        || (Lower<bound> == 0 && Lower<R> == 0)))
      {
        if constexpr (P & (clamp | wrap | checked | sentinel))
        {
          imax new_raw = raw_imax(*this) + raw_imax(rhs);
          if (new_raw < RawLo<bound> || new_raw > RawHi<bound>)
            return apply_raw_overflow(new_raw);
          Raw = raw_cast<bound>(new_raw);
        }
        else
          Raw += raw_cast<bound>(rhs.Raw);
        return *this;
      }
      else
      {
        *this = *this + rhs;
        return *this;
      }
    }

    private:
    // Out-of-range tail for raw-space compound arithmetic: dispatch on the
    // type's policy (clamp/wrap/sentinel/checked). `new_raw` is the unclamped
    // raw-space result; one of the four branches stores back to `Raw`. Called
    // from `operator+=(boundable)` only (operator-= delegates through +=, the
    // arithmetic-RHS path uses `integer_compound_assign`, and *= / /= / %=
    // route through `assign_op_result`).
    constexpr bound& apply_raw_overflow(imax new_raw)
    {
      if constexpr (P & clamp)
        // RawLo/RawHi are raw-space constants by construction (Lower/Upper for
        // direct storage, 0/NotchCount for notch-offset), so they're already
        // the correct Raw — no raw_from_offset needed.
        Raw = raw_cast<bound>(new_raw < RawLo<bound> ? RawLo<bound> : RawHi<bound>);
      else if constexpr (P & wrap)
      {
        constexpr imax range = RawHi<bound> - RawLo<bound> + 1;
        new_raw = ((new_raw - RawLo<bound>) % range + range) % range + RawLo<bound>;
        Raw = raw_cast<bound>(new_raw);
      }
      else if constexpr (P & sentinel)
        Raw = sentinel_raw<bound>();
      else
        make_policy<P>().report(errc::domain_error, "operator+= result out of range");
      return *this;
    }
    public:

    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
    private:
    constexpr bound& assign_imax(imax v)
    { return assignment<bound, imax>::assign(*this, v, make_policy<P>()); }

    template <typename Result>
    constexpr bound& assign_op_result(Result const& r)
    {
      if constexpr (requires { typename Result::value_type; })
        *this = r.value();
      else
        *this = r;
      return *this;
    }

    // Integer fast path for +=, -=, *= with arithmetic rhs.
    // `WrapOp` is the umax-safe fallback (no overflow check); `CheckedOp` is
    // one of `add_overflow`/`sub_overflow`/`mul_overflow`. Function-pointer
    // params are inlined under -O1+.
    constexpr bound& integer_compound_assign(
        imax rhs,
        bool (*CheckedOp)(imax, imax, imax*),
        imax (*WrapOp)(imax, imax),
        const char* err_msg)
    {
      imax l = to_value(*this);
      imax result;
      if constexpr (P & checked)
      {
        if (CheckedOp(l, rhs, &result))
        {
          make_policy<P>().report(errc::overflow, err_msg);
          return *this;
        }
      }
      else
        result = WrapOp(l, rhs);
      return assign_imax(result);
    }

    static constexpr imax wrap_add_(imax a, imax b) noexcept
    { return static_cast<imax>(static_cast<umax>(a) + static_cast<umax>(b)); }
    static constexpr imax wrap_sub_(imax a, imax b) noexcept
    { return static_cast<imax>(static_cast<umax>(a) - static_cast<umax>(b)); }
    static constexpr imax wrap_mul_(imax a, imax b) noexcept
    { return static_cast<imax>(static_cast<umax>(a) * static_cast<umax>(b)); }

    constexpr bool report_div_by_zero(const char* msg)
    {
      if constexpr (!(P & ignore_zero))
        make_policy<P>().report(errc::division_by_zero, msg);
      return false;   // caller returns *this directly
    }
    public:

    template <arithmetic A>
    constexpr bound& operator+=(A rhs)
    {
      if constexpr (std::integral<A>)
        return integer_compound_assign(static_cast<imax>(rhs),
            add_overflow, wrap_add_, "operator+= overflow");
      else if constexpr (std::same_as<A, rational>)
        return assign_op_result(rational{*this} + rhs);
      else  // floating_point — route through double, snap via policy
        return *this = static_cast<double>(*this) + static_cast<double>(rhs);
    }

    template <boundable R>
    constexpr bound& operator-=(R const& rhs)
    { return *this += (-rhs); }

    template <arithmetic A>
    constexpr bound& operator-=(A rhs)
    {
      if constexpr (std::integral<A>)
        return integer_compound_assign(static_cast<imax>(rhs),
            sub_overflow, wrap_sub_, "operator-= overflow");
      else if constexpr (std::same_as<A, rational>)
        return assign_op_result(rational{*this} - rhs);
      else  // floating_point
        return *this = static_cast<double>(*this) - static_cast<double>(rhs);
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

    template <arithmetic A>
    constexpr bound& operator*=(A rhs)
    {
      if constexpr (std::integral<A>)
        return integer_compound_assign(static_cast<imax>(rhs),
            mul_overflow, wrap_mul_, "operator*= overflow");
      else if constexpr (std::same_as<A, rational>)
        return assign_op_result(rational{*this} * rhs);
      else  // floating_point
        return *this = static_cast<double>(*this) * static_cast<double>(rhs);
    }

    template <arithmetic A>
    constexpr bound& operator/=(A rhs)
    {
      // Rational stores zero canonically as {0, 1}; the {0, 0} sentinel
      // would compare unequal to 0_r. Check Numerator == 0 to catch
      // every canonical zero independent of representation.
      bool is_zero;
      if constexpr (std::same_as<A, rational>) is_zero = (rhs.Numerator == 0);
      else                                     is_zero = (rhs == A{0});
      if (is_zero) { report_div_by_zero("operator/= division by zero"); return *this; }

      if constexpr (std::integral<A>)
        return assign_imax(to_value(*this) / static_cast<imax>(rhs));
      else if constexpr (std::same_as<A, rational>)
        return assign_op_result(rational{*this} / rhs);
      else  // floating_point
        return *this = static_cast<double>(*this) / static_cast<double>(rhs);
    }

    template <arithmetic A>
    constexpr bound& operator%=(A rhs)
    {
      if (rhs == 0) { report_div_by_zero("operator%= division by zero"); return *this; }
      return assign_imax(to_value(*this) % static_cast<imax>(rhs));
    }

    constexpr bound& operator++()    { return *this += 1; }
    constexpr bound  operator++(int) { bound t = *this; ++*this; return t; }
    constexpr bound& operator--()    { return *this -= 1; }
    constexpr bound  operator--(int) { bound t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr std::expected<bound, errc> try_make(A value)
    {
      std::error_code ec;
      bound result;
      assignment<bound, A>::assign(result, value, make_policy<P>(ec));
      if (ec) return std::unexpected{static_cast<errc>(ec.value())};
      // For `sentinel` policy types, an out-of-range write silently sets
      // result.Raw to the sentinel; surface that as a domain error.
      if constexpr ((P & sentinel) != 0)
        if (result.Raw == sentinel_raw<bound>())
          return std::unexpected{errc::domain_error};
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
      return raw_imax(lhs) <=> raw_imax(rhs);
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
      return raw_imax(lhs) == raw_imax(rhs);
    else
      return as_rational(lhs) == as_rational(rhs);
  }

  template <boundable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs)
  {
    if constexpr (!IsRawRational<B> && IsDirectStorage<B>)
      return raw_imax(lhs) <=> static_cast<imax>(rhs);
    else
      return as_rational(lhs) <=> rational{rhs};
  }

  template <boundable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs)
  {
    if constexpr (!IsRawRational<B> && IsDirectStorage<B>)
      return raw_imax(lhs) == static_cast<imax>(rhs);
    else
      return as_rational(lhs) == rational{rhs};
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

#include "bound/casts.hpp"
#include "bound/arithmetic.hpp"
#include "bound/range.hpp"

#endif // BNDboundHPP
