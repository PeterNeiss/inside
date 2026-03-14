#include <iostream>
#include <limits>

#include "bound/bound.hpp"
#include "bound/detail/debug.hpp"

using namespace bnd;
using namespace bnd::literals;

void test_comparision()
{
  static_assert
  (
    rational{-1} < rational{0} &&
    rational{0}  < rational{1} &&
    rational{-3, 2} < rational{-2, 3} 
  );

  static_assert
  (
    rational{3,2} + rational{1,5} == rational{17,10} &&
    rational{3,2} - rational{1,5} == rational{13,10} &&
    rational{3,2} / rational{1,2} == rational{3} &&
    rational{3,2} * rational{1,2} == rational{3, 4}
  );
}

using test0_t = bound<1,3>;
using test1_t = bound<0, {1u, 1, sign::negative}, {-1}>;
//using test2_t = bound<{1,1, sign::zero}>;
//using test3_t = bound<{1,0}>;
using test4_t = bound<0, std::numeric_limits<umax>::max(), -1>;
using test5_t = bound<1_r>;
using test6_t = bound<-6_r/(1 << 4)>;
using test7_t = bound<-6.0/(1 << 4)>;
using test8_t = bound<-10.0, 10.0, 0.25>;
using test9_t = bound<3,1>;
using test10_t = bound<1,100, 2_r/3>;

int main()
{
  test_comparision();
  
  // print_values<-(10.0_r)>{};
  //print_types<bound<>, test0_t, test1_t>{};
  //print_types<test2_t>{};
  //print_types<test3_t>{};
  //print_types<test4_t>{};
  //print_types<test6_t>{};
  //print_types<test7_t>{};
  //print_types<test8_t>{};
  //test9_t{};
  //test10_t{};
  bound b;
  (void)b;
  std::cout << "bound test\n";
  return 0;
}
