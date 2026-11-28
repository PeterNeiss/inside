// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: quantity kinds separate quantities of one dimension.
#include <inside/units.hpp>

#include <type_traits>

using namespace ins;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main() {
    // [listing]
    id<"[0..1000] [Hz]">     f{50, si::hertz};
    id<"[0..1000] [Bq]"> a{50, si::becquerel};
    static_assert(!addable<decltype(f), decltype(a)>);        // both 1/s, but not one kind

    id<"[1..10] [m]{width}">  w{3, isq::width[si::metre]};
    id<"[1..10] [m]{height}"> h{4, isq::height[si::metre]};
    id<"[1..100] [m^2]{area}"> area = w * h; // width · height
    auto around = w + h;                                      // 7 m, a length: the common kind
    auto tall   = kind_cast<isq::height>(w);              // a width read as a height, explicitly
    // [/listing]
    if (area.numerical_value_in(square(si::metre)) != 12 || around.numerical_value_in(si::metre) != 7 ||
        tall.numerical_value_in(si::metre) != 3)
        return 1;
    return 0;
}
