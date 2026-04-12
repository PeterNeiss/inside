#include <iostream>
#include <limits>

#include "bound/common.hpp"
#include "bound/bound.hpp"
#include "bound/interval.hpp"
#include "bound/print.hpp"

#include <iostream>

using namespace bnd;

constexpr handler handlers =
{
  .domain_error = +[](const char* = nullptr) { }
};

template<handler H>
class testHandler {};

void test_handler()
{
  testHandler<handlers> th;
  (void)th;
}

void test_waiver()
{
  policy p;
  (void) p;
}

void test_conversion()
{
  using f30t40 = bound<{{30, 40}, 2}>;
  using f20t50 = bound<{interval{20, 50}, 1}>;
  f30t40 smaller{34};
  f20t50 bigger;

  bigger = smaller;
  std::cout << "bigger(34) = " << bigger << std::endl;
  bigger = 39;

  smaller.policy<ignore_round>() = bigger; // does round
  std::cout << "smaller(38) = " << smaller << std::endl;

  bigger = 30;
  smaller.with_round() = bigger;
  std::cout << "smaller(30) = " << smaller << std::endl;
}

void test_rational()
{
  constexpr rational a{std::numeric_limits<int>::min()};
  (void)a;
}

void test_div()
{
  using r = bound<{1,255}>;
 // using r = bound<{-255,-1}>;
  std::error_code ec;
  //r a{1020ull, make_policy(ec)};
  r a{102, make_policy(ec)};
  if (ec)
    std::cout << ec.message() << std::endl;
  else
    std::cout << "no error" << std::endl;
 // r a{1020ull};
  r b{16};

  auto c = a / b;
  std::cout << "a = " << a << std::endl;
  std::cout << "b = " << b << std::endl;
  std::cout << "c = " << *c << std::endl;
}

void test_mul()
{
  using r = bound<{10,255, 1}>;
  r a{16};
  r b = 102;

  auto c = a * b;
  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;

  using u4 = bound<{{0.75, 10.5}, 0.25}>;

  u4 d{3};
  u4 e{3.25};

  auto f = d * e;
  std::cout << d << std::endl;
  std::cout << e << std::endl;
  std::cout << f << std::endl;

  using n4 = u4::negative;

  auto g = n4(-2);
  auto h = n4(-2.25);

  std::cout << "g = " << g << std::endl;
  std::cout << "h = " << h << std::endl;
  auto i = g * h;
  std::cout << "i = " << i << std::endl;

  auto j = d * g;
  std::cout << "j = " << j << std::endl;
  auto k = g * d;
  std::cout << "k = " << k << std::endl;

  using s = bound<{{-100, 10}, 1.0/16}>;
  using n = s::negative;

  //auto l = s(-700.125);
  auto l = s(-70.125);
  auto m = n(1.125);

  std::cout << "l = " << l << std::endl;
  std::cout << m << std::endl;
  std::cout << l*m << std::endl;
  std::cout << m*l << std::endl;
}

void test_add()
{
  using u8 = bound<{10,255}>;
  //constexpr u8 invalid{-6};
  u8 a{16};
  u8 b = 220;
  //auto b1 = b + 1;
  std::cout << "b(220) = " << b << std::endl;
  std::cout << "just<1> = " << just<1> << std::endl;
  auto b1{b + just<1>};
  std::cout << "b1(221) = " << b1 << std::endl;

  b = a + b1;
  std::cout << "a(16) = " << a << std::endl;
  std::cout << "b(237) = " << b << std::endl;

  auto d = a - b;
  std::cout << "d(-221) = " << d << std::endl;

  using u64 = bound<{{0, std::numeric_limits<std::uint64_t>::max()}, 1}>;

  constexpr u64 biggest{std::numeric_limits<std::uint64_t>::max()};
  std::cout << biggest << std::endl;
  //auto e = biggest + biggest;
}

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
    *(rational{3,2} * rational{1,2}) == rational{3, 4}
  );
}

void test_interval()
{
  constexpr interval point{*(3_r/4), *(3_r/4)};
  constexpr interval a{0,1};

  (void)point;
  (void)a;
  //print_values<point>{};
}

void test_grid()
{
  static_assert
  (
    grid{{0,100}, 1}.max_notch() == 100 &&
    grid{{1,5}, 0.25}.max_notch() == 16 &&
    grid{{0,UINT64_MAX}, 1}.max_notch() == UINT64_MAX
  );
}

void test_bound()
{
  //bound<-0.5, 0.5, 1>{}; // zero displaced here
  using frac = bound<{{-10, 10}, 0}>;
  //print_types<frac>{};
  static_assert(Grid<frac> == grid{-10, 10, 0});
  static_assert(std::is_same_v<typename frac::raw_type, rational>);

 // auto test = frac::unchecked{3};

  constexpr frac f = *(2_r/3);
  std::cout << "frac f = " << f << std::endl;

  using step = bound<{{-5, 5}, 0.5}>;
  static_assert(not std::is_same_v<step::raw_type, rational>);
  constexpr step s = *(3_r/2);
  std::cout << "step s = " << s << std::endl;
}

using test0_t = bound<{{1,3},1}>;
using test1_t = bound<{{0, {1u, -1}}, -1}>; // fails on instantiation
//using test2_t = bound<{1,1}>; // zero numerator normalization test
//using test3_t = bound<{1,0}>;
using test4_t = bound<{{0, std::numeric_limits<umax>::max()}, 1}>;
using test5_t = bound<{1_r}>;
using test6_t = bound<{*(-6_r/(1 << 4))}>;
using test7_t = bound<{-6.0/(1 << 4)}>;
using test8_t = bound<{{-10.0, 10.0}, 0.25}>;
//using test9_t = bound<{3,1}>;  // fails on instantiation
using test10_t = bound<{{1,100}, *(3_r/2)}>;
//using test11_t = bound<{1,5}>;

static_assert(std::is_same_v<typename test0_t::raw_type, std::uint8_t>);
static_assert(std::is_same_v<typename test4_t::raw_type, std::uint64_t>);

static_assert(std::is_same_v<typename test5_t::raw_type, rational>);
//static_assert(std::is_same_v<test11_t::raw_type, rational>);

int main()
{
  try
  {
    test_conversion();
    test_comparision();
    test_add();
    test_bound();
    test_mul();
    test_div();
    test_waiver();

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
  }
  catch(std::system_error& e)
  {
    std::cout << "code:    [" << e.code() << "]\n"
                 "message: [" << e.code().message() << "]\n"
                 "what:    [" << e.what() << "]\n";
  }
  catch(std::exception& e)
  {
    std::cout << e.what() << std::endl;
  }
  catch(char const* str)
  {
    std::cout << str << std::endl;
  }
  return 0;
}
