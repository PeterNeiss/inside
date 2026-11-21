// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Colours as vectors: the channels R, G and B are inner basis vectors (b·b = 1, b₁b₂ = 0), so lights add as
// vectors, and the product of two colours is their dot product. Luminance, the CIE XYZ coordinates and a filter
// are dot products; a turn of hue is a rotation of geometric algebra.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace
{
  // Linear sRGB and CIE XYZ: two orthonormal bases of their own.
  inline constexpr auto R = basis_vector<"R", inner>;
  inline constexpr auto G = basis_vector<"G", inner>;
  inline constexpr auto B = basis_vector<"B", inner>;
  inline constexpr auto X = basis_vector<"X", inner>;
  inline constexpr auto Y = basis_vector<"Y", inner>;
  inline constexpr auto Z = basis_vector<"Z", inner>;
  // The same three channels as vectors of G(3,0), for rotations; i turns the chroma plane.
  inline constexpr auto e1 = basis_vector<"e₁", geometric<+1>, {.ascii = "e1"}>;
  inline constexpr auto e2 = basis_vector<"e₂", geometric<+1>, {.ascii = "e2"}>;
  inline constexpr auto e3 = basis_vector<"e₃", geometric<+1>, {.ascii = "e3"}>;
  inline constexpr auto i  = unit<"i", e1 * e2>;

  // Linear light relative to white, to the 1/1000; a colour's parts print in name order, B G R.
  using channel = inside<{{0, 1}, per<1000>}, round_half_away>;
  using colour  = multivector<quantity<B, channel>, quantity<G, channel>, quantity<R, channel>>;

  // The rows of the sRGB to XYZ matrix (D65): X = rx·c, Y = ry·c (the luminance), Z = rz·c.
  inline constexpr auto rx{0.4124_ins * R + 0.3576_ins * G + 0.1805_ins * B};
  inline constexpr auto ry{0.2126_ins * R + 0.7152_ins * G + 0.0722_ins * B};
  inline constexpr auto rz{0.0193_ins * R + 0.1192_ins * G + 0.9505_ins * B};

  template <class C>
  constexpr auto to_xyz(const C& c)
  {
    return (c * rx) * X + (c * ry) * Y + (c * rz) * Z;
  }

  // A red glass passes 9/10 of red, 1/5 of green and 1/10 of blue.
  template <class C>
  constexpr auto red_glass(const C& c)
  {
    return just<9, 10> * (c * R) * R + just<1, 5> * (c * G) * G + just<1, 10> * (c * B) * B;
  }
} // namespace

int main()
{
  // 1. Lights add as vectors (Grassmann's law of colour mixing), and each channel's range is proved:
  // two colours sum to [0, 2] per channel, and the store into a colour clamps the sum to the gamut.
  const colour orange{R + just<1, 2> * G};
  const colour sky   {just<1, 5> * R + just<1, 2> * G + B};
  const auto   sum   {orange + sky};
  static_assert(upper_of<decltype(part(sum, R))> == grid_number{2});
  const colour shown {sum, clamp};
  std::cout << "orange + sky = " << sum << ", shown " << shown << '\n';
  if (!(shown == R + G + B))
    return 1;

  // 2. The product of two colours is their dot product: c·R is c's red channel, c·ry its luminance.
  // RGB to XYZ is three dot products; white goes to D65's white point, exactly.
  const colour white{R + G + B};
  std::cout << "orange·R = " << orange * R << ", luminance " << orange * ry << ", XYZ(white) = " << to_xyz(white)
            << '\n';
  if (!(orange * R == 1) || !(orange * ry == 0.5702_ins)
      || !(to_xyz(white) == 0.9505_ins * X + Y + 1.089_ins * Z))
    return 1;

  // 3. A filter passes a share of each channel: a diagonal map, ρ_R (c·R) R + ρ_G (c·G) G + ρ_B (c·B) B,
  // not a product (two colours multiply to a number).
  std::cout << "sky through red glass = " << red_glass(sky) << '\n';
  if (!(red_glass(sky) == just<18, 100> * R + just<1, 10> * G + just<1, 10> * B))
    return 1;

  // 4. A turn of hue is a rotation about the grey axis. The 120° rotor ½ - ½(e₁e₂ + e₂e₃ + e₃e₁) is exact
  // (cos 60° and sin 60° / √3 are both ½), and it moves red to green, green to blue, blue to red.
  const auto   v     {(orange * R) * e1 + (orange * G) * e2 + (orange * B) * e3};
  const auto   rotor {just<1, 2> - just<1, 2> * e1 * e2 - just<1, 2> * e2 * e3 - just<1, 2> * e3 * e1};
  const auto   w     {rotate(rotor, v)};
  const colour turned{part(w, e1) * R + part(w, e2) * G + part(w, e3) * B, clamp};
  std::cout << "orange turned 120° = " << turned << '\n';
  if (!(turned == G + just<1, 2> * B))
    return 1;

  // Any angle: the chroma a + b i of an opponent space (Lab) turns by e^(iθ), i·i = -1. A quarter turn is i,
  // exactly; e^(iπ/6) rounds each part once, onto 1/1000.
  using fine = inside<{{-4, 4}, per<(1 << 20)>}, round_half_away>;
  const fine pi = math::pi;
  const auto chroma{just<3, 10> + just<4, 10> * i};
  const auto turn  {math::exp<per<1000>>(pi / just<6> * i)};
  std::cout << "chroma " << chroma << ": turned 90° " << chroma * i << ", by e^(iπ/6) = " << turn << ": "
            << chroma * turn << '\n';
  if (!(chroma * i == just<-4, 10> + just<3, 10> * i) || !(chroma * turn == 0.0598_ins + 0.4964_ins * i))
    return 1;
  return 0;
}
