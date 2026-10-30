// EXPECT: is not on the grid's notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A checked store that fails in a constant expression names the code and the fix.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

constexpr inside<{{0, 100}, per<100>}> price{3.49}; // 3.49 is no multiple of 1/100

int main() {}
