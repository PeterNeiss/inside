// A bounded index cannot leave the array. The valid range is the type,
// so the bounds check happens once -- at construction -- rather than at
// every access, or not at all.

#include <array>
#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  std::array<int, 10> data{};
  for (std::size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<int>(i * i);

  using index = inside<{0, 9}, sentinel>;

  // In range: usable directly.
  index i{7};
  std::cout << "data[7]            = " << data[i.to<std::size_t>().value()] << "\n";

  // Out of range: absence is a value, not a buffer overrun.
  auto bad = index::try_make(10);
  std::cout << "index 10           = " << (bad ? "has value" : "nullopt")
            << "   (no out-of-bounds access possible)\n";

  // Iterating the valid slots exactly, with no off-by-one to get wrong.
  int sum = 0;
  for (auto k : inside_range<{0, 9}>{})
    sum += data[k.to<std::size_t>().value()];
  std::cout << "sum of all slots   = " << sum << "\n";
  return 0;
}
