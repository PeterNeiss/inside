// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Text input: from_chars<B> (exact, through B::try_make) and operator>>.
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace beman::inside;
using detail::rational;

namespace
{
  template <class B>
  rational value_of(std::string_view s)
  {
    auto r = from_chars<B>(s);
    EXPECT_TRUE(r.has_value()) << s;
    return r ? rational{*r} : rational{};
  }
  template <class B>
  errc error_of(std::string_view s)
  {
    auto r = from_chars<B>(s);
    EXPECT_FALSE(r.has_value()) << s;
    return r ? errc{} : r.error();
  }
}

TEST(FromCharsTest, accepts_the_literal_grammar_signs_and_fractions)
{
  using wide = inside<{{-100000, 100000}, per<1024>}, round_nearest>;
  EXPECT_EQ(value_of<wide>("42"), rational{42});
  EXPECT_EQ(value_of<wide>("-1.25"), (rational{-5, 4}));
  EXPECT_EQ(value_of<wide>("+0.5"), (rational{1, 2}));
  EXPECT_EQ(value_of<wide>("1'000"), rational{1000});
  EXPECT_EQ(value_of<wide>("1.5e2"), rational{150});
  EXPECT_EQ(value_of<wide>("25e-2"), (rational{1, 4}));
  EXPECT_EQ(value_of<wide>("0xff"), rational{255});
  EXPECT_EQ(value_of<wide>("0b1010"), rational{10});
  EXPECT_EQ(value_of<wide>("0x1.8p3"), rational{12});
  EXPECT_EQ(value_of<wide>("0x1p-10"), (rational{1, 1024}));
  EXPECT_EQ(value_of<wide>("-3/4"), (rational{-3, 4}));
  using thirds = inside<{{-10, 10}, per<3>}>;
  EXPECT_EQ(value_of<thirds>("7/3"), (rational{7, 3}));                // exact: 0.333… has no decimal
  using tenth = inside<{{0, 1}, per<10>}>;
  EXPECT_EQ(value_of<tenth>("0.1"), (rational{1, 10}));                 // exact, unlike the double 0.1
}

TEST(FromCharsTest, reports_malformed_text_range_and_notch_errors)
{
  using pct = inside<{0, 100}>;
  for (std::string_view bad : {"", "-", "abc", "1.2.3", "12x", "1e", "0x", " 5", "5 ", "0b102", "1/"})
    EXPECT_EQ(error_of<pct>(bad), errc::invalid_format) << bad;
  EXPECT_EQ(error_of<pct>("150"), errc::overflow);
  EXPECT_EQ(error_of<pct>("-1"), errc::overflow);
  EXPECT_EQ(error_of<pct>("2.5"), errc::rounding_error);
  EXPECT_EQ(error_of<pct>("1/0"), errc::division_by_zero);
  EXPECT_EQ(error_of<pct>("99999999999999999999999"), errc::overflow);

  // The policy applies: rounding and clamping instead of errors.
  using rpct = inside<{0, 100}, round_nearest | clamp>;
  EXPECT_EQ(value_of<rpct>("2.5"), rational{3});
  EXPECT_EQ(value_of<rpct>("150"), rational{100});
}

TEST(FromCharsTest, stream_extraction_round_trips_to_string)
{
  using q = inside<{{-10, 10}, per<12>}>;
  for (auto v : {rational{-5, 4}, rational{0}, rational{7, 3}, rational{1, 12}, rational{-10}})
  {
    const q x{v};
    std::stringstream ss{to_string(x)};
    if (to_string(x).find(' ') != std::string::npos) continue;          // mixed "2 1/3" is two tokens
    q y{0};
    ss >> y;
    EXPECT_FALSE(ss.fail()) << to_string(x);
    EXPECT_EQ(y, x) << to_string(x);
  }

  std::stringstream in{"12 abc 7"};
  inside<{0, 100}> a{0}, b{5};
  in >> a >> b;
  EXPECT_EQ(a, 12);
  EXPECT_TRUE(in.fail());
  EXPECT_EQ(b, 5);                                                       // unchanged on error
}

TEST(FromCharsTest, from_chars_is_constexpr)
{
  constexpr const char text[] = "0.75";
  constexpr auto r = from_chars<inside<{{0, 1}, per<4>}>>(text, text + 4);
  static_assert(r.has_value() && rational{*r} == rational{3, 4});
  constexpr const char bad[] = "1.5";
  static_assert(from_chars<inside<{{0, 1}, per<4>}>>(bad, bad + 3).error() == errc::overflow);
}

// Fractional trailing zeros and the exponent of zero carry no value: they must
// not overflow the 64-bit numerator / denominator.
TEST(FromCharsTest, trailing_zeros_and_zero_exponent)
{
  using B = inside<{{0, 10}, per<100>}>;
  EXPECT_EQ(value_of<B>("1.000000000000000000000"), 1);
  EXPECT_EQ(value_of<B>("1.0500000000000000000000000"), (rational{105, 100}));
  EXPECT_EQ(value_of<B>("0.000000000000000000000000"), 0);
  EXPECT_EQ(value_of<B>("0e-30"), 0);
  EXPECT_EQ(value_of<B>("0x0p99"), 0);
  EXPECT_EQ(value_of<B>("0x1.80000000000000000p1"), 3);
  EXPECT_EQ(value_of<B>("2.50e0"), (rational{5, 2}));
  EXPECT_EQ(error_of<B>("1.00000000000000000000001"), errc::overflow);
  static_assert(1.250000000000000000000_r == rational{5, 4});
}
