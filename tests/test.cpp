#include <iostream>
#include <limits>

#include "bound/bound.hpp"
#include "bound/detail/debug.hpp"

using namespace bnd;

void test_comparision()
{
  static_assert
  (
    rational{-1} < rational{0} &&
    rational{0}  < rational{1} &&
    rational{-3, 2} < rational{-2, 3} 
  );
}

using test0_t = bound<1,3>;
using test1_t = bound<0, {1u, 1, sign::negative}, {-1}>;
//using test2_t = bound<{1,1, sign::zero}>;
//using test3_t = bound<{1,0}>;
using test4_t = bound<0, {std::numeric_limits<umax>::max(), 1}, -1>;

int main()
{
  test_comparision();
  //print_types<bound<>, test0_t, test1_t>{};
  //print_types<test2_t>{};
  //print_types<test3_t>{};
  //print_types<test4_t>{};
  bound b;
  (void)b;
  std::cout << "bound test\n";
  return 0;
}
