// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_INTROSPECTION_HPP
#define BEMAN_INSIDE_DETAIL_INTROSPECTION_HPP

#include <beman/inside/detail/debug.hpp>
#include <beman/inside/lift.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

#include <algorithm>
#include <bit>
#include <initializer_list>

//---------------------------------------------------------------------------
// introspection — an inside's template parameters as values (grid_of<B>,
// policy_of<B>, lower_of<B>, notch_of<B>, … and their 64-bit forms), the
// insidable / numeric concepts, and the basic plumbing every layer uses
// (plain_t, dependent_false, as_rational, raw_t). The storage shapes build
// on it in detail/storage.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
template <grid G = grid{{0, 0}, 0}, policy_flag P = checked>
struct inside;

template <class>
inline constexpr bool is_inside_v = false;
template <grid G, policy_flag P>
inline constexpr bool is_inside_v<inside<G, P>> = true;

template <typename B>
concept insidable = is_inside_v<std::remove_cvref_t<B>>;

//---------------------------------------------------------------------------
// Public grid/policy introspection — extract an inside's template parameters.
// These mirror std::numeric_limits: they report what the grid is, used
// opaquely (the rational return type is never named by callers).
//---------------------------------------------------------------------------
namespace detail {
template <typename B>
struct inside_params;
template <grid G, policy_flag P>
struct inside_params<inside<G, P>> {
    static constexpr grid        grid_v   = G;
    static constexpr policy_flag policy_v = P;
};
} // namespace detail

template <insidable B>
inline constexpr grid grid_of = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

template <insidable B>
inline constexpr policy_flag policy_of = detail::inside_params<std::remove_cvref_t<B>>::policy_v;

template <typename T>
inline constexpr interval interval_of = {0, 0};

template <insidable B>
inline constexpr interval interval_of<B> = grid_of<B>.Interval;

template <std::integral I>
inline constexpr interval interval_of<I> = {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

template <insidable B>
inline constexpr detail::grid_rational lower_of = grid_of<B>.Interval.Lower;
template <insidable B>
inline constexpr detail::grid_rational upper_of = grid_of<B>.Interval.Upper;
template <insidable B>
inline constexpr detail::grid_rational notch_of = grid_of<B>.Notch;

namespace detail {
// The same as 64-bit rationals, for the paths that work in 64 bits (the
// integer fast paths, the math engines). A grid number past 64 bits fails
// the build here, never truncates. Under C++23 they equal lower_of & co.
template <insidable B>
inline constexpr rational lower64 = to_rational(grid_of<B>.Interval.Lower);
template <insidable B>
inline constexpr rational upper64 = to_rational(grid_of<B>.Interval.Upper);
template <insidable B>
inline constexpr rational notch64 = to_rational(grid_of<B>.Notch);
} // namespace detail

template <typename N>
concept numeric = insidable<N> or detail::arithmetic<N>;

//---------------------------------------------------------------------------
// Internal plumbing — storage shape, raw/value conversion, dispatch.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
using plain_t = std::remove_cvref_t<T>;

// Always-false but template-dependent: lets a `static_assert` inside a
// template body fire only when that template is actually instantiated
// (e.g. the guidance overloads that make `inside + 1` ill-formed).
template <typename...>
inline constexpr bool dependent_false = false;

//-------------------------------------------------------------------------
// Conversion-helper legend — the value/raw plumbing reused across the
// engine. "value space" = the number an inside denotes; "raw space" = how it
// is stored (see §2 "Storage encoding" in docs/internals.md). Use this to
// tell the similarly-named helpers apart:
//
//   as_rational(x)         value → rational    exact view of a scalar or inside
//   as_double(b)           raw   → double       kind-aware decode; lossy off dyadic grids
//   to_value(b)            raw   → imax         the inside's integer value (decodes an index)
//   from_value(b, v)       imax  → raw          store integer value v into b (inverse of to_value)
//   raw_cast<B>(x)         x     → raw_t<B>     TYPE cast only — no value arithmetic
//   raw_imax(b)            raw   → imax         widen the raw bits (NOT the value for index storage)
//   raw_from_offset<B>(o)  index → raw_t<B>     adds raw_lo (Lower for a value raw, 0 for an index)
//   raw_of_slot<B>(o)      index → raw_t<B>     any storage, any width (detail/wide_value.hpp)
//-------------------------------------------------------------------------

// Uniform rational view of a scalar or inside (rational{v} / operator rational()).
template <numeric N>
[[nodiscard]] constexpr rational as_rational(N v) {
    if constexpr (arithmetic<N>)
        return rational{v};
    else
        return v;
}

// Canonical-zero test for a divisor. rational stores zero as {0, 1}, so
// Numerator == 0 catches it regardless of representation; other types compare
// against their own zero.
template <typename T>
[[nodiscard]] constexpr bool is_canonical_zero(const T& v) {
    if constexpr (std::same_as<T, rational>)
        return v.Numerator == 0;
    else
        return v == T{0};
}

template <insidable B>
using raw_t = typename B::raw_type;
} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_INTROSPECTION_HPP
