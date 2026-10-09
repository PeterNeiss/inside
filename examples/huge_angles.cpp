// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sin and cos of huge arguments, correctly rounded. An angle of 10^18 radians
// is exact as an integer inside; the engine reduces it by 2π exactly and
// returns the nearest point of a 2^-52 amplitude grid. The double route has two
// problems: 2^62 − 1 is not a double at all (it rounds to 2^62, a different
// angle), and reducing with fmod(x, 2π) uses a 2π that is off by 2^-51, an
// error the huge argument multiplies into garbage.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

using radians = inside<{0, (std::int64_t{1} << 62) - 1}>; // integers up to 2^62 − 1
using amp52   = math::amp<(std::int64_t{1} << 52)>;       // [-1, 1] on a 2^-52 grid

int main() {
    struct row {
        std::int64_t x;
        std::int64_t sin_slot, cos_slot; // the correctly rounded results × 2^52
    };
    for (const row r : {row{1'000'000'000'000'000'000, -4'471'936'262'876'823, 533'100'051'040'090},
                        row{(std::int64_t{1} << 62) - 1, 985'025'092'865'520, -4'394'557'448'717'327}}) {
        const radians x{r.x};
        const amp52   s = math::sin_into<amp52>(x), c = math::cos_into<amp52>(x);
        const double  xd = static_cast<double>(r.x);
        std::cout << "x = " << x
                  << std::format("\n  sin                    {:.16f}\n  cos                    {:.16f}\n", s, c);
        std::printf("  std::sin(double)       %.16f\n  sin(fmod(x, 2*pi))     %.16f\n",
                    std::sin(xd),
                    std::sin(std::fmod(xd, 2 * 3.141592653589793)));
        if (s != amp52{rational{r.sin_slot, std::int64_t{1} << 52}} ||
            c != amp52{rational{r.cos_slot, std::int64_t{1} << 52}})
            return 1;
    }
    return 0;
}
