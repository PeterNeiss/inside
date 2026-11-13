// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Counting: the numbers grow faster than any fixed-width integer, and the
// result types follow that growth at compile time (math/integer.hpp).
//   1. n! for a run-time n: on inside<{0, 20}> the type is inside<{1, 20!}>,
//      8 bytes, known to fit before anything runs; on inside<{0, 100}> a wide
//      raw that holds 100!.
//   2. Binomials: Pascal's triangle up to 60, the lottery's C(49, 6), and
//      C(1000, 500), 300 digits, at run time.
//   3. Arrangements: podium orders, ice-cream scoops with repeats, the letters
//      of MISSISSIPPI.
//   4. Odds as exact fractions: a lottery ticket, four of a kind, the birthday
//      problem, de Méré's bet.
//   5. gcd and lcm: when two buses meet again, the largest square tile.

#include <format>
#include <iostream>
#include <string>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/math.hpp>

using namespace beman::inside;

// The first and last 12 of a number's digits.
std::string digits(const std::string& s) {
    return s.substr(0, 12) + "..." + s.substr(s.size() - 12) + " (" + std::to_string(s.size()) + " digits)";
}

// A count read at run time, as from input, into the inside T: checked where
// it comes in, so a value past T's range is an error value here, not later.
template <insidable T>
std::expected<T, errc> read(int v) {
    volatile int x = v;
    return T::try_make(x);
}

int main() {
    // 1. Factorials. The result's grid is [0!, 20!]: the type is the answer's
    // range, so a uint64_t is proven enough before anything runs.
    const auto n = read<inside<{0, 20}>>(20);
    if (!n)
        return 1;
    const auto f = math::factorial(*n);
    static_assert(upper_of<decltype(f)> == 2'432'902'008'176'640'000);
    static_assert(sizeof(f) == 8);
    std::cout << "20!         " << f << "\n";
    if (to_string(f) != "2432902008176640000")
        return 1;
    // On [0, 100] the same call returns a 525-bit integer.
    const auto n100 = read<inside<{0, 100}>>(100);
    if (!n100)
        return 1;
    const auto f100 = math::factorial(*n100);
    std::cout << "100!        " << digits(to_string(f100)) << "\n";
    if (digits(to_string(f100)) != "933262154439...000000000000 (158 digits)")
        return 1;

    // 2. Binomials. On [0, 60] the deduced type ends exactly at C(60, 30), the
    // largest value C(n, k) can take there; Pascal's identity holds everywhere.
    using small = inside<{0, 60}>;
    static_assert(upper_of<decltype(math::binomial(small{}, small{}))> == 118'264'581'564'861'424);
    // (The loop bounds keep a and b in [0, 60], which an int's type does not
    // say: `unsafe` states that precondition.)
    for (int a = 1; a <= 60; ++a)
        for (int b = 1; b < a; ++b)
            if (math::binomial(small{a, unsafe}, small{b, unsafe}) !=
                math::binomial(small{a - 1, unsafe}, small{b - 1, unsafe}) +
                    math::binomial(small{a - 1, unsafe}, small{b, unsafe}))
                return 1;
    const auto n49 = read<small>(49), k6 = read<small>(6);
    if (!n49 || !k6)
        return 1;
    const auto tickets = math::binomial(*n49, *k6);
    std::cout << "C(49, 6)    " << tickets << "\n";
    using big        = inside<{0, 1000}>;
    const auto n1000 = read<big>(1000), k500 = read<big>(500);
    if (!n1000 || !k500)
        return 1;
    const auto c1000 = math::binomial(*n1000, *k500);
    std::cout << "C(1000,500) " << digits(to_string(c1000)) << "\n";
    if (tickets != 13'983'816 || digits(to_string(c1000)) != "270288240945...799821216320 (300 digits)")
        return 1;

    // 3. Arrangements. A podium of 3 from 8 runners is ordered: P(8, 3).
    // 3 scoops from 5 flavours, repeats allowed: 5·6·7 / 3! = C(7, 3).
    // The letters of MISSISSIPPI: 11! / (1!·4!·4!·2!).
    const auto runners = read<inside<{0, 8}>>(8), places = read<inside<{0, 8}>>(3);
    if (!runners || !places)
        return 1;
    const auto podium    = math::falling_factorial(*runners, *places);
    const auto scoops    = math::rising_factorial(just<5>, just<3>) / math::factorial(just<3>);
    const auto mississip = math::multinomial(1_ins, 4_ins, 4_ins, 2_ins); // M, I, S, P
    std::cout << "podiums     " << podium << "\nscoops      " << scoops << "\nMISSISSIPPI " << mississip << "\n";
    if (podium != 336 || scoops != 35 || mississip != 34'650)
        return 1;

    // 4. Odds as exact fractions. One ticket's chance is exactly 1/C(49, 6); a
    // double rounds it.
    const auto odds = just<1> / tickets; // expected: C(n, k) can be 0 on [0, 60]
    std::cout << "lottery     " << *odds << " (double " << std::format("{:.17g}", 1.0 / 13'983'816) << ")\n";
    if (to_string(*odds) != "1/13983816")
        return 1;

    // Four of a kind: 13 ranks, any fifth card of 48, over C(52, 5) hands.
    constexpr auto quads = just<13> * just<48> / math::binomial(just<52>, just<5>);
    std::cout << "4 of a kind " << quads << "\n";
    static_assert(quads == just<1, 4165>);

    // The birthday problem: with n people, no two share a birthday with
    // probability 365·364···(366 − n) / 365^n. At 23 a shared birthday is more
    // likely than not, at 22 not yet: decided exactly, while compiling.
    constexpr auto shared23 = just<1> - math::falling_factorial(just<365>, just<23>) / math::pown<23>(just<365>);
    constexpr auto shared22 = just<1> - math::falling_factorial(just<365>, just<22>) / math::pown<22>(just<365>);
    static_assert(shared23 > just<1, 2> && shared22 < just<1, 2>);
    std::cout << "birthday 23 " << std::format("{:.6f}", shared23.to<double>()) << " (" << to_string(shared23).size()
              << " characters exactly)\n";

    // De Méré: P(at least one six in four rolls) = 1 − (5/6)^4.
    constexpr auto demer = just<1> - math::pown<4>(just<5, 6>);
    std::cout << "de Mere     " << demer << " > 1/2\n";
    static_assert(demer > just<1, 2>);
    if (to_string(demer) != "671/1296")
        return 1;

    // 5. gcd and lcm. Buses every 12 and 18 minutes leave together again after
    // lcm(12, 18) minutes; a 42 × 56 board cuts into squares of side gcd.
    using minutes      = inside<{1, 60}>;
    const auto every12 = read<minutes>(12), every18 = read<minutes>(18);
    using cm       = inside<{1, 100}>;
    const auto w42 = read<cm>(42), h56 = read<cm>(56);
    if (!every12 || !every18 || !w42 || !h56)
        return 1;
    const auto meet = math::lcm(*every12, *every18);
    const cm   w = *w42, h = *h56;
    const auto side = math::gcd(w, h);
    std::cout << "buses meet  every " << meet << " minutes\nsquare tile " << side
              << " cm: " << div(w, side, round_toward_zero) << " x " << div(h, side, round_toward_zero) << "\n";
    if (meet != 36 || side != 14)
        return 1;
    return 0;
}
