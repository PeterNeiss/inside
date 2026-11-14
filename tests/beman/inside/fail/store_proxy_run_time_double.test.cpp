// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Through a proxy without a report channel a double is a constant only.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main(int argc, char**) {
    inside<{0, 100}> x{0};
    x.policy<round_floor>() = argc * 0.5;
}
