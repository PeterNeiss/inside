// EXPECT: write conversion_overflows<Q>
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The predicates take a number and its unit: conversion_overflows<Q>(v, si::metre).
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    (void)conversion_overflows<quantity<si::metre, inside<{0, 100}>>>(150);
}
