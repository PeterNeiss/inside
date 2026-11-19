// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Exact linear algebra on std::mdspan: result types that hold every possible total, rotations in Q1.14 with each
// coordinate an exact sum rounded once, and Fibonacci matrices whose overflow is reported, not wrapped.

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/mdspan.hpp>
#include <inside/units.hpp>
#include <inside/units/math.hpp>

using namespace ins;

using q1_14 = inside<{{-2, 2}, per<(1 << 14)>}, round_half_away>;
using degrees = quantity<si::degree, inside<{0, 360}>>;
using rotation_2x2 = std::array<q1_14, 4>;

// [[cos a, −sin a], [sin a, cos a]], each entry correctly rounded to Q1.14.
rotation_2x2 rotation(degrees a) {
    const q1_14 c = math::cos<q1_14>(a), s = math::sin<q1_14>(a);
    return {c, q1_14{-s}, s, c};
}

int main() {
    // 1. dot of two static vectors: 4 products of [−100, 100] lie in [−40000, 40000], worked out at compile time.
    using element = inside<{-100, 100}>;
    std::array<element, 4> a{3, -7, 100, 25}, b{12, 9, -4, 30};
    const auto d = dot(std::mdspan{a.data(), std::extents<std::size_t, 4>{}},
                       std::mdspan{b.data(), std::extents<std::size_t, 4>{}});
    static_assert(grid_of<decltype(d)> == grid{-40000, 40000});
    std::cout << "dot         " << d << "\n";
    if (d != 323)
        return 1;

    // 2. Three points rotated by 30° with matrix_vector_product, compared with double.
    using coord = inside<{{-100, 100}, per<(1 << 8)>}, round_half_away>;
    using point = std::array<coord, 2>;
    using matrix_2x2 = std::extents<std::size_t, 2, 2>;
    using vector_2 = std::extents<std::size_t, 2>;

    const auto r30 = rotation(degrees{30, si::degree});
    const std::mdspan rot{r30.data(), matrix_2x2{}};
    double worst = 0;
    for (const point& p : {point{10, 0}, point{0, 10}, point{3, 4}}) {
        point q{};
        if (!matrix_vector_product(rot, std::mdspan{p.data(), vector_2{}}, std::mdspan{q.data(), vector_2{}}))
            return 1; // a coordinate past its range
        const double x = p[0].to<double>(), y = p[1].to<double>();
        const double c = std::cos(std::numbers::pi / 6), s = std::sin(std::numbers::pi / 6);
        const double ex = c * x - s * y, ey = s * x + c * y;
        worst = std::max({worst, std::abs(q[0].to<double>() - ex), std::abs(q[1].to<double>() - ey)});
        std::cout << "(" << p[0] << ", " << p[1] << ") -> (" << q[0] << ", " << q[1] << ")  double (" << ex
                  << ", " << ey << ")\n";
    }
    std::cout << "worst error " << worst << " (a coordinate notch is " << 1.0 / (1 << 8) << ")\n";
    if (!(worst <= 1.0 / (1 << 8)))
        return 1;

    // 3. Two 30° turns make one of 60°, within a notch: each entry is an exact sum of two products, rounded once.
    rotation_2x2 r60{};
    if (!matrix_product(rot, rot, std::mdspan{r60.data(), matrix_2x2{}}))
        return 1; // an entry past Q1.14
    const auto direct = rotation(degrees{60, si::degree});
    std::cout << "R30·R30     [" << r60[0] << ", " << r60[1] << "; " << r60[2] << ", " << r60[3] << "]\n"
              << "R60         [" << direct[0] << ", " << direct[1] << "; " << direct[2] << ", " << direct[3] << "]\n";
    for (std::size_t k = 0; k < 4; ++k)
        if (r60[k] - direct[k] > just<1, (1 << 14)> || direct[k] - r60[k] > just<1, (1 << 14)>)
            return 1;

    // 4. Fibonacci: [[1, 1], [1, 0]]^n = [[F(n+1), F(n)], [F(n), F(n−1)]], 64-bit entries whose products are held
    //    exactly in 128 bits. No product of two such matrices is proven to fit, so matrix_product returns an
    //    expected<void, error>: the first entry that does not fit.
    using fibonacci = inside<{0, 12'200'160'415'121'876'738u}>; // F(93), the largest below 2^64
    const std::array<fibonacci, 4> step{1, 1, 1, 0};
    std::array<fibonacci, 4> power{1, 0, 0, 1};
    std::array<fibonacci, 4> next{};
    for (int n = 1; n <= 92; ++n) {
        if (!matrix_product(std::mdspan{power.data(), matrix_2x2{}}, std::mdspan{step.data(), matrix_2x2{}},
                            std::mdspan{next.data(), matrix_2x2{}}))
            return 1;
        power = next;
    }
    std::cout << "F(93)       " << power[0] << "\n";
    if (to_string(power[0]) != "12200160415121876738")
        return 1;
    const auto f94 = matrix_product(std::mdspan{power.data(), matrix_2x2{}}, std::mdspan{step.data(), matrix_2x2{}},
                                    std::mdspan{next.data(), matrix_2x2{}});
    if (f94)
        return 1; // F(94) must not fit
    std::cout << "F(94)       " << error_message(f94.error()) << "\n";
    return 0;
}
