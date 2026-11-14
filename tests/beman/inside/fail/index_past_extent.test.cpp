// EXPECT: index_for
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// index_for ties an index type to a static extent: an index that reaches
// past it is rejected where a function asks for the proof.
#include <beman/inside/inside.hpp>
#include <beman/inside/inside/mdspan.hpp>

#include <array>

using namespace beman::inside;
using image = std::mdspan<int, std::extents<std::size_t, 4, 5>>;

template <index_for<image::extents_type, 0> R, index_for<image::extents_type, 1> C>
int at(const image& m, R r, C c) {
    return m[r, c];
}

int main() {
    std::array<int, 20> a{};
    const image         m{a.data()};
    return at(m, inside<{0, 4}>{}, inside<{0, 4}>{}); // rows are 0 ... 3
}
