//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// Internal — include "bound/bound.hpp" (the umbrella), not this directly.
// Defines the core `bnd::bound<G, P>` type; the umbrella adds the free-function
// casts/arithmetic/range layers that depend on this complete type.
//---------------------------------------------------------------------------
#ifndef BNDcoreHPP
#define BNDcoreHPP

#include "bound/generic.hpp"
#include "bound/lift.hpp"
#include "bound/policy.hpp"
#include "bound/detail/addition.hpp"
#include "bound/detail/multiplication.hpp"
#include "bound/detail/division.hpp"
#include "bound/assignment.hpp"
#include "bound/predicates.hpp"

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

// Forward-declare the `bnd::math` entry points used in-class, so the bodies
// pass `-Wtemplate-body` without pulling cmath.hpp in unconditionally (its
// definitions live there).
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
// bound — the public struct users include. Defines `bound<G, P>` and its
// per-instance operators; free-function arithmetic and `bound_range` also live
// here. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap/sentinel), and
// generic.hpp/policy.hpp (traits + policy machinery).
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
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only a bound that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr raw_type const& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw()       noexcept
      requires ((P & unsafe) == unsafe) { return Raw; }

    // Trivial default ctor — Raw is left uninitialized, like a built-in scalar: a
    // default-constructed bound has no value until assigned. (A previous checked
    // overload zero-filled Raw, which decoded to an out-of-range value or the {0,0}
    // rational sentinel for grids not containing 0 — a defined-but-invalid footgun.
    // Value-init `bound{}` still zero-fills where a zero raw is genuinely wanted.)
    constexpr bound() = default;

    // `real` storage holds the value as a double directly. An arithmetic rhs
    // casts straight to double; a bound rhs goes through its exact rational view.
    private:
    template <numeric A>
    constexpr double to_double(A const& value)
    {
      if constexpr (std::is_arithmetic_v<A>) return value;
      else                                   return static_cast<double>(detail::as_rational(value));
    }
    public:
    // Snap a value onto `real` storage: lossless on the dyadic grid. Out-of-range
    // values run the same policy cascade as the fractional path (clamp → wrap →
    // sentinel/checked-report → store as-is); all arithmetic stays in double.
    constexpr void store_real(double v)
    {
      // NaN/±inf would reach snap_double's integer cast (UB); reject like the
      // non-real path. `v - v` is 0 for every finite v, NaN otherwise.
      if (!(v - v == 0))
        throw std::system_error(make_error_code(errc::not_finite),
                                "non-finite double");
      const double lo = static_cast<double>(G.Interval.Lower);
      const double hi = static_cast<double>(G.Interval.Upper);
      if (v < lo || v > hi)
      {
        if constexpr ((P & clamp) == clamp)
          v = v < lo ? lo : hi;
        else if constexpr ((P & wrap) == wrap)
        {
          // Fold into [Lower, Lower + range), range = span + notch — the same
          // convention as the fractional apply_wrap.
          const double range = hi - lo + static_cast<double>(G.Notch);
          const double q = (v - lo) / range;
          imax k = static_cast<imax>(q);
          if (q < 0 && static_cast<double>(k) != q) --k;   // floor
          v -= static_cast<double>(k) * range;
        }
        else if (detail::domain_fail(*this, make_policy<P>(),
                   std::to_string(v) + " is not in " + bnd::to_string(G.Interval)))
          return;            // sentinel stored / reported (error_code mode)
        // no handler (unchecked policy): fall through and store snapped as-is
      }
      Raw = G.snap_double(v);
    }

    template <numeric A>
    constexpr void store_value(A const& value)
    {
      if constexpr (detail::real_raw<bound>)
        store_real(to_double(value));
      else if constexpr (is_bound_v<A>)
      {
        // A `real` SOURCE holds its value as a double raw; the assignment engine's
        // integer offset formula (Lower + raw·Notch) would misread it. Extract as
        // a double and route through the arithmetic-source path.
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
    constexpr bound(A value, Pol&& pol)   // if assign reports an error (ec mode), Raw is
    {                                     // left ill-defined — check the error before reading
      if constexpr (detail::real_raw<bound>)
        store_real(to_double(value));
      else
        detail::assignment<bound, A>::assign(*this, value, pol);
    }

    // Error-code construction: `bound x(value, ec)`. Needs its own overload (a raw
    // error_code would bind the Pol&& template above). On a reported (out-of-range)
    // error, ec is set and the bound's value is ill-defined — do not read it without
    // checking ec first.
    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value, std::error_code& ec)
    {
      if constexpr (detail::real_raw<bound>)
        store_real(to_double(value));
      else
        detail::assignment<bound, A>::assign(*this, value, make_policy<P>(ec));
    }

    // optional<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws on `nullopt`.
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

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr bound from_raw(raw_type r) noexcept
    { bound b; b.Raw = r; return b; }

    // The reserved empty slot used by `slim::optional<bound>` and `sentinel`
    // policy, without poking `Raw`.
    [[nodiscard]] static constexpr bound make_sentinel() noexcept
    { return from_raw(detail::sentinel_raw<bound>()); }

    // Canonical "is this slot empty?" check under `sentinel` policy.
    [[nodiscard]] constexpr bool is_sentinel() const noexcept
    {
      if constexpr (detail::rational_raw<bound>)
        return Raw.Denominator == 0;
      else
        return Raw == detail::sentinel_raw<bound>();
    }

    // to<T>() emptiness predicate: under sentinel policy an empty slot reports
    // overflow; compiles to constant `false` under every other policy (the
    // reserved slot is unreachable). Internal: used only by the to<T>() overloads.
    private:
    [[nodiscard]] constexpr bool is_sentinel_under_policy() const noexcept
    {
      if constexpr ((P & sentinel) != 0) return is_sentinel();
      else                               return false;
    }
    public:

    // Conversion summary:
    //   operator imax     — implicit, when the grid is notch-aligned and fits in
    //                       int64 (else use `to<imax>()`). Also the `vec[b]` index
    //                       path. No second implicit integer operator (would make
    //                       `imax_var += b` ambiguous).
    //   operator rational — implicit; lossless and exact.
    //   operator double   — implicit for `real` bounds (dyadic grid → lossless);
    //                       explicit otherwise and gated on a rounding flag.
    //                       Strict bounds opt in via `to<double>().value()`.
    //   to<T>()           — typed-error narrowing/widening → `expected<T, errc>`
    //                       (overflow / domain_error / sentinel-state).
    //   as<T>()           — non-expected sibling; asserts on sentinel. For known-
    //                       in-range sites (array indexing). FP shares the gate.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    constexpr operator imax() const
      requires (detail::notch_is_unit_integer<G>
             && G.Interval.Lower >= bnd::detail::rational{std::numeric_limits<imax>::min()}
             && G.Interval.Upper <= bnd::detail::rational{std::numeric_limits<imax>::max()})
    { return detail::to_value(*this); }

    constexpr explicit((P & real) != real) operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snapping)) != 0)
    { return detail::as_double(*this); }

    constexpr operator bnd::detail::rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (!detail::index_raw<bound>)
        return Raw;

      // Q-format-with-integer-Lower fast path skips the generic path's three
      // rational ops. Falls through to the rational path when the raw is too wide
      // to widen safely (e.g. uint64 from a Q16.16 × Q16.16 result type).
      if constexpr (detail::HasQFormatFastPath<bound>)
        return detail::q_format_decode(*this);

      return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    // to<T>() — typed-error scalar extraction (mirrors rational::to<T>, extended
    // to signed and floating point). Returns `errc::not_a_value` (sentinel
    // state), `errc::overflow` (out of T's range), and `errc::domain_error`
    // (negative into unsigned T); fractional truncation is silent.
    template <std::unsigned_integral T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::not_a_value};

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
      if (is_sentinel_under_policy()) return slim::unexpected{errc::not_a_value};

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
      if (is_sentinel_under_policy()) return slim::unexpected{errc::not_a_value};
      return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict bound rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
      requires (!std::floating_point<T>
             || (P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snapping)) != 0)
    { return to<T>().value(); }

    // numerator() / denominator() — the exact value of a fractional bound as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
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

    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `bnd::math` — `bnd::math::floor(b)` etc. (auto-deduced Out)
    // or `bnd::math::floor_impl<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<bound/cmath.hpp>`.

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
        // Unsigned-offset fast path: with `value = Raw*Notch + Lower`, negating
        // is `NotchCount - Raw` (index from the opposite end) — no rational ops.
        // Unreachable for direct storage, so `Raw` here is guaranteed an offset.
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

    [[nodiscard]] constexpr auto with_truncate()        { return policy<snapping>(); }
    [[nodiscard]] constexpr auto with_round_nearest()   { return policy<round_nearest>(); }
    [[nodiscard]] constexpr auto with_floor()           { return policy<round_floor>(); }
    [[nodiscard]] constexpr auto with_ceil()            { return policy<round_ceil>(); }
    [[nodiscard]] constexpr auto with_round_half_even() { return policy<round_half_even>(); }
    [[nodiscard]] constexpr auto with_clamp()           { return policy<clamp>(); }
    [[nodiscard]] constexpr auto with_wrap()            { return policy<wrap>(); }

    // Shared builder for the single-action fluent hooks below. Merges the tag's
    // implied policy flag, then returns a policy_ref bound to *this carrying the
    // tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    // Internal: consumed only by the on_* hooks below.
    private:
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action)
    {
       using tag = Tag<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | detail::implied_flags<tag>>();
       return detail::policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }
    public:

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
    // policy_ref rejects mutually exclusive combinations at compile time. E.g.
    // `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs` — overflow probe fires λ1,
    // post-probe narrowing fires λ2.
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
      // Fast path: raw-level integer addition, safe when raw_a + raw_b is the raw
      // of value_a + value_b — direct storage, or offset encoding with Lower==0 both.
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
    // Out-of-range tail for raw-space compound arithmetic: dispatch on policy
    // (clamp/wrap/sentinel/checked) and store back to `Raw`. Called from
    // `operator+=(boundable)` only.
    constexpr bound& apply_raw_overflow(imax new_raw)
    {
      if constexpr (P & clamp)
        // RawLo/RawHi are already raw-space constants, so no raw_from_offset.
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
    template <typename Result>
    constexpr bound& assign_op_result(Result const& r)
    {
      if constexpr (requires { typename Result::value_type; })
        *this = r.value();
      else
        *this = r;
      return *this;
    }

    constexpr bool report_div_by_zero(const char* msg)
    {
      if constexpr (!(P & ignore_zero))
        make_policy<P>().report(errc::division_by_zero, msg);
      return false;   // caller returns *this directly
    }
    public:

    // Only a `rational` (a library type) may join a bound in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_b` /
    // `just<1>` / `bound<{lo,hi}>{n}`), mirroring the binary operators.
    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator+=(A const& rhs)
    { return assign_op_result(bnd::detail::rational{*this} + rhs); }

    template <boundable R>
    constexpr bound& operator-=(R const& rhs)
    { return *this += (-rhs); }

    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator-=(A const& rhs)
    { return assign_op_result(bnd::detail::rational{*this} - rhs); }

    template <boundable R>
    constexpr bound& operator*=(R const& rhs)
    { return assign_op_result(*this * rhs); }

    template <boundable R>
    constexpr bound& operator/=(R const& rhs)
    {
      if (rhs == 0)
      { report_div_by_zero("operator/= division by zero"); return *this; }
      return assign_op_result(*this / rhs);
    }

    template <boundable R>
    constexpr bound& operator%=(R const& rhs)
    {
      if (rhs == 0)
      { report_div_by_zero("operator%= division by zero"); return *this; }
      return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator*=(A const& rhs)
    { return assign_op_result(bnd::detail::rational{*this} * rhs); }

    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator/=(A const& rhs)
    {
      if (detail::is_canonical_zero(rhs))
      { report_div_by_zero("operator/= division by zero"); return *this; }
      return assign_op_result(bnd::detail::rational{*this} / rhs);
    }

    constexpr bound& operator++()    { return *this += bnd::detail::rational{1}; }
    constexpr bound  operator++(int) { bound t = *this; ++*this; return t; }
    constexpr bound& operator--()    { return *this -= bnd::detail::rational{1}; }
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
  // to<T>(b) / as<T>(b) — free-function forms, for generic code that would
  // otherwise need the `.template` disambiguator. Same semantics as the members.
  //---------------------------------------------------------------------------
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
  // zero / one — universal exact constants. Single-point bounds that assign into
  // any grid able to represent the value (compile-time checked) and otherwise
  // behave as 0 / 1. `b = zero;` is a compile error when 0 is not on b's grid.
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
  // Parse is exact (no double round-trip); same parser backs `_r` in
  // rational.hpp. `-1.5_b` parses as `-(1.5_b)`.
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

#endif // BNDcoreHPP
