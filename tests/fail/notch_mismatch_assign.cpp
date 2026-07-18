// EXPECT: incompatible notches
// A source lattice that does not land on the target lattice needs explicit
// rounding permission (`with_snap()` / `policy<snap>()`); without it the
// assignment is ill-formed.
#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  bound<{{0, 1}, notch<1, 2>}> halves{0.5_b};
  bound<{{0, 1}, notch<1, 3>}> thirds{};
  thirds = halves;   // ill-formed: 1/2 grid points miss the 1/3 lattice
}
