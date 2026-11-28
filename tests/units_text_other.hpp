// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The second translation unit of units_text.test.cpp: it parses the same quantity type without seeing the cent,
// so its parse<dollars> is a specialization of its own.
#ifndef INSIDE_TESTS_UNITS_TEXT_OTHER_HPP
#define INSIDE_TESTS_UNITS_TEXT_OTHER_HPP

#include <inside/units.hpp>

#include <expected>
#include <string_view>

namespace units_text_other {
inline constexpr auto USD  = ins::base_unit<"USD", "dollar">;
inline constexpr auto item = ins::base_unit<"item", "count">;
using dollars              = ins::id<"[0..1000] 1/100 [USD]">;

std::expected<dollars, ins::error> parse_dollars(std::string_view text);
} // namespace units_text_other

#endif // INSIDE_TESTS_UNITS_TEXT_OTHER_HPP
