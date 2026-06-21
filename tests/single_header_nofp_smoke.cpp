// FP-free smoke test: compiles the amalgamated single header with BND_MATH_NO_FP,
// the no-hardware-floating-point path. Proves two things at compile time:
//   1. The single header builds with NO <cmath> — a poison <cmath> shim (placed
//      first on the include path by CMake) hard-errors if anything pulls it in.
//   2. With the double engine compiled out, the always-present integer/CORDIC
//      engine still serves the full bnd::math transcendental API.
// Built on demand via the `single_header_nofp_smoke` target (EXCLUDE_FROM_ALL).

#include "bound/bound.hpp"   // the single header (bnd::math amalgamated in too)

#include <cstdio>

#ifndef BND_MATH_NO_FP
#  error "single_header_nofp_smoke must be built with BND_MATH_NO_FP defined"
#endif

// The FP engine namespace must be gone; only the integer engine remains. (We do
// not name bnd::math::dbl here — it does not exist under BND_MATH_NO_FP.)
static_assert(noexcept(true));

int main()
{
  using namespace bnd;

  // Core arithmetic — no FP anywhere.
  bound<{0, 100}, clamp> a{200};                 // -> 100
  bound<{0, 9},   wrap>  w{13};                   // -> 3

  // Transcendentals via the integer/CORDIC engine on a snap grid. These are
  // constexpr under BND_MATH_NO_FP, so evaluate at compile time too.
  using Ang = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;
  constexpr auto s0 = math::sin(Ang{0});
  constexpr auto c0 = math::cos(Ang{0});
  static_assert(detail::rational{s0} == 0);
  static_assert(detail::rational{c0} == 1);

  auto s1 = math::sin(Ang{1});                    // runtime, integer engine
  (void)s1;

  std::printf("a=%d w=%d s0=%d c0=%d\n",
              (int)detail::to_value(a),
              (int)detail::to_value(w),
              (int)detail::to_value(s0),
              (int)detail::to_value(c0));

  return (detail::rational{c0} == 1) ? 0 : 1;
}
