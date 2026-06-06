//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcastsHPP
#define BNDcastsHPP

#include "bound/bound.hpp"

//---------------------------------------------------------------------------
// Free-function casts — the named casts that complement the constructors.
// (`as_rational`, the uniform rational view, now lives in `generic.hpp`.)
// Unlike a direct `B{value}` call, these read
// naturally in algorithm callbacks (`std::transform`,
// `std::ranges::views::transform`) and make the intent — clamp vs. wrap
// vs. throw vs. trust — explicit at the call site.
//---------------------------------------------------------------------------
namespace bnd
{
  // Each named cast constructs the result directly through the value+policy
  // constructor, passing a one-shot policy that overrides B's declared policy
  // for this conversion only — clamp regardless of how B was declared. For
  // scalar inputs `bound_assignable` reduces to `numeric`, so out-of-range
  // and fractional values are accepted and handled at runtime by the policy.
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_cast(N value)
  { return B{value, make_policy<clamp>()}; }

  // `wrap_cast` rounds out the named-cast trio with modular semantics:
  // the input value is reduced into the target grid's interval rather than
  // clipped at the boundaries. Useful where the caller wants integer-style
  // wraparound (sequence numbers, angles, ring-buffer indices) inside an
  // `std::transform` or other algorithm callback.
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B wrap_cast(N value)
  { return B{value, make_policy<wrap>()}; }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round
  //
  // Compose `clamp` and one of the rounding modes — the canonical pipeline
  // for "double in, bounded integer out, never throw" used in audio,
  // graphics, and DSP code. Without these helpers the caller would chain
  // `with_clamp().policy<round_floor>()` etc. by hand.
  //---------------------------------------------------------------------------
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_floor(N value)
  { return B{value, make_policy<clamp | round_floor>()}; }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_ceil(N value)
  { return B{value, make_policy<clamp | round_ceil>()}; }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_round(N value)
  { return B{value, make_policy<clamp | round_nearest>()}; }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (will_conversion_overflow<B>(value))
      throw std::system_error(make_error_code(errc::domain_error),
                              "checked_cast: value out of bound interval");
    if (will_conversion_truncate<B>(value))
      throw std::system_error(make_error_code(errc::rounding_error),
                              "checked_cast: value does not land on notch");
    return B{value};
  }

  // `unchecked_cast` is the strong sibling of constructing via the `unsafe`
  // policy: it routes through `bound<G, unsafe>` so the compiler can elide
  // every domain/round check. UB if the caller's value is actually out of
  // range — same contract as bounded::integer's `assume_in_range`.
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    using twin = bound<Grid<B>, unsafe>;
    return B::from_raw(twin{value}.raw());   // same grid → identical raw layout
  }

} // namespace bnd

#endif // BNDcastsHPP
