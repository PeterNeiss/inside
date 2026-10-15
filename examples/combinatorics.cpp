// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Counting: the numbers grow faster than any fixed-width integer, and the
// grids follow that growth at compile time.
//   1. n! as a product of 1..n: its grid is [1, n!], so 20! is known to fit
//      64 bits before anything runs; 21! does not compile under C++23.
//   2. Binomials by the multiplicative rule C·(n − i + 1) / i: divmod gives
//      the quotient and the remainder, which is zero at every step. The count
//      type ends exactly at C(60, 30), the largest answer it must hold.
//   3. Odds as exact fractions: one lottery ticket in C(49, 6), and de Méré's
//      bet on at least one six in four rolls.

#include <format>
#include <iostream>
#include <utility>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

// 1 · 2 · … · N, each factor on its own grid [1, k].
template <std::size_t... I>
constexpr auto factorial(std::index_sequence<I...>) {
    return (just<1> * ... * just<I + 1>);
}

using count = inside<{0, 118'264'581'564'861'424}>; // C(60, 30)
using small = inside<{0, 60}>;
using step  = inside<{1, 60}>; // a divisor that cannot be zero: no expected

// C(n, k) for k ≤ n ≤ 60. After step i the running product is C(n, i), so
// the division is exact; snapped asks for a whole quotient and divmod
// returns what remains, which is checked rather than assumed.
constexpr count binomial(small n, small k) {
    count c{1};
    for (cursor<step> j; j <= k; ++j) {
        const auto [q, r] = divmod(c * (n - j + 1_ins), j, snapped);
        if (r != 0_ins)
            throw "not a whole number";
        c = q; // a checked store: past C(60, 30) it would report overflow
    }
    return c;
}

int main() {
    // 1. The grid of the product is the factorial itself.
    const auto f20 = factorial(std::make_index_sequence<20>{});
    static_assert(upper_of<decltype(f20)> == 2'432'902'008'176'640'000);
    std::cout << "20!         " << f20 << "\n";
    if (to_string(f20) != "2432902008176640000")
        return 1;

    // 2. Pascal's triangle by divmod, checked against its own identity.
    static_assert(binomial(small{60}, small{30}) == just<upper_of<count>>);
    for (int n = 1; n <= 60; ++n)
        for (int k = 1; k < n; ++k)
            if (binomial(small{n}, small{k}) !=
                binomial(small{n - 1}, small{k - 1}) + binomial(small{n - 1}, small{k}))
                return 1;
    const count tickets = binomial(small{49}, small{6});
    std::cout << "C(49, 6)    " << tickets << "\n"
              << "C(60, 30)   " << binomial(small{60}, small{30}) << "\n";

    // 3. One ticket's chance is exactly 1/C(49, 6); a double rounds it.
    const auto odds = one / tickets;
    std::cout << "lottery     " << *odds << " (double " << std::format("{:.17g}", 1.0 / 13'983'816) << ")\n";
    if (to_string(*odds) != "1/13983816")
        return 1;

    // De Méré: P(at least one six in four rolls) = 1 − (5/6)^4 — a constant
    // grid point, computed exactly at compile time.
    constexpr auto miss  = just<rational{5, 6}>;
    constexpr auto demer = one - miss * miss * miss * miss;
    std::cout << "de Mere     " << demer << " > 1/2\n";
    static_assert(demer > just<rational{1, 2}>);
    if (to_string(demer) != "671/1296")
        return 1;
    return 0;
}
