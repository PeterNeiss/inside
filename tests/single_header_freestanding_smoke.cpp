// Freestanding-leaning smoke test: sees ONLY the amalgamated single header, with
// the string/printing block dropped (BND_NO_STRING) and exceptions off
// (-fno-exceptions, set by CMake). Proves the core needs neither <string> nor
// the exception ABI on its own surface — a custom bnd::error_handler stands in
// for the default throw. Build: `--target single_header_freestanding_smoke`.

#include "bound/bound.hpp"   // the single header (single_include/ is the only -I)

#include <cstdio>

namespace
{
  volatile bnd::errc g_last{};

  [[noreturn]] void trap_handler(bnd::errc code, const char* /*what*/)
  {
    g_last = code;
    for (;;) {}   // a real target would reset/halt
  }
}

int main()
{
  bnd::set_error_handler(&trap_handler);

  // clamp / wrap resolve without invoking the handler
  bnd::bound<{0, 100}, bnd::clamp> a{200};        // -> 100
  bnd::bound<{0, 9},   bnd::wrap>  w{13};          // -> 3

  // error-code channel reports without throwing
  bnd::errc ec{};
  bnd::bound<{0, 100}> x(150, ec);
  (void)x;

  // a checked arithmetic op that stays in range
  bnd::bound<{0, 100}, bnd::checked> s{40};
  s = s + bnd::bound<{0, 100}, bnd::checked>{10};  // 50

  std::printf("a=%d w=%d s=%d ec=%d\n",
              (int)bnd::detail::to_value(a),
              (int)bnd::detail::to_value(w),
              (int)bnd::detail::to_value(s),
              (int)ec);

  return (ec != bnd::errc{}) ? 0 : 1;   // ec must have been set (domain_error)
}
