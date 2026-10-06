// EXPECT: fixed-width storage: the chosen raw type is too small
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A pinned width flag never widens silently: {0, 300} does not fit a uint8.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

inside<{0, 300}, u8> too_small;
