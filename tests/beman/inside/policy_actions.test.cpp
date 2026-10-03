#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string_view>

using namespace beman::inside;
using namespace beman::inside::detail;

TEST_CASE("clamp policy on assignment", "[inside][policy][clamp]")
{
  using pct = inside<{0, 100}, clamp>;
  pct over{150};
  REQUIRE(over == 100);

  pct under{-5};
  REQUIRE(under == 0);

  pct ok{50};
  REQUIRE(ok == 50);

  // compound +
  pct a{90};
  a += inside<{0, 100}>{20};
  REQUIRE(a == 100);
}

TEST_CASE("wrap policy on assignment", "[inside][policy][wrap]")
{
  using angle = inside<{0, 359}, wrap>;
  angle a{370};   REQUIRE(a == 10);
  angle b{-10};   REQUIRE(b == 350);
  angle c{360};   REQUIRE(c == 0);
  angle d{180};   REQUIRE(d == 180);
}

TEST_CASE("sentinel policy hides overflow as nullopt", "[inside][policy][sentinel]")
{
  using idx = inside<{0, 9}, sentinel>;

  idx a{5};
  REQUIRE(a == 5);

  slim::optional<idx> opt{5};
  ++opt;
  REQUIRE(opt.has_value());
  REQUIRE(*opt == 6);

  slim::optional<idx> top{9};
  ++top;
  REQUIRE_FALSE(top.has_value());

  slim::optional<idx> bot{0};
  --bot;
  REQUIRE_FALSE(bot.has_value());
}

TEST_CASE("sentinel policy on fixed-point grids", "[inside][policy][sentinel][fixed]")
{
  SECTION("Q8.8 (notch 1/256) — try_make + in-place ++")
  {
    using fp = inside<{{0, 255}, 1.0/256}, sentinel>;
    static_assert(sizeof(fp) == 2);

    // notch-aligned in-range value
    auto ok = fp::try_make(42.5);
    REQUIRE(ok.has_value());
    REQUIRE(ok->to<double>().value() == 42.5);

    // out-of-range produces nullopt
    auto high = fp::try_make(300.0);
    REQUIRE_FALSE(high.has_value());

    auto low = fp::try_make(-0.5);
    REQUIRE_FALSE(low.has_value());

    // in-place increment past upper bound -> nullopt
    auto made = fp::try_make(255.0);
    REQUIRE(made.has_value());
    slim::optional<fp> top = *made;
    ++top;
    REQUIRE_FALSE(top.has_value());
  }

  SECTION("half-step (notch 0.5) — uint8 storage")
  {
    using sensor = inside<{{0, 50}, 0.5}, sentinel>;
    static_assert(sizeof(sensor) == 1);

    auto s = sensor::try_make(23.5);
    REQUIRE(s.has_value());
    REQUIRE(s->to<double>().value() == 23.5);

    REQUIRE_FALSE(sensor::try_make(50.5).has_value());
    REQUIRE_FALSE(sensor::try_make(-1.0).has_value());
  }

  SECTION("signed Q1.14 (notch 1/16384) — uint16 storage")
  {
    using sample = inside<{{-1, 1}, notch<1, 16384>}, sentinel | round_nearest>;
    static_assert(sizeof(sample) == 2);

    auto s = sample::try_make(0.5);
    REQUIRE(s.has_value());
    REQUIRE(double(*s) == 0.5);

    REQUIRE_FALSE(sample::try_make(1.5).has_value());
    REQUIRE_FALSE(sample::try_make(-1.25).has_value());

    // boundaries ±1 are inclusive
    REQUIRE(sample::try_make(1.0).has_value());
    REQUIRE(sample::try_make(-1.0).has_value());
  }

  SECTION("Q16.16 (notch 1/65536) — uint32 storage")
  {
    using fp = inside<{{0, 65535}, notch<1, 65536>}, sentinel>;
    static_assert(sizeof(fp) == 4);

    auto v = fp::try_make(1000.125);
    REQUIRE(v.has_value());
    REQUIRE(v->to<double>().value() == 1000.125);

    REQUIRE_FALSE(fp::try_make(-0.001).has_value());
    REQUIRE_FALSE(fp::try_make(70000.0).has_value());
  }
}

TEST_CASE("on_sentinel action on fixed-point grids", "[inside][policy][on_sentinel][fixed]")
{
  using fp = inside<{{0, 50}, 0.5}, sentinel>;

  fp v{10.5};
  rational orig{0u};
  v.on_sentinel([&](auto& self, auto orig_in){
    orig = orig_in;
    self = 0;
  }) = 75.5;

  REQUIRE(v.to<double>().value() == 0);
  REQUIRE(orig == 75.5_r);  // 75.5
}

TEST_CASE("on_wrap action receives inside& and carry", "[inside][policy][on_wrap]")
{
  using sec = inside<{0, 59}, wrap>;
  sec s{0};
  imax carry = 0;
  s.on_wrap([&](auto& self, auto c){ carry = c; (void)self; }) = 65;
  REQUIRE(s == 5);
  REQUIRE(carry == 1);

  // handler may override the wrapped value
  sec s2{0};
  s2.on_wrap([](auto& self, auto c){ if (c > 0) self = 0; }) = 65;
  REQUIRE(s2 == 0);
}

TEST_CASE("on_wrap carry is an inside for an inside RHS", "[inside][policy][on_wrap]")
{
  // A insidable RHS routes the wrap through inside arithmetic, so the carry is a
  // inside<excess-grid>. `carry + just<0>` is inside+inside (compiles only for an inside;
  // an imax carry would hit the grid-less-scalar guidance overload).
  using w10 = inside<{0, 9}, wrap>;        // wraps at 10
  w10 x{8};
  inside<{0, 5}> add{4};                    // inside RHS
  imax carry_val = -99;
  x.on_wrap([&](auto&, auto carry){ carry_val = carry + just<0>; }) += add;
  REQUIRE(x == 2);                         // 8 + 4 = 12 -> 12 mod 10
  REQUIRE(carry_val == 1);                 // floor(12 / 10)

  // Fractional grid (notch 1/2) exercises the rational wrap path; the carry is
  // still an inside (range = 3 + 0.5 = 3.5).
  using pos = inside<{{0, 3}, notch<1, 2>}, wrap>;
  pos p{2.5};
  inside<{{0, 2}, notch<1, 2>}> fadd{1.5};
  imax fcarry = -99;
  p.on_wrap([&](auto&, auto carry){ fcarry = carry + just<0>; }) += fadd;
  REQUIRE(p == frac<1, 2>);                // 2.5 + 1.5 = 4.0 -> 4.0 - 3.5 = 0.5
  REQUIRE(fcarry == 1);                    // floor(4.0 / 3.5)
}

TEST_CASE("on_clamp action receives overshoot", "[inside][policy][on_clamp]")
{
  using u100 = inside<{0, 100}>;
  u100 x{0};
  imax overshoot = 0;
  x.on_clamp([&](auto& self, auto over){
    overshoot = over;                            // integer RHS: over is imax
    (void)self;
  }) = 150;
  REQUIRE(x == 100);
  REQUIRE(overshoot == 50);
}

TEST_CASE("on_clamp overshoot is an inside for an inside RHS", "[inside][policy][on_clamp]")
{
  // A insidable RHS routes clamp through inside arithmetic, so the overshoot is a
  // inside<Grid<R> - Grid<L>> — `over` converts to imax implicitly (it would not
  // compile against the old raw optional<rational>).
  using c100 = inside<{0, 100}, clamp>;
  c100 x{0};
  inside<{0, 200}> v{150};                        // overlaps [0,100], runtime out of range
  imax ov = 0;
  x.on_clamp([&](auto&, auto over){ ov = over; }) = v;
  REQUIRE(x == 100);
  REQUIRE(ov == 50);                             // overshoot 150 - 100
}

TEST_CASE("on_error action receives code and message", "[inside][policy][on_error]")
{
  using c100 = inside<{0, 100}, checked>;
  c100 e{50};
  bool fired = false;
  e.on_error([&](auto& self, errc code, std::string_view msg){
    fired = (code == errc::domain_error) && !msg.empty();
    self = 0;
  }) = 200;
  REQUIRE(fired);
  REQUIRE(e == 0);
}

TEST_CASE("on_sentinel action receives original value", "[inside][policy][on_sentinel]")
{
  using s100 = inside<{0, 100}, sentinel>;
  s100 sv{50};
  imax orig = 0;
  sv.on_sentinel([&](auto& self, auto orig_in){
    orig = orig_in;
    self = 50;
  }) = 200;
  REQUIRE(sv == 50);
  REQUIRE(orig == 200);
}

// (Removed: "on_overflow on compound op" / "...subtraction" — they fired the
// action on imax-level overflow from a raw scalar RHS. Raw compound assigns are
// gone, and a range-bounded operand can't overflow imax, so the path is moot.)

TEST_CASE("multi-action with(...) - overflow vs clamp paths fire correctly",
          "[inside][policy][multi]")
{
  using c100 = inside<{0, 100}, checked>;

  SECTION("post-probe narrowing fires on_clamp only")
  {
    c100 b{50};
    bool of = false, cl = false;
    imax over = 0;
    b.with(on_overflow([&](auto&, errc){ of = true; }),
           on_clamp([&](auto&, auto o){ cl = true; over = o; }))
      += 200_ins;   // 50+200=250 overshoots [0,100]; on_clamp fires, not on_overflow
    REQUIRE_FALSE(of);
    REQUIRE(cl);
    REQUIRE(over == 150);
    REQUIRE(b == 100);
  }
}

TEST_CASE("free-fn pack form", "[inside][policy][pack]")
{
  using c100 = inside<{0, 100}, checked>;
  c100 d{50}, z{0};
  bool of = false;
  errc seen{};
  auto q = div(d, z,
    on_overflow([&](auto& res, errc c){
      of = true; seen = c;
      res = std::remove_cvref_t<decltype(res)>{1};
    }),
    on_clamp([](auto&, auto){}));   // inert here, accepted
  (void)q;
  REQUIRE(of);
  REQUIRE(seen == errc::division_by_zero);

  // SFINAE positive cases: pack with on_overflow must bind. The negative case
  // (pack without on_overflow) is enforced by the overload's `requires` clause
  // and would surface noisy compiler diagnostics if probed here.
  using u100 = inside<{0, 100}>;
  auto noop = [](auto&, auto){};
  STATIC_REQUIRE(requires(u100 x, u100 y) { add(x, y, on_overflow(noop)); });
  STATIC_REQUIRE(requires(u100 x, u100 y) { add(x, y, on_overflow(noop), on_clamp(noop)); });
}

TEST_CASE("mod free-fn with on_overflow recovers from div/0", "[inside][policy][mod]")
{
  using u100ic = inside<{0, 100}, checked | snap>;
  u100ic l{7}, r{0};
  bool fired = false;
  auto m = mod(l, r,
    on_overflow([&](auto& res, errc){
      fired = true;
      res = std::remove_cvref_t<decltype(res)>{0};
    }));
  (void)m;
  REQUIRE(fired);
}

TEST_CASE("free-fn div with beman::inside::errc& sets ec on div/0",
          "[inside][policy][ec][div]")
{
  using c100 = inside<{0, 100}, checked>;
  beman::inside::errc ec{};
  auto q = div(c100{10}, c100{0}, ec);
  REQUIRE(ec == errc::division_by_zero);
  REQUIRE_FALSE(q.has_value());

  SECTION("success path leaves ec clear")
  {
    beman::inside::errc ec2{};
    auto r = div(c100{10}, c100{2}, ec2);
    REQUIRE(ec2 == errc{});
    REQUIRE(r.has_value());
  }
}

TEST_CASE("free-fn mod with beman::inside::errc& sets ec on div/0",
          "[inside][policy][ec][mod]")
{
  using u100ic = inside<{0, 100}, checked | snap>;
  beman::inside::errc ec{};
  auto m = mod(u100ic{7}, u100ic{0}, ec);
  REQUIRE(ec == errc::division_by_zero);
  REQUIRE_FALSE(m.has_value());
}

TEST_CASE("free-fn add with beman::inside::errc& compiles and clears on success",
          "[inside][policy][ec][add]")
{
  using c100 = inside<{0, 100}, checked>;
  beman::inside::errc ec{};
  auto sum = add(c100{40}, c100{50}, ec);
  REQUIRE(ec == errc{});
  REQUIRE(sum == 90);

  // SFINAE: the ec form must bind for add / sub / mul / div / mod.
  STATIC_REQUIRE(requires(c100 x, c100 y, beman::inside::errc& e) { add(x, y, e); });
  STATIC_REQUIRE(requires(c100 x, c100 y, beman::inside::errc& e) { sub(x, y, e); });
  STATIC_REQUIRE(requires(c100 x, c100 y, beman::inside::errc& e) { mul(x, y, e); });
  STATIC_REQUIRE(requires(c100 x, c100 y, beman::inside::errc& e) { div(x, y, e); });
}

TEST_CASE("no-arg div on div/0 still returns nullopt without throwing",
          "[inside][policy][regression]")
{
  // Regression guard for fix 1b's empty_ref/error_ref gate: the no-arg form
  // must NOT throw on div/0 — it should silently return nullopt.
  using c100 = inside<{0, 100}, checked>;
  REQUIRE_NOTHROW([]{
    auto q = div(c100{10}, c100{0});
    REQUIRE_FALSE(q.has_value());
  }());
}

TEST_CASE("on_error catches rounding_error on float assignment",
          "[inside][policy][on_error][rounding]")
{
  using coarse = inside<{{0, 10}, 2}>;   // notch 2: 3.0 doesn't land
  coarse c{0};
  errc seen{};
  bool fired = false;
  c.on_error([&](auto& self, errc code, std::string_view) {
    fired = true;
    seen = code;
    self = 4;       // recover to a valid notch value
  }) = 3.0;
  REQUIRE(fired);
  REQUIRE(seen == errc::rounding_error);
  REQUIRE(c == 4);
}

TEST_CASE("policy(ec) catches rounding_error on float assignment",
          "[inside][policy][ec][rounding]")
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};
  beman::inside::errc ec{};
  c.policy(ec) = 3.0;
  REQUIRE(ec == errc::rounding_error);
}
