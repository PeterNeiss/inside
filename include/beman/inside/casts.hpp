// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CASTS_HPP
#define BEMAN_INSIDE_CASTS_HPP

#include <beman/inside/core.hpp>

//---------------------------------------------------------------------------
// Free-function casts complementing the constructors. Unlike a direct B{value}
// call, these read naturally in algorithm callbacks and make the intent (clamp
// vs. wrap vs. throw vs. trust) explicit at the call site.
//---------------------------------------------------------------------------
namespace beman::inside
{
  // Each cast constructs via the value+policy constructor, passing a one-shot
  // policy that overrides B's declared one for this conversion only.
  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_cast(N value)
  { return B{value, make_policy<clamp>()}; }

  // `wrap_cast` — modular semantics: the input is reduced into the target grid's
  // interval rather than clipped. For integer-style wraparound (angles, indices).
  template <insidable B, numeric N>
  [[nodiscard]] constexpr B wrap_cast(N value)
  { return B{value, make_policy<wrap>()}; }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round — compose `clamp` with a rounding
  // mode: the canonical "double in, bounded integer out, never throw" pipeline.
  //---------------------------------------------------------------------------
  template <insidable B, policy_flag RoundMode, numeric N>
  [[nodiscard]] constexpr B clamp_with_rounding(N value)
  { return B{value, make_policy<clamp | RoundMode>()}; }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_floor(N value)
  { return clamp_with_rounding<B, round_floor>(value); }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_ceil(N value)
  { return clamp_with_rounding<B, round_ceil>(value); }

  template <insidable B, numeric N>
  [[nodiscard]] constexpr B clamp_round(N value)
  { return clamp_with_rounding<B, round_nearest>(value); }

  // `checked_cast` — throws (via the installed handler) when the value would not
  // fit exactly: errc::overflow out of the interval (as to<T> and the predicate
  // name it), errc::rounding_error off the notch. Any numeric source, insides
  // included; once both checks pass the store is exact.
  template <insidable B, numeric A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (conversion_overflows<B>(value))
      detail::raise(errc::overflow, "checked_cast: value out of inside interval");
    if (conversion_rounds<B>(value))
      detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value, make_policy<snap>()};
  }

  // `unchecked_cast` routes through `inside<G, unsafe>` so the compiler elides
  // every domain/round check. UB if the value is actually out of range.
  template <insidable B, numeric A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    // Keep B's representation flags so the twin's raw layout is B's.
    constexpr policy_flag representation =
        policy_of<B> & (exact | f64 | f32 | direct | indexed | raw_width_mask);
    using twin = inside<grid_of<B>, unsafe | representation>;
    return B::from_raw(twin{value}.raw());   // same grid → identical raw layout
  }

} // namespace beman::inside

#endif // BEMAN_INSIDE_CASTS_HPP
