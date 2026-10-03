// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Freestanding-leaning smoke test: sees ONLY the amalgamated single header, with
// the string/printing block dropped (BEMAN_INSIDE_NO_STRING) and exceptions off
// (-fno-exceptions, set by CMake). Proves the core needs neither <string> nor
// the exception ABI on its own surface — a custom beman::inside::error_handler stands in
// for the default throw. Runs as the ctest test of the same name.

#include <beman/inside/inside.hpp>   // the single header (single_include/ is the only -I)

#include <cstdio>

namespace
{
  volatile beman::inside::errc g_last{};

  [[noreturn]] void trap_handler(beman::inside::errc code, const char* /*what*/)
  {
    g_last = code;
    for (;;) {}   // a real target would reset/halt
  }
}

int main()
{
  beman::inside::set_error_handler(&trap_handler);

  // clamp / wrap resolve without invoking the handler
  beman::inside::inside<{0, 100}, beman::inside::clamp> a{200};        // -> 100
  beman::inside::inside<{0, 9},   beman::inside::wrap>  w{13};          // -> 3

  // error-code channel reports without throwing
  beman::inside::errc ec{};
  beman::inside::inside<{0, 100}> x(150, ec);
  (void)x;

  // a checked arithmetic op that stays in range
  beman::inside::inside<{0, 100}, beman::inside::checked> s{40};
  s = s + beman::inside::inside<{0, 100}, beman::inside::checked>{10};  // 50

  std::printf("a=%d w=%d s=%d ec=%d\n",
              static_cast<int>(beman::inside::detail::to_value(a)),
              static_cast<int>(beman::inside::detail::to_value(w)),
              static_cast<int>(beman::inside::detail::to_value(s)),
              static_cast<int>(ec));

  return (ec != beman::inside::errc{}) ? 0 : 1;   // ec must have been set (domain_error)
}
