#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("conversion between bounds with compatible grids", "[bound][assign]")
{
  using f30t40 = bound<{{30, 40}, 2}>;
  using f20t50 = bound<{interval{20, 50}, 1}>;
  f30t40 smaller{34};
  f20t50 bigger;

  bigger = smaller;
  REQUIRE(bigger == 34);

  // notch 2 -> notch 1 is always compatible (every step lands on integer)
  bigger = 39;
}

TEST_CASE("rounding requires opt-in", "[bound][assign][round]")
{
  using f30t40 = bound<{{30, 40}, 2}>;
  using f20t50 = bound<{interval{20, 50}, 1}>;
  f20t50 bigger{39};
  f30t40 smaller;

  SECTION("policy<snapping>() rounds (truncates toward zero on positive)")
  {
    smaller.policy<snapping>() = bigger;
    REQUIRE(smaller == 38);
  }

  SECTION("with_truncate() is the alias")
  {
    bigger = 30;
    smaller.with_truncate() = bigger;
    REQUIRE(smaller == 30);
  }

  SECTION("with_round_nearest() rounds half up to grid")
  {
    using celsius = bound<{{-40, 60}, 0.5}, round_nearest>;
    celsius room = 21.4;
    REQUIRE(room == 21.5_r);

    celsius exact = 21.5;
    REQUIRE(exact == 21.5_r);

    celsius truncated = 21.2;
    REQUIRE(truncated == 21);

    using half = bound<{{0, 10}, 0.5}>;
    half h;
    h.with_round_nearest() = 3.3;
    REQUIRE(h == 3.5_r);

    h.with_round_nearest() = 3.2;
    REQUIRE(h == 3);
  }

  SECTION("type-level snapping")
  {
    using n2_round = bound<{{0, 10}, 2}, snapping>;
    using n1       = bound<{{0, 10}, 1}>;
    n2_round c;
    c = n1{3};
    REQUIRE(c == 2);
  }

  SECTION("compatible notches require no opt-in")
  {
    using n1 = bound<{{0, 10}, 1}>;
    using n2 = bound<{{0, 10}, 2}>;
    n1 a;
    a = n2{6};
    REQUIRE(a == 6);
  }

  SECTION("wide compatible interval needs no opt-in")
  {
    using n2   = bound<{{0, 10}, 2}>;
    using wide = bound<{{0, 20}, 2}>;
    n2 b;
    b = wide{6};
    REQUIRE(b == 6);
  }
}

TEST_CASE("with_clamp / with_wrap per-operation", "[bound][assign][policy]")
{
  using u100 = bound<{0, 100}>;
  u100 x{50};

  x.with_clamp() = 150;
  REQUIRE(x == 100);

  x.with_wrap() = 103;
  REQUIRE(x == 2);
}

TEST_CASE("clamp during boundable assignment", "[bound][assign][clamp]")
{
  using wide   = bound<{0, 200}>;
  using narrow = bound<{0, 100}, clamp>;
  wide w{150};
  narrow n{0};
  n = w;
  REQUIRE(n == 100);
}

TEST_CASE("unsafe relaxes domain and round checks", "[bound][assign][unsafe]")
{
  // Notch-incompatible assignment compiles under unsafe.
  using src = bound<{{0, 100}, 2}, unsafe>;
  using dst = bound<{{0, 100}, 1}, unsafe>;
  src s{50};
  dst d{0};
  d = s;
  REQUIRE(d == 50);

  // Native int division path engages
  using u100u = bound<{0, 100}, unsafe>;
  u100u a{51}, b{8};
  auto q = a / b;
  STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>);
  REQUIRE(*q == 6);

  // Under unsafe (ignore_zero) the div-by-zero check is skipped entirely:
  // binary `a / 0` is undefined behavior, consistent with the compound `/= 0`
  // no-op below. UB is not testable at runtime, so there is no assertion here.

  // Compound /= by 0: unsafe implies ignore_zero -> silent no-op, value unchanged
  u100u y{50};
  y /= 0;
  REQUIRE(y == 50);

  // Out-of-range silent overwrite (no domain check)
  REQUIRE_NOTHROW(([&] { u100u x{50}; x = 200; (void)x; }()));
}

TEST_CASE("disjoint-interval assignment is allowed under wrap/clamp", "[bound][assign][wrap][clamp]")
{
  // A bound whose interval is wholly outside the target is rejected for strict
  // policies but allowed under wrap/clamp (they bring any value into range —
  // matching the integral-RHS path, whose unbounded interval always overlaps).
  using src = bound<{25, 34}>;
  STATIC_REQUIRE      (bound_assignable<bound<{0, 9}, wrap>,  src>);
  STATIC_REQUIRE      (bound_assignable<bound<{0, 9}, clamp>, src>);
  STATIC_REQUIRE_FALSE(bound_assignable<bound<{0, 9}>,        src>);   // checked: rejected

  bound<{0, 9}, wrap> w{0};
  w = src{27};
  REQUIRE(w == 7);                    // 27 mod 10
  bound<{0, 9}, clamp> c{0};
  c = src{27};
  REQUIRE(c == 9);                    // clamped to upper
}

TEST_CASE("trivial-type guarantees", "[bound][trivial]")
{
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 0},      unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100},    unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{-100, 100}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{{0, 10}, rational{1u, 2}}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{{-10, 10}, 0}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}, clamp>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}, wrap>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}, sentinel>>);

  STATIC_REQUIRE(std::is_trivially_copyable_v<bound<{0, 100}>>);
  STATIC_REQUIRE(std::is_trivially_destructible_v<bound<{0, 100}>>);
  // The default ctor is `= default` for every policy (no zero-fill), so even a
  // `checked` bound is trivially default-constructible — and fully trivial.
  STATIC_REQUIRE(std::is_trivially_default_constructible_v<bound<{0, 100}>>);
  STATIC_REQUIRE(std::is_trivial_v<bound<{0, 100}>>);
}

TEST_CASE("type-alias smoke checks", "[bound][types]")
{
  using test0_t = bound<{{1,3}, 1}>;
  using test4_t = bound<{{0u, std::numeric_limits<umax>::max()}, 1}>;
  using test5_t = bound<{1_r}>;
  STATIC_REQUIRE(std::is_same_v<test0_t::raw_type, std::uint8_t>);
  STATIC_REQUIRE(std::is_same_v<test4_t::raw_type, std::uint64_t>);
  STATIC_REQUIRE(std::is_same_v<test5_t::raw_type, rational>);
}

TEST_CASE("integer rhs into non-integer-interval bound", "[bound][assign][edge]")
{
  // Lower/Upper are non-integer: integer-interval fast path is skipped,
  // exercising the rational-aware out-of-range branch in handle_out_of_range.
  using halfgrid = bound<{{0.5_r, 5.5_r}, 0.5_r}, clamp>;

  halfgrid over{100};                 // out of range, clamps to 5.5
  REQUIRE(over == 5.5_r);

  halfgrid under{-100};               // clamps to 0.5
  REQUIRE(under == 0.5_r);

  halfgrid in{3};                     // 3 lands on a notch (3.0 = 6 notches)
  REQUIRE(in == 3);
}

TEST_CASE("checked policy throws on float out-of-range", "[bound][assign][checked]")
{
  using c10 = bound<{0, 10}, checked>;
  REQUIRE_THROWS_AS(c10{100.0},  std::system_error);
  REQUIRE_THROWS_AS(c10{-1.0},   std::system_error);

  // rational rhs takes the same path
  REQUIRE_THROWS_AS(c10{20_r}, std::system_error);
}

TEST_CASE("checked policy throws on rounding error", "[bound][assign][checked][round]")
{
  using coarse = bound<{{0, 10}, 2}, checked>;
  coarse c;

  // 3.0 doesn't land on notch 2 — round_check fires
  REQUIRE_THROWS_AS((c = 3.0), std::system_error);

  // value on the notch is fine
  REQUIRE_NOTHROW((c = 4.0));
  REQUIRE(c == 4);
}

TEST_CASE("snapping truncates non-notch float at runtime",
          "[bound][assign][snapping]")
{
  using coarse = bound<{{0, 10}, 2}, snapping>;
  coarse c;
  c = 3.0;
  REQUIRE(c == 2);    // truncates toward zero

  c = 7.99;
  REQUIRE(c == 6);
}

TEST_CASE("checked bound-to-bound out-of-range throws", "[bound][assign][bound2bound]")
{
  using src = bound<{0, 100}>;
  using dst = bound<{0, 50}, checked>;
  src s{75};
  REQUIRE_THROWS_AS(dst{s}, std::system_error);
}

TEST_CASE("non-integer-mapping bound-to-bound clamp / domain_fail",
          "[bound][assign][bound2bound][edge]")
{
  // Source has notch 1 to a destination with notch 1/3 — Factor=3, integer.
  // To force the non-integer-mapping path we use a fractional-notch source
  // with a destination that doesn't cleanly align: notch 1/2 -> notch 1/3.
  using src = bound<{{0, 5}, rational{1u, 2}}, snapping>;
  using dst = bound<{{0, 5}, rational{1u, 3}}, snapping | clamp>;

  src s{4};   // value 4 -> dst aligns
  dst d{s};
  REQUIRE(d == 4);

  // src[0,5] dst[0,4] — value 5 in src is out of dst, exercises clamp path
  using dst2 = bound<{{0, 4}, rational{1u, 3}}, snapping | clamp>;
  src s2{5};
  dst2 d2{s2};
  REQUIRE(d2 == 4);
}

TEST_CASE("wrap on fractional / notch grids (boundable rhs)", "[bound][assign][wrap]")
{
  using namespace bnd;
  // {0,1} notch 1/4 — integer interval but fractional notch. Wrap period is
  // (Upper - Lower) + Notch = 1.25; slots {0, .25, .5, .75, 1.0}.
  using dst = bound<{{0, 1}, notch<1, 4>}, wrap | round_nearest>;
  using src = bound<{{-2, 2}, notch<1, 4>}, round_nearest>;

  REQUIRE(rational{dst{src{rational{5, 4}}}}  == rational{0});       // 1.25 -> 0
  REQUIRE(rational{dst{src{rational{6, 4}}}}  == rational{1, 4});    // 1.50 -> 0.25
  REQUIRE(rational{dst{src{rational{7, 4}}}}  == rational{1, 2});    // 1.75 -> 0.50
  REQUIRE(rational{dst{src{rational{-1, 4}}}} == rational{1});       // -0.25 -> 1.0
  REQUIRE(rational{dst{src{rational{-2, 4}}}} == rational{3, 4});    // -0.50 -> 0.75

  // Non-integer interval, notch 1/2: {1/2, 5/2} period = 2 + 1/2 = 2.5.
  using dst2 = bound<{{rational{1, 2}, rational{5, 2}}, notch<1, 2>}, wrap | round_nearest>;
  using src2 = bound<{{-4, 4}, notch<1, 2>}, round_nearest>;
  REQUIRE(rational{dst2{src2{rational{3}}}} == rational{1, 2});      // 3.0 -> 0.5

  // Unit-integer grid still uses the fast path and wraps as before.
  using deg = bound<{0, 359}, wrap>;
  using wide = bound<{-720, 720}>;
  REQUIRE(deg{wide{370}} == 10);
  REQUIRE(deg{wide{-10}} == 350);
}
