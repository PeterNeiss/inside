// Cartesian-to-polar conversion: (x, y) → (magnitude, angle in turns).
//
// Demonstrates:
//   - `bnd::math::atan2` for the angle. Output is a signed turn-phase in
//     [-1/2, 1/2], not radians — multiply by 2π at the boundary if needed.
//   - `bnd::math::sqrt` for the magnitude, fed `x² + y²` computed in rational.
//   - Round-trip back via `sin`/`cos`: starting from (x, y), recovering
//     (mag·cos(θ), mag·sin(θ)) should land within a few Q-format ULPs of the
//     original — a sanity check that the trig and sqrt are wired correctly.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Cartesian coordinate bound: [-1, 1] with Q.14 resolution.
  using coord_t = bound<{{-1, 1}, notch<1, 16384>}, round_nearest>;
  // Magnitude is non-negative; max possible |v| from (±1, ±1) is √2 ≈ 1.4142.
  // We accept inputs in [-1, 1], so mag_sq ∈ [0, 2], mag ∈ [0, √2].
  using magsq_t = bound<{{0, 2}, notch<1, 16384>}, round_nearest>;

  struct point { rational x; rational y; const char* label; };
  const point points[] = {
    { rational{ 1},    rational{ 0},    "+x axis      " },
    { rational{ 0},    rational{ 1},    "+y axis      " },
    { -1_r,    rational{ 0},    "-x axis      " },
    { rational{ 0},    -1_r,    "-y axis      " },
    { rational{ 1},    rational{ 1},    "+45° (mag √2)" },
    { -1_r,    rational{ 1},    "+135°        " },
    { rational{ 3, 5}, rational{ 4, 5}, "(3/5, 4/5) — 3-4-5 triangle, mag 1" },
    { rational{ 1, 2}, rational{ 1, 2}, "(½, ½)       " },
  };

  std::cout << "Cartesian → polar:\n";
  std::cout << "  (x, y)                              magnitude        angle (turns)\n";
  for (auto& p : points) {
    coord_t x{p.x}, y{p.y};

    // mag² = x² + y² in rational (no bound math; the cross-multiplies stay
    // small here, ≤ 2 in magnitude).
    rational x2 = rational::mul_unchecked(p.x, p.x);
    rational y2 = rational::mul_unchecked(p.y, p.y);
    magsq_t  mag_sq{rational::add_unchecked(x2, y2)};

    auto magnitude = math::sqrt(mag_sq);
    auto angle     = math::atan2(y, x);

    std::cout << "  " << p.label
              << "    mag = " << magnitude
              << "    θ = "  << angle << "\n";
  }

  std::cout << "\nRound-trip (polar → cartesian via sin/cos):\n";
  std::cout << "  original           recovered (mag·cos θ, mag·sin θ)\n";
  // atan2 returns turns ∈ [-1/2, 1/2]; sin/cos take radians. Scale the
  // turn output by 2π to land in a radians-valued angle bound, then
  // drive the public sin/cos. ±4 rad comfortably brackets ±π.
  using angle_t = bound<{{-4, 4}, notch<1, 16384>}, round_nearest>;
  for (auto& p : points) {
    coord_t x{p.x}, y{p.y};
    rational x2 = rational::mul_unchecked(p.x, p.x);
    rational y2 = rational::mul_unchecked(p.y, p.y);
    magsq_t  mag_sq{rational::add_unchecked(x2, y2)};
    auto magnitude = math::sqrt(mag_sq);
    auto angle     = math::atan2(y, x);

    // Turn → radians: one rational multiply by 2π, then snap into angle_t.
    angle_t a{rational::mul_unchecked(rational{angle}, math::two_pi)};

    rational m_r = rational{magnitude};
    coord_t rx{rational::mul_unchecked(m_r, rational{math::cos(a)})};
    coord_t ry{rational::mul_unchecked(m_r, rational{math::sin(a)})};

    std::cout << "  (" << x << ", " << y << ") → (" << rx << ", " << ry << ")\n";
  }

  return 0;
}
