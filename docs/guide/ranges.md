# Ranges and the STL

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The values of a grid form a range you can loop over, and insides work with
the standard containers and algorithms.

```cpp
using bin = inside<{0, 9}>;
std::vector<int> bins(10);
for (bin b : values<bin>)            // 0, 1, …, 9
    bins[b.as<std::size_t>()] = 0;   // an index, with no check left
```

## Every value of a type: `values<T>`

`values<T>` is every point of `T`'s grid, in order, as `T`s.
`inside_range<G>` is the same range named by a grid, and it can start at any
point and wrap once past the end:

```cpp
for (auto i : inside_range<{0, 9}>{5}) std::cout << i;   // 5 6 7 8 9 0 1 2 3 4: a ring in age order
```

Both are random-access sized ranges, so views and algorithms compose with
them, and `[i]`, `size()` and `back()` cost nothing:

```cpp
inside_range<{0, 9}> r;
for (auto i : std::views::reverse(r)) { /* 9 8 … 0 */ }
for (auto i : r | std::views::stride(3)) { /* 0 3 6 9 */ }
auto it = std::ranges::find_if(values<bin>, [](bin b) { return b * b > 50_ins; });   // 8
```

## Stepping loops: `cursor<T, Step>`

`for (T t = lo; t <= hi; t += step)` does not work on an inside: the last
step needs a value past `hi`, which `T` cannot hold. A `cursor<T, Step>` can.
It steps through `T`'s range by `Step`, starting at `T`'s lower limit, and
`end(t)` is the value one step past the last:

```cpp
using time_t = inside<{{-4, 4}, per<1024>}, round_nearest>;
for (cursor<time_t, per<4>> t; t != end(t); ++t)          // −4, −3.75, …, 4
    std::cout << t << ' ';
for (cursor<time_t, 0.25_r> t = -2_ins; t <= 2_ins; ++t)  // any start, the classic test
    std::cout << t << ' ';
```

- `Step` defaults to `T`'s notch. It must be a whole number of notches and
  divide `T`'s range, which is checked at compile time.
- `*t` is the value as a `T`. A cursor is also an inside, so arithmetic,
  printing and comparison work on it directly.
- A cursor is its own bidirectional iterator, with `end(t)` as its sentinel:
  `std::ranges::find(t, end(t), 1.5_ins)`.

`values` and `cursor` take [quantities](units.md) too; each value is then a
quantity, and the step is a number in the quantity's unit.

## Standard algorithms

Insides sort, search and compare like integers, and as fast:

```cpp
using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
std::vector<celsius> temps = {21.5, -5.0, 37.0, 0.0, 15.5};
std::ranges::sort(temps);
auto hot      = std::ranges::count_if(temps, [](celsius c) { return c > 30; });
auto [lo, hi] = std::ranges::minmax_element(temps);
```

To add up a range, use [`sum<Target>`](arithmetic.md#sums-of-many-values),
which checks the total once, rather than `std::accumulate` into a wider type.

## `min`, `max`, `midpoint`

`min`, `max` and `midpoint` are found by argument-dependent lookup, so
unqualified calls in generic code work. On two different grids, the result
type holds exactly the values the result can take: `min` of
`inside<{0, 100}>` and `inside<{0, 10}>` is an `inside<{0, 10}>`. `midpoint` is
the **exact** mean, on a grid with half the notch: unlike `std::midpoint` on
integers, it neither rounds nor overflows.

```cpp
inside<{0, 100}> a = 30, b = 71;
inside<{0, 10}>  c = 4;
auto lo  = min(a, c);           // 4, an inside<{0, 10}>
auto mid = midpoint(a, b);      // 50.5, exactly
```

There is no `clamp` function: `clamp` is the policy flag. To clamp a value,
store it with the flag: `T{v, clamp}`.

## Hashing and limits

`std::hash` and `std::numeric_limits` are specialised for insides and
quantities, so an inside works as a key of `std::unordered_set` and
`std::unordered_map`:

```cpp
std::unordered_set<inside<{0, 9}>> seen;
seen.insert(inside<{0, 9}>{3});
static_assert(std::numeric_limits<inside<{0, 100}>>::max() == 100);
```

## Random values: `uniform<T>(rng)`

`<beman/inside/inside/random.hpp>` adds `uniform<T>(rng)`, which draws one
point of `T`'s grid uniformly from any standard engine:

```cpp
std::mt19937 rng{42};
auto die  = uniform<inside<{1, 6}>>(rng);                // 1 … 6
auto gain = uniform<inside<{{0, 1}, per<256>}>>(rng);    // one of the 257 points of [0, 1]
```

A seed gives the same values on every standard library, because the draw
does not go through `std::uniform_int_distribution`. A continuous grid has
no points to draw from, so it does not compile. `<beman/inside/units/random.hpp>`
adds `uniform<Q>(rng)` for quantities.

## Details

- [`ranges.cpp`](../../examples/ranges.cpp) and
  [`algorithms.cpp`](../../examples/algorithms.cpp) run these on real data.
- [Determinism](../advanced/determinism.md#other-surfaces): what a seed and a
  hash guarantee across platforms.
