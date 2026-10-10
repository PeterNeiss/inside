// EXPECT: divmod requires integer-valued grids within imax and snap
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// divmod rounds its quotient, so it needs a rounding policy (snap or round_*),
// like `%`: without one, `a / b` is the exact quotient.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 100}> a{17};
    inside<{1, 10}>  b{5};
    auto [q, r] = divmod(a, b); // ill-formed: no rounding policy
    (void)q;
    (void)r;
}
