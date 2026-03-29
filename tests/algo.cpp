#include "bound/bound.hpp"

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>

namespace rng = std::ranges;
using namespace bnd;
using u8 = bnd::bound<{0, 100, 0.5}>;
using u16 = bnd::bound<{0, 1000, 0.5}>;


int main()
{
  std::vector<u8> vu8;
  vu8.resize(10);

  rng::iota(vu8, 7);

  vu8.push_back(5);
  vu8.push_back(5);
 
  for (auto value: vu8)
    std::cout << value << std::endl; 

 // auto sum = rng::fold_right(vu8, bnd::just<0>, std::plus<u16::flag<ignore_domain>>{});
  auto sum = rng::fold_right(vu8, bnd::just<0>, std::plus<u16>{});
  std::cout << sum << std::endl; 
  return 0;
}
