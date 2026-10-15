// EXPECT: on_clamp: the overshoot grid
// REQUIRES: 64-bit-grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An on_clamp's overshoot from an inside source lives on the source grid
// minus the target grid; past the 64-bit grid numbers it has none, and the
// message says so (with_clamp() stores the same value).
#include <beman/inside/inside.hpp>

#include <cstdint>

int main() {
    using namespace beman::inside;
    using R = inside<{0, UINT64_MAX}>;
    inside<{-100, -1}> x{-50};
    x.on_clamp([](auto&, auto) {}) = R{5u};
}
