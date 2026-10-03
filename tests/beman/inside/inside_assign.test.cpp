#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

TEST_CASE("conversion between bounds with compatible grids", "[inside][assign]")
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  f30t40 smaller{34};
  f20t50 bigger;

  bigger = smaller;
  REQUIRE(bigger == 34);

  // notch 2 -> notch 1 is always compatible (every step lands on integer)
  bigger = 39;
}

TEST_CASE("rounding requires opt-in", "[inside][assign][round]")
{
  using f30t40 = inside<{{30, 40}, 2}>;
  using f20t50 = inside<{interval{20, 50}, 1}>;
  f20t50 bigger{39};
  f30t40 smaller;

  SECTION("policy<snap>() rounds (truncates toward zero on positive)")
  {
    smaller.policy<snap>() = bigger;
    REQUIRE(smaller == 38);
  }

  SECTION("with_snap() is the alias")
  {
    bigger = 30;
    smaller.with_snap() = bigger;
    REQUIRE(smaller == 30);
  }

  SECTION("with_snap<round_nearest>() rounds half up to grid")
  {
    using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
    celsius room = 21.4;
    REQUIRE(room == 21.5_r);

    celsius exact = 21.5;
    REQUIRE(exact == 21.5_r);

    celsius truncated = 21.2;
    REQUIRE(truncated == 21);

    using half = inside<{{0, 10}, 0.5}>;
    half h;
    h.with_snap<round_nearest>() = 3.3;
    REQUIRE(h == 3.5_r);

    h.with_snap<round_nearest>() = 3.2;
    REQUIRE(h == 3);
  }

  SECTION("type-level snap")
  {
    using n2_round = inside<{{0, 10}, 2}, snap>;
    using n1       = inside<{{0, 10}, 1}>;
    n2_round c;
    c = n1{3};
    REQUIRE(c == 2);
  }

  SECTION("type-level round_nearest")
  {
    // Same cross-grid, off-notch assignment as `type-level snap`, but the
    // coarse grid rounds to the nearest slot instead of truncating toward zero
    // (the `num * num` finer→coarser case). Grid points are 0,4,8,12,16,20.
    using n4_round = inside<{{0, 20}, 4}, round_nearest>;
    using n1       = inside<{{0, 20}, 1}>;
    n4_round c;

    c = n1{3};  REQUIRE(c == 4);   // nearer 4 than 0 (snap would give 0)
    c = n1{5};  REQUIRE(c == 4);   // nearer 4 than 8
    c = n1{1};  REQUIRE(c == 0);   // nearer 0 than 4

    // Exact ties round up (half away from zero), per `round_nearest`.
    c = n1{2};  REQUIRE(c == 4);   // 0|4 tie → 4
    c = n1{6};  REQUIRE(c == 8);   // 4|8 tie → 8

    c = n1{8};  REQUIRE(c == 8);   // already on a notch
  }

  SECTION("compatible notches require no opt-in")
  {
    using n1 = inside<{{0, 10}, 1}>;
    using n2 = inside<{{0, 10}, 2}>;
    n1 a;
    a = n2{6};
    REQUIRE(a == 6);
  }

  SECTION("wide compatible interval needs no opt-in")
  {
    using n2   = inside<{{0, 10}, 2}>;
    using wide = inside<{{0, 20}, 2}>;
    n2 b;
    b = wide{6};
    REQUIRE(b == 6);
  }
}

TEST_CASE("with_clamp / with_wrap per-operation", "[inside][assign][policy]")
{
  using u100 = inside<{0, 100}>;
  u100 x{50};

  x.with_clamp() = 150;
  REQUIRE(x == 100);

  x.with_wrap() = 103;
  REQUIRE(x == 2);
}

TEST_CASE("clamp during insidable assignment", "[inside][assign][clamp]")
{
  using wide   = inside<{0, 200}>;
  using narrow = inside<{0, 100}, clamp>;
  wide w{150};
  narrow n{0};
  n = w;
  REQUIRE(n == 100);
}

TEST_CASE("unsafe relaxes domain and round checks", "[inside][assign][unsafe]")
{
  // Notch-incompatible assignment compiles under unsafe.
  using src = inside<{{0, 100}, 2}, unsafe>;
  using dst = inside<{{0, 100}, 1}, unsafe>;
  src s{50};
  dst d{0};
  d = s;
  REQUIRE(d == 50);

  // Native int division path engages
  using u100u = inside<{0, 100}, unsafe>;
  u100u a{51}, b{8};
  auto q = a / b;
  STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>);
  REQUIRE(*q == 6);

  // Under unsafe (ignore_zero) the div-by-zero check is skipped entirely:
  // binary `a / 0` is undefined behavior, consistent with the compound
  // `/= zero-inside` no-op below. UB is not testable at runtime, so no assertion.

  // Compound /= by a zero inside: unsafe implies ignore_zero -> silent no-op.
  u100u y{50};
  y /= u100u{0};
  REQUIRE(y == 50);

  // Out-of-range silent overwrite (no domain check)
  REQUIRE_NOTHROW(([&] { u100u x{50}; x = 200; (void)x; }()));
}

TEST_CASE("disjoint-interval assignment is allowed under wrap/clamp", "[inside][assign][wrap][clamp]")
{
  // An inside whose interval is wholly outside the target is rejected for strict
  // policies but allowed under wrap/clamp (they bring any value into range —
  // matching the integral-RHS path, whose unbounded interval always overlaps).
  using src = inside<{25, 34}>;
  STATIC_REQUIRE      (inside_assignable<inside<{0, 9}, wrap>,  src>);
  STATIC_REQUIRE      (inside_assignable<inside<{0, 9}, clamp>, src>);
  STATIC_REQUIRE_FALSE(inside_assignable<inside<{0, 9}>,        src>);   // checked: rejected

  inside<{0, 9}, wrap> w{0};
  w = src{27};
  REQUIRE(w == 7);                    // 27 mod 10
  inside<{0, 9}, clamp> c{0};
  c = src{27};
  REQUIRE(c == 9);                    // clamped to upper
}

TEST_CASE("trivial-type guarantees", "[inside][trivial]")
{
  STATIC_REQUIRE(std::is_trivial_v<inside<{0, 0},      unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{0, 100},    unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{-100, 100}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{{0, 10}, rational{1u, 2}}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{{-10, 10}, 0}, unsafe>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{0, 100}, clamp>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{0, 100}, wrap>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{0, 100}, sentinel>>);

  STATIC_REQUIRE(std::is_trivially_copyable_v<inside<{0, 100}>>);
  STATIC_REQUIRE(std::is_trivially_destructible_v<inside<{0, 100}>>);
  // The default ctor is `= default` for every policy (no zero-fill), so even a
  // `checked` inside is trivially default-constructible — and fully trivial.
  STATIC_REQUIRE(std::is_trivially_default_constructible_v<inside<{0, 100}>>);
  STATIC_REQUIRE(std::is_trivial_v<inside<{0, 100}>>);
}

TEST_CASE("type-alias smoke checks", "[inside][types]")
{
  using test0_t = inside<{{1,3}, 1}>;
  using test4_t = inside<{{0u, std::numeric_limits<umax>::max()}, 1}>;
  using test5_t = inside<{1_r}>;
  STATIC_REQUIRE(std::is_same_v<test0_t::raw_type, std::uint8_t>);
  STATIC_REQUIRE(std::is_same_v<test4_t::raw_type, std::uint64_t>);
  STATIC_REQUIRE(std::is_same_v<test5_t::raw_type, rational>);
}

TEST_CASE("integer rhs into non-integer-interval inside", "[inside][assign][edge]")
{
  // Lower/Upper are non-integer: integer-interval fast path is skipped,
  // exercising the rational-aware out-of-range branch in handle_out_of_range.
  using halfgrid = inside<{{0.5_r, 5.5_r}, 0.5_r}, clamp>;

  halfgrid over{100};                 // out of range, clamps to 5.5
  REQUIRE(over == 5.5_r);

  halfgrid under{-100};               // clamps to 0.5
  REQUIRE(under == 0.5_r);

  halfgrid in{3};                     // 3 lands on a notch (3.0 = 6 notches)
  REQUIRE(in == 3);
}

TEST_CASE("checked policy throws on float out-of-range", "[inside][assign][checked]")
{
  using c10 = inside<{0, 10}, checked>;
  REQUIRE_THROWS_AS(c10{100.0},  beman::inside::inside_error);
  REQUIRE_THROWS_AS(c10{-1.0},   beman::inside::inside_error);

  // rational rhs takes the same path
  REQUIRE_THROWS_AS(c10{20_r}, beman::inside::inside_error);
}

TEST_CASE("checked policy throws on rounding error", "[inside][assign][checked][round]")
{
  using coarse = inside<{{0, 10}, 2}, checked>;
  coarse c;

  // 3.0 doesn't land on notch 2 — round_check fires
  REQUIRE_THROWS_AS((c = 3.0), beman::inside::inside_error);

  // value on the notch is fine
  REQUIRE_NOTHROW((c = 4.0));
  REQUIRE(c == 4);
}

TEST_CASE("snap truncates non-notch float at runtime",
          "[inside][assign][snap]")
{
  using coarse = inside<{{0, 10}, 2}, snap>;
  coarse c;
  c = 3.0;
  REQUIRE(c == 2);    // truncates toward zero

  c = 7.99;
  REQUIRE(c == 6);
}

TEST_CASE("checked inside-to-inside out-of-range throws", "[inside][assign][inside2inside]")
{
  using src = inside<{0, 100}>;
  using dst = inside<{0, 50}, checked>;
  src s{75};
  REQUIRE_THROWS_AS(dst{s}, beman::inside::inside_error);
}

TEST_CASE("non-integer-mapping inside-to-inside clamp / domain_fail",
          "[inside][assign][inside2inside][edge]")
{
  // Source has notch 1 to a destination with notch 1/3 — Factor=3, integer.
  // To force the non-integer-mapping path we use a fractional-notch source
  // with a destination that doesn't cleanly align: notch 1/2 -> notch 1/3.
  using src = inside<{{0, 5}, rational{1u, 2}}, snap>;
  using dst = inside<{{0, 5}, rational{1u, 3}}, snap | clamp>;

  src s{4};   // value 4 -> dst aligns
  dst d{s};
  REQUIRE(d == 4);

  // src[0,5] dst[0,4] — value 5 in src is out of dst, exercises clamp path
  using dst2 = inside<{{0, 4}, rational{1u, 3}}, snap | clamp>;
  src s2{5};
  dst2 d2{s2};
  REQUIRE(d2 == 4);
}

TEST_CASE("wrap on fractional / notch grids (insidable rhs)", "[inside][assign][wrap]")
{
  using namespace beman::inside;
  // {0,1} notch 1/4 — integer interval but fractional notch. Wrap period is
  // (Upper - Lower) + Notch = 1.25; slots {0, .25, .5, .75, 1.0}.
  using dst = inside<{{0, 1}, notch<1, 4>}, wrap | round_nearest>;
  using src = inside<{{-2, 2}, notch<1, 4>}, round_nearest>;

  REQUIRE(rational{dst{src{rational{5, 4}}}}  == rational{0});       // 1.25 -> 0
  REQUIRE(rational{dst{src{rational{6, 4}}}}  == rational{1, 4});    // 1.50 -> 0.25
  REQUIRE(rational{dst{src{rational{7, 4}}}}  == rational{1, 2});    // 1.75 -> 0.50
  REQUIRE(rational{dst{src{rational{-1, 4}}}} == rational{1});       // -0.25 -> 1.0
  REQUIRE(rational{dst{src{rational{-2, 4}}}} == rational{3, 4});    // -0.50 -> 0.75

  // Non-integer interval, notch 1/2: {1/2, 5/2} period = 2 + 1/2 = 2.5.
  using dst2 = inside<{{rational{1, 2}, rational{5, 2}}, notch<1, 2>}, wrap | round_nearest>;
  using src2 = inside<{{-4, 4}, notch<1, 2>}, round_nearest>;
  REQUIRE(rational{dst2{src2{rational{3}}}} == rational{1, 2});      // 3.0 -> 0.5

  // Unit-integer grid still uses the fast path and wraps as before.
  using deg = inside<{0, 359}, wrap>;
  using wide = inside<{-720, 720}>;
  REQUIRE(deg{wide{370}} == 10);
  REQUIRE(deg{wide{-10}} == 350);
}
