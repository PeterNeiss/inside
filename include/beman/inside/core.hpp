// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
// Defines the core `beman::inside::inside<G, P>` type; the umbrella adds the free-function
// casts/arithmetic/range layers that depend on this complete type.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CORE_HPP
#define BEMAN_INSIDE_CORE_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/lift.hpp>
#include <beman/inside/policy.hpp>
#include <beman/inside/detail/addition.hpp>
#include <beman/inside/detail/multiplication.hpp>
#include <beman/inside/detail/division.hpp>
#include <beman/inside/detail/assignment.hpp>
#include <beman/inside/predicates.hpp>

#include <bit> // std::countr_zero, std::has_single_bit
#include <optional>
#include <expected> // std::expected, std::unexpected
#include <iterator> // std::incrementable_traits, std::indirectly_readable_traits
#include <utility>  // std::pair

// Forward-declare the `beman::inside::math` entry points used in-class, so the bodies
// pass `-Wtemplate-body` without pulling cmath.hpp in unconditionally (its
// definitions live there).
namespace beman::inside::math {
template <insidable Out, insidable In>
constexpr Out floor_into(In x);
template <insidable Out, insidable In>
constexpr Out ceil_into(In x);
template <insidable Out, insidable In>
constexpr Out round_into(In x);
template <insidable Out, insidable In>
constexpr Out trunc_into(In x);
namespace detail {
// abs_into's constraint (cmath.hpp): Out includes the smallest |x|.
template <class Out, class In>
consteval bool abs_into_ok();
} // namespace detail
template <insidable Out, insidable In>
    requires(detail::abs_into_ok<Out, In>())
constexpr Out abs_into(In x);
template <insidable In>
constexpr auto floor(In x);
template <insidable In>
constexpr auto ceil(In x);
template <insidable In>
constexpr auto round(In x);
template <insidable In>
constexpr auto trunc(In x);
template <insidable In>
constexpr auto abs(In x);
} // namespace beman::inside::math

//---------------------------------------------------------------------------
// inside — defines `inside<G, P>` and its member operators. Free-function
// arithmetic (arithmetic.hpp), casts (casts.hpp) and `inside_range` (range.hpp)
// follow in the umbrella. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap), generic.hpp
// (inside_assignable, over detail/introspection.hpp and detail/storage.hpp)
// and policy.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
#if BEMAN_INSIDE_MSVC
namespace detail {
// just<N>.per<D> on MSVC: a compiler bug reads a class-type static member
// variable template through an object wrongly, so .per<D> is this
// placeholder, which no copy, conversion or operator accepts.
struct msvc_per_unsupported {
    constexpr msvc_per_unsupported() = default;
    BEMAN_INSIDE_DELETE_BEGIN
    msvc_per_unsupported(const msvc_per_unsupported&) =
        BEMAN_INSIDE_DELETE("just<N>.per<D> does not work on MSVC (a compiler bug) - write just<N, D>");
    template <class T>
    operator T() const =
        BEMAN_INSIDE_DELETE("just<N>.per<D> does not work on MSVC (a compiler bug) - write just<N, D>");
    BEMAN_INSIDE_DELETE_END
};
} // namespace detail
#endif
namespace detail {
// What `*t` yields for a cursor on G: G without its end point, no cursor — a
// value of T, which assigns into T.
template <grid G, policy_flag P>
using cursor_value_t =
    inside<grid{interval{G.Interval.Lower, grid_sub(G.Interval.Upper, G.Notch)}, G.Notch}, P & ~cursor_marker>;
} // namespace detail

//---------------------------------------------------------------------------
// inside
//---------------------------------------------------------------------------
template <grid G, policy_flag P>
struct inside {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");

    using negated_type = inside<-G, P & ~detail::cursor_marker>; // −cursor is no cursor
    using raw_type     = detail::raw_for_t<G>;

  private:
    BEMAN_INSIDE_NO_UNIQUE_ADDRESS raw_type Raw; // empty for a point grid

  public:
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only an inside that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr const raw_type& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw() noexcept
        requires(has_flag(P, unsafe))
    {
        return Raw;
    }

    // Trivial default ctor — Raw is left uninitialized, like a built-in scalar: a
    // default-constructed inside has no value until assigned. (A zero raw would
    // decode to an out-of-range value on grids without 0.) Value-init `inside{}`
    // still zero-fills where a zero raw is genuinely wanted.
    constexpr inside()
        requires(!has_flag(P, detail::cursor_marker))
    = default;

    // A cursor starts at Lower: slot 0.
    constexpr inside()
        requires(has_flag(P, detail::cursor_marker))
        : Raw{detail::raw_from_offset<inside>(umax{0})} {}

  private:
    // The one store every constructor and assignment goes through — the same
    // for every raw kind.
    template <numeric A, typename Pol>
    constexpr void store_value(const A& value, Pol&& pol) {
        detail::assignment<inside, A>::assign(*this, value, pol);
    }

    template <numeric A>
    constexpr void store_value(const A& value) {
        store_value(value, make_policy<P>());
    }

  public:
    // (Raw starts zeroed: a store that rounds a fine double starts from the
    // destination's value, which a constant expression may not read
    // uninitialized. The optimizer drops the dead store.)
    template <numeric A>
        requires inside_assignable<inside, A, P>
    constexpr inside(A value) : Raw{} {
        store_value(value);
    }

    // One-shot policy: `pol`'s flags widen the assignable check (a clamp/round
    // relaxes the interval/notch clause, e.g. clamp_cast<B, round_nearest>(some_inside)) and
    // apply to this store. If it reports an error (ec mode), Raw is ill-defined.
    template <numeric A, typename Pol>
        requires inside_assignable<inside, A, P | flags_of<Pol>>
    constexpr inside(A value, Pol&& pol) : Raw{} {
        store_value(value, pol);
    }

    // No error-code constructor: construction that can fail is `try_make(value)`,
    // which returns expected<inside, errc>. This overload only turns
    // `inside x(value, ec)` into a readable error (an errc& would otherwise bind
    // the Pol&& constructor above).
    template <numeric A>
    constexpr inside(A, errc&) {
        static_assert(detail::dependent_false<A>,
                      "inside(value, errc&) was removed: use `auto r = B::try_make(value);` "
                      "(expected<B, errc>), or `b.policy(ec) = value` to assign with an error code");
    }

    // expected<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws
    // std::bad_expected_access on an error.
    template <numeric A>
        requires inside_assignable<inside, A, P>
    constexpr inside(const std::expected<A, errc>& value) : Raw{} {
        store_value(value.value());
    }

    template <numeric B>
        requires inside_assignable<inside, B, P>
    constexpr inside& operator=(const B& other) {
        store_value(other);
        return *this;
    }

    template <numeric B>
        requires inside_assignable<inside, B, P>
    constexpr inside& operator=(const std::expected<B, errc>& other) {
        store_value(other.value());
        return *this;
    }

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
        requires(!inside_assignable<inside, A, P>)
    constexpr inside(A) {
        static_assert(inside_assignable_why<inside, A, P>::value,
                      "inside: cannot construct this inside from the value — see the per-clause notes above");
    }

    template <numeric A>
        requires(!inside_assignable<inside, A, P>)
    constexpr inside(const std::expected<A, errc>&) {
        static_assert(
            inside_assignable_why<inside, A, P>::value,
            "inside: cannot construct this inside from the expected's value — see the per-clause notes above");
    }

    template <numeric B>
        requires(!inside_assignable<inside, B, P>)
    constexpr inside& operator=(const B&) {
        static_assert(inside_assignable_why<inside, B, P>::value,
                      "inside: cannot assign this value to this inside — see the per-clause notes above");
        return *this;
    }

    template <numeric B>
        requires(!inside_assignable<inside, B, P>)
    constexpr inside& operator=(const std::expected<B, errc>&) {
        static_assert(inside_assignable_why<inside, B, P>::value,
                      "inside: cannot assign this expected's value to this inside — see the per-clause notes above");
        return *this;
    }
#endif

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr inside from_raw(raw_type r) noexcept {
        inside b;
        b.Raw = r;
        return b;
    }

    // just<N>.per<D> — the point N/D (D >= 1), the same as just<N, D>, which
    // is the portable spelling (see MSVC below). Only on an integer point that
    // fits imax. (In dependent code: `just<N>.template per<D>`.)
    template <umax D>
        requires(G.Interval.Lower == G.Interval.Upper && detail::fits_imax(G.Interval) &&
                 detail::wide_denominator(G.Interval.Lower) == detail::grid_int{1})
#if BEMAN_INSIDE_MSVC
    // MSVC reads a class-type static member variable template through an
    // object wrongly (an error in a constant expression, a wrong value at run
    // time), so .per<D> is a placeholder there that no expression accepts.
    static constexpr detail::msvc_per_unsupported per{};
#else
    static constexpr inside<grid{detail::grid_mul(G.Interval.Lower, detail::grid_rational{::beman::inside::per<D>})}>
        per{};
#endif

    // Conversion summary:
    //   operator imax     — implicit, when the values are integers or the policy
    //                       may round (then rounded by its mode), and the interval
    //                       fits int64 (else use `to<imax>()`). Also the `vec[b]`
    //                       index path. No second implicit integer operator (would
    //                       make `imax_var += b` ambiguous).
    //   operator rational — implicit; lossless and exact.
    //   operator double   — explicit, and gated on a rounding flag (a double
    //                       may round the value).
    //                       A strict inside opts in via `to<double>().value()`.
    //   to<T>()           — typed-error narrowing/widening → `expected<T, errc>`
    //                       (overflow / domain_error).
    //   as<T>()           — non-expected sibling; throws on error. For known-
    //                       in-range sites (array indexing). FP shares the gate.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    constexpr operator imax() const
        requires((detail::integer_notch<G> || (has_flag(P, snap) && detail::rational_readable<inside>)) &&
                 G.Interval.Lower >= detail::rational{std::numeric_limits<imax>::min()} &&
                 G.Interval.Upper <= detail::rational{std::numeric_limits<imax>::max()})
    {
        if constexpr (detail::integer_notch<G>)
            return detail::to_value(*this);
        else
            return round_to_int(detail::as_rational(*this), detail::rounding_of(P));
    }

    constexpr explicit operator double() const
        requires(has_flag(P, snap))
    {
        return detail::as_double(*this);
    }

    // Unavailable on a wide-index grid: its values outgrow the 64-bit rational
    // (compare it, or read it with to<T>()).
    constexpr operator detail::rational() const
        requires(detail::rational_readable<inside>)
    {
        if constexpr (G.Interval.Lower == G.Interval.Upper)
            return G.Interval.Lower;
        else if constexpr (detail::value_storage<inside>)
            return Raw;
        // Q-format-with-integer-Lower fast path skips the generic path's three
        // rational ops. Falls through to the rational path when a value times
        // the notch's denominator could pass imax (e.g. a Q16.16 × Q16.16 result).
        else if constexpr (detail::qformat_codec_fits<inside>)
            return detail::q_format_decode(*this);
        else {
            // Raw·Notch can pass 64 bits on the way to a value that fits.
            const auto scaled = detail::try_mul(rational{Raw}, detail::notch64<inside>);
            const auto value  = scaled ? detail::try_add(*scaled, detail::lower64<inside>) : scaled;
            if (value) [[likely]]
                return *value;
            if (const auto exact = detail::try_rational(detail::exact_of(*this)))
                return *exact;
            // (Not reached for a rational_readable grid; reported, as any
            // overflow, rather than a bad_expected_access.)
            detail::raise(errc::overflow, "inside: value past the 64-bit rationals");
        }
    }

    // to<T>() — typed-error scalar extraction (mirrors rational::to<T>, extended
    // to signed and floating point). Returns `errc::overflow` (out of T's range;
    // `errc::domain_error` for a negative value into unsigned T); fractional
    // truncation is silent. Bounds the grid already satisfies are not checked.
    template <std::integral T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const {
        using lim               = std::numeric_limits<T>;
        constexpr bool check_lo = lower_of<inside> < detail::rational{lim::min()};
        constexpr bool check_hi = upper_of<inside> > detail::rational{lim::max()};
        if constexpr (!check_lo && !check_hi && detail::values_fit_imax<inside>)
            return static_cast<T>(detail::to_value(*this));
        else if constexpr (detail::wide_valued<inside>) { // truncate first: -0.5 → unsigned is 0
            const auto t = trunc(detail::exact_of(*this));
            if (check_lo && t < decltype(t){lim::min()})
                return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if (check_hi && t > decltype(t){lim::max()})
                return std::unexpected{errc::overflow};
            return static_cast<T>(t);
        } else {
            const auto t = detail::round_to_integral(detail::as_rational(*this), detail::round_mode::trunc);
            if (check_lo && t < detail::rational{lim::min()})
                return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if (check_hi && t > detail::rational{lim::max()})
                return std::unexpected{errc::overflow};
            return static_cast<T>(detail::signed_numerator(t));
        }
    }

    // Rounded once to T's digits: a floating point wider than double (long
    // double) from the exact value; a narrower one (float) through the double,
    // unless that double is a tie between two Ts.
    template <std::floating_point T>
    [[nodiscard]] constexpr std::expected<T, errc> to() const {
        if constexpr (detail::wide_float<T>)
            return detail::nearest_float<T>(detail::exact_of(*this));
        else if constexpr (sizeof(T) < sizeof(double)) {
            const double d = detail::as_double(*this);
            if (detail::narrow_midpoint<T>(d)) [[unlikely]]
                return detail::nearest_float<T>(detail::exact_of(*this));
            return static_cast<T>(d);
        } else
            return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict inside rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
        requires(std::integral<T> || std::floating_point<T>) && (!std::floating_point<T> || has_flag(P, snap))
    {
        return to<T>().value();
    }

    // numerator() / denominator() — the exact value in lowest terms as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
    // imax when every value fits it, else a wide integer that holds every value
    // (a wide grid, a grid past int64).
    [[nodiscard]] constexpr auto numerator() const {
        if constexpr (detail::fraction_fits_imax<inside>)
            return fraction().first;
        else
            return detail::reduced_exact(*this).Num;
    }
    [[nodiscard]] constexpr auto denominator() const {
        if constexpr (detail::fraction_fits_imax<inside>)
            return fraction().second;
        else
            return detail::reduced_exact(*this).Den;
    }

  private:
    // The reduced exact value as {numerator, positive denominator}. Integer
    // grids need no division; dyadic Q-format grids reduce by shifting out
    // common factors of two instead of a gcd.
    constexpr std::pair<imax, imax> fraction() const {
        if constexpr (detail::integer_storage<inside> && detail::integer_lattice<inside>)
            return {detail::to_value(*this), 1};
        else if constexpr (detail::index_storage<inside> && detail::qformat_codec_fits<inside> &&
                           std::has_single_bit(detail::abs_den(detail::notch64<inside>.Denominator))) {
            constexpr imax nd  = detail::abs_den(detail::notch64<inside>.Denominator);
            const imax     num = detail::raw_imax(*this) + detail::lower_imax<inside> * nd;
            const int      s   = std::countr_zero(static_cast<umax>(num) | static_cast<umax>(nd)); // 0 → 0/1
            return {num >> s, nd >> s};
        } else {
            auto r = detail::as_rational(*this);
            return {signed_numerator(r), static_cast<imax>(detail::abs_den(r.Denominator))};
        }
    }

  public:
    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `beman::inside::math` — `beman::inside::math::floor(b)` etc. (auto-deduced Out)
    // or `beman::inside::math::floor_into<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<beman/inside/cmath.hpp>`.

    [[nodiscard]] constexpr negated_type operator-() const {
        negated_type neg;
        if constexpr (detail::point_storage<inside>)
            neg = negated_type::from_raw({});        // −point is a point: no raw
        else if constexpr (!detail::notched<inside>) // continuous: the raw is the value
            neg = negated_type::from_raw(-(Raw));
        else {
            // Integer raws: the negated value index is −J (wide_value.hpp), in imax
            // when the bounds allow, else by wrapping. Index storage on both sides
            // counts the slot from the opposite end instead.
            using W = detail::index_work_t<negated_type, inside, G.Notch, inside, G.Notch>;
            if constexpr (detail::index_storage<inside> && detail::index_storage<negated_type>) {
                constexpr W count = static_cast<W>(G.slot_count());
                neg = negated_type::from_raw(static_cast<detail::raw_t<negated_type>>(count - static_cast<W>(Raw)));
            } else
                neg = detail::from_value_index<negated_type>(W{0} - detail::value_index<W>(*this));
        }
        return neg;
    }

    // policy<F>() — per-operation policy override. On an lvalue it returns a
    // policy_ref holding `*this` by reference (needed so `b.policy<…>() = x` writes
    // back into b, and cheap for the common immediate use). On an *rvalue* receiver
    // it returns a policy_buffer that OWNS the moved-in value, so a snapped temporary
    // survives being returned/stored (`return (a*b).with_snap();`) — no dangling.
    template <policy_flag F = none, typename Self>
    [[nodiscard]] constexpr auto policy(this Self&& self) {
        auto pol = make_policy<P | F>();
        if constexpr (std::is_lvalue_reference_v<Self>)
            return detail::policy_ref<inside, decltype(pol)>{self, pol, {}};
        else
            return detail::policy_buffer<inside, decltype(pol)>{std::move(self), pol, {}};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(errc& ec) {
        auto pol = make_policy<P | F>(ec);
        return detail::policy_ref<inside, decltype(pol)>{*this, pol, {}};
    }

    // with_snap<Mode>() — opt this assignment into snapping with the given rounding
    // mode. Bare `with_snap()` is truncate-toward-zero (Mode == snap); pass an
    // explicit mode for the others: with_snap<round_nearest>(), <round_floor>,
    // <round_ceil>, <round_half_even>. Like policy<F>(), a temporary receiver
    // yields a value-owning policy_buffer.
    template <policy_flag Mode = snap, typename Self>
    [[nodiscard]] constexpr auto with_snap(this Self&& self) {
        static_assert(has_flag(Mode, snap),
                      "with_snap<Mode>: Mode must be a snapping mode — snap (truncate), round_nearest, "
                      "round_floor, round_ceil, or round_half_even");
        return std::forward<Self>(self).template policy<Mode>();
    }
    template <typename Self>
    [[nodiscard]] constexpr auto with_clamp(this Self&& self) {
        return std::forward<Self>(self).template policy<clamp>();
    }
    template <typename Self>
    [[nodiscard]] constexpr auto with_wrap(this Self&& self) {
        return std::forward<Self>(self).template policy<wrap>();
    }

  private:
    // Shared builder for the single-action fluent hooks below (internal). Merges
    // the tag's implied policy flag, then returns a policy_ref bound to *this
    // carrying the tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action) {
        using tag = Tag<std::remove_cvref_t<A>>;
        auto pol  = make_policy<P | detail::implied_flags<tag>>();
        return detail::policy_ref<inside, decltype(pol), tag>{*this, pol, tag{std::forward<A>(action)}};
    }

  public:
    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& a) {
        return make_action_ref<on_wrap_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& a) {
        return make_action_ref<on_clamp_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& a) {
        return make_action_ref<on_error_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_overflow(A&& a) {
        return make_action_ref<on_overflow_t>(std::forward<A>(a));
    }

    // Multi-action entry point: combine N tagged actions into one policy_ref.
    // policy_ref rejects mutually exclusive combinations at compile time. E.g.
    // `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs` — the arithmetic fires λ1,
    // the narrowing back into b fires λ2.
    template <typename... Actions>
    [[nodiscard]] constexpr auto with(Actions&&... actions) {
        constexpr policy_flag merged = detail::merged_implied_flags<Actions...>;
        auto                  pol    = make_policy<P | merged>();
        return detail::policy_ref<inside, decltype(pol), std::remove_cvref_t<Actions>...>{
            *this, pol, std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

  private:
    // Raw-space fast paths of += and -=. Each adds a delta to the raw: a
    // compile-time constant (point rhs), or ±rhs raw plus a constant bias.
    // With equal notches, raw(v_l ± v_r) = raw_l ± (raw_r + bias): the bias is
    // Lower/Notch of an index-raw rhs (its raw is Lower-relative), else 0.
    //   point_delta<R>   — rhs is one whole number of notches: the delta.
    //   raw_add_ok<R>    — rhs raw adds or subtracts with that bias.
    // The add runs in raw_work_t, sized from the raw and delta ranges: imax for
    // every grid within int64, a wide_int beyond — never overflowing.
    template <insidable R>
    static constexpr bool point_delta_ok =
        detail::notched<inside> && detail::point_grid<R> &&
        (detail::wide_numerator(lower_of<R>) * detail::wide_denominator(notch_of<inside>)) %
                (detail::wide_denominator(lower_of<R>) * detail::wide_numerator(notch_of<inside>)) ==
            detail::grid_int{0};

    // (R anchored: its Lower is a whole number of notches, the bias.)
    template <insidable R>
    static constexpr bool raw_add_ok = detail::notched<inside> && notch_of<inside> == notch_of<R> &&
                                       !detail::point_storage<R> && detail::anchored<R>; // ⇒ integer raws

    template <insidable R>
    static constexpr detail::grid_int point_delta = detail::exact_quotient(lower_of<R>, notch_of<inside>);
    template <insidable R>
    static constexpr detail::grid_int add_bias = [] {
        if constexpr (detail::index_storage<R>)
            return detail::slot_base<R>;
        else
            return detail::grid_int{0};
    }();

    // Work type of a raw-space add whose delta lies in [Dlo, Dhi]: it holds
    // every raw of this grid, the delta, the new raw (in [raw_lo + Dlo,
    // raw_hi + Dhi]), the wrap range raw_hi − raw_lo + 1 and the new raw's
    // offset from raw_lo (wrap divides it), so the add cannot overflow. imax
    // for every grid within int64.
    // (A variable template, not a function: Clang would instantiate a plain
    // member function's body while the class is still incomplete.)
    template <detail::grid_int Dlo, detail::grid_int Dhi>
    static constexpr int raw_work_bits = [] {
        using W        = detail::grid_int;
        constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
        return detail::signed_value_bits_of({lo, hi, Dlo, Dhi, lo + Dlo, hi + Dhi, hi - lo + W{1}, hi - lo + Dhi});
    }();
    template <detail::grid_int Dlo, detail::grid_int Dhi>
    using raw_work_t = detail::work_int_t<raw_work_bits<Dlo, Dhi>>;

    // Work type of the raw delta ±(rhs.raw() + bias) itself: it holds R's raws,
    // their negations and the bias, so neither step overflows before the delta
    // (in raw_work_t's range) is formed.
    template <insidable R>
    using delta_work_t = detail::work_int_t<detail::signed_value_bits_of({detail::raw_lo_exact<R>,
                                                                          detail::raw_hi_exact<R>,
                                                                          -detail::raw_lo_exact<R>,
                                                                          -detail::raw_hi_exact<R>,
                                                                          add_bias<R>,
                                                                          -add_bias<R>,
                                                                          detail::raw_lo_exact<R> + add_bias<R>,
                                                                          detail::raw_hi_exact<R> + add_bias<R>,
                                                                          -detail::raw_lo_exact<R> - add_bias<R>,
                                                                          -detail::raw_hi_exact<R> - add_bias<R>})>;

    // A compound op's constraint: its binary op's result (an expected's
    // value) assigns to this type, so `requires { x op= y; }` is honest.
    template <typename Result>
    static constexpr bool compound_assignable = inside_assignable<inside, detail::unwrap_t<Result>, P>;

  public:
    template <insidable R>
        requires(point_delta_ok<R> || raw_add_ok<R> ||
                 compound_assignable<decltype(std::declval<const inside&>() + std::declval<const R&>())>)
    constexpr inside& operator+=(const R& rhs) {
        // Point-inside rhs (just<v> / 1_ins / ++) whose value is a whole number of
        // this grid's notches: the raw delta is a compile-time constant and the
        // raw encoding cancels every Lower term (raw(v+d) = raw(v) + d/Notch for
        // index and value raws alike), so this compiles to one integer add.
        if constexpr (point_delta_ok<R>) {
            using W           = raw_work_t<point_delta<R>, point_delta<R>>;
            constexpr W delta = static_cast<W>(point_delta<R>);
            return store_raw<W>(static_cast<W>(Raw) + delta);
        } else if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<detail::raw_lo_exact<R> + add_bias<R>, detail::raw_hi_exact<R> + add_bias<R>>;
            using D = delta_work_t<R>;
            constexpr D bias = static_cast<D>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(static_cast<D>(rhs.raw()) + bias));
        } else
            return assign_op_result(*this + rhs);
    }

  private:
    // Store a raw computed by the raw-space fast paths of += and -=, in their
    // work type W. Under clamp/wrap/checked an out-of-range raw is clamped,
    // wrapped or reported.
    template <detail::raw_integer W>
    constexpr inside& store_raw(W new_raw) {
        constexpr W lo = static_cast<W>(detail::raw_lo_exact<inside>);
        constexpr W hi = static_cast<W>(detail::raw_hi_exact<inside>);
        if constexpr (range_handled(P))
            if (new_raw < lo || new_raw > hi) {
                if constexpr (P & clamp)
                    new_raw = new_raw < lo ? lo : hi;
                else if constexpr (P & wrap)
                    new_raw = lo + detail::floor_divmod(new_raw - lo, hi - lo + W{1}).Rem;
                else {
                    make_policy<P>().report(errc::overflow);
                    return *this;
                }
            }
        Raw = static_cast<raw_type>(new_raw);
        return *this;
    }

    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
    template <typename Result>
    constexpr inside& assign_op_result(const Result& r) {
        if constexpr (detail::expected_like<Result>) {
            // A failed op is reported through this type's policy (throw / handler);
            // *this stays unchanged. `*r` (not value()): no bad_expected_access.
            if (r.has_value())
                *this = *r;
            else
                make_policy<P>().report(r.error());
        } else
            *this = r;
        return *this;
    }

    // A zero divisor in /= or %=; ignore_zero on either operand silences it,
    // as it does for div/mod.
    template <insidable R>
    constexpr inside& report_div_by_zero() {
        if constexpr (!has_flag(P | flags_of<R>, ignore_zero))
            make_policy<P>().report(errc::division_by_zero);
        return *this;
    }

    // A rational on a Q-format grid's lattice as a signed notch count (rhs·K
    // for notch 1/K), when it is a whole number of notches of moderate size.
    static constexpr bool raw_delta_ok = detail::qformat_codec_fits<inside> && detail::values_fit_imax<inside>;
    // The largest count it takes: store_raw forms Raw ± n and, to wrap, its
    // offset from raw_lo, so n leaves room for the raws and their span in imax.
    static constexpr umax max_notch_delta = [] {
        using W        = detail::grid_int;
        constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
        const W     mag  = lo.negative() && hi < -lo ? -lo : hi;
        const W     room = W{std::numeric_limits<imax>::max()} - (hi - lo + W{1}) - mag;
        return room.negative() ? umax{0} : static_cast<umax>(room);
    }();
    static constexpr std::optional<imax> notch_delta(const detail::rational& r) noexcept {
        constexpr umax K = detail::abs_den(detail::notch64<inside>.Denominator);
        const umax     d = detail::abs_den(r.Denominator);
        if (K % d != 0 || r.Numerator > max_notch_delta / (K / d))
            return std::nullopt;
        const imax n = static_cast<imax>(r.Numerator * (K / d));
        return r.Denominator < 0 ? -n : n;
    }

  public:
    // Only a `rational` (a library type) may join an inside in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_ins` /
    // `just<1>` / `inside<{lo,hi}>{n}`), mirroring the binary operators.

    template <std::same_as<detail::rational> A>
    constexpr inside& operator+=(const A& rhs) {
        if constexpr (raw_delta_ok)
            if (const auto n = notch_delta(rhs)) [[likely]]
                return store_raw<imax>(static_cast<imax>(Raw) + *n);
        return assign_op_result(detail::rational{*this} + rhs);
    }

    template <insidable R>
        requires(raw_add_ok<R> ||
                 compound_assignable<decltype(std::declval<const inside&>() - std::declval<const R&>())>)
    constexpr inside& operator-=(const R& rhs) {
        // Raw-space fast path, the subtraction mirror of +='s.
        if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<-detail::raw_hi_exact<R> - add_bias<R>, -detail::raw_lo_exact<R> - add_bias<R>>;
            using D = delta_work_t<R>;
            constexpr D bias = static_cast<D>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(-static_cast<D>(rhs.raw()) - bias));
        } else
            return *this += (-rhs);
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator-=(const A& rhs) {
        if constexpr (raw_delta_ok)
            if (const auto n = notch_delta(rhs)) [[likely]]
                return store_raw<imax>(static_cast<imax>(Raw) - *n);
        return assign_op_result(detail::rational{*this} - rhs);
    }

    template <insidable R>
        requires compound_assignable<decltype(std::declval<const inside&>() * std::declval<const R&>())>
    constexpr inside& operator*=(const R& rhs) {
        return assign_op_result(*this * rhs);
    }

    // The outer zero check is semantic, not redundant: the binary `a / b`
    // yields an error value on a zero divisor (expected vocabulary), so the compound
    // form's report comes from here. (Measured perf-neutral to remove.)
    template <insidable R>
        requires compound_assignable<decltype(std::declval<const inside&>() / std::declval<const R&>())>
    constexpr inside& operator/=(const R& rhs) {
        if (rhs == 0)
            return report_div_by_zero<R>();
        return assign_op_result(*this / rhs);
    }

    template <insidable R>
        requires compound_assignable<
            decltype(mod(std::declval<const inside&>(), std::declval<const R&>(), make_policy<P>()))>
    constexpr inside& operator%=(const R& rhs) {
        if (rhs == 0)
            return report_div_by_zero<R>();
        return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator*=(const A& rhs) {
        return assign_op_result(detail::rational{*this} * rhs);
    }

    template <std::same_as<detail::rational> A>
    constexpr inside& operator/=(const A& rhs) {
        if (detail::is_canonical_zero(rhs))
            return report_div_by_zero<inside>();
        return assign_op_result(detail::rational{*this} / rhs);
    }

    // expected<inside> RHS (e.g. `x += a / b`): unwrap once, reporting an error
    // through this type's policy like any other failed compound op.
    template <insidable R>
        requires requires(inside& self, const R& r) { self += r; }
    constexpr inside& operator+=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this += *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self -= r; }
    constexpr inside& operator-=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this -= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self *= r; }
    constexpr inside& operator*=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this *= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self /= r; }
    constexpr inside& operator/=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this /= *rhs) : report_error(rhs.error());
    }
    template <insidable R>
        requires requires(inside& self, const R& r) { self %= r; }
    constexpr inside& operator%=(const std::expected<R, errc>& rhs) {
        return rhs ? (*this %= *rhs) : report_error(rhs.error());
    }

  private:
    constexpr inside& report_error(errc e) {
        make_policy<P>().report(e);
        return *this;
    }

  public:
    // ++/-- move one notch: they add the point inside of ±Notch through the
    // insidable += (which has the raw-level integer fast path) instead of the
    // rational round-trip (~30× the instructions on an integer grid). On a
    // notch-1 grid that is ±1; on a cursor, one step. `just` itself is declared
    // after the class, so spell the point inside directly.
    constexpr inside& operator++() {
        static_assert(G.Notch != 0, "inside: ++ steps one notch - a grid without a notch has none to step");
        // constexpr local: the point inside is materialized at compile time.
        constexpr auto notch_step = inside<grid{G.Notch}>::from_raw({});
        return *this += notch_step;
    }
    constexpr inside operator++(int) {
        inside t = *this;
        ++*this;
        return t;
    }
    constexpr inside& operator--() {
        static_assert(G.Notch != 0, "inside: -- steps one notch - a grid without a notch has none to step");
        constexpr auto notch_step = inside<grid{-G.Notch}>::from_raw({});
        return *this += notch_step;
    }
    constexpr inside operator--(int) {
        inside t = *this;
        --*this;
        return t;
    }

    // *t: a cursor's value without the end, so a cursor is its own iterator
    // and end(t) its sentinel. At the end it reports out of range.
    [[nodiscard]] constexpr auto operator*() const
        requires(has_flag(P, detail::cursor_marker))
    {
        return detail::cursor_value_t<G, P>{*this};
    }

    // try_make<F>(value): the constructors' store under this type's policy plus
    // the flags F, with the failure as the error. An expected value passes its
    // error on, so fallible steps chain.
    template <policy_flag F = none, numeric A>
        requires inside_assignable<inside, A, P | F>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value) {
        errc   ec{};
        inside result{};
        result.store_value(value, make_policy<P | F>(ec));
        if (ec != errc{})
            return std::unexpected{ec};
        return result;
    }
    template <policy_flag F = none, numeric A>
        requires inside_assignable<inside, A, P | F>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(const std::expected<A, errc>& value) {
        if (!value)
            return std::unexpected{value.error()};
        return try_make<F>(*value);
    }
};

//---------------------------------------------------------------------------
// to<T>(b) / as<T>(b) — free-function forms, for generic code that would
// otherwise need the `.template` disambiguator. Same semantics as the members.
//---------------------------------------------------------------------------
template <typename T, insidable B>
[[nodiscard]] constexpr auto to(const B& b)
    requires requires { b.template to<T>(); }
{
    return b.template to<T>();
}

template <typename T, insidable B>
[[nodiscard]] constexpr T as(const B& b)
    requires requires { b.template as<T>(); }
{
    return b.template as<T>();
}

//---------------------------------------------------------------------------
// parse<B, F>(first, last) — text → B, exactly (no double round-trip). The
// whole range must be one number: an optional sign, then the literal grammar
// (1'000, 1.25, 1.5e2, 0xff, 0b1010, 0x1.8p3) or a fraction N/D. Malformed text
// is errc::invalid_format; the value then goes through B::try_make<F>, so B's
// policy plus the flags F round, clamp or wrap it and report overflow /
// rounding_error. parse_exact<B> accepts only a value B holds exactly:
// off the grid is rounding_error and out of range overflow, whatever B's
// policy. (io.hpp adds std::string_view overloads and operator>>.)
//---------------------------------------------------------------------------
namespace detail {
// parse past the 64-bit rational (a long decimal, a wide grid's value,
// 1e-200): the text parsed exactly, or past that width as a tiny or huge
// value's stand-in. Out of line: the common parse stays small.
template <insidable B, policy_flag F>
BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE constexpr std::expected<B, errc> parse_wide(const char* first,
                                                                                    const char* last) {
    constexpr std::size_t K = exact_max<exact_limbs<B>, parse_limbs>;
    auto                  w = parse_exact<K>(first, last);
    if (!w && w.error() == errc::overflow)
        if (const auto t = stand_in<B, K, static_cast<bool>((flags_of<B> | F) & wrap)>(first, last))
            w = *t;
    if (!w)
        return std::unexpected{w.error()};
    errc ec{};
    B    b{};
    assign_exact<rational>(b, *w, make_policy<flags_of<B> | F>(ec), no_action{});
    if (ec != errc{})
        return std::unexpected{ec};
    return b;
}
} // namespace detail

template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> parse(const char* first, const char* last) {
    const auto v = detail::parse_text(first, last);
    if (!v && v.error() == errc::overflow) [[unlikely]]
        return detail::parse_wide<B, F>(first, last);
    if (!v)
        return std::unexpected{v.error()};
    return B::template try_make<F>(*v);
}

template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> parse_exact(const char* first, const char* last) {
    const auto v = detail::parse_text(first, last);
    if (v) {
        if (conversion_overflows<B>(*v))
            return std::unexpected{errc::overflow};
        if (conversion_rounds<B>(*v))
            return std::unexpected{errc::rounding_error};
        return B::template try_make<snap>(*v); // on the grid: nothing rounds
    }
    if (v.error() != errc::overflow)
        return std::unexpected{v.error()};
    constexpr std::size_t K = detail::exact_max<detail::exact_limbs<B>, detail::parse_limbs>;
    auto                  w = detail::parse_exact<K>(first, last);
    if (!w && w.error() == errc::overflow)
        if (const auto t = detail::stand_in<B, K>(first, last))
            w = *t; // never on the grid: rounding_error, or overflow outside it
    if (!w)
        return std::unexpected{w.error()};
    if (*w < detail::exact_of_grid<K>(lower_of<B>) || detail::exact_of_grid<K>(upper_of<B>) < *w)
        return std::unexpected{errc::overflow};
    if constexpr (detail::notched<B>) {
        if (!detail::exact_index<B, detail::round_mode::trunc>(*w).Exact)
            return std::unexpected{errc::rounding_error};
    } else if constexpr (grid_of<B>.MaxDenominator != 0) {
        // A bounded continuous grid holds denominators up to its bound only.
        if (static_cast<detail::wide_sint<K>>(detail::wide_numerator(grid_of<B>.MaxDenominator)) <
            detail::reduced(*w).Den)
            return std::unexpected{errc::rounding_error};
    }
    errc ec{};
    B    b{};
    detail::assign_exact<detail::rational>(b, *w, make_policy<flags_of<B>>(ec), no_action{});
    if (ec != errc{})
        return std::unexpected{ec};
    return b;
}

//---------------------------------------------------------------------------
// just
//---------------------------------------------------------------------------
// The point grid holds the value, so no value constructor is needed: grid
// numbers past 64 bits (C++26 `_g`) work too. just<N, D> is the point N/D
// for an integer D ≥ 1: just<1, 3>, just<-6, 5> for -1.2 (as just<N>.per<D>).
namespace detail {
template <auto value, auto den>
consteval auto just_point() {
    if constexpr (std::integral<decltype(den)> && den == 1)
        return inside<grid{value}>::from_raw({});
    else {
        static_assert(std::integral<decltype(den)> && den >= 1, "just<N, D>: D is an integer of at least 1");
        return inside<grid{grid_div_of(grid_rational{value}, grid_rational{den})}>::from_raw({});
    }
}
} // namespace detail
template <auto value, auto den = 1>
inline constexpr auto just = detail::just_point<value, den>();

//---------------------------------------------------------------------------
// _ins literal — compile-time `inside<{V, V}>` from a numeric literal.
//   5_ins           // inside<{5, 5}>            integer
//   1.25_ins        // inside<{rational{5,4}}>   decimal
//   1.5e2_ins       // inside<{150}>             decimal scientific
//   0xff_ins        // inside<{255}>             hex integer
//   0b1010_ins      // inside<{10}>              binary integer
//   0x1p15_ins      // inside<{32768}>           hex with 2^N exponent (Q-format)
//   0x1p-15_ins     // inside<{rational{1,32768}}>   1/2^15 grid notch
//   0x1.8p3_ins     // inside<{12}>              hex float
//   1'000_ins       // inside<{1000}>            digit separator
//
// Parse is exact (no double round-trip); same parser backs `_r` in
// detail/number_text.hpp. `-1.5_ins` parses as `-(1.5_ins)`.
//---------------------------------------------------------------------------
template <char... Chars>
constexpr auto operator""_ins() {
    return just<detail::parse_ins_literal<Chars...>()>;
}

} // namespace beman::inside

// A cursor is a bidirectional iterator: it steps by ++/--, *t reads its value,
// and end(t) is its sentinel. Not random access: t - u is arithmetic, an
// inside, not a distance. Plain insides get no traits, so they stay numbers.
template <beman::inside::grid G, beman::inside::policy_flag P>
    requires(beman::inside::has_flag(P, beman::inside::detail::cursor_marker))
struct std::incrementable_traits<beman::inside::inside<G, P>> {
    using difference_type = beman::inside::imax;
};

template <beman::inside::grid G, beman::inside::policy_flag P>
    requires(beman::inside::has_flag(P, beman::inside::detail::cursor_marker))
struct std::indirectly_readable_traits<beman::inside::inside<G, P>> {
    using value_type = beman::inside::detail::cursor_value_t<G, P>;
};

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_CORE_HPP
