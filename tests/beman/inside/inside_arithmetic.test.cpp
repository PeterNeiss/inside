#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <type_traits>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;

TEST_CASE("inside add", "[inside][arithmetic][add]")
{
  using u8 = inside<{10, 255}>;
  constexpr u8 a{16};
  constexpr u8 b{220};
  STATIC_REQUIRE(a + one == 17);
  STATIC_REQUIRE(b + one == 221);

  STATIC_REQUIRE(a + b == 236);

  // -ve result widens to signed
  STATIC_REQUIRE(a - b == -204);

  // works at uint64 max
  using u64 = inside<{{0u, std::numeric_limits<std::uint64_t>::max()}, 1}>;
  constexpr u64 biggest{std::numeric_limits<std::uint64_t>::max()};
  STATIC_REQUIRE(biggest == rational{std::numeric_limits<std::uint64_t>::max()});
}

TEST_CASE("inside add: mixed notch with offset storage", "[inside][arithmetic][add][notch]")
{
  // Regression: the notch-offset add path must scale each operand's raw from
  // its own notch up to the result notch (lhs_widen = Notch<L>/Notch<result>).
  // A previous inversion left the scale at 1 whenever notches differed, so the
  // sum was only correct when the lhs offset happened to be 0 or notches were
  // equal. These exercise different notches AND a non-zero (and negative-Lower)
  // offset, which is where the bug surfaced.
  using fine   = inside<{{-4, 4}, notch<1, 16>},  round_nearest>;
  using coarse = inside<{{-8, 8}, notch<1, 256>}, round_nearest>;
  using offset = inside<{{0, 4},  notch<1, 16>},  round_nearest>;

  REQUIRE((fine{0}   + coarse{2}) == 2);    // was -1.75
  REQUIRE((offset{1} + coarse{2}) == 3);    // was 2.0625
  REQUIRE((fine{3}   + coarse{2}) == 5);
  REQUIRE((coarse{2} - fine{1})   == 1);    // sub routes through add(-r)

  // exact fractional result via the integer-pair accessors
  auto frac = fine{rational{1, 2}} + coarse{rational{1, 4}};
  REQUIRE(frac.numerator()   == 3);         // 3/4
  REQUIRE(frac.denominator() == 4);
}

TEST_CASE("inside mul", "[inside][arithmetic][mul]")
{
  using r = inside<{10, 255, 1}>;
  constexpr r a{16};
  constexpr r b{102};
  STATIC_REQUIRE(a * b == 1632);

  // Fractional-notch ops require runtime construction from `double` (the
  // real-valued assignment specialization is not constexpr-evaluable).
  using u4 = inside<{{0.75, 10.5}, 0.25}>;
  u4 d{3};
  u4 e{3.25};
  auto f = d * e;
  REQUIRE(f == *(39_r/4));

  using n4 = u4::negative;
  n4 g{-2};
  n4 h{-2.25};
  auto i = g * h;
  REQUIRE(i == *(9_r/2));

  // Mixed signs
  REQUIRE(d * g == -6);
  REQUIRE(g * d == -6);
}

TEST_CASE("inside div: rational vs integer paths", "[inside][arithmetic][div]")
{
  SECTION("default returns rational raw")
  {
    using r = inside<{1, 255}>;
    constexpr r a{102};
    constexpr r b{16};
    constexpr auto c = a / b;
    STATIC_REQUIRE(c.has_value());
    STATIC_REQUIRE(*c == *(51_r/8));
  }

  SECTION("snap selects integer-storage div path")
  {
    using ui = inside<{0, 100}, snap>;
    constexpr ui a{51}, b{8};
    constexpr auto e = a / b;
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(e)::value_type::raw_type, rational>);
    STATIC_REQUIRE(*e == 6);
  }

  SECTION("per-call snap")
  {
    using u100 = inside<{0, 100}>;
    constexpr u100 a{51}, b{8};
    constexpr auto d = div(a, b, truncated);
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(d)::value_type::raw_type, rational>);
    STATIC_REQUIRE(*d == 6);
  }

  SECTION("division by zero -> nullopt")
  {
    using ui = inside<{0, 100}, snap>;
    ui a{51}, zero{0};
    REQUIRE_FALSE((a / zero).has_value());
  }

  SECTION("offset-encoded storage takes integer path")
  {
    using off = inside<{5, 100}>;
    STATIC_REQUIRE_FALSE(!index_raw<off>);
    STATIC_REQUIRE(index_raw<off>);

    constexpr off a{50}, b{10};
    constexpr auto q = div(a, b, truncated);
    // off's grid {5,100} excludes zero, so div returns a plain inside (no optional).
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(q)::raw_type, rational>);
    STATIC_REQUIRE(q == 5);
  }

  SECTION("non-unit notch trunc")
  {
    using step2 = inside<{{0, 10}, 2}>;
    constexpr step2 a{10}, b{6};
    constexpr auto q = div(a, b, truncated);
    STATIC_REQUIRE(*q == 1);
  }

  SECTION("offset + non-unit notch")
  {
    using step5 = inside<{{5, 15}, 5}>;
    constexpr step5 a{15}, b{5};
    constexpr auto q = div(a, b, truncated);
    // {5,15} excludes zero → plain inside result.
    STATIC_REQUIRE(q == 3);
  }

  SECTION("signed integer division truncates toward zero")
  {
    using si = inside<{-100, 100}, snap>;
    constexpr si a{-7}, b{2};
    constexpr auto q = a / b;
    STATIC_REQUIRE(*q == -3);
  }

  SECTION("divisor grid excluding zero yields a non-optional result")
  {
    using num   = inside<{0, 100}, snap>;
    using pos   = inside<{1, 10},  snap>;   // grid excludes zero
    using spanz = inside<{-5, 10}, snap>;   // grid straddles zero

    constexpr num   a{42};
    constexpr pos   p{3};
    constexpr spanz s{2};

    // Divisor proven nonzero at compile time → plain inside, no unwrap needed.
    STATIC_REQUIRE(insidable<decltype(a / p)>);
    STATIC_REQUIRE_FALSE(is_slim_optional_v<decltype(a / p)>);
    STATIC_REQUIRE(a / p == 14);

    // Divisor whose grid contains zero → still slim::optional<inside>.
    STATIC_REQUIRE(is_slim_optional_v<decltype(a / s)>);
    REQUIRE((a / s).has_value());
    REQUIRE(*(a / s) == 21);
  }
}

TEST_CASE("inside modulo", "[inside][arithmetic][mod]")
{
  using ui = inside<{0, 100}, snap>;
  constexpr ui a{17}, b{5};
  STATIC_REQUIRE(*(a % b) == 2);

  constexpr ui c{100}, d{10};
  STATIC_REQUIRE(*(c % d) == 0);

  // mod-by-zero — runtime only: the `is_constant_evaluated()` throw short-circuits.
  ui a_rt{17}, zero{0};
  REQUIRE_FALSE((a_rt % zero).has_value());

  using si = inside<{-100, 100}, snap>;
  constexpr si sa{-17}, sb{5};
  STATIC_REQUIRE(*(sa % sb) == -2);

  using u50 = inside<{0, 50}>;
  constexpr u50 e{23}, f{7};
  constexpr auto r = mod(e, f, truncated);
  STATIC_REQUIRE(*r == 2);
}

TEST_CASE("inside optional ops propagate nullopt", "[inside][arithmetic][optional]")
{
  using u8 = inside<{1, 255}>;
  constexpr u8 a{100}, b{10};
  constexpr slim::optional<u8> opt_a{a}, opt_b{b};
  constexpr slim::optional<u8> none{slim::nullopt};

  // +
  STATIC_REQUIRE(*(opt_a + b)     == 110);
  STATIC_REQUIRE(*(a + opt_b)     == 110);
  STATIC_REQUIRE(*(opt_a + opt_b) == 110);
  STATIC_REQUIRE_FALSE((none + b).has_value());
  STATIC_REQUIRE_FALSE((a + none).has_value());

  // -
  STATIC_REQUIRE(*(opt_a - b) == 90);
  STATIC_REQUIRE_FALSE((none - b).has_value());

  // *
  STATIC_REQUIRE(*(opt_a * b) == 1000);
  STATIC_REQUIRE_FALSE((a * none).has_value());

  // /
  STATIC_REQUIRE((opt_a / b).has_value());
  STATIC_REQUIRE(*(opt_a / b) == 10);
  STATIC_REQUIRE_FALSE((none / b).has_value());
}

TEST_CASE("inside rational-storage optional ops do not double-wrap", "[inside][arithmetic][optional]")
{
  using frac = inside<{{-10, 10}, 0}>;
  frac f1 = *(2_r/3);
  frac f2 = *(1_r/3);
  slim::optional<frac> opt1{f1}, opt2{f2};
  slim::optional<frac> none{slim::nullopt};

  REQUIRE(*(opt1 + f2) == 1);
  REQUIRE(*(f1 + opt2) == 1);
  REQUIRE_FALSE((none + f2).has_value());

  REQUIRE(*(opt1 - f2) == *(1_r/3));
  REQUIRE(*(opt1 * f2) == *(2_r/9));

  using pfrac = inside<{{1, 10}, 0}>;
  pfrac p1 = 3, p2 = 2;
  slim::optional<pfrac> op1{p1}, op2{p2};
  REQUIRE(*(op1 / p2) == *(3_r/2));
}

TEST_CASE("inside action-first arithmetic overloads", "[inside][arithmetic][actions]")
{
  using c100 = inside<{0, 100}, checked>;
  c100 d{50}, z{0};

  bool fired = false;
  errc seen{};
  auto q = div(d, z,
    on_overflow([&](auto& res, errc c) {
      fired = true;
      seen  = c;
      res = std::remove_cvref_t<decltype(res)>{0};
    }));
  (void)q;

  REQUIRE(fired);
  REQUIRE(seen == errc::division_by_zero);

  // No-action default: nullopt
  auto q_def = div(d, z);
  REQUIRE_FALSE(q_def.has_value());
}

TEST_CASE("inside rational-storage on_overflow free fn", "[inside][arithmetic][rational]")
{
  // Disparate large denominators force cross-multiplication overflow even
  // though the result interval [0, 2] fits.
  constexpr umax M = std::numeric_limits<umax>::max();
  using unit = inside<{{0_r, 1_r}, 0}, checked>;

  auto a = unit::from_raw(rational{1u, static_cast<imax>(M / 2)});
  auto b = unit::from_raw(rational{1u, static_cast<imax>(M / 2 - 1)});

  bool fired = false;
  errc seen{};
  auto sum = add(a, b,
    on_overflow([&](auto& res, errc code) {
      fired = true; seen = code;
      res = unit::from_raw(0_r);   // unit is rational-storage; reset to value 0
    }));
  (void)sum;

  REQUIRE(fired);
  REQUIRE(seen == errc::overflow);
}

TEST_CASE("inside just<N>", "[inside][just]")
{
  STATIC_REQUIRE(just<1>  == 1);
  STATIC_REQUIRE(just<42> == 42);
}

TEST_CASE("unary -_ins composes through the literal parser", "[inside][unary_minus][literal]")
{
  // `-1.5_ins` parses as `-(1.5_ins)`. Verifies that `inside::operator-()` returns
  // a point on the negated grid for rational-storage point bounds.
  STATIC_REQUIRE(rational{-1.5_ins} == -1.5_r);
  STATIC_REQUIRE(rational{-5_ins}   == rational{-5});
  STATIC_REQUIRE(rational{-0x1p-8_ins} == -0x1p-8_r);

  // Compose: `5_ins + (-1.5_ins)` — point + point. The result may be wrapped
  // in optional by the grid arithmetic; either way the value is 3.5.
  STATIC_REQUIRE((5_ins + -1.5_ins) == 3.5_r);
}

TEST_CASE("constexpr arithmetic", "[inside][arithmetic][constexpr]")
{
  using u100 = inside<{0, 100}>;
  constexpr u100 a{30}, b{20};
  STATIC_REQUIRE(a + b == 50);
  STATIC_REQUIRE(a - b == 10);
  STATIC_REQUIRE(b - a == -10);

  using u10 = inside<{1, 10}>;
  constexpr u10 m{3}, n{7};
  STATIC_REQUIRE(m * n == 21);
  STATIC_REQUIRE(-a == -30);

  using s50 = inside<{-50, 50}>;
  constexpr s50 sa{-20}, sb{30};
  STATIC_REQUIRE(sa + sb == 10);
  STATIC_REQUIRE(sa - sb == -50);
  STATIC_REQUIRE(sa * sb == -600);
}

TEST_CASE("scalars need a grid to join inside arithmetic", "[inside][arithmetic][mixed]")
{
  using bin_t = inside<{0, 9}>;
  bin_t b{5};

  // A literal given a grid (`_ins` / `just<N>`) widens like any other inside, so
  // the result stays in the bounded world — no escape to rational/double.
  STATIC_REQUIRE(insidable<decltype(b + 1_ins)>);
  STATIC_REQUIRE(insidable<decltype(2_ins * b)>);
  REQUIRE(b + 1_ins   == 6);
  REQUIRE(b - 1_ins   == 4);
  REQUIRE(b * 2_ins   == 10);
  REQUIRE(just<1> + b == 6);
  REQUIRE(2_ins * b   == 10);

  // Raw `int` / `double` operands are intentionally ill-formed (see the
  // static_assert guidance in arithmetic.hpp): expressions like `b + 1`,
  // `1 + b`, `b * 2.5` AND compound forms like `b += 1` do NOT compile — the
  // programmer must give the scalar a grid (`1_ins` / `just<1>`). Only comparisons
  // (`b == 5`) against a raw scalar stay ergonomic.
  REQUIRE(b == 5);
  b += 1_ins;
  REQUIRE(b == 6);
}

TEST_CASE("exact scalar math stays in inside-space", "[inside][arithmetic][mixed]")
{
  using rn = inside<{{-100, 100}, notch<1, 16>}, round_nearest>;
  rn a{0.5_r};                              // 0.5

  // The old mixed-mode `inside op rational` overloads are gone: a scalar joins
  // inside arithmetic by wearing a grid (`_ins` / `just<>`), and the result stays
  // an inside — no escape into rational.
  STATIC_REQUIRE(insidable<decltype(a + 0.5_ins)>);
  STATIC_REQUIRE(insidable<decltype(a * 2_ins)>);

  // Exact values come back out through the integer-pair accessors, not a
  // rational: 0.5 + 0.5 == 1, 0.5 - 0.25 == 1/4, 0.5 * 2 == 1.
  auto sum = a + 0.5_ins;
  REQUIRE(sum.numerator() == 1);
  REQUIRE(sum.denominator() == 1);

  auto diff = a - 0.25_ins;
  REQUIRE(diff.numerator() == 1);
  REQUIRE(diff.denominator() == 4);

  REQUIRE((a * 2_ins) == 1);
  REQUIRE((2_ins * a) == 1);
}

TEST_CASE("beman::inside::sum - bulk reduction with one deferred check",
          "[inside][arithmetic][sum]")
{
  // Integer raws (fast path): matches the naive += total.
  using elem   = inside<{0, 200'000}, checked>;
  std::vector<elem> v(1000);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<int>(i % 5);
  REQUIRE(beman::inside::sum<elem>(v) == 2000);          // 200·(0+1+2+3+4)

  // The TOTAL is validated, not the running prefix: a clamp target clips
  // once at the end.
  using clamped = inside<{0, 100}, clamp>;
  REQUIRE(beman::inside::sum<clamped>(v) == 100);

  // Q-format elements (index raw): exact fractional accumulation.
  using q = inside<{{0, 4}, notch<1, 256>}, round_nearest>;
  std::vector<q> qs(3, q{rational{1, 256}});
  using qsum = inside<{{0, 16}, notch<1, 256>}, round_nearest>;
  REQUIRE(rational{beman::inside::sum<qsum>(qs)} == rational{3, 256});

  // real storage falls to the exact rational fold — same result.
  using r = inside<{{0, 4}, notch<1, 256>}, round_nearest | real>;
  std::vector<r> rs(3, r{rational{1, 256}});
  REQUIRE(rational{beman::inside::sum<qsum>(rs)} == rational{3, 256});
}

// Issue #7 closure (user decision 2026-06-12): `inside op raw-scalar` is the
// DESIGNED ban — `1_ins` / `just<V>` / `inside<{lo,hi}>{n}` are the API. These
// pins guard the mechanism: the guidance overloads stay SFINAE-transparent
// (probing compiles; the assert fires only on real instantiation), and they
// stay the ONLY match — their `B` return type distinguishes them from any
// accidentally-introduced real widening overload, whose result grid would be
// a different type.
TEST_CASE("scalar-operand ban: guidance overloads pinned",
          "[inside][arithmetic][scalars]")
{
  using pct = inside<{0, 100}>;

  // SFINAE-transparent: the probes are well-formed...
  STATIC_REQUIRE(requires(pct b) { b + 1; });
  STATIC_REQUIRE(requires(pct b) { b - 1; });
  STATIC_REQUIRE(requires(pct b) { b * 2; });
  STATIC_REQUIRE(requires(pct b) { b / 2; });
  STATIC_REQUIRE(requires(pct b) { 1 + b; });

  // ...and resolve to the guidance overloads (return type B), not to a real
  // widening operator (whose result grid would be a different inside type).
  STATIC_REQUIRE(std::same_as<decltype(std::declval<pct>() + 1), pct>);
  STATIC_REQUIRE(std::same_as<decltype(std::declval<pct>() * 2), pct>);
  STATIC_REQUIRE(std::same_as<decltype(2.5 * std::declval<pct>()), pct>);

  // The sanctioned spellings stay open: literals widen, compound narrows,
  // comparisons are free.
  pct b{5};
  auto w = b + 1_ins;
  STATIC_REQUIRE(!std::same_as<decltype(w), pct>);   // widened grid
  REQUIRE(w == 6);
  b += 1_ins;
  REQUIRE(b == 6);
  REQUIRE(b < 10);
}

TEST_CASE("inside-space geometry helpers: dot / cross / lerp", "[inside][arithmetic][geometry]")
{
  using coord = inside<{-10, 10}>;

  // dot(a, b) = ax*bx + ay*by ; cross(a, b) = ax*by - ay*bx (z-component).
  STATIC_REQUIRE(dot  (coord{3}, coord{4}, coord{1}, coord{2}) == 11);
  STATIC_REQUIRE(cross(coord{3}, coord{4}, coord{1}, coord{2}) ==  2);

  // Perpendicular vectors: dot is zero, cross is the signed area.
  STATIC_REQUIRE(dot  (coord{1}, coord{0}, coord{0}, coord{1}) ==  0);
  STATIC_REQUIRE(cross(coord{1}, coord{0}, coord{0}, coord{1}) ==  1);

  // Each result widens past either input grid — no overflow possible.
  STATIC_REQUIRE(!std::same_as<decltype(dot(coord{1}, coord{0}, coord{0}, coord{1})), coord>);

  // lerp(a, b, t) = a + (b - a) * t, with t a [0, 1] fixed-point inside.
  using val = inside<{0, 10}>;
  using t_t = inside<{{0, 1}, notch<1, 4>}, round_nearest>;
  STATIC_REQUIRE(lerp(val{2}, val{8}, t_t{0})   == 2);   // t = 0 -> a
  STATIC_REQUIRE(lerp(val{2}, val{8}, t_t{1})   == 8);   // t = 1 -> b
  STATIC_REQUIRE(lerp(val{2}, val{8}, t_t{0.5}) == 5);   // midpoint (exact)
}
