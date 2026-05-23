#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <type_traits>

using namespace bnd;

TEST_CASE("bound add", "[bound][arithmetic][add]")
{
  using u8 = bound<{10, 255}>;
  constexpr u8 a{16};
  constexpr u8 b{220};
  STATIC_REQUIRE(a + just<1> == 17);
  STATIC_REQUIRE(b + just<1> == 221);

  STATIC_REQUIRE(a + b == 236);

  // -ve result widens to signed
  STATIC_REQUIRE(a - b == -204);

  // works at uint64 max
  using u64 = bound<{{0u, std::numeric_limits<std::uint64_t>::max()}, 1}>;
  constexpr u64 biggest{std::numeric_limits<std::uint64_t>::max()};
  STATIC_REQUIRE(biggest == rational{std::numeric_limits<std::uint64_t>::max()});
}

TEST_CASE("bound mul", "[bound][arithmetic][mul]")
{
  using r = bound<{10, 255, 1}>;
  constexpr r a{16};
  constexpr r b{102};
  STATIC_REQUIRE(a * b == 1632);

  // Fractional-notch ops require runtime construction from `double` (the
  // real-valued assignment specialization is not constexpr-evaluable).
  using u4 = bound<{{0.75, 10.5}, 0.25}>;
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

TEST_CASE("bound div: rational vs integer paths", "[bound][arithmetic][div]")
{
  SECTION("default returns rational raw")
  {
    using r = bound<{1, 255}>;
    constexpr r a{102};
    constexpr r b{16};
    constexpr auto c = a / b;
    STATIC_REQUIRE(c.has_value());
    STATIC_REQUIRE(*c == *(51_r/8));
  }

  SECTION("ignore_round selects integer-storage div path")
  {
    using ui = bound<{0, 100}, ignore_round>;
    constexpr ui a{51}, b{8};
    constexpr auto e = a / b;
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(e)::value_type::raw_type, rational>);
    STATIC_REQUIRE(*e == 6);
  }

  SECTION("per-call ignore_round")
  {
    using u100 = bound<{0, 100}>;
    constexpr u100 a{51}, b{8};
    constexpr auto d = div(a, b, truncated);
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(d)::value_type::raw_type, rational>);
    STATIC_REQUIRE(*d == 6);
  }

  SECTION("division by zero -> nullopt")
  {
    using ui = bound<{0, 100}, ignore_round>;
    ui a{51}, zero{0};
    REQUIRE_FALSE((a / zero).has_value());
  }

  SECTION("offset-encoded storage takes integer path")
  {
    using off = bound<{5, 100}>;
    STATIC_REQUIRE_FALSE(is_direct_storage<off>);
    STATIC_REQUIRE(is_notch_storage<off>);

    constexpr off a{50}, b{10};
    constexpr auto q = div(a, b, truncated);
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>);
    STATIC_REQUIRE(*q == 5);
  }

  SECTION("non-unit notch trunc")
  {
    using step2 = bound<{{0, 10}, 2}>;
    constexpr step2 a{10}, b{6};
    constexpr auto q = div(a, b, truncated);
    STATIC_REQUIRE(*q == 1);
  }

  SECTION("offset + non-unit notch")
  {
    using step5 = bound<{{5, 15}, 5}>;
    constexpr step5 a{15}, b{5};
    constexpr auto q = div(a, b, truncated);
    STATIC_REQUIRE(*q == 3);
  }

  SECTION("signed integer division truncates toward zero")
  {
    using si = bound<{-100, 100}, ignore_round>;
    constexpr si a{-7}, b{2};
    constexpr auto q = a / b;
    STATIC_REQUIRE(*q == -3);
  }
}

TEST_CASE("bound modulo", "[bound][arithmetic][mod]")
{
  using ui = bound<{0, 100}, ignore_round>;
  constexpr ui a{17}, b{5};
  STATIC_REQUIRE(*(a % b) == 2);

  constexpr ui c{100}, d{10};
  STATIC_REQUIRE(*(c % d) == 0);

  // mod-by-zero — runtime only: `if consteval { throw }` short-circuits.
  ui a_rt{17}, zero{0};
  REQUIRE_FALSE((a_rt % zero).has_value());

  using si = bound<{-100, 100}, ignore_round>;
  constexpr si sa{-17}, sb{5};
  STATIC_REQUIRE(*(sa % sb) == -2);

  using u50 = bound<{0, 50}>;
  constexpr u50 e{23}, f{7};
  constexpr auto r = mod(e, f, truncated);
  STATIC_REQUIRE(*r == 2);
}

TEST_CASE("bound optional ops propagate nullopt", "[bound][arithmetic][optional]")
{
  using u8 = bound<{1, 255}>;
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

TEST_CASE("bound rational-storage optional ops do not double-wrap", "[bound][arithmetic][optional]")
{
  using frac = bound<{{-10, 10}, 0}>;
  frac f1 = *(2_r/3);
  frac f2 = *(1_r/3);
  slim::optional<frac> opt1{f1}, opt2{f2};
  slim::optional<frac> none{slim::nullopt};

  REQUIRE(*(opt1 + f2) == 1);
  REQUIRE(*(f1 + opt2) == 1);
  REQUIRE_FALSE((none + f2).has_value());

  REQUIRE(*(opt1 - f2) == *(1_r/3));
  REQUIRE(*(opt1 * f2) == *(2_r/9));

  using pfrac = bound<{{1, 10}, 0}>;
  pfrac p1 = 3, p2 = 2;
  slim::optional<pfrac> op1{p1}, op2{p2};
  REQUIRE(*(op1 / p2) == *(3_r/2));
}

TEST_CASE("bound action-first arithmetic overloads", "[bound][arithmetic][actions]")
{
  using c100 = bound<{0, 100}, checked>;
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

TEST_CASE("bound rational-storage on_overflow free fn", "[bound][arithmetic][rational]")
{
  // Disparate large denominators force cross-multiplication overflow even
  // though the result interval [0, 2] fits.
  constexpr umax M = std::numeric_limits<umax>::max();
  using unit = bound<{{rational{0u}, rational{1u}}, 0}, checked>;

  unit a, b;
  a.Raw = rational{1u, static_cast<imax>(M / 2)};
  b.Raw = rational{1u, static_cast<imax>(M / 2 - 1)};

  bool fired = false;
  errc seen{};
  auto sum = add(a, b,
    on_overflow([&](auto& res, errc code) {
      fired = true; seen = code;
      res.Raw = rational{0u};
    }));
  (void)sum;

  REQUIRE(fired);
  REQUIRE(seen == errc::overflow);
}

TEST_CASE("bound just<N>", "[bound][just]")
{
  STATIC_REQUIRE(just<1>  == 1);
  STATIC_REQUIRE(just<42> == 42);
}

TEST_CASE("constexpr arithmetic", "[bound][arithmetic][constexpr]")
{
  using u100 = bound<{0, 100}>;
  constexpr u100 a{30}, b{20};
  STATIC_REQUIRE(a + b == 50);
  STATIC_REQUIRE(a - b == 10);
  STATIC_REQUIRE(b - a == -10);

  using u10 = bound<{1, 10}>;
  constexpr u10 m{3}, n{7};
  STATIC_REQUIRE(m * n == 21);
  STATIC_REQUIRE(-a == -30);

  using s50 = bound<{-50, 50}>;
  constexpr s50 sa{-20}, sb{30};
  STATIC_REQUIRE(sa + sb == 10);
  STATIC_REQUIRE(sa - sb == -50);
  STATIC_REQUIRE(sa * sb == -600);
}
