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
using namespace bnd::detail;

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
  // A delta bound with Lower==0 keeps the raw-add fast path (a singleton `200_b`
  // would widen to a disjoint grid that only clamp/wrap can absorb).
  REQUIRE_NOTHROW((b += bound<{0, 200}>{200}));
  // No assertion on the value — `unsafe` doesn't promise meaningful behaviour
  // for out-of-range writes, only that they don't throw.
}

TEST_CASE("checked + no action: range check still fires", "[bound][checked][perf]")
{
  using L = bound<{0, 100}, checked>;
  L b{50};
  REQUIRE_THROWS_AS((b += bound<{0, 200}>{200}), std::system_error);
}

TEST_CASE("clamp policy: range check still fires", "[bound][clamp][perf]")
{
  using L = bound<{0, 100}, clamp>;
  L b{50};
  b += bound<{0, 200}>{200};
  REQUIRE(b == 100);
}

TEST_CASE("wrap policy: range check still fires", "[bound][wrap][perf]")
{
  using L = bound<{0, 100}, wrap>;
  L b{50};
  b += bound<{0, 200}>{200};
  REQUIRE(b == 48);   // (50 + 200 - 0) mod 101 + 0 = 250 mod 101 = 48
}

//---------------------------------------------------------------------------
// Case 3 — Q-format fast path correctness
//---------------------------------------------------------------------------
TEST_CASE("Q-format division: native_div_qformat matches rational arithmetic",
          "[bound][qformat][division]")
{
  using fp = bound<{{0, 255}, 0x1p-8_r}>;   // Q8.8; the `truncated` call policy
                                            // supplies snapping for the native
                                            // path, without unsafe's ignore_zero

  // Spot checks against expected Q-format integer-truncation values.
  auto q1 = div(fp{200}, fp{8}, truncated);
  REQUIRE(q1.has_value());
  REQUIRE(*q1 == 25);

  auto q2 = div(fp{255}, fp{1}, truncated);
  REQUIRE(q2.has_value());
  REQUIRE(*q2 == 255);

  // Non-integer quotient: 200 / 3 ≈ 66.6667. Q-format multiplies before
  // dividing — (51200 * 256) / 768 = 17066 (= floor(66.6667 * 256)) — same
  // as native `(a << 8) / b`. NOT 66 * 256 = 16896 (that would be
  // truncate-then-scale, which loses fractional precision).
  auto q3 = div(fp{200}, fp{3}, truncated);
  REQUIRE(q3.has_value());
  REQUIRE((*q3).raw() == 17066);

  // Divide by zero produces nullopt.
  auto q4 = div(fp{1}, fp{0}, truncated);
  REQUIRE_FALSE(q4.has_value());
}

TEST_CASE("Q-format division: result type is Q-format (same notch as L)",
          "[bound][qformat][division]")
{
  using fp = bound<{{0, 255}, 0x1p-8_r}, unsafe>;
  auto q = div(fp{200}, fp{8}, truncated);
  using R = std::remove_cvref_t<decltype(*q)>;
  STATIC_REQUIRE(Notch<R> == Notch<fp>);   // same Q-format, not rational-raw
  STATIC_REQUIRE_FALSE(rational_raw<R>);
}

//---------------------------------------------------------------------------
// Case 3 — operator rational() fast path correctness
//---------------------------------------------------------------------------
TEST_CASE("operator rational() round-trips through Q-format fast path",
          "[bound][qformat][rational]")
{
  using fp = bound<{{0, 255}, 0x1p-8_r}, unsafe>;

  // Bit-for-bit exactness over a sampling of values.
  for (int v : {0, 1, 50, 127, 254, 255})
  {
    fp b{v};
    rational r = b;
    REQUIRE(r == rational{static_cast<unsigned>(v)});
    // round-trip back to fp.value
    REQUIRE(b == v);
  }
}

TEST_CASE("operator rational() handles fractional Q-format value",
          "[bound][qformat][rational]")
{
  using fp = bound<{{0, 255}, 0x1p-8_r}, unsafe>;
  // Raw=128 → value 128/256 = 0.5.
  auto b = fp::from_raw(128);
  rational r = b;
  REQUIRE(r == 0.5_r);
}
