// The named-cast family — explicit "double in, bounded out" conversions whose
// intent (clamp / wrap / trust, and the rounding direction) reads at the call
// site. Their canonical home is inside a `std::ranges::transform` lambda, where
// a bare `B{v}` constructor would obscure what happens at the boundary.
//
// Demonstrates:
//   - `clamp_floor` / `clamp_ceil` / `clamp_round` — saturate + a rounding
//     direction, applied across a buffer with std::ranges::transform
//   - `wrap_cast`   — modular reduction into the target interval
//   - `clamp_cast`  — saturate to [Lower, Upper]
//   - `unchecked_cast` — trust the caller (UB if out of range)
//   - the per-operation `with_snap<Mode>()` (Mode = snap / round_nearest /
//     round_floor / round_ceil / round_half_even), `with_clamp()`, `with_wrap()`
//     overrides on a strict (throwing) bound — the same intents, fluent form

#include <algorithm>
#include <iostream>
#include <ranges>
#include <vector>

#include "bound/bound.hpp"
#include "bound/io.hpp"
#include "bound/casts.hpp"

using namespace bnd;

int main()
{
  // Coarse output grid: 0..10 in steps of 2 — the notch makes the three
  // rounding directions visibly different.
  using level = bound<{{0, 10}, 2}>;

  std::vector<double> raw = { -1.0, 2.4, 3.5, 7.9, 12.0 };
  std::vector<level> down(raw.size()), up(raw.size()), nearest(raw.size());

  // Same inputs, three rounding directions — the clamp_* family in transform.
  std::ranges::transform(raw, down.begin(),    [](double v){ return clamp_floor<level>(v); });
  std::ranges::transform(raw, up.begin(),      [](double v){ return clamp_ceil <level>(v); });
  std::ranges::transform(raw, nearest.begin(), [](double v){ return clamp_round<level>(v); });

  std::cout << "clamp_* into bound<{{0,10},2}> (out-of-range saturates):\n";
  std::cout << "   raw     floor   ceil    round\n";
  for (std::size_t i = 0; i < raw.size(); ++i)
    std::cout << "  " << raw[i] << "\t " << down[i]
              << "\t " << up[i] << "\t " << nearest[i] << "\n";

  // wrap_cast: modular reduction — a hue angle folded back into [0, 359].
  using hue = bound<{0, 359}>;
  std::cout << "\nwrap_cast<hue>(370) = " << wrap_cast<hue>(370)
            << "   wrap_cast<hue>(-30) = " << wrap_cast<hue>(-30) << "\n";

  // clamp_cast: saturate regardless of the target's declared policy.
  using pct = bound<{0, 100}>;
  std::cout << "clamp_cast<pct>(150) = " << clamp_cast<pct>(150)
            << "   clamp_cast<pct>(-5) = " << clamp_cast<pct>(-5) << "\n";

  // unchecked_cast: caller asserts in-range; skips every check (UB otherwise).
  std::cout << "unchecked_cast<pct>(42) = " << unchecked_cast<pct>(42)
            << "   (trusted — no runtime check)\n";

  // Per-operation overrides on a strict bound: same intents, fluent form. The
  // base type carries no rounding/clamp policy, so each write opts in for that
  // one operation.
  std::cout << "\nper-operation overrides on a strict bound<{0,100}>:\n";
  bound<{0, 100}> q{0};
  q.with_snap<round_floor>()           = 3.7;  std::cout << "  with_snap<round_floor>()           = 3.7  -> " << q << "\n";
  q.with_snap<round_ceil>()            = 3.2;  std::cout << "  with_snap<round_ceil>()            = 3.2  -> " << q << "\n";
  q.with_snap()        = 9.9;  std::cout << "  with_snap()        = 9.9  -> " << q << "\n";
  q.with_snap<round_half_even>() = 2.5;  std::cout << "  with_snap<round_half_even>() = 2.5  -> " << q << "\n";

  bound<{0, 359}> angle{0};
  angle.with_wrap() = 400;          std::cout << "  with_wrap()            = 400  -> " << angle << "\n";

  return 0;
}
