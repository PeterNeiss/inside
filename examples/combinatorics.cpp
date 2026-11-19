// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Counting (math/integer.hpp): the numbers outgrow any fixed-width integer, and the result types follow that
// growth at compile time, so a type is the range of its answer.

#include <format>
#include <iostream>
#include <string>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/math.hpp>

using namespace ins;

// The first and last 12 of a number's digits.
std::string digits(const std::string& s) {
    return s.substr(0, 12) + "..." + s.substr(s.size() - 12) + " (" + std::to_string(s.size()) + " digits)";
}

// A count read at run time, as from input: checked where it comes in, so a value past T is an error value here.
template <insidable T>
std::expected<T, error> read(int v) {
    volatile int x = v;
    return T::try_make(x);
}

int main() {
    // 1. Factorials: on [0, 20] the result's grid is [0!, 20!], so a uint64_t is proven enough before anything runs.
    const auto n = read<inside<{0, 20}>>(20);
    if (!n)
        return 1;
    const auto f = math::factorial(*n);
    static_assert(upper_of<decltype(f)> == 2'432'902'008'176'640'000);
    static_assert(sizeof(f) == 8);
    std::cout << "20!         " << f << "\n";
    if (to_string(f) != "2432902008176640000")
        return 1;

    const auto n100 = read<inside<{0, 100}>>(100); // on [0, 100] the same call returns a 525-bit integer
    if (!n100)
        return 1;
    const auto f100 = math::factorial(*n100);
    std::cout << "100!        " << digits(to_string(f100)) << "\n";
    if (digits(to_string(f100)) != "933262154439...000000000000 (158 digits)")
        return 1;

    // 2. Binomials: on [0, 60] the deduced type ends exactly at C(60, 30); Pascal's identity holds everywhere.
    using small_count = inside<{0, 60}>;
    static_assert(upper_of<decltype(math::binomial(small_count{}, small_count{}))> == 118'264'581'564'861'424);
    for (int a = 1; a <= 60; ++a)
        for (int b = 1; b < a; ++b) // the loop bounds keep a and b in [0, 60]: unsafe states that precondition
            if (math::binomial(small_count{a, unsafe}, small_count{b, unsafe})
                != math::binomial(small_count{a - 1, unsafe}, small_count{b - 1, unsafe})
                       + math::binomial(small_count{a - 1, unsafe}, small_count{b, unsafe}))
                return 1;

    const auto n49 = read<small_count>(49), k6 = read<small_count>(6);
    if (!n49 || !k6)
        return 1;
    const auto tickets = math::binomial(*n49, *k6);
    std::cout << "C(49, 6)    " << tickets << "\n";

    using large_count = inside<{0, 1000}>;
    const auto n1000 = read<large_count>(1000), k500 = read<large_count>(500);
    if (!n1000 || !k500)
        return 1;
    const auto c1000 = math::binomial(*n1000, *k500);
    std::cout << "C(1000,500) " << digits(to_string(c1000)) << "\n";
    if (tickets != 13'983'816 || digits(to_string(c1000)) != "270288240945...799821216320 (300 digits)")
        return 1;

    // 3. Arrangements: a podium of 3 from 8 runners, P(8, 3); 3 scoops from 5 flavours with repeats, C(7, 3);
    // the letters of MISSISSIPPI, 11! / (1!·4!·4!·2!).
    using runner_count = inside<{0, 8}>;
    const auto runners = read<runner_count>(8), places = read<runner_count>(3);
    if (!runners || !places)
        return 1;
    const auto podium = math::falling_factorial(*runners, *places);
    const auto scoops = math::rising_factorial(just<5>, just<3>) / math::factorial(just<3>);
    const auto mississip = math::multinomial(1_ins, 4_ins, 4_ins, 2_ins); // M, I, S, P
    std::cout << "podiums     " << podium << "\nscoops      " << scoops << "\nMISSISSIPPI " << mississip << "\n";
    if (podium != 336 || scoops != 35 || mississip != 34'650)
        return 1;

    // 4. Odds as exact fractions: one ticket's chance is exactly 1/C(49, 6), which a double rounds.
    const auto odds = just<1> / tickets; // an expected: C(n, k) can be 0 on [0, 60]
    std::cout << "lottery     " << *odds << " (double " << std::format("{:.17g}", 1.0 / 13'983'816) << ")\n";
    if (to_string(*odds) != "1/13983816")
        return 1;

    // Four of a kind: 13 ranks, any fifth card of 48, over C(52, 5) hands.
    constexpr auto quads = just<13> * just<48> / math::binomial(just<52>, just<5>);
    std::cout << "4 of a kind " << quads << "\n";
    static_assert(quads == just<1, 4165>);

    // The birthday problem, 1 − 365·364···(366 − n) / 365^n: at 23 a shared birthday is more likely than not,
    // at 22 not yet, decided exactly while compiling.
    constexpr auto shared23 = just<1> - math::falling_factorial(just<365>, just<23>) / math::pown<23>(just<365>);
    constexpr auto shared22 = just<1> - math::falling_factorial(just<365>, just<22>) / math::pown<22>(just<365>);
    static_assert(shared23 > just<1, 2> && shared22 < just<1, 2>);
    std::cout << "birthday 23 " << std::format("{:.6f}", shared23.to<double>()) << " ("
              << to_string(shared23).size() << " characters exactly)\n";

    // De Méré: P(at least one six in four rolls) = 1 − (5/6)^4.
    constexpr auto demer = just<1> - math::pown<4>(just<5, 6>);
    std::cout << "de Mere     " << demer << " > 1/2\n";
    static_assert(demer > just<1, 2>);
    if (to_string(demer) != "671/1296")
        return 1;

    // 5. gcd and lcm: buses every 12 and 18 minutes meet again after lcm minutes; a 42 × 56 board cuts into
    // squares of side gcd.
    using minutes = inside<{1, 60}>;
    using centimetres = inside<{1, 100}>;
    const auto every12 = read<minutes>(12), every18 = read<minutes>(18);
    const auto w42 = read<centimetres>(42), h56 = read<centimetres>(56);
    if (!every12 || !every18 || !w42 || !h56)
        return 1;
    const auto meet = math::lcm(*every12, *every18);
    const centimetres w = *w42, h = *h56;
    const auto side = math::gcd(w, h);
    std::cout << "buses meet  every " << meet << " minutes\nsquare tile " << side << " cm: "
              << div(w, side, round_toward_zero) << " x " << div(h, side, round_toward_zero) << "\n";
    if (meet != 36 || side != 14)
        return 1;
    return 0;
}
