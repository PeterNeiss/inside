// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// MIGRATION, BEFORE: an invoice calculation in int.
//
// Three separate defects, none of which the compiler mentions:
//   1. subtotal * 8 overflows int for large invoices  (UB)
//   2. * 8 / 100 truncates the tax down to the cent   (silent)
//   3. / 3 truncates the discount                     (silent)

#include <cstdio>

int main()
{
  // A large but entirely plausible invoice: $2,684,354.56 in cents.
  int subtotal = 268435456;

  int tax      = subtotal * 8 / 100;   // 8% sales tax
  int discount = subtotal / 3;         // "a third off"
  int total    = subtotal + tax - discount;

  std::printf("subtotal (cents)   = %d\n", subtotal);
  std::printf("tax  (8%%)          = %d   <- overflowed\n", tax);
  std::printf("discount (1/3)     = %d   <- truncated\n", discount);
  std::printf("total              = %d\n", total);
  std::printf("\nexpected tax       = 21474836.48\n");
  std::printf("expected discount  = 89478485.33\n");
  return 0;
}
