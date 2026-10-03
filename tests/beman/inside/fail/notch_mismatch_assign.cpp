// EXPECT: incompatible notches
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A source lattice that does not land on the target lattice needs explicit
// rounding permission (`with_snap()` / `policy<snap>()`); without it the
// assignment is ill-formed.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  inside<{{0, 1}, notch<1, 2>}> halves{0.5_ins};
  inside<{{0, 1}, notch<1, 3>}> thirds{};
  thirds = halves;   // ill-formed: 1/2 grid points miss the 1/3 lattice
}
