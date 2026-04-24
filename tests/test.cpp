#include <iostream>
#include <limits>

#include "bound/generic.hpp"
#include "bound/bound.hpp"
#include "bound/interval.hpp"
#include "bound/print.hpp"

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
void noop_domain_error(const char*) { }

constexpr handler handlers =
{
  .domain_error = noop_domain_error
};

template<handler H>
class testHandler {};

void test_handler()
{
  testHandler<handlers> th;
  (void)th;
}

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
    test_signed();
    test_round_check();

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
