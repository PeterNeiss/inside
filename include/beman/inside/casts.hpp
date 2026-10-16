// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_CASTS_HPP
#define BEMAN_INSIDE_CASTS_HPP

#include <beman/inside/core.hpp>

//---------------------------------------------------------------------------
// Free-function casts complementing the constructors. Unlike a direct B{value}
// call, these read naturally in algorithm callbacks and make the intent (clamp
// vs. wrap vs. throw vs. trust) explicit at the call site.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
inline constexpr policy_flag rounding_flags = round_nearest | round_floor | round_ceil | round_half_even;
inline constexpr policy_flag range_flags    = clamp | wrap;

// B's grid under B's flags with the `Replaced` ones swapped for `Cast`: a
// one-shot flag overrides the declared one of its kind (OR-ing them would
// let the type's rounding mode outrank the cast's, or its clamp the cast's
// wrap). Same grid, so the raw carries over.
template <insidable B, policy_flag Cast, policy_flag Replaced>
using cast_twin = inside<grid_of<B>, (policy_of<B> & ~Replaced) | Cast>;
} // namespace detail

// Each cast constructs through B's grid with a one-shot policy that
// overrides B's declared one for this conversion only — and is constrained
// like that constructor, so a probe sees whether it compiles.
template <insidable B, numeric N>
    requires inside_assignable<detail::cast_twin<B, clamp, detail::range_flags>,
                               N,
                               policy_of<detail::cast_twin<B, clamp, detail::range_flags>>>
[[nodiscard]] constexpr B clamp_cast(N value) {
    return B::from_raw(detail::cast_twin<B, clamp, detail::range_flags>{value}.raw());
}

// `wrap_cast` — modular semantics: the input is reduced into the target grid's
// interval rather than clipped. For integer-style wraparound (angles, indices).
template <insidable B, numeric N>
    requires inside_assignable<detail::cast_twin<B, wrap, detail::range_flags>,
                               N,
                               policy_of<detail::cast_twin<B, wrap, detail::range_flags>>>
[[nodiscard]] constexpr B wrap_cast(N value) {
    return B::from_raw(detail::cast_twin<B, wrap, detail::range_flags>{value}.raw());
}

//---------------------------------------------------------------------------
// clamp_floor / clamp_ceil / clamp_round — compose `clamp` with a rounding
// mode: the canonical "double in, bounded integer out, never throw" pipeline.
// The mode replaces B's own.
//---------------------------------------------------------------------------
namespace detail {
template <insidable B, policy_flag RoundMode>
using clamp_round_twin = cast_twin<B, clamp | RoundMode, range_flags | rounding_flags>;

template <insidable B, policy_flag RoundMode, numeric N>
[[nodiscard]] constexpr B clamp_with_rounding(N value) {
    return B::from_raw(clamp_round_twin<B, RoundMode>{value}.raw());
}
} // namespace detail

template <insidable B, numeric N>
    requires inside_assignable<detail::clamp_round_twin<B, round_floor>,
                               N,
                               policy_of<detail::clamp_round_twin<B, round_floor>>>
[[nodiscard]] constexpr B clamp_floor(N value) {
    return detail::clamp_with_rounding<B, round_floor>(value);
}

template <insidable B, numeric N>
    requires inside_assignable<detail::clamp_round_twin<B, round_ceil>,
                               N,
                               policy_of<detail::clamp_round_twin<B, round_ceil>>>
[[nodiscard]] constexpr B clamp_ceil(N value) {
    return detail::clamp_with_rounding<B, round_ceil>(value);
}

template <insidable B, numeric N>
    requires inside_assignable<detail::clamp_round_twin<B, round_nearest>,
                               N,
                               policy_of<detail::clamp_round_twin<B, round_nearest>>>
[[nodiscard]] constexpr B clamp_round(N value) {
    return detail::clamp_with_rounding<B, round_nearest>(value);
}

// `checked_cast` — throws (via the installed handler) when the value would not
// fit exactly: errc::overflow out of the interval (as to<T> and the predicate
// name it), errc::rounding_error off the notch. Any numeric source, insides
// included; once both checks pass the store is exact.
template <insidable B, numeric A>
    requires inside_assignable<B, A, policy_of<B> | snap>
[[nodiscard]] constexpr B checked_cast(A value) {
    if (conversion_overflows<B>(value))
        detail::raise(errc::overflow, "checked_cast: value out of inside interval");
    if (conversion_rounds<B>(value))
        detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value, make_policy<snap>()};
}

// `unchecked_cast` routes through `inside<G, unsafe>` so the compiler elides
// every domain/round check. UB if the value is actually out of range.
template <insidable B, numeric A>
    requires inside_assignable<inside<grid_of<B>, unsafe>, A, unsafe>
[[nodiscard]] constexpr B unchecked_cast(A value) {
    using twin = inside<grid_of<B>, unsafe>;
    return B::from_raw(twin{value}.raw()); // same grid → identical raw layout
}

} // namespace beman::inside

#endif // BEMAN_INSIDE_CASTS_HPP
