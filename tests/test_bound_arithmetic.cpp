#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <type_traits>

using namespace bnd;

TEST_CASE("bound add", "[bound][arithmetic][add]")
{
  using u8 = bound<{10, 255}>;
  u8 a{16};
  u8 b{220};
  REQUIRE(a + just<1> == 17);
  REQUIRE(b + just<1> == 221);

  auto sum = a + b;
  REQUIRE(static_cast<rational>(sum) == 236);

  // -ve result widens to signed
  auto diff = a - b;
  REQUIRE(static_cast<rational>(diff) == -204);

  // works at uint64 max
  using u64 = bound<{{0u, std::numeric_limits<std::uint64_t>::max()}, 1}>;
  constexpr u64 biggest{std::numeric_limits<std::uint64_t>::max()};
  REQUIRE(static_cast<rational>(biggest)
          == rational{std::numeric_limits<std::uint64_t>::max()});
}

TEST_CASE("bound mul", "[bound][arithmetic][mul]")
{
  using r = bound<{10, 255, 1}>;
  r a{16};
  r b{102};
  auto c = a * b;
  REQUIRE(static_cast<rational>(c) == 1632);

  using u4 = bound<{{0.75, 10.5}, 0.25}>;
  u4 d{3};
  u4 e{3.25};
  auto f = d * e;
  REQUIRE(static_cast<rational>(f) == *(39_r/4));

  using n4 = u4::negative;
  n4 g{-2};
  n4 h{-2.25};
  auto i = g * h;
  REQUIRE(static_cast<rational>(i) == *(9_r/2));

  // Mixed signs
  REQUIRE(static_cast<rational>(d * g) == -6);
  REQUIRE(static_cast<rational>(g * d) == -6);
}

TEST_CASE("bound div: rational vs integer paths", "[bound][arithmetic][div]")
{
  SECTION("default returns rational raw")
  {
    using r = bound<{1, 255}>;
    r a{102};
    r b{16};
    auto c = a / b;
    REQUIRE(c.has_value());
    REQUIRE(*c == *(51_r/8));
  }

  SECTION("ignore_round selects integer-storage div path")
  {
    using ui = bound<{0, 100}, ignore_round>;
    ui a{51}, b{8};
    auto e = a / b;
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(e)::value_type::raw_type, rational>);
    REQUIRE(*e == 6);
  }

  SECTION("per-call ignore_round")
  {
    using u100 = bound<{0, 100}>;
    u100 a{51}, b{8};
    auto d = div(a, b, make_policy<ignore_round>());
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(d)::value_type::raw_type, rational>);
    REQUIRE(*d == 6);
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

    off a{50}, b{10};
    auto q = div(a, b, make_policy<ignore_round>());
    STATIC_REQUIRE_FALSE(std::is_same_v<typename decltype(q)::value_type::raw_type, rational>);
    REQUIRE(*q == 5);
  }

  SECTION("non-unit notch trunc")
  {
    using step2 = bound<{{0, 10}, 2}>;
    step2 a{10}, b{6};
    auto q = div(a, b, make_policy<ignore_round>());
    REQUIRE(*q == 1);
  }

  SECTION("offset + non-unit notch")
  {
    using step5 = bound<{{5, 15}, 5}>;
    step5 a{15}, b{5};
    auto q = div(a, b, make_policy<ignore_round>());
    REQUIRE(*q == 3);
  }

  SECTION("signed integer division truncates toward zero")
  {
    using si = bound<{-100, 100}, ignore_round>;
    si a{-7}, b{2};
    auto q = a / b;
    REQUIRE(*q == -3);
  }
}

TEST_CASE("bound modulo", "[bound][arithmetic][mod]")
{
  using ui = bound<{0, 100}, ignore_round>;
  ui a{17}, b{5};
  REQUIRE(*(a % b) == 2);

  ui c{100}, d{10};
  REQUIRE(*(c % d) == 0);

  ui zero{0};
  REQUIRE_FALSE((a % zero).has_value());

  using si = bound<{-100, 100}, ignore_round>;
  si sa{-17}, sb{5};
  REQUIRE(*(sa % sb) == -2);

  using u50 = bound<{0, 50}>;
  u50 e{23}, f{7};
  auto r = mod(e, f, make_policy<ignore_round>());
  REQUIRE(*r == 2);
}

TEST_CASE("bound optional ops propagate nullopt", "[bound][arithmetic][optional]")
{
  using u8 = bound<{1, 255}>;
  u8 a{100}, b{10};
  slim::optional<u8> opt_a{a}, opt_b{b};
  slim::optional<u8> none{slim::nullopt};

  // +
  REQUIRE(*(opt_a + b)     == 110);
  REQUIRE(*(a + opt_b)     == 110);
  REQUIRE(*(opt_a + opt_b) == 110);
  REQUIRE_FALSE((none + b).has_value());
  REQUIRE_FALSE((a + none).has_value());

  // -
  REQUIRE(*(opt_a - b) == 90);
  REQUIRE_FALSE((none - b).has_value());

  // *
  REQUIRE(*(opt_a * b) == 1000);
  REQUIRE_FALSE((a * none).has_value());

  // /
  REQUIRE((opt_a / b).has_value());
  REQUIRE(*(opt_a / b) == 10);
  REQUIRE_FALSE((none / b).has_value());
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
  auto sum = add(a, b, make_policy<checked>(),
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
  REQUIRE(static_cast<rational>(just<1>)  == 1);
  REQUIRE(static_cast<rational>(just<42>) == 42);
}

TEST_CASE("constexpr arithmetic", "[bound][arithmetic][constexpr]")
{
  using u100 = bound<{0, 100}>;
  constexpr u100 a{30}, b{20};
  STATIC_REQUIRE(static_cast<rational>(a + b) == 50);
  STATIC_REQUIRE(static_cast<rational>(a - b) == 10);
  STATIC_REQUIRE(static_cast<rational>(b - a) == -10);

  using u10 = bound<{1, 10}>;
  constexpr u10 m{3}, n{7};
  STATIC_REQUIRE(static_cast<rational>(m * n) == 21);
  STATIC_REQUIRE(static_cast<rational>(-a) == -30);

  using s50 = bound<{-50, 50}>;
  constexpr s50 sa{-20}, sb{30};
  STATIC_REQUIRE(static_cast<rational>(sa + sb) == 10);
  STATIC_REQUIRE(static_cast<rational>(sa - sb) == -50);
  STATIC_REQUIRE(static_cast<rational>(sa * sb) == -600);
}
