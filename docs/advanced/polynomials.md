# Polynomials

A polynomial's variable is a basis vector with the rule `polynomial`: its
exponent adds as a unit's does (x·x = x²), and it commutes with everything. A
sum of different powers is then a multivector of
[geometric algebra](geometric-algebra.md#two-other-rules-inner-and-polynomial),
one part per power, and each coefficient keeps its own inside, so its range is
proved. `+`, `-`, `*` and the division by one term come from
`<inside/units/geoalg.hpp>`; `<inside/units/polynomial.hpp>` adds the rest,
all of it exact.

```cpp
#include <inside/units/polynomial.hpp>

inline constexpr auto x = basis_vector<"x", polynomial>;
inline constexpr auto y = basis_vector<"y", polynomial>;

auto p = just<3> + just<2> * x + just<1> * x * x;   // 3 + 2 x + 1 x²
auto s = (just<1> + just<1> * x) * (just<1> - just<1> * x);   // 1 + 0 x - 1 x²
```

A term that cancels stays in the type with the value 0, as `0 x` above: the
type is fixed before the values are known.

## Operations

| Call | Result |
|---|---|
| `substitute(p, x, a)` | p with x replaced by a, by Horner's rule: a number or a quantity gives the value, a polynomial the composition p∘a |
| `derivative(p, x)` | c·xᵏ becomes k·c·xᵏ⁻¹ |
| `antiderivative(p, x)` | c·xᵏ becomes c/(k+1)·xᵏ⁺¹, with constant term 0 |
| `divmod(p, d)` | `{Quotient, Remainder}` with p = q·d + r, r's degree below d's |
| `part(p, x * x)` | the coefficient of x² |

```cpp
auto v = substitute(p, x, just<2>);                      // 11
auto c = substitute(p, x, just<1> * y + just<1>);        // 6 + 4 y + 1 y²
auto F = antiderivative(p, x);                           // 3 x + 1 x² + 1/3 x³
auto I = substitute(F, x, just<2>) - substitute(F, x, just<0>);   // 38/3
auto [q, r] = divmod(p, just<1> + just<2> * x);          // 0.75 + 0.5 x, 2.25
```

## Ranges

`substitute` evaluates with the types' interval arithmetic, so the value's
range follows from the coefficients' and the argument's: coefficients in
[−10, 10] and an argument in [0, 2] prove 2 − 3 x + x² in [−70, 70]. The bound
is safe but not tight, since each coefficient's range counts separately.

## Division

`divmod` divides polynomials in one variable. Each step divides by d's leading
coefficient, exactly, so the quotient's coefficients are fractions on bounded
continuous grids. When that coefficient's grid excludes 0, the result is a
plain `divmod_result`; when it holds 0, a `std::expected` that carries
`error::division_by_zero` if it is 0. A divisor whose leading coefficient is
not 1 multiplies the fractions' bound at each step, so a high degree can pass
the 1024-bit limit of exact arithmetic and does not compile.

## Limits

- **Numbers for x.** The parts of a polynomial share one dimension outside the
  variables, so `substitute` takes an argument without a unit (or a polynomial).
  A variable that stands for a quantity, such as a time in seconds, is not
  modelled.
- **Non-negative powers for `substitute` and `divmod`.** `1 / x` is a part
  (x⁻¹), and `derivative` takes it; `antiderivative` refuses x⁻¹ (its
  antiderivative is a logarithm).
- **One variable for `divmod`.** Division with a remainder is unique only in
  one variable. `substitute`, `derivative` and `antiderivative` take any number.
- **No `exp`** of a variable, and no division by a polynomial of several terms
  other than `divmod`.
- **Compile time.** A polynomial's type holds every part, and each operation
  forms its result type part by part.

[`polynomials.cpp`](../../examples/polynomials.cpp) shows the operations on
one polynomial.
