// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_CASTS_HPP
#define BEMAN_INSIDE_INSIDE_CASTS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/compare.hpp> // the inside type and its comparisons

//---------------------------------------------------------------------------
// checked_cast<B>(v): v stored into B exactly, or a report - whatever B's
// policy says about rounding or the range. (The other one-shot stores are the
// flag constructor: B{v, clamp}, B{v, wrap | round_floor}, B{v, unsafe}.)
//---------------------------------------------------------------------------
namespace beman::inside {
// `checked_cast<B>(v)` — v stored into B exactly, whatever B's policy says
// about rounding or the range: B itself where the grids prove every v fits
// B's interval and lands on its notch, else a std::expected<B, errc> holding
// errc::overflow (out of the interval, as to<T> and the predicate name it)
// or errc::rounding_error (off the notch). Any numeric source, insides
// included.
template <insidable B, detail::numeric A>
    requires inside_assignable<B, A, flags_of<B> | detail::snap>
[[nodiscard]] constexpr auto checked_cast(A value) {
    if constexpr (detail::values_in_range<B, A, detail::none>() && detail::values_on_lattice<B, A>())
        return detail::stored<B, flags_of<B> | detail::snap>(value);
    else {
        using result = std::expected<B, errc>;
        if constexpr (std::floating_point<A>)
            if (!detail::is_finite(value))
                return result{std::unexpect, errc::not_finite};
        if (conversion_overflows<B>(value))
            return result{std::unexpect, errc::overflow};
        if (conversion_rounds<B>(value))
            return result{std::unexpect, errc::rounding_error};
        return result{detail::stored<B, flags_of<B> | detail::snap>(value)}; // both checks passed: exact
    }
}

} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_CASTS_HPP
