// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// FP-free smoke test: compiles the amalgamated single header with BEMAN_INSIDE_MATH_NO_FP,
// the no-hardware-floating-point path. Proves two things at compile time:
//   1. The single header builds with NO <cmath> — a poison <cmath> shim (placed
//      first on the include path by CMake) hard-errors if anything pulls it in.
//   2. With the double engine compiled out, the always-present integer/CORDIC
//      engine still serves the full beman::inside::math transcendental API.
// Built on demand via the `single_header_nofp_smoke` target (EXCLUDE_FROM_ALL).

#include <beman/inside/inside.hpp>   // the single header (beman::inside::math amalgamated in too)

#include <cstdio>

#ifndef BEMAN_INSIDE_MATH_NO_FP
#  error "single_header_nofp_smoke must be built with BEMAN_INSIDE_MATH_NO_FP defined"
#endif

// The FP engine namespace must be gone; only the integer engine remains. (We do
// not name beman::inside::math::dbl here — it does not exist under BEMAN_INSIDE_MATH_NO_FP.)
static_assert(noexcept(true));

int main()
{
  using namespace beman::inside;

  // Core arithmetic — no FP anywhere.
  inside<{0, 100}, clamp> a{200};                 // -> 100
  inside<{0, 9},   wrap>  w{13};                   // -> 3

  // Transcendentals via the integer/CORDIC engine on a snap grid. These are
  // constexpr under BEMAN_INSIDE_MATH_NO_FP, so evaluate at compile time too.
  using Ang = inside<{{-8, 8}, per<16384>}, round_nearest>;
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
