// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Decibel ↔ linear conversion using `beman::inside::math::pow_base_into<…, 10>`
// and `log10_into`.
//
// Demonstrates:
//   - `db_to_linear`: 10^(dB/20), rounded once onto the gain grid. This is the
//     building block audio_mixer uses for per-channel gain.
//   - `linear_to_db`: 20 · log10(amp), with log10 rounded once onto a grid 20×
//     finer than the dB grid, so the exact × 20 lands on it.
//   - A `cursor<db_t, 3>` sweep showing dB → linear → dB recovers the input
//     at every step.

#include <version>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/cmath.hpp>

// std::println needs io.hpp's std::formatter, which needs <format> (libc++ 18
// has <print> without __cpp_lib_format).
#if defined(__cpp_lib_print) && defined(__cpp_lib_format)
    #include <print>
using std::println;
#else
    // Kept portable to standard libraries without <print>: a minimal stand-in
    // covering the `{}` forms this example uses, rendered through the
    // inside/rational operator<< from io.hpp.
    #include <iostream>
    #include <sstream>
    #include <string_view>
namespace {
template <class... Ts>
void println(std::string_view fmt, const Ts&... args) {
    std::ostringstream    oss;
    std::size_t           pos  = 0;
    [[maybe_unused]] auto emit = [&](const auto& a) {
        auto open = fmt.find("{}", pos);
        if (open == std::string_view::npos)
            return;
        oss << fmt.substr(pos, open - pos) << a;
        pos = open + 2;
    };
    (emit(args), ...);
    oss << fmt.substr(pos) << '\n';
    std::cout << oss.str();
}
} // namespace
#endif

using namespace beman::inside;

// dB in [-24, 12] at 0.5 dB resolution — matches a typical mixer fader range.
using db_t = inside<{{-24, 12}, per<2>}, round_nearest>;
// Linear amplitude. dB ∈ [-24, 12] ⇒ amp ∈ [10^-1.2, 10^0.6] ≈ [0.063, 3.98].
using gain_t = inside<{{0x1p-8, 4}, per<65536>}, round_nearest>;
// dB/20 on a 1/40 grid: × 20 maps it exactly onto db_t's 0.5 dB notch. The
// range covers log10 of every gain_t, [log10(2^-8), log10(4)].
using db_div20_t = inside<{{-2.5, 1}, per<40>}, round_nearest>;

// dB → linear: 10^(dB/20). dB/20 is exact; pow_base_into rounds once.
static constexpr gain_t db_to_linear(db_t db) { return math::pow_base_into<gain_t, 10>(db / just<20>); }

// linear → dB: 20·log10(amp). log10 rounds once onto 1/40 steps, which is the
// same as rounding 20·log10 onto 0.5 dB; the × 20 is exact.
static constexpr db_t linear_to_db(gain_t amp) { return db_t{just<20> * math::log10_into<db_div20_t>(amp)}; }

int main() {
    // `std::println` works on inside / rational because `beman/inside/io.hpp`
    // ships `std::formatter` specializations for both. Empty `{}` keeps the
    // exact rational rendering — same string `operator<<` would produce.
    println("dB → linear → dB round-trip:");
    println("    dB       linear         round-trip dB");

    // Sweep dB at 3 dB steps over the full range.
    for (cursor<db_t, 3> db; db != end(db); ++db) {
        gain_t lin          = db_to_linear(*db);
        db_t   db_recovered = linear_to_db(lin);
        println("    {}       {}         {}", *db, lin, db_recovered);
    }

    // The canonical "6 dB doubles" / "12 dB quadruples" landmarks.
    println("\nLandmark conversions (within db_t's [-24, 12] range):");
    for (int d : {-24, -12, -6, 0, 6, 12}) {
        db_t   db{d};
        gain_t lin = db_to_linear(db);
        println("    {} dB  =  {}", db, lin);
    }

    return 0;
}
