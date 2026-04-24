// Temperature with fractional notch: Celsius from -40 to 60 in 0.5 degree steps.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  using celsius = bound<{{-40, 60}, 0.5}>;

  celsius room = 21.4; //TODO: rounds to 21.0 not neareast, consider performance
  celsius freezing = 0;
  celsius body = 37;

  std::cout << "room:     " << room << " C" << std::endl;
  std::cout << "freezing: " << freezing << " C" << std::endl;
  std::cout << "body:     " << body << " C" << std::endl;

  auto diff = room - freezing;
  std::cout << "diff:     " << diff << " C" << std::endl;

  auto sum = room + freezing;
  std::cout << "sum:      " << sum << " C" << std::endl;

  return 0;
}
