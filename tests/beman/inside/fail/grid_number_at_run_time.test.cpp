// EXPECT: is not a constant expression
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grid numbers are formed at compile time: P - n with a run-time n is a
// compile error (an inside computes at run time). P - 2 folds.
#include <beman/inside/inside.hpp>

int main(int argc, char**) {
    using namespace beman::inside;
    constexpr auto P = 1267650600228229401496703205376_g;
    using residue    = inside<{0, P}>;
    residue ok{P - 2};
    residue bad{P - argc};
    (void)ok;
    (void)bad;
}
