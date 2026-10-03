// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Bounded array indexing with safe for loops.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  int arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

  // Range-based for loop
  std::cout << "range for loop:" << "\n";
  for (auto i : inside_range<{0, 9}>{})
    std::cout << "  arr[" << i << "] = " << arr[i] << "\n";

  // Wrapping range-based for loop starting at index 5
  std::cout << "wrapping from 5:" << "\n";
  for (auto i : inside_range<{0, 9}>{5})
    std::cout << "  arr[" << i << "] = " << arr[i] << "\n";

  // `.indexed()` pairs each inside with its 0-based position (the inside-range
  // counterpart of std::views::enumerate).
  std::cout << "indexed():" << "\n";
  for (auto [pos, i] : inside_range<{0, 9}>{}.indexed())
    std::cout << "  #" << pos << " -> arr[" << i << "] = " << arr[i] << "\n";

  // `.strided(n)` visits every n-th slot — here every other index.
  std::cout << "strided(2):" << "\n";
  for (auto i : inside_range<{0, 9}>{}.strided(2))
    std::cout << "  arr[" << i << "] = " << arr[i] << "\n";

  return 0;
}
