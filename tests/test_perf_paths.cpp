// Regression coverage for the perf-driven changes:
//   - Case 1: `needs_runtime_domain_check` gates the runtime range branch
//             in assignment::assign. Verify behaviour is preserved under
//             every policy and action that should still trigger it.
//   - Case 3: native_div_qformat fast path + Q-format operator rational()
//             fast path. Verify numerical exactness vs the rational route.

#include "bound/bound.hpp"
#include "bound/rational.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace bnd;

//---------------------------------------------------------------------------
// Case 1 — runtime range branch elision under `unsafe`
//---------------------------------------------------------------------------
TEST_CASE("unsafe + no action: out-of-range int rhs stores without throwing",
          "[bound][unsafe][perf]")
{
  using L = bound<{0, 100}, unsafe>;
  // The += must not throw, even with an out-of-range value. Storage holds
  // the raw value as-is (UB on read, but that's the `unsafe` contract).
  L b{50};
  REQUIRE_NOTHROW((b += 200));
  // No assertion on the value — `unsafe` doesn't promise meaningful behaviour
  // for out-of-range writes, only that they don't throw.
}

TEST_CASE("checked + no action: range check still fires", "[bound][checked][perf]")
{
  using L = bound<{0, 100}, checked>;
  L b{50};
  REQUIRE_THROWS_AS((b += 200), std::system_error);
}

TEST_CASE("clamp policy: range check still fires", "[bound][clamp][perf]")
{
  using L = bound<{0, 100}, clamp>;
  L b{50};
  b += 200;
  REQUIRE(b == 100);
}

TEST_CASE("wrap policy: range check still fires", "[bound][wrap][perf]")
{
  using L = bound<{0, 100}, wrap>;
  L b{50};
  b += 200;
  REQUIRE(b == 48);   // (50 + 200 - 0) mod 101 + 0 = 250 mod 101 = 48
}

TEST_CASE("unsafe + on_overflow action: action fires on imax overflow",
          "[bound][action][perf]")
{
  using L = bound<{0, 100}, unsafe>;
  L b{50};
  bool fired = false;
  // The += through policy_ref with on_overflow probes add_overflow (imax).
  // imax_max + 1 overflows imax; the action fires. (Bound-range overflow
  // is unrelated — that's handled by clamp/wrap/sentinel.)
  b.on_overflow([&](auto&, auto){ fired = true; }) += std::numeric_limits<imax>::max();
  REQUIRE(fired);
}

//---------------------------------------------------------------------------
// Case 3 — Q-format fast path correctness
//---------------------------------------------------------------------------
TEST_CASE("Q-format division: native_div_qformat matches rational arithmetic",
          "[bound][qformat][division]")
{
  using fp = bound<{{0, 255}, *(1_r / 256)}, unsafe>;       // Q8.8, unsafe → ignore_round

  // Spot checks against expected Q-format integer-truncation values.
  auto q1 = div(fp{200}, fp{8}, truncated);
  REQUIRE(q1.has_value());
  REQUIRE(*q1 == rational{25u});

  auto q2 = div(fp{255}, fp{1}, truncated);
  REQUIRE(q2.has_value());
  REQUIRE(*q2 == rational{255u});

  // Non-integer quotient: 200 / 3 ≈ 66.6667. Q-format multiplies before
  // dividing — (51200 * 256) / 768 = 17066 (= floor(66.6667 * 256)) — same
  // as native `(a << 8) / b`. NOT 66 * 256 = 16896 (that would be
  // truncate-then-scale, which loses fractional precision).
  auto q3 = div(fp{200}, fp{3}, truncated);
  REQUIRE(q3.has_value());
  REQUIRE((*q3).Raw == 17066);

  // Divide by zero produces nullopt.
  auto q4 = div(fp{1}, fp{0}, truncated);
  REQUIRE_FALSE(q4.has_value());
}

TEST_CASE("Q-format division: result type is Q-format (same notch as L)",
          "[bound][qformat][division]")
{
  using fp = bound<{{0, 255}, *(1_r / 256)}, unsafe>;
  auto q = div(fp{200}, fp{8}, truncated);
  using R = std::remove_cvref_t<decltype(*q)>;
  STATIC_REQUIRE(Notch<R> == Notch<fp>);   // same Q-format, not rational-raw
  STATIC_REQUIRE_FALSE(IsRawRational<R>);
}

//---------------------------------------------------------------------------
// Case 3 — operator rational() fast path correctness
//---------------------------------------------------------------------------
TEST_CASE("operator rational() round-trips through Q-format fast path",
          "[bound][qformat][rational]")
{
  using fp = bound<{{0, 255}, *(1_r / 256)}, unsafe>;

  // Bit-for-bit exactness over a sampling of values.
  for (int v : {0, 1, 50, 127, 254, 255})
  {
    fp b{v};
    rational r = static_cast<rational>(b);
    REQUIRE(r == rational{static_cast<unsigned>(v)});
    // round-trip back to fp.value
    REQUIRE(b == v);
  }
}

TEST_CASE("operator rational() handles fractional Q-format value",
          "[bound][qformat][rational]")
{
  using fp = bound<{{0, 255}, *(1_r / 256)}, unsafe>;
  // Raw=128 → value 128/256 = 0.5.
  fp b{0};
  b.Raw = 128;
  rational r = static_cast<rational>(b);
  REQUIRE(r == rational{1u, 2});
}
