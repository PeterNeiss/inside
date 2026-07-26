// Double hazard 2: the error accumulates. A running total over a day's
// transactions drifts away from the answer the ledger expects.

#include <cstdio>

int main()
{
  double total = 0.0;
  for (int i = 0; i < 10000; ++i)
    total += 0.01;   // one cent, ten thousand times

  std::printf("10000 x $0.01      = %.20f\n", total);
  std::printf("expected           = %.20f\n", 100.0);
  std::printf("exactly $100.00    ? %s\n", (total == 100.0) ? "true" : "false");
  std::printf("error              = %.20f\n", total - 100.0);
  return 0;
}
