// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A run-time double may be NaN, which clamp cannot place: T{d, clamp} needs a
// report channel (try_make<clamp>(d), make_policy<clamp>(ec)) or unsafe.
#include <inside/inside.hpp>

using namespace ins;

int main(int argc, char**) {
    const double     d = argc * 0.5;
    id<"[0..100]"> x{d, clamp};
    (void)x;
}
