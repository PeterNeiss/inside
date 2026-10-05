// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The adaptive math engine: fixed-point helpers, constants at any precision,
// and the decision step that rounds a result onto the output grid correctly.

#include <beman/inside/detail/math_adaptive.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
namespace ax = beman::inside::math::detail::ax;
using detail::rational;
using detail::exact_frac;
using detail::wide_sint;
using beman::inside::umax;

namespace
{
  // {0, 1} in eighths, rounded to nearest / floor / ceil.
  using eighths       = inside<{{0, 1}, rational{1, 8}}, round_nearest>;
  using eighths_floor = inside<{{0, 1}, rational{1, 8}}, round_floor>;
  using eighths_ceil  = inside<{{0, 1}, rational{1, 8}}, round_ceil>;
  using eighths_clamp = inside<{{0, 1}, rational{1, 8}}, round_nearest | clamp>;
  // A notch that is not a power of two, and an offset Lower.
  using sevenths      = inside<{{rational{-6, 7}, rational{15, 7}}, rational{3, 7}}, round_nearest>;

  template <std::size_t K>
  constexpr wide_sint<K> hex2(umax hi, umax lo)
  { return (wide_sint<K>{hi} << 64) | wide_sint<K>{lo}; }

  // Value v (a rational) as an approx at scale 2^W with error e units.
  template <int W>
  constexpr ax::approx<4> approx_of(rational v, umax e)
  {
    return {ax::to_q<W, 4>(detail::exact_of(v)), W, e};
  }
}

TEST(MathAdaptiveTest, constants)
{
  // floor(π·2^128) and floor(ln 2·2^128) are 0x3243f6a8885a308d313198a2e03707344
  // and 0xb17217f7d1cf79abc9e3b39803f2f6af; each constant is within one unit.
  constexpr auto pi  = ax::pi_q<128>;
  constexpr auto ln2 = ax::ln2_q<128>;
  using P = std::remove_cvref_t<decltype(pi)>;
  using L = std::remove_cvref_t<decltype(ln2)>;
  const P pi_ref  = (P{3} << 128) | hex2<ax::limbs_of<P>>(0x243f6a8885a308d3ULL, 0x13198a2e03707344ULL);
  const L ln2_ref = hex2<ax::limbs_of<L>>(0xb17217f7d1cf79abULL, 0xc9e3b39803f2f6afULL);
  const P dp = pi - pi_ref;
  const L dl = ln2 - ln2_ref;
  EXPECT_TRUE(P{-1} <= dp && dp <= P{1});
  EXPECT_TRUE(L{-1} <= dl && dl <= L{1});
  // At 64 bits.
  static_assert(static_cast<umax>(ax::ln2_q<64>) - 0xb17217f7d1cf79abULL <= 1);
}

TEST(MathAdaptiveTest, fixed_point_helpers)
{
  using W4 = wide_sint<4>;
  // 1/3 at scale 2^10: 341.33 → 341; −2/3 → −683 (half away from zero).
  static_assert(ax::to_q<10, 4>(detail::exact_of(rational{1, 3})) == W4{341});
  static_assert(ax::to_q<10, 4>(detail::exact_of(rational{-2, 3})) == W4{-683});
  // 5/2 at scale 2^-1: 1.25 → 1.
  static_assert(ax::to_q<-1, 4>(detail::exact_of(rational{5, 2})) == W4{1});
  static_assert(ax::floor_log2(detail::exact_of(rational{1, 3})) == -2);
  static_assert(ax::floor_log2(detail::exact_of(rational{8})) == 3);
  static_assert(ax::floor_log2(detail::exact_of(rational{-9, 1})) == 3);
  static_assert(ax::floor_log2(detail::exact_of(rational{1, 1024})) == -10);
  // Products and quotients at scale 2^8: 1.5·2.25 = 3.375, 1/3 = 0.33…
  static_assert(ax::mul_q(W4{384}, W4{576}, 8) == W4{864});
  static_assert(ax::div_q(W4{256}, W4{768}, 8) == W4{85});
  static_assert(ax::round_shift(W4{-6}, 2) == W4{-2});    // −1.5 → −2
  static_assert(ax::round_shift(W4{5}, 1) == W4{3});      // 2.5 → 3
}

TEST(MathAdaptiveTest, output_precision)
{
  static_assert(ax::out_bits<eighths> == 5);     // conservative: 2^-5 ≤ 1/16
  static_assert(ax::out_bits<sevenths> == 3);    // 2^-3 ≤ 3/14
  static_assert(ax::mag_bits<eighths> >= 1);
}

TEST(MathAdaptiveTest, decide_rounds_when_the_interval_is_inside_one_slot)
{
  // 0.3 ± 2^-20 → nearest eighth is 2/8.
  constexpr auto a = approx_of<30>(rational{3, 10}, 1024);
  constexpr auto d = ax::decide<eighths>(a);
  static_assert(d.Decided);
  EXPECT_EQ(ax::store<eighths>(d.Index), eighths{0.25});
  // Floor and ceil of 0.3: 2/8 and 3/8.
  EXPECT_EQ(ax::store<eighths_floor>(ax::decide<eighths_floor>(a).Index), eighths_floor{0.25});
  EXPECT_EQ(ax::store<eighths_ceil>(ax::decide<eighths_ceil>(a).Index), eighths_ceil{0.375});
  // 0.5 on multiples of 3/7: nearest is 3/7.
  constexpr auto s = ax::decide<sevenths>(approx_of<30>(rational{1, 2}, 4));
  static_assert(s.Decided);
  EXPECT_EQ(ax::store<sevenths>(s.Index), (sevenths{rational{3, 7}}));
}

TEST(MathAdaptiveTest, decide_refuses_an_interval_across_a_boundary)
{
  // 1/16 is a tie between 0 and 1/8: any error straddles it.
  static_assert(!ax::decide<eighths>(approx_of<30>(rational{1, 16}, 1)).Decided);
  // Exact (error 0) ties round by the mode: half away from zero → 1/8.
  constexpr auto t = ax::decide<eighths>(approx_of<30>(rational{1, 16}, 0));
  static_assert(t.Decided);
  EXPECT_EQ(ax::store<eighths>(t.Index), eighths{0.125});
  // For floor, the boundary is the grid point itself.
  static_assert(!ax::decide<eighths_floor>(approx_of<30>(rational{1, 8}, 1)).Decided);
  static_assert(ax::decide<eighths_floor>(approx_of<30>(rational{1, 16}, 1)).Decided);
}

TEST(MathAdaptiveTest, store_out_of_range_runs_the_policy)
{
  // 1.3 rounds to 10/8, past Upper: a clamp grid saturates.
  constexpr auto d = ax::decide<eighths_clamp>(approx_of<30>(rational{13, 10}, 1));
  static_assert(d.Decided);
  EXPECT_EQ(ax::store<eighths_clamp>(d.Index), eighths_clamp{1});
  // The default checked policy reports it.
  EXPECT_ANY_THROW((void)ax::store<eighths>(ax::decide<eighths>(approx_of<30>(rational{13, 10}, 1)).Index));
}

namespace
{
  // A core whose error shrinks with W: 1/16 + 2^-40 approximated to ±2^-(W−2).
  // At W ≤ 40 the interval contains the tie 1/16; past it the result decides.
  struct near_tie
  {
    mutable int Runs = 0;
    template <int W>
    constexpr ax::approx<8> run() const
    {
      ++Runs;
      const auto v = detail::exact_of<8>(rational{1, 16}) + exact_frac<8>{wide_sint<8>{1}, wide_sint<8>{1} << 40};
      return {ax::to_q<W, 8>(v), W, 4};
    }
  };
}

TEST(MathAdaptiveTest, evaluate_escalates_until_decided)
{
  near_tie core;
  const auto r = ax::evaluate<eighths, 24>(core);
  EXPECT_EQ(r, eighths{0.125});
  EXPECT_EQ(core.Runs, 2);                         // undecided at W = 24, decided at 48
}

//---------------------------------------------------------------------------
// Correct rounding against long double <cmath>: every sampled result must be
// the nearest grid point (or the floor point, for round_floor). Results within
// a hair of a rounding boundary are skipped (long double cannot tell).
//---------------------------------------------------------------------------
#include <beman/inside/cmath_adaptive.hpp>

#include <cmath>
#include <functional>

namespace
{
  namespace am = beman::inside::math::adaptive;
  using ld = long double;

  template <typename Out>
  ld notch_ld() { return static_cast<ld>(static_cast<double>(detail::to_rational(notch_of<Out>))); }

  // Checks fn(x) == the rounding of ref(x) onto Out for every In slot in
  // [Lo, Hi] (stepping `stride` slots); returns the number of mismatches.
  template <typename Out, typename In, typename Fn, typename Ref>
  int check_rounding(Fn fn, Ref ref, int stride = 1)
  {
    const ld n = notch_ld<Out>();
    constexpr bool floor_mode = detail::rounding_of(policy_of<Out>) == detail::round_mode::floor;
    int bad = 0;
    const auto count = static_cast<long long>(grid_of<In>.slot_count());
    for (long long i = 0; i <= count; i += stride)
    {
      const In x = In::from_raw(detail::raw_from_offset<In>(static_cast<umax>(i)));
      const ld xv = static_cast<ld>(static_cast<double>(x));
      const ld want = ref(xv);
      const ld got = static_cast<ld>(static_cast<double>(fn(x)));
      const ld tol = 1e-15L * (std::fabs(want) > 1 ? std::fabs(want) : 1);
      ld k = want / n;
      if (floor_mode)
      {
        const ld frac = k - std::floor(k);
        if (frac * n < tol || (1 - frac) * n < tol) continue;   // too close to a grid point
        if (!(got <= want && want < got + n)) { if (bad++ < 5) ADD_FAILURE() << "x=" << static_cast<double>(xv) << " got " << static_cast<double>(got) << " want floor of " << static_cast<double>(want); }
      }
      else
      {
        const ld frac = k - std::floor(k);
        if (std::fabs(frac - 0.5L) * n < tol) continue;          // too close to a tie
        if (!(std::fabs(got - want) <= n / 2)) { if (bad++ < 5) ADD_FAILURE() << "x=" << static_cast<double>(xv) << " got " << static_cast<double>(got) << " want " << static_cast<double>(want); }
      }
    }
    return bad;
  }

  using sym4   = inside<{{-4, 4}, rational{1, 512}}, round_nearest>;
  using pos64  = inside<{{rational{1, 64}, 64}, rational{1, 64}}, round_nearest>;
  using unit   = inside<{{-1, 1}, rational{1, 1024}}, round_nearest>;
  using open1  = inside<{{rational{-1023, 1024}, rational{1023, 1024}}, rational{1, 1024}}, round_nearest>;
  using ge1    = inside<{{1, 64}, rational{1, 64}}, round_nearest>;
  using tan_in = inside<{{rational{-3, 2}, rational{3, 2}}, rational{1, 512}}, round_nearest>;
  using out20  = inside<{{-64, 64}, rational{1, 1 << 20}}, round_nearest>;
  using out20f = inside<{{-64, 64}, rational{1, 1 << 20}}, round_floor>;
  using outdec = inside<{{-64, 64}, rational{1, 1000}}, round_nearest>;
  using out40  = inside<{{-64, 64}, rational{1, umax{1} << 40}}, round_nearest>;
  using wide20 = inside<{{-1024, 1024}, rational{1, 1 << 20}}, round_nearest>;
}

TEST(MathAdaptiveTest, exp_family_rounds_correctly)
{
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::exp_into<out20>(x); }, [](ld v) { return std::exp(v); })), 0);
  EXPECT_EQ((check_rounding<out20f, sym4>([](sym4 x) { return am::exp_into<out20f>(x); }, [](ld v) { return std::exp(v); })), 0);
  EXPECT_EQ((check_rounding<outdec, sym4>([](sym4 x) { return am::exp_into<outdec>(x); }, [](ld v) { return std::exp(v); })), 0);
  EXPECT_EQ((check_rounding<out40, sym4>([](sym4 x) { return am::exp_into<out40>(x); }, [](ld v) { return std::exp(v); }, 7)), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::exp2_into<out20>(x); }, [](ld v) { return std::exp2(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::sinh_into<out20>(x); }, [](ld v) { return std::sinh(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::cosh_into<out20>(x); }, [](ld v) { return std::cosh(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::tanh_into<out20>(x); }, [](ld v) { return std::tanh(v); })), 0);
}

TEST(MathAdaptiveTest, log_family_rounds_correctly)
{
  EXPECT_EQ((check_rounding<out20, pos64>([](pos64 x) { return am::log_into<out20>(x); }, [](ld v) { return std::log(v); })), 0);
  EXPECT_EQ((check_rounding<out20f, pos64>([](pos64 x) { return am::log_into<out20f>(x); }, [](ld v) { return std::log(v); })), 0);
  EXPECT_EQ((check_rounding<out20, pos64>([](pos64 x) { return am::log2_into<out20>(x); }, [](ld v) { return std::log2(v); })), 0);
  EXPECT_EQ((check_rounding<out20, pos64>([](pos64 x) { return am::log10_into<out20>(x); }, [](ld v) { return std::log10(v); })), 0);
  EXPECT_EQ((check_rounding<out40, pos64>([](pos64 x) { return am::log_into<out40>(x); }, [](ld v) { return std::log(v); }, 3)), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::asinh_into<out20>(x); }, [](ld v) { return std::asinh(v); })), 0);
  EXPECT_EQ((check_rounding<out20, ge1>([](ge1 x) { return am::acosh_into<out20>(x); }, [](ld v) { return std::acosh(v); })), 0);
  EXPECT_EQ((check_rounding<out20, open1>([](open1 x) { return am::atanh_into<out20>(x); }, [](ld v) { return std::atanh(v); })), 0);
}

TEST(MathAdaptiveTest, roots_round_correctly)
{
  EXPECT_EQ((check_rounding<out20, pos64>([](pos64 x) { return am::sqrt_into<out20>(x); }, [](ld v) { return std::sqrt(v); })), 0);
  EXPECT_EQ((check_rounding<out20f, pos64>([](pos64 x) { return am::sqrt_into<out20f>(x); }, [](ld v) { return std::sqrt(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::cbrt_into<out20>(x); }, [](ld v) { return std::cbrt(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::sqrt_into<out20>(x).value_or(out20{0}); }, [](ld v) { return v < 0 ? 0 : std::sqrt(v); }, 3)), 0);
  EXPECT_EQ(am::sqrt_into<out20>(sym4{-1}).error(), errc::domain_error);
  // Exact roots of rationals, including ties: √(1/4) = 1/2 exactly.
  using quarters = inside<{{0, 4}, rational{1, 4}}, round_nearest>;
  using halves   = inside<{{0, 4}, rational{1, 2}}, round_floor>;
  EXPECT_EQ(am::sqrt_into<halves>(quarters{0.25}), halves{0.5});
  EXPECT_EQ(am::sqrt_into<halves>(quarters{2.25}), halves{1.5});
}

TEST(MathAdaptiveTest, trig_rounds_correctly)
{
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::sin_into<out20>(x); }, [](ld v) { return std::sin(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::cos_into<out20>(x); }, [](ld v) { return std::cos(v); })), 0);
  EXPECT_EQ((check_rounding<out20f, sym4>([](sym4 x) { return am::cos_into<out20f>(x); }, [](ld v) { return std::cos(v); })), 0);
  EXPECT_EQ((check_rounding<out40, sym4>([](sym4 x) { return am::sin_into<out40>(x); }, [](ld v) { return std::sin(v); }, 5)), 0);
  EXPECT_EQ((check_rounding<wide20, tan_in>([](tan_in x) { return *am::tan_into<wide20>(x); }, [](ld v) { return std::tan(v); })), 0);
  EXPECT_EQ((check_rounding<out20, sym4>([](sym4 x) { return am::atan_into<out20>(x); }, [](ld v) { return std::atan(v); })), 0);
  EXPECT_EQ((check_rounding<out20, unit>([](unit x) { return am::asin_into<out20>(x); }, [](ld v) { return std::asin(v); })), 0);
  EXPECT_EQ((check_rounding<out20, unit>([](unit x) { return am::acos_into<out20>(x); }, [](ld v) { return std::acos(v); })), 0);
}

TEST(MathAdaptiveTest, two_argument_functions_round_correctly)
{
  const ld n = notch_ld<out20>();
  int bad = 0;
  for (int i = -32; i <= 32; ++i)
    for (int j = -32; j <= 32; ++j)
    {
      const sym4 y{i / 8.0}, x{j / 8.0};
      const ld a = std::atan2(static_cast<ld>(i) / 8, static_cast<ld>(j) / 8);
      const ld h = std::hypot(static_cast<ld>(i) / 8, static_cast<ld>(j) / 8);
      if (std::fabs(static_cast<double>(am::atan2_into<out20>(y, x)) - a) > n / 2) ++bad;
      if (std::fabs(static_cast<double>(am::hypot_into<out20>(x, y)) - h) > n / 2) ++bad;
    }
  EXPECT_EQ(bad, 0);
  using base = inside<{{rational{1, 16}, 8}, rational{1, 16}}, round_nearest>;
  using expo = inside<{{-3, 3}, rational{1, 8}}, round_nearest>;
  for (int i = 1; i <= 128; i += 3)
    for (int j = -24; j <= 24; ++j)
    {
      const ld want = std::pow(static_cast<ld>(i) / 16, static_cast<ld>(j) / 8);
      const auto r = am::pow_into<wide20>(base{i / 16.0}, expo{j / 8.0});
      if (want > 1024.5L) { EXPECT_FALSE(r.has_value()); continue; }
      ASSERT_TRUE(r.has_value()) << i << " " << j;
      const ld frac = want / n - std::floor(want / n);
      if (std::fabs(frac - 0.5L) * n < 1e-15L * want) continue;
      if (std::fabs(static_cast<double>(*r) - want) > n / 2) ++bad;
    }
  EXPECT_EQ(bad, 0);
  // Exact powers, including ties on the output grid.
  using halves = inside<{{0, 64}, rational{1, 2}}, round_nearest>;
  EXPECT_EQ(*am::pow_into<halves>(base{1.5}, expo{2}), halves{2.5});    // 2.25 → 2.5 (half away)
  EXPECT_EQ((am::pow_base_into<out20, 10>(expo{1})), out20{10});
  EXPECT_EQ((am::pow_base_into<out20, 2>(expo{-3})), out20{0.125});
  EXPECT_EQ((am::pow_base_into<out20, 10>(expo{-1})), (out20{rational{1, 10}}));   // 1/10 rounded
  EXPECT_FALSE((am::pow_into<out20>(base{8}, expo{3})).has_value());              // 512 is past 64
}

//---------------------------------------------------------------------------
// Grids past 64 bits (C++26): fine notches and huge inputs. The expected raws
// come from an exact integer computation at 600 bits.
//---------------------------------------------------------------------------
#if BEMAN_INSIDE_BIG_GRIDS
namespace
{
  using detail::big_int;
  using detail::grid_rational;

  consteval big_int pow2(int k) { return big_int{1} << k; }
  consteval big_int from_limbs(std::initializer_list<umax> hi_to_lo)
  {
    big_int v{0};
    for (umax l : hi_to_lo) v = (v << 64) + big_int{l};
    return v;
  }

  using fine_in  = inside<{{0, 1}, grid_rational{big_int{1}, pow2(80)}}, round_nearest>;
  using fine_out = inside<{{-1, 1}, grid_rational{big_int{1}, pow2(100)}}, round_nearest>;
  using fine_pos = inside<{{0, 4}, grid_rational{big_int{1}, pow2(100)}}, round_nearest>;
  using huge_in  = inside<{1, grid_rational{pow2(300)}}, round_nearest>;
  using log_out  = inside<{{0, 256}, grid_rational{big_int{1}, pow2(64)}}, round_nearest>;
  using root_out = inside<{0, grid_rational{pow2(150)}}, round_floor>;

  // x = ⌊2^80/3⌋·2^-80.
  constexpr fine_in third = fine_in::from_raw(static_cast<fine_in::raw_type>(pow2(80) / big_int{3}));
}

TEST(MathAdaptiveTest, fine_grids_past_64_bits)
{
  constexpr auto sin_raw = static_cast<fine_out::raw_type>(from_limbs({0x153c3081a2ULL, 0xa031ab144c43424bULL}));
  constexpr auto exp_raw = static_cast<fine_pos::raw_type>(from_limbs({0x16546db1baULL, 0x2d1310a7f6bf8106ULL}));
  EXPECT_EQ(am::sin_into<fine_out>(third).raw(), sin_raw);
  EXPECT_EQ(am::exp_into<fine_pos>(third).raw(), exp_raw);
}

TEST(MathAdaptiveTest, huge_inputs_past_64_bits)
{
  // log(2^299 + 12345) on a 2^-64 grid.
  constexpr huge_in x = huge_in::from_raw(static_cast<huge_in::raw_type>(pow2(299) + big_int{12344}));
  constexpr auto log_raw = static_cast<log_out::raw_type>(from_limbs({0xcfULL, 0x4041fe720d531ba5ULL}));
  EXPECT_EQ(am::log_into<log_out>(x).raw(), log_raw);
  // ⌊√(2^300 − 1)⌋ = 2^150 − 1.
  constexpr huge_in y = huge_in::from_raw(static_cast<huge_in::raw_type>(pow2(300) - big_int{2}));
  constexpr auto root_raw = static_cast<root_out::raw_type>(pow2(150) - big_int{1});
  EXPECT_EQ(am::sqrt_into<root_out>(y).raw(), root_raw);
  // log2 of a power of two is exact: 2^299 → 299.
  constexpr huge_in p = huge_in::from_raw(static_cast<huge_in::raw_type>(pow2(299) - big_int{1}));
  using log2_out = inside<{{0, 512}, grid_rational{big_int{1}, pow2(64)}}, round_nearest>;
  EXPECT_EQ(am::log2_into<log2_out>(p), log2_out{299});
}

TEST(MathAdaptiveTest, deduced_outputs_past_64_bits)
{
  // log over [1, 2^300] in steps of 1: [0, ⌈300·ln 2⌉] = [0, 208].
  using L = decltype(am::log(huge_in{1}));
  static_assert(lower_of<L> == 0 && upper_of<L> == 208);
  // sqrt over [1, 2^300]: exact endpoints, [1, 2^150].
  using R = decltype(am::sqrt(huge_in{1}));
  static_assert(lower_of<R> == 1 && upper_of<R> == grid_rational{pow2(150)});
  // sin on a 2^-80 grid keeps the notch: a wide output.
  using S = decltype(am::sin(third));
  static_assert(notch_of<S> == notch_of<fine_in>);
  EXPECT_EQ(am::sin(third), am::sin_into<S>(third));
}
#endif

TEST(MathAdaptiveTest, constant_evaluation)
{
  // The engine is integer-only, so the same results are available at compile time.
  using quarters = inside<{{0, 4}, rational{1, 4}}, round_nearest>;
  using halves   = inside<{{0, 4}, rational{1, 2}}, round_floor>;
  static_assert(am::sqrt_into<halves>(quarters{2.25}) == halves{1.5});
  static_assert(am::exp_into<out20>(sym4{1}).raw() == out20{rational{2850325, 1 << 20}}.raw());   // e·2^20 = 2850325.0…
  static_assert(am::sin_into<out20>(sym4{0}) == out20{0});
  SUCCEED();
}

TEST(MathAdaptiveTest, deduced_outputs)
{
  // exp over [-4, 4] in 1/512: [⌊e^-4·512⌋, ⌈e^4·512⌉]/512.
  using E = decltype(am::exp(sym4{0}));
  static_assert(lower_of<E> == rational{9, 512} && upper_of<E> == rational{27955, 512});
  static_assert(notch_of<E> == rational{1, 512});
  EXPECT_EQ(am::exp(sym4{1}), am::exp_into<E>(sym4{1}));
  // acos over [-1, 1]: [0, ⌈π·1024⌉/1024], decreasing; acos(1) = 0 exactly.
  using A = decltype(am::acos(unit{0}));
  static_assert(lower_of<A> == 0 && upper_of<A> == rational{3217, 1024});
  // sqrt over [1/64, 64]: exact endpoints 1/8 and 8.
  using R = decltype(am::sqrt(pos64{1}));
  static_assert(lower_of<R> == rational{1, 8} && upper_of<R> == 8);
  // cosh over [-4, 4]: least value 1 at 0.
  using C = decltype(am::cosh(sym4{0}));
  static_assert(lower_of<C> == 1 && upper_of<C> == rational{13982, 512});
  // sin is [-1, 1]; atan2 is [-π, π] rounded out.
  static_assert(lower_of<decltype(am::sin(sym4{0}))> == -1);
  using T = decltype(am::atan2(out20{0}, out20{1}));
  static_assert(upper_of<T> == rational{3294199, 1 << 20} && lower_of<T> == -rational{3294199, 1 << 20});
  EXPECT_EQ(am::atan2(out20{1}, out20{-1}), (am::atan2_into<T>(out20{1}, out20{-1})));
  // Expected-returning forms.
  EXPECT_TRUE(am::tan(sym4{1}).has_value());
  EXPECT_EQ(am::sqrt(sym4{-1}).error(), errc::domain_error);
  using base = inside<{{rational{1, 2}, 4}, rational{1, 16}}, round_nearest>;
  using expo = inside<{{-2, 2}, rational{1, 4}}, round_nearest>;
  using P = decltype(am::pow(base{1}, expo{1}))::value_type;
  static_assert(lower_of<P> == rational{1, 16} && upper_of<P> == 16);   // 4^-2 and 4^2 (1/2)^±2 inside
  EXPECT_EQ(*am::pow(base{2}, expo{2}), P{4});
  EXPECT_EQ((am::pow_base<10>(expo{1})), (am::pow_base_into<decltype(am::pow_base<10>(expo{1})), 10>(expo{1})));
}

//---------------------------------------------------------------------------
// The double tier agrees with the integer path on every input slot: the
// _into forms (which try the double kernels first where an FPU is present)
// against the integer cores run through the driver directly.
//---------------------------------------------------------------------------
namespace
{
  template <typename Out, typename In, typename Core, typename Fn>
  int tier_mismatches(Fn fn)
  {
    int bad = 0;
    const auto count = static_cast<long long>(grid_of<In>.slot_count());
    for (long long i = 0; i <= count; ++i)
    {
      const In x = In::from_raw(detail::raw_from_offset<In>(static_cast<umax>(i)));
      const Out integer = ax::evaluate<Out, ax::start_bits<Out>>(Core{ax::exact_input(x)});
      if (!(fn(x).raw() == integer.raw()) && bad++ < 3)
        ADD_FAILURE() << "x = " << static_cast<double>(x);
    }
    return bad;
  }

  using out8   = inside<{{-64, 64}, rational{1, 8}}, round_nearest>;
  using out8f  = inside<{{-64, 64}, rational{1, 8}}, round_floor>;
  using out16c = inside<{{-64, 64}, rational{1, 1 << 16}}, round_ceil>;
}

#define TIER_CHECK(fn, Out, In, ...) \
  EXPECT_EQ((tier_mismatches<Out, In, __VA_ARGS__>([](In x) { return am::fn##_into<Out>(x); })), 0) << #fn " " #Out

TEST(MathAdaptiveTest, double_tier_agrees_with_the_integer_path)
{
  TIER_CHECK(sin,   out20,  sym4, ax::trig_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::trig::sin, 1>);
  TIER_CHECK(sin,   out8f,  sym4, ax::trig_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::trig::sin, 1>);
  TIER_CHECK(cos,   outdec, sym4, ax::trig_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::trig::cos, 1>);
  TIER_CHECK(cos,   out16c, sym4, ax::trig_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::trig::cos, 1>);
  TIER_CHECK(exp,   out20,  sym4, ax::exp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::out_kmax<out20>>);
  TIER_CHECK(exp,   out8,   sym4, ax::exp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::out_kmax<out8>>);
  TIER_CHECK(exp2,  out16c, sym4, ax::exp2_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::out_kmax<out16c>>);
  TIER_CHECK(sinh,  out20,  sym4, ax::hyp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::hyp::sinh, ax::out_kmax<out20>>);
  TIER_CHECK(cosh,  out8f,  sym4, ax::hyp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::hyp::cosh, ax::out_kmax<out8f>>);
  TIER_CHECK(tanh,  outdec, sym4, ax::hyp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::hyp::tanh, 1>);
  TIER_CHECK(atan,  out20,  sym4, ax::atan_core<ax::input_limbs<sym4>>);
  TIER_CHECK(asinh, out16c, sym4, ax::ahyp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::ahyp::asinh>);
  TIER_CHECK(cbrt,  out20,  sym4, ax::cbrt_core<ax::input_limbs<sym4>>);
  TIER_CHECK(log,   out20,  pos64, ax::log_core<ax::input_limbs<pos64>>);
  TIER_CHECK(log2,  out8f,  pos64, ax::logb_core<ax::input_limbs<pos64>, 2>);
  TIER_CHECK(log10, outdec, pos64, ax::logb_core<ax::input_limbs<pos64>, 10>);
  TIER_CHECK(sqrt,  out16c, pos64, ax::sqrt_core<ax::input_limbs<pos64>>);
  TIER_CHECK(sqrt,  out8,   pos64, ax::sqrt_core<ax::input_limbs<pos64>>);
  TIER_CHECK(asin,  out20,  unit, ax::asin_core<ax::input_limbs<unit>>);
  TIER_CHECK(acos,  out16c, unit, ax::acos_core<ax::input_limbs<unit>>);
  TIER_CHECK(atanh, out20,  open1, ax::ahyp_core<ax::input_limbs<open1>, ax::in_mag<open1>, ax::ahyp::atanh>);
  // Floating-point outputs: the slot decided in double arithmetic.
  using f64out  = inside<{{-64, 64}, per<16384>}, round_nearest | f64>;
  using f64outf = inside<{{-64, 64}, per<16384>}, round_floor | f64>;
  TIER_CHECK(sin,   f64out,  sym4,   ax::trig_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::trig::sin, 1>);
  TIER_CHECK(exp,   f64outf, sym4,   ax::exp_core<ax::input_limbs<sym4>, ax::in_mag<sym4>, ax::out_kmax<f64outf>>);
  TIER_CHECK(atan,  f64out,  sym4,   ax::atan_core<ax::input_limbs<sym4>>);
  TIER_CHECK(log,   f64outf, pos64,  ax::log_core<ax::input_limbs<pos64>>);
}
#undef TIER_CHECK

TEST(MathAdaptiveTest, tables_agree_with_the_integer_path)
{
  // 256 slots: within the default table size.
  using s8 = inside<{{rational{-128, 64}, rational{127, 64}}, rational{1, 64}}, round_nearest>;
  using p8 = inside<{{rational{1, 64}, 4}, rational{1, 64}}, round_nearest>;
  static_assert(ax::table_input<s8> && ax::table_output<out20>);
  EXPECT_EQ((tier_mismatches<out20, s8, ax::trig_core<ax::input_limbs<s8>, ax::in_mag<s8>, ax::trig::sin, 1>>([](s8 x) { return am::sin_into<out20>(x); })), 0);
  EXPECT_EQ((tier_mismatches<out40, s8, ax::exp_core<ax::input_limbs<s8>, ax::in_mag<s8>, ax::out_kmax<out40>>>([](s8 x) { return am::exp_into<out40>(x); })), 0);
  EXPECT_EQ((tier_mismatches<outdec, p8, ax::log_core<ax::input_limbs<p8>>>([](p8 x) { return am::log_into<outdec>(x); })), 0);
  // Results past Out's range keep the computed path, and its policy.
  using small_clamp = inside<{{0, 4}, rational{1, 64}}, round_nearest | clamp>;
  using table = ax::result_table<small_clamp, s8, ax::start_bits<small_clamp>,
      [](s8 v) { return ax::exp_core<ax::input_limbs<s8>, ax::in_mag<s8>, ax::out_kmax<small_clamp>>{ax::exact_input(v)}; }>;
  static_assert(!table::Table.Valid);                       // e^2 > 4
  EXPECT_EQ(am::exp_into<small_clamp>(s8{1.984375}), small_clamp{4});
  EXPECT_EQ(am::exp_into<small_clamp>(s8{1}), (small_clamp{rational{174, 64}}));   // e·64 = 173.97…
}

TEST(MathAdaptiveTest, double_tier_agrees_on_decimal_inputs_and_checked_forms)
{
  // Inputs that are not doubles exactly (steps of 1/1000): the tier adds the
  // conversion's rounding to its bound.
  using symd = inside<{{-4, 4}, rational{1, 1000}}, round_nearest>;
  using posd = inside<{{rational{1, 1000}, 8}, rational{1, 1000}}, round_nearest>;
  using unid = inside<{{-1, 1}, rational{1, 1000}}, round_nearest>;
  using ge1d = inside<{{1, 8}, rational{1, 1000}}, round_nearest>;
  using tanout = inside<{{-1024, 1024}, rational{1, 1 << 20}}, round_nearest>;
  static_assert(!ax::fp_exact_input<symd> && (ax::fp_tier<out20, symd> || !ax::fp_tier_available));
  EXPECT_EQ((tier_mismatches<out20, symd, ax::trig_core<ax::input_limbs<symd>, ax::in_mag<symd>, ax::trig::sin, 1>>([](symd x) { return am::sin_into<out20>(x); })), 0);
  EXPECT_EQ((tier_mismatches<outdec, symd, ax::exp_core<ax::input_limbs<symd>, ax::in_mag<symd>, ax::out_kmax<outdec>>>([](symd x) { return am::exp_into<outdec>(x); })), 0);
  EXPECT_EQ((tier_mismatches<out20, symd, ax::cbrt_core<ax::input_limbs<symd>>>([](symd x) { return am::cbrt_into<out20>(x); })), 0);
  EXPECT_EQ((tier_mismatches<out16c, posd, ax::log_core<ax::input_limbs<posd>>>([](posd x) { return am::log_into<out16c>(x); })), 0);
  EXPECT_EQ((tier_mismatches<out20, unid, ax::asin_core<ax::input_limbs<unid>>>([](unid x) { return am::asin_into<out20>(x); })), 0);
  EXPECT_EQ((tier_mismatches<out20, ge1d, ax::ahyp_core<ax::input_limbs<ge1d>, ax::in_mag<ge1d>, ax::ahyp::acosh>>([](ge1d x) { return am::acosh_into<out20>(x); })), 0);
  EXPECT_EQ((tier_mismatches<out8f, ge1, ax::ahyp_core<ax::input_limbs<ge1>, ax::in_mag<ge1>, ax::ahyp::acosh>>([](ge1 x) { return am::acosh_into<out8f>(x); })), 0);
  EXPECT_EQ((tier_mismatches<tanout, tan_in, ax::trig_core<ax::input_limbs<tan_in>, ax::in_mag<tan_in>, ax::trig::tan, ax::out_kmax<tanout>>>([](tan_in x) { return *am::tan_into<tanout>(x); })), 0);

  // Two inputs: atan2, hypot and pow against their cores on a grid of pairs.
  using base = inside<{{rational{1, 16}, 8}, rational{1, 16}}, round_nearest>;
  using expo = inside<{{-3, 3}, rational{1, 100}}, round_nearest>;
  int bad = 0;
  for (int i = -40; i <= 40; i += 3)
    for (int j = -40; j <= 40; j += 3)
    {
      const symd y{i / 10.0}, x{j / 10.0};
      const auto a = am::atan2_into<out20>(y, x);
      const auto a2 = ax::evaluate<out20, ax::start_bits<out20>>(ax::atan2_core<ax::input_limbs<symd>>{ax::exact_input(y), ax::exact_input(x)});
      if (a.raw() != a2.raw()) ++bad;
      using F = detail::exact_frac<2 * ax::input_limbs<symd> + 1>;
      const F fx{ax::exact_input(x)}, fy{ax::exact_input(y)};
      const auto h2 = ax::evaluate<out20, ax::start_bits<out20>>(ax::sqrt_core<2 * ax::input_limbs<symd> + 1>{fx * fx + fy * fy});
      if (am::hypot_into<out20>(x, y).raw() != h2.raw()) ++bad;
    }
  for (int i = 1; i <= 128; i += 5)
    for (int j = -300; j <= 300; j += 13)
    {
      const base b{i / 16.0};
      const expo e{rational{j, 100}};
      const auto r = am::pow_into<tanout>(b, e);
      using core = ax::pow_core<ax::input_limbs<base>, ax::input_limbs<expo>, ax::in_mag<expo>, ax::out_kmax<tanout>>;
      const auto r2 = ax::evaluate_checked<tanout, ax::start_bits<tanout>>(core{ax::exact_input(b), ax::exact_input(e)});
      if (r.has_value() != r2.has_value() || (r && r->raw() != r2->raw())) ++bad;
    }
  // pow_base<10> over a decimal exponent.
  for (int j = -300; j <= 300; ++j)
  {
    const expo e{rational{j, 100}};
    using core = ax::pow_core<2, ax::input_limbs<expo>, ax::in_mag<expo>, ax::out_kmax<tanout>>;
    const auto r2 = ax::evaluate<tanout, ax::start_bits<tanout>>(core{ax::exact_int<2>(10), ax::exact_input(e)});
    if ((am::pow_base_into<tanout, 10>(e)).raw() != r2.raw()) ++bad;
  }
  EXPECT_EQ(bad, 0);
}
