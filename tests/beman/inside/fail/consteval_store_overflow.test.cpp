// EXPECT: is outside the grid's interval
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A checked store that fails in a constant expression names the code and the fix.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

constexpr inside<{0, 10}> level{11};

int main() {}
