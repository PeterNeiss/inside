// EXPECT: an index is a builtin integer or an inside on the integers
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// inside_mdspan takes inside indices on the integers only.
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/mdspan.hpp>

#include <array>

int main() {
    using namespace beman::inside;
    std::array<int, 4> a{};
    inside_mdspan      m{a.data(), std::extents<std::size_t, 4>{}};
    (void)m[inside<{{0, 3}, 0.5}>{1.5}];
}
