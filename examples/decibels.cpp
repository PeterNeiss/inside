// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Decibel ↔ linear conversion using `units::pow_base_into<…, 10>` and
// `log10_into`. A level is a quantity of a unit of our own, dB, on a
// dimensionless kind: levels add (−6 dB + 3 dB), but a level is no plain
// number. L / 20 dB is one, and that is what 10^x takes.
//
// Demonstrates:
//   - `db_to_linear`: 10^(dB/20), rounded once onto the gain grid. This is the
//     building block audio_mixer uses for per-channel gain.
//   - `linear_to_db`: 20 · log10(amp), with log10 rounded once onto a grid 20×
//     finer than the dB grid, so the exact × 20 lands on it.
//   - A sweep in 3 dB steps (a cursor over the level's number grid) showing
//     dB → linear → dB recovers the input at every step.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

#include <print>
using std::println;

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// A level: dimensionless, but a kind of its own, so it never turns into a
// plain number by itself.
inline constexpr quantity_spec level_def{"level", isq::dimensionless, is_kind};
inline constexpr named_unit    decibel_def{"dB", one, spec_c<level_def>};
inline constexpr auto          dB = unit_c<decibel_def>;
} // namespace

// dB in [-24, 12] at 0.5 dB resolution — matches a typical mixer fader range.
using db_t = quantity<dB, inside<{{-24, 12}, per<2>}, round_nearest>>;
// Linear amplitude. dB ∈ [-24, 12] ⇒ amp ∈ [10^-1.2, 10^0.6] ≈ [0.063, 3.98].
using gain_t = inside<{{0x1p-8, 4}, per<65536>}, round_nearest>;
// dB/20 on a 1/40 grid: × 20 maps it exactly onto db_t's 0.5 dB notch. The
// range covers log10 of every gain_t, [log10(2^-8), log10(4)].
using db_div20_t = inside<{{-2.5, 1}, per<40>}, round_nearest>;

// dB → linear: 10^(L / 20 dB). The ratio is exact; pow_base_into rounds once.
static constexpr gain_t db_to_linear(db_t db) { return units::pow_base_into<gain_t, 10>(db / (just<20> * dB)); }

// linear → dB: 20 dB · log10(amp). log10 rounds once onto 1/40 steps, which is
// the same as rounding 20·log10 onto 0.5 dB; the × 20 dB is exact.
static constexpr db_t linear_to_db(gain_t amp) { return db_t{units::log10_into<db_div20_t>(amp) * (just<20> * dB)}; }

int main() {
    // `std::println` works on inside / rational because `beman/inside/inside/io.hpp`
    // ships `std::formatter` specializations for both. Empty `{}` keeps the
    // exact rational rendering — same string `operator<<` would produce.
    println("dB → linear → dB round-trip:");
    println("    dB       linear         round-trip dB");

    // Sweep dB at 3 dB steps over the full range.
    for (cursor<db_t::rep, 3> n; n != end(n); ++n) {
        const db_t   db{*n};
        const gain_t lin          = db_to_linear(db);
        const db_t   db_recovered = linear_to_db(lin);
        println("    {}       {}         {}", db, lin, db_recovered);
        if (db_recovered != db)
            return 1;
    }

    // The canonical "6 dB doubles" / "12 dB quadruples" landmarks.
    println("\nLandmark conversions (within db_t's [-24, 12] range):");
    for (int d : {-24, -12, -6, 0, 6, 12}) {
        db_t   db{d};
        gain_t lin = db_to_linear(db);
        println("    {}  =  {}", db, lin);
    }
    // Levels add; −6 dB is close to half the amplitude.
    if (db_t{-6} + just<3> * dB != db_t{-3} || db_to_linear(db_t{-6}) != gain_t{rational{32846, 65536}})
        return 1;

    return 0;
}
