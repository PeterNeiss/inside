// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Cartesian-to-polar conversion: (x, y) → (magnitude, angle in radians).
//
// Demonstrates:
//   - `beman::inside::math::atan2` for the angle. Output is in radians ∈ [-π, π],
//     consistent with sin/cos/tan — feed it straight back into sin/cos.
//   - `beman::inside::math::sqrt` for the magnitude, fed `x² + y²` computed in rational.
//   - Round-trip back via `sin`/`cos`: starting from (x, y), recovering
//     (mag·cos(θ), mag·sin(θ)) should land within a few Q-format ULPs of the
//     original — a sanity check that the trig and sqrt are wired correctly.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

int main() {
    // Cartesian coordinate inside: [-1, 1] with Q.14 resolution.
    using coord_t = inside<{{-1, 1}, per<16384>}, round_nearest | f64>;
    // Magnitude is non-negative; max possible |v| from (±1, ±1) is √2 ≈ 1.4142.
    // We accept inputs in [-1, 1], so mag_sq ∈ [0, 2], mag ∈ [0, √2].
    using magsq_t = inside<{{0, 2}, per<16384>}, round_nearest | f64>;

    struct point {
        coord_t     x;
        coord_t     y;
        const char* label;
    };
    const point points[] = {
        {1, 0, "+x axis      "},
        {0, 1, "+y axis      "},
        {-1, 0, "-x axis      "},
        {0, -1, "-y axis      "},
        {1, 1, "+45° (mag √2)"},
        {-1, 1, "+135°        "},
        {frac<3, 5>, frac<4, 5>, "(3/5, 4/5) — 3-4-5 triangle, mag 1"},
        {0.5, 0.5, "(½, ½)       "},
    };

    std::cout << "Cartesian → polar:\n";
    std::cout << "  (x, y)                              magnitude        angle (rad)\n";
    for (auto& p : points) {
        coord_t x{p.x}, y{p.y};

        // mag² = x² + y² in rational (no inside math; the cross-multiplies stay
        // small here, ≤ 2 in magnitude). The checked rational ops return
        // `std::expected<rational, errc>`; arithmetic forwards through the expected
        // and the inside ctor unwraps once at the sink.
        auto    x2 = p.x * p.x;
        auto    y2 = p.y * p.y;
        magsq_t mag_sq{x2 + y2};

        auto magnitude = math::sqrt(mag_sq);
        auto angle     = math::atan2(y, x);

        std::cout << "  " << p.label << "    mag = " << magnitude << "    θ = " << angle << "\n";
    }

    std::cout << "\nRound-trip (polar → cartesian via sin/cos):\n";
    std::cout << "  original           recovered (mag·cos θ, mag·sin θ)\n";
    // atan2 already returns radians ∈ [-π, π]; feed it straight into sin/cos.
    // ±4 rad comfortably brackets ±π.
    using angle_t = inside<{{-4, 4}, per<16384>}, round_nearest | f64>;
    for (auto& p : points) {
        coord_t x{p.x}, y{p.y};
        auto    x2 = p.x * p.x;
        auto    y2 = p.y * p.y;
        magsq_t mag_sq{x2 + y2};
        auto    magnitude = math::sqrt(mag_sq);
        auto    angle     = math::atan2(y, x);

        angle_t a{angle};

        coord_t rx{magnitude * math::cos(a)};
        coord_t ry{magnitude * math::sin(a)};

        std::cout << "  (" << x << ", " << y << ") → (" << rx << ", " << ry << ")\n";
    }

    return 0;
}
