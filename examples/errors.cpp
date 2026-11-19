// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Every way to handle an error, and the policies that avoid one (clamp, wrap). Nothing throws: a store clamps,
// wraps, takes an alternative value, reports, or does not compile. clock.cpp has a fuller on_wrap workflow.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>

using namespace ins;

// An int read at run time, as from input.
int read(int v) {
    volatile int x = v;
    return x;
}

int main() {
    // 1. Reporting an out-of-range 200. A constant is checked while compiling (`percent x = 200;` does not
    // compile), and a run-time int may not fit, so `percent x = n;` does not compile either: name a channel.
    using percent = inside<{0, 100}, checked>;
    const int n = read(200);

    auto maybe = percent::try_make(n);
    std::cout << "expected: " << (maybe ? "has value" : error_message(maybe.error())) << "\n";

    auto ok = percent::try_make(50);
    std::cout << "in-range: " << *ok << "\n";

    // 2. Per-store overrides on a default-checked inside: an error&, a hook with an alternative value, clamp.
    ins::error ec{};
    percent z(50);
    z.on_error(ec) = n;
    std::cout << "policy ec:" << (ec != error{} ? error_message(ec) : "no error") << " (z=" << z << ")" << "\n";

    percent r(50);
    r.on_error([](auto& self, error code, std::string_view msg) {
        std::cout << "on_error: [" << error_message(code) << "] " << msg << " -> recover to 0" << "\n";
        self = 0; // the alternative value
    }) = n;
    std::cout << "          (r=" << r << ")" << "\n";

    inside<{0, 100}> w(50);
    w.with<clamp>() = 200;
    std::cout << "clamped:  value=" << w << " (this store saturates)" << "\n";

    // 3. The other error values: rounding_error (in a hook) and division_by_zero (in the expected of a division).
    using coarse = inside<{{0, 10}, 2}>; // notch 2
    coarse c(0);
    c.on_error([](auto& self, error code, std::string_view msg) {
        std::cout << "rounding: [" << error_message(code) << "] " << msg << " -> recover to 4" << "\n";
        self = 4;
    }) = 3.0; // 3 is not on the notch-2 grid
    std::cout << "          (c=" << c << ")" << "\n";

    const auto quotient = div(percent(10), percent(0));
    if (!quotient)
        std::cout << "div/0:    [" << error_message(quotient.error()) << "] -> recover to 0" << "\n";
    const auto q = quotient.value_or(std::remove_cvref_t<decltype(*quotient)>{0});
    std::cout << "          (q=" << q << ")" << "\n";

    // 4. add / sub / mul / div / mod take an error& too: ec is set, and the result holds the error.
    ec = error{};
    auto qz = div(percent(10), percent(0), ec);
    std::cout << "free-fn:  " << (ec != error{} ? error_message(ec) : "no error")
              << " (q has_value=" << (qz.has_value() ? "true" : "false") << ")" << "\n";

    // 5. Policies that avoid errors: clamp saturates, wrap is modular; try_make reports.
    using clamped_percent = inside<{0, 100}, clamp>;
    using compass_degrees = inside<{0, 359}, wrap>;
    using digit = inside<{0, 9}>;

    clamped_percent cl = 150;
    std::cout << "clamp:    150 -> " << cl << "\n";

    compass_degrees wr = 370;
    std::cout << "wrap:     370 -> " << wr << "\n";

    auto tm = digit::try_make(10);
    std::cout << "try_make: 10  -> " << (tm ? "has value" : error_message(tm.error())) << "\n";

    // 6. Watching clamp and wrap: on_clamp and on_wrap add their policy, so the inside need not have it.
    inside<{0, 100}> p(80);
    p.on_clamp([](auto& self, auto overshoot) {
        std::cout << "on_clamp: overshoot=" << overshoot << " (saturated to " << self << ")" << "\n";
    }) = 150;

    inside<{0, 59}> sec(50);
    sec.on_wrap([](auto& self, auto carry) {
        std::cout << "on_wrap:  carry=" << carry << " (wrapped to " << self << ")" << "\n";
    }) = 75;

    // A hook on a compound op: the sum cannot fail, but storing it back past [0,100] fires on_clamp.
    clamped_percent combo(50);
    combo.on_clamp([](auto&, auto over) { std::cout << "combo:    on_clamp overshoot=" << over << "\n"; }) += 200;
    std::cout << "          (combo=" << combo << ")" << "\n";
    return 0;
}
