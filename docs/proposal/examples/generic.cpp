// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: insides in generic code — common_type, numeric_limits and hash.
#include <beman/inside/inside.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <unordered_set>

using namespace beman::inside;

int main() {
    // [listing]
    using small = inside<{0, 10}>;
    using wide  = inside<{-50, 50}>;

    using common = std::common_type_t<small, wide>;          // the hull of the two grids
    static_assert(std::is_same_v<common, inside<{-50, 50}>>);
    common hi = std::max<common>(small{7}, wide{-3});        // 7

    using limits = std::numeric_limits<small>;
    static_assert(limits::is_specialized && !limits::is_signed && limits::max() == 10);

    std::unordered_set<small> seen{small{3}, small{4}, small{3}};  // two elements
    // [/listing]
    if (hi != just<7> || seen.size() != 2)
        return 1;
    return 0;
}
