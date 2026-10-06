// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Self-containment check for the amalgamated single header. This TU is compiled
// with ONLY single_include/ on the include path (no include/), so it fails to
// build unless the generated header is fully
// self-sufficient. Built on demand via the `single_header_smoke` target; it is
// not part of the default build (EXCLUDE_FROM_ALL).
//---------------------------------------------------------------------------
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;

    inside<{0, 100}> a{42};
    inside<{0, 100}> b{8};
    auto             sum = a + b; // result grid {0, 200}, unit integer

    return (static_cast<int>(sum) == 50) ? 0 : 1;
}
