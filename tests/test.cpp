#include <iostream>
#include <limits>

#include "bound/generic.hpp"
#include "bound/bound.hpp"
#include "bound/interval.hpp"
#include "bound/lift.hpp"
#include "bound/print.hpp"
#include "bound/formatter.hpp"

using namespace bnd;

//---------------------------------------------------------------------------
// check helpers
//---------------------------------------------------------------------------
int failures = 0;

template <boundable B>
void check(const char* label, B actual, rational expected)
{
  rational got = static_cast<rational>(actual);
  std::cout << label << bnd::to_string_debug(actual);
  if (got == expected)
    std::cout << "  [PASS]" << std::endl;
  else
  {
    std::cout << "  [FAIL] expected " << expected << std::endl;
    ++failures;
  }
}

template <boundable B>
void check(const char* label, slim::optional<B> actual, rational expected)
{
  if (!actual)
  {
    std::cout << label << "(null)  [FAIL] expected " << expected << std::endl;
    ++failures;
    return;
  }
  check(label, *actual, expected);
}

template <typename T>
void check_null(const char* label, T actual)
{
  std::cout << label;
  if (!actual.has_value())
    std::cout << "(null)  [PASS]" << std::endl;
  else
  {
    std::cout << *actual << "  [FAIL] expected null" << std::endl;
    ++failures;
  }
}

void check_no_error(const char* label, std::error_code ec)
{
  std::cout << label;
  if (!ec)
    std::cout << "no error  [PASS]" << std::endl;
  else
  {
    std::cout << ec.message() << "  [FAIL] expected no error" << std::endl;
    ++failures;
  }
}

void check_has_error(const char* label, std::error_code ec)
{
  std::cout << label;
  if (ec)
    std::cout << ec.message() << "  [PASS]" << std::endl;
  else
  {
    std::cout << "no error  [FAIL] expected error" << std::endl;
    ++failures;
  }
}

//---------------------------------------------------------------------------
// compile-time only tests
//---------------------------------------------------------------------------
void test_waiver()
{
  policy p;
  (void) p;
}

void test_rational()
{
  constexpr rational a{std::numeric_limits<int>::min()};
  (void)a;
}

void test_comparision()
{
  static_assert
  (
    rational{-1} < rational{0} &&
    rational{0}  < rational{1} &&
    rational{-3, 2} < rational{-2, 3}
  );

  static_assert
  (
    rational{3,2} + rational{1,5} == rational{17,10} &&
    rational{3,2} - rational{1,5} == rational{13,10} &&
    rational{3,2} / rational{1,2} == rational{3} &&
    *(rational{3,2} * rational{1,2}) == rational{3, 4}
  );
}

void test_interval()
{
  constexpr interval point{*(3_r/4), *(3_r/4)};
  constexpr interval a{0,1};

  (void)point;
  (void)a;
}

void test_grid()
{
  static_assert
  (
    grid{{0,100}, 1}.max_notch() == 100 &&
    grid{{1,5}, 0.25}.max_notch() == 16 &&
    grid{{0,UINT64_MAX}, 1}.max_notch() == UINT64_MAX
  );
}

void test_rational_overflow()
{
  constexpr umax M = std::numeric_limits<umax>::max();

  // checked: returns nullopt at runtime when cross-multiplication overflows
  check_null("M*2:           ", rational{M} * rational{2});
  check_null("M+1:           ", rational{M} + rational{1});

  // checked /: cross-multiply overflow (lhs.Num * rhs_ad)
  check_null("M / (1/2):     ", rational{M} / rational{1, 2});

  // unchecked: well-defined when caller's no-overflow precondition holds
  static_assert(rational::mul_unchecked(rational{2}, rational{3}) == rational{6});
  static_assert(rational::add_unchecked(rational{2}, rational{3}) == rational{5});
  static_assert(rational::div_unchecked(rational{6}, rational{3}) == rational{2});

  // shape-coverage for the lift-based mixed-type overloads (no-overflow inputs)
  {
    slim::optional<int> a{4};
    auto r = a * rational{3};
    if (!r || *r != rational{12}) { std::cout << "rational opt*r [FAIL]\n"; ++failures; }
    else                          std::cout << "rational opt*r [PASS]\n";
  }
  {
    slim::optional<int> a{slim::nullopt};
    auto r = a + rational{3};
    if (r) { std::cout << "rational empty+r [FAIL]\n"; ++failures; }
    else   std::cout << "rational empty+r [PASS]\n";
  }
  {
    auto r = rational{10} / 4;
    if (!r || *r != rational{5, 2}) { std::cout << "rational r/int [FAIL]\n"; ++failures; }
    else                            std::cout << "rational r/int [PASS]\n";
  }
}

void test_lift()
{
  auto plus = [](int a, int b) { return a + b; };
  auto opt_div = [](int a, int b) -> slim::optional<int>
  { return (b == 0) ? slim::optional<int>{slim::nullopt} : slim::optional<int>{a / b}; };

  // (value, value) -> optional<int> wrapping the result
  static_assert(*lift(plus, 2, 3) == 5);

  // (optional, value) — engaged
  {
    slim::optional<int> a{2};
    auto r = lift(plus, a, 3);
    if (!r || *r != 5) { std::cout << "test_lift opt+val [FAIL]\n"; ++failures; }
    else                std::cout << "test_lift opt+val [PASS]\n";
  }

  // (optional, value) — empty -> nullopt
  {
    slim::optional<int> a{slim::nullopt};
    auto r = lift(plus, a, 3);
    if (r) { std::cout << "test_lift empty+val [FAIL]\n"; ++failures; }
    else   std::cout << "test_lift empty+val [PASS]\n";
  }

  // (value, optional) — empty
  {
    slim::optional<int> b{slim::nullopt};
    auto r = lift(plus, 2, b);
    if (r) { std::cout << "test_lift val+empty [FAIL]\n"; ++failures; }
    else   std::cout << "test_lift val+empty [PASS]\n";
  }

  // (optional, optional) — both engaged
  {
    slim::optional<int> a{2}, b{3};
    auto r = lift(plus, a, b);
    if (!r || *r != 5) { std::cout << "test_lift opt+opt [FAIL]\n"; ++failures; }
    else                std::cout << "test_lift opt+opt [PASS]\n";
  }

  // op already returns optional — auto-flatten, no double wrap
  {
    auto r = lift(opt_div, 6, 2);
    static_assert(std::is_same_v<decltype(r), slim::optional<int>>);
    if (!r || *r != 3) { std::cout << "test_lift flatten ok [FAIL]\n"; ++failures; }
    else                std::cout << "test_lift flatten ok [PASS]\n";
  }
  {
    auto r = lift(opt_div, 6, 0);
    if (r) { std::cout << "test_lift flatten nullopt [FAIL]\n"; ++failures; }
    else   std::cout << "test_lift flatten nullopt [PASS]\n";
  }
}

void test_trivial()
{
  // The non-checked variants stay fully trivial so they can sit in NTTPs,
  // unions, and memcpy buffers. The default `checked` policy gains a
  // user-provided default ctor that zero-inits Raw, so it loses trivial
  // default-construction but remains trivially copyable.
  static_assert(std::is_trivial_v<bound<{0, 0},        unsafe>>);
  static_assert(std::is_trivial_v<bound<{0, 100},      unsafe>>);
  static_assert(std::is_trivial_v<bound<{-100, 100},   unsafe>>);
  static_assert(std::is_trivial_v<bound<{{0, 10}, rational{1, 2}}, unsafe>>);
  static_assert(std::is_trivial_v<bound<{{-10, 10}, 0}, unsafe>>);
  static_assert(std::is_trivial_v<bound<{0, 100}, clamp>>);
  static_assert(std::is_trivial_v<bound<{0, 100}, wrap>>);
  static_assert(std::is_trivial_v<bound<{0, 100}, sentinel>>);

  // Default (`checked`) keeps trivial copy / destruction; only default
  // construction is now user-provided.
  static_assert(std::is_trivially_copyable_v<bound<{0, 100}>>);
  static_assert(std::is_trivially_destructible_v<bound<{0, 100}>>);
  static_assert(!std::is_trivially_default_constructible_v<bound<{0, 100}>>);
}

//---------------------------------------------------------------------------
// runtime checked tests
//---------------------------------------------------------------------------
void test_conversion()
{
  using f30t40 = bound<{{30, 40}, 2}>;
  using f20t50 = bound<{interval{20, 50}, 1}>;
  f30t40 smaller{34};
  f20t50 bigger;

  bigger = smaller;
  check("bigger = ", bigger, 34);
  bigger = 39;

  smaller.policy<ignore_round>() = bigger; // does round
  check("smaller(round) = ", smaller, 38);

  bigger = 30;
  smaller.with_round() = bigger;
  check("smaller(with_round) = ", smaller, 30);
}

void test_div()
{
  using r = bound<{1,255}>;
  std::error_code ec;
  r a{102, make_policy(ec)};
  check_no_error("construct a: ", ec);
  r b{16};

  auto c = a / b;
  check("a = ", a, 102);
  check("b = ", b, 16);
  check("a/b = ", c, *(51_r/8));

  // integer division via per-call policy (direct-storage bounds)
  using u100 = bound<{0, 100}>;
  u100 da{51}, db{8};
  auto d = div(da, db, make_policy<ignore_round>());
  static_assert(!std::is_same_v<typename decltype(d)::value_type::raw_type, rational>);
  check("idiv call = ", *d, 6);

  // integer division via type-level ignore_round
  using ui = bound<{0, 100}, ignore_round>;
  ui ai{51}, bi{8};
  auto e = ai / bi;
  static_assert(!std::is_same_v<typename decltype(e)::value_type::raw_type, rational>);
  check("idiv type = ", *e, 6);

  // exact integer division
  ui ai2{100}, bi2{10};
  check("idiv exact = ", *(ai2 / bi2), 10);

  // division by zero returns nullopt
  ui zero{0};
  check_null("idiv zero: ", ai / zero);

  // signed integer division
  using si = bound<{-100, 100}, ignore_round>;
  si sa{-7}, sb{2};
  auto g = sa / sb;
  static_assert(!std::is_same_v<typename decltype(g)::value_type::raw_type, rational>);
  check("idiv signed = ", *g, -3);

  // without ignore_round: still exact rational
  u100 an{7}, bn{2};
  auto h = an / bn;
  static_assert(std::is_same_v<typename decltype(h)::value_type::raw_type, rational>);
  check("div exact = ", *h, *(7_r/2));

  // offset-encoded: bound<{5,100}> has Lower=5, not direct storage
  using off = bound<{5, 100}>;
  static_assert(!is_direct_storage<off>);
  static_assert(is_notch_storage<off>);
  off oa{50}, ob{10};
  auto oc = div(oa, ob, make_policy<ignore_round>());
  static_assert(!std::is_same_v<typename decltype(oc)::value_type::raw_type, rational>);
  check("idiv offset = ", *oc, 5);

  // offset-encoded truncation
  off od{51}, oe{10};
  auto of_ = div(od, oe, make_policy<ignore_round>());
  check("idiv offset trunc = ", *of_, 5);

  // non-unit notch: bound<{{0,10},2}> has Notch=2
  using step2 = bound<{{0, 10}, 2}>;
  step2 sa2{10}, sb2{4};
  auto sc2 = div(sa2, sb2, make_policy<ignore_round>());
  static_assert(!std::is_same_v<typename decltype(sc2)::value_type::raw_type, rational>);
  check("idiv notch2 = ", *sc2, 2);

  // non-unit notch truncation: 10/6 = 1
  step2 sd2{10}, se2{6};
  auto sf2 = div(sd2, se2, make_policy<ignore_round>());
  check("idiv notch2 trunc = ", *sf2, 1);

  // offset + non-unit notch: bound<{{5,15},5}>
  using step5 = bound<{{5, 15}, 5}>;
  step5 s5a{15}, s5b{5};
  auto s5c = div(s5a, s5b, make_policy<ignore_round>());
  static_assert(!std::is_same_v<typename decltype(s5c)::value_type::raw_type, rational>);
  check("idiv notch5 = ", *s5c, 3);
}

void test_mul()
{
  using r = bound<{10,255, 1}>;
  r a{16};
  r b = 102;

  auto c = a * b;
  check("a = ", a, 16);
  check("b = ", b, 102);
  check("a*b = ", c, 1632);

  using u4 = bound<{{0.75, 10.5}, 0.25}>;

  u4 d{3};
  u4 e{3.25};

  auto f = d * e;
  check("d = ", d, 3);
  check("e = ", e, *(13_r/4));
  check("d*e = ", f, *(39_r/4));

  using n4 = u4::negative;

  auto g = n4(-2);
  auto h = n4(-2.25);

  check("g = ", g, -2);
  check("h = ", h, -(*(9_r/4)));
  auto i = g * h;
  check("g*h = ", i, *(9_r/2));

  auto j = d * g;
  check("d*g = ", j, -6);
  auto k = g * d;
  check("g*d = ", k, -6);

  using s = bound<{{-100, 10}, 1.0/16}>;
  using n = s::negative;

  auto l = s(-70.125);
  auto m = n(1.125);

  check("l = ", l, -(*(561_r/8)));
  check("m = ", m, *(9_r/8));
  check("l*m = ", l*m, -(*(5049_r/64)));
  check("m*l = ", m*l, -(*(5049_r/64)));
}

void test_add()
{
  using u8 = bound<{10,255}>;
  u8 a{16};
  u8 b = 220;
  check("b = ", b, 220);
  check("just<1> = ", just<1>, 1);
  auto b1{b + just<1>};
  check("b+just<1> = ", b1, 221);

  b = a + b1;
  check("a = ", a, 16);
  check("a+b1 = ", b, 237);

  auto d = a - b;
  check("a-b = ", d, -221);

  using u64 = bound<{{0, std::numeric_limits<std::uint64_t>::max()}, 1}>;

  constexpr u64 biggest{std::numeric_limits<std::uint64_t>::max()};
  check("biggest = ", biggest, rational{std::numeric_limits<std::uint64_t>::max()});
}

void test_bound()
{
  using frac = bound<{{-10, 10}, 0}>;
  static_assert(Grid<frac> == grid{-10, 10, 0});
  static_assert(std::is_same_v<typename frac::raw_type, rational>);

  constexpr frac f = *(2_r/3);
  check("frac f = ", f, *(2_r/3));

  using step = bound<{{-5, 5}, 0.5}>;
  static_assert(not std::is_same_v<step::raw_type, rational>);
  constexpr step s = *(3_r/2);
  check("step s = ", s, *(3_r/2));
}

void test_optional_ops()
{
  // Integer storage — add/mul return bare bound, overloads wrap to optional
  using u8 = bound<{1,255}>;
  u8 a{100};
  u8 b{10};
  slim::optional<u8> opt_a{a};
  slim::optional<u8> opt_b{b};
  slim::optional<u8> no_val{slim::nullopt};

  // +
  check("opt+bare: ", opt_a + b, 110);
  check("bare+opt: ", a + opt_b, 110);
  check("opt+opt:  ", opt_a + opt_b, 110);
  check_null("null+bare: ", no_val + b);
  check_null("bare+null: ", a + no_val);
  check_null("opt+null:  ", opt_a + no_val);
  // -
  check("opt-bare: ", opt_a - b, 90);
  check("bare-opt: ", a - opt_b, 90);
  check("opt-opt:  ", opt_a - opt_b, 90);
  check_null("null-bare: ", no_val - b);
  check_null("bare-null: ", a - no_val);
  check_null("opt-null:  ", opt_a - no_val);
  // *
  check("opt*bare: ", opt_a * b, 1000);
  check("bare*opt: ", a * opt_b, 1000);
  check("opt*opt:  ", opt_a * opt_b, 1000);
  check_null("null*bare: ", no_val * b);
  check_null("bare*null: ", a * no_val);
  check_null("opt*null:  ", opt_a * no_val);
  // /
  check("opt/bare: ", opt_a / b, 10);
  check("bare/opt: ", a / opt_b, 10);
  check("opt/opt:  ", opt_a / opt_b, 10);
  check_null("null/bare: ", no_val / b);
  check_null("bare/null: ", a / no_val);
  check_null("opt/null:  ", opt_a / no_val);

  // Raw-rational storage — add/mul return slim::optional<bound>, tests no double-wrap
  using frac = bound<{{-10, 10}, 0}>;
  frac f1 = *(2_r/3);
  frac f2 = *(1_r/3);
  slim::optional<frac> opt_f1{f1};
  slim::optional<frac> opt_f2{f2};
  slim::optional<frac> no_frac{slim::nullopt};

  // +
  check("frac opt+bare: ", opt_f1 + f2, 1);
  check("frac bare+opt: ", f1 + opt_f2, 1);
  check("frac opt+opt:  ", opt_f1 + opt_f2, 1);
  check_null("frac null+bare: ", no_frac + f2);
  check_null("frac bare+null: ", f1 + no_frac);
  check_null("frac opt+null:  ", opt_f1 + no_frac);
  // -
  check("frac opt-bare: ", opt_f1 - f2, *(1_r/3));
  check("frac bare-opt: ", f1 - opt_f2, *(1_r/3));
  check("frac opt-opt:  ", opt_f1 - opt_f2, *(1_r/3));
  check_null("frac null-bare: ", no_frac - f2);
  check_null("frac bare-null: ", f1 - no_frac);
  check_null("frac opt-null:  ", opt_f1 - no_frac);
  // *
  check("frac opt*bare: ", opt_f1 * f2, *(2_r/9));
  check("frac bare*opt: ", f1 * opt_f2, *(2_r/9));
  check("frac opt*opt:  ", opt_f1 * opt_f2, *(2_r/9));
  check_null("frac null*bare: ", no_frac * f2);
  check_null("frac bare*null: ", f1 * no_frac);
  check_null("frac opt*null:  ", opt_f1 * no_frac);
  // / (use non-zero interval for division)
  using pfrac = bound<{{1, 10}, 0}>;
  pfrac p1 = 3;
  pfrac p2 = 2;
  slim::optional<pfrac> opt_p1{p1};
  slim::optional<pfrac> opt_p2{p2};
  slim::optional<pfrac> no_pfrac{slim::nullopt};

  check("frac opt/bare: ", opt_p1 / p2, *(3_r/2));
  check("frac bare/opt: ", p1 / opt_p2, *(3_r/2));
  check("frac opt/opt:  ", opt_p1 / opt_p2, *(3_r/2));
  check_null("frac null/bare: ", no_pfrac / p2);
  check_null("frac bare/null: ", p1 / no_pfrac);
  check_null("frac opt/null:  ", opt_p1 / no_pfrac);
}

void test_clamp()
{
  using pct = bound<{0, 100}, clamp>;
  pct x = 150;
  check("clamp over: ", x, 100);
  pct y = -5;
  check("clamp under: ", y, 0);
  pct z = 50;
  check("clamp ok: ", z, 50);

  // clamp on operator+=
  pct a = 90;
  a += bound<{0, 100}>(20);
  check("clamp +=: ", a, 100);
}

void test_wrap()
{
  using angle = bound<{0, 359}, wrap>;
  angle a = 370;
  check("wrap over: ", a, 10);
  angle b = -10;
  check("wrap under: ", b, 350);
  angle c = 360;
  check("wrap 360: ", c, 0);
  angle d = 180;
  check("wrap ok: ", d, 180);
}

void test_action()
{
  using sec = bound<{0, 59}, wrap>;
  using min_t = bound<{0, 59}>;
  sec seconds(0);
  min_t minutes(0);

  seconds.policy<wrap>([&](auto carry) {
    minutes = static_cast<int>(static_cast<rational>(minutes).Numerator) + static_cast<int>(carry);
  }) = 65;
  check("action sec: ", seconds, 5);
  check("action min: ", minutes, 1);

  // action with clamp
  using pct = bound<{0, 100}, clamp>;
  pct clamped(0);
  imax excess = 0;
  clamped.policy<clamp>([&](auto e) { excess = static_cast<imax>(e); }) = 150;
  check("action clamp val: ", clamped, 100);
  std::cout << "action clamp excess: " << excess;
  if (excess == 50)
    std::cout << "  [PASS]" << std::endl;
  else
  {
    std::cout << "  [FAIL] expected 50" << std::endl;
    ++failures;
  }
}

void test_clamp_boundable()
{
  // clamp when assigning from a wider bound to a narrower clamped bound
  using wide = bound<{0, 200}>;
  using narrow = bound<{0, 100}, clamp>;
  wide w = 150;
  narrow n(0);
  n = w;
  check("clamp b2b: ", n, 100);
}

void test_with_clamp_wrap()
{
  // per-operation clamp/wrap via with_clamp()/with_wrap()
  using u100 = bound<{0, 100}>;
  u100 x(50);
  x.with_clamp() = 150;
  check("with_clamp: ", x, 100);
  x.with_wrap() = 103;
  check("with_wrap: ", x, 2);
}

void test_on_actions()
{
  using u100 = bound<{0, 100}>;
  using sec  = bound<{0, 59}, wrap>;

  // on_wrap — handler receives bound& and carry; here it cascades the carry.
  sec seconds(0);
  imax carry_seen = 0;
  seconds.on_wrap([&](auto& self, auto carry) {
    carry_seen = static_cast<imax>(carry);
    (void)self;
  }) = 65;
  check("on_wrap value: ", seconds, 5);
  if (carry_seen == 1) std::cout << "on_wrap carry: 1  [PASS]" << std::endl;
  else { std::cout << "on_wrap carry: " << carry_seen << "  [FAIL] expected 1" << std::endl; ++failures; }

  // on_wrap — handler overrides the wrapped value.
  sec s2(0);
  s2.on_wrap([](auto& self, auto carry) {
    if (carry > 0) self = 0;
  }) = 65;
  check("on_wrap override: ", s2, 0);

  // on_clamp — handler receives bound& and overshoot.
  u100 clamped(0);
  imax over_seen = 0;
  clamped.on_clamp([&](auto& self, auto over) {
    over_seen = static_cast<imax>(over);
    (void)self;
  }) = 150;
  check("on_clamp value: ", clamped, 100);
  if (over_seen == 50) std::cout << "on_clamp over: 50  [PASS]" << std::endl;
  else { std::cout << "on_clamp over: " << over_seen << "  [FAIL] expected 50" << std::endl; ++failures; }

  // on_error — fires instead of throw, may overwrite to a recovery value.
  using c100 = bound<{0, 100}, checked>;
  c100 e(50);
  bool err_fired = false;
  e.on_error([&](auto& self, errc code, std::string_view msg) {
    err_fired = (code == errc::domain_error) && !msg.empty();
    self = 0;
  }) = 200;
  check("on_error value: ", e, 0);
  std::cout << "on_error fired: " << (err_fired ? "yes" : "no")
            << (err_fired ? "  [PASS]" : "  [FAIL]") << std::endl;
  if (!err_fired) ++failures;

  // on_sentinel — fires after sentinel raw is set; handler may overwrite.
  using s100 = bound<{0, 100}, sentinel>;
  s100 sv(50);
  imax orig_seen = 0;
  sv.on_sentinel([&](auto& self, auto orig) {
    orig_seen = static_cast<imax>(orig);
    self = 50;  // recover to a default
  }) = 200;
  check("on_sentinel value: ", sv, 50);
  if (orig_seen == 200) std::cout << "on_sentinel orig: 200  [PASS]" << std::endl;
  else { std::cout << "on_sentinel orig: " << orig_seen << "  [FAIL] expected 200" << std::endl; ++failures; }

  // on_overflow — fires when checked compound op overflows imax.
  c100 acc(50);
  bool of_fired = false;
  acc.on_overflow([&](auto& self, errc code) {
    of_fired = (code == errc::overflow);
    self = 0;
  }) += std::numeric_limits<imax>::max();
  check("on_overflow value: ", acc, 0);
  std::cout << "on_overflow fired: " << (of_fired ? "yes" : "no")
            << (of_fired ? "  [PASS]" : "  [FAIL]") << std::endl;
  if (!of_fired) ++failures;

  // back-compat: existing policy<wrap>(lambda) form still works.
  sec s3(0);
  imax legacy_carry = 0;
  s3.policy<wrap>([&](auto c) { legacy_carry = static_cast<imax>(c); }) = 65;
  check("legacy wrap value: ", s3, 5);
  if (legacy_carry == 1) std::cout << "legacy wrap carry: 1  [PASS]" << std::endl;
  else { std::cout << "legacy wrap carry: " << legacy_carry << "  [FAIL] expected 1" << std::endl; ++failures; }
}

void test_unsafe_flag()
{
  // unsafe implies ignore_round: notch-incompatible assigns compile.
  using src    = bound<{{0, 100}, 2}, unsafe>;     // notch=2
  using dst    = bound<{{0, 100}, 1}, unsafe>;     // notch=1 — incompatible factor
  src s{50};
  dst d{0};
  d = s;  // would fail to compile under plain `none` (factor non-integral, no ignore_round)
  check("unsafe assign: ", d, 50);

  // unsafe implies ignore_round: native-int div path engages (result is integer-storage).
  using u100u = bound<{0, 100}, unsafe>;
  u100u a{51}, b{8};
  auto q = a / b;
  static_assert(!std::is_same_v<typename decltype(q)::value_type::raw_type, rational>,
                "unsafe should take native-int div path");
  check("unsafe int div: ", *q, 6);

  // unsafe binary div / mod with zero returns nullopt (not crash).
  u100u zero{0};
  auto qz = a / zero;
  if (!qz.has_value()) std::cout << "unsafe a/0 nullopt: PASS" << std::endl;
  else { std::cout << "unsafe a/0 nullopt: FAIL" << std::endl; ++failures; }

  // unsafe compound /= with zero still reports (ignore_zero NOT set in unsafe).
  bool threw = false;
  try { u100u y{50}; y /= 0; (void)y; }
  catch (std::system_error&) { threw = true; }
  std::cout << "unsafe /=0 throws: " << (threw ? "yes  [PASS]" : "no  [FAIL]") << std::endl;
  if (!threw) ++failures;

  // unsafe disables domain check on assignment (silent overwrite).
  u100u x{50};
  x = 200;  // out of range, no throw
  // value is whatever raw_type stores; we just confirm no throw happened
  std::cout << "unsafe oor silent: PASS" << std::endl;
  (void)x;
}

void test_on_overflow_freefn()
{
  // add overflow on rational-storage bound: action fires, returns recovery value.
  // Use disparate large denominators so cross-multiplication overflows even
  // though numerators / values are small. Result interval [0, 2] fits; the
  // overflow is internal to rational::add.
  constexpr umax M = std::numeric_limits<umax>::max();
  using unit = bound<{{rational{0}, rational{1}}, 0}, checked>;  // raw_rational, [0,1]
  unit a, b;
  a.Raw = rational{1, static_cast<imax>(M / 2)};       // 1 / (M/2)
  b.Raw = rational{1, static_cast<imax>(M / 2 - 1)};   // 1 / (M/2 - 1)  → cross-mul overflows
  bool fired = false;
  errc seen{};
  auto sum = add(a, b, make_policy<checked>(),
    on_overflow_t{[&](auto& res, errc code) {
      fired = true; seen = code;
      res.Raw = rational{0};
    }});
  // Return type collapsed to a plain bound (no .has_value()); a static_assert
  // confirming this would require naming the result type, which is awkward
  // across overload chains, so we rely on the runtime evidence that the action
  // fired and produced a recovery value.
  std::cout << "on_overflow add fired: " << (fired ? "yes" : "no")
            << (fired ? "  [PASS]" : "  [FAIL]") << std::endl;
  if (!fired) ++failures;
  if (seen == errc::overflow) std::cout << "on_overflow add code: PASS" << std::endl;
  else { std::cout << "on_overflow add code: FAIL" << std::endl; ++failures; }
  (void)sum;

  // div-by-zero through free div() with on_overflow action.
  using u100c = bound<{0, 100}, checked>;
  u100c d{50}, z{0};
  bool dz_fired = false;
  errc dz_code{};
  auto q = div(d, z, make_policy<checked>(),
    on_overflow_t{[&](auto& res, errc c) {
      dz_fired = true; dz_code = c;
      res = std::remove_cvref_t<decltype(res)>{0};
    }});
  // Same observation as for `add` — runtime evidence is enough.
  std::cout << "on_overflow div/0 fired: "
            << (dz_fired && dz_code == errc::division_by_zero ? "yes  [PASS]" : "no  [FAIL]")
            << std::endl;
  if (!dz_fired) ++failures;

  // mod-by-zero with on_overflow.
  using u100ic = bound<{0, 100}, checked | ignore_round>;  // mod requires ignore_round
  u100ic ml{7}, mr{0};
  bool mz_fired = false;
  auto m = mod(ml, mr, make_policy<checked>(),
    on_overflow_t{[&](auto& res, errc) { mz_fired = true; res = std::remove_cvref_t<decltype(res)>{0}; }});
  (void)m;
  std::cout << "on_overflow mod/0 fired: " << (mz_fired ? "yes  [PASS]" : "no  [FAIL]") << std::endl;
  if (!mz_fired) ++failures;

  // No-action default still produces optional and returns nullopt on div/0.
  auto q_def = div(d, z);
  if (!q_def.has_value()) std::cout << "no-action div/0 nullopt: PASS" << std::endl;
  else { std::cout << "no-action div/0 nullopt: FAIL" << std::endl; ++failures; }
}

void test_signed()
{
  // --- type selection ---
  using s8  = bound<{-127, 127}>;
  using s16 = bound<{-32000, 32000}>;
  using s32 = bound<{-100000, 100000}>;
  static_assert(std::is_same_v<typename s8::raw_type,  std::int8_t>);
  static_assert(std::is_same_v<typename s16::raw_type, std::int16_t>);
  static_assert(std::is_same_v<typename s32::raw_type, std::int32_t>);

  // fractional notch with negative lower stays unsigned
  using frac_neg = bound<{{-10, 10}, 0.25}>;
  static_assert(std::is_unsigned_v<typename frac_neg::raw_type>);

  // --- construction ---
  s32 a(42);
  check("signed 42: ", a, 42);
  s32 b(-300);
  check("signed -300: ", b, -300);
  s8 c(-127); // min usable (-128 is sentinel)
  check("signed -127: ", c, -127);

  // --- negation ---
  auto neg_a = -a;
  check("signed -42: ", neg_a, -42);
  auto neg_b = -b;
  check("signed 300: ", neg_b, 300);

  // --- addition ---
  auto sum = a + b;
  check("signed 42+(-300): ", sum, -258);
  auto diff = a - b;
  check("signed 42-(-300): ", diff, 342);

  // --- multiplication ---
  s32 x(-7), y(14);
  auto prod = x * y;
  check("signed -7*14: ", prod, -98);
  auto prod2 = y * x;
  check("signed 14*-7: ", prod2, -98);

  // --- division ---
  auto quot = x / y;
  check("signed -7/14: ", *quot, -(*(1_r/2)));

  // --- operator+=(bound) ---
  s32 acc(100);
  s32 delta(-30);
  acc += delta;
  check("signed +=bound: ", acc, 70);

  // --- operator+=(int) ---
  s32 acc2(50);
  acc2 += -75;
  check("signed +=int: ", acc2, -25);

  // --- mixed: signed + unsigned ---
  using u100 = bound<{0, 100}>;
  u100 u(80);
  s32 s(-500);
  auto mixed = u + s;
  check("signed+unsigned: ", mixed, -420);

  // --- mixed: unsigned - unsigned → signed result ---
  using u8 = bound<{0, 255}>;
  u8 p(10), q(200);
  auto d = p - q;
  check("unsigned sub: ", d, -190);

  // --- clamp ---
  using sc = bound<{-100, 100}, clamp>;
  sc clamped_over = 200;
  check("signed clamp over: ", clamped_over, 100);
  sc clamped_under = -200;
  check("signed clamp under: ", clamped_under, -100);

  // --- wrap ---
  using sw = bound<{-100, 100}, wrap>;
  sw wrapped = 150;
  check("signed wrap over: ", wrapped, -51);
  sw wrapped2 = -150;
  check("signed wrap under: ", wrapped2, 51);

  // --- optional/sentinel ---
  slim::optional<s32> opt_a{s32(42)};
  slim::optional<s32> opt_none{slim::nullopt};
  check("signed opt has: ", *opt_a, 42);
  std::cout << "signed opt null: ";
  if (!opt_none.has_value())
    std::cout << "(null)  [PASS]" << std::endl;
  else
  {
    std::cout << "[FAIL] expected null" << std::endl;
    ++failures;
  }

  // sentinel doesn't collide with valid negative values
  s8 min_val(-127);
  slim::optional<s8> opt_min{min_val};
  check("signed opt min: ", *opt_min, -127);

  // optional arithmetic
  auto opt_sum = opt_a + s32(-100);
  check("signed opt add: ", opt_sum, -58);
}

void test_round_check()
{
  using n1 = bound<{{0, 10}, 1}>;
  using n2 = bound<{{0, 10}, 2}>;

  // Compatible notches: notch 2 -> notch 1 (every notch-2 value fits notch-1 grid)
  n1 a;
  a = n2(6);
  check("round compatible: ", a, 6);

  // Incompatible notches: notch 1 -> notch 2 requires with_round()
  n2 b;
  b.with_round() = n1(3);
  check("round with_round: ", b, 2);

  // policy<ignore_round>() also works
  b.policy<ignore_round>() = n1(5);
  check("round policy: ", b, 4);

  // Exact value through with_round() preserves it
  b.with_round() = n1(4);
  check("round exact: ", b, 4);

  // Type-level ignore_round: no opt-in needed per operation
  using n2_round = bound<{{0, 10}, 2}, ignore_round>;
  n2_round c;
  c = n1(3);
  check("round type-level: ", c, 2);

  // Different intervals, compatible notches: no opt-in needed
  using wide = bound<{{0, 20}, 2}>;
  b = wide(6);
  check("round wide compatible: ", b, 6);
}

void test_comparison()
{
  using u100 = bound<{0, 100}>;
  using u50 = bound<{0, 50}>;

  u100 a{30}, b{50};
  u50  c{30};

  // same-type comparison
  std::cout << "cmp a<b: ";
  if (a < b) std::cout << "[PASS]" << std::endl;
  else { std::cout << "[FAIL]" << std::endl; ++failures; }

  std::cout << "cmp a==c: ";
  if (a == c) std::cout << "[PASS]" << std::endl;
  else { std::cout << "[FAIL]" << std::endl; ++failures; }

  // bound vs arithmetic
  std::cout << "cmp a==30: ";
  if (a == 30) std::cout << "[PASS]" << std::endl;
  else { std::cout << "[FAIL]" << std::endl; ++failures; }

  std::cout << "cmp b>25: ";
  if (b > 25) std::cout << "[PASS]" << std::endl;
  else { std::cout << "[FAIL]" << std::endl; ++failures; }

  // signed comparison
  using s100 = bound<{-100, 100}>;
  s100 neg{-30}, pos{30};
  std::cout << "cmp neg<pos: ";
  if (neg < pos) std::cout << "[PASS]" << std::endl;
  else { std::cout << "[FAIL]" << std::endl; ++failures; }
}

void test_compound_assign()
{
  using u100 = bound<{0, 100}>;
  u100 a{50};
  a -= 10;
  check("a -= 10: ", a, 40);

  a *= 2;
  check("a *= 2: ", a, 80);

  // -= with bound
  u100 delta{20};
  a -= delta;
  check("a -= bound: ", a, 60);

  // /=
  a /= 3;
  check("a /= 3: ", a, 20);

  a /= 2;
  check("a /= 2: ", a, 10);

  // %=
  u100 b{17};
  b %= 5;
  check("b %= 5: ", b, 2);

  u100 c{100};
  c %= 10;
  check("c %= 10: ", c, 0);

  // boundable RHS for *=, /=, %=
  using uir = bound<{0, 100}, ignore_round>;
  uir d{50}, two{2};
  d *= two;
  check("d *= bound{2}: ", d, 100);

  uir e{60}, three{3};
  e /= three;
  check("e /= bound{3}: ", e, 20);

  uir f{17}, five{5};
  f %= five;
  check("f %= bound{5}: ", f, 2);
}

void test_modulo()
{
  using ui = bound<{0, 100}, ignore_round>;
  ui a{17}, b{5};
  auto r = a % b;
  check("17 % 5 = ", *r, 2);

  ui c{100}, d{10};
  auto r2 = c % d;
  check("100 % 10 = ", *r2, 0);

  // modulo by zero returns nullopt
  ui zero{0};
  check_null("mod zero: ", a % zero);

  // signed modulo
  using si = bound<{-100, 100}, ignore_round>;
  si sa{-17}, sb{5};
  auto r3 = sa % sb;
  check("-17 % 5 = ", *r3, -2);

  // mod free function
  using u50 = bound<{0, 50}>;
  u50 e{23}, f{7};
  auto r4 = mod(e, f, make_policy<ignore_round>());
  check("mod(23,7) = ", *r4, 2);
}

//---------------------------------------------------------------------------
// compile-time type alias tests
//---------------------------------------------------------------------------
using test0_t = bound<{{1,3},1}>;
using test1_t = bound<{{0, {1u, -1}}, -1}>; // fails on instantiation
//using test2_t = bound<{1,1}>; // zero numerator normalization test
//using test3_t = bound<{1,0}>;
using test4_t = bound<{{0, std::numeric_limits<umax>::max()}, 1}>;
using test5_t = bound<{1_r}>;
using test6_t = bound<{*(-6_r/(1 << 4))}>;
using test7_t = bound<{-6.0/(1 << 4)}>;
using test8_t = bound<{{-10.0, 10.0}, 0.25}>;
//using test9_t = bound<{3,1}>;  // fails on instantiation
using test10_t = bound<{{1,100}, *(3_r/2)}>;
//using test11_t = bound<{1,5}>;

static_assert(std::is_same_v<typename test0_t::raw_type, std::uint8_t>);
static_assert(std::is_same_v<typename test4_t::raw_type, std::uint64_t>);

static_assert(std::is_same_v<typename test5_t::raw_type, rational>);
//static_assert(std::is_same_v<test11_t::raw_type, rational>);

//---------------------------------------------------------------------------
// constexpr arithmetic tests
//---------------------------------------------------------------------------
namespace constexpr_tests
{
  // addition
  using u100 = bound<{0, 100}>;
  constexpr u100 ca{30}, cb{20};
  static_assert(static_cast<rational>(ca + cb) == 50);

  // subtraction (result is signed)
  static_assert(static_cast<rational>(ca - cb) == 10);
  static_assert(static_cast<rational>(cb - ca) == -10);

  // multiplication
  using u10 = bound<{1, 10}>;
  constexpr u10 cm{3}, cn{7};
  static_assert(static_cast<rational>(cm * cn) == 21);

  // negation
  static_assert(static_cast<rational>(-ca) == -30);

  // signed arithmetic
  using s50 = bound<{-50, 50}>;
  constexpr s50 sa{-20}, sb{30};
  static_assert(static_cast<rational>(sa + sb) == 10);
  static_assert(static_cast<rational>(sa - sb) == -50);
  static_assert(static_cast<rational>(sa * sb) == -600);

  // comparison
  static_assert(ca > cb);
  static_assert(cb < ca);
  static_assert(ca == 30);
  static_assert(cb != 30);
  static_assert(ca >= 30);
  static_assert(cb <= 30);

  // cross-grid comparison
  constexpr u10 small{5};
  constexpr u100 big{50};
  static_assert(small < big);
  static_assert(big > small);

  // just
  static_assert(static_cast<rational>(just<42>) == 42);
}

void test_round_nearest()
{
  // Type-level round_nearest: 21.4 rounds to 21.5 (nearest 0.5 step)
  using celsius = bound<{{-40, 60}, 0.5}, round_nearest>;
  celsius room = 21.4;
  check("round_nearest 21.4: ", room, rational{43, 2}); // 21.5

  celsius exact = 21.5;
  check("round_nearest 21.5: ", exact, rational{43, 2}); // exact

  celsius trunc_case = 21.2;
  check("round_nearest 21.2: ", trunc_case, rational{21}); // 21.0 (nearest)

  // Per-operation round_nearest via with_round_nearest()
  using half_step = bound<{{0, 10}, 0.5}>;
  half_step h;
  h.with_round_nearest() = 3.3;
  check("with_round_nearest 3.3: ", h, rational{7, 2}); // 3.5

  h.with_round_nearest() = 3.2;
  check("with_round_nearest 3.2: ", h, rational{3}); // 3.0

  // Without round_nearest: truncation (existing behavior)
  using half_trunc = bound<{{0, 10}, 0.5}, ignore_round>;
  half_trunc t = 3.7;
  check("truncate 3.7: ", t, rational{7, 2}); // 3.5 (truncated from 7.4 -> 7)
}

void check_str(const char* label, std::string actual, std::string expected)
{
  std::cout << label << actual;
  if (actual == expected)
    std::cout << "  [PASS]" << std::endl;
  else
  {
    std::cout << "  [FAIL] expected " << expected << std::endl;
    ++failures;
  }
}

void test_to_string_format()
{
  // Power-of-2 denominators -> decimal
  check_str("fmt 1/2: ",   bnd::to_string(rational{1, 2}),  "0.5");
  check_str("fmt 43/2: ",  bnd::to_string(rational{43, 2}), "21.5");
  check_str("fmt 1/4: ",   bnd::to_string(rational{1, 4}),  "0.25");
  check_str("fmt 3/8: ",   bnd::to_string(rational{3, 8}),  "0.375");

  // Power-of-10 denominators -> decimal
  check_str("fmt 1/10: ",  bnd::to_string(rational{1, 10}), "0.1");
  check_str("fmt 15/100: ",bnd::to_string(rational{15, 100}), "0.15");

  // Negative decimals
  check_str("fmt -1/2: ",  bnd::to_string(rational{1, -2}), "-0.5");
  check_str("fmt -43/2: ", bnd::to_string(rational{43, -2}), "-21.5");

  // Non-decimal fractions -> mixed number
  check_str("fmt 7/3: ",   bnd::to_string(rational{7, 3}),  "2 1/3");
  check_str("fmt 22/7: ",  bnd::to_string(rational{22, 7}), "3 1/7");

  // Proper fractions stay as-is
  check_str("fmt 1/3: ",   bnd::to_string(rational{1, 3}),  "1/3");
  check_str("fmt 2/7: ",   bnd::to_string(rational{2, 7}),  "2/7");

  // Integer
  check_str("fmt 5/1: ",   bnd::to_string(rational{5, 1}),  "5");

  // Negative fractions -> mixed number
  check_str("fmt -7/3: ",  bnd::to_string(rational{7, -3}), "-2 1/3");
  check_str("fmt -1/3: ",  bnd::to_string(rational{1, -3}), "-1/3");

  // Overflow boundary: numerator * scale would wrap -> fall back to fraction
  // form. UINT64_MAX is odd so {M, 2} is irreducible; {M/5, 2} fits exactly
  // (M/5 * 5 = M, no overflow).
  constexpr umax M = std::numeric_limits<umax>::max();
  check_str("fmt M/2: ",     bnd::to_string(rational{M, 2}),
            "9223372036854775807 1/2");
  check_str("fmt -M/2: ",    bnd::to_string(rational{M, -2}),
            "-9223372036854775807 1/2");
  check_str("fmt (M/5)/2: ", bnd::to_string(rational{M / 5, 2}),
            "1844674407370955161.5");
}

void test_std_formatter()
{
  check_str("std::format bound: ",   std::format("{}", bound<{0,99}>{42}), "42");
  check_str("std::format pad: ",     std::format("[{:>6}]", bound<{0,99}>{42}), "[    42]");
  check_str("std::format rational: ",std::format("{}", rational{1, 2}), "0.5");
}

void test_sentinel()
{
  using idx = bound<{0, 9}, sentinel>;

  // Normal construction
  idx a = 5;
  check("sentinel normal: ", a, 5);

  // Overflow -> sentinel (nullopt)
  slim::optional<idx> opt = 9;
  ++opt;
  std::cout << "sentinel overflow: " << (opt.has_value() ? "has_value" : "nullopt");
  if (!opt.has_value())
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL]" << std::endl; ++failures; }

  // Underflow -> sentinel
  slim::optional<idx> opt2 = 0;
  --opt2;
  std::cout << "sentinel underflow: " << (opt2.has_value() ? "has_value" : "nullopt");
  if (!opt2.has_value())
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL]" << std::endl; ++failures; }

  // In-range increment stays valid
  slim::optional<idx> opt3 = 5;
  ++opt3;
  std::cout << "sentinel in-range: " << (opt3.has_value() ? "has_value" : "nullopt");
  if (opt3.has_value() && *opt3 == 6)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL]" << std::endl; ++failures; }
}

void test_increment_decrement()
{
  using u10 = bound<{0, 10}>;
  u10 a = 5;

  ++a;
  check("pre++: ", a, 6);

  a++;
  check("post++: ", a, 7);

  --a;
  check("pre--: ", a, 6);

  a--;
  check("post--: ", a, 5);
}

void test_implicit_cast()
{
  // Direct storage: bound<{0,9}> -> imax
  using idx = bound<{0, 9}>;
  idx a = 5;
  imax val = a;
  std::cout << "implicit cast direct: " << val;
  if (val == 5)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL]" << std::endl; ++failures; }

  // Offset storage: bound<{1,10}> -> imax
  using idx2 = bound<{1, 10}>;
  idx2 b = 7;
  imax val2 = b;
  std::cout << "implicit cast offset: " << val2;
  if (val2 == 7)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL]" << std::endl; ++failures; }

  // Array subscript without cast
  int arr[] = {10, 20, 30, 40, 50};
  using ai = bound<{0, 4}>;
  ai c = 3;
  std::cout << "arr[bound]: " << arr[c];
  if (arr[c] == 40)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL]" << std::endl; ++failures; }
}

void test_bound_range()
{
  // Sequential range
  int count = 0;
  imax sum = 0;
  for (auto i : bound_range<{0, 9}>{})
  {
    sum += static_cast<imax>(i);
    ++count;
  }
  std::cout << "range seq count: " << count;
  if (count == 10)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL] expected 10" << std::endl; ++failures; }

  std::cout << "range seq sum: " << sum;
  if (sum == 45)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL] expected 45" << std::endl; ++failures; }

  // Wrapping range from 7
  count = 0;
  imax first = -1, last = -1;
  for (auto i : bound_range<{0, 9}>{7})
  {
    if (count == 0) first = static_cast<imax>(i);
    last = static_cast<imax>(i);
    ++count;
  }
  std::cout << "range wrap count: " << count;
  if (count == 10)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL] expected 10" << std::endl; ++failures; }

  std::cout << "range wrap first: " << first;
  if (first == 7)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL] expected 7" << std::endl; ++failures; }

  std::cout << "range wrap last: " << last;
  if (last == 6)
    std::cout << "  [PASS]" << std::endl;
  else
  { std::cout << "  [FAIL] expected 6" << std::endl; ++failures; }
}

int main()
{
  try
  {
    test_conversion();
    test_comparision();
    test_add();
    test_bound();
    test_mul();
    test_div();
    test_optional_ops();
    test_waiver();
    test_clamp();
    test_wrap();
    test_action();
    test_clamp_boundable();
    test_with_clamp_wrap();
    test_on_actions();
    test_unsafe_flag();
    test_on_overflow_freefn();
    test_signed();
    test_round_check();
    test_comparison();
    test_compound_assign();
    test_modulo();

    test_lift();
    test_rational_overflow();
    test_trivial();
    test_round_nearest();
    test_to_string_format();
    test_std_formatter();
    test_sentinel();
    test_increment_decrement();
    test_implicit_cast();
    test_bound_range();

    bound b;
    (void)b;
  }
  catch(std::system_error& e)
  {
    std::cout << "EXCEPTION: code:    [" << e.code() << "]\n"
                 "           message: [" << e.code().message() << "]\n"
                 "           what:    [" << e.what() << "]\n";
    return 1;
  }
  catch(std::exception& e)
  {
    std::cout << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
  catch(char const* str)
  {
    std::cout << "EXCEPTION: " << str << std::endl;
    return 1;
  }

  std::cout << "\n" << (failures ? "FAILED" : "PASSED")
            << " (" << failures << " failure" << (failures == 1 ? "" : "s") << ")" << std::endl;
  return failures;
}
