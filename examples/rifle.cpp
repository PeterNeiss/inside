// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Rifle / magazine / reserve simulation built on the inside policy system.
// Ammunition is a base dimension with the unit `rd` (rounds): magazine,
// reserve and dropped rounds are quantities of it, while trigger pulls,
// reloads and dry clicks stay plain event counts — the two never mix.
//
// Three classes, each leaning on a different policy:
//
//   - `magazine` holds the round count as `inside<{0, capacity}, wrap>` —
//     31 distinct states, and the wrap from 0 back up to capacity is
//     exactly the magazine-swap event.
//   - `rifle` owns a magazine and the per-weapon trigger-event counters
//     (`pulled_trigger`, `missed_shots`, `reloads`, `rounds_dropped`).
//   - `player` owns a rifle plus an `inside<{0, 200}, clamp>` reserve pool;
//     withdrawals saturate at 0 so we never underflow, and the on_clamp
//     overshoot tells us how short the reserve was without an explicit
//     `std::min` or extraction cast.
//
// Demonstrates:
//   - `wrap` policy on the magazine — firing past empty triggers on_wrap
//   - `clamp` policy on the reserve — over-withdrawal surfaces via on_clamp
//   - Partial reloads when the reserve is short of a full mag
//   - Per-shot counters: pulled_trigger, missed_shots, dry_clicks
//   - Combat-reload that drops the partial mag (rounds_dropped)

#include <iostream>
#include <string_view>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension ammunition_dim{"ammunition"};
inline constexpr quantity_spec  ammunition_def{"ammunition", ammunition_dim};
inline constexpr named_unit     round_def{"rd", spec_c<ammunition_def>};
inline constexpr auto           rd = unit_c<round_def>;
} // namespace

class magazine {
  public:
    static constexpr auto capacity = just<30> * rd;
    using count_t                  = quantity<rd, inside<{0, 30}, wrap>>; // 31 states; wraps at empty

    count_t rounds{capacity}; // start full

    bool empty() const { return rounds == just<0> * rd; }
};

class rifle {
  public:
    magazine                                    mag;
    clamp_counter<1'000'000>                    pulled_trigger{0}; // saturating event tallies
    clamp_counter<1'000'000>                    missed_shots{0};
    clamp_counter<1'000'000>                    reloads{0};
    quantity<rd, inside<{0, 1'000'000}, clamp>> rounds_dropped{0}; // accumulates count_t values
};

class player {
  public:
    using reserve_t = quantity<rd, inside<{0, 200}, clamp>>;
    reserve_t                reserve{60}; // two spare mags' worth
    rifle                    weapon;
    clamp_counter<1'000'000> dry_clicks{0}; // mag empty AND reserve empty

    // Pull the trigger once. Four outcomes:
    //   (a) mag empty AND reserve empty  -> dry click, no decrement.
    //   (b) mag empty, reserve has rounds -> missed shot; the same trigger
    //       pull drives the wrap, which auto-reloads from the reserve.
    //   (c) mag has rounds                -> normal hit; mag decrements.
    //   (d) reserve was short of a full mag -> the new mag is partial.
    void pull_trigger();

    // Manual combat reload — drops the partial mag on the ground (those
    // rounds are gone) and pulls a fresh mag from the reserve. If the
    // reserve is short, the new mag is partial.
    void combat_reload();
};

void player::pull_trigger() {
    ++weapon.pulled_trigger;

    if (weapon.mag.empty() && reserve == just<0> * rd) {
        ++dry_clicks;
        return;
    }
    if (weapon.mag.empty())
        ++weapon.missed_shots;

    weapon.mag.rounds.on_wrap([&](auto& m, auto) {
        // The wrap put `m` at capacity. Withdraw a full mag from the reserve;
        // on_clamp's overshoot tells us how much we were short of capacity.
        magazine::count_t shortfall{0};
        reserve.on_clamp([&](auto&, auto over) { shortfall = magazine::count_t{-over}; }) -= magazine::capacity;
        ++weapon.reloads;
        if (shortfall > just<0> * rd)
            m = magazine::capacity - shortfall;
    }) -= just<1> * rd;
}

void player::combat_reload() {
    // The partial mag is dropped; accumulate its rounds. `rounds_dropped`
    // saturates, so this stays a plain += of two round counts.
    weapon.rounds_dropped += weapon.mag.rounds;

    magazine::count_t shortfall{0};
    reserve.on_clamp([&](auto&, auto over) { shortfall = magazine::count_t{-over}; }) -= magazine::capacity;
    ++weapon.reloads;
    weapon.mag.rounds = magazine::capacity - shortfall;
}

int main() {
    player p;

    auto status = [&](std::string_view tag) {
        std::cout << tag << "  mag=" << p.weapon.mag.rounds << "  reserve=" << p.reserve
                  << "  triggers=" << p.weapon.pulled_trigger << "  missed=" << p.weapon.missed_shots
                  << "  dropped=" << p.weapon.rounds_dropped << "  reloads=" << p.weapon.reloads
                  << "  dry=" << p.dry_clicks << "\n";
    };

    status("start:                              ");

    // Phase 1 — burn 2/3 of the mag, then combat-reload (10 dropped).
    for (int i = 0; i < 20; ++i)
        p.pull_trigger();
    status("after 20 shots:                     ");
    p.combat_reload();
    status("after combat reload (10 dropped):   ");

    // Phase 2 — sustained fire across the empty threshold. The 31st shot
    // pulls a fresh mag from the reserve via on_wrap.
    for (int i = 0; i < 31; ++i)
        p.pull_trigger();
    status("after 31 more (1 wrap-reload):      ");

    // Phase 3 — fire past the reserve into dry-click territory.
    for (int i = 0; i < 40; ++i)
        p.pull_trigger();
    status("after 40 more (reserve dries up):   ");

    if (to_string(p.weapon.rounds_dropped) != "10 rd" || p.weapon.reloads != 2 || p.dry_clicks != 10 ||
        !p.weapon.mag.empty())
        return 1;
    return 0;
}
