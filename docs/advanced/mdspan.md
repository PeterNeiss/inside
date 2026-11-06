# mdspan

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`std::mdspan` holds insides and quantities as it holds any element. Two
opt-in headers add what the element type alone cannot give:

- `<beman/inside/inside/mdspan.hpp>`: indices whose type proves them in
  bounds, views of existing buffers, and exact sums and products;
- `<beman/inside/units/mdspan.hpp>`: the same for quantities, and a view of
  quantities in another unit.

[`image.cpp`](../../examples/image.cpp) blurs an image and reads a heat map
from raw sensor codes; [`matrix.cpp`](../../examples/matrix.cpp) rotates
points in Q1.14 and computes Fibonacci numbers by matrix powers.

## Indices that prove themselves

An index type is an inside on 0, 1, …, N − 1, and `extents_of` turns index
types into extents. An `inside_mdspan` is a `std::mdspan` (it derives from
one, with all its API) that also takes inside indices:

```cpp
using pixel = inside<{0, 255}>;
using row   = inside<{0, 479}>;
using col   = inside<{0, 639}>;
std::vector<pixel> data(480 * 640);
inside_mdspan<pixel, extents_of<row, col>> img{data.data()};   // extents 480 × 640

row r = 17;
col c = 300;
img[r, c] = pixel{255};     // no bounds check: the types prove it
```

An index whose grid lies within a static extent needs no check. Any other
inside index is held to the extent by its own policy, so the boundary rule is
part of the index type:

| Index policy | Past the extent |
|---|---|
| checked (the default) | reports `errc::overflow` |
| `clamp` | the nearest edge: edge padding for a filter |
| `wrap` | folded onto the extent: a periodic, torus-like boundary |
| `unsafe` | trusted, as a builtin integer |

An offset keeps the policy: with `inside<{0, 479}, clamp>` rows and
`inside<{-1, 1}, clamp>` offsets, `img[r + dr, c + dc]` reads the edge for a
neighbour past it. Builtin integer indices keep `std::mdspan`'s meaning (a
precondition, no check), and may mix with insides.

`indices(m)` walks every index tuple, the last index fastest. A static extent
yields its index type `extent_index_t<E, r>`, so indexing with them needs no
check; a dynamic extent yields integers:

```cpp
for (auto [r, c] : indices(img))
    img[r, c] = pixel{0};
```

`indices` is for clear code. The algorithms below run plain loops, which the
compiler can vectorize.

## Views of existing buffers

Two accessors view a buffer that is not of the element type, without copying:

| Accessor | Storage | Reading | Writing |
|---|---|---|---|
| `value_accessor<E, T>` | plain values of `T` (`double`, `int16_t`, …) | stored into E's number by its policy: rounded, clamped or checked | the number as a `T` |
| `raw_accessor<E>` | the number's raws (fixed-point words, ADC counts) | `from_raw`; a raw outside the grid is held by the policy | the raw |

With a `const T` buffer the view is read-only and an element is a value of
E, so every operator applies to it. A writable view gives a small reference
object: assign to it, read it with `.get()` or by converting to E.

```cpp
using adc = inside<{0, 4095}>;                                  // 12 bits in a uint16_t
std::array<std::uint16_t, 64> dma{};                            // filled by the hardware
inside_mdspan<adc, std::extents<std::size_t, 64>, std::layout_right,
              raw_accessor<adc, const std::uint16_t>> samples{dma.data()};
adc first = samples[0];                                         // a raw past 4095 is reported
```

## Exact sums and products

| Function | Result |
|---|---|
| `sum(m)` | with all extents static, the type holds every total: N·[Lower, Upper] on the element's notch, N the element count |
| `sum<Target>(m)` | any extents: the exact total, checked once by Target's policy |
| `dot(x, y)`, `dot<Target>(x, y)` | the same for the products of two vectors |
| `matrix_vector_product(A, x, y)` | y[i] = Σⱼ A[i, j]·x[j] |
| `matrix_product(A, B, C)` | C[i, j] = Σₖ A[i, k]·B[k, j] |
| `transform_into(in, out, f)`, `transform_into(in1, in2, out, f)` | out[idx] = f(in[idx], …) |

Every output element is the exact sum of exact products, stored once by the
output's policy: a rounding mode rounds it once, `clamp` saturates it, and a
checked type reports a value past its range. Nothing overflows on the way,
however large the intermediate products are:

```cpp
std::array<inside<{-100, 100}>, 4> a{3, -7, 100, 25}, b{12, 9, -4, 30};
auto d = dot(std::mdspan{a.data(), std::extents<std::size_t, 4>{}},
             std::mdspan{b.data(), std::extents<std::size_t, 4>{}});   // 323 on [−40000, 40000]
```

The names follow `std::linalg`, whose algorithms accumulate in the element
type; an inside's sums widen instead. Extents that differ are a compile error
where they are static, else `errc::domain_error`. `transform_into` is not
`transform`, which argument-dependent lookup would confuse with
`std::transform(first, last, out, f)`.

## Quantities

With `<beman/inside/units/mdspan.hpp>` the accessors and algorithms take
quantities: `raw_accessor<kelvin>` reads deci-kelvin codes as kelvin, `sum`
keeps the unit, `dot` and the matrix products multiply the units (m·m = m²),
and a target in another unit gets the total converted once.

`units::view_in(m, u)` reads the same quantities in unit `u`. An element is
`q.in(u)`, which keeps the raw, so the view costs nothing; a store through it
converts back exactly:

```cpp
using km = quantity<si::kilo(si::metre), inside<{0, 10}>>;
std::array<km, 2> legs{km{1}, km{3}};
auto mm = units::view_in(std::mdspan{legs.data(), std::extents<std::size_t, 2>{}}, si::milli(si::metre));
// mm[0].get() is 1000000 mm; the storage is still one byte per leg
```

## The header

Both headers are opt-in and need a hosted standard library (`<mdspan>`,
`<ranges>`). In the [single header](single-header.md) they sit behind
`BEMAN_INSIDE_NO_MDSPAN`.
