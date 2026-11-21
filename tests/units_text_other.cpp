// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "units_text_other.hpp"

#include <inside/units/io.hpp>

namespace units_text_other {
std::expected<dollars, ins::error> parse_dollars(std::string_view text) { return ins::parse<dollars>(text); }
} // namespace units_text_other
