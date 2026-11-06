// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// std::common_type works with the umbrella header alone:
// mixed-grid insides meet in the grid hull, so generic code compiles.
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>

using namespace beman::inside;

TEST(CommonTypeTest, common_type_is_the_grid_hull_without_opt_in_headers) {
    using A = inside<{0, 100}>;
    using B = inside<{{-10, 10}, per<2>}>;
    using C = std::common_type_t<A, B>;
    static_assert(std::is_same_v<C, common_inside_t<A, B>>);
    static_assert(grid_of<C> == grid{{-10, 100}, per<2>});
    static_assert(std::is_same_v<std::common_type_t<A, A>, A>);

    // A policy both share stays; different ones give the default, checked.
    using CA = inside<{0, 10}, clamp | round_nearest>;
    using CB = inside<{-5, 5}, clamp | round_nearest>;
    static_assert(flags_of<std::common_type_t<CA, CB>> == (clamp | round_nearest));
    static_assert(flags_of<std::common_type_t<CA, inside<{-5, 5}, wrap>>> == checked);

    // Generic code that needs a common type: std::max on two different grids.
    const C m = std::max<C>(A{30}, B{2.5});
    EXPECT_EQ(m, 30);
}
