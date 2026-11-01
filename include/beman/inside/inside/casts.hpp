// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_CASTS_HPP
#define BEMAN_INSIDE_INSIDE_CASTS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/compare.hpp> // the inside type and its comparisons

//---------------------------------------------------------------------------
// checked_cast<B>(v): v stored into B exactly, or a report - whatever B's
// policy says about rounding or the range. (The other one-shot stores are the
// flag constructor: B{v, clamp}, B{v, wrap | round_floor}, B{v, unsafe}.)
//---------------------------------------------------------------------------
namespace beman::inside {
// `checked_cast` — throws (via the installed handler) when the value would not
// fit exactly: errc::overflow out of the interval (as to<T> and the predicate
// name it), errc::rounding_error off the notch. Any numeric source, insides
// included; once both checks pass the store is exact.
template <insidable B, numeric A>
    requires inside_assignable<B, A, flags_of<B> | snap>
[[nodiscard]] constexpr B checked_cast(A value) {
    if (conversion_overflows<B>(value))
        detail::raise(errc::overflow, "checked_cast: value out of inside interval");
    if (conversion_rounds<B>(value))
        detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value, make_policy<snap>()};
}

} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_CASTS_HPP
