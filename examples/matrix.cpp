// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Exact linear algebra on std::mdspan (inside/mdspan.hpp).
//   1. dot of two static vectors: the result type holds every possible total,
//      worked out at compile time from the extent and the element grid.
//   2. A rotation by 30° in Q1.14: cos and sin correctly rounded from an
//      angle in degrees, applied to points by matrix_vector_product, each
//      coordinate the exact sum of products, rounded once. Compared with
//      double.
//   3. The rotation squared by matrix_product is the rotation by 60°, within
//      a notch of the directly rounded one.
//   4. Fibonacci numbers as powers of [[1, 1], [1, 0]]: 64-bit entries whose
//      products are held exactly in 128 bits; F(94) does not fit the entry
//      type and is reported, not wrapped.

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/mdspan.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

using q14     = inside<{{-2, 2}, per<16384>}, round_nearest>; // Q1.14
using coord   = inside<{{-100, 100}, per<256>}, round_nearest>;
using matrix  = std::extents<std::size_t, 2, 2>;
using vector2 = std::extents<std::size_t, 2>;
using degrees = quantity<si::degree, inside<{0, 360}>>;

// [[cos a, −sin a], [sin a, cos a]], each entry correctly rounded to Q1.14.
std::array<q14, 4> rotation(degrees a) {
    const q14 c = units::cos_into<q14>(a), s = units::sin_into<q14>(a);
    return {c, q14{-s}, s, c};
}

int main() {
    // 1. dot: 4 products of [−100, 100] lie in [−40000, 40000].
    using small = inside<{-100, 100}>;
    std::array<small, 4> a{3, -7, 100, 25}, b{12, 9, -4, 30};
    const auto           d = dot(std::mdspan{a.data(), std::extents<std::size_t, 4>{}},
                                 std::mdspan{b.data(), std::extents<std::size_t, 4>{}});
    static_assert(grid_of<decltype(d)> == grid{-40000, 40000});
    std::cout << "dot         " << d << "\n";
    if (d != 323)
        return 1;

    // 2. Rotate three points by 30°.
    const auto        r30 = rotation(degrees{30});
    const std::mdspan rot{r30.data(), matrix{}};
    double            worst = 0;
    for (const auto& p : {std::array<coord, 2>{10, 0}, std::array<coord, 2>{0, 10}, std::array<coord, 2>{3, 4}}) {
        std::array<coord, 2> q{};
        matrix_vector_product(rot, std::mdspan{p.data(), vector2{}}, std::mdspan{q.data(), vector2{}});
        const double x = p[0].to<double>(), y = p[1].to<double>();
        const double c = std::cos(std::numbers::pi / 6), s = std::sin(std::numbers::pi / 6);
        const double ex = c * x - s * y, ey = s * x + c * y;
        worst = std::max({worst, std::abs(q[0].to<double>() - ex), std::abs(q[1].to<double>() - ey)});
        std::cout << "(" << p[0] << ", " << p[1] << ") -> (" << q[0] << ", " << q[1] << ")  double (" << ex << ", "
                  << ey << ")\n";
    }
    std::cout << "worst error " << worst << " (a coordinate notch is " << 1.0 / 256 << ")\n";
    if (!(worst <= 1.0 / 256))
        return 1;

    // 3. Two 30° turns make one of 60°: every entry is the exact sum of two
    // Q1.14 products, rounded once into Q1.14.
    std::array<q14, 4> r60{};
    matrix_product(rot, rot, std::mdspan{r60.data(), matrix{}});
    const auto direct = rotation(degrees{60});
    std::cout << "R30·R30     [" << r60[0] << ", " << r60[1] << "; " << r60[2] << ", " << r60[3] << "]\n"
              << "R60         [" << direct[0] << ", " << direct[1] << "; " << direct[2] << ", " << direct[3] << "]\n";
    for (std::size_t k = 0; k < 4; ++k)
        if (r60[k] - direct[k] > just<1, 16384> || direct[k] - r60[k] > just<1, 16384>)
            return 1;

    // 4. Fibonacci: [[1, 1], [1, 0]]^n = [[F(n+1), F(n)], [F(n), F(n−1)]].
    using fib = inside<{0, 12'200'160'415'121'876'738u}>; // F(93), the largest below 2^64
    const std::array<fib, 4> step{1, 1, 1, 0};
    std::array<fib, 4>       power{1, 0, 0, 1};
    std::array<fib, 4>       next{};
    for (int n = 1; n <= 92; ++n) {
        matrix_product(std::mdspan{power.data(), matrix{}},
                       std::mdspan{step.data(), matrix{}},
                       std::mdspan{next.data(), matrix{}});
        power = next;
    }
    std::cout << "F(93)       " << power[0] << "\n";
    if (to_string(power[0]) != "12200160415121876738")
        return 1;
    try {
        matrix_product(std::mdspan{power.data(), matrix{}},
                       std::mdspan{step.data(), matrix{}},
                       std::mdspan{next.data(), matrix{}});
        return 1; // F(94) must not fit
    } catch (const inside_error& e) {
        std::cout << "F(94)       " << e.what() << "\n";
    }
    return 0;
}
