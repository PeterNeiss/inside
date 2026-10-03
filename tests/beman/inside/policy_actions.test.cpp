// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string_view>

using namespace beman::inside;
using namespace beman::inside::detail;

// clamp policy on assignment
TEST(PolicyActionsTest, clamp_policy_on_assignment)
{
  using pct = inside<{0, 100}, clamp>;
  pct over{150};
  ASSERT_EQ(over, 100);

  pct under{-5};
  ASSERT_EQ(under, 0);

  pct ok{50};
  ASSERT_EQ(ok, 50);

  // compound +
  pct a{90};
  a += inside<{0, 100}>{20};
  ASSERT_EQ(a, 100);
}

// wrap policy on assignment
TEST(PolicyActionsTest, wrap_policy_on_assignment)
{
  using angle = inside<{0, 359}, wrap>;
  angle a{370};   ASSERT_EQ(a, 10);
  angle b{-10};   ASSERT_EQ(b, 350);
  angle c{360};   ASSERT_EQ(c, 0);
  angle d{180};   ASSERT_EQ(d, 180);
}

// sentinel policy hides overflow as nullopt
TEST(PolicyActionsTest, sentinel_policy_hides_overflow_as_nullopt)
{
  using idx = inside<{0, 9}, sentinel>;

  idx a{5};
  ASSERT_EQ(a, 5);

  slim::optional<idx> opt{5};
  ++opt;
  ASSERT_TRUE(opt.has_value());
  ASSERT_EQ(*opt, 6);

  slim::optional<idx> top{9};
  ++top;
  ASSERT_FALSE(top.has_value());

  slim::optional<idx> bot{0};
  --bot;
  ASSERT_FALSE(bot.has_value());
}

// sentinel policy on fixed-point grids
TEST(PolicyActionsTest, sentinel_policy_on_fixed_point_grids)
{
  {
    SCOPED_TRACE("Q8.8 (notch 1/256) — try_make + in-place ++");
    using fp = inside<{{0, 255}, 1.0/256}, sentinel>;
    static_assert(sizeof(fp) == 2);

    // notch-aligned in-range value
    auto ok = fp::try_make(42.5);
    ASSERT_TRUE(ok.has_value());
    ASSERT_TRUE(ok->to<double>().value() == 42.5);

    // out-of-range produces nullopt
    auto high = fp::try_make(300.0);
    ASSERT_FALSE(high.has_value());

    auto low = fp::try_make(-0.5);
    ASSERT_FALSE(low.has_value());

    // in-place increment past upper bound -> nullopt
    auto made = fp::try_make(255.0);
    ASSERT_TRUE(made.has_value());
    slim::optional<fp> top = *made;
    ++top;
    ASSERT_FALSE(top.has_value());
  }

  {
    SCOPED_TRACE("half-step (notch 0.5) — uint8 storage");
    using sensor = inside<{{0, 50}, 0.5}, sentinel>;
    static_assert(sizeof(sensor) == 1);

    auto s = sensor::try_make(23.5);
    ASSERT_TRUE(s.has_value());
    ASSERT_TRUE(s->to<double>().value() == 23.5);

    ASSERT_FALSE(sensor::try_make(50.5).has_value());
    ASSERT_FALSE(sensor::try_make(-1.0).has_value());
  }

  {
    SCOPED_TRACE("signed Q1.14 (notch 1/16384) — uint16 storage");
    using sample = inside<{{-1, 1}, notch<1, 16384>}, sentinel | round_nearest>;
    static_assert(sizeof(sample) == 2);

    auto s = sample::try_make(0.5);
    ASSERT_TRUE(s.has_value());
    ASSERT_EQ(double(*s), 0.5);

    ASSERT_FALSE(sample::try_make(1.5).has_value());
    ASSERT_FALSE(sample::try_make(-1.25).has_value());

    // boundaries ±1 are inclusive
    ASSERT_TRUE(sample::try_make(1.0).has_value());
    ASSERT_TRUE(sample::try_make(-1.0).has_value());
  }

  {
    SCOPED_TRACE("Q16.16 (notch 1/65536) — uint32 storage");
    using fp = inside<{{0, 65535}, notch<1, 65536>}, sentinel>;
    static_assert(sizeof(fp) == 4);

    auto v = fp::try_make(1000.125);
    ASSERT_TRUE(v.has_value());
    ASSERT_TRUE(v->to<double>().value() == 1000.125);

    ASSERT_FALSE(fp::try_make(-0.001).has_value());
    ASSERT_FALSE(fp::try_make(70000.0).has_value());
  }
}

// on_sentinel action on fixed-point grids
TEST(PolicyActionsTest, on_sentinel_action_on_fixed_point_grids)
{
  using fp = inside<{{0, 50}, 0.5}, sentinel>;

  fp v{10.5};
  rational orig{0u};
  v.on_sentinel([&](auto& self, auto orig_in){
    orig = orig_in;
    self = 0;
  }) = 75.5;

  ASSERT_TRUE(v.to<double>().value() == 0);
  ASSERT_EQ(orig, 75.5_r);  // 75.5
}

// on_wrap action receives inside& and carry
TEST(PolicyActionsTest, on_wrap_action_receives_inside_and_carry)
{
  using sec = inside<{0, 59}, wrap>;
  sec s{0};
  imax carry = 0;
  s.on_wrap([&](auto& self, auto c){ carry = c; (void)self; }) = 65;
  ASSERT_EQ(s, 5);
  ASSERT_EQ(carry, 1);

  // handler may override the wrapped value
  sec s2{0};
  s2.on_wrap([](auto& self, auto c){ if (c > 0) self = 0; }) = 65;
  ASSERT_EQ(s2, 0);
}

// on_wrap carry is an inside for an inside RHS
TEST(PolicyActionsTest, on_wrap_carry_is_an_inside_for_an_inside_rhs)
{
  // A insidable RHS routes the wrap through inside arithmetic, so the carry is a
  // inside<excess-grid>. `carry + just<0>` is inside+inside (compiles only for an inside;
  // an imax carry would hit the grid-less-scalar guidance overload).
  using w10 = inside<{0, 9}, wrap>;        // wraps at 10
  w10 x{8};
  inside<{0, 5}> add{4};                    // inside RHS
  imax carry_val = -99;
  x.on_wrap([&](auto&, auto carry){ carry_val = carry + just<0>; }) += add;
  ASSERT_EQ(x, 2);                         // 8 + 4 = 12 -> 12 mod 10
  ASSERT_EQ(carry_val, 1);                 // floor(12 / 10)

  // Fractional grid (notch 1/2) exercises the rational wrap path; the carry is
  // still an inside (range = 3 + 0.5 = 3.5).
  using pos = inside<{{0, 3}, notch<1, 2>}, wrap>;
  pos p{2.5};
  inside<{{0, 2}, notch<1, 2>}> fadd{1.5};
  imax fcarry = -99;
  p.on_wrap([&](auto&, auto carry){ fcarry = carry + just<0>; }) += fadd;
  ASSERT_TRUE((p == frac<1, 2>));                // 2.5 + 1.5 = 4.0 -> 4.0 - 3.5 = 0.5
  ASSERT_EQ(fcarry, 1);                    // floor(4.0 / 3.5)
}

// on_clamp action receives overshoot
TEST(PolicyActionsTest, on_clamp_action_receives_overshoot)
{
  using u100 = inside<{0, 100}>;
  u100 x{0};
  imax overshoot = 0;
  x.on_clamp([&](auto& self, auto over){
    overshoot = over;                            // integer RHS: over is imax
    (void)self;
  }) = 150;
  ASSERT_EQ(x, 100);
  ASSERT_EQ(overshoot, 50);
}

// on_clamp overshoot is an inside for an inside RHS
TEST(PolicyActionsTest, on_clamp_overshoot_is_an_inside_for_an_inside_rhs)
{
  // A insidable RHS routes clamp through inside arithmetic, so the overshoot is a
  // inside<Grid<R> - Grid<L>> — `over` converts to imax implicitly (it would not
  // compile against the old raw optional<rational>).
  using c100 = inside<{0, 100}, clamp>;
  c100 x{0};
  inside<{0, 200}> v{150};                        // overlaps [0,100], runtime out of range
  imax ov = 0;
  x.on_clamp([&](auto&, auto over){ ov = over; }) = v;
  ASSERT_EQ(x, 100);
  ASSERT_EQ(ov, 50);                             // overshoot 150 - 100
}

// on_error action receives code and message
TEST(PolicyActionsTest, on_error_action_receives_code_and_message)
{
  using c100 = inside<{0, 100}, checked>;
  c100 e{50};
  bool fired = false;
  e.on_error([&](auto& self, errc code, std::string_view msg){
    fired = (code == errc::domain_error) && !msg.empty();
    self = 0;
  }) = 200;
  ASSERT_TRUE(fired);
  ASSERT_EQ(e, 0);
}

// on_sentinel action receives original value
TEST(PolicyActionsTest, on_sentinel_action_receives_original_value)
{
  using s100 = inside<{0, 100}, sentinel>;
  s100 sv{50};
  imax orig = 0;
  sv.on_sentinel([&](auto& self, auto orig_in){
    orig = orig_in;
    self = 50;
  }) = 200;
  ASSERT_EQ(sv, 50);
  ASSERT_EQ(orig, 200);
}

// (Removed: "on_overflow on compound op" / "...subtraction" — they fired the
// action on imax-level overflow from a raw scalar RHS. Raw compound assigns are
// gone, and a range-bounded operand can't overflow imax, so the path is moot.)

// multi-action with(...) - overflow vs clamp paths fire correctly
TEST(PolicyActionsTest, multi_action_with_overflow_vs_clamp_paths_fire_correctly)
{
  using c100 = inside<{0, 100}, checked>;

  {
    SCOPED_TRACE("post-probe narrowing fires on_clamp only");
    c100 b{50};
    bool of = false, cl = false;
    imax over = 0;
    b.with(on_overflow([&](auto&, errc){ of = true; }),
           on_clamp([&](auto&, auto o){ cl = true; over = o; }))
      += 200_ins;   // 50+200=250 overshoots [0,100]; on_clamp fires, not on_overflow
    ASSERT_FALSE(of);
    ASSERT_TRUE(cl);
    ASSERT_EQ(over, 150);
    ASSERT_EQ(b, 100);
  }
}

// free-fn pack form
TEST(PolicyActionsTest, free_fn_pack_form)
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
  ASSERT_TRUE(of);
  ASSERT_EQ(seen, errc::division_by_zero);

  // SFINAE positive cases: pack with on_overflow must bind. The negative case
  // (pack without on_overflow) is enforced by the overload's `requires` clause
  // and would surface noisy compiler diagnostics if probed here.
  using u100 = inside<{0, 100}>;
  auto noop = [](auto&, auto){};
  static_assert(requires(u100 x, u100 y) { add(x, y, on_overflow(noop)); });
  static_assert(requires(u100 x, u100 y) { add(x, y, on_overflow(noop), on_clamp(noop)); });
}

// mod free-fn with on_overflow recovers from div/0
TEST(PolicyActionsTest, mod_free_fn_with_on_overflow_recovers_from_div_0)
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
  ASSERT_TRUE(fired);
}

// free-fn div with beman::inside::errc& sets ec on div/0 / success path leaves ec clear
TEST(PolicyActionsTest, free_fn_div_with_beman_inside_errc_sets_ec_on_div_0__success_path_leaves_ec_clear)
{
  using c100 = inside<{0, 100}, checked>;
  beman::inside::errc ec{};
  auto q = div(c100{10}, c100{0}, ec);
  ASSERT_EQ(ec, errc::division_by_zero);
  ASSERT_FALSE(q.has_value());

  {
    SCOPED_TRACE("success path leaves ec clear");
    beman::inside::errc ec2{};
    auto r = div(c100{10}, c100{2}, ec2);
    ASSERT_EQ(ec2, errc{});
    ASSERT_TRUE(r.has_value());
  }
}

// free-fn mod with beman::inside::errc& sets ec on div/0
TEST(PolicyActionsTest, free_fn_mod_with_beman_inside_errc_sets_ec_on_div_0)
{
  using u100ic = inside<{0, 100}, checked | snap>;
  beman::inside::errc ec{};
  auto m = mod(u100ic{7}, u100ic{0}, ec);
  ASSERT_EQ(ec, errc::division_by_zero);
  ASSERT_FALSE(m.has_value());
}

// free-fn add with beman::inside::errc& compiles and clears on success
TEST(PolicyActionsTest, free_fn_add_with_beman_inside_errc_compiles_and_clears_on_success)
{
  using c100 = inside<{0, 100}, checked>;
  beman::inside::errc ec{};
  auto sum = add(c100{40}, c100{50}, ec);
  ASSERT_EQ(ec, errc{});
  ASSERT_EQ(sum, 90);

  // SFINAE: the ec form must bind for add / sub / mul / div / mod.
  static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { add(x, y, e); });
  static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { sub(x, y, e); });
  static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { mul(x, y, e); });
  static_assert(requires(c100 x, c100 y, beman::inside::errc& e) { div(x, y, e); });
}

// no-arg div on div/0 still returns nullopt without throwing
TEST(PolicyActionsTest, no_arg_div_on_div_0_still_returns_nullopt_without_throwing)
{
  // Regression guard for fix 1b's empty_ref/error_ref gate: the no-arg form
  // must NOT throw on div/0 — it should silently return nullopt.
  using c100 = inside<{0, 100}, checked>;
  ASSERT_NO_THROW((void)([]{
    auto q = div(c100{10}, c100{0});
    ASSERT_FALSE(q.has_value());
  }()));
}

// on_error catches rounding_error on float assignment
TEST(PolicyActionsTest, on_error_catches_rounding_error_on_float_assignment)
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
  ASSERT_TRUE(fired);
  ASSERT_EQ(seen, errc::rounding_error);
  ASSERT_EQ(c, 4);
}

// policy(ec) catches rounding_error on float assignment
TEST(PolicyActionsTest, policy_ec_catches_rounding_error_on_float_assignment)
{
  using coarse = inside<{{0, 10}, 2}>;
  coarse c{0};
  beman::inside::errc ec{};
  c.policy(ec) = 3.0;
  ASSERT_EQ(ec, errc::rounding_error);
}
