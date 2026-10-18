// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The standard headers the library includes, for the global module fragment
// of the modules: included there, none of them is included again inside a
// module's purview (where it would attach to the module). config.hpp decides
// the conditional ones the way the headers do. Keep it in step with the
// headers' includes (`grep -rh '#include <' include/`).
#include <beman/inside/detail/config.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <functional>
#include <initializer_list>
#include <istream>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <ostream>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>
#if !BEMAN_INSIDE_MATH_NO_FP
    #include <cmath>
#endif
#if BEMAN_INSIDE_HAS_STD_FORMAT
    #include <format>
#endif
#if BEMAN_INSIDE_BIG_GRIDS
    #include <meta>
#endif
#if BEMAN_INSIDE_HAS_UMUL128
    #include <intrin.h>
#endif
