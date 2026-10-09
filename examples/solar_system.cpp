// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Positions across the solar system to the nanometre: ±10^13 m (past Neptune)
// on a 10^-9 m grid is 2·10^22 points, a wide raw — exact where a double,
// with 53 bits, resolves only about 2 mm at that distance.
//   1. A probe's position as the exact sum of many short legs; the same sum in
//      double drifts.
//   2. Straight-line distance with hypot, correctly rounded onto a 1 mm grid.

#include <cstdio>
#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

inline constexpr rational nm{1, 1'000'000'000};
using metres = inside<{{-10'000'000'000'000, 10'000'000'000'000}, nm}>;      // ±10^13 m to 1 nm
using leg    = inside<{{-10'000'000, 10'000'000}, nm}>;                      // one step, ±10 000 km
using mm     = inside<{{0, 100'000'000'000'000}, per<1000>}, round_nearest>; // a distance, to 1 mm

int main() {
    // 1. Start at Saturn's distance and add a million legs of 1000.000000001 km
    //    (the 1 nm is what a double cannot keep at this scale). Each += is a
    //    raw add of two 64-bit limbs.
    const metres start{1'433'000'000'000};
    const leg    step{rational{1'000'000'000'000'001, 1'000'000'000}}; // 1e6 m + 1 nm
    metres       x  = start;
    double       xd = 1.433e12;
    for (int i = 0; i < 1'000'000; ++i) {
        x += step;
        xd += 1e6 + 1e-9;
    }
    std::cout << "exact   " << x << " m\n";
    std::printf("double  %.9f m\n", xd);
    // 1.433e12 + 1e12 + 1e6·1e-9 m = 2433000000000.001 m
    if (to_string(x) != "2433000000000.001000000") // to the nanometre
        return 1;

    // 2. Distance to a point 3·10^12 m east and 4·10^12 m north: exactly
    //    5·10^12 m, and hypot returns exactly that.
    const metres east{3'000'000'000'000}, north{4'000'000'000'000};
    const mm     d = math::hypot_into<mm>(east, north);
    std::cout << "hypot   " << d << " m\n";
    if (d != mm{5'000'000'000'000})
        return 1;
    return 0;
}
