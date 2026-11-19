// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Through a proxy without a report channel a double is a constant only.
#include <inside/inside.hpp>

using namespace ins;

int main(int argc, char**) {
    inside<{0, 100}> x{0};
    x.with<round_floor>() = argc * 0.5;
}
