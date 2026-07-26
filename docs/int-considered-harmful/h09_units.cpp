// Hazard 9: the semantic failure. An int holding cents and an int holding
// dollars are the same type. Nothing in the language notices the mix-up,
// and no sanitizer will ever flag it -- the arithmetic is perfectly valid.

#include <cstdio>

int price_in_cents(int item)
{
  static const int table[] = {1999, 450, 1234};
  return table[item];
}

int shipping_in_dollars()
{
  return 5;   // $5 flat rate
}

int main()
{
  int subtotal = price_in_cents(0) + price_in_cents(1) + price_in_cents(2);
  int total    = subtotal + shipping_in_dollars();   // <- units mismatch

  std::printf("subtotal (cents)   = %d\n", subtotal);
  std::printf("shipping (dollars) = %d\n", shipping_in_dollars());
  std::printf("total              = %d   <- charged 5 cents for shipping\n", total);
  std::printf("compiles clean, runs clean, bills the customer wrong.\n");
  return 0;
}
