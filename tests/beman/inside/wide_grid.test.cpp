// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids with more than 2^64 slots store a wide_int index instead of falling
// back to a rational: construction, assignment under every policy, comparison,
// arithmetic, conversions, io, hashing and sampling.

#include <beman/inside/inside.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/random.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

using namespace beman::inside;

namespace {
constexpr umax p32 = umax{1} << 32;
constexpr umax p34 = umax{1} << 34;

// {0, 2^34} in steps of 2^-32: 2^66 + 1 slots, a 67-bit index.
using fine         = inside<{{0, p34}, per<p32>}>;
using fine_nearest = inside<{{0, p34}, per<p32>}, round_nearest>;
using fine_clamp   = inside<{{0, p34}, per<p32>}, clamp>;
using fine_wrap    = inside<{{0, p34}, per<p32>}, wrap>;
// Signed and offset: {−2^34, 2^34} in steps of 2^-32, Lower ≠ 0.
using fine_signed = inside<{{-rational{p34}, rational{p34}}, per<p32>}>;

constexpr rational tick{umax{1}, static_cast<imax>(p32)}; // 2^-32
} // namespace

TEST(WideGridTest, deduces_wide_index) {
    static_assert(std::is_same_v<fine::raw_type, detail::wide_uint<2>>);
    static_assert(std::is_same_v<fine_signed::raw_type, detail::wide_uint<2>>);
    static_assert(sizeof(fine) == 16);
    static_assert(detail::wide_index_storage<fine> && detail::index_storage<fine>);
    static_assert(grid_of<fine>.slot_bits() == 67);
    // `indexed` sizes from the same slot count.
    static_assert(std::is_same_v<inside<{{0, p34}, per<p32>}, indexed>::raw_type, detail::wide_uint<2>>);
    // A 64-bit rational cannot hold every value: no implicit conversion.
    static_assert(!std::is_convertible_v<fine, rational>);
    static_assert(!std::is_convertible_v<fine, imax>);
}

TEST(WideGridTest, construction_from_scalars) {
    fine a = 5;
    EXPECT_TRUE(a == 5);
    EXPECT_TRUE(a.raw() == detail::wide_uint<2>{5 * p32});
    fine b = 0.5;
    EXPECT_TRUE(b == 0.5);
    fine c = tick; // the first notch
    EXPECT_TRUE(c == tick);
    EXPECT_TRUE(c.raw() == detail::wide_uint<2>{1});
    fine top = p34;
    EXPECT_TRUE(top == p34);
    EXPECT_TRUE(top.raw() == (detail::wide_uint<2>{1} << 66));

    fine_signed s = -3.25;
    EXPECT_TRUE(s == -3.25);
    EXPECT_TRUE(s < 0);
}

TEST(WideGridTest, assignment_policies) {
    // checked: off-lattice and out-of-range report.
    fine a = 1;
    EXPECT_THROW((a = rational{1, imax{1} << 33}), inside_error); // half a notch
    EXPECT_THROW(a = p34 + 1, inside_error);
    EXPECT_THROW(a = -1, inside_error);
    EXPECT_TRUE(a == 1); // unchanged

    // rounding: half a notch rounds away from zero.
    fine_nearest n = 0;
    n              = rational{1, imax{1} << 33};
    EXPECT_TRUE(n == tick);

    // clamp and wrap.
    fine_clamp c = 0;
    c            = p34 + 7;
    EXPECT_TRUE(c == p34);
    c = -2.0;
    EXPECT_TRUE(c == 0);
    fine_wrap w = 0;
    w           = p34 + 1; // one past the top: 2^32 notches past it
    EXPECT_TRUE(w == (rational{1} - tick).value());

    // errc-reporting policy.
    errc e{};
    fine x      = 3;
    x.policy(e) = p34 * 2;
    EXPECT_EQ(e, errc::overflow);
    EXPECT_TRUE(x == 3);

    // non-finite doubles.
    EXPECT_THROW(x = std::numeric_limits<double>::infinity(), inside_error);
    c = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(c == p34);
}

TEST(WideGridTest, assignment_between_insides) {
    fine a = inside<{0, 100}>{42};
    EXPECT_TRUE(a == 42);
    // wide → narrow: the narrow grid's notch is coarser, so it rounds under snap.
    fine                            b = 41.75;
    inside<{0, 100}, round_nearest> n = 0;
    n                                 = b;
    EXPECT_EQ(static_cast<imax>(n), 42);
    // wide → wide of another lattice.
    fine_signed s = b;
    EXPECT_TRUE(s == 41.75);
}

TEST(WideGridTest, comparisons) {
    fine a = 1.5, b = 2;
    EXPECT_TRUE(a < b && b > a && a != b);
    EXPECT_TRUE((a == 1.5 && a == rational{3, 2}));
    EXPECT_TRUE(a > 1 && a < 2u && a < 1e30 && a > -1e30);
    EXPECT_TRUE((a == inside<{{0, 10}, per<2>}>{1.5}));
    EXPECT_TRUE((a < inside<{0, 10}>{2}));
    fine_signed s = -1.5;
    EXPECT_TRUE(s < a);
    EXPECT_TRUE(-a == s);
    EXPECT_EQ(a <=> b, std::strong_ordering::less);
}

TEST(WideGridTest, arithmetic) {
    fine a = 1.5, b = rational{1} + tick;
    auto sum = a + b; // {0, 2^35}, notch 2^-32
    static_assert(detail::wide_index_storage<decltype(sum)>);
    EXPECT_TRUE(sum == (rational{5, 2} + tick).value());
    auto diff = a - b; // {−2^34, 2^34}
    EXPECT_TRUE(diff == (rational{1, 2} - tick).value());
    auto neg = -a;
    EXPECT_TRUE(neg == -1.5);
    auto prod = a * inside<{0, 3}>{3}; // {0, 3·2^34}, notch 2^-32
    static_assert(detail::wide_index_storage<decltype(prod)>);
    EXPECT_TRUE(prod == 4.5);
    // narrow + wide and back.
    auto mixed = inside<{0, 100}>{7} + a;
    EXPECT_TRUE(mixed == 8.5);
}

TEST(WideGridTest, uint64_difference_widens) {
    constexpr umax kUM = ~umax{0};
    qword          x{kUM}, y{3};
    auto           d = x - y; // spans 2^65 values
    static_assert(detail::wide_index_storage<decltype(d)>);
    EXPECT_TRUE(d == kUM - 3);
    EXPECT_TRUE(y - x == -rational{kUM - 3});
    EXPECT_EQ(x.to<std::uint64_t>().value(), kUM);
}

TEST(WideGridTest, conversions) {
    fine a = 41.75;
    EXPECT_EQ(a.to<int>().value(), 41);
    EXPECT_EQ(a.to<double>().value(), 41.75);
    EXPECT_EQ(a.to<std::uint8_t>().value(), 41);
    fine big = p34;
    EXPECT_EQ(big.to<std::int32_t>().error(), errc::overflow);
    EXPECT_EQ(fine_signed{-1}.to<unsigned>().error(), errc::domain_error);
}

TEST(WideGridTest, exact_read_out) {
    // 2^33 + 2^-32 = (2^65 + 1) / 2^32, in lowest terms.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    using N        = decltype(odd.numerator());
    static_assert(detail::is_wide_int_v<N>);
    EXPECT_TRUE(odd.numerator() == (N{1} << 65) + N{1});
    EXPECT_TRUE(odd.denominator() == N{1} << 32);
    const fine half{0.5};
    EXPECT_TRUE(half.numerator() == N{1} && half.denominator() == N{2});
    const fine_signed neg{-0.75};
    EXPECT_TRUE(neg.numerator() == -decltype(neg.numerator()){3});
    // A 64-bit grid whose values pass imax reads out wide too.
    using big64     = inside<{0, std::numeric_limits<umax>::max()}>;
    const big64 top = std::numeric_limits<umax>::max();
    EXPECT_TRUE(top.numerator() == decltype(top.numerator()){std::numeric_limits<umax>::max()});
    // A grid whose values fit imax keeps imax.
    static_assert(std::is_same_v<decltype(inside<{0, 100}>{}.numerator()), imax>);
}

TEST(WideGridTest, same_notch_assignment) {
    // A wider sum maps back by a raw shift; out of range runs the policy.
    fine f = fine{1.5} + fine{2.25};
    EXPECT_TRUE(f == 3.75);
    fine_signed g = fine{1.5} + fine{2.25}; // offset raw (Lower ≠ 0)
    EXPECT_TRUE(g == 3.75);
    g = fine_signed{-1.5} - fine{2.25};
    EXPECT_TRUE(g == -3.75);
    const auto over = fine{rational{p34}} + fine{tick};
    EXPECT_THROW(f = over, inside_error);
    fine_clamp c = 0;
    c            = over;
    EXPECT_TRUE(c == rational{p34});
    fine_wrap w = 0;
    w           = over; // 2^34 + 2^-32 wraps to 0
    EXPECT_TRUE(w == 0);
}

TEST(WideGridTest, sum) {
    // 2^33 + 2^-32 per element: exact, however many 64-bit words the
    // index total needs.
    const fine        e = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    std::vector<fine> v(1000, e);
    using total = inside<{{0, p34 * 4096}, per<p32>}>;
    EXPECT_TRUE(beman::inside::sum<total>(v) == total::from_raw(detail::wide_uint<2>{1000} * e.raw()));
    // Offset (index raw, Lower ≠ 0) wide elements.
    std::vector<fine_signed> s{fine_signed{-1.5}, fine_signed{0.25}, fine_signed{tick}};
    EXPECT_TRUE(beman::inside::sum<fine_signed>(s) == fine_signed{(rational{-5, 4} + tick).value()});
    // Into a 64-bit target, by its policy: the total is checked once.
    using coarse = inside<{0, 100}, round_nearest>;
    EXPECT_TRUE(beman::inside::sum<coarse>(std::vector<fine>(3, fine{2.25})) == 7);
    using six = inside<{0, 6}>;
    EXPECT_THROW((void)beman::inside::sum<six>(std::vector<fine>(3, fine{2.25})), inside_error);
}

TEST(WideGridTest, to_string) {
    EXPECT_EQ(to_string(fine{1.5}), to_string(rational{3, 2}));
    // 2^33 + 2^-32 = (2^65 + 1) / 2^32: the numerator passes 64 bits.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_EQ(to_string(odd), "8589934592.00000000023283064365386962890625");
    EXPECT_EQ(from_chars<fine>(to_string(odd)), odd);
    EXPECT_EQ(to_string(detail::wide_uint<2>{1} << 100), "1267650600228229401496703205376");
    EXPECT_NE(to_string_debug(odd).find("wide_uint<2>"), std::string::npos);
}

TEST(WideGridTest, from_chars) {
    EXPECT_TRUE(from_chars<fine>("1.5").value() == 1.5);
    // 2^33 + 2^-32 needs a 66-bit numerator: the exact fallback parses it.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_TRUE(from_chars<fine>("36893488147419103233/4294967296").value() == odd);
    EXPECT_TRUE(from_chars<fine>("8589934592.00000000023283064365386962890625").value() == odd);
    EXPECT_EQ(from_chars<fine>("1e40").error(), errc::overflow); // out of range
    EXPECT_EQ(from_chars<fine>("12x").error(), errc::invalid_format);
    // Round trip through to_string.
    EXPECT_TRUE(from_chars<fine>(to_string(odd)).value() == odd);
}

TEST(WideGridTest, hashing) {
    std::unordered_set<fine> set{fine{1}, fine{1.5}, fine{1}};
    EXPECT_EQ(set.size(), 2u);
    EXPECT_EQ(std::hash<fine>{}(fine{2}), std::hash<fine>{}(fine{2}));
}

TEST(WideGridTest, uniform_sampling) {
    std::mt19937_64 rng{1};
    bool            above_2_33 = false;
    for (int i = 0; i < 1000; ++i) {
        const fine v = uniform<fine>(rng);
        ASSERT_TRUE(v >= 0 && v <= p34);
        above_2_33 = above_2_33 || v > (umax{1} << 33);
    }
    EXPECT_TRUE(above_2_33);
}

TEST(WideGridTest, division) {
    // (fine / fine has a quotient interval past 2^64: it needs C++26 big grids.)
    using divisor = inside<{{1, 4}, per<4>}>;
    fine a        = 3;
    auto q        = a / divisor{1.5}; // exact; may report overflow
    ASSERT_TRUE(q.has_value());
    EXPECT_TRUE(*q == 2);
    // (2^33 + 2^-32) / 1 has no 64-bit rational form: reported, not wrapped.
    const fine odd = fine::from_raw((detail::wide_uint<2>{1} << 65) + detail::wide_uint<2>{1});
    EXPECT_EQ((odd / divisor{1}).error(), errc::overflow);
    // narrow ÷ wide, including a zero divisor.
    auto r = inside<{0, 10}>{3} / fine{0.5};
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(*r == 6);
    EXPECT_EQ((inside<{0, 10}>{3} / fine{0}).error(), errc::division_by_zero);
}

TEST(WideGridTest, modulo) {
    // qword differences span 2^65 values: integers past imax.
    constexpr umax kUM = ~umax{0};
    auto           d   = qword{kUM} - qword{0};
    static_assert(detail::wide_index_storage<decltype(d)>);
    auto m = mod(d, inside<{1, 1000}>{7}, policy<snap>{});
    EXPECT_TRUE(m == (kUM % 7));
    auto n = mod(-d, inside<{1, 1000}>{7}, policy<snap>{}); // takes the dividend's sign
    EXPECT_TRUE(n == -static_cast<imax>(kUM % 7));
    // qword itself (a uint64 value raw past imax) now has a modulo too.
    EXPECT_TRUE((mod(qword{kUM}, inside<{1, 1000}>{10}, policy<snap>{}) == 5));
}

TEST(WideGridTest, clamp_and_wrap_actions) {
    // on_clamp: the overshoot is shaped like the builtin paths' (imax for an
    // integral source, an inside for an inside source).
    fine c                                       = 0;
    imax over                                    = 0;
    c.on_clamp([&](auto&, imax o) { over = o; }) = p34 + 5;
    EXPECT_TRUE(c == p34);
    EXPECT_EQ(over, 5);
    rational frac_over{0};
    c.on_clamp([&](auto&, rational o) { frac_over = o; }) = rational{-1, 2};
    EXPECT_TRUE(c == 0);
    EXPECT_TRUE((frac_over == rational{-1, 2}));
    bool got_inside = false;
    c.on_clamp([&](auto&, auto o) { got_inside = insidable<decltype(o)> && o == 6; }) =
        inside<{0, umax{1} << 40}>{p34 + 6};
    EXPECT_TRUE(got_inside);

    // on_wrap: the carry is the number of full turns.
    fine w     = 0;
    imax carry = 0;
    // One turn is 2^34 + 2^-32, so 3·2^34 + 2 = 3 turns + (2 − 3·2^-32).
    w.on_wrap([&](auto&, auto q) { carry = q; }) = 3 * p34 + 2;
    EXPECT_EQ(carry, 3);
    EXPECT_TRUE(w == (rational{2} - (rational{3} * tick).value()).value());
}

// Limb boundaries, where the removed 128-bit helpers kept their edge cases:
// slot counts 2^64 − 1 (the last uint64 index) and 2^64 (the first two-limb
// one), a 128-bit count, and arithmetic that crosses from one to the other.
TEST(WideGridTest, limb_edges) {
    constexpr umax kUM = ~umax{0};
    const rational q1{1, 4};                                                 // one notch
    using top64  = inside<{{rational{0}, rational{kUM, 4}}, per<4>}>;        // 2^64 − 1 slots
    using over64 = inside<{{rational{0}, rational{umax{1} << 62}}, per<4>}>; // 2^64 slots
    static_assert(std::is_same_v<top64::raw_type, std::uint64_t>);
    static_assert(std::is_same_v<over64::raw_type, detail::wide_uint<2>>);
    static_assert(grid_of<over64>.slot_bits() == 65);

    // The top slot of each round-trips.
    top64 t = rational{kUM, 4};
    EXPECT_EQ(t.raw(), kUM);
    over64 o = rational{umax{1} << 62};
    EXPECT_TRUE(o.raw() == (detail::wide_uint<2>{1} << 64));
    EXPECT_TRUE((-t == -rational{kUM, 4}));
    EXPECT_TRUE(-o == -rational{umax{1} << 62});

    // += across the 64-bit index boundary, by a point delta and by a raw.
    over64 c = rational{kUM, 4}; // index 2^64 − 1
    c += just<frac<1, 4>>;       // index 2^64
    EXPECT_TRUE(c.raw() == (detail::wide_uint<2>{1} << 64));
    EXPECT_THROW((c += just<frac<1, 4>>), inside_error); // past the top
    inside<{{rational{0}, rational{umax{1} << 62}}, per<4>}, wrap> w = rational{umax{1} << 62};
    w += just<frac<1, 4>>; // wraps to 0
    EXPECT_TRUE(w == 0);

    // Sums and differences across the boundary, both ends.
    auto s = o + inside<{{0, 1}, per<4>}>{1}; // 2^64 + 4 slots
    static_assert(detail::wide_index_storage<decltype(s)>);
    EXPECT_TRUE((s == rational{(umax{1} << 62) + 1}));
#if BEMAN_INSIDE_BIG_GRIDS
    // (2^64 − 1)/4 + (2^64 − 1)/4 passes the 64-bit grid numbers.
    auto tt = t + t; // 2^65 − 1 slots
    EXPECT_TRUE((tt == rational{kUM, 2}));
#endif
    auto d = top64{0} - t;
    EXPECT_TRUE((d == -rational{kUM, 4}));
    EXPECT_TRUE(t - top64{q1} == (rational{kUM, 4} - q1).value());

    // A 128-bit slot count: {−(2^64 − 1), 2^64 − 1} in steps of 1/(2^63 − 1).
    using full = inside<{{rational{kUM, imax{-1}}, rational{kUM}}, per<(umax{1} << 63) - 1>}>;
    static_assert(std::is_same_v<full::raw_type, detail::wide_uint<2>>);
    static_assert(grid_of<full>.slot_bits() == 128);
    full lo = rational{kUM, imax{-1}}, hi = rational{kUM};
    EXPECT_TRUE(lo.raw() == detail::wide_uint<2>{0});
    constexpr auto full_top = static_cast<detail::wide_uint<2>>(grid_of<full>.slot_count());
    EXPECT_TRUE(hi.raw() == full_top);
    EXPECT_TRUE(-lo == hi && lo < hi);
}

// Q-format division whose scaled dividend raw·N passes 64 bits (here 95):
// the native Q-format path runs in a wide work type instead of falling back
// to an exact fraction, so the result keeps the Q-format notch.
TEST(WideGridTest, qformat_division_past_64_bits) {
    using q32 = inside<{{0, (1ull << 31)}, per<(1ull << 32)>}, round_nearest>; // 2^63 slots
    q32  a = 1000.5, b = 0.25;
    auto q = (a / b).value();                              // b's grid includes 0
    static_assert(notch_of<decltype(q)> == notch_of<q32>); // Q-format result
    static_assert(!detail::rational_storage<decltype(q)>);
    EXPECT_TRUE(q == 4002);
    auto r = (q32{1} / q32{3}).value(); // rounds to the nearest 2^-32
    EXPECT_TRUE((r == rational{1431655765, imax{1} << 32}));
}

// The wide paths are constexpr.
namespace {
static_assert(fine{1.5} + fine{2} == 3.5);
static_assert(fine{1.5} < fine_signed{2});
static_assert(-fine{1} == -1);
static_assert([] {
    fine_clamp c = 0;
    c            = p34 + 1;
    return c == p34;
}());
} // namespace
