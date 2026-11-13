// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_GENERIC_HPP
#define BEMAN_INSIDE_INSIDE_GENERIC_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/call_policy.hpp>   // error_ref
#include <beman/inside/inside/detail/introspection.hpp> // grid_of, flags_of, insidable, numeric
#include <beman/inside/inside/detail/storage.hpp>       // storage concepts, raw/value plumbing

#include <type_traits>

//---------------------------------------------------------------------------
// generic — the type-level layer every other header builds on: the public
// introspection (inside/detail/introspection.hpp), the storage shapes
// (inside/detail/storage.hpp), and here the inside_assignable concept with its
// diagnostics.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// The two non-trivial clauses of `inside_assignable`, named so the concept and
// its `inside_assignable_why` diagnostic share one definition. Concepts (not
// bools) so `||` short-circuits *instantiation* (e.g. notches_compatible<L,R>
// is never formed when R isn't insidable).
template <typename L, typename R, policy_flag P = checked>
concept assign_intervals_ok =
    (!insidable<R> && !std::integral<R>)
    // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
    // for them (the integral-rhs path already allows it — int's interval is unbounded).
    || has_any_flag(flags_of<L> | P, wrap | clamp) || not excludes(interval_of<L>, interval_of<R>);

template <typename L, typename R, policy_flag P>
concept assign_notch_ok = !insidable<R> || has_flag(flags_of<L> | P, snap) || notches_compatible<L, R>;

// Every value of R lies on L's lattice (or within L's MaxDenominator), so a
// store of an R into L never reports rounding_error. A double, a rational or
// a grid number carries no lattice in its type: never.
template <typename L, typename R>
consteval bool values_on_lattice() {
    if constexpr (insidable<R>) {
        if constexpr (point_grid<R> && notched<L>)
            return point_on_lattice<L, R>; // (the range is values_in_range's question)
        else if constexpr (point_grid<R>)
            return grid_of<L>.MaxDenominator == 0 ||
                   !(grid_numerator(grid_of<L>.MaxDenominator) < grid_denominator(lower_of<R>));
        else if constexpr (notched<L>)
            return notched<R> && notches_compatible<L, R>;
        else if constexpr (grid_of<L>.MaxDenominator == 0)
            return true;
        else if constexpr (!notched<R>) // continuous into continuous: the bounds
            return grid_of<R>.MaxDenominator != 0 && !(grid_of<L>.MaxDenominator < grid_of<R>.MaxDenominator);
        else // a lattice: every value's denominator divides den(Lower)·den(Notch)
            return !(grid_numerator(grid_of<L>.MaxDenominator) <
                     grid_denominator(lower_of<R>) * grid_denominator(notch_of<R>));
    } else if constexpr (std::integral<R>) {
        if constexpr (notched<L>)
            return grid_divides_evenly(grid_rational{1}, notch_of<L>) &&
                   grid_same_lattice(grid_rational{0}, lower_of<L>, notch_of<L>);
        else
            return true;
    } else
        return false;
}

// Every value of R lies in L's interval, or the flags F clamp or wrap it.
template <typename L, typename R, policy_flag F>
consteval bool values_in_range() {
    if constexpr (insidable<R> || std::integral<R>)
        return has_any_flag(F, clamp | wrap) || includes(interval_of<L>, interval_of<R>);
    else
        return false; // a double may be NaN; a rational or grid number has no interval in its type
}

// store_proven<L, R, P>: the grids prove that a store of any R into L under
// P cannot fail at run time — the range and the notch both hold, or P is
// `unsafe` (the fit is then a precondition). A store that is not proven
// needs a channel (an errc&, an on_error hook, try_make), or a constant.
template <typename L, typename R, policy_flag P = flags_of<L>>
concept store_proven =
    !is_checked(flags_of<L> | P) ||
    (values_in_range<L, R, flags_of<L> | P>() && (has_flag(flags_of<L> | P, snap) || values_on_lattice<L, R>()));

// A policy that reports into an errc& (policy(ec), make_policy<F>(ec)).
template <typename Pol>
inline constexpr bool reports_into_errc = std::is_base_of_v<error_ref, std::remove_cvref_t<Pol>>;

// R's notch for a diagnostic (0 for a non-inside, which never fails there).
template <typename R>
consteval grid_rational notch_or_zero() {
    if constexpr (insidable<R>)
        return notch_of<R>;
    else
        return grid_rational{0};
}
} // namespace detail

// Compile-time prerequisites for L = R, gating three failure modes at the call
// site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
// skipped for float/rational, which have no static interval); (3) integer
// notch ratio or snap set (else R's notch doesn't divide L's; opt into
// rounding). Named `inside_assignable` to avoid shadowing std::assignable_from.
template <typename L, typename R, policy_flag P = checked>
concept inside_assignable =
    detail::numeric<R> && detail::assign_intervals_ok<L, R, P> && detail::assign_notch_ok<L, R, P>;

// Diagnostic helper: instantiating `inside_assignable_why<L,R,P>` fires a named
// static_assert per failed clause, with the two grids' intervals and notches.
// Backs the public `explain_assignable` probe below; inside's deleted stores
// (inside/detail/core.hpp) name only the clause, as a deletion's reason is a fixed string.
namespace detail {
template <typename L, typename R, policy_flag P = checked>
struct inside_assignable_why {
    // Collapse each clause to a plain bool *before* the static_assert. Asserting on
    // a concept-id makes GCC dump the whole satisfaction tree ("constraints not
    // satisfied / no operand of the disjunction…") on top of the message; a bool
    // condition prints just the message. Each clause is self-guarding (the inner
    // disjunctions gate `notches_compatible<L,R>` on `insidable<R>`), so evaluating
    // all three unconditionally is safe even when R is not numeric.
    static constexpr bool is_numeric   = numeric<R>;
    static constexpr bool intervals_ok = detail::assign_intervals_ok<L, R, P>;
    static constexpr bool notch_ok     = detail::assign_notch_ok<L, R, P>;
    static_assert(is_numeric, "inside_assignable: rhs is not numeric (must be an inside or arithmetic type)");
    static_assert(
        intervals_ok,
        detail::diag_text{} << "inside_assignable: rhs interval lies entirely outside lhs interval and the policy "
                               "(not wrap/clamp) cannot bring it into range — assignment can never succeed (lhs ["
                            << interval_of<L>.Lower << ", " << interval_of<L>.Upper << "], rhs ["
                            << interval_of<R>.Lower << ", " << interval_of<R>.Upper << "])");
    static_assert(
        notch_ok,
        detail::diag_text{} << "inside_assignable: incompatible notches — use `policy<round_toward_zero>()` or a "
                               "rounding policy to allow rounding (lhs notch "
                            << notch_of<L> << ", rhs notch " << detail::notch_or_zero<R>() << ")");
    static constexpr bool value = inside_assignable<L, R, P>;
};
} // namespace detail

// Public manual probe: `static_assert(beman::inside::explain_assignable<DstInside, decltype(src)>);`
// emits the named per-clause reasons with the grids' numbers.
template <typename Dst, typename Src, policy_flag P = flags_of<Dst>>
inline constexpr bool explain_assignable = detail::inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_GENERIC_HPP
