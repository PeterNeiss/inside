//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// Public umbrella header. Include this to get the full `bnd::bound` API:
// the core type (core.hpp) plus the free-function layers that depend on the
// complete type — casts, arithmetic operators, and bound_range. Those three
// must follow core.hpp because they need `bound<G, P>` fully defined.
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/core.hpp"
#include "bound/casts.hpp"
#include "bound/arithmetic.hpp"
#include "bound/range.hpp"

#endif // BNDboundHPP
