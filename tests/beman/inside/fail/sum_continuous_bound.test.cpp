// EXPECT: too large for an exact total
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A sum of continuous elements adds exactly over lcm(1..D), D their
// denominator bound; past 4096 bits for that lcm it is rejected instead of
// reporting overflow at run time.
#include <beman/inside/inside.hpp>

#include <array>

using namespace beman::inside;

int main() {
    using fine = inside<{{0, 1}, max_denominator{100000}}>;
    std::array<fine, 2> r{};
    (void)sum<inside<{{0, 2}, per<1024>}, round_nearest>>(r);
}
