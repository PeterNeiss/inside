// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Representation policy flags — `exact` / `direct` / `indexed` force a raw
// representation the way `real` does; without one the grid deduces it.
// Selection resolves widest-wins (exact > real > direct > indexed > deduced),
// matching the OR-propagation of policies through arithmetic.
//
// (The grid-shape gates — `direct` needs Notch == 1, `indexed` needs a notch —
// are in-class static_asserts, so an invalid combination is a compile error,
// not probe-able via concepts.)

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/formats.hpp>   // beman::inside::byte — cross-check explicit u8 flag agrees
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace beman::inside;
using beman::inside::detail::rational;

// default ctor is trivial (no zero-fill footgun)
TEST(StorageFlagsTest, default_ctor_is_trivial_no_zero_fill_footgun)
{
  // The default ctor is `= default` for every policy — Raw is uninitialized,
  // like a built-in scalar, rather than zero-filled to a possibly-invalid slot.
  // checked is the default policy; rational/value/real storage all stay trivial.
  static_assert(std::is_trivially_default_constructible_v<inside<{0, 10}>>);          // index
  static_assert(std::is_trivially_default_constructible_v<inside<{-100, -5}>>);       // signed value
  static_assert(std::is_trivially_default_constructible_v<inside<grid{5}>>);          // rational (point grid)
  static_assert(std::is_trivially_default_constructible_v<inside<{{0, 10}, notch<1, 4>}, exact | round_nearest>>);
}

// exact forces rational raw on any grid
TEST(StorageFlagsTest, exact_forces_rational_raw_on_any_grid)
{
  // A notched grid that deduction would store as an integer index.
  using E = inside<{{0, 10}, notch<1, 4>}, exact | round_nearest>;
  static_assert(std::is_same_v<E::raw_type, rational>);
  static_assert(detail::rational_raw<E>);

  // The value is held as an exact fraction and still obeys the grid.
  E q{rational{3, 4}};
  ASSERT_EQ(q.raw(), (rational{3, 4}));
  ASSERT_EQ(rational{q}, (rational{3, 4}));

  // round_nearest snaps an off-grid value to the nearest quarter.
  E r{rational{2, 5}};                       // 0.4 → 0.5? no: nearest 1/4 is 2/4
  ASSERT_EQ(rational{r}, (rational{1, 2}));

  // Integral rhs and arithmetic stay exact.
  E one{1};
  ASSERT_EQ(rational{one}, 1);
  auto sum = q + q;
  ASSERT_EQ(rational{sum}, (rational{3, 2}));

  // Non-dyadic grids are fine (this is what `real` cannot do).
  using T = inside<{{0, 1}, notch<1, 3>}, exact>;
  static_assert(detail::rational_raw<T>);
  T third{rational{1, 3}};
  ASSERT_EQ(rational{third}, (rational{1, 3}));
  ASSERT_EQ(beman::inside::to_string(third), "1/3");
}

// direct forces raw == value where deduction picks an index
TEST(StorageFlagsTest, direct_forces_raw_eq_value_where_deduction_picks_an_index)
{
  // Deduced: {5,100} is index storage (unsigned raw 0..95).
  static_assert(detail::index_raw<inside<{5, 100}>>);

  // Forced: raw holds the value 5..100 itself.
  using D = inside<{5, 100}, direct>;
  static_assert(detail::value_raw<D>);
  static_assert(std::is_same_v<D::raw_type, std::uint8_t>);

  D d{42};
  ASSERT_EQ(d.raw(), 42);                    // raw IS the value
  ASSERT_TRUE((inside<{5, 100}>{42}.raw() == 37));  // deduced index for contrast

  // Value extraction goes through the kind-aware decode, not the
  // raw-signedness heuristic (which would have returned 42 + Lower = 47).
  ASSERT_EQ(rational{d}, 42);
  ASSERT_TRUE(static_cast<imax>(d) == 42);
  ASSERT_TRUE(d.to<double>().value() == 42.0);

  // Round-trip through assignment and comparison.
  D e{d};
  ASSERT_EQ(e, d);
  ASSERT_EQ(e, 42);
}

// indexed forces raw == 0-based index where deduction picks a value
TEST(StorageFlagsTest, indexed_forces_raw_eq_0_based_index_where_deduction_picks_a_value)
{
  // Deduced: {-5,5} is signed direct storage.
  static_assert(detail::value_raw<inside<{-5, 5}>>);

  // Forced: dense unsigned 0-based index 0..10.
  using I = inside<{-5, 5}, indexed>;
  static_assert(detail::index_raw<I>);
  static_assert(std::is_same_v<I::raw_type, std::uint8_t>);

  ASSERT_EQ(I{-5}.raw(), 0);
  ASSERT_EQ(I{0}.raw(), 5);
  ASSERT_EQ(I{5}.raw(), 10);
  ASSERT_EQ(rational{I{-5}}, -5);
  ASSERT_TRUE(static_cast<imax>(I{3}) == 3);
}

// fixed-width flags pin the raw type (value storage)
TEST(StorageFlagsTest, fixed_width_flags_pin_the_raw_type_value_storage)
{
  // Deduced: {0,100} fits uint8. The width flag pins a wider type instead.
  static_assert(std::is_same_v<inside<{0, 100}>::raw_type, std::uint8_t>);

  using W = inside<{0, 100}, u16>;
  static_assert(std::is_same_v<W::raw_type, std::uint16_t>);
  static_assert(detail::value_raw<W>);                 // raw == value
  ASSERT_EQ(W{42}.raw(), 42);
  ASSERT_EQ(rational{W{42}}, 42);
  ASSERT_TRUE(static_cast<imax>(W{42}) == 42);

  // Signed width holds negatives directly (value storage, not an index).
  using S = inside<{-5, 5}, i8>;
  static_assert(std::is_same_v<S::raw_type, std::int8_t>);
  static_assert(detail::value_raw<S>);
  ASSERT_EQ(S{-5}.raw(), -5);
  ASSERT_EQ(S{0}.raw(), 0);
  ASSERT_EQ(rational{S{-3}}, -3);

  // Unsigned width with a non-zero Lower is still VALUE storage (the width flag
  // overrides deduction, which would otherwise pick an index for Lower != 0).
  using V = inside<{5, 100}, u8>;
  static_assert(std::is_same_v<V::raw_type, std::uint8_t>);
  static_assert(detail::value_raw<V>);
  ASSERT_EQ(V{42}.raw(), 42);                           // raw IS the value, not 37
  ASSERT_TRUE((inside<{5, 100}>{42}.raw() == 37));             // deduced index for contrast

  // beman::inside::byte (formats.hpp) and an explicit u8 flag agree on the raw type.
  static_assert(std::is_same_v<byte::raw_type, inside<{0, 254}, u8>::raw_type>);

  // These are deliberate compile errors (storage_pick static_asserts) — there is
  // no silent widening for a user-pinned width, so they cannot be instantiated:
  //   inside<{0, 100000}, u8>       // value range overflows uint8
  //   inside<{-200, 0}, u8>         // unsigned type can't hold negatives
  //   inside<{0, 100}, u8 | u16>    // more than one width flag
  //   inside<{{0,4},notch<1,16>}, u16>  // value storage needs Notch == 1
}

// a width flag with `indexed` pins the raw type for index storage
TEST(StorageFlagsTest, a_width_flag_with_indexed_pins_the_raw_type_for_index_storage)
{
  // Notched grid: value storage is impossible, so index storage in the pinned
  // type. {0,4} step 1/16 → 64 slots; pin uint32 even though uint8 would fit.
  using I = inside<{{0, 4}, notch<1, 16>}, u32 | indexed>;
  static_assert(std::is_same_v<I::raw_type, std::uint32_t>);
  static_assert(detail::index_raw<I>);
  ASSERT_EQ(I{rational{0}}.raw(), 0);
  ASSERT_EQ(I{rational{4}}.raw(), 64);
  ASSERT_EQ((rational{I{rational{1, 16}}}), (rational{1, 16}));

  // Round-trip a forced-wide unit grid through assignment and comparison.
  using B = inside<{0, 10}, u32>;
  static_assert(std::is_same_v<B::raw_type, std::uint32_t>);
  B x{7}; B y{x};
  ASSERT_EQ(y, x);
  ASSERT_EQ(y, 7);
}

// representation flags resolve widest-wins
TEST(StorageFlagsTest, representation_flags_resolve_widest_wins)
{
  // exact beats real: a mixed math chain falls back to exact fractions.
  using Ex = inside<{{0, 4}, notch<1, 256>}, exact | round_nearest>;
  using Re = inside<{{0, 4}, notch<1, 256>}, round_nearest | real>;
  using Sum = decltype(Ex{} + Re{});
  static_assert((InsidePolicy<Sum> & exact) == exact);
  static_assert(detail::rational_raw<Sum>);
  ASSERT_EQ((rational{Sum{Ex{rational{1, 256}} + Re{rational{2, 256}}}}), (rational{3, 256}));

  // exact | real spelled directly on one inside: exact wins, both engines.
  using Both = inside<{{0, 4}, notch<1, 256>}, exact | real>;
  static_assert(detail::rational_raw<Both>);

  // real beats direct on a dyadic unit grid (default engine only — under
  // BEMAN_INSIDE_MATH_FIXED the real arm is elided and direct wins).
  using RD = inside<{0, 4}, real | direct>;
#ifndef BEMAN_INSIDE_MATH_FIXED
  static_assert(detail::f64_raw<RD>);
#else
  static_assert(detail::value_raw<RD>);
#endif

  // direct beats indexed.
  using DI = inside<{5, 100}, direct | indexed>;
  static_assert(detail::value_raw<DI>);
  ASSERT_EQ(DI{42}.raw(), 42);
}

#ifndef BEMAN_INSIDE_MATH_FIXED
// f32 selects binary32-backed storage; arithmetic demotes when too fine
TEST(StorageFlagsTest, f32_selects_binary32_backed_storage_arithmetic_demotes_when_too_fine)
{
  using F = inside<{{-8, 8}, notch<1, 256>}, round_nearest | f32>;
  static_assert(std::is_same_v<F::raw_type, float>);
  static_assert(detail::f32_raw<F>);
  static_assert(detail::fp_raw<F> && !detail::f64_raw<F>);

  // Construct/read are lossless on the float-exact grid.
  F a{rational{3, 2}};
  ASSERT_TRUE(static_cast<double>(a) == 1.5);
  ASSERT_EQ((rational{a + F{rational{1, 4}}}), (rational{7, 4}));   // 1.75
  ASSERT_EQ(rational{a * F{2}}, 3);

  // f32 ⊕ f32 stays f32 while the result grid still fits float...
  using Sum = decltype(F{} + F{});                              // notch 1/256, |v|≤16
  static_assert(detail::f32_raw<Sum>);

  // ...but a product whose grid outgrows float's 24-bit significand demotes to f64
  // (notch 1/65536, |v|≤2^16 → 2^32 > 2^24, but < 2^53).
  using Wide = inside<{{-256, 256}, notch<1, 256>}, round_nearest | f32>;
  using WideProd = decltype(Wide{} * Wide{});
  static_assert(detail::f64_raw<WideProd>);

  // Mixing f32 with f64 widens to f64; exact still beats both.
  using D = inside<{{-8, 8}, notch<1, 256>}, round_nearest | f64>;
  static_assert(detail::f64_raw<decltype(F{} + D{})>);
  using E = inside<{{-8, 8}, notch<1, 256>}, exact>;
  static_assert(detail::rational_raw<decltype(E{} + F{})>);
}

// math output lands in f32 storage (flt engine pairs with f32)
TEST(StorageFlagsTest, math_output_lands_in_f32_storage_flt_engine_pairs_with_f32)
{
  using Ang = inside<{{-8, 8}, notch<1, 256>}, round_nearest | f32>;
  using Sq  = inside<{{0, 16}, notch<1, 256>}, round_nearest | f32>;
  // The float engine stores its result straight into the f32 raw (no rational).
  auto s = math::flt::sin(Ang{0});
  auto r = math::flt::sqrt(Sq{4});
  static_assert(detail::f32_raw<decltype(s)>);
  static_assert(detail::f32_raw<decltype(r)>);
  ASSERT_EQ(rational{s}, 0);
  ASSERT_EQ(rational{r}, 2);

  // Auto-demote: an f32 input whose result grid overflows binary32 (exp's range
  // e^20 ≈ 4.85e8 > 2^24, still < 2^53) widens its OUTPUT to f64 storage rather
  // than hard-erroring — the deduced output never static_asserts on f32 overflow.
  using Big = inside<{{0, 20}, notch<1, 256>}, round_nearest | f32>;
  auto e = math::flt::exp(Big{2});
  static_assert(detail::f64_raw<decltype(e)>);   // demoted f32 → f64
  ASSERT_TRUE(rational{e} > rational{7});             // ≈ 7.39
}
#endif // !BEMAN_INSIDE_MATH_FIXED

// f64 is the canonical double-backed flag; real is its alias
TEST(StorageFlagsTest, f64_is_the_canonical_double_backed_flag_real_is_its_alias)
{
  // `real` was renamed `f64`; the alias is bit-identical, so old code compiles.
  static_assert(beman::inside::f64 == beman::inside::real);
  static_assert(has_flag(beman::inside::f64, round_nearest));   // still carries snap/round

#ifndef BEMAN_INSIDE_MATH_FIXED
  // f64 selects binary64-backed storage exactly as `real` did (storage is
  // independent of the compute engine — true in the double AND float builds).
  using F = inside<{{0, 4}, notch<1, 256>}, round_nearest | f64>;
  using R = inside<{{0, 4}, notch<1, 256>}, round_nearest | real>;
  static_assert(std::is_same_v<F::raw_type, double>);
  static_assert(std::is_same_v<F::raw_type, R::raw_type>);
  static_assert(detail::f64_raw<F>);
#endif
}

// representation flags compose with behavior policies
TEST(StorageFlagsTest, representation_flags_compose_with_behavior_policies)
{
  // exact + clamp: out-of-range snaps to the endpoint, stored exactly.
  using EC = inside<{{0, 10}, notch<1, 4>}, exact | clamp | round_nearest>;
  ASSERT_EQ(rational{EC{rational{15}}}, 10);
  ASSERT_EQ(rational{EC{rational{-3}}}, 0);

  // exact + wrap: modular reduction onto the exact grid
  // (range = Upper − Lower + Notch = 10.25, so 11.25 wraps to 1).
  using EW = inside<{{0, 10}, notch<1, 4>}, exact | wrap | round_nearest>;
  ASSERT_EQ((rational{EW{rational{45, 4}}}), 1);

  // direct + sentinel: out-of-range yields the empty slot.
  using DS = inside<{5, 100}, direct | sentinel>;
  auto ok   = DS::try_make(42);
  auto fail = DS::try_make(200);
  ASSERT_TRUE(ok.has_value());
  ASSERT_EQ(*ok, 42);
  ASSERT_TRUE(!fail.has_value());
}

// representation flags print the value, not the raw
TEST(StorageFlagsTest, representation_flags_print_the_value_not_the_raw)
{
  using E = inside<{{0, 1}, notch<1, 3>}, exact>;
  ASSERT_EQ((beman::inside::to_string(E{rational{2, 3}})), "2/3");

  using I = inside<{-5, 5}, indexed>;
  ASSERT_EQ(beman::inside::to_string(I{-3}), "-3");      // value, not index 2
}

// real storage runs the full out-of-range policy cascade
TEST(StorageFlagsTest, real_storage_runs_the_full_out_of_range_policy_cascade)
{
  // clamp: saturate to the (grid-point) endpoint.
  using RC = inside<{{0, 4}, notch<1, 256>}, real | clamp>;
  ASSERT_TRUE(static_cast<double>(rational{RC{9.5}})  == 4.0);
  ASSERT_TRUE(static_cast<double>(rational{RC{-1.5}}) == 0.0);

  // wrap: fold into [Lower, Lower + span + notch) — same convention as the
  // fractional path. Span 0..359 with notch 1 wraps 370 → 10, -10 → 350.
  using RW = inside<{{0, 359}, notch<1>}, real | wrap>;
  ASSERT_TRUE(static_cast<double>(rational{RW{370.0}}) == 10.0);
  ASSERT_TRUE(static_cast<double>(rational{RW{-10.0}}) == 350.0);

  // checked: out-of-range reports (throws) instead of silently storing.
  using RK = inside<{{0, 4}, notch<1, 256>}, real | checked>;
  ASSERT_THROW((void)(RK{9.5}), beman::inside::inside_error);
  ASSERT_TRUE(static_cast<double>(rational{RK{2.5}}) == 2.5);

  // sentinel: out-of-range yields the empty slot.
  using RS = inside<{{0, 4}, notch<1, 256>}, real | sentinel>;
  ASSERT_TRUE(RS::try_make(2.0).has_value());
  ASSERT_TRUE(!RS::try_make(9.5).has_value());

  // unchecked (bare real): stores as-is — unchanged legacy behavior.
  using RU = inside<{{0, 4}, notch<1, 256>}, real>;
  ASSERT_TRUE(static_cast<double>(rational{RU{2.5}}) == 2.5);
}

// non-finite doubles are rejected, both engines
TEST(StorageFlagsTest, non_finite_doubles_are_rejected_both_engines)
{
  // Default engine: store_real guards before the grid snap; fixed engine:
  // the integer-backed path throws in rational(double). Same observable.
  using R = inside<{{0, 4}, notch<1, 256>}, round_nearest | real>;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  ASSERT_THROW((void)(R{nan}), beman::inside::inside_error);
  ASSERT_THROW((void)(R{inf}), beman::inside::inside_error);
  ASSERT_THROW((void)(R{-inf}), beman::inside::inside_error);

  // Non-finite input is reported as errc::not_finite (distinct from the
  // domain_error used for finite-but-out-of-interval values), both engines.
  try { R{nan}; FAIL() << "expected throw"; }
  catch (const beman::inside::inside_error& e)
  { ASSERT_EQ(e.code, errc::not_finite); }
}

// Full-domain inverse trig (improvement #2): atan beyond |x| ≤ 1 via
// reciprocal reduction; atan2 beyond the unit square via max-magnitude
// normalization. Engine-neutral (both engines accept the same programs).
// atan / atan2 accept magnitudes beyond 1
TEST(StorageFlagsTest, atan_atan2_accept_magnitudes_beyond_1)
{
  using wide_t = inside<{{-16, 16}, notch<1, 16384>}, round_nearest | real>;

  auto val = [](auto b) { return static_cast<double>(rational{b}); };

  // Determinism: pin the exact grid output (a multiple of 1/16384, exactly
  // representable in double). These values are bit-identical on both math engines
  // and every platform; the comments give the true atan they snap to.
  ASSERT_EQ(val(math::atan(wide_t{2})), 0x1.1b7p+0);    // ~1.1071488
  ASSERT_EQ(val(math::atan(wide_t{-3})), -0x1.3fcp+0);    // ~-1.2490234
  ASSERT_EQ(val(math::atan(wide_t{16})), 0x1.8224p+0);   // ~1.5083618
  ASSERT_EQ((val(math::atan(wide_t{rational{1, 2}}))), 0x1.dacp-2);  // ~0.4636230

  ASSERT_EQ((val(math::atan2(wide_t{3},  wide_t{1}))), 0x1.3fcp+0);    // ~1.2490234
  ASSERT_EQ((val(math::atan2(wide_t{1},  wide_t{-5}))), 0x1.78dcp+1);   // ~2.9442139
  ASSERT_EQ((val(math::atan2(wide_t{-7}, wide_t{2}))), -0x1.4aep+0);    // ~-1.2924805
}

// Regression: per-operation policy overrides (`with_*`, `on_*`, `policy(ec)`)
// route through the assignment engine, which previously had no f64_raw arm
// and wrote integer offsets into the double raw (e.g. with_clamp stored the
// notch COUNT instead of the endpoint).
// per-operation policies work on real-backed bounds
TEST(StorageFlagsTest, per_operation_policies_work_on_real_backed_bounds)
{
  using R = inside<{{1, 4}, notch<1, 256>}, round_nearest | real>;  // Lower != 0

  R a{2.0};
  a.with_clamp() = 9.5;
  ASSERT_TRUE(static_cast<double>(rational{a}) == 4.0);
  a.with_clamp() = 0.25;
  ASSERT_TRUE(static_cast<double>(rational{a}) == 1.0);

  R b{2.0};
  int fired = 0;
  b.on_clamp([&](auto&, auto){ ++fired; }) = 0.0;
  ASSERT_TRUE(static_cast<double>(rational{b}) == 1.0);
  ASSERT_EQ(fired, 1);

  // In-range per-operation store snaps onto the grid.
  R c{1.0};
  c.policy<snap>() = 2.5;
  ASSERT_TRUE(static_cast<double>(rational{c}) == 2.5);

  // error_code mode: out-of-range reports, value unchanged.
  beman::inside::errc ec{};
  R d{2.0};
  d.policy<checked>(ec) = 9.5;
  ASSERT_TRUE(ec != errc{});
  ASSERT_TRUE(static_cast<double>(rational{d}) == 2.0);

  // inside rhs through the per-operation clamp.
  using S = inside<{0, 100}>;
  R e{2.0};
  e.with_clamp() = S{50};
  ASSERT_TRUE(static_cast<double>(rational{e}) == 4.0);

  // wrap override on a real-backed grid.
  using W = inside<{{0, 359}, notch<1>}, round_nearest | real>;
  W w{0.0};
  w.with_wrap() = 370.0;
  ASSERT_TRUE(static_cast<double>(rational{w}) == 10.0);
}

// Issue #4: checked exact arithmetic drops the slim::optional wrapper when
// the grids PROVE no rational overflow is reachable (notched grids inside the
// denominators). Continuous (Notch == 0) grids store arbitrary rationals —
// nothing is provable, so they keep the wrapper (this also fixes a soundness
// hole: the old mul gate claimed safety for continuous grids).
// provably-safe exact arithmetic returns a plain inside
TEST(StorageFlagsTest, provably_safe_exact_arithmetic_returns_a_plain_inside)
{
  using E = inside<{{0, 10}, notch<1, 4>}, exact | checked | round_nearest>;
  E a{rational{3, 4}}, b{rational{5, 4}};

  auto s = a + b;
  static_assert(!detail::is_slim_optional_v<decltype(s)>);
  ASSERT_EQ(rational{s}, 2);

  auto d = a - b;
  static_assert(!detail::is_slim_optional_v<decltype(d)>);
  ASSERT_EQ(rational{d}, (rational{-1, 2}));

  auto p = a * b;
  static_assert(!detail::is_slim_optional_v<decltype(p)>);
  ASSERT_EQ(rational{p}, (rational{15, 16}));

  // Continuous grids: denominators unbounded → wrapper stays (add AND mul).
  using C = inside<{{0, 10}, 0}, checked>;
  static_assert(detail::is_slim_optional_v<decltype(C{} + C{})>);
  static_assert(detail::is_slim_optional_v<decltype(C{} * C{})>);

  // ...and the check is real: huge-denominator values overflow into nullopt
  // instead of silently wrapping (the pre-fix mul gate claimed these safe).
  C x = C::from_raw(rational{1, imax{1} << 40});
  auto wide = x * x;                      // den 2^80 > imax
  ASSERT_TRUE(!wide.has_value());
}

// Smaller-threads batch: wide trig envelope, pown, clamp saturation.
// sin/cos/tan accept radians up to 2^20
TEST(StorageFlagsTest, sin_cos_tan_accept_radians_up_to_2_20)
{
  using wide_t = inside<{{-(imax{1} << 20), imax{1} << 20}, notch<1, 1024>},
                       round_nearest | real>;
  auto val = [](auto b) { return static_cast<double>(rational{b}); };
  const double tol = 2.0 / 1024;          // grid tolerance (notch ≈ 9.8e-4)

  const double xs[] = {100000.0, -551496.5, 1048576.0, -1048576.0, 3.0};
  for (double x : xs)
  {
    ASSERT_TRUE(std::fabs(val(math::sin(wide_t{x})) - std::sin(x)) < tol);
    ASSERT_TRUE(std::fabs(val(math::cos(wide_t{x})) - std::cos(x)) < tol);
  }
}

// pown<E> - exact compile-time integer powers on any inside
TEST(StorageFlagsTest, pown_e_exact_compile_time_integer_powers_on_any_inside)
{
  using s8 = inside<{-10, 10}>;
  ASSERT_TRUE(math::pown<3>(s8{-2}) == -8);
  ASSERT_TRUE(math::pown<0>(s8{7})  == 1);
  ASSERT_TRUE(math::pown<1>(s8{5})  == 5);
  ASSERT_TRUE(math::pown<5>(s8{3})  == 243);

  // Result grid widens corner-correctly: (-10..10)^3 covers ±1000.
  using cube_t = decltype(math::pown<3>(s8{}));
  static_assert(Lower<cube_t> <= -1000);
  static_assert(Upper<cube_t> >= 1000);

  // Exact on fractional grids.
  using q = inside<{{0, 2}, notch<1, 4>}, round_nearest>;
  ASSERT_EQ((rational{math::pown<2>(q{rational{3, 4}})}), (rational{9, 16}));
}

// tan saturates instead of erroring when Out carries clamp
TEST(StorageFlagsTest, tan_saturates_instead_of_erroring_when_out_carries_clamp)
{
  // Explicit-Out spelling is the impl form (`tan<T>(x)` would bind T as the
  // INPUT of the auto form). tan_impl's CORDIC core is compiled in both
  // engine builds (it is the compile-time grid oracle), so one path tests
  // both configs.
  using in_t  = inside<{{-2, 2}, notch<1, 16384>}, round_nearest | real>;
  using sat_t = inside<{{-1, 1}, notch<1, 16384>}, round_nearest | real | clamp>;
  using err_t = inside<{{-1, 1}, notch<1, 16384>}, round_nearest | real>;

  // tan(1.2) ≈ 2.57 — beyond [-1, 1].
  auto sat = math::tan_impl<sat_t>(in_t{1.2});
  ASSERT_TRUE(sat.has_value());
  ASSERT_TRUE(static_cast<double>(rational{*sat}) == 1.0);   // clamped to Upper

  auto err = math::tan_impl<err_t>(in_t{1.2});
  ASSERT_TRUE(!err.has_value());
  ASSERT_EQ(err.error(), errc::overflow);
}
