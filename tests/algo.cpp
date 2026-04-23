#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>

namespace rng = std::ranges;
using namespace bnd;
using u8 = bnd::bound<{0, 100, 0.5}>;
using u16 = bnd::bound<{0, 1000, 0.5}>;

slim::optional<rational> test_opt()
{
  return rational{1}/0;
}

int main()
{
  try
  {
    auto opt_rat = test_opt();
    std::cout << "has_value: " << opt_rat.has_value() << "  value = " << *opt_rat << std::endl;

    std::vector<u8> vu8;
    vu8.resize(10);

    rng::generate(vu8, [counter = 7] mutable { return ++counter; });

  //  vu8.push_back(5);
  //  vu8.push_back(5);

    for (auto value: vu8)
      std::cout << value << std::endl;

   // auto sum = rng::fold_right(vu8, bnd::just<0>, std::plus<u16::flag<ignore_domain>>{});
    auto sum = std::reduce(vu8.begin(), vu8.end(), u16{0}, std::plus<>{});
    std::cout << sum << std::endl;
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
