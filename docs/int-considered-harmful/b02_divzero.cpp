// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to hazard 4: division by zero is a value you can test, not a
// signal that kills the process. The result is std::expected<inside, errc>;
// a divisor grid that provably excludes zero gives you a plain value with
// nothing to unwrap.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main()
{
  using val = inside<{0, 100}>;

  val a = 7;
  val b = 3;

  // Exact rational result -- no silent truncation.
  auto exact = a / b;
  if (exact)
    std::cout << "7 / 3              = " << *exact << "   (exact, not 2)\n";

  // Opt in to C-style truncation explicitly, per call.
  auto trunc = div(a, b, truncated);
  if (trunc)
    std::cout << "div(7, 3, trunc)   = " << *trunc << "\n";

  // Division by zero: an error value, and the program keeps running.
  val zero = 0;
  auto oops = val(10) / zero;
  std::cout << "10 / 0             = "
            << (oops.has_value() ? "value" : errc_message(oops.error()))
            << "   (no SIGFPE)\n";

  return 0;
}
