// EXPECT: take no hooks
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Free arithmetic takes no hooks (it cannot fail but for a zero divisor, which
// stays in the result), so passing one is rejected instead of silently ignored.
// The hook factories are internal since 0.8.0; the guard stays.
#include <inside/inside.hpp>

int main() {
    using b = ins::id<"[0..10]">;
    auto s  = ins::add(b{1}, b{2}, ins::detail::on_error([](auto&, auto, auto) {}));
    (void)s;
}
