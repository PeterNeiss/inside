// Temperature with fractional notch: Celsius from -40 to 60 in 0.5 degree steps.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  using celsius = bound<{{-40, 60}, 0.5}, round_nearest>;

  celsius room = 21.4;
  celsius freezing = 0;
  celsius body = 37;

  std::cout << "room:     " << room << " C" << "\n";
  std::cout << "freezing: " << freezing << " C" << "\n";
  std::cout << "body:     " << body << " C" << "\n";

  auto diff = room - freezing;
  std::cout << "diff:     " << diff << " C" << "\n";

  auto sum = room + freezing;
  std::cout << "sum:      " << sum << " C" << "\n";

  return 0;
}
