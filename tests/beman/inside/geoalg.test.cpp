// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Geometric algebra on units (units/geoalg.hpp): basis vectors and their
// squares, the signs of unit products against an independent bitmask
// reference, multivectors, and the operations of the algebra.

#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/io.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <sstream>
#include <string>
#include <type_traits>

namespace geoalg_tests {
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// G(3,0): x, y, z.
inline constexpr basis_vector x_def{"x", +1};
inline constexpr basis_vector y_def{"y", +1};
inline constexpr basis_vector z_def{"z", +1};
inline constexpr auto         x = unit_c<x_def>;
inline constexpr auto         y = unit_c<y_def>;
inline constexpr auto         z = unit_c<z_def>;
inline constexpr named_unit   zx_def{"zx", z * x}; // -(x z)
inline constexpr auto         zx = unit_c<zx_def>;
inline constexpr named_unit   xy_def{"xy", x * y};
inline constexpr auto         xy = unit_c<xy_def>;

// G(1,3), spacetime: γ₀ squares to +1, γ₁ γ₂ γ₃ to -1.
inline constexpr basis_vector g0_def{"γ₀", +1, {.ascii = "g0"}};
inline constexpr basis_vector g1_def{"γ₁", -1, {.ascii = "g1"}};
inline constexpr basis_vector g2_def{"γ₂", -1, {.ascii = "g2"}};
inline constexpr basis_vector g3_def{"γ₃", -1, {.ascii = "g3"}};
inline constexpr auto         g0 = unit_c<g0_def>;
inline constexpr auto         g1 = unit_c<g1_def>;
inline constexpr auto         g2 = unit_c<g2_def>;
inline constexpr auto         g3 = unit_c<g3_def>;

// G(2,0,1), the projective plane: p₀ is a null vector.
inline constexpr basis_vector p0_def{"p₀", 0, {.ascii = "p0"}};
inline constexpr basis_vector p1_def{"p₁", +1, {.ascii = "p1"}};
inline constexpr basis_vector p2_def{"p₂", +1, {.ascii = "p2"}};
inline constexpr auto         p0 = unit_c<p0_def>;
inline constexpr auto         p1 = unit_c<p1_def>;
inline constexpr auto         p2 = unit_c<p2_def>;

constexpr auto km = si::kilo(si::metre);

template <class A, class B>
concept can_add = requires(const A& a, const B& b) { a + b; };
template <class X>
concept can_invert = requires(const X& v) { inverse(v); };

// The blade of `mask` as a unit: its basis vectors multiplied in order.
template <std::size_t N>
consteval unit blade_unit(const std::array<const basis_vector*, N>& basis, unsigned mask) {
    unit u{};
    for (std::size_t k = 0; k < N; ++k)
        if ((mask >> k & 1U) != 0)
            u = units::detail::unit_mul(u, unit{basis[k]->Unit}, 1);
    return u;
}
// The reference: e_A e_B = s e_(A xor B), s = (-1)^(pairs a in A, b in B with
// a > b) times the square of each common vector.
template <std::size_t N>
consteval int reference_sign(unsigned a, unsigned b, const std::array<const basis_vector*, N>& basis) {
    int s = 1;
    for (std::size_t i = 0; i < N; ++i) {
        if ((b >> i & 1U) == 0)
            continue;
        for (std::size_t j = i + 1; j < N; ++j)
            if ((a >> j & 1U) != 0)
                s = -s;
        if ((a >> i & 1U) != 0)
            s *= basis[i]->Self.Square;
    }
    return s;
}
// Every product of blade a with each basis blade: the unit's magnitude is
// the reference sign, and its dimension the blade a xor b.
template <std::size_t N>
consteval bool products_match(const std::array<const basis_vector*, N>& basis, unsigned a) {
    for (unsigned b = 0; b < (1U << N); ++b) {
        const unit u = units::detail::unit_mul(blade_unit(basis, a), blade_unit(basis, b), 1);
        if (magnitude_of(u) != magnitude{grid_rational{reference_sign(a, b, basis)}})
            return false;
        if (reference_sign(a, b, basis) != 0 && !(dimension_of(u) == dimension_of(blade_unit(basis, a ^ b))))
            return false;
    }
    return true;
}
// One constant evaluation per row, each within the compiler's step limit.
inline constexpr std::array<const basis_vector*, 3> g30{&x_def, &y_def, &z_def};
inline constexpr std::array<const basis_vector*, 4> g13{&g0_def, &g1_def, &g2_def, &g3_def};
inline constexpr std::array<const basis_vector*, 3> g201{&p0_def, &p1_def, &p2_def};
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
    static_assert(magnitude_of((g1 * g1).value) == magnitude{grid_rational{-1}});
    static_assert(magnitude_of((p0 * p0).value) == magnitude{grid_rational{0}});
    static_assert(magnitude_of((y * x).value) == magnitude{grid_rational{-1}});
    static_assert(dimension_of((y * x).value) == dimension_of((x * y).value));
    static_assert(magnitude_of((xy * xy).value) == magnitude{grid_rational{-1}});
    // A named blade keeps the sign of its spelling.
    static_assert(zx_def.Mag == magnitude{grid_rational{-1}});
    // A basis vector's exponent is 0 or 1.
    static_assert(dimension_of(x.value).pow(2) == dim_one);
    static_assert(dimension_of(x.value).pow(3) == dimension_of(x.value));
    static_assert(dimension_of((x / x).value) == dim_one);
    // x⁻¹ = x, (x y)⁻¹ = -(x y).
    static_assert(magnitude_of(inverse(x).value).is_one());
    static_assert(magnitude_of(inverse(xy).value) == magnitude{grid_rational{-1}});
}

TEST(GeoalgTest, products_of_basis_blades_match_a_bitmask_reference) {
    static_assert(all_rows<g30>(std::make_integer_sequence<unsigned, 8>{}));
    static_assert(all_rows<g13>(std::make_integer_sequence<unsigned, 16>{}));
    static_assert(all_rows<g201>(std::make_integer_sequence<unsigned, 8>{}));
}

TEST(GeoalgTest, quantity_products_carry_the_sign) {
    const auto a = just<3> * x;
    const auto b = just<4> * y;
    EXPECT_EQ(to_string(a * b), "12 x y");
    EXPECT_EQ(to_string(b * a), "-12 x y");
    static_assert(decltype(a * b)::reference == decltype(b * a)::reference); // the sign is in the number
    EXPECT_TRUE(a * a == just<9>);
    EXPECT_TRUE((just<2> * xy) * (just<3> * xy) == just<-6>);
    // A product that vanishes is the zero multivector.
    static_assert(std::same_as<decltype((just<1> * p0) * (just<1> * p0)), multivector<>>);
    // Division by a blade: x / y = x y (y² = 1); x / γ₁ = -x γ₁ (γ₁² = -1).
    EXPECT_EQ(to_string((just<6> * x) / (just<2> * y)), "3 x y");
    EXPECT_EQ(to_string(just<1> / (just<2> * xy)), "-0.5 xy");
    EXPECT_EQ(to_string((just<1> * g0) / (just<1> * g1)), "-1 γ₀ γ₁");
    // Physical units commute with everything and print first.
    EXPECT_EQ(to_string((just<3> * si::metre * x) * (just<2> * si::second * y)), "6 m s x y");
}

TEST(GeoalgTest, named_blades_convert_with_their_sign) {
    const auto q = just<5> * zx;
    EXPECT_EQ(to_string(q.in(x * z)), "-5 x z");
    // A sum runs in the shorter spelling of one size.
    EXPECT_EQ(to_string(q + just<1> * x * z), "4 zx");
    EXPECT_TRUE(q == just<-5> * x * z);
    EXPECT_TRUE(just<1> * x * y == just<1> * xy);
}

TEST(GeoalgTest, multivectors_sum_per_blade_in_order) {
    const auto a = just<1> * y + just<2> * x + just<3>;
    const auto b = just<3> + just<2> * x + just<1> * y;
    static_assert(std::same_as<decltype(a), decltype(b)>);
    EXPECT_EQ(to_string(a), "3 + 2 x + 1 y");
    EXPECT_EQ(to_string(a - just<1> * xy), "3 + 2 x + 1 y - 1 xy");
    EXPECT_EQ(to_string(-a), "-3 - 2 x - 1 y");
    EXPECT_EQ(to_string(a + a), "6 + 4 x + 2 y");
    EXPECT_EQ(to_string(a - a), "0 + 0 x + 0 y");
    EXPECT_TRUE(a - a == multivector<>{});
    // The parts of one blade add in their common unit.
    const auto d = just<1> * km * x + just<500> * si::metre * x + just<2> * si::metre * y;
    EXPECT_EQ(to_string(d), "1500 m x + 2 m y");
    // The zero is the identity of +.
    static_assert(std::same_as<decltype(multivector<>{} + a), std::remove_cvref_t<decltype(a)>>);
    static_assert(std::same_as<decltype(multivector<>{} + just<1> * x), decltype(just<1> * x)>);
    EXPECT_EQ(to_string(multivector<>{}), "0");
    // Another dimension outside the blades does not add.
    static_assert(!can_add<decltype(just<1> * si::metre * x), decltype(just<1> * si::second * y)>);
    static_assert(!can_add<decltype(d), decltype(just<1> * si::second * z)>);
    static_assert(can_add<decltype(d), decltype(just<1> * si::metre * z)>);
}

TEST(GeoalgTest, the_algebra_holds) {
    const auto a = just<1> + just<2> * x - just<1> * y + just<3> * xy + just<1, 2> * z;
    const auto b = just<2> - just<1> * x + just<4> * zx + just<1> * x * y * z;
    const auto c = just<-1> * y + just<2> * z + just<1> * xy;
    EXPECT_TRUE((a * b) * c == a * (b * c));
    EXPECT_TRUE(a * (b + c) == a * b + a * c);
    EXPECT_TRUE(reverse(a * b) == reverse(b) * reverse(a));
    EXPECT_TRUE(involute(a * b) == involute(a) * involute(b));
    EXPECT_TRUE(conjugate(a) == reverse(involute(a)));
    EXPECT_TRUE(grade<0>(a) + grade<1>(a) + grade<2>(a) + grade<3>(a) == a);
    EXPECT_TRUE(scalar_product(a, b) == grade<0>(a * b));
    EXPECT_TRUE(wedge(wedge(a, b), c) == wedge(a, wedge(b, c)));

    const auto u = just<1> * x + just<2> * y + just<3> * z;
    const auto v = just<4> * x - just<5> * y + just<6> * z;
    const auto w = just<-7> * x + just<8> * y + just<9> * z;
    EXPECT_TRUE(u * v == left_contraction(u, v) + wedge(u, v));
    EXPECT_TRUE(wedge(u, v) == -wedge(v, u));
    EXPECT_TRUE(left_contraction(u, v) == right_contraction(u, v));
    EXPECT_TRUE(left_contraction(u, wedge(v, w)) == left_contraction(u, v) * w - left_contraction(u, w) * v);
    EXPECT_TRUE(squared_norm(u) == just<14>);
    // In spacetime, γ₀ is timelike and γ₁ spacelike.
    EXPECT_TRUE(squared_norm(just<3> * g0 + just<2> * g1) == just<5>);
    EXPECT_TRUE((just<3> * g0 + just<2> * g1) * (just<3> * g0 + just<2> * g1) == just<5>);
}

TEST(GeoalgTest, results_collapse) {
    const auto u = just<1> * x + just<2> * y;
    static_assert(std::same_as<decltype(grade<0>(u * u)), decltype(part(u * u, one))>);
    static_assert(insidable<decltype(grade<0>(u * u))>);
    static_assert(quantity_like<decltype(wedge(just<1> * x, just<1> * y))>);
    static_assert(std::same_as<decltype(wedge(just<1> * x, just<1> * x)), multivector<>>);
    static_assert(std::same_as<decltype(grade<3>(u)), multivector<>>);
    EXPECT_TRUE(part(u, y) == just<2>);
    EXPECT_TRUE(part(u, z) == multivector<>{});
    // The coefficient of a named blade has its spelling's sign.
    const auto t = just<3> * x * z + just<1>;
    EXPECT_TRUE(part(t, zx) == just<-3>);
    EXPECT_TRUE(part(just<2> * si::metre * x + just<1> * si::metre * y, x) == just<2> * si::metre);
}

TEST(GeoalgTest, inverse_when_provably_a_scalar_norm) {
    const auto z1 = just<3> + just<4> * xy;
    EXPECT_TRUE(z1 * inverse(z1) == just<1>);
    const auto q = just<1> + just<2> * xy + just<3> * zx + just<4> * y * z;
    EXPECT_TRUE(q * inverse(q) == just<1>);
    EXPECT_TRUE(inverse(q) * q == just<1>);
    const auto u = just<1> * x + just<2> * y + just<2> * z;
    EXPECT_TRUE(u * inverse(u) == just<1>);
    const auto dual_number = just<2> + just<3> * p0;
    EXPECT_TRUE(dual_number * inverse(dual_number) == just<1>);
    const auto split = just<2> + just<1> * x; // G(1,0): the conjugate
    EXPECT_TRUE(split * inverse(split) == just<1>);
    static_assert(can_invert<decltype(q)>);
    // An even element of G(1,3) with two commuting bivectors: x x~ has a
    // grade-4 part, so there is no inverse of that form.
    static_assert(!can_invert<decltype(just<1> + just<1> * g0 * g1 + just<1> * g2 * g3)>);
    static_assert(!can_invert<decltype(just<1> * p0)>);
    EXPECT_TRUE((just<3> + just<4> * xy) / (just<1> - just<2> * xy) == just<-1> + just<2> * xy);

    // A grid that holds 0: the inverse is an expected.
    using gaussian = multivector<inside<{-2, 2}>, quantity<xy, inside<{-2, 2}>>>;
    const gaussian zero{};
    const gaussian two{just<2> + just<0> * xy};
    const auto     bad  = inverse(zero);
    const auto     good = inverse(two);
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error(), errc::division_by_zero);
    ASSERT_TRUE(good.has_value());
    EXPECT_TRUE((*good == just<1, 2>));
}

TEST(GeoalgTest, declared_multivectors_convert) {
    using complex = multivector<inside<{-10, 10}>, quantity<xy, inside<{-10, 10}>>>;
    const complex a{just<3> + just<4> * xy};
    const complex b = just<2>; // the missing part is 0
    const complex c{just<1> * x * y};
    EXPECT_EQ(to_string(a), "3 + 4 xy");
    EXPECT_EQ(to_string(b), "2 + 0 xy");
    EXPECT_EQ(to_string(c), "0 + 1 xy");
    static_assert(!std::is_constructible_v<complex, decltype(just<1> * z)>);
    // Narrowing a part's grid is checked, as for an inside, or takes a policy.
    using small = multivector<inside<{-1, 1}>, quantity<xy, inside<{-1, 1}>>>;
    const small s{a, clamp};
    EXPECT_EQ(to_string(s), "1 + 1 xy");
    EXPECT_EQ(to_string((just<2> * km * x + just<3> * km * y).in(si::metre)), "2000 m x + 3000 m y");
}

TEST(GeoalgTest, duals_and_complements) {
    const auto u = just<1> * x + just<2> * y + just<3> * z;
    const auto v = just<4> * x + just<5> * y + just<6> * z;
    EXPECT_EQ(to_string(dual(wedge(u, v), x * y * z)), "-3 x + 6 y - 3 z"); // u × v
    EXPECT_EQ(to_string(complement(u, x * y * z)), "3 x y - 2 x z + 1 y z");
    const auto a = just<1> + just<2> * x - just<1> * y + just<3> * xy + just<5> * x * y * z;
    EXPECT_TRUE(uncomplement(complement(a, x * y * z), x * y * z) == a);
    // In the projective plane: the pseudoscalar is the identity of the join.
    const auto i = p0 * p1 * p2;
    const auto b = just<1> * p0 + just<2> * p1 * p2 - just<3> * p0 * p2 + just<4>;
    EXPECT_TRUE(regressive(just<1> * i, b, i) == b);
    // Two points of the plane (grade 2): their join is the line through them.
    const auto pa   = just<1> * p1 * p2 + just<1> * p0 * p2; // (1, 0) up to the sign convention
    const auto pb   = just<1> * p1 * p2 - just<2> * p0 * p1; // (0, 2)
    const auto line = regressive(pa, pb, i);
    static_assert(multivector_like<decltype(line)>);
    EXPECT_TRUE(regressive(line, pa, i) == multivector<>{});
    EXPECT_TRUE(regressive(line, pb, i) == multivector<>{});
}

TEST(GeoalgTest, text) {
    const auto z1 = just<3> - just<4> * xy;
    EXPECT_EQ(to_string(z1), "3 - 4 xy");
    EXPECT_EQ(to_string(just<1> * g0 * g1 * si::metre, text_encoding::ascii), "1 m g0 g1");
    EXPECT_EQ(std::format("[{:>10}]", z1), "[  3 - 4 xy]");
    std::ostringstream os;
    os << z1 << ' ' << multivector<>{};
    EXPECT_EQ(os.str(), "3 - 4 xy 0");
}
} // namespace geoalg_tests
