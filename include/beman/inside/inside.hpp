// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// Public umbrella header. Include this to get the full `beman::inside::inside` API:
// the core type (core.hpp) plus the free-function layers that depend on the
// complete type — casts, arithmetic operators, and inside_range. Those three
// must follow core.hpp because they need `inside<G, P>` fully defined.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_INSIDE_HPP
#define BEMAN_INSIDE_INSIDE_HPP

#include <beman/inside/core.hpp>
#include <beman/inside/casts.hpp>
#include <beman/inside/arithmetic.hpp>
#include <beman/inside/range.hpp>

#endif // BEMAN_INSIDE_INSIDE_HPP
