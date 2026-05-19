#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <type_traits>

using namespace bnd;

TEST_CASE("signed storage type selection", "[bound][signed][types]")
{
  using s8  = bound<{-127, 127}>;
  using s16 = bound<{-32000, 32000}>;
  using s32 = bound<{-100000, 100000}>;
  STATIC_REQUIRE(std::is_same_v<typename s8::raw_type,  std::int8_t>);
  STATIC_REQUIRE(std::is_same_v<typename s16::raw_type, std::int16_t>);
  STATIC_REQUIRE(std::is_same_v<typename s32::raw_type, std::int32_t>);

  // fractional notch with negative lower stays unsigned (notch storage)
  using frac_neg = bound<{{-10, 10}, 0.25}>;
  STATIC_REQUIRE(std::is_unsigned_v<typename frac_neg::raw_type>);
}

TEST_CASE("signed construction and arithmetic", "[bound][signed][arithmetic]")
{
  using s32 = bound<{-100000, 100000}>;
  s32 a{42};
  s32 b{-300};
  REQUIRE(a  == 42);
  REQUIRE(b  == -300);

  REQUIRE(-a == -42);
  REQUIRE(-b == 300);

  REQUIRE(a + b == -258);
  REQUIRE(a - b == 342);

  s32 x{-7}, y{14};
  REQUIRE(x * y == -98);
  REQUIRE(y * x == -98);

  auto quot = x / y;
  REQUIRE(*quot == -(*(1_r / 2)));

  s32 acc{100}, delta{-30};
  acc += delta;
  REQUIRE(acc == 70);

  s32 acc2{50};
  acc2 += -75;
  REQUIRE(acc2 == -25);
}

TEST_CASE("signed sentinel doesn't collide with valid -127", "[bound][signed][sentinel]")
{
  using s8 = bound<{-127, 127}>;
  s8 min_val{-127};
  slim::optional<s8> opt{min_val};
  REQUIRE(opt.has_value());
  REQUIRE(*opt == -127);
}

// Regression: signed-direct storage previously recorded the *offset*
// (rhs - Lower) instead of the value when constructed from real-valued
// rhs (rational/double) or from another bound via the bound-to-bound
// store path. Both paths now route through `raw_from_offset<L>`, which
// adds Lower<L> back for direct-storage targets.
TEST_CASE("signed-direct ctor from rational/double preserves value", "[bound][signed][regression]")
{
  using temp = bound<{-40, 60}>;

  REQUIRE(temp{rational{-40}}            == -40);
  REQUIRE(temp{rational{ 42}}            ==  42);
  REQUIRE(temp{rational{  0}}            ==   0);
  REQUIRE(temp{static_cast<double>(-40)} == -40);
  REQUIRE(temp{static_cast<double>( 42)} ==  42);

  // Raw must match value for direct storage.
  REQUIRE(temp{rational{-40}}.Raw == -40);
  REQUIRE(temp{rational{ 42}}.Raw ==  42);
}

TEST_CASE("bound-to-bound assignment preserves value across direct storages",
          "[bound][signed][regression]")
{
  // unsigned-direct → signed-direct
  using upos = bound<{0, 30}>;
  using s40  = bound<{-40, 50}>;
  upos u{25};
  s40  s{u};
  REQUIRE(s     == 25);
  REQUIRE(s.Raw == 25);

  // signed-direct → signed-direct (different grids)
  using s2 = bound<{-20, 30}>;
  s2  s2v{15};
  s40 s40v{s2v};
  REQUIRE(s40v     == 15);
  REQUIRE(s40v.Raw == 15);

  // same-grid stays a fast copy (Offset == 0, Factor == 1)
  s40 s40w{s40v};
  REQUIRE(s40w     == 15);
  REQUIRE(s40w.Raw == 15);
}

TEST_CASE("mixed signed/unsigned arithmetic", "[bound][signed][mixed]")
{
  using u100 = bound<{0, 100}>;
  using s32  = bound<{-100000, 100000}>;
  u100 u{80};
  s32 s{-500};
  auto mixed = u + s;
  REQUIRE(mixed == -420);

  using u8 = bound<{0, 255}>;
  u8 p{10}, q{200};
  auto d = p - q;
  REQUIRE(d == -190);
}

TEST_CASE("signed clamp / wrap", "[bound][signed][policy]")
{
  using sc = bound<{-100, 100}, clamp>;
  REQUIRE(sc{200}  == 100);
  REQUIRE(sc{-200} == -100);

  using sw = bound<{-100, 100}, wrap>;
  REQUIRE(sw{150}  == -51);
  REQUIRE(sw{-150} == 51);
}

TEST_CASE("signed optional helpers", "[bound][signed][optional]")
{
  using s32 = bound<{-100000, 100000}>;
  slim::optional<s32> a{s32{42}};
  slim::optional<s32> none{slim::nullopt};
  REQUIRE(a.has_value());
  REQUIRE(*a == 42);
  REQUIRE_FALSE(none.has_value());

  auto sum = a + s32{-100};
  REQUIRE(sum.has_value());
  REQUIRE(*sum == -58);
}

TEST_CASE("comparison across grids", "[bound][comparison]")
{
  using u100 = bound<{0, 100}>;
  using u50  = bound<{0, 50}>;
  u100 a{30}, b{50};
  u50 c{30};

  REQUIRE(a < b);
  REQUIRE(a == c);

  REQUIRE(a == 30);
  REQUIRE(b > 25);

  using s100 = bound<{-100, 100}>;
  s100 neg{-30}, pos{30};
  REQUIRE(neg < pos);
}
