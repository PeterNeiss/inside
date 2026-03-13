#include <iostream>

#include "bound/bound.hpp"
#include "bound/detail/debug.hpp"

using namespace bnd;

using test0_t = bound<{0,1}, {1,1}, {1}>;
using test1_t = bound<{0,1}, {1,1, sign::negative}, {1}>;
//using test2_t = bound<{1,1, sign::zero}>;
//using test3_t = bound<{1,0}>;

int main()
{
  print_types<bound<>, test0_t, test1_t>{};
  //print_types<test2_t>{};
  //print_types<test3_t>{};
  bound b;
  (void)b;
  std::cout << "bound test\n";
  return 0;
}
