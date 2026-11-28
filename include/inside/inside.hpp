// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Public umbrella header: include this to get the full `ins::inside` API.
// The core type (inside/detail/core.hpp) and its comparisons (inside/compare.hpp),
// plus the free-function layers that depend on the complete type:
// casts, arithmetic operators, inside_range and cursor, and the std::numeric_limits and std::hash specializations.
// Those follow inside/detail/core.hpp because they need `id<key{G, P}>` fully defined.
#ifndef INSIDE_INSIDE_HPP
#define INSIDE_INSIDE_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/core.hpp>
#include <inside/inside/compare.hpp>
#include <inside/inside/casts.hpp>
#include <inside/inside/arithmetic.hpp>
#include <inside/inside/range.hpp>
#include <inside/inside/cursor.hpp>
#include <inside/inside/numeric_limits.hpp>

#endif // INSIDE_INSIDE_HPP
