// EXPECT: write conversion_overflows<Q>
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The predicates take a number and its unit: conversion_overflows<Q>(v, si::metre).
#include <inside/units.hpp>

int main() {
    using namespace ins;
    (void)conversion_overflows<id<"[0..100] [m]">>(150);
}
