// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One length type from the Planck length (1.616255·10^-35 m) to the observable
// universe (8.8·10^26 m), on a 10^-41 m grid: about 2^226 points. Grid numbers
// past 64 bits need C++26 static reflection (GCC 16 -freflection); the `_g`
// literal spells them exactly.
//   1. The universe in Planck lengths: their exact ratio, and its log10
//      correctly rounded — about 10^61.74.
//   2. Their geometric mean — halfway in log scale — by sqrt, correctly rounded
//      onto the same 10^-41 m grid: about 0.12 mm.

#include <format>
#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

#if BEMAN_INSIDE_BIG_GRIDS

using length = inside<{{1e-41_g, 1e27_g}, 1e-41_g}, round_nearest>; // positive: log10 needs it
using decade = inside<{{-100, 100}, per<1 << 20>}, round_nearest>;  // log10 to 2^-20

int main() {
    const length planck   = just<1.616255e-35_g>;
    const length universe = just<8.8e26_g>;

    // 1. The exact ratio (a fraction of 226-bit integers), and one rounding
    //    for its log10.
    const auto ratio = universe / planck;
    std::cout << "universe / planck = " << *ratio << "\n";
    const decade d = math::log10_into<decade>(*ratio);
    std::cout << std::format("log10              = {:.6f}\n", d);
    if (to_string(*ratio) != "17600000000000000000000000000000000000000000000000000000000000000000/323251" ||
        d != decade{rational{64'734'859, 1 << 20}})
        return 1;

    // 2. The geometric mean, exact to the last of 41 decimals.
    const length mean = math::sqrt_into<length>(planck * universe);
    std::cout << "sqrt(planck * universe) = " << mean << " m\n";
    if (mean != *from_chars<length>("0.00011926040415829555623064347860850441433"))
        return 1;
    return 0;
}

#else

int main() {
    std::cout << "planck_to_cosmos needs C++26 static reflection (BEMAN_INSIDE_BIG_GRIDS).\n";
    return 0;
}

#endif
