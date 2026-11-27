// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A: + − × ÷ and the comparisons, exact, on pairs of grids (offset,
//    unanchored, fine, past 2^62, a divisor holding 0).
//    A <pair> a b sum difference product quotient less equal
// M: % and divmod on integer grids under each rounding mode.
//    M <mode> a b remainder quotient remainder

#include "dump.hpp"

namespace diff {
namespace {
template <class X, class Y>
void arith(rng_t& g, int n, const char* tag) {
    for (int i = 0; i < n; ++i) {
        const X a = pick<X>(g);
        const Y b = pick<Y>(g);
        std::cout << "A " << tag << ' ' << a << ' ' << b << ' ';
        if constexpr (requires { a + b; })
            out(a + b);
        else
            std::cout << 'N';
        std::cout << ' ';
        if constexpr (requires { a - b; })
            out(a - b);
        else
            std::cout << 'N';
        std::cout << ' ';
        if constexpr (requires { a * b; })
            out(a * b);
        else
            std::cout << 'N';
        std::cout << ' ';
        if constexpr (requires { a / b; })
            out(a / b);
        else
            std::cout << 'N';
        std::cout << ' ' << (a < b) << ' ' << (a == b) << '\n';
    }
}

template <class X, class D>
void mod(rng_t& g, int n, const char* mode) {
    for (int i = 0; i < n; ++i) {
        const X a = pick<X>(g);
        const D b = pick<D>(g);
        std::cout << "M " << mode << ' ' << a << ' ' << b << ' ';
        // X's rounding mode, named at the call (the operators take none).
        constexpr policy_flag F = policy_of<X> & (detail::mode_bits | detail::snap);
        out(ins::mod(a, b, make_policy<F>()));
        std::cout << ' ';
        const auto dm = divmod(a, b, make_policy<F>());
        if constexpr (requires { dm.has_value(); }) {
            if (dm) {
                out(dm->Quotient);
                std::cout << ' ';
                out(dm->Remainder);
            } else
                std::cout << 'E' << static_cast<int>(dm.error()) << " E";
        } else {
            out(dm.Quotient);
            std::cout << ' ';
            out(dm.Remainder);
        }
        std::cout << '\n';
    }
}

template <class X>
void mod_all(rng_t& g, int n, const char* mode) {
    mod<X, id<"[1..10]">>(g, n, mode);
    mod<X, id<"[-10..-1]">>(g, n, mode);
    mod<X, id<"[-10..10]">>(g, n, mode);
    mod<X, id<"[-9..9] 3">>(g, n, mode);
    mod<X, id<key{grid{1, 1ll << 40}}>>(g, n, mode);
}
} // namespace

void dump_arith(rng_t& g, int n) {
    using r = rational;
    arith<id<"[-100..100]">, id<"[1..10]">>(g, n, "int");
    arith<id<"[-7..23] 3">, id<"[-9..9] 3">>(g, n, "offset");
    arith<id<key{grid{{-1, 1}, per<(1ll << 40)>}}>, id<key{grid{{r{1, 3}, r{7, 3}}, r{1, 2}}}>>(g, n, "unanchored");
    arith<id<key{grid{-(1ll << 62), 1ll << 62}}>, id<key{grid{-(1ll << 30), 1ll << 30}}>>(g, n, "wide");
    arith<id<"[0..100] 1/1000">, id<"[1..7] 1/3">>(g, n, "thirds");
    arith<id<"[-5..5] 1/7">, id<"[-2..2] 1/11">>(g, n, "sevenths");

    mod_all<id<"[-100..100]">>(g, n, "trunc");
    mod_all<id<"[-100..100] round_floor">>(g, n, "floor");
    mod_all<id<"[-100..100] round_ceil">>(g, n, "ceil");
    mod_all<id<"[-100..100] round_half_away">>(g, n, "nearest");
    mod_all<id<"[-100..100] round_half_even">>(g, n, "even");
    mod_all<id<"[-97..101] 3">>(g, n, "trunc");
    mod_all<id<key{grid{-(1ll << 62), 1ll << 62}, round_half_away}>>(g, n, "nearest");
}
} // namespace diff
