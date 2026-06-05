// Q-format reference: which fractional grid maps to which storage size.
// Pick the smallest grid that covers your value range and precision needs;
// the library selects the narrowest unsigned/signed type automatically.

#include <iomanip>
#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Notch spelling: a plain decimal (`1.0/16`, `0.5`) reads well for coarse
  // exact fractions. For fine notches, prefer `notch<N, D>` — an exact
  // rational — because the equivalent `double` would lose precision (e.g.
  // `1.0/65536` no longer round-trips exactly).

  // Q4.4: 4 integer bits, 4 fraction bits  -> 256 steps in [0, 15+15/16]
  using q4_4 = bound<{{0, 15}, 1.0/16}>;
  static_assert(sizeof(q4_4) == 1);

  // Q1.7: 1 integer bit, 7 fraction bits   -> 128 steps in [0, 1+127/128]
  using q1_7 = bound<{{0, 1}, 1.0/128}>;
  static_assert(sizeof(q1_7) == 1);

  // Q8.8: 8 integer bits, 8 fraction bits  -> 65281 steps fits uint16
  using q8_8 = bound<{{0, 255}, 1.0/256}>;
  static_assert(sizeof(q8_8) == 2);

  // Q16.16: 16 integer bits, 16 fraction bits -> ~4.29B steps fits uint32
  // (fine notch: `notch<1, 65536>` keeps it exact; `1.0/65536` would not)
  using q16_16 = bound<{{0, 65535}, notch<1, 65536>}>;
  static_assert(sizeof(q16_16) == 4);

  // Half-step signed: -50 to 50 in 0.5 increments (201 steps, fits uint8)
  using half_signed = bound<{{-50, 50}, 0.5}>;
  static_assert(sizeof(half_signed) == 1);

  q4_4        a{3.25};      auto sum_a = a + a;
  q1_7        b{0.75};      auto sum_b = b + b;
  q8_8        c{42.5};      auto sum_c = c + c;
  q16_16      d{1000.125};  auto sum_d = d + d;
  half_signed e{-12.5};     auto sum_e = e + e;

  std::cout << std::left << std::setw(14) << "format"
            << std::setw(10) << "sizeof"
            << std::setw(14) << "sample"
            << "sample + sample" << "\n";
  std::cout << std::string(60, '-') << "\n";

  std::cout << std::left << std::setw(14) << "Q4.4"
            << std::setw(10) << sizeof(q4_4)
            << std::setw(14) << a << sum_a << "\n";
  std::cout << std::left << std::setw(14) << "Q1.7"
            << std::setw(10) << sizeof(q1_7)
            << std::setw(14) << b << sum_b << "\n";
  std::cout << std::left << std::setw(14) << "Q8.8"
            << std::setw(10) << sizeof(q8_8)
            << std::setw(14) << c << sum_c << "\n";
  std::cout << std::left << std::setw(14) << "Q16.16"
            << std::setw(10) << sizeof(q16_16)
            << std::setw(14) << d << sum_d << "\n";
  std::cout << std::left << std::setw(14) << "half_signed"
            << std::setw(10) << sizeof(half_signed)
            << std::setw(14) << e << sum_e << "\n";

  return 0;
}
