// Decibel ↔ linear conversion using `bnd::math::pow_base<10>` and `log`.
//
// Demonstrates:
//   - `db_to_linear`: 10^(dB/20) via `pow_base<10>`. This is the building
//     block audio_mixer uses for per-channel gain.
//   - `linear_to_db`: 20 · log(amp) / log(10) via `log`, with the 20/log(10)
//     factor folded into a compile-time rational constant (~8.6859).
//   - A round-trip sweep showing dB → linear → dB recovers the input within
//     a few Q-format ULPs at every step.

#include <version>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/formatter.hpp"
#include "bound/print.hpp"      // operator<< — the fallback println routes here

#if defined(__cpp_lib_print)
#include <print>
using std::println;
#else
// GCC 12 / C++20 lack <print>. Minimal stand-in covering the `{}` forms this
// example uses, rendered through the bound/rational operator<< from print.hpp.
#include <iostream>
#include <sstream>
#include <string_view>
namespace {
template <class... Ts>
void println(std::string_view fmt, Ts const&... args)
{
  std::ostringstream oss;
  std::size_t pos = 0;
  [[maybe_unused]] auto emit = [&](auto const& a) {
    auto open = fmt.find("{}", pos);
    if (open == std::string_view::npos) return;
    oss << fmt.substr(pos, open - pos) << a;
    pos = open + 2;
  };
  (emit(args), ...);
  oss << fmt.substr(pos) << '\n';
  std::cout << oss.str();
}
} // namespace
#endif

using namespace bnd;

// dB in [-24, 12] at 0.5 dB resolution — matches a typical mixer fader range.
using db_t       = bound<{{-24, 12}, notch<1, 2>}, round_nearest>;
// dB/20 intermediate. Range chosen to cover [-1.2, 0.6] (the true range for
// dB ∈ [-24, 12]) but rounded out to integer endpoints so the grid validates
// against the 1/65536 notch. The fine notch is deliberate — auto-deduced
// pow_base<10> output inherits Notch<In>, and a coarser exponent grid would
// snap the linear output to audible 0.025-wide steps.
using db_div20_t = bound<{{-2, 1}, notch<1, 65536>}, round_nearest | real>;
// Linear amplitude. dB ∈ [-24, 12] ⇒ amp ∈ [10^-1.2, 10^0.6] ≈ [0.063, 3.98].
using gain_t     = bound<{{0x1p-8, 4}, notch<1, 65536>}, round_nearest | real>;

// dB → linear: 10^(dB/20).
static constexpr gain_t db_to_linear(db_t db)
{
  db_div20_t exponent{db / just<20>};
  return gain_t{math::pow_base<10>(exponent)};
}

// linear → dB: 20·log10(amp) = (20/ln(10)) · ln(amp).
// 20/ln(10) ≈ 8.685889638. As an 8-digit rational source: 86858896/10^7.
static constexpr db_t linear_to_db(gain_t amp)
{
  // 20/ln(10) as an exact point-bound (no rational on the surface).
  constexpr auto k20_over_ln10 = just<frac<86858896, 10000000>>;
  auto log_amp = math::log(amp);
  return db_t{k20_over_ln10 * log_amp};
}

int main()
{
  // `std::println` works on bound / rational because `bound/formatter.hpp`
  // ships `std::formatter` specializations for both. Empty `{}` keeps the
  // exact rational rendering — same string `operator<<` would produce.
  println("dB → linear → dB round-trip:");
  println("    dB       linear         round-trip dB");

  // Sweep dB at 3 dB steps over the full range.
  for (int d = -24; d <= 12; d += 3) {
    db_t db{d};
    gain_t lin = db_to_linear(db);
    db_t db_recovered = linear_to_db(lin);
    println("    {}       {}         {}", db, lin, db_recovered);
  }

  // The canonical "6 dB doubles" / "12 dB quadruples" landmarks.
  println("\nLandmark conversions (within db_t's [-24, 12] range):");
  for (int d : {-24, -12, -6, 0, 6, 12}) {
    db_t db{d};
    gain_t lin = db_to_linear(db);
    println("    {} dB  =  {}", db, lin);
  }

  return 0;
}
