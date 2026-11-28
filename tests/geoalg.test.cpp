// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Geometric algebra on units (units/geoalg.hpp): basis vectors and their
// squares, the signs of unit products against an independent bitmask
// reference, multivectors, and the operations of the algebra.

#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <sstream>
#include <string>
#include <type_traits>

namespace geoalg_tests {
using namespace ins;

namespace {
// G(3,0): x, y, z.
inline constexpr auto x = basis_vector<"x", geometric<+1>>;
inline constexpr auto y = basis_vector<"y", geometric<+1>>;
inline constexpr auto z = basis_vector<"z", geometric<+1>>;
// -(x z)
inline constexpr auto zx = unit<"zx", z * x>;
inline constexpr auto xy = unit<"xy", x * y>;

// G(1,3), spacetime: γ₀ squares to +1, γ₁ γ₂ γ₃ to -1.
inline constexpr auto g0 = basis_vector<"γ₀", geometric<+1>, {.ascii = "g0"}>;
inline constexpr auto g1 = basis_vector<"γ₁", geometric<-1>, {.ascii = "g1"}>;
inline constexpr auto g2 = basis_vector<"γ₂", geometric<-1>, {.ascii = "g2"}>;
inline constexpr auto g3 = basis_vector<"γ₃", geometric<-1>, {.ascii = "g3"}>;

// G(2,0,1), the projective plane: p₀ is a null vector.
inline constexpr auto p0 = basis_vector<"p₀", geometric<0>, {.ascii = "p0"}>;
inline constexpr auto p1 = basis_vector<"p₁", geometric<+1>, {.ascii = "p1"}>;
inline constexpr auto p2 = basis_vector<"p₂", geometric<+1>, {.ascii = "p2"}>;

// Rⁿ with the dot product: b·b = 1, b₁b₂ = 0.
inline constexpr auto b1 = basis_vector<"b₁", inner, {.ascii = "b1"}>;
inline constexpr auto b2 = basis_vector<"b₂", inner, {.ascii = "b2"}>;
inline constexpr auto b3 = basis_vector<"b₃", inner, {.ascii = "b3"}>;

// Polynomials in u and q (named t and q here): u·u = u², and they commute with everything.
// (Not "t": a quantity's text spells its units by symbol, and t is the tonne.)
inline constexpr auto t = basis_vector<"u", polynomial>;
inline constexpr auto q = basis_vector<"q", polynomial>;

constexpr auto km = si::kilo(si::metre);

template <class A, class B>
concept can_add = requires(const A& a, const B& b) { a + b; };
template <class A, class B>
concept can_equal = requires(const A& a, const B& b) { a == b; } || requires(const A& a, const B& b) { b == a; };
template <class X>
concept can_invert = requires(const X& v) { inverse(v); };
template <class V, class X>
concept can_sandwich = requires(const V& v, const X& x) { sandwich(v, x); };
template <class R, class X>
concept can_rotate = requires(const R& r, const X& x) { rotate(r, x); };

// A basis vector for the reference: its unit and its square.
struct basis_ref {
    unit_value Unit;
    int        Square;
};
// The blade of `mask` as a unit: its basis vectors multiplied in order.
template <std::size_t N>
consteval unit_value blade_unit(const std::array<basis_ref, N>& basis, unsigned mask) {
    unit_value u{};
    for (std::size_t k = 0; k < N; ++k)
        if ((mask >> k & 1U) != 0)
            u = ins::detail::unit_mul(u, basis[k].Unit, 1);
    return u;
}
// The reference: e_A e_B = s e_(A xor B), s = (-1)^(pairs a in A, b in B with
// a > b) times the square of each common vector.
template <std::size_t N>
consteval int reference_sign(unsigned a, unsigned b, const std::array<basis_ref, N>& basis) {
    int s = 1;
    for (std::size_t i = 0; i < N; ++i) {
        if ((b >> i & 1U) == 0)
            continue;
        for (std::size_t j = i + 1; j < N; ++j)
            if ((a >> j & 1U) != 0)
                s = -s;
        if ((a >> i & 1U) != 0)
            s *= basis[i].Square;
    }
    return s;
}
// Every product of blade a with each basis blade: the unit's magnitude is
// the reference sign, and its dimension the blade a xor b.
template <std::size_t N>
consteval bool products_match(const std::array<basis_ref, N>& basis, unsigned a) {
    for (unsigned b = 0; b < (1U << N); ++b) {
        const unit_value u = ins::detail::unit_mul(blade_unit(basis, a), blade_unit(basis, b), 1);
        if (magnitude_of(u) != magnitude{grid_number{reference_sign(a, b, basis)}})
            return false;
        if (reference_sign(a, b, basis) != 0 && !(dimension_of(u) == dimension_of(blade_unit(basis, a ^ b))))
            return false;
    }
    return true;
}
// One constant evaluation per row, each within the compiler's step limit.
inline constexpr std::array<basis_ref, 3> g30{basis_ref{x.value, +1}, {y.value, +1}, {z.value, +1}};
inline constexpr std::array<basis_ref, 4> g13{basis_ref{g0.value, +1}, {g1.value, -1}, {g2.value, -1}, {g3.value, -1}};
inline constexpr std::array<basis_ref, 3> g201{basis_ref{p0.value, 0}, {p1.value, +1}, {p2.value, +1}};
template <const auto& Basis, unsigned A>
inline constexpr bool row_matches = products_match(Basis, A);
template <const auto& Basis, unsigned... A>
consteval bool all_rows(std::integer_sequence<unsigned, A...>) {
    return (row_matches<Basis, A> && ...);
}
} // namespace

TEST(GeoalgTest, basis_vectors_square_and_anticommute) {
    static_assert(dimension_of((x * x).value) == dim_one);
    static_assert(magnitude_of((x * x).value).is_one());
    static_assert(magnitude_of((g1 * g1).value) == magnitude{grid_number{-1}});
    static_assert(magnitude_of((p0 * p0).value) == magnitude{grid_number{0}});
    static_assert(magnitude_of((y * x).value) == magnitude{grid_number{-1}});
    static_assert(dimension_of((y * x).value) == dimension_of((x * y).value));
    static_assert(magnitude_of((xy * xy).value) == magnitude{grid_number{-1}});
    // A named blade keeps the sign of its spelling.
    static_assert(magnitude_of(zx.value) == magnitude{grid_number{-1}});
    // A basis vector's exponent is 0 or 1.
    static_assert(dimension_of(x.value).pow(2) == dim_one);
    static_assert(dimension_of(x.value).pow(3) == dimension_of(x.value));
    static_assert(dimension_of((x / x).value) == dim_one);
    // x⁻¹ = x, (x y)⁻¹ = -(x y).
    static_assert(magnitude_of(inverse(x).value).is_one());
    static_assert(magnitude_of(inverse(xy).value) == magnitude{grid_number{-1}});
}

TEST(GeoalgTest, products_of_basis_blades_match_a_bitmask_reference) {
    static_assert(all_rows<g30>(std::make_integer_sequence<unsigned, 8>{}));
    static_assert(all_rows<g13>(std::make_integer_sequence<unsigned, 16>{}));
    static_assert(all_rows<g201>(std::make_integer_sequence<unsigned, 8>{}));
}

TEST(GeoalgTest, quantity_products_carry_the_sign) {
    const auto a = 3_q * x;
    const auto b = 4_q * y;
    EXPECT_EQ(to_string(a * b), "12 x·y");
    EXPECT_EQ(to_string(b * a), "-12 x·y");
    static_assert(decltype(a * b)::reference == decltype(b * a)::reference); // the sign is in the number
    EXPECT_TRUE(a * a == 9_q * one);
    EXPECT_TRUE((2_q * xy) * (3_q * xy) == -6_q * one);
    // A product that vanishes is the zero multivector.
    static_assert(std::same_as<decltype((1_q * p0) * (1_q * p0)), multivector<>>);
    // Division by a blade: x / y = x y (y² = 1); x / γ₁ = -x γ₁ (γ₁² = -1).
    EXPECT_EQ(to_string((6_q * x) / (2_q * y)), "3 x·y");
    EXPECT_EQ(to_string(1_q / (2_q * xy)), "-0.5 xy");
    EXPECT_EQ(to_string((1_q * g0) / (1_q * g1)), "-1 γ₀·γ₁");
    // Physical units commute with everything and print first.
    EXPECT_EQ(to_string((3_q * si::metre * x) * (2_q * si::second * y)), "6 m·s·x·y");
}

TEST(GeoalgTest, named_blades_convert_with_their_sign) {
    const auto q = 5_q * zx;
    EXPECT_EQ(to_string(q.in(x * z)), "-5 x·z");
    // A sum runs in the shorter spelling of one size.
    EXPECT_EQ(to_string(q + x * z), "4 zx");
    EXPECT_TRUE(q == -5_q * x * z);
    EXPECT_TRUE(1_q * x * y == xy);
}

TEST(GeoalgTest, multivectors_sum_per_blade_in_order) {
    const auto a = y + 2_q * x + 3_q;
    const auto b = 3_q + 2_q * x + y;
    static_assert(std::same_as<decltype(a), decltype(b)>);
    EXPECT_EQ(to_string(a), "3 + 2 x + 1 y");
    EXPECT_EQ(to_string(a - xy), "3 + 2 x + 1 y - 1 xy");
    EXPECT_EQ(to_string(-a), "-3 - 2 x - 1 y");
    EXPECT_EQ(to_string(a + a), "6 + 4 x + 2 y");
    EXPECT_EQ(to_string(a - a), "0 + 0 x + 0 y");
    EXPECT_TRUE(a - a == multivector<>{});
    // The parts of one blade add in their common unit.
    const auto d = km * x + 500_q * si::metre * x + 2_q * si::metre * y;
    EXPECT_EQ(to_string(d), "1500 m·x + 2 m·y");
    // The zero is the identity of +.
    static_assert(std::same_as<decltype(multivector<>{} + a), std::remove_cvref_t<decltype(a)>>);
    static_assert(std::same_as<decltype(multivector<>{} + 1_q * x), decltype(1_q * x)>);
    EXPECT_EQ(to_string(multivector<>{}), "0");
    // Another dimension outside the blades does not add.
    static_assert(!can_add<decltype(1_q * si::metre * x), decltype(1_q * si::second * y)>);
    static_assert(!can_add<decltype(d), decltype(1_q * si::second * z)>);
    static_assert(can_add<decltype(d), decltype(1_q * si::metre * z)>);
}

TEST(GeoalgTest, the_algebra_holds) {
    const auto a = 1_q + 2_q * x - y + 3_q * xy + 0.5_q * z;
    const auto b = 2_q - x + 4_q * zx + x * y * z;
    const auto c = -1_q * y + 2_q * z + xy;
    EXPECT_TRUE((a * b) * c == a * (b * c));
    EXPECT_TRUE(a * (b + c) == a * b + a * c);
    EXPECT_TRUE(reverse(a * b) == reverse(b) * reverse(a));
    EXPECT_TRUE(involute(a * b) == involute(a) * involute(b));
    EXPECT_TRUE(conjugate(a) == reverse(involute(a)));
    EXPECT_TRUE(grade<0>(a) + grade<1>(a) + grade<2>(a) + grade<3>(a) == a);
    EXPECT_TRUE(scalar_product(a, b) == grade<0>(a * b));
    EXPECT_TRUE(wedge(wedge(a, b), c) == wedge(a, wedge(b, c)));

    const auto u = x + 2_q * y + 3_q * z;
    const auto v = 4_q * x - 5_q * y + 6_q * z;
    const auto w = -7_q * x + 8_q * y + 9_q * z;
    EXPECT_TRUE(u * v == left_contraction(u, v) + wedge(u, v));
    EXPECT_TRUE(wedge(u, v) == -wedge(v, u));
    EXPECT_TRUE(left_contraction(u, v) == right_contraction(u, v));
    EXPECT_TRUE(left_contraction(u, wedge(v, w)) == left_contraction(u, v) * w - left_contraction(u, w) * v);
    EXPECT_TRUE(squared_norm(u) == 14);
    // In spacetime, γ₀ is timelike and γ₁ spacelike.
    EXPECT_TRUE(squared_norm(3_q * g0 + 2_q * g1) == 5);
    EXPECT_TRUE((3_q * g0 + 2_q * g1) * (3_q * g0 + 2_q * g1) == 5_q * one);
    EXPECT_TRUE(5_q * one == (3_q * g0 + 2_q * g1) * (3_q * g0 + 2_q * g1));
    EXPECT_FALSE(u == 14_q * one);
    // A multivector or a quantity never compares with a number: only plain insides do.
    using g0_squared = decltype((3_q * g0) * (3_q * g0));
    static_assert(can_equal<g0_squared, decltype(9_q * one)> && can_equal<g0_squared, decltype(one)>);
    static_assert(!can_equal<g0_squared, int> && !can_equal<g0_squared, decltype(9_q)>);
    static_assert(!can_equal<decltype(u), int> && !can_equal<decltype(u), decltype(14_q)>);
}

TEST(GeoalgTest, results_collapse) {
    const auto u = x + 2_q * y;
    static_assert(std::same_as<decltype(grade<0>(u * u)), decltype(part(u * u, one))>);
    static_assert(insidable<decltype(grade<0>(u * u))>);
    static_assert(quantity_like<decltype(wedge(1_q * x, 1_q * y))>);
    static_assert(std::same_as<decltype(wedge(1_q * x, 1_q * x)), multivector<>>);
    static_assert(std::same_as<decltype(grade<3>(u)), multivector<>>);
    EXPECT_TRUE(part(u, y) == 2);
    EXPECT_TRUE(part(u, z) == multivector<>{});
    // The coefficient of a named blade has its spelling's sign.
    const auto t = 3_q * x * z + 1_q;
    EXPECT_TRUE(part(t, zx) == -3);
    EXPECT_TRUE(part(2_q * si::metre * x + si::metre * y, x) == 2_q * si::metre);
}

TEST(GeoalgTest, inverse_when_provably_a_scalar_norm) {
    const auto z1 = 3_q + 4_q * xy;
    EXPECT_TRUE(z1 * inverse(z1) == one);
    const auto q = 1_q + 2_q * xy + 3_q * zx + 4_q * y * z;
    EXPECT_TRUE(q * inverse(q) == one);
    EXPECT_TRUE(inverse(q) * q == one);
    const auto u = x + 2_q * y + 2_q * z;
    EXPECT_TRUE(u * inverse(u) == one);
    const auto dual_number = 2_q + 3_q * p0;
    EXPECT_TRUE(dual_number * inverse(dual_number) == one);
    const auto split = 2_q + x; // G(1,0): the conjugate
    EXPECT_TRUE(split * inverse(split) == one);
    static_assert(can_invert<decltype(q)>);
    // An even element of G(1,3) with two commuting bivectors: x x~ has a
    // grade-4 part, so there is no inverse of that form.
    static_assert(!can_invert<decltype(1_q + 1_q * g0 * g1 + 1_q * g2 * g3)>);
    static_assert(!can_invert<decltype(1_q * p0)>);
    EXPECT_TRUE((3_q + 4_q * xy) / (1_q - 2_q * xy) == -1_q + 2_q * xy);

    // A grid that holds 0: the inverse is an expected.
    using gaussian = multivector<id<"[-2..2]">, id<"[-2..2] [xy]">>;
    const gaussian zero{};
    const gaussian two{2_q + 0_q * xy};
    const auto     bad  = inverse(zero);
    const auto     good = inverse(two);
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error(), error::division_by_zero);
    ASSERT_TRUE(good.has_value());
    EXPECT_TRUE((*good == 0.5_q * one));
}

TEST(GeoalgTest, declared_multivectors_convert) {
    using complex = multivector<id<"[-10..10]">, id<"[-10..10] [xy]">>;
    const complex a{3_q + 4_q * xy};
    const complex b = 2_q; // the missing part is 0
    const complex c{1_q * x * y};
    EXPECT_EQ(to_string(a), "3 + 4 xy");
    EXPECT_EQ(to_string(b), "2 + 0 xy");
    EXPECT_EQ(to_string(c), "0 + 1 xy");
    static_assert(!std::is_constructible_v<complex, decltype(1_q * z)>);
    // Narrowing a part's grid is checked, as for an inside, or takes a policy.
    using small = multivector<id<"[-1..1]">, id<"[-1..1] [xy]">>;
    const small s{a, clamp};
    EXPECT_EQ(to_string(s), "1 + 1 xy");
    EXPECT_EQ(to_string((2_q * km * x + 3_q * km * y).in(si::metre)), "2000 m·x + 3000 m·y");
}

TEST(GeoalgTest, duals_and_complements) {
    const auto u = x + 2_q * y + 3_q * z;
    const auto v = 4_q * x + 5_q * y + 6_q * z;
    EXPECT_EQ(to_string(dual(wedge(u, v), x * y * z)), "-3 x + 6 y - 3 z"); // u × v
    EXPECT_EQ(to_string(complement(u, x * y * z)), "3 x·y - 2 x·z + 1 y·z");
    const auto a = 1_q + 2_q * x - y + 3_q * xy + 5_q * x * y * z;
    EXPECT_TRUE(uncomplement(complement(a, x * y * z), x * y * z) == a);
    // In the projective plane: the pseudoscalar is the identity of the join.
    const auto i = p0 * p1 * p2;
    const auto b = p0 + 2_q * p1 * p2 - 3_q * p0 * p2 + 4_q;
    EXPECT_TRUE(regressive(1_q * i, b, i) == b);
    // Two points of the plane (grade 2): their join is the line through them.
    const auto pa   = p1 * p2 + p0 * p2; // (1, 0) up to the sign convention
    const auto pb   = p1 * p2 - 2_q * p0 * p1; // (0, 2)
    const auto line = regressive(pa, pb, i);
    static_assert(multivector_like<decltype(line)>);
    EXPECT_TRUE(regressive(line, pa, i) == 0);
    EXPECT_TRUE(regressive(line, pb, i) == 0);
}

TEST(GeoalgTest, sandwich_and_rotate_keep_the_grades) {
    const auto v = 4_q * si::metre * x + 5_q * si::metre * y + 6_q * si::metre * z;
    // A reflection: sandwich(n, v) is -n v n⁻¹ without its grade-3 part, which is 0.
    const auto n = x + y;
    static_assert(std::same_as<decltype(sandwich(n, v)), decltype(grade<1>(-n * v * inverse(n)))>);
    EXPECT_TRUE(sandwich(n, v) == grade<1>(-n * v * inverse(n)));
    EXPECT_TRUE(grade<3>(-n * v * inverse(n)) == multivector<>{});
    // A rotor of norm 2 turns by 90° about z, exactly: V v V⁻¹.
    const auto r = 1_q - x * y;
    EXPECT_TRUE(sandwich(r, v) == -5_q * si::metre * x + 4_q * si::metre * y + 6_q * si::metre * z);
    // A unit rotor: rotate is R v R̃, no division; the 120° turn about the grey axis.
    const auto t = 0.5_q - 0.5_q * x * y - 0.5_q * y * z - 0.5_q * z * x;
    EXPECT_TRUE(rotate(t, v) == 6_q * si::metre * x + 4_q * si::metre * y + 5_q * si::metre * z);
    EXPECT_TRUE(rotate(t, v) == sandwich(t, v));
    // A rotor whose norm's grid holds 0: sandwich is an expected, rotate scales by |q|² (here ½).
    using unit_part = id<"[-1..1] 1/1024">;
    const multivector<unit_part, id<"[-1..1] 1/1024 [xy]">> q{unit_part{0.5}, id<"[-1..1] 1/1024 [xy]">{0.5, xy}};
    const auto s = sandwich(q, v);
    static_assert(detail::is_expected_v<decltype(s)>);
    ASSERT_TRUE(s.has_value());
    EXPECT_TRUE(*s == 5_q * si::metre * x + -4_q * si::metre * y + 6_q * si::metre * z);
    EXPECT_TRUE((rotate(q, v) == 2.5_q * si::metre * x - 2_q * si::metre * y + 3_q * si::metre * z));
    // A boost in G(1,3), 2 + γ₀γ₁ of norm 3: the vector stays a vector, its grade-3 part is exactly 0.
    const auto b = 2_q + g0 * g1;
    const auto u = 1_q * g0 + 2_q * g2;
    EXPECT_TRUE(grade<3>(b * u * inverse(b)) == multivector<>{});
    EXPECT_TRUE(sandwich(b, u) == grade<1>(b * u * inverse(b)));
    // No versor: mixed parity, inner vectors; rotate takes even ones only.
    static_assert(!can_sandwich<decltype(1_q + x), decltype(v)>);
    static_assert(!can_sandwich<decltype(b1 + b2), decltype(b1)>);
    static_assert(!can_rotate<decltype(n), decltype(v)> && can_rotate<decltype(t), decltype(v)>);
}

TEST(GeoalgTest, inner_basis_vectors_multiply_by_the_dot_product) {
    static_assert(dimension_of((b1 * b1).value) == dim_one);
    static_assert(magnitude_of((b1 * b1).value).is_one());
    static_assert(magnitude_of((b1 * b2).value) == magnitude{grid_number{0}});
    static_assert(std::same_as<decltype((1_q * b1) * (1_q * b2)), multivector<>>);
    // The product of two vectors is their dot product, a number.
    const auto u = b1 + 2_q * b2 + 3_q * b3;
    const auto v = 4_q * b1 + 5_q * b2 + 6_q * b3;
    static_assert(insidable<decltype(u * v)>);
    EXPECT_TRUE(u * v == 32);
    EXPECT_EQ(to_string(u), "1 b₁ + 2 b₂ + 3 b₃");
    EXPECT_EQ(to_string(u, text_encoding::ascii), "1 b1 + 2 b2 + 3 b3");
    EXPECT_TRUE(part(u, b2) == 2);
    // With units: the dot product of two lengths is an area.
    const auto w = 2_q * si::metre * b1 + si::metre * b2;
    EXPECT_TRUE(w * w == 5_q * square(si::metre));
    EXPECT_TRUE((u * v) * w == 64_q * si::metre * b1 + 32_q * si::metre * b2);
    // Not associative: (b₁b₁)b₂ = b₂, but b₁(b₁b₂) = 0.
    EXPECT_TRUE(((1_q * b1) * (1_q * b1)) * (1_q * b2) == b2);
    static_assert(std::same_as<decltype((1_q * b1) * ((1_q * b1) * (1_q * b2))), multivector<>>);
    // The inverse is w / (w·w): w·w⁻¹ = 1.
    EXPECT_TRUE(w * inverse(w) == 1);
    EXPECT_EQ(to_string(inverse(w)), "0.4 b₁/m + 0.2 b₂/m");
}

TEST(GeoalgTest, polynomial_variables_add_powers_and_commute) {
    static_assert(dimension_of((t * t).value) == dimension_of(t.value).pow(2));
    static_assert(dimension_of((q * t).value) == dimension_of((t * q).value));
    static_assert(magnitude_of((q * t).value).is_one());
    // Sums across powers are polynomials, in order of degree.
    const auto p = t * t + 2_q * t + 3_q;
    static_assert(std::same_as<std::remove_const_t<decltype(p)>, decltype(3_q + 2_q * t + 1_q * t * t)>);
    EXPECT_EQ(to_string(p), "3 + 2 u + 1 u²");
    EXPECT_TRUE(part(p, t * t) == 1);
    // Products multiply out; a cancelled term stays in the type.
    const auto a = 1_q + t;
    EXPECT_TRUE(a * a == 1_q + 2_q * t + t * t);
    EXPECT_EQ(to_string(a * (1_q - t)), "1 + 0 u - 1 u²");
    // Two variables, by total degree, then the higher power of the first variable first.
    const auto b = t + q;
    EXPECT_TRUE(b * b == q * q + 2_q * q * t + t * t);
    // Coefficients with units: one dimension but for the variables.
    const auto s = 2_q * si::metre + 3_q * si::metre * t;
    EXPECT_EQ(to_string(s), "2 m + 3 m·u");
    static_assert(!can_add<decltype(s), decltype(1_q * si::second * t)>);
    EXPECT_TRUE(s.in(si::centi(si::metre)) == 200_q * si::centi(si::metre) + 300_q * si::centi(si::metre) * t);
    // Vector coefficients: a polynomial curve in G(3,0), (x + 2 t x)² = 1 + 4 t + 4 t².
    const auto c = x + 2_q * t * x;
    EXPECT_TRUE(c * c == 1_q + 4_q * t + 4_q * t * t);
    EXPECT_TRUE(reverse(c) == c);
    // A negative power: 1 / t.
    EXPECT_EQ(to_string(1_q / t), "1 u⁻¹");
}

TEST(GeoalgTest, text) {
    const auto z1 = 3_q - 4_q * xy;
    EXPECT_EQ(to_string(z1), "3 - 4 xy");
    EXPECT_EQ(to_string(1_q * g0 * g1 * si::metre, text_encoding::ascii), "1 m*g0*g1");
    EXPECT_EQ(std::format("[{:>10}]", z1), "[  3 - 4 xy]");
    std::ostringstream os;
    os << z1 << ' ' << multivector<>{};
    EXPECT_EQ(os.str(), "3 - 4 xy 0");
}
} // namespace geoalg_tests
