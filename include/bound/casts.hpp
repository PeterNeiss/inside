//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcastsHPP
#define BNDcastsHPP

#include "bound/bound.hpp"

//---------------------------------------------------------------------------
// Free-function casts — `as_rational` plus the seven named casts that
// complement the constructors. Unlike a direct `B{value}` call, these read
// naturally in algorithm callbacks (`std::transform`,
// `std::ranges::views::transform`) and make the intent — clamp vs. wrap
// vs. throw vs. trust — explicit at the call site.
//---------------------------------------------------------------------------
namespace bnd
{
  // Library-internal inline rational view of a value or bound. Used inside
  // arithmetic / assignment machinery where the implicit `operator rational()`
  // would be hidden by overload resolution. User code should prefer
  // `b.to<rational>()`, which carries a typed sentinel-state error.
  template <arithmetic A>
  [[nodiscard]] constexpr rational as_rational(A v) { return rational{v}; }

  template <boundable B>
  [[nodiscard]] constexpr rational as_rational(B b) { return b; }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B saturated_cast(A value)
  {
    B b{};                  // default-init; immediately overwritten below
    b.with_clamp() = value; // clamp regardless of B's declared policy
    return b;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr B saturated_cast(A value)
  {
    B b{};
    b.with_clamp() = value;
    return b;
  }

  // `wrap_cast` rounds out the named-cast trio with modular semantics.
  // Mirrors `saturated_cast` but uses `with_wrap()` so the input value is
  // reduced into the target grid's interval rather than clipped at the
  // boundaries. Useful where the caller wants integer-style wraparound
  // (sequence numbers, angles, ring-buffer indices) inside an
  // `std::transform` or other algorithm callback.
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B wrap_cast(A value)
  {
    B b{};
    b.with_wrap() = value;
    return b;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr B wrap_cast(A value)
  {
    B b{};
    b.with_wrap() = value;
    return b;
  }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round
  //
  // Compose `with_clamp` and one of the rounding modes — the canonical
  // pipeline for "double in, bounded integer out, never throw" used in
  // audio, graphics, and DSP code. Without these helpers the caller would
  // chain `with_clamp().policy<round_floor>()` etc. by hand.
  //---------------------------------------------------------------------------
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B clamp_floor(A value)
  {
    B b{};
    b.template policy<clamp | round_floor>() = value;
    return b;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr B clamp_floor(A value)
  {
    B b{};
    b.template policy<clamp | round_floor>() = value;
    return b;
  }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B clamp_ceil(A value)
  {
    B b{};
    b.template policy<clamp | round_ceil>() = value;
    return b;
  }

  template <boundable B, boundable A>
  [[nodiscard]] constexpr B clamp_ceil(A value)
  {
    B b{};
    b.template policy<clamp | round_ceil>() = value;
    return b;
  }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B clamp_round(A value)
  {
    B b{};
    b.template policy<clamp | round_nearest>() = value;
    return b;
  }

  // Boundable-source overload: lets a `bound`-typed intermediate flow
  // through `clamp_round<Target>(expr)` directly, without the user having
  // to extract to a `double` or `rational` first. Mirrors the symmetric
  // overload on `saturated_cast`/`wrap_cast`.
  template <boundable B, boundable A>
  [[nodiscard]] constexpr B clamp_round(A value)
  {
    B b{};
    b.template policy<clamp | round_nearest>() = value;
    return b;
  }

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
    twin t{value};
    B out;
    out.Raw = t.Raw;        // same grid → identical raw layout
    return out;
  }

} // namespace bnd

#endif // BNDcastsHPP
