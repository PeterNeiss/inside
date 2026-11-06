// EXPECT: static extents differ
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// dot of two vectors whose static extents differ does not compile.
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/mdspan.hpp>

#include <array>

int main() {
    using namespace beman::inside;
    std::array<inside<{0, 9}>, 3> x{};
    std::array<inside<{0, 9}>, 2> y{};
    (void)dot(std::mdspan{x.data(), std::extents<std::size_t, 3>{}},
              std::mdspan{y.data(), std::extents<std::size_t, 2>{}});
}
