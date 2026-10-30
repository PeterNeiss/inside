// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Public umbrella header. Include this to get the full `beman::inside::inside` API:
// the core type (core.hpp) and its comparisons (detail/compare.hpp), plus the
// free-function layers that depend on the complete type — casts, arithmetic
// operators, inside_range and cursor, and the std::numeric_limits and
// std::hash specializations. Those follow core.hpp because they need
// `inside<G, P>` fully defined.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_INSIDE_HPP
#define BEMAN_INSIDE_INSIDE_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/core.hpp>
#include <beman/inside/detail/compare.hpp>
#include <beman/inside/casts.hpp>
#include <beman/inside/arithmetic.hpp>
#include <beman/inside/range.hpp>
#include <beman/inside/cursor.hpp>
#include <beman/inside/numeric_limits.hpp>

#endif // BEMAN_INSIDE_INSIDE_HPP
