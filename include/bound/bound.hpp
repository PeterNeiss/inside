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

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

// Forward-declare the `bnd::math` entry points the member-syntax aliases on
// `bound<G,P>` delegate to. The actual definitions live in <bound/cmath.hpp>;
// users who want `.floor()` / `.ceil()` / `.round()` / `.trunc()` / `.abs()`
// must include that header. The forward declarations here keep the in-class
// bodies through `-Wtemplate-body` without pulling cmath.hpp in unconditionally.
namespace bnd::math
{
  template <boundable Out, boundable In> constexpr Out floor_impl(In x) noexcept;
  template <boundable Out, boundable In> constexpr Out ceil_impl (In x) noexcept;
  template <boundable Out, boundable In> constexpr Out round_impl(In x) noexcept;
  template <boundable Out, boundable In> constexpr Out trunc_impl(In x) noexcept;
  template <boundable Out, boundable In> constexpr Out abs_impl  (In x) noexcept;
  template <boundable In> constexpr auto floor(In x) noexcept;
  template <boundable In> constexpr auto ceil (In x) noexcept;
  template <boundable In> constexpr auto round(In x) noexcept;
  template <boundable In> constexpr auto trunc(In x) noexcept;
  template <boundable In> constexpr auto abs  (In x) noexcept;
}

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
#ifndef BND_MATH_FIXED
    // Under the default (double) engine the `real` policy is double-backed, and
    // its value snaps to the grid (Lower + k·Notch). That snap is only exact
    // when the grid is dyadic — power-of-two notch and Lower — so grid points
    // are representable in IEEE-754 double. A continuous grid (Notch == 0) has
    // no grid to snap to. Anything else is rejected here rather than silently
    // demoted to integer storage.
    static_assert((P & real) != real || detail::dyadic_grid<G> || G.Notch == 0,
                  "bnd: the `real` policy requires a dyadic grid (power-of-two "
                  "notch and Lower, so values are exactly representable in double)");
#endif
    // Representation flags vs grid shape (exact has no requirement; a result
    // policy may carry several flags — storage selection resolves widest-wins,
    // so no mutual-exclusion asserts here).
    static_assert((P & direct) != direct || G.Notch == 1,
                  "bnd: the `direct` policy (raw == value as a plain integer) "
                  "requires Notch == 1");
    static_assert((P & indexed) != indexed || G.Notch != 0,
                  "bnd: the `indexed` policy (raw == 0-based notch index) "
                  "requires a notch (Notch != 0)");

    using negative = bound<-G, P>;
    using raw_type = detail::storage_for<G, P>;

    private:
    raw_type Raw;

    public:
    // raw() — the access escape hatch, symmetric with `from_raw`. The read
    // overload is available under every policy; read-only C interop can take
    // `&std::as_const(b).raw()` (a `const raw_type*`).
    //
    // The mutable overload is gated to the `unsafe` policy: a bound that has
    // already opted out of every range/round/zero check is the only one where
    // handing out a writable storage handle is honest. `&b.raw()` is then a
    // `raw_type*` a C routine can fill in place. Writing an out-of-range raw
    // into a checked/clamp/wrap/sentinel bound — which would make `value()`
    // and comparisons lie — is therefore a compile error, not a silent break.
    [[nodiscard]] constexpr raw_type const& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw()       noexcept
      requires ((P & unsafe) == unsafe) { return Raw; }

    constexpr bound() requires ((P & checked) == 0) = default; // trivial when not checked
    constexpr bound() requires ((P & checked) != 0) : Raw{} {} // zero-init under checked

    // Double-backed (`real`) storage stores the value as an IEEE-754 double
    // directly — the notch is nominal (no snap). An arithmetic rhs (the common
    // case: a math result) is cast straight to double; a bound rhs goes through
    // its exact rational view. Other storage routes through the policy-checked
    // `assignment` engine.
    template <numeric A>
    constexpr double to_double(A const& value)
    {
      if constexpr (std::is_arithmetic_v<A>) return value;
      else                                   return static_cast<double>(detail::as_rational(value));
    }
    // Snap a value onto `real` (double-backed) storage: it lands on the grid
    // exactly like any other bound (the grid is dyadic, so the snap is lossless)
    // — `real` changes the representation/speed, not the obey-the-grid contract.
    // Honors `clamp` to keep an out-of-range value on the grid's endpoints.
    constexpr void store_real(double v)
    {
      v = G.snap_double(v);
      if constexpr ((P & clamp) == clamp)
      {
        const double lo = static_cast<double>(G.Interval.Lower);
        const double hi = static_cast<double>(G.Interval.Upper);
        v = v < lo ? lo : (v > hi ? hi : v);
      }
      Raw = v;
    }

    template <numeric A>
    constexpr void store_value(A const& value)
    {
      if constexpr (detail::real_raw<bound>)
        store_real(to_double(value));
      else if constexpr (is_bound_v<A>)
      {
        // A `real` (double-backed) SOURCE holds its value directly as a `double`
        // raw. The assignment engine reads a boundable source through the
        // integer index/offset formula (Lower + raw·Notch), which misreads that
        // double raw. Extract the true value as a double and route through the
        // arithmetic-source path instead.
        if constexpr (detail::real_raw<A>)
          detail::assignment<bound, double>::assign(*this, detail::as_double(value), make_policy<P>());
        else
          detail::assignment<bound, A>::assign(*this, value, make_policy<P>());
      }
      else
        detail::assignment<bound, A>::assign(*this, value, make_policy<P>());
    }

    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value)
    { store_value(value); }

    template <numeric A, typename Pol>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value, Pol&& pol)
    {
      if constexpr (detail::real_raw<bound>)
        store_real(to_double(value));
      else
        detail::assignment<bound, A>::assign(*this, value, pol);
    }

    // optional<A> sink — unwrap once at the construction boundary so callers
    // can chain checked arithmetic without per-step `.value()`. Throws
    // `slim::bad_optional_access` on `nullopt`; matches the compound-assign
    // pattern in rational.hpp.
    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(slim::optional<A> const& value)
    { store_value(value.value()); }

    template <numeric B>
      requires bound_assignable<bound, B, P>
    constexpr bound& operator=(B const& other)
    { store_value(other); return *this; }

    template <numeric B>
      requires bound_assignable<bound, B, P>
    constexpr bound& operator=(slim::optional<B> const& other)
    { store_value(other.value()); return *this; }

    [[nodiscard]] constexpr auto value() const
    {
      if constexpr (detail::rational_raw<bound>)
        return Raw;
      else if constexpr (!detail::index_raw<bound>)
        return static_cast<std::common_type_t<raw_type, int>>(Raw);
      else
        return detail::as_rational(*this);
    }

    // Trusted construction from a storage-layout raw value. No validation —
    // the caller asserts `r` is a valid slot for this grid. Mirrors the public
    // `Raw` member and `value()`; the supported entry point for tests, fast
    // paths, and same-grid raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr bound from_raw(raw_type r) noexcept
    { bound b; b.Raw = r; return b; }

    // The reserved empty slot used by `slim::optional<bound>` and the sentinel
    // policy. Wraps the internal `detail::sentinel_raw<bound>()` so callers never have
    // to poke `Raw` to obtain an empty-state bound.
    [[nodiscard]] static constexpr bound make_sentinel() noexcept
    { return from_raw(detail::sentinel_raw<bound>()); }

    // Public sentinel probe — the canonical "is this slot empty?" check
    // under `sentinel` policy, matching the raw layout used by both the
    // policy machinery and `slim::optional<bound>`.
    [[nodiscard]] constexpr bool is_sentinel() const noexcept
    {
      if constexpr (detail::rational_raw<bound>)
        return Raw.Denominator == 0;
      else
        return Raw == detail::sentinel_raw<bound>();
    }

    // to<T>() emptiness predicate: under sentinel policy an empty slot has no
    // extractable value, so to<T>() reports overflow. Routes through the
    // canonical is_sentinel(); compiles to a constant `false` under every other
    // policy, where the reserved slot is unreachable (storage promotion
    // guarantees no valid value lands on it).
    [[nodiscard]] constexpr bool is_sentinel_under_policy() const noexcept
    {
      if constexpr ((P & sentinel) != 0) return is_sentinel();
      else                               return false;
    }

    // Conversion summary:
    //   operator imax     — implicit. Available only when the grid is
    //                       notch-aligned AND statically fits in
    //                       [INT64_MIN, INT64_MAX]. Pathological wide
    //                       grids must use `b.to<imax>()`. Also the index
    //                       path: `vec[b]` converts imax → size_t. (There is
    //                       deliberately NO second implicit integer operator:
    //                       two would make built-in mixed arithmetic like
    //                       `imax_var += b` ambiguous.)
    //   operator rational — implicit. Lossless and mathematically exact,
    //                       so no risk in letting it happen silently.
    //   operator double   — implicit for `real`-policy bounds: their dyadic
    //                       grid makes every value exactly representable in
    //                       double (both engines), so the conversion is
    //                       lossless — same rule as operator rational.
    //                       *Explicit* otherwise, AND gated by policy: only
    //                       available when P includes a rounding flag
    //                       (round_floor/ceil/nearest/half_even or
    //                       ignore_round). Strict-policy bounds must
    //                       use `b.to<double>().value()` to opt in.
    //   to<T>()           — typed-error narrowing/widening, returns
    //                       `slim::expected<T, errc>` for any
    //                       unsigned/signed integral or floating point
    //                       target. Reports overflow,
    //                       domain_error (negative into unsigned),
    //                       and sentinel-state via errc.
    //   as<T>()           — non-expected sibling. Returns T directly; asserts
    //                       on sentinel state. Use when the value is known
    //                       in range (the common case at array-index sites).
    //                       Floating-point targets share operator double's
    //                       policy gate; strict bounds use to<double>().
    //   to<T>(b) / as<T>(b) — free-function forms of the members, for generic
    //                       code (`as<imax>(b)` needs no `.template`).
    constexpr operator imax() const
      requires (detail::notch_is_unit_integer<G>
             && G.Interval.Lower >= bnd::detail::rational{std::numeric_limits<imax>::min()}
             && G.Interval.Upper <= bnd::detail::rational{std::numeric_limits<imax>::max()})
    { return detail::to_value(*this); }

    constexpr explicit((P & real) != real) operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | ignore_round)) != 0)
    { return detail::as_double(*this); }

    constexpr operator bnd::detail::rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (!detail::index_raw<bound>)
        return Raw;

      // Q-format-with-integer-Lower fast path skips the three rational ops
      // (multiply, add, optional-unwrap) of the generic path. Shared with
      // `from_value` and `assignment::store` via `q_format_decode`. When the
      // raw is too wide to widen safely (e.g. uint64 from a Q16.16 × Q16.16
      // result type), we fall through to the rational path below.
      if constexpr (detail::HasQFormatFastPath<bound>)
        return detail::q_format_decode(*this);

      return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    // to<T>() — typed-error scalar extraction. Mirrors rational::to<T>
    // and extends it to signed integers and floating point.
    // Unlike the always-succeed `detail::to_value(b)` / `operator T()` paths,
    // this returns `errc::overflow` (value out of T's range, or
    // sentinel-state) and `errc::domain_error` (negative into unsigned T).
    // Silent fractional truncation matches rational::to<T>.
    template <std::unsigned_integral T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::overflow};

      constexpr bool needs_neg_check = (Lower<bound> < 0);
      constexpr bool needs_max_check =
          (Upper<bound> > bnd::detail::rational{std::numeric_limits<T>::max()});

      if constexpr (!needs_neg_check && !needs_max_check)
        return static_cast<T>(detail::to_value(*this));
      else
      {
        auto r = detail::as_rational(*this);
        if constexpr (needs_neg_check)
          if (r < 0) return slim::unexpected{errc::domain_error};
        if constexpr (needs_max_check)
          if (r > bnd::detail::rational{std::numeric_limits<T>::max()})
            return slim::unexpected{errc::overflow};
        return static_cast<T>(r.trunc());
      }
    }

    template <std::signed_integral T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::overflow};

      constexpr bool needs_min_check =
          (Lower<bound> < bnd::detail::rational{std::numeric_limits<T>::min()});
      constexpr bool needs_max_check =
          (Upper<bound> > bnd::detail::rational{std::numeric_limits<T>::max()});

      if constexpr (!needs_min_check && !needs_max_check)
        return static_cast<T>(detail::to_value(*this));
      else
      {
        auto r = detail::as_rational(*this);
        if constexpr (needs_min_check)
          if (r < bnd::detail::rational{std::numeric_limits<T>::min()})
            return slim::unexpected{errc::overflow};
        if constexpr (needs_max_check)
          if (r > bnd::detail::rational{std::numeric_limits<T>::max()})
            return slim::unexpected{errc::overflow};
        return static_cast<T>(r.trunc());
      }
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::overflow};
      return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(). Returns T directly and lets
    // any error (sentinel-state, out of T's range, negative-into-unsigned)
    // surface as bad_expected_access from `to<T>().value()`. The shorthand
    // is for call sites where the user knows the value is in range — most
    // notably array indexing, capacity arithmetic, and rational extraction
    // for `.round()` / `.trunc()`.
    // Floating-point targets share operator double's policy gate so the two
    // spellings agree: a strict bound rejects both `double(b)` and
    // `b.as<double>()`, and `to<double>()` stays the explicit opt-in.
    template <typename T>
    [[nodiscard]] constexpr T as() const
      requires (!std::floating_point<T>
             || (P & (round_floor | round_ceil | round_nearest
                    | round_half_even | ignore_round)) != 0)
    { return to<T>().value(); }

    // numerator() / denominator() — the exact value of a fractional (Q-format)
    // bound as an integer pair, sign carried on the numerator and denominator
    // kept positive. This is the supported EXACT read-out: it never mentions
    // the internal representation type, so callers stay in plain integers
    // (e.g. `b.numerator() * step / b.denominator()`) without an intermediate
    // fraction object. For an integer-notch bound, denominator() == 1.
    [[nodiscard]] constexpr imax numerator() const
    {
      auto r = detail::as_rational(*this);
      return (r.Denominator < 0) ? -r.Numerator : r.Numerator;
    }

    [[nodiscard]] constexpr imax denominator() const
    {
      auto r = detail::as_rational(*this);
      return detail::abs_den(r.Denominator);
    }

    // Member-syntax aliases for the free functions in `bnd::math` — these
    // only compile when <bound/cmath.hpp> is also included (the bodies refer
    // to names declared there). No-arg form picks the auto-deduced output
    // grid; the templated form accepts an explicit `Out` bound.
    //
    //   b.floor()       // bound on ⌊In⌋ interval, notch 1
    //   b.floor<Out>()  // explicit Out
    [[nodiscard]] constexpr auto floor() const noexcept { return bnd::math::floor(*this); }
    template <typename Out>
    [[nodiscard]] constexpr Out floor() const noexcept { return bnd::math::floor_impl<Out>(*this); }

    [[nodiscard]] constexpr auto ceil() const noexcept { return bnd::math::ceil(*this); }
    template <typename Out>
    [[nodiscard]] constexpr Out ceil() const noexcept { return bnd::math::ceil_impl<Out>(*this); }

    [[nodiscard]] constexpr auto round() const noexcept { return bnd::math::round(*this); }
    template <typename Out>
    [[nodiscard]] constexpr Out round() const noexcept { return bnd::math::round_impl<Out>(*this); }

    [[nodiscard]] constexpr auto trunc() const noexcept { return bnd::math::trunc(*this); }
    template <typename Out>
    [[nodiscard]] constexpr Out trunc() const noexcept { return bnd::math::trunc_impl<Out>(*this); }

    [[nodiscard]] constexpr auto abs() const noexcept { return bnd::math::abs(*this); }
    template <typename Out>
    [[nodiscard]] constexpr Out abs() const noexcept { return bnd::math::abs_impl<Out>(*this); }

    [[nodiscard]] constexpr negative operator-() const
    {
      negative neg;
      if constexpr (detail::real_raw<bound>)
        neg = negative::from_raw(-Raw);
      else if constexpr (detail::rational_raw<bound>)
        neg = negative::from_raw(-(Raw));
      else if constexpr (!detail::index_raw<bound> || !detail::index_raw<negative>)
        detail::from_value(neg, -detail::to_value(*this));
      else
        // Unsigned-offset fast path: with offset encoding `value = Raw*Notch + Lower`,
        // negating the value is equivalent to indexing from the opposite end of the
        // grid — i.e. `NotchCount - Raw`. No rational arithmetic in the hot path.
        //
        // NOTE: not a sibling of the direct-storage encoding bugs — this branch is
        // unreachable when either operand uses direct storage, so `Raw` here is
        // guaranteed to be an offset.
        neg = negative::from_raw(detail::raw_cast<negative>(detail::NotchCount<bound> - Raw));
      return neg;
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy()
    {
       auto pol = make_policy<P | F>();
       return detail::policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(std::error_code& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return detail::policy_ref<bound, decltype(pol)>{*this, pol};
    }

    [[nodiscard]] constexpr auto with_truncate()        { return policy<ignore_round>(); }
    [[nodiscard]] constexpr auto with_round_nearest()   { return policy<round_nearest>(); }
    [[nodiscard]] constexpr auto with_floor()           { return policy<round_floor>(); }
    [[nodiscard]] constexpr auto with_ceil()            { return policy<round_ceil>(); }
    [[nodiscard]] constexpr auto with_round_half_even() { return policy<round_half_even>(); }
    [[nodiscard]] constexpr auto with_clamp()           { return policy<clamp>(); }
    [[nodiscard]] constexpr auto with_wrap()            { return policy<wrap>(); }

    // Shared builder for the single-action fluent hooks below. Merges the tag's
    // implied policy flag, then returns a policy_ref bound to *this carrying the
    // tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action)
    {
       using tag = Tag<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | detail::implied_flags<tag>>();
       return detail::policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }

    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& a)     { return make_action_ref<on_wrap_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& a)    { return make_action_ref<on_clamp_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& a)    { return make_action_ref<on_error_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_sentinel(A&& a) { return make_action_ref<on_sentinel_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_overflow(A&& a) { return make_action_ref<on_overflow_t>(std::forward<A>(a)); }

    // Multi-action entry point: combine N tagged actions into one policy_ref.
    // The merged implied flags drive the policy; conflict diagnostics in
    // policy_ref reject mutually exclusive combinations (e.g. on_clamp + on_wrap)
    // at compile time. Use case: `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs`
    // — the imax-overflow probe fires λ1, the post-probe narrowing fires λ2.
    template <typename... Actions>
    [[nodiscard]] constexpr auto with(Actions&&... actions)
    {
       constexpr policy_flag merged = detail::merged_implied_flags<Actions...>;
       auto pol = make_policy<P | merged>();
       return detail::policy_ref<bound, decltype(pol), std::remove_cvref_t<Actions>...>{
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
      if constexpr (!detail::rational_raw<bound> && !detail::rational_raw<R>
                    && Notch<bound> == Notch<R>
                    && (!detail::index_raw<R>
                        || (Lower<bound> == 0 && Lower<R> == 0)))
      {
        if constexpr (P & (clamp | wrap | checked | sentinel))
        {
          imax new_raw = detail::raw_imax(*this) + detail::raw_imax(rhs);
          if (new_raw < detail::RawLo<bound> || new_raw > detail::RawHi<bound>)
            return apply_raw_overflow(new_raw);
          Raw = detail::raw_cast<bound>(new_raw);
        }
        else
          Raw += detail::raw_cast<bound>(rhs.raw());
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
        Raw = detail::raw_cast<bound>(new_raw < detail::RawLo<bound> ? detail::RawLo<bound> : detail::RawHi<bound>);
      else if constexpr (P & wrap)
      {
        constexpr imax range = detail::RawHi<bound> - detail::RawLo<bound> + 1;
        new_raw = ((new_raw - detail::RawLo<bound>) % range + range) % range + detail::RawLo<bound>;
        Raw = detail::raw_cast<bound>(new_raw);
      }
      else if constexpr (P & sentinel)
        Raw = detail::sentinel_raw<bound>();
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
    { return detail::assignment<bound, imax>::assign(*this, v, make_policy<P>()); }

    template <typename Result>
    constexpr bound& assign_op_result(Result const& r)
    {
      if constexpr (requires { typename Result::value_type; })
        *this = r.value();
      else
        *this = r;
      return *this;
    }

    // Type-generic wrapping +/−/× in T's unsigned domain. At imax width these
    // are the overflow-free fallback of `integer_compound_assign`; at raw
    // width they are its vectorizable unchecked arm (same low bits either
    // way — two's complement).
    struct wrap_add_t { template <typename T> constexpr T operator()(T a, T b) const noexcept
      { using U = std::make_unsigned_t<T>; return static_cast<T>(static_cast<U>(a) + static_cast<U>(b)); } };
    struct wrap_sub_t { template <typename T> constexpr T operator()(T a, T b) const noexcept
      { using U = std::make_unsigned_t<T>; return static_cast<T>(static_cast<U>(a) - static_cast<U>(b)); } };
    struct wrap_mul_t { template <typename T> constexpr T operator()(T a, T b) const noexcept
      { using U = std::make_unsigned_t<T>; return static_cast<T>(static_cast<U>(a) * static_cast<U>(b)); } };

    // Integer fast path for +=, -=, *= with arithmetic rhs.
    // `WrapOp` is the wrapping fallback (no overflow check); `CheckedOp` is
    // one of `add_overflow`/`sub_overflow`/`mul_overflow`.
    template <typename WrapOp>
    constexpr bound& integer_compound_assign(
        imax rhs,
        bool (*CheckedOp)(imax, imax, imax*),
        WrapOp wrap_op,
        const char* err_msg)
    {
      // Unchecked policy + value storage: operate directly in raw width.
      // Bit-identical to the imax round-trip below (truncating a 64-bit
      // two's-complement result to N bits equals the N-bit result), but a
      // loop of `b += k` vectorizes at the raw type's lane width instead of
      // 64-bit lanes (e.g. 4× the lanes for a uint8 raw).
      if constexpr ((P & (checked | clamp | wrap | sentinel)) == 0
                    && detail::value_raw<bound>
                    && std::integral<raw_type>)
      {
        Raw = wrap_op(Raw, static_cast<raw_type>(rhs));
        return *this;
      }
      else
      {
        imax l = detail::to_value(*this);
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
          result = wrap_op(l, rhs);
        return assign_imax(result);
      }
    }

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
        return integer_compound_assign(rhs,
            add_overflow, wrap_add_t{}, "operator+= overflow");
      else if constexpr (std::same_as<A, bnd::detail::rational>)
        return assign_op_result(bnd::detail::rational{*this} + rhs);
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
        return integer_compound_assign(rhs,
            sub_overflow, wrap_sub_t{}, "operator-= overflow");
      else if constexpr (std::same_as<A, bnd::detail::rational>)
        return assign_op_result(bnd::detail::rational{*this} - rhs);
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
        return integer_compound_assign(rhs,
            mul_overflow, wrap_mul_t{}, "operator*= overflow");
      else if constexpr (std::same_as<A, bnd::detail::rational>)
        return assign_op_result(bnd::detail::rational{*this} * rhs);
      else  // floating_point
        return *this = static_cast<double>(*this) * static_cast<double>(rhs);
    }

    template <arithmetic A>
    constexpr bound& operator/=(A rhs)
    {
      // Rational stores zero canonically as {0, 1}; the {0, 0} sentinel
      // would compare unequal to rational{0}. Check Numerator == 0 to catch
      // every canonical zero independent of representation.
      bool is_zero;
      if constexpr (std::same_as<A, bnd::detail::rational>) is_zero = (rhs.Numerator == 0);
      else                                     is_zero = (rhs == A{0});
      if (is_zero) { report_div_by_zero("operator/= division by zero"); return *this; }

      if constexpr (std::integral<A>)
        return assign_imax(detail::to_value(*this) / static_cast<imax>(rhs));
      else if constexpr (std::same_as<A, bnd::detail::rational>)
        return assign_op_result(bnd::detail::rational{*this} / rhs);
      else  // floating_point
        return *this = static_cast<double>(*this) / static_cast<double>(rhs);
    }

    template <arithmetic A>
    constexpr bound& operator%=(A rhs)
    {
      if (rhs == 0) { report_div_by_zero("operator%= division by zero"); return *this; }
      return assign_imax(detail::to_value(*this) % static_cast<imax>(rhs));
    }

    constexpr bound& operator++()    { return *this += 1; }
    constexpr bound  operator++(int) { bound t = *this; ++*this; return t; }
    constexpr bound& operator--()    { return *this -= 1; }
    constexpr bound  operator--(int) { bound t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr slim::expected<bound, errc> try_make(A value)
    {
      std::error_code ec;
      bound result;
      detail::assignment<bound, A>::assign(result, value, make_policy<P>(ec));
      if (ec) return slim::unexpected{static_cast<errc>(ec.value())};
      // For `sentinel` policy types, an out-of-range write silently sets
      // result.Raw to the sentinel; surface that as a domain error.
      if constexpr ((P & sentinel) != 0)
        if (result.Raw == detail::sentinel_raw<bound>())
          return slim::unexpected{errc::domain_error};
      return result;
    }
  };

  //---------------------------------------------------------------------------
  // to<T>(b) / as<T>(b) — free-function forms of the members
  //---------------------------------------------------------------------------
  // In dependent contexts a member template call needs the `template`
  // disambiguator (`b.template as<imax>()`); the free form `as<imax>(b)` reads
  // naturally in generic code and is found by ADL. Same semantics and
  // constraints as the members (the requires-clause makes them SFINAE-clean).
  template <typename T, boundable B>
  [[nodiscard]] constexpr auto to(B const& b)
    requires requires { b.template to<T>(); }
  { return b.template to<T>(); }

  template <typename T, boundable B>
  [[nodiscard]] constexpr T as(B const& b)
    requires requires { b.template as<T>(); }
  { return b.template as<T>(); }

  //---------------------------------------------------------------------------
  // comparison
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  constexpr auto operator<=>(L const& lhs, R const& rhs)
  {
    // same grid: Raw is monotonically ordered regardless of storage kind
    if constexpr (Grid<L> == Grid<R>)
      return lhs.raw() <=> rhs.raw();
    // double-backed (`real`) operand: compare in double (raw_imax would truncate)
    else if constexpr (detail::real_raw<L> || detail::real_raw<R>)
      return detail::as_double(lhs) <=> detail::as_double(rhs);
    // both integer-direct (notch=1, Raw==value): compare as integers
    else if constexpr (!detail::rational_raw<L> && !detail::rational_raw<R>
                       && !detail::index_raw<L> && !detail::index_raw<R>)
      return detail::raw_imax(lhs) <=> detail::raw_imax(rhs);
    else
      return detail::as_rational(lhs) <=> detail::as_rational(rhs);
  }

  template <boundable L, boundable R>
  constexpr bool operator==(L const& lhs, R const& rhs)
  {
    if constexpr (Grid<L> == Grid<R>)
      return lhs.raw() == rhs.raw();
    else if constexpr (detail::real_raw<L> || detail::real_raw<R>)
      return detail::as_double(lhs) == detail::as_double(rhs);
    else if constexpr (!detail::rational_raw<L> && !detail::rational_raw<R>
                       && !detail::index_raw<L> && !detail::index_raw<R>)
      return detail::raw_imax(lhs) == detail::raw_imax(rhs);
    else
      return detail::as_rational(lhs) == detail::as_rational(rhs);
  }

  template <boundable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs)
  {
    if constexpr (detail::value_raw<B>)
      return detail::raw_imax(lhs) <=> static_cast<imax>(rhs);
    else
      return detail::as_rational(lhs) <=> bnd::detail::rational{rhs};
  }

  template <boundable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs)
  {
    if constexpr (detail::value_raw<B>)
      return detail::raw_imax(lhs) == static_cast<imax>(rhs);
    else
      return detail::as_rational(lhs) == bnd::detail::rational{rhs};
  }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = bound<grid{value}>{value};

  //---------------------------------------------------------------------------
  // zero / one — universal exact constants. Each is a single-point bound that
  // assigns into ANY grid able to represent the value (checked at compile time,
  // no runtime check) and otherwise behaves as the value 0 / 1 in comparison
  // and arithmetic (they are ordinary `bound`s flowing through the normal
  // operators). `b = zero;` is a clean compile error when 0 is not on b's grid.
  //---------------------------------------------------------------------------
  inline constexpr auto zero = just<0>;
  inline constexpr auto one  = just<1>;

  //---------------------------------------------------------------------------
  // _b literal — compile-time `bound<{V, V}>` from a numeric literal.
  //   5_b           // bound<{5, 5}>            integer
  //   1.25_b        // bound<{rational{5,4}}>   decimal
  //   1.5e2_b       // bound<{150}>             decimal scientific
  //   0xff_b        // bound<{255}>             hex integer
  //   0b1010_b      // bound<{10}>              binary integer
  //   0x1p15_b      // bound<{32768}>           hex with 2^N exponent (Q-format)
  //   0x1p-15_b     // bound<{rational{1,32768}}>   1/2^15 grid notch
  //   0x1.8p3_b     // bound<{12}>              hex float
  //   1'000_b       // bound<{1000}>            digit separator
  //
  // Parse is exact (no double round-trip). Same parser backs `_r` in
  // rational.hpp. Negative literals: `-1.5_b` parses as `-(1.5_b)` — fine,
  // since unary `operator-` on bound returns a point on the negated grid.
  //---------------------------------------------------------------------------
  template<char... Chars>
  constexpr auto operator""_b() { return just<detail::_detail::parse_b_literal<Chars...>()>; }

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
    return bnd::bound<G, P>::from_raw(bnd::detail::sentinel_raw<bnd::bound<G, P>>());
  }

  template <bnd::grid G, bnd::policy_flag P>
  constexpr bool sentinel_traits<bnd::bound<G, P>>::is_sentinel(const bnd::bound<G, P>& v) noexcept
  {
    using raw = typename bnd::bound<G, P>::raw_type;
    // Rational uses a broader check (any zero denominator) than equality
    // against the canonical {1, 0} sentinel.
    if constexpr (std::is_same_v<raw, bnd::detail::rational>)
      return v.raw().Denominator == 0;
    else
      return v.raw() == bnd::detail::sentinel_raw<bnd::bound<G, P>>();
  }
} // namespace slim

#include "bound/casts.hpp"
#include "bound/arithmetic.hpp"
#include "bound/range.hpp"

#endif // BNDboundHPP
