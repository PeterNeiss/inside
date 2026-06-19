//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// Self-containment check for the amalgamated single header. This TU is compiled
// with ONLY single_include/ on the include path (no include/bound, no
// include/slim), so it fails to build unless the generated header is fully
// self-sufficient. Built on demand via the `single_header_smoke` target; it is
// not part of the default build (EXCLUDE_FROM_ALL).
//---------------------------------------------------------------------------
#include "bound/bound.hpp"

int main()
{
    using namespace bnd;

    bound<{0, 100}> a{42};
    bound<{0, 100}> b{8};
    auto sum = a + b;                          // result grid {0, 200}, unit integer

    return (static_cast<int>(sum) == 50) ? 0 : 1;
}
