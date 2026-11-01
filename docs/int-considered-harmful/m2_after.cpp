// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Worked migration, part 2, after: counts are a dimension of their own.
// (raw - offset) / gain is an exact voltage, rounded once onto millivolts,
// and comparing a reading with a voltage does not compile.

#include <iostream>

#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

// ADC counts: a base dimension, its quantity and its unit.
inline constexpr base_dimension count_def{"count"};
inline constexpr quantity_spec  adc_counts_def{"adc_counts", count_def};
inline constexpr named_unit     cts_def{"cts", spec_c<adc_counts_def>};
inline constexpr auto           cts = unit_c<cts_def>;

using reading    = quantity<cts, inside<{0, 4095}>>;   // 12-bit ADC
using gain_t     = quantity<cts / si::volt, inside<{{1000, 1500}, per<1000>}>>;
using millivolts = quantity<si::milli(si::volt), inside<{-5000, 5000}, round_nearest>>;

template <class A, class B>
concept comparable = requires(A a, B b) { a < b; };

int main()
{
  const reading                          raw{2048};
  const quantity<cts, inside<{-64, 64}>> offset{12};
  const gain_t                           gain{rational{1240909, 1000}};
  const millivolts                       limit{2000};

  // The gain excludes 0, so the quotient is a plain, exact voltage.
  auto       volts = (raw - offset) / gain;
  millivolts mv    = value_cast<millivolts>(volts);   // one rounding

  std::cout << "reading            = " << mv << "\n";
  std::cout << "over-voltage?      = " << (mv > limit ? "yes" : "no") << "\n";

  // `raw > limit` does not compile: counts are not volts.
  static_assert(!comparable<reading, millivolts>);
  return 0;
}
