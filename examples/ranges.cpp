// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids as ranges: the values of a type are a sequence you can pipe, index and search without building a table,
// from a mid-grid start that wraps, or by a cursor and its end.

#include <algorithm>
#include <array>
#include <iostream>
#include <ranges>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

namespace rng   = std::ranges;
namespace views = std::views;
using namespace ins;

template <rng::input_range R>
void show(std::string_view label, R&& r)
{
  std::cout << label;
  for (const auto& x : r)
    std::cout << ' ' << x;
  std::cout << '\n';
}

int main()
{
  // 1. values<T>: every value of T as a random-access sized view,
  // so views compose on it and [i], size(), back() cost nothing.
  using digit = id<"[0..9]">; // an index: a container takes it as it is
  static_assert(rng::random_access_range<decltype(values<digit>)>);
  static_assert(rng::sized_range<decltype(values<digit>)>);

  show("digits:      ", values<digit>);
  show("odd squares: ", values<digit>
                          | views::filter([](digit d) { return d % 2_ins == 1; })
                          | views::transform([](digit d) { return d * d; })); // id<"[0..81]">: cannot overflow
  show("last three:  ", values<digit> | views::reverse | views::take(3));
  std::cout << "size:         " << values<digit>.size() << ", [7] = " << values<digit>[7]
            << ", back() = " << values<digit>.back() << '\n';

  using eighth = id<"[0..1] 1/8">; // a fractional notch walks the same way, exactly
  show("eighths:     ", values<eighth> | views::drop(5));

  // Every digit indexes another container in range, by construction.
  constexpr std::array<char, values<digit>.size()> names{'z', 'o', 't', 't', 'f', 'f', 's', 's', 'e', 'n'};
  show("initials:    ", values<digit> | views::transform([&](digit d) { return names[d]; }));

  // 2. partition_point bisects the grid itself:
  // the first tenth of a ℃ past 212 ℉ in 11 exact comparisons out of 2001 values
  // (a ℃ point's ℉ point is an exact shift and scale of the grid).
  using tenths = id<"[-50..150] 1/10">;
  const auto celsius   {values<tenths> | views::transform([](tenths t) { return point<si::degree_Celsius>(t); })};
  const auto fahrenheit{[](const auto& c) { return c.point_in(international::degree_Fahrenheit); }};
  const auto boiling   {point<international::degree_Fahrenheit>(212_ins)};
  const auto boil      {*rng::partition_point(celsius, [&](const auto& c) { return fahrenheit(c) <= boiling; })};
  std::cout << "above 212 ℉: " << boil << " = " << fahrenheit(boil) << "\n";
  if (boil != point<si::degree_Celsius>(100.1_ins) || to_string(fahrenheit(boil)) != "212.18 ℉")
    return 1;

  // The first thousandth whose square exceeds 2: sqrt(2), rounded up.
  using thousandths = id<"[0..2] 1/1000">;
  const auto root2{rng::partition_point(values<thousandths>, [](thousandths x) { return x * x <= 2; })};
  std::cout << "sqrt(2) <=    " << *root2 << '\n';
  if (*root2 != 1.415_ins)
    return 1;

  // 3. A ring buffer read oldest first:
  // inside_range<G>{head} starts at the next slot to overwrite and wraps once.
  using slot = id<"[0..7]">;
  const std::array<char, values<slot>.size()> ring{'i', 'j', 'c', 'd', 'e', 'f', 'g', 'h'};
  const slot                                  head{2};
  show("oldest first:", inside_range<grid_of<slot>>{head} | views::transform([&](slot s) { return ring[s]; }));

  // 4. A cursor and its end as a subrange, which pipes too: the quarter hours of a day (0 … 1440 min, both ends)
  // and the ones in office hours. The cursor is an index; the range it walks yields quantities.
  cursor<id<"[0..1440]">, 15> c;
  const auto quarters{rng::subrange{c, end(c)} | views::transform([](auto n) { return n * si::minute; })};
  const auto opens   {just<9> * si::hour};
  const auto closes  {just<17> * si::hour};
  auto       office  {quarters | views::filter([&](const auto& t) { return t >= opens && t < closes; })};
  std::cout << "quarters:     " << rng::distance(quarters) << ", in office hours: " << rng::distance(office)
            << " (" << rng::min(office) << " … " << rng::max(office) << ")\n";
  if (rng::distance(quarters) != 97 || rng::distance(office) != 32)
    return 1;
  return 0;
}
