// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A key reads a quantity's text with the reader where the key is formed: texts read before <inside/units.hpp>,
// of any length, leave the reading of later quantity texts alone.

#include <inside/inside.hpp>

namespace id_reader_order_tests {
// Read before the units header: an inside text as long as the quantity texts below.
using before  = ins::id<"[0..12345]">;    // 10 characters, as [0..1] [m]
using before2 = ins::id<"[0..12345678]">; // 13, as [0..1] [km/h] and the literal's 1 [0..1] [m]
static_assert(ins::upper_of<before> == 12345 && ins::upper_of<before2> == 12345678);
} // namespace id_reader_order_tests

#include <inside/units.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace id_reader_order_tests {
using namespace ins;

TEST(IdReaderOrderTest, a_quantity_text_after_the_units_header_reads) {
    static_assert(id<"[0..1] [m]">::unit == si::metre.value);
    static_assert(std::is_same_v<id<"[0..1] [km/h]">, id<quantity_key<si::kilo(si::metre) / si::hour, id<"[0..1]">>>>);
    constexpr auto v{"1 [0..1] [m]"_ins};
    EXPECT_EQ(v, just<1> * si::metre);
}
} // namespace id_reader_order_tests
