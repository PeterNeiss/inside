// EXPECT: fire only on_overflow
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Free arithmetic never fires on_clamp / on_wrap / on_error, so passing one is
// rejected instead of silently ignored.
#include <beman/inside/inside.hpp>

int main()
{
  using b = beman::inside::inside<{0, 10}>;
  auto s = beman::inside::add(b{1}, b{2}, beman::inside::on_error([](auto&, auto, auto) {}));
  (void)s;
}
