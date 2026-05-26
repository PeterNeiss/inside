// Rifle / magazine / reserve simulation built on the bound policy system.
//
// Three classes, each leaning on a different policy:
//
//   - `magazine` holds the round count as `bound<{0, capacity}, wrap>` —
//     31 distinct states, and the wrap from 0 back up to capacity is
//     exactly the magazine-swap event.
//   - `rifle` owns a magazine and the per-weapon trigger-event counters
//     (`pulled_trigger`, `missed_shots`, `reloads`, `rounds_dropped`).
//   - `player` owns a rifle plus a `bound<{0, 200}, clamp>` reserve pool;
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
//   - End-to-end without any explicit casts — every bound ↔ integer
//     interaction flows through the implicit operators or the policy
//     callbacks.

#include <iostream>
#include <string_view>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

class magazine
{
public:
  static constexpr int capacity = 30;
  using count_t = bound<{0, capacity}, wrap>;     // 31 states; wraps at empty

  count_t rounds{capacity};                        // start full

  bool empty() const { return rounds == 0; }       // arithmetic compare, no cast
};

class rifle
{
public:
  magazine mag;
  int  pulled_trigger = 0;
  int  missed_shots   = 0;
  int  reloads        = 0;
  imax rounds_dropped = 0;                          // accumulates count_t values
};

class player
{
public:
  using reserve_t = bound<{0, 200}, clamp>;
  reserve_t reserve{60};                            // two spare mags' worth
  rifle weapon;
  int dry_clicks = 0;                               // mag empty AND reserve empty

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

void player::pull_trigger()
{
  ++weapon.pulled_trigger;

  if (weapon.mag.empty() && reserve == 0) { ++dry_clicks; return; }
  if (weapon.mag.empty()) ++weapon.missed_shots;

  weapon.mag.rounds.on_wrap([&](auto& m, auto) {
    // The wrap put `m` at capacity. Withdraw a full mag from the reserve;
    // on_clamp's overshoot tells us how much we were short of capacity.
    imax shortfall = 0;
    reserve.on_clamp([&](auto&, auto over) { shortfall = -over; })
           -= magazine::capacity;
    ++weapon.reloads;
    if (shortfall > 0)
      m = magazine::count_t{magazine::capacity - shortfall};
  }) -= 1;
}

void player::combat_reload()
{
  // Direct-init `imax dropped = bound` picks the bound's implicit
  // `operator imax()` cleanly; the subsequent `imax += imax` would be
  // ambiguous if we tried `imax += bound` directly (the bound has
  // implicit conversions to both `imax` and `size_t`).
  imax dropped = weapon.mag.rounds;
  weapon.rounds_dropped += dropped;

  imax shortfall = 0;
  reserve.on_clamp([&](auto&, auto over) { shortfall = -over; })
         -= magazine::capacity;
  ++weapon.reloads;
  weapon.mag.rounds = magazine::count_t{magazine::capacity - shortfall};
}

int main()
{
  player p;

  auto status = [&](std::string_view tag) {
    std::cout << tag
              << "  mag=" << p.weapon.mag.rounds
              << "  reserve=" << p.reserve
              << "  triggers=" << p.weapon.pulled_trigger
              << "  missed=" << p.weapon.missed_shots
              << "  dropped=" << p.weapon.rounds_dropped
              << "  reloads=" << p.weapon.reloads
              << "  dry=" << p.dry_clicks
              << "\n";
  };

  status("start:                              ");

  // Phase 1 — burn 2/3 of the mag, then combat-reload (10 dropped).
  for (int i = 0; i < 20; ++i) p.pull_trigger();
  status("after 20 shots:                     ");
  p.combat_reload();
  status("after combat reload (10 dropped):   ");

  // Phase 2 — sustained fire across the empty threshold. The 31st shot
  // pulls a fresh mag from the reserve via on_wrap.
  for (int i = 0; i < 31; ++i) p.pull_trigger();
  status("after 31 more (1 wrap-reload):      ");

  // Phase 3 — fire past the reserve into dry-click territory.
  for (int i = 0; i < 40; ++i) p.pull_trigger();
  status("after 40 more (reserve dries up):   ");

  return 0;
}
