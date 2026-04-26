// Bounded array indexing with safe for loops.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  int arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

  // Regular for loop with slim::optional + sentinel policy
  using index = bound<{0, std::extent_v<decltype(arr)> - 1}, sentinel>;
  std::cout << "regular for loop:" << std::endl;
  for (slim::optional<index> i = 0; i; ++i)
    std::cout << "  arr[" << *i << "] = " << arr[*i] << std::endl;

  // Range-based for loop
  std::cout << "range for loop:" << std::endl;
  for (auto i : bound_range<{0, 9}>{})
    std::cout << "  arr[" << i << "] = " << arr[i] << std::endl;

  // Wrapping range-based for loop starting at index 5
  std::cout << "wrapping from 5:" << std::endl;
  for (auto i : bound_range<{0, 9}>{5})
    std::cout << "  arr[" << i << "] = " << arr[i] << std::endl;

  return 0;
}
