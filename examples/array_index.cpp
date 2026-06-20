// Bounded array indexing with safe for loops.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"

using namespace bnd;

int main()
{
  int arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

  // Regular for loop with slim::optional + sentinel policy
  using index = bound<{0, std::extent_v<decltype(arr)> - 1}, sentinel>;
  std::cout << "regular for loop:" << "\n";
  for (slim::optional<index> i = 0; i; ++i)
    std::cout << "  arr[" << *i << "] = " << arr[*i] << "\n";

  // Range-based for loop
  std::cout << "range for loop:" << "\n";
  for (auto i : bound_range<{0, 9}>{})
    std::cout << "  arr[" << i << "] = " << arr[i] << "\n";

  // Wrapping range-based for loop starting at index 5
  std::cout << "wrapping from 5:" << "\n";
  for (auto i : bound_range<{0, 9}>{5})
    std::cout << "  arr[" << i << "] = " << arr[i] << "\n";

  // `.indexed()` pairs each bound with its 0-based position (the bound-range
  // stand-in for std::views::enumerate, working on C++20 too).
  std::cout << "indexed():" << "\n";
  for (auto [pos, i] : bound_range<{0, 9}>{}.indexed())
    std::cout << "  #" << pos << " -> arr[" << i << "] = " << arr[i] << "\n";

  // `.strided(n)` visits every n-th slot — here every other index.
  std::cout << "strided(2):" << "\n";
  for (auto i : bound_range<{0, 9}>{}.strided(2))
    std::cout << "  arr[" << i << "] = " << arr[i] << "\n";

  return 0;
}
