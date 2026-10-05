// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/range.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  using small_grid = inside_range<{0, 4}>;        // 5 slots: 0,1,2,3,4
  using frac_grid  = inside_range<{{0, 2}, 0.5}>; // 5 slots: 0,0.5,1,1.5,2
  using small_it   = small_grid::iterator;
}

//---------------------------------------------------------------------------
// concept conformance
//---------------------------------------------------------------------------
static_assert(std::input_or_output_iterator<small_it>);
static_assert(std::forward_iterator<small_it>);
static_assert(std::bidirectional_iterator<small_it>);
static_assert(std::random_access_iterator<small_it>);

static_assert(std::ranges::range<small_grid>);
static_assert(std::ranges::forward_range<small_grid>);
static_assert(std::ranges::random_access_range<small_grid>);
static_assert(std::ranges::sized_range<small_grid>);

static_assert(std::ranges::random_access_range<frac_grid>);

//---------------------------------------------------------------------------
// basic iteration
//---------------------------------------------------------------------------
// inside_range: default iteration walks every slot once
TEST(RangeTest, inside_range_default_iteration_walks_every_slot_once)
{
  small_grid r;
  std::vector<int> seen;
  for (auto b : r) seen.push_back(int(to_value(b)));
  ASSERT_TRUE((seen == std::vector<int>{0, 1, 2, 3, 4}));
}

// inside_range: mid-range start wraps around
TEST(RangeTest, inside_range_mid_range_start_wraps_around)
{
  small_grid r{small_grid::value_type{2}};
  std::vector<int> seen;
  for (auto b : r) seen.push_back(int(to_value(b)));
  ASSERT_TRUE((seen == std::vector<int>{2, 3, 4, 0, 1}));
}

// inside_range: fractional notch iterates exact values
TEST(RangeTest, inside_range_fractional_notch_iterates_exact_values)
{
  frac_grid r;
  std::vector<rational> seen;
  for (auto b : r) seen.push_back(b);  // inside -> rational is implicit
  ASSERT_TRUE((seen == std::vector<rational>{0_r, 0.5_r, 1_r, 1.5_r, 2_r}));
}

//---------------------------------------------------------------------------
// random-access arithmetic
//---------------------------------------------------------------------------
// inside_range: iterator arithmetic
TEST(RangeTest, inside_range_iterator_arithmetic)
{
  small_grid r;
  auto b = r.begin();
  auto e = r.end();

  ASSERT_EQ(e - b, 5);
  ASSERT_EQ((b + 5), e);
  ASSERT_EQ((e - 5), b);
  ASSERT_EQ(int(to_value(*(b + 3))), 3);
  ASSERT_EQ(int(to_value(b[2])), 2);

  auto it = b + 2;
  ASSERT_EQ(int(to_value(*it)), 2);
  it += 2;
  ASSERT_EQ(int(to_value(*it)), 4);
  it -= 3;
  ASSERT_EQ(int(to_value(*it)), 1);
  ASSERT_EQ(it - b, 1);
}

// inside_range: ordering and equality
TEST(RangeTest, inside_range_ordering_and_equality)
{
  small_grid r;
  auto b = r.begin();
  auto m = b + 2;
  auto e = r.end();

  ASSERT_EQ(b, b);
  ASSERT_TRUE(b != m);
  ASSERT_TRUE(b <  m);
  ASSERT_TRUE(m <= m);
  ASSERT_TRUE(e >  m);
}

//---------------------------------------------------------------------------
// std::ranges interop
//---------------------------------------------------------------------------
// inside_range: std::ranges::size
TEST(RangeTest, inside_range_std_ranges_size)
{
  small_grid r;
  ASSERT_EQ(std::ranges::size(r), 5);
}

// inside_range: std::views::take
TEST(RangeTest, inside_range_std_views_take)
{
  small_grid r;
  std::vector<int> seen;
  for (auto b : r | std::views::take(3))
    seen.push_back(int(to_value(b)));
  ASSERT_TRUE((seen == std::vector<int>{0, 1, 2}));
}

// inside_range: std::views::reverse
TEST(RangeTest, inside_range_std_views_reverse)
{
  small_grid r;
  std::vector<int> seen;
  for (auto b : r | std::views::reverse)
    seen.push_back(int(to_value(b)));
  ASSERT_TRUE((seen == std::vector<int>{4, 3, 2, 1, 0}));
}

// inside_range: backwards iteration via operator--
TEST(RangeTest, inside_range_backwards_iteration_via_operator)
{
  small_grid r;
  auto it = r.end();
  std::vector<int> seen;
  while (it != r.begin())
  {
    --it;
    seen.push_back(int(to_value(*it)));
  }
  ASSERT_TRUE((seen == std::vector<int>{4, 3, 2, 1, 0}));
}

// inside_range: postfix ++ / -- behave standard
TEST(RangeTest, inside_range_postfix_plus_plus_behave_standard)
{
  small_grid r;
  auto it = r.begin();
  auto snap = it++;
  ASSERT_EQ(int(to_value(*snap)), 0);
  ASSERT_EQ(int(to_value(*it)), 1);

  auto snap2 = it--;
  ASSERT_EQ(int(to_value(*snap2)), 1);
  ASSERT_EQ(int(to_value(*it)), 0);
}

// inside_range::indexed pairs each value with its position
TEST(RangeTest, inside_range_indexed_pairs_each_value_with_its_position)
{
  small_grid r;
  std::vector<std::pair<imax, imax>> seen;
  for (auto [i, v] : r.indexed())
    seen.emplace_back(i, v);
  ASSERT_TRUE((seen == std::vector<std::pair<imax, imax>>{
    {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}
  }));
}

// inside_range works with std::views::reverse
TEST(RangeTest, inside_range_works_with_std_views_reverse)
{
  small_grid r;   // 0,1,2,3,4
  std::vector<imax> seen;
  for (auto v : std::views::reverse(r))
    seen.push_back(v);
  ASSERT_TRUE((seen == std::vector<imax>{4, 3, 2, 1, 0}));
}

// inside_range::strided visits every step-th value
TEST(RangeTest, inside_range_strided_visits_every_step_th_value)
{
  small_grid r;   // 0,1,2,3,4
  auto collect = [&](std::size_t step) {
    std::vector<imax> seen;
    for (auto v : r.strided(step)) seen.push_back(v);
    return seen;
  };
  ASSERT_TRUE((collect(1) == std::vector<imax>{0, 1, 2, 3, 4}));
  ASSERT_TRUE((collect(2) == std::vector<imax>{0, 2, 4}));
  ASSERT_TRUE((collect(3) == std::vector<imax>{0, 3}));
  ASSERT_TRUE(collect(5) == std::vector<imax>{0});

  // Fractional grid strides over notch values too.
  using frac_grid = inside_range<{{0, 1}, per<4>}>;  // 0,.25,.5,.75,1
  frac_grid f;
  std::vector<rational> fseen;
  for (auto v : f.strided(2)) fseen.push_back(rational{v});
  ASSERT_TRUE((fseen == std::vector<rational>{rational{0}, rational{1, 2}, rational{1}}));
}

//---------------------------------------------------------------------------
// storage-kind decode — operator* takes integer fast arms for index/value
// raws (see range.hpp); every arm must agree with the analytic
// Lower + index·Notch, and the ctor must invert it. [perf-paths] pins which
// arm each representative type dispatches to.
//---------------------------------------------------------------------------
namespace
{
  template <typename RangeType>
  void require_decodes_analytically()
  {
    using value_type = typename RangeType::value_type;
    RangeType r;
    std::size_t position = 0;
    for (auto b : r)
    {
      rational expected = (detail::lower64<value_type>
          + (rational{position} * detail::notch64<value_type>).value()).value();
      ASSERT_EQ(as_rational(b), expected);
      ++position;
    }
    ASSERT_EQ(position, r.size());
  }
}

// inside_range: decode agrees with Lower + i*Notch on every storage kind
TEST(RangeTest, inside_range_decode_agrees_with_lower_plus_i_notch_on_every_storage_kind)
{
  require_decodes_analytically<inside_range<{0, 999}>>();                    // value raw
  require_decodes_analytically<inside_range<{-500, 500}>>();                 // value raw, signed
  require_decodes_analytically<inside_range<{{0, 4}, per<256>}>>();     // index raw
  require_decodes_analytically<inside_range<{{-2, 2}, per<4>}>>();      // index raw, offset Lower
  require_decodes_analytically<inside_range<{0, 255}>>();                    // full-width uint8 raw
  require_decodes_analytically<inside_range<{{0, 2}, per<3>}, exact>>();          // rational raw fallback
#ifndef BEMAN_INSIDE_MATH_NO_FP   // under BEMAN_INSIDE_MATH_NO_FP the f64 storage arm is elided
  require_decodes_analytically<inside_range<{{0, 4}, per<256>}, f64 | round_nearest>>(); // fp raw fallback
#endif
}

// inside_range: start ctor inverts the decode on every storage kind
TEST(RangeTest, inside_range_start_ctor_inverts_the_decode_on_every_storage_kind)
{
  auto first_equals_start = [](auto range_tag, auto start_value) {
    using RangeType = decltype(range_tag);
    typename RangeType::value_type start{start_value};
    RangeType r{start};
    ASSERT_EQ(as_rational(*r.begin()), as_rational(start));
  };
  first_equals_start(inside_range<{0, 999}>{}, 500);
  first_equals_start(inside_range<{{0, 4}, per<256>}>{}, 2);
  first_equals_start(inside_range<{{0, 2}, per<3>}, exact>{}, 1);
}

// inside_range: a grid filling its raw type visits every value once
TEST(RangeTest, inside_range_full_width_raw_visits_every_value_once)
{
  using r_t = inside_range<{0, 255}>;
  static_assert(sizeof(r_t::value_type) == 1);
  int expected = 0;
  for (auto b : r_t{})
  {
    ASSERT_EQ(b, expected);
    ++expected;
  }
  ASSERT_EQ(expected, 256);
}

// inside_range: fast decode arms engage (dispatch pins)
TEST(RangeTest, inside_range_fast_decode_arms_engage_dispatch_pins)
{
  // The operator* fast arms are gated on the storage kind; these pins fail if
  // a storage-selection change silently reroutes a type to another arm.
  static_assert(value_raw<inside_range<{0, 999}>::value_type>);
  static_assert(index_raw<inside_range<{{0, 4}, per<256>}>::value_type>);
  static_assert(index_raw<inside_range<{{-2, 2}, per<4>}>::value_type>);
  static_assert(rational_raw<inside_range<{{0, 2}, per<3>}, exact>::value_type>);
#ifndef BEMAN_INSIDE_MATH_NO_FP   // under BEMAN_INSIDE_MATH_NO_FP the f64 storage arm is elided
  static_assert(fp_raw<inside_range<{{0, 4}, per<256>}, f64 | round_nearest>::value_type>);
#endif
}

// inside_range: random access and reverse on a mid-range start
TEST(RangeTest, inside_range_mid_start_random_access_and_reverse)
{
  small_grid r{small_grid::value_type{3}};
  const std::vector<int> want{3, 4, 0, 1, 2};
  auto b = r.begin();
  for (int k = 0; k < 5; ++k)
  {
    EXPECT_EQ(int(to_value(b[k])), want[k]);
    EXPECT_EQ(int(to_value(*(b + k))), want[k]);
  }
  auto e = r.end();
  EXPECT_EQ(int(to_value(*(e - 1))), 2);
  EXPECT_EQ(e - b, 5);
  std::vector<int> rev;
  for (auto v : std::views::reverse(r)) rev.push_back(int(to_value(v)));
  EXPECT_TRUE((rev == std::vector<int>{2, 1, 0, 4, 3}));
}
