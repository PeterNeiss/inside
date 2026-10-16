// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_CASTS_HPP
#define BEMAN_INSIDE_CASTS_HPP

#include <beman/inside/detail/compare.hpp> // the inside type and its comparisons

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
using cast_twin = inside<grid_of<B>, (flags_of<B> & ~Replaced) | Cast>;
} // namespace detail

// Each cast constructs through B's grid with a one-shot policy that
// overrides B's declared one for this conversion only — and is constrained
// like that constructor, so a probe sees whether it compiles.
namespace detail {
// clamp, and with a rounding mode, that mode in place of B's own:
// clamp_cast<B, round_floor> is the "double in, bounded integer out, never
// throw" pipeline.
template <insidable B, policy_flag Round>
using clamp_twin = std::conditional_t<Round == none,
                                      cast_twin<B, clamp, range_flags>,
                                      cast_twin<B, clamp | Round, range_flags | rounding_flags>>;

template <policy_flag Round>
concept cast_rounding =
    Round == none || Round == round_nearest || Round == round_floor || Round == round_ceil || Round == round_half_even;
} // namespace detail

template <insidable B, policy_flag Round = none, numeric N>
    requires detail::cast_rounding<Round> &&
             inside_assignable<detail::clamp_twin<B, Round>, N, flags_of<detail::clamp_twin<B, Round>>>
[[nodiscard]] constexpr B clamp_cast(N value) {
    return B::from_raw(detail::clamp_twin<B, Round>{value}.raw());
}

// `wrap_cast` — modular semantics: the input is reduced into the target grid's
// interval rather than clipped. For integer-style wraparound (angles, indices).
template <insidable B, numeric N>
    requires inside_assignable<detail::cast_twin<B, wrap, detail::range_flags>,
                               N,
                               flags_of<detail::cast_twin<B, wrap, detail::range_flags>>>
[[nodiscard]] constexpr B wrap_cast(N value) {
    return B::from_raw(detail::cast_twin<B, wrap, detail::range_flags>{value}.raw());
}

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
