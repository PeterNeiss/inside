// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The first things to reach for: what happens at the edge of a range is the
// type's policy, written once.
//   1. `clamp` — saturate: a brightness, an RGB pixel.
//   2. `wrap` — modular: a compass heading.
//   3. `inside_range` — loops whose index is an inside, so `arr[i]` is in range.
// The default policy, `checked`, reports instead; errors.cpp shows how.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

// A pixel whose channels saturate. A runtime delta has a range too — name it,
// so `R += amount` stays inside += inside.
using channel       = inside<{0, 255}, clamp>;
using channel_delta = inside<{-512, 512}>;

struct rgb {
    channel R, G, B;
    void    brighten(channel_delta d) {
        R += d;
        G += d;
        B += d;
    }
    friend std::ostream& operator<<(std::ostream& os, const rgb& c) {
        return os << "rgb(" << c.R << ", " << c.G << ", " << c.B << ")";
    }
};

int main() {
    // 1. clamp.
    using pct      = inside<{0, 100}, clamp>;
    pct brightness = 120;   // 100
    brightness += -200_ins; // 0
    std::cout << "brightness " << brightness << "\n";
    inside<{0, 100}> strict{50}; // checked by default …
    strict.with_clamp() = 200;   // … clamped for this one store
    std::cout << "with_clamp " << strict << "\n";
    if (brightness != 0 || strict != 100)
        return 1;

    rgb pixel{200, 100, 50};
    pixel.brighten(80);
    std::cout << "brighten   " << pixel << "\n"; // rgb(255, 180, 130)
    pixel.brighten(-300);
    std::cout << "darken     " << pixel << "\n"; // rgb(0, 0, 0)
    if (pixel.R != 0 || pixel.G != 0)
        return 1;

    // 2. wrap.
    using degree   = inside<{0, 359}, wrap>;
    degree heading = 350;
    heading += 30_ins;  // 20
    heading += -90_ins; // 290
    heading += 360_ins; // a full turn: 290
    std::cout << "heading    " << heading << "\n";
    if (heading != 290)
        return 1;

    // 3. Bounded loops.
    const int arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    std::cout << "from 5, wrapping:";
    for (auto i : inside_range<{0, 9}>{5})
        std::cout << ' ' << arr[i];
    std::cout << "\nevery other:     ";
    for (auto i : inside_range<{0, 9}>{}.strided(2))
        std::cout << ' ' << arr[i];
    std::cout << "\nindexed:         ";
    for (auto [pos, i] : inside_range<{0, 9}>{}.indexed())
        if (pos < 3)
            std::cout << " #" << pos << "=" << arr[i];
    std::cout << "\n";
    return 0;
}
