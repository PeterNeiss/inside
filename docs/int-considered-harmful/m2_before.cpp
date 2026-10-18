// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Worked migration, part 2, before: a 12-bit ADC reading converted to
// millivolts in int. The gain is rounded to whole counts per volt, the
// division truncates, and an over-voltage check compares counts with
// millivolts. It compiles cleanly at -Wall -Wextra.

#include <cstdio>

int main()
{
  const int raw       = 2048;   // counts
  const int offset    = 12;     // counts
  const int gain      = 1241;   // counts per volt (data sheet: 1240.909)
  const int limit_mv  = 2000;   // over-voltage threshold, millivolts

  int mv = (raw - offset) * 1000 / gain;

  std::printf("reading            = %d mV   <- truncated\n", mv);
  std::printf("over-voltage?      = %s   <- counts compared with mV\n",
              raw > limit_mv ? "yes" : "no");

  std::printf("\nexpected           = 1641 mV (1640.73), no over-voltage\n");
  return 0;
}
