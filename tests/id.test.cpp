// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The text of an id (inside/detail/id_text.hpp): every spelling read into its grid and policy,
// the canonical text written back, and where a wrong text stops.

#include <inside/inside.hpp>
#include <inside/inside/detail/id_text.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace id_tests {
using namespace ins;
using detail::parse_id_text;

// The canonical text of a grid and a policy.
struct written {
    char        Buf[detail::key_capacity]{};
    std::size_t Size{0};
    constexpr std::string_view view() const { return {Buf, Size}; }
};
consteval written write(const grid& g, policy_flag p = 0) {
    written           w;
    detail::text_out t{w.Buf, sizeof w.Buf};
    detail::write_id_text(t, g, p);
    w.Size = t.Size;
    return w;
}

// A text spells the grid and the policy.
consteval bool spells(std::string_view text, const grid& g, policy_flag p = 0) {
    const detail::id_spec s{parse_id_text(text)};
    return s.Expected.empty() && s.Grid == g && s.Policy == p;
}
// A text's canonical spelling.
consteval bool canonical(std::string_view text, std::string_view expected) {
    const detail::id_spec s{parse_id_text(text)};
    return s.Expected.empty() && write(s.Grid, s.Policy).view() == expected;
}
// A text that stops at `at`, expecting something that starts with `expected`.
consteval bool stops(std::string_view text, std::size_t at, std::string_view expected) {
    const detail::id_spec s{parse_id_text(text)};
    return s.Stop == at && s.Expected.starts_with(expected);
}
// grid → text → grid, and text → grid → text.
consteval bool round_trips(const grid& g, policy_flag p = 0) {
    const written         w{write(g, p)};
    const detail::id_spec s{parse_id_text(w.view(), false)};
    return s.Expected.empty() && s.Grid == g && s.Policy == p && write(s.Grid, s.Policy).view() == w.view();
}

constexpr grid_number two_70 = 1180591620717411303424_g;

TEST(IdTextTest, intervals_spell_the_grids_inside_spells) {
    static_assert(spells("[0..9]", grid{0, 9}));
    static_assert(spells("[0..10)", grid{0, 9}));
    static_assert(spells("(0..10)", grid{1, 9}));
    static_assert(spells("(0..10]", grid{1, 10}));
    static_assert(spells("[0..360)", grid{0, 360, bounds::half_open}));
    static_assert(spells("[-5..5]", grid{-5, 5}));
    static_assert(spells("[-10..-5]", grid{-10, -5}));
    static_assert(spells("[3..3]", grid{3, 3}));                       // notch 1, one value
    static_assert(spells("[5]", grid{5}));                             // a point
    static_assert(spells("[-1/2]", grid{just<-1, 2>}));
    static_assert(spells("[0..1/2]", grid{0, just<1, 2>}));            // the notch derived: 1/2
    static_assert(spells("[2/5..7/10)", grid{{just<2, 5>, just<3, 5>}, just<1, 10>}));
}

TEST(IdTextTest, steps_and_bounds) {
    static_assert(spells("[0..10) 1/4", grid{{0, 10}, per<4>, bounds::half_open}));
    static_assert(spells("[0..1] 1/1000", grid{{0, 1}, per<1000>}));
    static_assert(spells("(0..1) 1/16", grid{{0, 1}, per<16>, bounds::open}));
    static_assert(spells("[0..100] 5", grid{{0, 100}, 5}));
    static_assert(spells("[1/2..21/2] 1", grid{{0.5, 10.5}, 1}));    // unanchored
    static_assert(spells("[5/3..17/3] 2/3", grid{{just<5, 3>, just<17, 3>}, just<2, 3>}));
    static_assert(spells("[0..1] <1000>", grid{{0, 1}, max_denominator{1000}}));
    static_assert(spells("[-1..1] <7>", grid{{-1, 1}, max_denominator{7}}));
}

TEST(IdTextTest, numbers_read_as_the_ins_literal_reads_them) {
    static_assert(spells("[0..1e9]", grid{0, 1'000'000'000}));
    static_assert(spells("[0..1'000]", grid{0, 1000}));
    static_assert(spells("[0..0.25]", grid{0, just<1, 4>}));
    static_assert(spells("[0..2.5e-1]", grid{0, just<1, 4>}));
    static_assert(spells("[0..0x10]", grid{0, 16}));
    static_assert(spells("[0..1] 0x1p-4", grid{{0, 1}, per<16>}));
    static_assert(spells("[0..18446744073709551615]", grid{0, grid_number{18446744073709551615ull}}));
    static_assert(spells("[0..1180591620717411303424]", grid{0, two_70}));
    static_assert(spells("[0..1] 1/1180591620717411303424", grid{{0, 1}, grid_number{1} / two_70}));
    static_assert(spells("[0.5..1.5]", grid{0.5, 1.5}));
}

TEST(IdTextTest, flags) {
    static_assert(spells("[0..360) wrap", grid{0, 359}, wrap));
    static_assert(spells("[0..9] clamp", grid{0, 9}, clamp));
    static_assert(spells("[0..9] clamp|round_toward_zero", grid{0, 9}, clamp | round_toward_zero));
    static_assert(spells("[0..9] round_half_away|clamp", grid{0, 9}, clamp | round_half_away));
    static_assert(spells("[0..9] round_floor", grid{0, 9}, round_floor));
    static_assert(spells("[0..9] unsafe", grid{0, 9}, unsafe));
    static_assert(spells("[0..9] round_ceil|unsafe", grid{0, 9}, round_ceil | unsafe));
    static_assert(spells("[0..9] 3 wrap", grid{{0, 9}, 3}, wrap));
}

TEST(IdTextTest, spaces_are_free_between_tokens) {
    static_assert(spells("  [ 0 .. 10 )  1 / 4  clamp | round_floor ", grid{{0, 10}, per<4>, bounds::half_open},
                         clamp | round_floor));
    static_assert(spells("[0..9]clamp", grid{0, 9}, clamp));
}

// The written limits with the smallest common denominator win, closed on a tie.
TEST(IdTextTest, the_canonical_text) {
    static_assert(canonical("(0..10)", "[1..9]"));
    static_assert(canonical("[0..10)", "[0..9]"));
    static_assert(canonical("[0..10) 1/4", "[0..10) 1/4"));
    static_assert(canonical("[0..39/4] 1/4", "[0..10) 1/4"));
    static_assert(canonical("[0..360) wrap", "[0..359] wrap"));
    static_assert(canonical("[0..1) 1/1000", "[0..1) 1/1000"));
    static_assert(canonical("(0..1) 1/1000", "(0..1) 1/1000"));
    static_assert(canonical("[1/1000..999/1000] 1/1000", "(0..1) 1/1000"));
    static_assert(canonical("[2/5..7/10)", "[2/5..3/5] 1/10"));
    static_assert(canonical("[2/5..7/10) clamp|round_toward_zero", "[2/5..3/5] 1/10 clamp|round_toward_zero"));
    static_assert(canonical("[0..1] 1/4", "[0..1] 1/4"));                // integer limits: closed
    static_assert(canonical("[0..1/2]", "[0..1) 1/2"));
    static_assert(canonical("[1/2..21/2] 1", "[1/2..21/2] 1"));
    static_assert(canonical("[0..100] 1", "[0..100]"));                  // the derived notch is left out
    static_assert(canonical("[0..30] 3", "[0..30] 3"));
    static_assert(canonical("[0..1] <1000>", "[0..1] <1000>"));
    static_assert(canonical("[5]", "[5]"));
    static_assert(canonical("[3..3]", "[3..3]"));
    static_assert(canonical("[0..1e9]", "[0..1000000000]"));
    static_assert(canonical("[0..0.25]", "[0..1/2) 1/4"));                // {0, 1/4}
    static_assert(canonical("[0..1180591620717411303424]", "[0..0x1p70]"));             // past 64 bits: hex,
    static_assert(canonical("[0..1180591620717411303423]", "[0..0x3fffffffffffffffff]")); // m·2^e where shorter
    static_assert(canonical("[0..0x1p20000]", "[0..0x1p20000]"));
    static_assert(canonical("[0..1] 1/0x1p100", "[0..1] 1/0x1p100"));
    static_assert(canonical("[-0x3p200..0]", "[-0x3p200..0]"));
    static_assert(canonical("[0..9] round_floor|clamp", "[0..9] clamp|round_floor"));
    static_assert(canonical("[0..9] round_toward_zero", "[0..9] round_toward_zero"));
    static_assert(canonical("[0..9] unsafe", "[0..9] unsafe"));
    static_assert(canonical("[0..9] unsafe|round_ceil", "[0..9] round_ceil|unsafe"));
    static_assert(canonical("[0..9] round_toward_zero|clamp", "[0..9] clamp|round_toward_zero"));
}

TEST(IdTextTest, the_canonical_text_reads_back) {
    static_assert(round_trips(grid{0, 100}));
    static_assert(round_trips(grid{-128, 127}));
    static_assert(round_trips(grid{{0, 1}, per<100>}));
    static_assert(round_trips(grid{{-8, 8}, per<16384>}));
    static_assert(round_trips(grid{{0, 30}, 3}));
    static_assert(round_trips(grid{{0, 3}, just<3, 2>}));
    static_assert(round_trips(grid{0.5, 10}));
    static_assert(round_trips(grid{{0.5, 10.5}, 1}));
    static_assert(round_trips(grid{{just<1, 3>, just<7, 3>}, 1}));
    static_assert(round_trips(grid{{0, 1}, max_denominator{1000}}));
    static_assert(round_trips(grid{just<1, 2>}));
    static_assert(round_trips(grid{3, 3}));
    static_assert(round_trips(grid{0, 360, bounds::half_open}, wrap));
    static_assert(round_trips(grid{0, two_70}));
    static_assert(round_trips(grid{{0, 1}, grid_number{1} / two_70}));
    static_assert(round_trips(grid{{-two_70, two_70}, grid_number{1} / two_70}));
    static_assert(round_trips(grid{0, 18446744073709551615ull}));
    static_assert(round_trips(grid{0, grid_number{detail::grid_int{1} << 20000}}));
    static_assert(round_trips(grid{{0, 1}, grid_number{1} / grid_number{detail::grid_int{3} << 150}}));
    // Grids no inside holds read back too: a type names them (an operator's constraint tests the grid).
    static_assert(round_trips(grid{{0, 1}, grid_number{0}}));            // continuous, unbounded
    static_assert(round_trips(grid{{0, 10}, 3}));                        // a notch that does not divide
    static_assert(round_trips(grid{{1, 1}, max_denominator{10}}));       // a point with a bound
    static_assert(round_trips(grid{0, 9}, round_floor | round_ceil));    // two rounding modes
    static_assert(round_trips(grid{0, 9}, clamp | wrap));
    static_assert(round_trips(grid{0, 9}, 7));                           // bits no flag names: bits_7
    static_assert(write(grid{0, 9}, 7).view() == "[0..9] bits_7");
    static_assert(round_trips(grid{0, 9}, clamp | round_half_away));
    static_assert(round_trips(grid{0, 9}, round_toward_zero));
    static_assert(round_trips(grid{0, 9}, unsafe));
    static_assert(round_trips(grid{0, 9}, round_floor | unsafe));
    static_assert(round_trips(grid{0, 9}, wrap | round_half_even));
    static_assert(round_trips(grid{0, 9}, detail::cursor_marker));
}

// Where a text stops making sense, and what it expected there.
TEST(IdTextTest, a_wrong_text_stops_where_it_goes_wrong) {
    static_assert(stops("", 0, "'[' or '('"));
    static_assert(stops("{0..9}", 0, "'[' or '('"));
    static_assert(stops("[x..9]", 1, "a number"));
    static_assert(stops("[0...9]", 4, "a number"));
    static_assert(stops("[0..9", 5, "']' or ')'"));
    static_assert(stops("[0..10x", 4, "a number"));      // one token of digits and letters
    static_assert(stops("[0,9]", 2, "'..', or ']' for a point"));
    static_assert(stops("(5)", 2, "'..'"));
    static_assert(stops("[9..0]", 0, "an upper limit at least"));
    static_assert(stops("[0..9] 0", 7, "a step above 0"));
    static_assert(stops("[0..9] -1", 7, "a step above 0"));
    static_assert(stops("[0..10] 3", 8, "a step that divides"));
    static_assert(stops("[0..1/0]", 6, "a denominator above 0"));
    static_assert(stops("[5] 1/2", 4, "no step on a point"));
    static_assert(stops("[0..1) <1000>", 7, "closed limits"));
    static_assert(stops("(0..1] <1000>", 0, "closed limits"));
    static_assert(stops("[0..1] <0>", 8, "a max denominator that is a whole number"));
    static_assert(stops("[0..1/3] <2>", 10, "a max denominator at least"));
    static_assert(stops("[0..1] <1000", 12, "'>'"));
    static_assert(stops("(0..1)", 0, "a grid point between"));
    static_assert(stops("[0..9] clampy", 7, "a flag:"));
    static_assert(stops("[0..9] clamp|wrap", 13, "clamp or wrap"));
    static_assert(stops("[0..9] round_floor|round_ceil", 19, "one rounding mode"));
    static_assert(stops("[0..9] 1 2", 9, "a [unit], a flag, or the end"));
    static_assert(stops("[0..9] clamp 2", 13, "'|' and a flag, or the end"));
    static_assert(stops("[0..9] 2", 7, "a step that divides"));
    static_assert(stops("[0..9] x", 7, "a flag:"));
}
// The types: every spelling of one grid and policy is one type, the one id<key{G, P}> names,
// and the operators' results read the same way.
TEST(IdTypeTest, every_spelling_is_one_type) {
    static_assert(std::is_same_v<id<"[0..10)">, id<"[0..9]">>);
    static_assert(std::is_same_v<id<"(0..10)">, id<"[1..9]">>);
    static_assert(std::is_same_v<id<"[ 0 .. 10 ) 1/4">, id<"[0..39/4] 1/4">>);
    static_assert(std::is_same_v<id<"[0..9]">, id<key{grid{0, 9}}>>);
    static_assert(std::is_same_v<id<"[0..360) wrap">, id<key{grid{0, 360, bounds::half_open}, wrap}>>);
    static_assert(std::is_same_v<id<"[0..1] 1/100 clamp|round_half_away">,
                                 id<key{grid{{0, 1}, per<100>}, round_half_away | clamp}>>);
    static_assert(std::is_same_v<id<"[0..1] <1000>">, id<key{grid{{0, 1}, max_denominator{1000}}}>>);
    static_assert(std::is_same_v<id<"[5]">, decltype(5_ins)>);
    static_assert(!std::is_same_v<id<"[3..3]">, decltype(3_ins)>);        // notch 1 vs a point
    static_assert(grid_of<id<"[0..10) 1/4">> == grid{{0, 10}, per<4>, bounds::half_open});
    static_assert(policy_of<id<"[0..9] clamp|round_floor">> == (clamp | round_floor));
}

TEST(IdTypeTest, results_read_as_ids) {
    using digit = id<"[1..9]">;
    static_assert(std::is_same_v<decltype(digit{} + digit{}), id<"[2..18]">>);
    static_assert(std::is_same_v<decltype(digit{} - digit{}), id<"[-8..8]">>);
    static_assert(std::is_same_v<decltype(digit{} * digit{}), id<"[1..81]">>);
    static_assert(std::is_same_v<decltype(-digit{}), id<"[-9..-1]">>);
    // The policy both operands share; different policies give the default.
    using degrees = id<"[0..360) wrap">;
    static_assert(std::is_same_v<decltype(degrees{} + degrees{}), id<"[0..718] wrap">>);
    static_assert(std::is_same_v<decltype(id<"[0..9] clamp">{} + id<"[0..9]">{}), id<"[0..18]">>);
    // Quarters stay quarters.
    using quarters = id<"[0..10) 1/4">;
    static_assert(std::is_same_v<decltype(quarters{} + quarters{}), id<"[0..39/2] 1/4">>);
    EXPECT_EQ(digit{3} + digit{4}, 7);
}

// A key holds the characters written, so a type prints as it reads: ins::id<ins::key{"[0..9]"}>.
TEST(IdTypeTest, a_key_holds_its_text) {
    static_assert(key{"[0..10)"}.text() == "[0..9]");
    static_assert(key{"[0..10) 1/4 clamp"}.text() == "[0..10) 1/4 clamp");
    static_assert(key{grid{0, 9}, wrap}.text() == "[0..9] wrap");
    static_assert(key{grid{0, 9}}.Text[6] == '\0');                     // nothing after the text
    static_assert(!key{"[0..10x"}.ok() && key{"[0..10x"}.text() == "![0..10x");
    static_assert(key{grid{0, 9}, 1ull << 6}.text() == "[0..9] bits_64");        // round_floor's bit without snap
}

// An id is a type like any: a member, a template argument, deduced by its key.
template <key K>
constexpr policy_flag policy_param(id<K>) {
    return policy_of<id<K>>;
}
struct reading {
    id<"[0..4095]">      raw{2048};
    id<"[0..10) 1/4">    quarters{2.5};
    static constexpr id<"[0..9]"> nine{9};
};
TEST(IdTypeTest, ids_as_members_and_deduced) {
    constexpr reading r{};
    static_assert(r.raw == 2048 && r.quarters == just<5, 2> && reading::nine == 9);
    static_assert(policy_param(id<"[0..9] wrap">{}) == wrap);
}
// "42 [1..100)"_ins: a value with its type, stored while compiling under the type's policy.
TEST(IdLiteralTest, a_value_with_its_type) {
    constexpr auto a{"42 [1..100)"_ins};
    static_assert(std::is_same_v<std::remove_const_t<decltype(a)>, id<"[1..99]">> && a == 42);
    constexpr auto q{"0.25 [0..1) 1/4"_ins};
    static_assert(std::is_same_v<std::remove_const_t<decltype(q)>, id<"[0..1) 1/4">> && q == just<1, 4>);
    static_assert("-1/2 [-1..1] 1/2"_ins == just<-1, 2>);
    static_assert("1e3 [0..1e6]"_ins == 1000);
    static_assert("0x1p70 [0..0x1p80]"_ins == just<1180591620717411303424_g>);
    static_assert("  7   [0..9]  wrap "_ins == 7);
    static_assert(std::is_same_v<decltype("7 [0..9] wrap"_ins), id<"[0..9] wrap">>);
    // The type's policy stores it: clamp saturates, a rounding mode rounds.
    static_assert("200 [0..99] clamp"_ins == 99);
    static_assert("0.3 [0..1] 1/4 round_half_away"_ins == just<1, 4>);
    static_assert("0.4 [0..1] 1/4 round_half_away"_ins == just<1, 2>);
    // Without a type: the point, as the numeric literal.
    static_assert(std::is_same_v<decltype("42"_ins), decltype(42_ins)> && "42"_ins == 42_ins);
    static_assert(std::is_same_v<decltype("2.5"_ins), decltype(2.5_ins)>);
    static_assert(std::is_same_v<decltype("1/3"_ins), std::remove_const_t<decltype(just<1, 3>)>>);
    EXPECT_EQ("42 [1..100)"_ins, 42);
}

struct calibrated {
    decltype("0 [0..4095]"_ins) raw{"2048 [0..4095]"_ins};
};
TEST(IdLiteralTest, a_literal_initializes_a_member) {
    constexpr calibrated c{};
    static_assert(c.raw == 2048);
}
} // namespace id_tests
