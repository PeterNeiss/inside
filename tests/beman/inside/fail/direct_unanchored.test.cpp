// EXPECT: requires integer values
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// `direct` stores the value itself in an integer raw: a grid with notch 1 off
// the integers ({{0.5, 10.5}, 1}) has no integer values to store.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    inside<{{0.5_r, 10.5_r}, 1}, direct> d{1.5};
    (void)d;
}
