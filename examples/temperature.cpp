// Temperature with fractional notch: Celsius from -40 to 60 in 0.5 degree steps.

#include <iostream>

#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  using celsius = bound<{{-40, 60}, 0.5}>;

  celsius room = 21;
  celsius freezing = 0;
  celsius body = *(37_r/1);

  std::cout << "room:     " << static_cast<double>(room) << " C" << std::endl;
  std::cout << "freezing: " << static_cast<double>(freezing) << " C" << std::endl;
  std::cout << "body:     " << static_cast<double>(body) << " C" << std::endl;

  auto diff = room - freezing;
  std::cout << "diff:     " << static_cast<double>(diff) << " C" << std::endl;

  auto sum = room + freezing;
  std::cout << "sum:      " << static_cast<double>(sum) << " C" << std::endl;

  return 0;
}
