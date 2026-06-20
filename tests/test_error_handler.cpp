// The replaceable failure handler (bound/detail/debug.hpp). A checked violation
// funnels through bnd::detail::raise -> the installed handler. By default that
// throws bnd::bound_error carrying the originating errc; set_error_handler swaps
// in a user handler (e.g. a bare-metal reset/log) without any <system_error>
// dependency. These tests cover both the default and a custom handler.

#include "bound/bound.hpp"
#include "bound/casts.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <limits>

using namespace bnd;

namespace
{
  // A custom [[noreturn]] handler that records the code, then escapes via a
  // throw of its own type so Catch can observe it (a real bare-metal handler
  // would reset/halt instead).
  struct handler_fired { errc code; const char* what; };
  errc g_seen{};

  [[noreturn]] void recording_handler(errc code, const char* what)
  {
    g_seen = code;
    throw handler_fired{code, what};
  }

  // RAII guard so a thrown handler still restores the default.
  struct scoped_handler
  {
    error_handler_t prev;
    explicit scoped_handler(error_handler_t h) : prev(set_error_handler(h)) {}
    ~scoped_handler() { set_error_handler(prev); }
  };
}

TEST_CASE("default handler throws bound_error carrying the code", "[error][handler]")
{
  using c100 = bound<{0, 100}, checked>;

  SECTION("domain_error on out-of-range assignment")
  {
    try { c100 x{200}; (void)x; FAIL("expected throw"); }
    catch (bound_error const& e) { REQUIRE(e.code == errc::domain_error); }
  }

  SECTION("rounding_error on off-notch checked cast")
  {
    using coarse = bound<{{0, 10}, 2}>;     // notch 2: 3 doesn't land
    try { (void)checked_cast<coarse>(3); FAIL("expected throw"); }
    catch (bound_error const& e) { REQUIRE(e.code == errc::rounding_error); }
  }

  SECTION("not_finite on non-finite real input")
  {
    using R = bound<{0.0, 1.0}, real>;
    try { R r{std::numeric_limits<double>::infinity()}; (void)r; FAIL("expected throw"); }
    catch (bound_error const& e) { REQUIRE(e.code == errc::not_finite); }
  }

  // what() defaults to the static category message.
  try { c100 x{200}; (void)x; }
  catch (bound_error const& e)
  { REQUIRE(std::string_view{e.what()} == errc_message(errc::domain_error)); }
}

TEST_CASE("set_error_handler redirects failures and is restorable", "[error][handler]")
{
  using c100 = bound<{0, 100}, checked>;

  error_handler_t before = get_error_handler();
  {
    scoped_handler guard{&recording_handler};
    REQUIRE(get_error_handler() == &recording_handler);

    g_seen = errc{};
    try { c100 x{200}; (void)x; }
    catch (handler_fired const& f) { REQUIRE(f.code == errc::domain_error); }
    REQUIRE(g_seen == errc::domain_error);
  }
  // Default restored after the guard.
  REQUIRE(get_error_handler() == before);
  REQUIRE_THROWS_AS((c100{200}), bound_error);

  // A null handler restores the default too.
  set_error_handler(&recording_handler);
  set_error_handler(nullptr);
  REQUIRE(get_error_handler() == before);
}
