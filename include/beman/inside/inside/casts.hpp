// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_CASTS_HPP
#define BEMAN_INSIDE_INSIDE_CASTS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/compare.hpp> // the inside type and its comparisons

//---------------------------------------------------------------------------
// exact_cast<B>(v): v stored into B exactly, or a report - whatever B's
// policy says about rounding or the range. (The other one-shot stores are the
// flag constructor: B{v, clamp}, B{v, wrap | round_floor}, B{v, unsafe}.)
//---------------------------------------------------------------------------
namespace beman::inside {
// `exact_cast<B>(v)` — v stored into B exactly, whatever B's policy says
// about rounding or the range: B itself where the grids prove every v fits
// B's interval and lands on its notch, else a std::expected<B, error> holding
// error::overflow (out of the interval, as to<T> and the predicate name it)
// or error::rounding_error (off the notch). Any numeric source, insides
// included.
template <insidable B, detail::numeric A>
    requires inside_assignable<B, A, policy_of<B> | detail::snap>
[[nodiscard]] constexpr auto exact_cast(A value) {
    if constexpr (detail::values_in_range<B, A, detail::none>() && detail::values_on_lattice<B, A>())
        return detail::stored<B, policy_of<B> | detail::snap>(value);
    else {
        using result = std::expected<B, error>;
        if constexpr (std::floating_point<A>)
            if (!detail::is_finite(value))
                return result{std::unexpect, error::not_finite};
        if (conversion_overflows<B>(value))
            return result{std::unexpect, error::overflow};
        if (conversion_rounds<B>(value))
            return result{std::unexpect, error::rounding_error};
        return result{detail::stored<B, policy_of<B> | detail::snap>(value)}; // both checks passed: exact
    }
}

} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_CASTS_HPP
