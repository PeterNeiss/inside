// Hyperbolic and logarithmic corners of bnd::math: a `tanh` soft-clipper plus
// `sinh`/`cosh`, `log2`, and `cbrt`. Every operand carries the `real` policy on
// a dyadic grid (the math-operand shape), so the whole thing is constexpr and
// bit-identical on both engines — no `<cmath>`, no FPU dependence.
//
// Demonstrates:
//   - `tanh` — a smooth saturating waveshaper (the analog soft-clip curve)
//   - `sinh` / `cosh` — and the identity cosh²(x) − sinh²(x) = 1
//   - `log2` — octave distance between two frequencies
//   - `cbrt` — a cube-root perceptual curve

#include <iostream>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // tanh soft-clip: drive in [-4, 4] maps smoothly into (-1, 1). Compare a hard
  // clip (clamp to [-1, 1]) against tanh's gentle knee.
  using drive_t = bound<{{-4, 4}, notch<1, 1024>}, round_nearest | real>;
  std::cout << "tanh soft-clip vs hard clip:\n";
  std::cout << "  x        tanh(x)        hard-clip\n";
  for (drive_t x : { drive_t{-3}, drive_t{-1}, drive_t{-0.5}, drive_t{0},
                     drive_t{0.5}, drive_t{1}, drive_t{3} })
  {
    auto soft = math::tanh(x);
    bound<{-1, 1}, clamp | round_nearest> hard{x};
    std::cout << "  " << x << "     " << soft << "     " << hard << "\n";
  }

  // sinh / cosh and the identity cosh^2 - sinh^2 = 1 (within Q-format ULPs).
  using arg_t = bound<{{-3, 3}, notch<1, 1024>}, round_nearest | real>;
  std::cout << "\nsinh / cosh and cosh^2 - sinh^2 (= 1):\n";
  std::cout << "  x        sinh(x)        cosh(x)        cosh^2 - sinh^2\n";
  for (arg_t x : { arg_t{-2}, arg_t{-1}, arg_t{0}, arg_t{1}, arg_t{2} })
  {
    auto s = math::sinh(x);
    auto c = math::cosh(x);
    auto identity = c * c - s * s;        // bound-space; widens, no overflow
    std::cout << "  " << x << "     " << s << "     " << c
              << "     " << identity << "\n";
  }

  // log2: octave distance log2(f / f_ref). Ratio is a positive real bound.
  using ratio_t = bound<{{0x1p-4, 16}, notch<1, 1024>}, round_nearest | real>;
  std::cout << "\nlog2(ratio) — octaves above/below reference:\n";
  for (ratio_t r : { ratio_t{0.5}, ratio_t{1}, ratio_t{2}, ratio_t{4}, ratio_t{8} })
    std::cout << "  log2(" << r << ") = " << math::log2(r) << "\n";

  // cbrt: cube-root perceptual curve.
  using vol_t = bound<{{0, 64}, notch<1, 256>}, round_nearest | real>;
  std::cout << "\ncbrt (cube root):\n";
  for (vol_t v : { vol_t{1}, vol_t{8}, vol_t{27}, vol_t{64} })
    std::cout << "  cbrt(" << v << ") = " << math::cbrt(v) << "\n";

  return 0;
}
