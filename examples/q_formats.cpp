// Q-format reference: which fractional grid maps to which storage size.
// Pick the smallest grid that covers your value range and precision needs;
// the library selects the narrowest unsigned/signed type automatically.

#include <iomanip>
#include <iostream>

#include "bound/bound.hpp"
#include "bound/formats.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // The common Q-formats are predefined in <bound/formats.hpp>:
  static_assert(sizeof(q4_4)   == 1);   // [0, 15],    notch 1/16    -> uint8
  static_assert(sizeof(q8_8)   == 2);   // [0, 255],   notch 1/256   -> uint16
  static_assert(sizeof(q16_16) == 4);   // [0, 65535], notch 1/65536 -> uint32

  // Custom formats are spelled by hand. A plain decimal (`1.0/128`, `0.5`)
  // reads well for coarse exact fractions; for fine notches prefer
  // `notch<N, D>` (exact rational), since the equivalent double loses precision.

  // Q1.7: 1 integer bit, 7 fraction bits   -> 128 steps in [0, 1+127/128]
  using q1_7 = bound<{{0, 1}, 1.0/128}>;
  static_assert(sizeof(q1_7) == 1);

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
