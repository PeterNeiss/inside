// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Public umbrella header. Include this to get the full `beman::inside::inside` API:
// the core type (inside/detail/core.hpp) and its comparisons (inside/detail/compare.hpp), plus the
// free-function layers that depend on the complete type — casts, arithmetic
// operators, inside_range and cursor, and the std::numeric_limits and
// std::hash specializations. Those follow inside/detail/core.hpp because they need
// `inside<G, P>` fully defined.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_INSIDE_HPP
#define BEMAN_INSIDE_INSIDE_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/core.hpp>
#include <beman/inside/inside/detail/compare.hpp>
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/arithmetic.hpp>
#include <beman/inside/inside/range.hpp>
#include <beman/inside/inside/cursor.hpp>
#include <beman/inside/inside/numeric_limits.hpp>

#endif // BEMAN_INSIDE_INSIDE_HPP
