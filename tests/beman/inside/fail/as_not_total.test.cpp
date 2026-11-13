// EXPECT: takes a conversion the grid proves total
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// as<T>() is for conversions the grid proves total; to<T>() reports.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    const inside<{-5, 5}> s{3};
    (void)s.as<unsigned>();
}
