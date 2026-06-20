#include "bound/bound.hpp"
#include "bound/io.hpp"

#include <catch2/catch_test_macros.hpp>

#include <version>          // __cpp_lib_format
#ifdef __cpp_lib_format
#include <format>
#endif
#include <limits>
#include <sstream>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("rational to_string: decimal forms", "[format][rational]")
{
  REQUIRE(bnd::to_string(rational{1u, 2})    == "0.5");
  REQUIRE(bnd::to_string(rational{43u, 2})   == "21.5");
  REQUIRE(bnd::to_string(rational{1u, 4})    == "0.25");
  REQUIRE(bnd::to_string(rational{3u, 8})    == "0.375");
  REQUIRE(bnd::to_string(rational{1u, 10})   == "0.1");
  REQUIRE(bnd::to_string(rational{15u, 100}) == "0.15");

  // negative
  REQUIRE(bnd::to_string(rational{1, -2})   == "-0.5");
  REQUIRE(bnd::to_string(rational{43, -2})  == "-21.5");
}

TEST_CASE("rational to_string: mixed-number forms", "[format][rational]")
{
  REQUIRE(bnd::to_string(rational{7u, 3})  == "2 1/3");
  REQUIRE(bnd::to_string(rational{22u, 7}) == "3 1/7");
  REQUIRE(bnd::to_string(rational{1u, 3})  == "1/3");
  REQUIRE(bnd::to_string(rational{2u, 7})  == "2/7");
  REQUIRE(bnd::to_string(rational{5u, 1})  == "5");
  REQUIRE(bnd::to_string(0_r)     == "0");

  REQUIRE(bnd::to_string(rational{7,  -3}) == "-2 1/3");
  REQUIRE(bnd::to_string(rational{1,  -3}) == "-1/3");
}

TEST_CASE("rational to_string: overflow boundary falls back to fraction",
          "[format][rational][overflow]")
{
  constexpr umax M = std::numeric_limits<umax>::max();
  REQUIRE(bnd::to_string(rational{M, 2})     == "9223372036854775807 1/2");
  REQUIRE(bnd::to_string(rational{M, -2})    == "-9223372036854775807 1/2");
  REQUIRE(bnd::to_string(rational{M / 5, 2}) == "1844674407370955161.5");
}

TEST_CASE("rational max as integer formats without 1/", "[format][rational]")
{
  constexpr umax M = std::numeric_limits<umax>::max();
  REQUIRE(bnd::to_string(rational{M, 1}) == std::to_string(M));
}

// std::format integration is only exercised where <format> is available
// (libstdc++ from GCC 13). GCC 12 / C++20 builds skip these and keep the
// to_string()/operator<< coverage below.
#ifdef __cpp_lib_format
TEST_CASE("std::format integration", "[format][std_format]")
{
  REQUIRE(std::format("{}",      bound<{0, 99}>{42})    == "42");
  REQUIRE(std::format("[{:>6}]", bound<{0, 99}>{42})    == "[    42]");
  REQUIRE(std::format("[{:<6}]", bound<{0, 99}>{42})    == "[42    ]");
  REQUIRE(std::format("[{:0>4}]",bound<{0, 99}>{42})    == "[0042]");
  REQUIRE(std::format("{}",      rational{1u, 2})       == "0.5");
}

TEST_CASE("std::format numeric specs — integer-aligned bound", "[format][std_format]")
{
  // Integer-aligned bounds route through std::formatter<imax>, so every
  // standard integer spec flows through and matches a direct integer format.
  using pct = bound<{0, 100}>;
  pct x{42};
  imax v = to_value(x);

  REQUIRE(std::format("{:5}",  x) == std::format("{:5}",  v));
  REQUIRE(std::format("{:+}",  x) == std::format("{:+}",  v));
  REQUIRE(std::format("{:#x}", x) == std::format("{:#x}", v));
  REQUIRE(std::format("{:o}",  x) == std::format("{:o}",  v));
  REQUIRE(std::format("{:b}",  x) == std::format("{:b}",  v));
  REQUIRE(std::format("{:08}", x) == std::format("{:08}", v));

  // signed bound
  using s100 = bound<{-100, 100}>;
  s100 y{-7};
  REQUIRE(std::format("{:+}", y)  == "-7");
  REQUIRE(std::format("{:4}", y)  == "  -7");
}

TEST_CASE("std::format numeric specs — fractional bound goes through double",
          "[format][std_format]")
{
  // Fractional grids fall to std::formatter<double> when a spec is present.
  using frac = bound<{{0, 2}, rational{1u, 4}}>;  // 0.25 notch
  frac f{rational{5u, 4}};                          // value = 1.25
  double d = static_cast<double>(rational{f});

  REQUIRE(std::format("{:.2f}", f) == std::format("{:.2f}", d));
  REQUIRE(std::format("{:.4f}", f) == std::format("{:.4f}", d));
  REQUIRE(std::format("{:e}",   f) == std::format("{:e}",   d));

  // empty spec preserves exact-rational form
  REQUIRE(std::format("{}", f) == "1.25");
}

TEST_CASE("std::format numeric specs — representation flags",
          "[format][std_format][storage]")
{
  // `exact` on a notched grid: empty spec keeps the exact fraction; a
  // numeric spec routes through std::formatter<double> like any fractional
  // grid.
  using ex = bound<{{0, 1}, notch<1, 3>}, exact>;
  ex e{rational{2u, 3}};
  REQUIRE(std::format("{}", e) == "2/3");
  REQUIRE(std::format("{:.3f}", e)
          == std::format("{:.3f}", static_cast<double>(rational{e})));

  // `indexed` keeps the integer-grid routing: specs format the VALUE (via
  // std::formatter<imax>), never the raw index.
  using ix = bound<{-5, 5}, indexed>;
  ix i{-3};
  REQUIRE(std::format("{}",    i) == "-3");
  REQUIRE(std::format("{:>4}", i) == "  -3");
  REQUIRE(std::format("{:+}",  i) == "-3");
}

TEST_CASE("std::format numeric specs — rational", "[format][std_format][rational]")
{
  // Empty spec — exact via to_string.
  REQUIRE(std::format("{}", rational{1u, 3}) == "1/3");
  REQUIRE(std::format("{}", rational{7u, 3}) == "2 1/3");

  // Non-empty spec — double formatter.
  rational r = rational{1u, 3};
  REQUIRE(std::format("{:.3f}", r) == std::format("{:.3f}", static_cast<double>(r)));
  REQUIRE(std::format("{:.6f}", r) == std::format("{:.6f}", static_cast<double>(r)));
  REQUIRE(std::format("{:e}",   r) == std::format("{:e}",   static_cast<double>(r)));
}
#endif // __cpp_lib_format

TEST_CASE("interval and grid to_string", "[format][interval][grid]")
{
  REQUIRE(bnd::to_string(interval{0, 10})           == "[0..10]");
  REQUIRE(bnd::to_string(grid{interval{0, 10}, 1_r}) == "{[0..10], 1}");
}

TEST_CASE("ostream operator<< for rational", "[format][print][ostream]")
{
  std::ostringstream os;
  os << rational{3u, 4};
  REQUIRE(os.str() == "0.75");

  std::ostringstream os2;
  os2 << rational{1, -3};
  REQUIRE(os2.str() == "-1/3");
}

TEST_CASE("ostream operator<< for bound", "[format][print][ostream]")
{
  std::ostringstream os;
  os << bound<{0, 100}>{42};
  REQUIRE(os.str() == "42");

  std::ostringstream os2;
  os2 << bound<{{-5, 5}, 0.5}>{2.5};
  REQUIRE(os2.str() == "2.5");
}

TEST_CASE("to_string_debug emits raw, type, and grid", "[format][print][debug]")
{
  using pct = bound<{0, 100}>;
  pct x{42};
  auto s = to_string_debug(x);

  // sanity: value, raw type, and grid all surface in the debug string
  REQUIRE(s.find("42") != std::string::npos);
  REQUIRE(s.find("uint8_t") != std::string::npos);
  REQUIRE(s.find("[0..100]") != std::string::npos);

  // signed-storage path — raw type should show the chosen signed integer
  using s8 = bound<{-100, 100}>;
  s8 y{-7};
  auto sd = to_string_debug(y);
  REQUIRE(sd.find("-7") != std::string::npos);
  REQUIRE(sd.find("int8_t") != std::string::npos);
}

TEST_CASE("type_name covers all raw types", "[format][type_name]")
{
  REQUIRE(type_name<std::uint8_t>()  == "uint8_t");
  REQUIRE(type_name<std::uint16_t>() == "uint16_t");
  REQUIRE(type_name<std::uint32_t>() == "uint32_t");
  REQUIRE(type_name<std::uint64_t>() == "uint64_t");
  REQUIRE(type_name<std::int8_t>()   == "int8_t");
  REQUIRE(type_name<std::int16_t>()  == "int16_t");
  REQUIRE(type_name<std::int32_t>()  == "int32_t");
  REQUIRE(type_name<std::int64_t>()  == "int64_t");
  REQUIRE(type_name<rational>()      == "rational");
  REQUIRE(type_name<float>()         == "unknown");
}
