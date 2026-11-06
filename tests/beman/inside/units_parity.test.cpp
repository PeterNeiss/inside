// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Parity between inside and quantity: every public function of
// beman::inside, and every public member function of an inside, either has a
// quantity form (checked below with a requires-expression) or is listed with
// the reason it has none. Reflection lists the functions, so a new inside
// function without either fails this test.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/mdspan.hpp>
#include <beman/inside/inside/random.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/mdspan.hpp>
#include <beman/inside/units/random.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <meta>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
using num     = inside<{0, 100}>;
using metres  = quantity<si::metre, num>;
using km      = quantity<si::kilo(si::metre), inside<{0, 10}>>;
using seconds = quantity<si::second, inside<{1, 10}>>;
using ratio   = quantity<percent, inside<{0, 100}>>;

// Functions with a quantity form, each checked by a requires-expression.
constexpr std::array with_quantity_form{
    std::string_view{"add"},
    std::string_view{"add_all"},
    std::string_view{"as"},
    std::string_view{"checked_cast"},
    std::string_view{"conversion_loses"},
    std::string_view{"conversion_overflows"},
    std::string_view{"conversion_rounds"},
    std::string_view{"cross"},
    std::string_view{"div"},
    std::string_view{"div_into"},
    std::string_view{"divmod"},
    std::string_view{"dot"},
    std::string_view{"end"},
    std::string_view{"lerp"},
    std::string_view{"matrix_product"},
    std::string_view{"matrix_vector_product"},
    std::string_view{"max"},
    std::string_view{"midpoint"},
    std::string_view{"min"},
    std::string_view{"mod"},
    std::string_view{"mul"},
    std::string_view{"mul_all"},
    std::string_view{"mul_into"},
    std::string_view{"parse"},
    std::string_view{"parse_exact"},
    std::string_view{"sub"},
    std::string_view{"sum"},
    std::string_view{"to"},
    std::string_view{"to_string"},
    std::string_view{"transform_into"},
    std::string_view{"uniform"},
};

// Functions without one, and why.
struct exception {
    std::string_view Name;
    std::string_view Reason;
};
constexpr std::array inside_only{
    // Grids and intervals, not values.
    exception{"hull", "grid algebra"},
    exception{"includes", "grid algebra"},
    exception{"excludes", "grid algebra"},
    exception{"overlaps", "grid algebra"},
    // Vocabulary both share.
    exception{"errc_message", "shared vocabulary"},
    exception{"get_error_handler", "shared vocabulary"},
    exception{"set_error_handler", "shared vocabulary"},
    exception{"has_flag", "shared vocabulary"},
    exception{"make_policy", "shared vocabulary"},
    // The raw and the grid of an inside; a quantity's is its number's.
    exception{"to_string_debug", "an inside's raw and grid"},
    // Index tuples of an mdspan's extents: no values.
    exception{"indices", "index tuples, no values"},
};

// Member functions of an inside without a quantity counterpart: properties of
// the number in one unit (5 km and 5000 m have other raws, numerators and
// notch indices). A quantity's number is numerical_value(), and these are its
// members.
constexpr std::array member_only{
    std::string_view{"raw"},
    std::string_view{"from_raw"},
    std::string_view{"numerator"},
    std::string_view{"denominator"},
    std::string_view{"index"},
    std::string_view{"from_index"},
    std::string_view{"try_from_index"},
};

template <class Q>
concept has_as = requires(const Q& q) { q.template as<int>(); };
template <class A, class B>
concept has_min = requires(const A& a, const B& b) { min(a, b); };

template <std::size_t N>
constexpr bool listed(const std::array<std::string_view, N>& names, std::string_view n) {
    return std::ranges::find(names, n) != names.end();
}
constexpr bool excepted(std::string_view n) {
    return std::ranges::any_of(inside_only, [n](const exception& e) { return e.Name == n; });
}

template <std::meta::info Scope, bool Members>
std::vector<std::string> function_names() {
    std::vector<std::string> names;
    template for (constexpr auto m :
                  std::define_static_array(std::meta::members_of(Scope, std::meta::access_context::current()))) {
        if constexpr ((std::meta::is_function_template(m) || std::meta::is_function(m)) &&
                      std::meta::has_identifier(m))
            if constexpr (!Members || std::meta::is_public(m))
                // (Reserved names are the compiler's: GCC lists deduction
                // guides as __dguide_<class>.)
                if (!std::meta::identifier_of(m).starts_with("__"))
                    names.emplace_back(std::meta::identifier_of(m));
    }
    std::ranges::sort(names);
    names.erase(std::ranges::unique(names).begin(), names.end());
    return names;
}
} // namespace

TEST(UnitsParityTest, every_inside_function_has_a_quantity_form_or_a_reason) {
    for (const auto& n : function_names<^^beman::inside, false>())
        EXPECT_TRUE(listed(with_quantity_form, n) || excepted(n))
            << "beman::inside::" << n << " has no quantity form: add one (and list it here) or list why not";
}

TEST(UnitsParityTest, every_inside_member_has_a_quantity_member_or_a_reason) {
    const auto quantity_members = function_names<^^metres, true>();
    for (const auto& n : function_names<^^num, true>())
        EXPECT_TRUE(std::ranges::find(quantity_members, n) != quantity_members.end() || listed(member_only, n))
            << "inside::" << n << " has no quantity member: add one or list why not";
}

// Each listed quantity form compiles.
TEST(UnitsParityTest, the_quantity_forms_compile) {
    std::mt19937_64 rng{42};
    metres          m{40};
    km              k{1};
    errc            ec{};

    // Members: the one-shot policy, the policy handles and the hooks.
    static_assert(requires { metres{150, clamp}; });
    static_assert(requires { metres{k, clamp}; });
    static_assert(requires { m.policy<clamp>() = k; });
    static_assert(requires { m.policy<clamp>() += k; });
    static_assert(requires { m.policy<clamp>() *= just<2>; });
    static_assert(requires { m.policy(ec) = k; });
    static_assert(requires { m.on_clamp([](auto&, auto) {}) *= just<2>; });
    static_assert(requires { metres::try_make(5); });
    static_assert(requires(ratio r) { r.as<int>(); });
    static_assert(requires(ratio r) { r.to<int>(); });
    static_assert(requires(ratio r) { as<int>(r); });
    static_assert(requires(ratio r) { to<double>(r); });
    static_assert(!has_as<metres>); // a length is no number

    // Free functions.
    static_assert(requires { checked_cast<metres>(5); });
    static_assert(requires { checked_cast<metres>(k); });
    static_assert(requires { min(m, k); });
    static_assert(requires { max(m, k); });
    static_assert(requires { midpoint(m, k); });
    static_assert(!has_min<metres, seconds>); // other dimensions
    static_assert(requires { conversion_overflows<metres>(5); });
    static_assert(requires { conversion_rounds<metres>(5); });
    static_assert(requires { conversion_loses<metres>(5); });
    static_assert(requires { add_all(m, k, m); });
    static_assert(requires { mul_all(m, m); });
    static_assert(requires(std::vector<metres> v) { sum<quantity<si::metre, inside<{0, 1000}>>>(v); });
    static_assert(requires { mul_into<metres>(m, just<2>); });
    static_assert(requires { div_into<metres>(m, just<2>); });
    static_assert(requires { divmod(m, k); });
    static_assert(requires { add(m, k); });
    static_assert(requires { sub(k, m, round_floor); });
    static_assert(requires { mod(k, m); });
    static_assert(requires(seconds s) { mul(m, s); });
    static_assert(requires { mul(m, just<2>); });
    static_assert(requires { mul(just<2>, m); });
    static_assert(requires(seconds s) { div(m, s, round_floor); });
    static_assert(requires(seconds s) { div(m, s, ec); });
    static_assert(requires { div(m, just<2>, round_floor); });
    static_assert(requires(seconds s) { div(just<2>, s, round_floor); });
    static_assert(requires { dot(m, m, m, m); });
    static_assert(requires { cross(m, m, m, m); });
    static_assert(requires { lerp(m, k, just<1, 2>); });
    static_assert(requires { parse<metres>("5"); });
    static_assert(requires { parse_exact<metres>("5"); });
    static_assert(requires { to_string(m); });
    static_assert(requires { uniform<metres>(rng); });
    static_assert(requires(cursor<metres, 10> c) { end(c); });

    // mdspan: the algorithms take quantity elements (units/mdspan.hpp).
    using vec   = std::mdspan<metres, std::extents<std::size_t, 2>>;
    using mat   = std::mdspan<metres, std::extents<std::size_t, 2, 2>>;
    using area  = std::mdspan<quantity<si::metre * si::metre, inside<{0, 100000}>>, std::extents<std::size_t, 2>>;
    using areas = std::mdspan<quantity<si::metre * si::metre, inside<{0, 100000}>>, std::extents<std::size_t, 2, 2>>;
    static_assert(requires(vec v) { sum(v); });
    static_assert(requires(vec v) { dot(v, v); });
    static_assert(requires(mat a, vec v, area y) { matrix_vector_product(a, v, y); });
    static_assert(requires(mat a, areas c) { matrix_product(a, a, c); });
    static_assert(requires(vec v) { transform_into(v, v, [](metres x) { return x; }); });
}

// The new forms do what the inside forms do.
TEST(UnitsParityTest, the_new_forms_work) {
    metres m{40};
    km     k{1};

    // Unary + on a quantity and on an inside: the value itself.
    static_assert(std::same_as<decltype(+m), metres>);
    EXPECT_TRUE(+m == m);
    EXPECT_TRUE(+num{7} == num{7});

    m.policy<clamp>() = k; // 1 km is 1000 m: clamped to 100 m
    EXPECT_TRUE(m == just<100> * si::metre);
    m.policy<clamp>() *= just<0>;
    EXPECT_TRUE(m == just<0> * si::metre);
    m.policy<wrap>() += just<105> * si::metre; // the period is 101
    EXPECT_TRUE(m == just<4> * si::metre);

    errc ec{};
    m.policy(ec) = k;
    EXPECT_EQ(ec, errc::overflow);
    EXPECT_TRUE(m == just<4> * si::metre);

    EXPECT_TRUE(checked_cast<metres>(7) == just<7> * si::metre);
    EXPECT_THROW((void)checked_cast<metres>(k), inside_error);

    EXPECT_TRUE(min(m, k) == just<4> * si::metre);
    EXPECT_TRUE(max(m, k) == k);
    EXPECT_TRUE(midpoint(m, k) == just<502> * si::metre);

    const ratio r{25}; // 25 %: the number 1/4
    EXPECT_EQ(r.to<double>().value(), 0.25);
    EXPECT_EQ((r.as<int, round_nearest>()), 0);
    EXPECT_EQ((as<int, round_ceil>(r)), 1);
    EXPECT_EQ(r.to<int>().error(), errc::rounding_error); // 1/4 is no int without a rounding

    std::mt19937_64 rng{7};
    for (int i = 0; i < 100; ++i) {
        const auto u = uniform<metres>(rng);
        EXPECT_TRUE(u >= just<0> * si::metre && u <= just<100> * si::metre);
    }
}

// add ... mod with a per-call policy: sums in the common unit, products and
// quotients with the units multiplied, a number scaling; a hook is rejected.
template <class A, class B, class H>
concept add_with = requires(const A& a, const B& b, H h) { add(a, b, h); };

TEST(UnitsParityTest, per_call_arithmetic) {
    const metres m{40};
    const km     k{1};
    using per_s = quantity<si::second, inside<{1, 10}>>;
    const per_s s{3};

    EXPECT_TRUE(add(m, k) == just<1040> * si::metre);
    EXPECT_TRUE(sub(k, m) == just<960> * si::metre);
    EXPECT_TRUE(mod(k, m) == just<0> * si::metre);
    EXPECT_TRUE(mul(m, s) == just<120> * si::metre * si::second);
    EXPECT_TRUE(mul(just<2>, m) == just<80> * si::metre);
    EXPECT_TRUE(div(m, s, round_floor) == just<13> * si::metre / si::second);
    EXPECT_TRUE(div(m, s, round_ceil) == just<14> * si::metre / si::second);
    EXPECT_TRUE(div(m, inside<{1, 10}>{3}, round_floor) == just<13> * si::metre);
    EXPECT_TRUE(div(just<30>, s, round_floor) == just<10> / si::second);

    errc       ec{};
    const auto z = div(m, quantity<si::second, inside<{0, 10}>>{0}, ec);
    EXPECT_EQ(ec, errc::division_by_zero);
    (void)z;

    static_assert(!add_with<metres, km, decltype(beman::inside::detail::on_clamp([](auto&, auto) {}))>);
}

// cursor<Q, Step>, values<Q> and end(t) on a quantity: the number's grid,
// yielding quantities; Step is a number in Q's unit.
TEST(UnitsParityTest, cursors_and_values_over_quantities) {
    using s_t = quantity<si::second, inside<{{0, 2}, per<4>}, round_nearest>>;
    std::vector<s_t> seen;
    for (cursor<s_t, rational{1, 2}> t; t != end(t); ++t)
        seen.push_back(*t);
    ASSERT_EQ(seen.size(), 5u);
    EXPECT_TRUE(seen.front() == just<0> * si::second);
    EXPECT_TRUE(seen.back() == just<2> * si::second);
    EXPECT_TRUE((seen[1] == just<1, 2> * si::second));

    static_assert(std::ranges::random_access_range<decltype(values<s_t>)>);
    static_assert(std::ranges::borrowed_range<decltype(values<s_t>)>);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(values<s_t>)>, s_t>);
    EXPECT_EQ(values<s_t>.size(), 9u);
    EXPECT_TRUE(values<s_t>.back() == just<2> * si::second);
    EXPECT_EQ(std::ranges::find(values<s_t>, s_t{rational{3, 4}}) - values<s_t>.begin(), 3);

    // A quantity's ++ steps one notch of its number, as an inside's does.
    s_t q{0};
    ++q;
    EXPECT_TRUE((q == just<1, 4> * si::second));
}

// std::hash and std::numeric_limits of a quantity: its number's, the limits
// as quantities.
TEST(UnitsParityTest, hash_and_numeric_limits) {
    std::unordered_set<metres> set{metres{3}, metres{4}, metres{3}};
    EXPECT_EQ(set.size(), 2u);
    EXPECT_EQ(std::hash<metres>{}(metres{7}), std::hash<num>{}(num{7}));

    using lim = std::numeric_limits<metres>;
    static_assert(lim::is_specialized && lim::is_integer && lim::is_exact && !lim::is_signed);
    static_assert(std::same_as<decltype(lim::max()), metres>);
    EXPECT_TRUE(lim::max() == just<100> * si::metre);
    EXPECT_TRUE(lim::lowest() == just<0> * si::metre);
    EXPECT_TRUE(lim::epsilon() == just<0> * si::metre);
    EXPECT_EQ(lim::digits, std::numeric_limits<num>::digits);
}

// A quantity's on_clamp passes the overshoot as a quantity in its unit and
// kind, exactly; on_wrap's carry stays a count of periods.
TEST(UnitsParityTest, clamp_overshoot_is_a_quantity) {
    metres m{40};
    bool   exact = false;
    m.on_clamp([&](metres& self, auto over) {
        static_assert(quantity_like<decltype(over)>);
        static_assert(decltype(over)::reference == metres::reference);
        exact = &self == &m && over == just<900> * si::metre; // 1 km is 1000 m: 900 m past 100 m
    })           = km{1};
    EXPECT_TRUE(exact);
    EXPECT_TRUE(m == just<100> * si::metre);

    using turn = quantity<si::second, inside<{0, 59}, wrap>>;
    turn t{0};
    t.on_wrap([&](auto&, auto carry) {
        static_assert(!quantity_like<decltype(carry)>);
        exact = carry == 2_ins;
    }) = just<125> * si::second;
    EXPECT_TRUE(exact);
}
