# Geometric algebra

`<inside/units/geoalg.hpp>` adds the geometric algebra of the units: a
basis vector is a unit of your own, and you state only its square. Sums of
different blades are multivectors, and the products, involutions, duals and
inverses of the algebra work on them, with physical units and exact numbers.
Complex numbers and quaternions are two special cases, not two features.
Two other rules share the machinery: the vectors of Rⁿ with the dot product,
and the variables of [polynomials](polynomials.md).
[Units](../guide/units.md#geometric-algebra) has the short version.

## Basis vectors

```cpp
// e₁ e₁ = 1
inline constexpr auto         e1 = basis_vector<"e₁", geometric<+1>, {.ascii = "e1"}>;
inline constexpr auto         e2 = basis_vector<"e₂", geometric<+1>, {.ascii = "e2"}>;
// a blade, named
inline constexpr auto         i = unit<"i", e1 * e2>;
```

A `basis_vector` is a base dimension of your own and its unit, in one static
object. Its second argument is its rule: `geometric<S>` gives its square S,
+1, −1 or 0 (a null vector). Its options are `.ascii` and `.space_before`, as
for a named unit. Two different basis vectors anticommute: the basis is
orthogonal. That is all the algebra needs, and every signature G(p, q, r) has
such a basis. A non-orthogonal vector, such as the e₀ and e∞ of conformal GA,
is a multivector over e₊ and e₋.

| Algebra | Basis vectors | |
|---|---|---|
| complex numbers | e₁, e₂ (+1) | the even part: 1 and i = e₁e₂, i² = −1 |
| quaternions | e₁, e₂, e₃ (+1) | the even part: 1, i = e₁e₂, j = e₃e₁, k = e₂e₃ |
| vectors in space | e₁, e₂, e₃ (+1) | G(3,0): the full algebra |
| spacetime | γ₀ (+1), γ₁, γ₂, γ₃ (−1) | G(1,3) |
| the projective plane | e₀ (0), e₁, e₂ (+1) | G(2,0,1), points and lines of PGA |
| dual numbers | ε (0) | a + b ε with ε² = 0: exact derivatives |
| split-complex numbers | j (+1) | G(1,0) |

## Two other rules: `inner` and `polynomial`

```cpp
inline constexpr auto         R = basis_vector<"R", inner>;        // R·R = 1, R·G = 0
inline constexpr auto         G = basis_vector<"G", inner>;
inline constexpr auto         x = basis_vector<"x", polynomial>;   // x·x = x²

auto c = just<4, 5> * R + just<1, 2> * G;
auto l = c * (just<1, 5> * R + just<7, 10> * G);   // 0.51: the dot product, a number
auto p = just<3> + just<2> * x + x * x;            // 3 + 2 x + 1 x²
```

| Rule | v·v | two different ones | exponent |
|---|---|---|---|
| `geometric<S>` | S (+1, −1, 0) | anticommute: a blade | 0 or 1 |
| `inner` | 1 | 0 | 0 or 1 |
| `polynomial` | x² | commute: a monomial | any, as a unit's |

An **inner** basis vector is a vector of Rⁿ with the dot product: the product
of two vectors is a number (with units, two lengths give an area), and the
product of two different inner vectors is the zero multivector. The product is
not associative: (b₁b₁)b₂ = b₂, but b₁(b₁b₂) = 0, as (u·v)w ≠ u(v·w), and
`u * v * w` groups from the left. `inverse(c)` is c / (c·c). The wedge, the
contractions, the scalar product, duals and `exp` take geometric basis vectors
only, and an inner vector and a geometric one do not multiply.
[`colorspace.cpp`](../../examples/colorspace.cpp) makes R, G and B inner: a
channel, the luminance, the map to CIE XYZ and a filter are dot products.

A **polynomial** basis vector is a variable: its exponent adds as a unit's
does, and it commutes with everything. A sum of different powers is a
multivector, its parts in order of total degree (then the higher power of the
first variable first: 1, x, y, x², x y, y²). Coefficients may carry units or
blades, `1 / x` is x⁻¹, and `part(p, x * x)` is x²'s coefficient. Dividing by a
polynomial of several terms is `divmod`, and the rest of the calculus is in
[polynomials](polynomials.md).

## Products of units carry a sign

A blade is the set of basis vectors of a dimension; its grade is their number,
and its canonical value is their product in name order. For canonical blades
A and B,

> A B = s (A ⊕ B),  s = (−1)^(pairs a ∈ A, b ∈ B with a > b) × the square of each vector in both.

So a product of two units is again a unit, up to a sign. A unit's magnitude
carries that sign: `e2 * e1` is −1 times e₁ e₂, and a named unit keeps the sign
of its spelling (j := e₃e₁ is −1 times the canonical e₁e₃). When two
quantities multiply, the sign goes to the number, so the unit in a quantity's
type never has one:

```cpp
auto a = just<3> * e1;
auto b = just<4> * e2;
auto p = b * a;                      // -12 e₁·e₂
auto q = (just<2> * i) * (just<3> * i);   // -6: i i is a number
```

A product keeps its factors' spelling when their blades share no basis vector
(1·i stays i, and 2 m times e₁ is 2 m·e₁). Otherwise the blade of the product is
spelled by its basis vectors in name order: i·i is a number, and the
quaternions' i·j (both hold e₁) is 1 e₂ e₃, which is k. A sum picks the shorter of two spellings of one size, so the k part
of a product of two quaternions prints as k. A physical unit commutes with
everything and prints first: `2 m·e₁`. A product that vanishes (ε ε) is the
zero, `multivector<>`.

Basis vectors divide too: B⁻¹ = B B² for a blade whose square is ±1, so
x / y is x y when y² = 1. A blade with a null vector has no inverse, and
dividing by it is a compile error.

## Multivectors

A sum of quantities of different blades is a `multivector`:

```cpp
auto z = just<3> + just<4> * i;                                    // 3 + 4 i
auto v = just<2> * si::metre * e1 + si::metre * e2;                // 2 m·e₁ + 1 m·e₂
```

- Its **parts** are insides (the scalar part, when it is a plain number) and
  quantities, in canonical order: by grade, then by the names of the basis
  vectors. Each part keeps its own inside, so each part's range is proved, and
  the arithmetic is exact.
- All parts have **one dimension but for their blades**: a vector in metres has
  no part in seconds.
- A result with **one part** is that quantity or inside, and one with **none** is
  the zero `multivector<>`, which adds to anything.
- A **declared type** fixes the parts' grids:
  `multivector<inside<{-10, 10}>, quantity<i, inside<{-10, 10}>>>`. It takes any
  element whose blades are among its own (the missing parts are 0), converting
  each part as the part converts, and `{x, clamp}` applies a policy to each
  part's store.

## Operations

| Spelling | Gives |
|---|---|
| `a + b`, `a - b`, `-a` | per blade; parts of one blade add in their common unit |
| `a * b` | the geometric product |
| `wedge(a, b)` | the outer product a ∧ b: the pairs of blades with no basis vector in common |
| `left_contraction(a, b)`, `right_contraction(a, b)` | a ⌋ b (the pairs with A ⊆ B) and a ⌊ b (B ⊆ A) |
| `scalar_product(a, b)` | ⟨a b⟩₀ |
| `reverse(x)`, `involute(x)`, `conjugate(x)` | each grade k times (−1)^(k(k−1)/2), (−1)^k, (−1)^(k(k+1)/2) |
| `grade<K>(x)` | the parts of grade K |
| `part(x, u)` | the coefficient of blade u (a unit of a blade, or `one`), with the sign of u's spelling: 2 m for 2 m·e₁; the zero when x has no such part |
| `x * u`, `x.in(u)` | each part times a unit; each part in unit u, its blade kept: `(V / Z).in(si::ampere)` |
| `squared_norm(x)` | ⟨x x̃⟩₀ = Σ partᵢ · reverse(partᵢ): \|z\|², \|q\|², \|v\|² |
| `inverse(x)`, `a / b` | α(x) / (x α(x)), α the reverse or the Clifford conjugate, when that is a scalar (below) |
| `a / s` | each part divided by s, a number or a quantity of one blade |
| `dual(x, I)` | x I⁻¹, for a pseudoscalar unit I that squares to ±1: `dual(u ∧ v, e1 * e2 * e3)` is u × v |
| `complement(x, I)`, `uncomplement(x, I)` | the right complement within I's blade, A ∧ c(A) = I, without the metric, and its inverse |
| `regressive(a, b, I)` | the regressive product a ∨ b = c⁻¹(c(a) ∧ c(b)): the join of PGA |
| `==` | per blade; a missing part compares as 0. With multivectors and quantities only, never a number: `q * inverse(q) == one`, `i * i == -one`, `z * reverse(z) == just<25> * one` |
| `to_string`, `<<`, `std::format` | units/io.hpp: `3 - 4 i`, `2 m·e₁ + 1 m·e₂`, `0` |

A division by a grid that holds 0 gives an `std::expected`, as `/` does
everywhere: `inverse` of a multivector whose norm's grid holds 0 is an
`expected` with `error::division_by_zero` when it is 0.

### When `inverse` exists

x⁻¹ = x̃ / (x x̃) holds when x x̃ is a scalar. The library decides that from
x's type, at compile time: for each pair of parts i < j, the two cross terms
Bᵢ α(Bⱼ) and Bⱼ α(Bᵢ) must cancel. That holds for the reverse on complex
numbers, quaternions, rotors and vectors, and for the Clifford conjugate on
the dual and split-complex numbers. It fails for the even elements of a 4-D
algebra (two commuting bivectors leave a grade-4 part), and `inverse` is then
a deleted overload that says so.

## exp

`math::exp(x)` (from `<inside/units/math.hpp>`) takes a multivector,
or a quantity of one blade, and gives every part of its output (its
CoDomain) correctly rounded, each onto its own grid by its own mode. Write
x = a + y,
a the scalar part. When every two of y's parts anticommute, y·y is a scalar
for every value of the type, and with z = −y·y and λ = √|z|

  exp(x) = e^a (cos λ + y sin λ / λ)       for z > 0 (complex, quaternions, rotors),
  exp(x) = e^a (cosh λ + y sinh λ / λ)     for z < 0 (boosts, split-complex),
  exp(x) = e^a (1 + y)                     for z = 0 (dual numbers, light-like).

The library checks the anticommuting parts from the type, at compile time;
e₁e₂ + e₃e₄ in a 4-D algebra has no such form, and `exp` is then a deleted
overload that says so. The parts may carry blades and `rad`, nothing
physical.

The output is deduced as for the scalar functions: the scalar part and each
blade of y, in x's units, every part's range a bound computed at compile time
(so its value needs no range check), on x's notch, or the one you name:
`math::exp<per<(1 << 20)>>(x)`. `math::exp<C>(x)` names it: a multivector that
holds the scalar part and every blade of y (a part it has beyond them is 0,
and a part spelled with a sign, j = e₃e₁, gets its sign), or one inside, the
number of every part.

```cpp
using coef  = inside<{{-1, 1}, per<(1 << 20)>}, round_half_away>;
using part  = inside<{{-1, 1}, per<(1 << 16)>}, round_half_away>;
using rotor = multivector<part, quantity<i, part>, quantity<j, part>, quantity<k, part>>;
const coef c{just<6045997880780726, 10000000000000000>};    // π/(3√3)
math::exp<rotor>(c * i + c * j + c * k);                    // ½ + ½ i + ½ j + ½ k
math::exp<part>(c * i + c * j + c * k);                     // the same rotor
math::exp<per<(1 << 16)>>(c * i + c * j + c * k);           // the same, its range ±(1 + 2^-16)
```

e^a, cos λ and sin λ / λ are computed once per call and each part is decided
from its own error bound, in `double` for parts of up to 47 bits, else in
double-double, and on the integer path where neither decides; the same
integer path runs at compile time. So the result is the same in every build.
A rotor of G(3,0) onto a grid of 2^-20 costs about what `std::sqrt`,
`std::sin`, `std::cos` and three products in `double` cost
([performance](../performance.md)).

## Examples

```cpp
auto w  = just<1> - just<2> * i;
auto zw = z * w;                     // 11 - 2 i
auto zq = z / w;                     // -1 + 2 i, exactly
auto r  = v * i;                     // -1 m·e₁ + 2 m·e₂: a quarter turn
```

- [`complex.cpp`](../../examples/complex.cpp) covers five things:
  - z·w, the conjugate, |z|² and z / w;
  - turns of the plane;
  - Gaussian integers whose part ranges are proved in the type;
  - the current through an impedance in Ω;
  - e^(iπ) = −1 on a grid of 2^-20.
- [`quaternions.cpp`](../../examples/quaternions.cpp) covers five things:
  - Hamilton's rules at compile time;
  - Euler's four-square identity;
  - an exact turn of a vector in metres by q v q⁻¹;
  - q³ = −1;
  - the same q as exp of its angle times its axis.
- [`geometric_algebra.cpp`](../../examples/geometric_algebra.cpp) covers four things:
  - u v = u⌋v + u∧v with lengths;
  - a reflection;
  - the volume and the cross product as a dual;
  - the exact derivative from the dual numbers.
- [`colorspace.cpp`](../../examples/colorspace.cpp) covers four things:
  - lights that add as vectors of inner basis vectors, clamped to the gamut;
  - the luminance and the map to CIE XYZ as dot products;
  - a filter as a diagonal map;
  - a turn of hue: the exact 120° rotor about the grey axis, and e^(iθ) on
    the chroma plane.

## Limits

- **Orthogonal bases only.** You state each basis vector's square; a metric with
  off-diagonal terms is a change of basis away.
- **Six atoms per unit.** A unit holds at most 6 atoms, physical and blade
  together, as everywhere in the units. A blade spelled by its basis vectors
  uses one per vector, so m·e₁·e₂·e₃·e₄·e₅ fits, and CGA's pseudoscalar times
  N·m does not.
- **exp only.** There is no log or sqrt of a rotor, and exp needs the closed
  form: its argument's non-scalar parts must anticommute.
- **No sandwich helper.** R v R̃ keeps every grade the product can have. A
  reflection or rotation of a vector has a grade-3 part that is 0; take
  `grade<1>` of it.
- **No hooks on whole multivectors.** inside's store hooks (`on_wrap`, …) and
  the algorithms on quantities work on the parts, not on a whole multivector.
- **Compile time.** Each multivector operation forms its result type part by
  part. A product of two quaternions is 16 quantity products and their sums.
