// EXPECT: a continuous element has no deduced total type
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The deduced sum of a static mdspan needs a notched element.
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/mdspan.hpp>

#include <array>

int main() {
    using namespace beman::inside;
    using ratio = inside<{{0, 1}, max_denominator{100}}>;
    std::array<ratio, 2> r{};
    (void)sum(std::mdspan{r.data(), std::extents<std::size_t, 2>{}});
}
