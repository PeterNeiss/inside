// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One-shot stores — explicit "double in, bounded out" conversions whose intent
// (clamp / wrap / trust, and the rounding direction) reads at the call site:
// the flag constructor B{v, F}, whose flags replace B's own of the same kind.
// Their canonical home is inside a `std::ranges::transform` lambda, where a
// bare `B{v}` would obscure what happens at the boundary.
//
// Demonstrates:
//   - `B{v, clamp | round_floor}` (and round_ceil / round_nearest) — saturate + a
//     rounding direction, applied across a buffer with std::ranges::transform
//   - `B{v, wrap}`   — modular reduction into the target interval
//   - `B{v, clamp}`  — saturate to [Lower, Upper]
//   - `B{v, unsafe}` — trust the caller (UB if out of range)
//   - the per-store `x.policy<F>()` (F = snap / round_nearest / round_floor /
//     round_ceil / round_half_even / clamp / wrap) on a strict (throwing)
//     inside — the same intents, for one assignment

#include <algorithm>
#include <iostream>
#include <ranges>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/casts.hpp>

using namespace beman::inside;

int main() {
    // Coarse output grid: 0..10 in steps of 2 — the notch makes the three
    // rounding directions visibly different.
    using level = inside<{{0, 10}, 2}>;

    std::vector<double> raw = {-1.0, 2.4, 3.5, 7.9, 12.0};
    std::vector<level>  down(raw.size()), up(raw.size()), nearest(raw.size());

    // Same inputs, three rounding directions — clamp with a rounding mode, in transform.
    std::ranges::transform(raw, down.begin(), [](double v) { return level{v, clamp | round_floor}; });
    std::ranges::transform(raw, up.begin(), [](double v) { return level{v, clamp | round_ceil}; });
    std::ranges::transform(raw, nearest.begin(), [](double v) { return level{v, clamp | round_nearest}; });

    std::cout << "level{v, clamp | round_*} into inside<{{0,10},2}> (out-of-range saturates):\n";
    std::cout << "   raw     floor   ceil    round\n";
    for (std::size_t i = 0; i < raw.size(); ++i)
        std::cout << "  " << raw[i] << "\t " << down[i] << "\t " << up[i] << "\t " << nearest[i] << "\n";

    // wrap: modular reduction — a hue angle folded back into [0, 359].
    using hue = inside<{0, 359}>;
    std::cout << "\nhue{370, wrap} = " << hue{370, wrap} << "   hue{-30, wrap} = " << hue{-30, wrap} << "\n";

    // clamp: saturate regardless of the target's declared policy.
    using pct = inside<{0, 100}>;
    std::cout << "pct{150, clamp} = " << pct{150, clamp} << "   pct{-5, clamp} = " << pct{-5, clamp} << "\n";

    // unsafe: caller asserts in-range; skips every check (UB otherwise).
    std::cout << "pct{42, unsafe} = " << pct{42, unsafe} << "   (trusted — no runtime check)\n";

    // Per-operation overrides on a strict inside: same intents, fluent form. The
    // base type carries no rounding/clamp policy, so each write opts in for that
    // one operation.
    std::cout << "\nper-operation overrides on a strict inside<{0,100}>:\n";
    inside<{0, 100}> q{0};
    q.policy<round_floor>() = 3.7;
    std::cout << "  policy<round_floor>()     = 3.7  -> " << q << "\n";
    q.policy<round_ceil>() = 3.2;
    std::cout << "  policy<round_ceil>()      = 3.2  -> " << q << "\n";
    q.policy<snap>() = 9.9;
    std::cout << "  policy<snap>()            = 9.9  -> " << q << "\n";
    q.policy<round_half_even>() = 2.5;
    std::cout << "  policy<round_half_even>() = 2.5  -> " << q << "\n";

    inside<{0, 359}> angle{0};
    angle.policy<wrap>() = 400;
    std::cout << "  policy<wrap>()            = 400  -> " << angle << "\n";

    return 0;
}
