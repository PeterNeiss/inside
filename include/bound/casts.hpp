//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcastsHPP
#define BNDcastsHPP

#include "bound/core.hpp"

//---------------------------------------------------------------------------
// Free-function casts complementing the constructors. Unlike a direct B{value}
// call, these read naturally in algorithm callbacks and make the intent (clamp
// vs. wrap vs. throw vs. trust) explicit at the call site.
//---------------------------------------------------------------------------
namespace bnd
{
  // Each cast constructs via the value+policy constructor, passing a one-shot
  // policy that overrides B's declared one for this conversion only.
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_cast(N value)
  { return B{value, make_policy<clamp>()}; }

  // `wrap_cast` — modular semantics: the input is reduced into the target grid's
  // interval rather than clipped. For integer-style wraparound (angles, indices).
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B wrap_cast(N value)
  { return B{value, make_policy<wrap>()}; }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round — compose `clamp` with a rounding
  // mode: the canonical "double in, bounded integer out, never throw" pipeline.
  //---------------------------------------------------------------------------
  template <policy_flag RoundMode, boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_with_rounding(N value)
  { return B{value, make_policy<clamp | RoundMode>()}; }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_floor(N value)
  { return clamp_with_rounding<round_floor, B>(value); }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_ceil(N value)
  { return clamp_with_rounding<round_ceil, B>(value); }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_round(N value)
  { return clamp_with_rounding<round_nearest, B>(value); }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (will_conversion_overflow<B>(value))
      detail::raise(errc::domain_error, "checked_cast: value out of bound interval");
    if (will_conversion_trunc<B>(value))
      detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value};
  }

  // `unchecked_cast` routes through `bound<G, unsafe>` so the compiler elides
  // every domain/round check. UB if the value is actually out of range.
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    using twin = bound<Grid<B>, unsafe>;
    return B::from_raw(twin{value}.raw());   // same grid → identical raw layout
  }

} // namespace bnd

#endif // BNDcastsHPP
