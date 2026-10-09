// EXPECT: storage needs a grid whose values double holds exactly
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// f64 is storage only: a continuous grid holds any fraction, which a double
// cannot, so the flag is rejected rather than rounding values.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

using real = inside<{{0, 1}, 0}, f64>;
real r{0};
