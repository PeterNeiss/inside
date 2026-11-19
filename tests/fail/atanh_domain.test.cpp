// EXPECT: atanh: input must be in
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// atanh(±1) is infinite: an input grid reaching ±1 is rejected at compile time.
#include <inside/inside.hpp>
#include <inside/math.hpp>

using namespace ins;

int main() {
    using unit_value = inside<{{-1, 1}, per<1024>}, round_half_away>;
    auto r           = math::atanh(unit_value{0});
    (void)r;
}
