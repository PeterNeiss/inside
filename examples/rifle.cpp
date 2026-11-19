// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A rifle, its magazine and a reserve, built on policies. Ammunition is a base dimension in rounds (rd), while
// trigger pulls, reloads and dry clicks stay plain event counts, so the two never mix.
// The magazine wraps (firing past empty is the reload event, on_wrap), and the reserve clamps (a withdrawal past
// empty saturates, and on_clamp's overshoot says how short it was).

#include <iostream>
#include <string_view>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/formats.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;
using namespace ins::formats;

namespace {
inline constexpr auto rd = base_unit<"rd", "ammunition">;
} // namespace

class magazine {
  public:
    static constexpr auto capacity = just<30> * rd;
    using rounds_t = quantity<rd, inside<{0, 30}, wrap>>; // 31 states; wraps at empty

    rounds_t rounds{capacity}; // starts full

    bool empty() const { return rounds == just<0> * rd; }
};

class rifle {
  public:
    using dropped_rounds = quantity<rd, inside<{0, 1'000'000}, clamp>>;

    magazine mag;
    clamp_counter<1'000'000> pulled_trigger{0}; // saturating event tallies
    clamp_counter<1'000'000> missed_shots{0};
    clamp_counter<1'000'000> reloads{0};
    dropped_rounds rounds_dropped{0, rd}; // accumulates the magazine's rounds
};

class player {
  public:
    using reserve_t = quantity<rd, inside<{0, 200}, clamp>>;

    reserve_t reserve{60, rd}; // two spare magazines' worth
    rifle weapon;
    clamp_counter<1'000'000> dry_clicks{0}; // the magazine and the reserve both empty

    // Pull the trigger once:
    //   magazine and reserve empty  → a dry click;
    //   magazine empty, reserve not → a missed shot, whose wrap reloads from the reserve (partly, if it is short);
    //   magazine not empty          → a hit, one round less.
    void pull_trigger();

    // A combat reload drops the partial magazine (those rounds are gone) and pulls a fresh one from the reserve.
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
        // The wrap put m at capacity: withdraw a full magazine; on_clamp's overshoot is how short the reserve was.
        magazine::rounds_t shortfall{0, rd};
        reserve.on_clamp([&](auto&, auto over) { shortfall = magazine::rounds_t{-over}; }) -= magazine::capacity;
        ++weapon.reloads;
        if (shortfall > just<0> * rd)
            m = magazine::capacity - shortfall;
    }) -= just<1> * rd;
}

void player::combat_reload() {
    weapon.rounds_dropped += weapon.mag.rounds; // saturates, so this stays a plain += of two round counts

    magazine::rounds_t shortfall{0, rd};
    reserve.on_clamp([&](auto&, auto over) { shortfall = magazine::rounds_t{-over}; }) -= magazine::capacity;
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

    // 1. Burn 2/3 of the magazine, then a combat reload (10 rounds dropped).
    for (int i = 0; i < 20; ++i)
        p.pull_trigger();
    status("after 20 shots:                     ");
    p.combat_reload();
    status("after combat reload (10 dropped):   ");

    // 2. Sustained fire across empty: the 31st shot pulls a fresh magazine from the reserve, via on_wrap.
    for (int i = 0; i < 31; ++i)
        p.pull_trigger();
    status("after 31 more (1 wrap-reload):      ");

    // 3. Fire past the reserve into dry clicks.
    for (int i = 0; i < 40; ++i)
        p.pull_trigger();
    status("after 40 more (reserve dries up):   ");

    if (to_string(p.weapon.rounds_dropped) != "10 rd" || p.weapon.reloads != 2 || p.dry_clicks != 10
        || !p.weapon.mag.empty())
        return 1;
    return 0;
}
