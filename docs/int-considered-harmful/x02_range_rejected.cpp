// DOES NOT COMPILE, BY DESIGN.
//
// A constant that cannot fit the target range is a build error, not a
// runtime surprise. The equivalent `int` code (h11_narrowing.cpp) is
// accepted silently.

#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  constexpr bound<{0, 100}> pct{150};   // error: 150 is outside [0, 100]
  return static_cast<int>(pct.raw());
}
