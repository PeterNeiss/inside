// EXPECT: expected a symbol one visible unit has
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Two visible units with one symbol (and different content) leave the symbol unread.
#include <inside/units.hpp>
inline constexpr auto xa = ins::base_unit<"xu", "dim_a">;
inline constexpr auto xb = ins::base_unit<"xu", "dim_b">;
int main() {
    ins::id<"[0..9] [xu]"> x;
    (void)x;
}
