// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
// Defines the core `beman::inside::inside<G, P>` type; the umbrella adds the free-function
// casts/arithmetic/range layers that depend on this complete type.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CORE_HPP
#define BEMAN_INSIDE_CORE_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/lift.hpp>
#include <beman/inside/policy.hpp>
#include <beman/inside/detail/addition.hpp>
#include <beman/inside/detail/multiplication.hpp>
#include <beman/inside/detail/division.hpp>
#include <beman/inside/detail/assignment.hpp>
#include <beman/inside/predicates.hpp>

#include <bit>        // std::countr_zero, std::has_single_bit
#include <expected>   // std::expected, std::unexpected
#include <utility>    // std::pair

// Deducing `this` (P0847) folds the lvalue/rvalue overload pairs below.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
#  error "beman.inside requires GCC 14 or newer (deducing this)"
#endif

// Forward-declare the `beman::inside::math` entry points used in-class, so the bodies
// pass `-Wtemplate-body` without pulling cmath.hpp in unconditionally (its
// definitions live there).
namespace beman::inside::math
{
  template <insidable Out, insidable In> constexpr Out floor_impl(In x) noexcept;
  template <insidable Out, insidable In> constexpr Out ceil_impl (In x) noexcept;
  template <insidable Out, insidable In> constexpr Out round_impl(In x) noexcept;
  template <insidable Out, insidable In> constexpr Out trunc_impl(In x) noexcept;
  template <insidable Out, insidable In> constexpr Out abs_impl  (In x) noexcept;
  template <insidable In> constexpr auto floor(In x) noexcept;
  template <insidable In> constexpr auto ceil (In x) noexcept;
  template <insidable In> constexpr auto round(In x) noexcept;
  template <insidable In> constexpr auto trunc(In x) noexcept;
  template <insidable In> constexpr auto abs  (In x) noexcept;
}

//---------------------------------------------------------------------------
// inside — the public struct users include. Defines `inside<G, P>` and its
// per-instance operators; free-function arithmetic and `inside_range` also live
// here. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap), and
// generic.hpp/policy.hpp (traits + policy machinery).
//---------------------------------------------------------------------------
namespace beman::inside
{
  //---------------------------------------------------------------------------
  // inside
  //---------------------------------------------------------------------------
  template<grid G, policy_flag P>
  struct inside
  {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");
#ifndef BEMAN_INSIDE_MATH_FIXED
    // Under the default (double) engine the `real` policy is double-backed, and
    // its value snaps to the grid (Lower + k·Notch). That snap is only exact
    // when the grid is dyadic — power-of-two notch and Lower — so grid points
    // are representable in IEEE-754 double. A continuous grid (Notch == 0) has
    // no grid to snap to. Anything else is rejected here rather than silently
    // demoted to integer storage.
    static_assert(!has_flag(P, real) || detail::dyadic_grid<G> || G.Notch == 0,
                  "inside: the `real`/`f64` policy requires a dyadic grid (power-of-two "
                  "notch and Lower, so values are exactly representable in double)");
    static_assert(!has_flag(P, f32) || detail::dyadic_grid<G> || G.Notch == 0,
                  "inside: the `f32` policy requires a dyadic grid (power-of-two notch "
                  "and Lower); values must also fit float's 24-bit significand "
                  "(checked at storage selection — see `float_exact`)");
#endif
    // Representation flags vs grid shape (exact has no requirement; a result
    // policy may carry several flags — storage selection resolves widest-wins,
    // so no mutual-exclusion asserts here).
    static_assert(!has_flag(P, direct) || G.Notch == 1,
                  "inside: the `direct` policy (raw == value as a plain integer) "
                  "requires Notch == 1");
    static_assert(!has_flag(P, indexed) || G.Notch != 0,
                  "inside: the `indexed` policy (raw == 0-based notch index) "
                  "requires a notch (Notch != 0)");

    using negative = inside<-G, P>;
    using raw_type = detail::storage_for<G, P>;

    private:
    raw_type Raw;

    public:
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only an inside that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr raw_type const& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw()       noexcept
      requires (has_flag(P, unsafe)) { return Raw; }

    // Trivial default ctor — Raw is left uninitialized, like a built-in scalar: a
    // default-constructed inside has no value until assigned. (A previous checked
    // overload zero-filled Raw, which decoded to an out-of-range value or an invalid
    // {0,0} rational for grids not containing 0 — a defined-but-invalid footgun.
    // Value-init `inside{}` still zero-fills where a zero raw is genuinely wanted.)
    constexpr inside() = default;

    private:
    // Snap a value onto fp storage: lossless on the (fp-exact) dyadic grid — the
    // snap is computed in double and narrowed to the raw type (double or float),
    // which is exact because every grid point fits the raw's significand. Out-of-
    // range values run the same policy cascade as the fractional path (clamp →
    // wrap → checked-report → store as-is), with Pol's one-shot flags merged in.
    template <typename Pol>
    constexpr void store_fp(double v, Pol& pol)
    {
      constexpr policy_flag F = P | detail::policy_flags_of<std::remove_cvref_t<Pol>>;
      // NaN/±inf would reach snap_double's integer cast (UB); reject like the
      // non-real path. `v - v` is 0 for every finite v, NaN otherwise.
      if (!(v - v == 0))
        detail::raise(errc::not_finite, "non-finite double");
      const double lo = static_cast<double>(G.Interval.Lower);
      const double hi = static_cast<double>(G.Interval.Upper);
      if (v < lo || v > hi)
      {
        if constexpr (has_flag(F, clamp))
          v = v < lo ? lo : hi;
        else if constexpr (has_flag(F, wrap))
        {
          // Fold into [Lower, Lower + range), range = span + notch — the same
          // convention as the fractional apply_wrap. floor(q) without an
          // unguarded imax cast: for |q| >= 2^52 the double is already integral
          // (floor(q) == q), else narrow to imax (safe) and adjust toward -inf.
          const double range = hi - lo + static_cast<double>(G.Notch);
          const double q = (v - lo) / range;
          const double aq = q < 0 ? -q : q;
          double kd;
          if (aq >= 4503599627370496.0)            // 2^52
            kd = q;
          else
          {
            const imax k = static_cast<imax>(q);   // |q| < 2^52 < imax
            kd = static_cast<double>(k);
            if (q < 0 && kd != q) kd -= 1.0;        // floor toward -inf
          }
          v -= kd * range;
        }
        else if (detail::domain_fail(*this, pol))
          return;            // reported (error_code mode)
        // no handler (unchecked policy): fall through and store snapped as-is
      }
      Raw = static_cast<raw_type>(G.snap_double(v));   // narrow to float for f32 (lossless)
    }

    // The one store every constructor and assignment goes through; fp storage
    // takes the value as a double.
    template <numeric A, typename Pol>
    constexpr void store_value(A const& value, Pol&& pol)
    {
      if constexpr (!detail::fp_raw<inside>)
        detail::assignment<inside, A>::assign(*this, value, pol);
      else if constexpr (std::is_arithmetic_v<A>)
        store_fp(static_cast<double>(value), pol);
      else
        store_fp(static_cast<double>(detail::as_rational(value)), pol);
    }

    template <numeric A>
    constexpr void store_value(A const& value) { store_value(value, make_policy<P>()); }
    public:

    template <numeric A>
      requires inside_assignable<inside, A, P>
    constexpr inside(A value)
    { store_value(value); }

    // One-shot policy: `pol`'s flags widen the assignable check (a clamp/round
    // relaxes the interval/notch clause, e.g. clamp_round<B>(some_inside)) and
    // apply to this store. If it reports an error (ec mode), Raw is ill-defined.
    template <numeric A, typename Pol>
      requires inside_assignable<inside, A, P | detail::policy_flags_of<std::remove_cvref_t<Pol>>>
    constexpr inside(A value, Pol&& pol)
    { store_value(value, pol); }

    // Error-code construction: `inside x(value, ec)`. Needs its own overload (a raw
    // error_code would bind the Pol&& template above). On a reported (out-of-range)
    // error, ec is set and the inside's value is ill-defined — do not read it without
    // checking ec first.
    template <numeric A>
      requires inside_assignable<inside, A, P>
    constexpr inside(A value, errc& ec)
    { store_value(value, make_policy<P>(ec)); }

    // expected<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws
    // std::bad_expected_access on an error.
    template <numeric A>
      requires inside_assignable<inside, A, P>
    constexpr inside(std::expected<A, errc> const& value)
    { store_value(value.value()); }

    template <numeric B>
      requires inside_assignable<inside, B, P>
    constexpr inside& operator=(B const& other)
    { store_value(other); return *this; }

    template <numeric B>
      requires inside_assignable<inside, B, P>
    constexpr inside& operator=(std::expected<B, errc> const& other)
    { store_value(other.value()); return *this; }

    // ---- Diagnostic fallbacks (default on; -DBEMAN_INSIDE_STRICT_SFINAE removes them) ----
    // When a source is numeric but NOT assignable to this inside, every constrained
    // sink above drops out of overload resolution and the compiler emits a bare
    // "could not convert" — losing the reason. These complementary overloads (enabled
    // exactly when `inside_assignable` is false) catch that case and turn it into the
    // named per-clause notes from `inside_assignable_why` (interval-excludes /
    // incompatible-notch). The trade: they make `inside` *appear* is_constructible /
    // assignable from incompatible types (the static_assert is in the body, not the
    // immediate context, so trait probes return true then hard-error only on real
    // use). Define BEMAN_INSIDE_STRICT_SFINAE to drop them and restore SFINAE-pure traits for
    // metaprogramming that probes convertibility (variant/expected/`if constexpr`).
#ifndef BEMAN_INSIDE_STRICT_SFINAE
    template <numeric A>
      requires (!inside_assignable<inside, A, P>)
    constexpr inside(A)
    { static_assert(inside_assignable_why<inside, A, P>::value,
        "inside: cannot construct this inside from the value — see the per-clause notes above"); }

    template <numeric A>
      requires (!inside_assignable<inside, A, P>)
    constexpr inside(std::expected<A, errc> const&)
    { static_assert(inside_assignable_why<inside, A, P>::value,
        "inside: cannot construct this inside from the expected's value — see the per-clause notes above"); }

    template <numeric B>
      requires (!inside_assignable<inside, B, P>)
    constexpr inside& operator=(B const&)
    { static_assert(inside_assignable_why<inside, B, P>::value,
        "inside: cannot assign this value to this inside — see the per-clause notes above"); return *this; }

    template <numeric B>
      requires (!inside_assignable<inside, B, P>)
    constexpr inside& operator=(std::expected<B, errc> const&)
    { static_assert(inside_assignable_why<inside, B, P>::value,
        "inside: cannot assign this expected's value to this inside — see the per-clause notes above"); return *this; }
#endif

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr inside from_raw(raw_type r) noexcept
    { inside b; b.Raw = r; return b; }

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
    //                       (overflow / domain_error).
    //   as<T>()           — non-expected sibling; throws on error. For known-
    //                       in-range sites (array indexing). FP shares the gate.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    constexpr operator imax() const
      requires (detail::notch_is_unit_integer<G>
             && G.Interval.Lower >= detail::rational{std::numeric_limits<imax>::min()}
             && G.Interval.Upper <= detail::rational{std::numeric_limits<imax>::max()})
    { return detail::to_value(*this); }

    constexpr explicit(!has_flag(P, real) && !has_flag(P, f32)) operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return detail::as_double(*this); }

    constexpr operator detail::rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (!detail::index_raw<inside>)
        return Raw;

      // Q-format-with-integer-Lower fast path skips the generic path's three
      // rational ops. Falls through to the rational path when the raw is too wide
      // to widen safely (e.g. uint64 from a Q16.16 × Q16.16 result type).
      if constexpr (detail::HasQFormatFastPath<inside>)
        return detail::q_format_decode(*this);

      return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    // to<T>() — typed-error scalar extraction (mirrors rational::to<T>, extended
    // to signed and floating point). Returns `errc::overflow` (out of T's range;
    // `errc::domain_error` for a negative value into unsigned T); fractional
    // truncation is silent. Bounds the grid already satisfies are not checked.
    template <std::integral T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      using lim = std::numeric_limits<T>;
      constexpr bool check_lo = Lower<inside> < detail::rational{lim::min()};
      constexpr bool check_hi = Upper<inside> > detail::rational{lim::max()};
      if constexpr (!check_lo && !check_hi)
        return static_cast<T>(detail::to_value(*this));
      else
      {
        const auto r = detail::as_rational(*this);
        if (check_lo && r < detail::rational{lim::min()})
          return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
        if (check_hi && r > detail::rational{lim::max()})
          return std::unexpected{errc::overflow};
        return static_cast<T>(trunc(r));
      }
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const
    {
      return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict inside rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
      requires (!std::floating_point<T>
             || (P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return to<T>().value(); }

    // numerator() / denominator() — the exact value of a fractional inside as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
    [[nodiscard]] constexpr imax numerator() const   { return fraction().first; }
    [[nodiscard]] constexpr imax denominator() const { return fraction().second; }

    private:
    // The reduced exact value as {numerator, positive denominator}. Integer
    // grids need no division; dyadic Q-format grids reduce by shifting out
    // common factors of two instead of a gcd.
    constexpr std::pair<imax, imax> fraction() const
    {
      if constexpr (detail::index_raw<inside> && detail::IsIntegerAligned<inside>)
        return {detail::to_value(*this), 1};
      else if constexpr (detail::index_raw<inside> && detail::HasQFormatFastPath<inside>
                         && std::has_single_bit(detail::abs_den(G.Notch.Denominator)))
      {
        constexpr imax nd = detail::abs_den(G.Notch.Denominator);
        constexpr int  k  = std::countr_zero(static_cast<umax>(nd));
        const imax num = detail::raw_imax(*this) + detail::LowerImax<inside> * nd;
        const int  tz  = std::countr_zero(static_cast<umax>(num));   // num == 0: 64
        const int  s   = tz < k ? tz : k;
        return {num >> s, nd >> s};
      }
      else
      {
        auto r = detail::as_rational(*this);
        return {signed_numerator(r),
                static_cast<imax>(detail::abs_den(r.Denominator))};
      }
    }
    public:

    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `beman::inside::math` — `beman::inside::math::floor(b)` etc. (auto-deduced Out)
    // or `beman::inside::math::floor_impl<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<beman/inside/cmath.hpp>`.

    [[nodiscard]] constexpr negative operator-() const
    {
      negative neg;
      if constexpr (detail::fp_raw<inside>)
        neg = negative::from_raw(-Raw);
      else if constexpr (detail::rational_raw<inside>)
        neg = negative::from_raw(-(Raw));
      else if constexpr (!detail::index_raw<inside> || !detail::index_raw<negative>)
        detail::from_value(neg, -detail::to_value(*this));
      else
        // Unsigned-offset fast path: with `value = Raw*Notch + Lower`, negating
        // is `NotchCount - Raw` (index from the opposite end) — no rational ops.
        // Unreachable for direct storage, so `Raw` here is guaranteed an offset.
        neg = negative::from_raw(detail::raw_cast<negative>(detail::NotchCount<inside> - Raw));
      return neg;
    }

    // policy<F>() — per-operation policy override. On an lvalue it returns a
    // policy_ref holding `*this` by reference (needed so `b.policy<…>() = x` writes
    // back into b, and cheap for the common immediate use). On an *rvalue* receiver
    // it returns a policy_buffer that OWNS the moved-in value, so a snapped temporary
    // survives being returned/stored (`return (a*b).with_snap();`) — no dangling.
    template <policy_flag F = none, typename Self>
    [[nodiscard]] constexpr auto policy(this Self&& self)
    {
      auto pol = make_policy<P | F>();
      if constexpr (std::is_lvalue_reference_v<Self>)
        return detail::policy_ref<inside, decltype(pol)>{self, pol};
      else
        return detail::policy_buffer<inside, decltype(pol)>{std::move(self), pol};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(errc& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return detail::policy_ref<inside, decltype(pol)>{*this, pol};
    }

    // with_snap<Mode>() — opt this assignment into snapping with the given rounding
    // mode. Bare `with_snap()` is truncate-toward-zero (Mode == snap); pass an
    // explicit mode for the others: with_snap<round_nearest>(), <round_floor>,
    // <round_ceil>, <round_half_even>. Like policy<F>(), a temporary receiver
    // yields a value-owning policy_buffer.
    template <policy_flag Mode = snap, typename Self>
    [[nodiscard]] constexpr auto with_snap(this Self&& self)
    {
      static_assert(has_flag(Mode, snap),
        "with_snap<Mode>: Mode must be a snapping mode — snap (truncate), round_nearest, "
        "round_floor, round_ceil, or round_half_even");
      return std::forward<Self>(self).template policy<Mode>();
    }
    template <typename Self>
    [[nodiscard]] constexpr auto with_clamp(this Self&& self) { return std::forward<Self>(self).template policy<clamp>(); }
    template <typename Self>
    [[nodiscard]] constexpr auto with_wrap(this Self&& self)  { return std::forward<Self>(self).template policy<wrap>(); }

    private:
    // Shared builder for the single-action fluent hooks below (internal). Merges
    // the tag's implied policy flag, then returns a policy_ref bound to *this
    // carrying the tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action)
    {
       using tag = Tag<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | detail::implied_flags<tag>>();
       return detail::policy_ref<inside, decltype(pol), tag>{
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
       return detail::policy_ref<inside, decltype(pol), std::remove_cvref_t<Actions>...>{
         *this, pol,
         std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

    template <insidable R>
    constexpr inside& operator+=(R const& rhs)
    {
      // Point-inside rhs (just<v> / 1_ins / ++) whose value is a whole number of
      // this grid's notches: the raw delta is a compile-time constant and the
      // raw encoding cancels every Lower term (raw(v+d) = raw(v) + d/Notch for
      // offset and direct storage alike), so this compiles to one integer add.
      if constexpr (!detail::rational_raw<inside> && !detail::fp_raw<inside> && Notch<inside> != 0
                    && Lower<R> == Upper<R>
                    && (Lower<R> / Notch<inside>).has_value()
                    && detail::abs_den((*(Lower<R> / Notch<inside>)).Denominator) == 1)
      {
        constexpr imax delta = signed_numerator(*(Lower<R> / Notch<inside>));
        return store_raw(detail::raw_imax(*this) + delta);
      }
      // Fast path: raw-level integer addition, safe when raw_a + raw_b is the raw
      // of value_a + value_b — direct storage, or offset encoding with Lower==0 both.
      else if constexpr (!detail::rational_raw<inside> && !detail::rational_raw<R>
                    && !detail::fp_raw<inside> && !detail::fp_raw<R>
                    && Notch<inside> == Notch<R>
                    && (!detail::index_raw<R>
                        || (Lower<inside> == 0 && Lower<R> == 0)))
        return store_raw(detail::raw_imax(*this) + detail::raw_imax(rhs));
      else
        return *this = *this + rhs;
    }

    private:
    // Store a raw computed by the raw-space fast paths of += and -=. Under
    // clamp/wrap/checked an out-of-range raw is clamped, wrapped or reported.
    constexpr inside& store_raw(imax new_raw)
    {
      constexpr imax lo = detail::RawLo<inside>, hi = detail::RawHi<inside>;
      if constexpr (P & (clamp | wrap | checked))
        if (new_raw < lo || new_raw > hi)
        {
          if constexpr (P & clamp)
            new_raw = new_raw < lo ? lo : hi;
          else if constexpr (P & wrap)
            new_raw = detail::euclid_mod(new_raw - lo, hi - lo + 1) + lo;
          else
          {
            make_policy<P>().report(errc::domain_error);
            return *this;
          }
        }
      Raw = detail::raw_cast<inside>(new_raw);
      return *this;
    }
    public:

    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
    private:
    template <typename Result>
    constexpr inside& assign_op_result(Result const& r)
    {
      if constexpr (detail::is_expected_v<Result>)
      {
        // A failed op (an error) has already been reported through the policy
        // channel; keep *this unchanged instead of dereferencing — a
        // non-throwing installed handler must not turn into
        // bad_expected_access here. `*r` (not value()): no second check.
        if (r.has_value())
          *this = *r;
      }
      else
        *this = r;
      return *this;
    }

    constexpr inside& report_div_by_zero()
    {
      if constexpr (!(P & ignore_zero))
        make_policy<P>().report(errc::division_by_zero);
      return *this;
    }
    public:

    // Only a `rational` (a library type) may join an inside in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_ins` /
    // `just<1>` / `inside<{lo,hi}>{n}`), mirroring the binary operators.
    template <std::same_as<detail::rational> A>
    constexpr inside& operator+=(A const& rhs)
    { return assign_op_result(detail::rational{*this} + rhs); }

    template <insidable R>
    constexpr inside& operator-=(R const& rhs)
    {
      // Raw-space fast path, the subtraction mirror of +='s: with equal
      // notches, raw(v_l − v_r) = raw_l − raw_r − bias, where the bias is
      // Lower<R>/Notch for an index-raw rhs (its raw is Lower-relative) and 0
      // for a value-raw rhs. Delegating to `+= (-rhs)` instead shifts R's
      // Lower by negation and defeats +='s raw path for index-backed grids.
      if constexpr (!detail::rational_raw<inside> && !detail::rational_raw<R>
                    && !detail::fp_raw<inside> && !detail::fp_raw<R>
                    && Notch<inside> != 0 && Notch<inside> == Notch<R>
                    && (!detail::index_raw<R>
                        || ((Lower<R> / Notch<inside>).has_value()
                            && detail::abs_den((*(Lower<R> / Notch<inside>)).Denominator) == 1)))
      {
        constexpr imax bias = [] {
          if constexpr (detail::index_raw<R>) return signed_numerator(*(Lower<R> / Notch<inside>));
          else                                return imax{0};
        }();
        return store_raw(detail::raw_imax(*this) - detail::raw_imax(rhs) - bias);
      }
      else
        return *this += (-rhs);
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator-=(A const& rhs)
    { return assign_op_result(detail::rational{*this} - rhs); }

    template <insidable R>
    constexpr inside& operator*=(R const& rhs)
    { return assign_op_result(*this * rhs); }

    // The outer zero check is semantic, not redundant: the binary `a / b`
    // yields an error value on a zero divisor (expected vocabulary), so the compound
    // form's report comes from here. (Measured perf-neutral to remove.)
    template <insidable R>
    constexpr inside& operator/=(R const& rhs)
    {
      if (rhs == 0)
        return report_div_by_zero();
      return assign_op_result(*this / rhs);
    }

    template <insidable R>
    constexpr inside& operator%=(R const& rhs)
    {
      if (rhs == 0)
        return report_div_by_zero();
      return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator*=(A const& rhs)
    { return assign_op_result(detail::rational{*this} * rhs); }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator/=(A const& rhs)
    {
      if (detail::is_canonical_zero(rhs))
        return report_div_by_zero();
      return assign_op_result(detail::rational{*this} / rhs);
    }

    // ++/-- add the point inside `just<±1>` through the insidable += (which has
    // the raw-level integer fast path) instead of the rational round-trip,
    // which decodes to rational and re-stores through the full quotient/
    // rounding machinery (~30× the instructions on an integer grid). `just`
    // itself is declared after the class, so spell the point inside directly.
    constexpr inside& operator++()
    {
      // constexpr local: the point inside is materialised at compile time (the
      // ctor's error path otherwise blocks constant folding at -O3).
      constexpr auto one_b = inside<grid{detail::rational{1}}>{detail::rational{1}};
      return *this += one_b;
    }
    constexpr inside  operator++(int) { inside t = *this; ++*this; return t; }
    constexpr inside& operator--()
    {
      constexpr auto minus_one_b =
          inside<grid{detail::rational{-1}}>{detail::rational{-1}};
      return *this += minus_one_b;
    }
    constexpr inside  operator--(int) { inside t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value)
    {
      errc ec{};
      inside result;
      detail::assignment<inside, A>::assign(result, value, make_policy<P>(ec));
      if (ec != errc{}) return std::unexpected{ec};
      return result;
    }
  };

  //---------------------------------------------------------------------------
  // to<T>(b) / as<T>(b) — free-function forms, for generic code that would
  // otherwise need the `.template` disambiguator. Same semantics as the members.
  //---------------------------------------------------------------------------
  template <typename T, insidable B>
  [[nodiscard]] constexpr auto to(B const& b)
    requires requires { b.template to<T>(); }
  { return b.template to<T>(); }

  template <typename T, insidable B>
  [[nodiscard]] constexpr T as(B const& b)
    requires requires { b.template as<T>(); }
  { return b.template as<T>(); }

  //---------------------------------------------------------------------------
  // comparison
  //---------------------------------------------------------------------------
  namespace detail
  {
    // Integer value-index comparison eligibility: an integer-backed inside
    // whose value indices (value/Notch — integral by the grid anchor
    // invariant) fit imax, so two same-notch bounds compare as
    // `bias + raw` without a rational decode.
    template <insidable B>
    inline constexpr bool index_cmp_fits = []{
      if constexpr (rational_raw<B> || fp_raw<B> || Notch<B> == 0)
        return false;
      else
      {
        constexpr auto lo = Lower<B> / Notch<B>;
        constexpr auto hi = Upper<B> / Notch<B>;
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        return lo.has_value() && hi.has_value()
            && (*lo).Numerator <= cap && (*hi).Numerator <= cap;
      }
    }();

    // Signed value index of Raw == 0: Lower/Notch for offset (index) storage,
    // 0 for direct storage (raw is already the value == the index at notch 1).
    template <insidable B>
    inline constexpr imax index_cmp_bias = []{
      if constexpr (index_raw<B>)
      {
        constexpr auto lo = *(Lower<B> / Notch<B>);
        return signed_numerator(lo);
      }
      else
        return imax{0};
    }();
  }

  namespace detail
  {
    inline constexpr auto three_way = [](auto const& a, auto const& b) { return a <=> b; };
    inline constexpr auto equal_to  = [](auto const& a, auto const& b) { return a == b; };

    // inside ⋈ inside (⋈ = `cmp`: <=> or ==) in the cheapest exact form the two
    // storage shapes allow.
    template <insidable L, insidable R, class Cmp>
    constexpr auto compare(L const& lhs, R const& rhs, Cmp cmp)
    {
      // same grid: Raw is monotonically ordered regardless of storage kind
      if constexpr (Grid<L> == Grid<R>)
        return cmp(lhs.raw(), rhs.raw());
      // double-backed (`real`) operand: compare in double (raw_imax would truncate)
      else if constexpr (fp_raw<L> || fp_raw<R>)
        return cmp(as_double(lhs), as_double(rhs));
      // both integer-direct (notch=1, Raw==value): compare as integers
      else if constexpr (value_raw<L> && value_raw<R>)
        return cmp(raw_imax(lhs), raw_imax(rhs));
      // same nonzero notch, integer-backed: compare signed value indices
      // (compile-time bias + raw) — e.g. two same-Q-format fixed-point types
      // with different intervals, without the rational decode.
      else if constexpr (Notch<L> == Notch<R> && index_cmp_fits<L> && index_cmp_fits<R>)
        return cmp(index_cmp_bias<L> + raw_imax(lhs), index_cmp_bias<R> + raw_imax(rhs));
      else
        return cmp(as_rational(lhs), as_rational(rhs));
    }
  }

  template <insidable L, insidable R>
  constexpr auto operator<=>(L const& lhs, R const& rhs) { return detail::compare(lhs, rhs, detail::three_way); }

  template <insidable L, insidable R>
  constexpr bool operator==(L const& lhs, R const& rhs) { return detail::compare(lhs, rhs, detail::equal_to); }

  namespace detail
  {
    // inside ⋈ integral scalar without the rational decode: with the positive
    // notch n/d, value ⋈ c ⟺ (bias + raw)·n ⋈ c·d — both sides exact
    // integers (bias + raw is the signed value index, exact ordering AND
    // equality since c·d is exact too). Eligible when both cross terms
    // provably fit imax for every representable c of type A.
    template <insidable B, typename A>
    inline constexpr bool scalar_index_cmp_fits = []{
      if constexpr (!std::integral<A> || !index_raw<B> || !index_cmp_fits<B>)
        return false;
      else
      {
        constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
        constexpr umax notch_num = Notch<B>.Numerator;
        constexpr umax notch_den = static_cast<umax>(Notch<B>.Denominator); // Notch > 0
        constexpr umax index_mag = []{
          constexpr auto lo = *(Lower<B> / Notch<B>);
          constexpr auto hi = *(Upper<B> / Notch<B>);
          return lo.Numerator > hi.Numerator ? lo.Numerator : hi.Numerator;
        }();
        constexpr umax scalar_mag = []{
          umax mag = static_cast<umax>(std::numeric_limits<A>::max());
          if constexpr (std::signed_integral<A>)
          {
            umax min_mag = static_cast<umax>(
                -(std::numeric_limits<A>::min() + 1)) + 1;
            if (min_mag > mag) mag = min_mag;
          }
          return mag;
        }();
        umax product;
        return !mul_overflow(index_mag, notch_num, &product) && product <= cap
            && !mul_overflow(scalar_mag, notch_den, &product) && product <= cap;
      }
    }();
  }

  namespace detail
  {
    // inside ⋈ arithmetic scalar. Integer storage compares as integers when the
    // scalar's type fits imax, and in double when it is floating point and the
    // grid's values are exact in double; everything else goes through rational.
    template <insidable B, arithmetic A, class Cmp>
    constexpr auto compare_scalar(B const& lhs, A rhs, Cmp cmp)
    {
      constexpr bool imax_scalar = std::signed_integral<A> || (std::unsigned_integral<A> && sizeof(A) < sizeof(imax));
      constexpr bool double_exact_values = Lower<B> >= rational{-(imax{1} << 53)} && Upper<B> <= rational{imax{1} << 53};
      if constexpr (value_raw<B> && imax_scalar)
        return cmp(raw_imax(lhs), static_cast<imax>(rhs));
      else if constexpr (value_raw<B> && std::floating_point<A> && double_exact_values)
        return cmp(static_cast<double>(raw_imax(lhs)), static_cast<double>(rhs));
      else if constexpr (scalar_index_cmp_fits<B, A>)
        return cmp((index_cmp_bias<B> + raw_imax(lhs)) * static_cast<imax>(Notch<B>.Numerator),
                   static_cast<imax>(rhs) * Notch<B>.Denominator);
      else
        return cmp(as_rational(lhs), rational{rhs});
    }
  }

  template <insidable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs) { return detail::compare_scalar(lhs, rhs, detail::three_way); }

  template <insidable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs) { return detail::compare_scalar(lhs, rhs, detail::equal_to); }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = inside<grid{value}>{value};

  //---------------------------------------------------------------------------
  // zero / one — universal exact constants. Single-point bounds that assign into
  // any grid able to represent the value (compile-time checked) and otherwise
  // behave as 0 / 1. `b = zero;` is a compile error when 0 is not on b's grid.
  //---------------------------------------------------------------------------
  inline constexpr auto zero = just<0>;
  inline constexpr auto one  = just<1>;

  //---------------------------------------------------------------------------
  // _ins literal — compile-time `inside<{V, V}>` from a numeric literal.
  //   5_ins           // inside<{5, 5}>            integer
  //   1.25_ins        // inside<{rational{5,4}}>   decimal
  //   1.5e2_ins       // inside<{150}>             decimal scientific
  //   0xff_b        // inside<{255}>             hex integer
  //   0b1010_ins      // inside<{10}>              binary integer
  //   0x1p15_ins      // inside<{32768}>           hex with 2^N exponent (Q-format)
  //   0x1p-15_ins     // inside<{rational{1,32768}}>   1/2^15 grid notch
  //   0x1.8p3_ins     // inside<{12}>              hex float
  //   1'000_ins       // inside<{1000}>            digit separator
  //
  // Parse is exact (no double round-trip); same parser backs `_r` in
  // rational.hpp. `-1.5_ins` parses as `-(1.5_ins)`.
  //---------------------------------------------------------------------------
  template<char... Chars>
  constexpr auto operator""_ins() { return just<detail::_detail::parse_ins_literal<Chars...>()>; }

} // namespace beman::inside

#endif // BEMAN_INSIDE_CORE_HPP
