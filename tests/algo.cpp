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
    std::cout << "has_value: " << opt_rat.has_value() << std::endl;

    std::vector<u8> vu8;
    vu8.resize(10);

    rng::generate(vu8, [counter = 7] mutable { return ++counter; });

  //  vu8.push_back(5);
  //  vu8.push_back(5);

    for (auto value: vu8)
      std::cout << value << std::endl;

   // auto sum = rng::fold_right(vu8, bnd::just<0>, std::plus<u16::flag<ignore_domain>>{});
    auto sum = std::reduce(vu8.begin(), vu8.end(), u16{0}, std::plus<>{});
    std::cout << "reduce: " << sum << std::endl;

    // sort + is_sorted
    rng::sort(vu8);
    std::cout << "is_sorted: " << rng::is_sorted(vu8) << std::endl;

    // find / count_if
    auto it = rng::find(vu8, u8{12});
    std::cout << "find 12: " << (it != vu8.end() ? "found" : "not found") << std::endl;

    auto above = rng::count_if(vu8, [](u8 v) { return v > 10; });
    std::cout << "count > 10: " << above << std::endl;

    // min_element / max_element
    auto lo = rng::min_element(vu8);
    auto hi = rng::max_element(vu8);
    std::cout << "min: " << *lo << "  max: " << *hi << std::endl;

    // transform
    std::vector<u16> shifted(vu8.size());
    rng::transform(vu8, shifted.begin(), [](u8 v) { return v + u8{1}; });
    std::cout << "transform +1: ";
    for (auto v : shifted) std::cout << v << " ";
    std::cout << std::endl;

    // copy_if
    std::vector<u8> filtered;
    rng::copy_if(vu8, std::back_inserter(filtered), [](u8 v) { return v > 12; });
    std::cout << "filtered > 12: ";
    for (auto v : filtered) std::cout << v << " ";
    std::cout << std::endl;

    // any_of / all_of / none_of
    std::cout << "all > 0: " << rng::all_of(vu8, [](u8 v) { return v > 0; }) << std::endl;
    std::cout << "any == 10: " << rng::any_of(vu8, [](u8 v) { return v == 10; }) << std::endl;
    std::cout << "none > 100: " << rng::none_of(vu8, [](u8 v) { return v > 100; }) << std::endl;

    // --- classic STL (iterator-based) ---

    // std::sort
    std::vector<u8> v2 = {50, 10, 30, 20, 40};
    std::sort(v2.begin(), v2.end());
    std::cout << "std::sort: ";
    for (auto v : v2) std::cout << v << " ";
    std::cout << std::endl;

    // std::stable_sort descending
    std::stable_sort(v2.begin(), v2.end(), std::greater<>{});
    std::cout << "std::stable_sort desc: ";
    for (auto v : v2) std::cout << v << " ";
    std::cout << std::endl;

    // std::nth_element — find median
    std::vector<u8> v3 = {70, 10, 50, 30, 90};
    std::nth_element(v3.begin(), v3.begin() + 2, v3.end());
    std::cout << "nth_element median: " << v3[2] << std::endl;

    // std::partial_sort — top 3
    std::vector<u8> v4 = {20, 80, 10, 60, 90, 40};
    std::partial_sort(v4.begin(), v4.begin() + 3, v4.end(), std::greater<>{});
    std::cout << "partial_sort top 3: " << v4[0] << " " << v4[1] << " " << v4[2] << std::endl;

    // std::accumulate
    auto acc = std::accumulate(vu8.begin(), vu8.end(), u16{0}, std::plus<>{});
    std::cout << "accumulate: " << acc << std::endl;

    // std::for_each
    std::cout << "for_each: ";
    std::for_each(v2.begin(), v2.end(), [](u8 v) { std::cout << v << " "; });
    std::cout << std::endl;

    // std::reverse
    std::reverse(v2.begin(), v2.end());
    std::cout << "reverse: ";
    for (auto v : v2) std::cout << v << " ";
    std::cout << std::endl;

    // std::rotate
    std::rotate(v2.begin(), v2.begin() + 2, v2.end());
    std::cout << "rotate: ";
    for (auto v : v2) std::cout << v << " ";
    std::cout << std::endl;

    // std::partition
    auto pivot = std::partition(v2.begin(), v2.end(), [](u8 v) { return v > 30; });
    std::cout << "partition >30: ";
    for (auto p = v2.begin(); p != pivot; ++p) std::cout << *p << " ";
    std::cout << "| ";
    for (auto p = pivot; p != v2.end(); ++p) std::cout << *p << " ";
    std::cout << std::endl;

    // std::equal
    std::vector<u8> eq1 = {10, 20, 30};
    std::vector<u8> eq2 = {10, 20, 30};
    std::cout << "equal: " << std::equal(eq1.begin(), eq1.end(), eq2.begin()) << std::endl;

    // std::mismatch
    eq2[1] = 99;
    auto mm = std::mismatch(eq1.begin(), eq1.end(), eq2.begin());
    std::cout << "mismatch: " << *mm.first << " vs " << *mm.second << std::endl;

    // std::iota
    std::vector<u8> iota_v(5);
    std::iota(iota_v.begin(), iota_v.end(), u8{10});
    std::cout << "iota: ";
    for (auto v : iota_v) std::cout << v << " ";
    std::cout << std::endl;

    // bound_range with for_each
    std::cout << "bound_range: ";
    for (auto i : bound_range<{0, 9}>{})
      std::cout << i << " ";
    std::cout << std::endl;
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
