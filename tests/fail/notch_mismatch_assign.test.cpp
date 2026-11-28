// EXPECT: incompatible notches
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A source lattice that does not land on the target lattice needs explicit
// rounding permission (`with<round_toward_zero>()` or a rounding policy); without it the
// assignment is ill-formed.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    id<"[0..1] 1/2"> halves{0.5_q};
    id<"[0..1] 1/3"> thirds{};
    thirds = halves; // ill-formed: 1/2 grid points miss the 1/3 lattice
}
