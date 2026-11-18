// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Freestanding-leaning smoke test: sees ONLY the amalgamated single header, with
// the string/printing block dropped (INSIDE_NO_STRING) and exceptions off
// (-fno-exceptions, set by CMake). Proves the core needs neither <string> nor
// the exception ABI on its own surface — a custom ins::error_handler stands in
// for the default throw. Runs as the ctest test of the same name.

#include <inside/inside.hpp> // the single header (single_include/ is the only -I)

#include <cstdio>

namespace {
volatile ins::error g_last{};

[[noreturn]] void trap_handler(ins::error code, const char* /*what*/) {
    g_last = code;
    for (;;) {
    } // a bare-metal target would reset/halt
}
} // namespace

int main() {
    ins::set_error_handler(&trap_handler);

    // clamp / wrap resolve without invoking the handler
    ins::inside<{0, 100}, ins::clamp> a{200}; // -> 100
    ins::inside<{0, 9}, ins::wrap>    w{13};  // -> 3

    // error-code channel reports without throwing
    ins::error            ec{};
    ins::inside<{0, 100}> x{0};
    x.on_error(ec) = 150;

    // a checked arithmetic op that stays in range: its result's own grid
    const ins::inside<{0, 100}, ins::checked> s0{40};
    const auto s = s0 + ins::inside<{0, 10}, ins::checked>{10}; // 50, on [0, 110]

    std::printf("a=%d w=%d s=%d ec=%d\n",
                static_cast<int>(ins::detail::to_value(a)),
                static_cast<int>(ins::detail::to_value(w)),
                static_cast<int>(ins::detail::to_value(s)),
                static_cast<int>(ec));

    return (ec != ins::error{}) ? 0 : 1; // ec must have been set (overflow)
}
