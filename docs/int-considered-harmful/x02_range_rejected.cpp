// DOES NOT COMPILE, BY DESIGN.
//
// A constant that cannot fit the target range is a build error, not a
// runtime surprise. The equivalent `int` code (h11_narrowing.cpp) is
// accepted silently.

#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  constexpr inside<{0, 100}> pct{150};   // error: 150 is outside [0, 100]
  return static_cast<int>(pct.raw());
}
