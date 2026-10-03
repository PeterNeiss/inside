// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hazard 11: narrowing conversions. Assigning a wider value into a
// narrower type is not an error, it is a silent truncation.
//
// This is the Ariane 5 failure in miniature: a 64-bit value that no longer
// fitted a 16-bit destination, in code that had been correct on Ariane 4.

#include <cstdint>
#include <cstdio>

int main()
{
  std::int64_t horizontal_velocity = 40000;   // fine on the old rocket

  std::int16_t converted = static_cast<std::int16_t>(horizontal_velocity);

  std::printf("velocity (64-bit)  = %ld\n", horizontal_velocity);
  std::printf("stored as 16-bit   = %d   <- sign flipped\n", converted);

  // Implicit narrowing compiles without a cast, and without a warning
  // unless you asked for one.
  int  big   = 300;
  char small = big;
  std::printf("int 300 -> char    = %d\n", small);
  return 0;
}
