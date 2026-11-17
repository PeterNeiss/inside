#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""perfmap: every public run-time operation on every grid shape and policy it
takes, measured against an ideal twin.

    perfmap.py inventory     the public surface (inventory.cpp + a scan for
                             members GCC's reflection omits) and the coverage
                             check: each name has a family, or a reason
    perfmap.py run [regex]   generate, compile and measure the kernels whose
                             id matches; results in build/perf-gate/map/
    perfmap.py report        the CSV, a summary per family, the flagged rows

A kernel pair is k_<n> (the library) and t_<n> (its ideal twin: plain
integer or double code with the same checks and rounding, twins.hpp, or a
reference such as std::to_chars). Each pair is measured statically (the
optimized asm: instructions on the hot path, divisions, calls, stack
protector, stack traffic) and dynamically (nanobench: ns, instructions and
cycles per call, minus the cost of an empty noinline call).
"""
import concurrent.futures as cf
import csv
import itertools
import json
import os
import pathlib
import re
import subprocess
import sys

R = pathlib.Path(__file__).resolve().parents[4]
OUT = R / "build" / "perf-gate" / "map"
NB = os.environ.get("NANOBENCH_INCLUDE", str(R / "build/perf26/_deps/nanobench-src/src/include"))
CXX = ["g++-16", "-std=c++26", "-freflection", "-O3", "-DNDEBUG", "-mfma", f"-I{R}/include", f"-I{R}",
       "-isystem", NB, "-fno-asynchronous-unwind-tables"]
JOBS = int(os.environ.get("JOBS", "6"))
BATCH = int(os.environ.get("BATCH", "40"))

# ---------------------------------------------------------------------------
# Shapes: one grid per storage kind (and sign, width).
# ---------------------------------------------------------------------------
SHAPES = {
    "u8": ("{0, 200}", "int-value"),
    "s32": ("{-100000, 100000}", "int-value"),
    "s64": ("{-4000000000000000000, 4000000000000000000}", "int-value"),
    "idx": ("{5, 1000}", "int-index"),
    "q88": ("{{0, 255}, per<256>}", "q-format"),
    "q114": ("{{-2, 2}, per<16384>}", "q-format"),
    "q1616": ("{{-30000, 30000}, per<65536>}", "q-format"),
    "dec2": ("{{0, 1000000}, per<100>}", "decimal"),
    "dec3": ("{{-1000, 1000}, per<1000>}", "decimal"),
    "n60": ("{{0, 36000}, 60}", "coarse"),
    "n72": ("{{-7, 14}, just<7, 2>}", "coarse"),
    "un": ("{{1, 601}, 60}", "unanchored"),
    "half": ("{{just<1, 2>, just<201, 2>}, 1}", "unanchored"),
    "cont": ("{{0, 100}, max_denominator{1000}}", "continuous"),
}
NOTCHED = [s for s, (_, k) in SHAPES.items() if k != "continuous"]
INTEGER = [s for s, (_, k) in SHAPES.items() if k.startswith("int")]

# Policies: flags, and the twin's range handling, mode and permission to round.
POLICIES = {
    "checked": ("checked", "check", "trunc", False),
    "unsafe": ("unsafe", "none", "trunc", False),
    "clamp": ("clamp", "clamp", "trunc", False),
    "wrap": ("wrap", "wrap", "trunc", False),
    "trunc": ("round_toward_zero", "check", "trunc", True),
    "floor": ("round_floor", "check", "floor", True),
    "ceil": ("round_ceil", "check", "ceil", True),
    "half_away": ("round_half_away", "check", "half_away", True),
    "half_even": ("round_half_even", "check", "half_even", True),
    "clamp_floor": ("clamp | round_floor", "clamp", "floor", True),
    "wrap_half_even": ("wrap | round_half_even", "wrap", "half_even", True),
    "unsafe_half_away": ("unsafe | round_half_away", "none", "half_away", True),
}
ROUNDING = ["trunc", "floor", "ceil", "half_away", "half_even"]


def T(shape, policy="checked"):
    flags = POLICIES[policy][0]
    return f"inside<{SHAPES[shape][0]}>" if flags == "checked" else f"inside<{SHAPES[shape][0]}, {flags}>"


def twin_args(policy):
    _, rng, mode, rounds = POLICIES[policy]
    return f"twin::range::{rng}, twin::mode::{mode}, {'true' if rounds else 'false'}"


# ---------------------------------------------------------------------------
# A kernel pair. args: list of (kind, type) where kind is
#   "x"   an inside of `type` (the twin receives its raw)
#   "int" an int64 in [lo, hi] ("type" = (lo, hi) in C++ expressions)
#   "dbl" a double in [lo, hi]
#   "ref" an inside of `type`, passed by reference (compound ops)
# real / ideal: C++ bodies over the parameters a, b, c …
# ---------------------------------------------------------------------------
class Pair:
    def __init__(self, family, ident, args, real, ideal, ideal_kind="twin", note=""):
        self.family, self.ident, self.args = family, ident, args
        self.real, self.ideal, self.ideal_kind, self.note = real, ideal, ideal_kind, note


FAMILIES = {}  # name -> (inventory names, generator)


def family(name, covers):
    def deco(fn):
        FAMILIES[name] = (covers, fn)
        return fn
    return deco


def X(shape, policy="checked"):
    return ("x", T(shape, policy))


# --- stores ----------------------------------------------------------------
def int_range(shape, wide):
    lo_hi = {"u8": (0, 200), "s32": (-100000, 100000), "s64": (-4000000000000000000, 4000000000000000000),
             "idx": (5, 1000), "q88": (0, 255), "q114": (-2, 2), "q1616": (-30000, 30000), "dec2": (0, 1000000),
             "dec3": (-1000, 1000), "n60": (0, 36000), "n72": (-7, 14), "un": (1, 601), "half": (1, 100),
             "cont": (0, 100)}[shape]
    if not wide:
        return lo_hi
    span = max(1, (lo_hi[1] - lo_hi[0]) // 4)
    return (lo_hi[0] - span, lo_hi[1] + span)


@family("store from an integer", ["inside::member (constructor)", "inside::member try_make", "inside::member with",
                                   "inside::member operator=", "inside exact_cast", "inside conversion_overflows",
                                   "inside conversion_rounds", "inside conversion_loses"])
def f_store_int():
    for s in NOTCHED:
        for p in POLICIES:
            base = T(s)
            rng = int_range(s, p in ("clamp", "wrap", "clamp_floor", "wrap_half_even"))
            arg = [("int", rng)]
            tw = f"return twin::store_int<{base}, {twin_args(p)}>(a);"
            if p == "checked":
                yield Pair("store from an integer", f"store_int/{s}/try_make", arg, f"return {base}::try_make(a);", tw)
                if SHAPES[s][1] in ("coarse", "unanchored", "int-index"):
                    yield Pair("store from an integer", f"store_int/{s}/try_make_onnotch", [("intval", base)],
                               f"return {base}::try_make(a);", tw, note="inputs on the notch")
                yield Pair("store from an integer", f"store_int/{s}/policy_ec", arg,
                           f"error ec{{}}; {base} x; x.on_error(ec) = a; return std::pair{{x, ec}};", tw)
                yield Pair("store from an integer", f"store_int/{s}/exact_cast", arg, f"return exact_cast<{base}>(a);", tw)
                yield Pair("store from an integer", f"store_int/{s}/conversion_overflows", arg,
                           f"return conversion_overflows<{base}>(a);",
                           f"auto r = twin::store_int<{base}, twin::range::check, twin::mode::trunc, true>(a); return !r;")
            elif p == "unsafe":
                yield Pair("store from an integer", f"store_int/{s}/unsafe", arg, f"return {base}{{a, unsafe}};", tw)
            else:
                if POLICIES[p][1] != "check":  # a run-time int needs a range channel
                    yield Pair("store from an integer", f"store_int/{s}/{p}", arg,
                               f"return {base}{{a, {POLICIES[p][0]}}};", tw)
                if p in ROUNDING:
                    yield Pair("store from an integer", f"store_int/{s}/try_make_{p}", arg,
                               f"return {base}::try_make<{POLICIES[p][0]}>(a);", tw)


@family("store from a double", ["inside::member (constructor)", "inside::member try_make"])
def f_store_double():
    for s in NOTCHED:
        lo, hi = int_range(s, False)
        for p in ["checked"] + ROUNDING + ["clamp_floor"]:
            mode = POLICIES[p][2] if POLICIES[p][3] else "half_away"
            base = T(s)
            arg = [("dbl", (lo, hi))]
            tw = f"return twin::store_double<{base}, twin::range::{POLICIES[p][1]}, twin::mode::{mode}>(a);"
            if p == "checked":
                yield Pair("store from a double", f"store_double/{s}/try_make", arg,
                           f"return {base}::try_make<round_half_away>(a);", tw)
            elif POLICIES[p][1] == "check":
                yield Pair("store from a double", f"store_double/{s}/try_make_{p}", arg,
                           f"return {base}::try_make<{POLICIES[p][0]}>(a);", tw)
            else:
                yield Pair("store from a double", f"store_double/{s}/{p}", arg, f"return {base}{{a, {POLICIES[p][0]}}};", tw)


CROSS = [("u8", "s32"), ("s32", "u8"), ("idx", "u8"), ("u8", "q88"), ("q88", "u8"), ("q88", "q114"),
         ("q114", "q88"), ("dec2", "q88"), ("q88", "dec2"), ("dec3", "dec2"), ("n60", "s32"), ("s32", "n60"),
         ("n72", "q88"), ("un", "n60"), ("half", "u8"), ("u8", "half"), ("dec2", "n60"), ("q1616", "dec3")]


@family("store from another inside", ["inside::member (constructor)", "inside::member operator=",
                                      "inside::member try_make", "inside exact_cast"])
def f_store_inside():
    for dst, src in CROSS:
        for p in ["checked", "clamp", "wrap", "floor", "half_away", "half_even", "clamp_floor"]:
            base = T(dst)
            tw = f"return twin::store_inside<{base}, {T(src)}, {twin_args(p)}>(a);"
            real = (f"return {base}::try_make(a);" if p == "checked"
                    else f"return {base}::try_make<{POLICIES[p][0]}>(a);" if POLICIES[p][1] == "check"
                    else f"return {base}{{a, {POLICIES[p][0]}}};")
            yield Pair("store from another inside", f"store_inside/{src}->{dst}/{p}", [X(src)], real, tw)


# --- arithmetic --------------------------------------------------------------
PAIRS = [(s, s) for s in NOTCHED] + CROSS


def res(expr):
    return f"std::remove_cvref_t<decltype({expr})>"


@family("+ and -", ["inside operator+", "inside operator-", "math operator+", "math operator-"])
def f_add():
    for a, b in PAIRS:
        for op, sign in (("+", 1), ("-", -1)):
            r = res(f"std::declval<{T(a)}>() {op} std::declval<{T(b)}>()")
            yield Pair("+ and -", f"add/{a}{op}{b}", [X(a), X(b)], f"return a {op} b;",
                       f"return twin::add<{r}, {T(a)}, {T(b)}, {sign}>(a, b);")


@family("×", ["inside operator*", "math operator*"])
def f_mul():
    for a, b in PAIRS:
        r = res(f"std::declval<{T(a)}>() * std::declval<{T(b)}>()")
        yield Pair("×", f"mul/{a}*{b}", [X(a), X(b)], "return a * b;", f"return twin::mul<{r}, {T(a)}, {T(b)}>(a, b);")


@family("unary -", ["inside::member operator-", "inside operator-"])
def f_neg():
    for a in NOTCHED:
        r = res(f"-std::declval<{T(a)}>()")
        yield Pair("unary -", f"neg/{a}", [X(a)], "return -a;", f"return twin::neg<{r}, {T(a)}>(a);")


@family("comparison", ["inside operator<=>", "inside operator==", "math operator<=>", "math operator=="])
def f_cmp():
    for a, b in PAIRS:
        yield Pair("comparison", f"less/{a}<{b}", [X(a), X(b)], "return a < b;",
                   f"return twin::less<{T(a)}, {T(b)}>(a, b);")
        yield Pair("comparison", f"equal/{a}=={b}", [X(a), X(b)], "return a == b;",
                   f"return twin::equal<{T(a)}, {T(b)}>(a, b);")


DIVISORS = {"u8": "{1, 9}", "s32": "{1, 1000}", "q88": "{{1, 16}, per<256>}", "dec2": "{{1, 100}, per<100>}",
            "n60": "{{60, 600}, 60}", "q114": "{{1, 2}, per<16384>}"}


@family("division", ["inside operator/", "inside div", "inside mod", "inside divmod", "inside operator%",
                     "math operator/"])
def f_div():
    for a in ["u8", "s32", "q88", "dec2", "n60", "q114", "idx"]:
        for b in ["u8", "s32", "q88", "dec2", "n60"]:
            nz = f"inside<{DIVISORS[b]}>"
            z = T(b)
            yield Pair("division", f"div_exact/{a}/{b}nz", [X(a), ("x", nz)], "return a / b;",
                       f"return twin::div_exact<{T(a)}, {nz}, false>(a, b);")
            if a == b or b == "u8":
                yield Pair("division", f"div_exact/{a}/{b}", [X(a), ("x", z)], "return a / b;",
                           f"return twin::div_exact<{T(a)}, {z}, true>(a, b);")
                for m in ROUNDING:
                    yield Pair("division", f"div_mode/{a}/{b}nz/{m}", [X(a), ("x", nz)],
                               f"return div(a, b, {POLICIES[m][0]});",
                               f"return twin::div_int<{T(a)}, {nz}, twin::mode::{m}, false>(a, b);")
                yield Pair("division", f"div_mode/{a}/{b}/trunc", [X(a), ("x", z)], "return div(a, b, round_toward_zero);",
                           f"return twin::div_int<{T(a)}, {z}, twin::mode::trunc, true>(a, b);")
                yield Pair("division", f"mod/{a}/{b}nz", [X(a), ("x", nz)], "return a % b;",
                           f"return twin::mod_int<{T(a)}, {nz}>(a, b);")
                yield Pair("division", f"divmod/{a}/{b}nz/floor", [X(a), ("x", nz)], "return divmod(a, b, round_floor);",
                           f"auto q = twin::div_int<{T(a)}, {nz}, twin::mode::floor, false>(a, b); "
                           f"return std::pair{{q, twin::mod_int<{T(a)}, {nz}>(a, b)}};")


OUTS = {"u8": "q88", "q88": "q88", "dec2": "dec2", "s32": "dec3", "n60": "dec2", "q114": "q114"}


@family("mul<Out> and div<Out>", ["inside mul", "inside div", "inside mul_all"])
def f_onto():
    for a, b in [("q88", "q88"), ("dec2", "q88"), ("dec2", "dec2"), ("u8", "u8"), ("s32", "q114"), ("q114", "q114"),
                 ("n60", "dec3")]:
        out = OUTS.get(a, "dec2")
        for p in ["half_away", "floor", "half_even", "clamp_floor"]:
            O = T(out, p)
            mode = POLICIES[p][2]
            rng = POLICIES[p][1]
            yield Pair("mul<Out> and div<Out>", f"mul_onto/{a}*{b}->{out}/{p}", [X(a), X(b)], f"return mul<{O}>(a, b);",
                       f"return twin::mul_onto<{O}, {T(a)}, {T(b)}, twin::range::{rng}, twin::mode::{mode}>(a, b);")
            nz = f"inside<{DIVISORS.get(b, '{1, 9}')}>"
            yield Pair("mul<Out> and div<Out>", f"div_onto/{a}/{b}nz->{out}/{p}", [X(a), ("x", nz)],
                       f"return div<{O}>(a, b);",
                       f"return twin::div_onto<{O}, {T(a)}, {nz}, twin::range::{rng}, twin::mode::{mode}>(a, b);")


# --- compound assignment and steps -------------------------------------------
@family("compound assignment", ["inside::member operator+=", "inside::member operator-=", "inside::member operator*=",
                                "inside::member operator/=", "inside::member operator%=", "inside::member with",
                                "inside::member on_clamp", "inside::member on_wrap", "inside::member on_error"])
def f_compound():
    for s in NOTCHED:
        for p in ["clamp", "wrap", "unsafe", "clamp_floor", "wrap_half_even"]:
            D = T(s, p)
            for op, sign in (("+=", 1), ("-=", -1)):
                tw = (f"auto n = twin::add<{T(s)}, {T(s)}, {T(s)}, {sign}>(a, b); (void)n; "
                      f"return twin::store_inside<{D}, {res(f'std::declval<{T(s)}>() + std::declval<{T(s)}>()')}, "
                      f"{twin_args(p)}>(twin::add<{res(f'std::declval<{T(s)}>() + std::declval<{T(s)}>()')}, {T(s)}, {T(s)}, {sign}>(a, b));")
                yield Pair("compound assignment", f"compound/{s}{op}/{p}", [("x", D), ("x", D)],
                           f"a {op} b; return a;", tw)
            yield Pair("compound assignment", f"compound/{s}*=/{p}", [("x", D), ("x", D)], "a *= b; return a;",
                       f"return twin::store_inside<{D}, {res(f'std::declval<{T(s)}>() * std::declval<{T(s)}>()')}, "
                       f"{twin_args(p)}>(twin::mul<{res(f'std::declval<{T(s)}>() * std::declval<{T(s)}>()')}, {T(s)}, {T(s)}>(a, b));")
        # the channels of a checked type
        C = T(s)
        S = res(f"std::declval<{C}>() + std::declval<{C}>()")
        tw = f"return twin::store_inside<{C}, {S}, twin::range::check, twin::mode::trunc, false>(twin::add<{S}, {C}, {C}, 1>(a, b));"
        yield Pair("compound assignment", f"compound/{s}+=/policy_ec", [X(s), X(s)],
                   "error ec{}; a.on_error(ec) += b; return std::pair{a, ec};", tw)
        yield Pair("compound assignment", f"compound/{s}+=/on_error", [X(s), X(s)],
                   "int n = 0; a.on_error([&](auto&&...) { ++n; }) += b; return std::pair{a, n};", tw)
        yield Pair("compound assignment", f"compound/{s}+=/on_clamp", [("x", T(s, 'clamp')), ("x", T(s, 'clamp'))],
                   "int n = 0; a.on_clamp([&](auto&&...) { ++n; }) += b; return std::pair{a, n};",
                   f"return twin::store_inside<{T(s, 'clamp')}, {S}, twin::range::clamp, twin::mode::trunc, false>(twin::add<{S}, {C}, {C}, 1>(a, b));")
        if s in INTEGER or s in ("q88", "dec2"):
            for p in ["clamp", "wrap"]:
                yield Pair("compound assignment", f"compound/{s}+=int/{p}", [("x", T(s, p)), ("int", (-50, 50))],
                           "a += b; return a;",
                           f"return twin::store_int<{T(s, p)}, {twin_args(p)}>(twin::to_int<{T(s, p)}, long long, twin::mode::trunc>(a).value() + b);")


@family("++ and --", ["inside::member operator++", "inside::member operator--", "cursor::member operator++",
                      "cursor::member operator--"])
def f_step():
    for s in NOTCHED:
        for p in ["clamp", "wrap", "unsafe"]:
            D = T(s, p)
            R = res(f"std::declval<{D}>() + std::declval<{D}>()")
            yield Pair("++ and --", f"incr/{s}/{p}", [("x", D)], "++a; return a;",
                       f"return twin::finish<{D}, twin::range::{POLICIES[p][1]}>(static_cast<long long>(a) + 1);")
        C = T(s)
        yield Pair("++ and --", f"incr/{s}/policy_ec", [X(s)], "error ec{}; a.on_error(ec) += 1_ins; return std::pair{a, ec};",
                   f"return twin::finish<{C}, twin::range::check>(static_cast<long long>(a) + 1);",
                   note="++ has no on_error(ec) form: += 1_ins")


# --- conversions out -----------------------------------------------------------
@family("conversion out", ["inside::member to", "inside to", "inside::member (conversion)", "inside::member raw",
                           "inside::member step", "inside::member from_step", "inside::member from_raw",
                           "inside::member numerator", "inside::member denominator"])
def f_out():
    for s in NOTCHED:
        C = T(s)
        yield Pair("conversion out", f"to_double/{s}", [X(s)], "return double(a);", f"return twin::to_double<{C}>(a);")
        yield Pair("conversion out", f"to_float/{s}", [X(s)], "return a.to<float>();",
                   f"return static_cast<float>(twin::to_double<{C}>(a));", note="twin rounds twice")
        for m in ["trunc", "half_away", "floor"]:
            yield Pair("conversion out", f"to_int/{s}/{m}", [X(s)], f"return to<long long, {POLICIES[m][0]}>(a);",
                       f"return twin::to_int<{C}, long long, twin::mode::{m}>(a);")
        yield Pair("conversion out", f"to_u8/{s}", [X(s)], "return to<std::uint8_t, round_toward_zero>(a);",
                   f"return twin::to_int<{C}, std::uint8_t, twin::mode::trunc>(a);")
        yield Pair("conversion out", f"numerator/{s}", [X(s)], "return std::pair{a.numerator(), a.denominator()};",
                   f"return twin::numerator_denominator<{C}>(a);")
        yield Pair("conversion out", f"step/{s}", [X(s)], "return a.step();",
                   f"return static_cast<long long>(a) - twin::enc<{C}>::lo;")
        yield Pair("conversion out", f"from_step/{s}", [("int", (0, 100))], f"return {C}::from_step(a);",
                   f"return twin::finish<{C}, twin::range::check>(a + twin::enc<{C}>::lo);")


# --- helpers ---------------------------------------------------------------------
@family("min, max, midpoint, lerp, add_all, mul_all", ["inside min", "inside max", "inside midpoint", "inside lerp",
                                                       "inside add_all", "inside mul_all"])
def f_helpers():
    for a, b in [(s, s) for s in NOTCHED] + CROSS[:8]:
        yield Pair("min, max, midpoint, lerp, add_all, mul_all", f"min/{a},{b}", [X(a), X(b)], "return min(a, b);",
                   f"return twin::less<{T(a)}, {T(b)}>(a, b) ? (long long)a : (long long)b;",
                   note="twin returns the raw of the smaller, not re-encoded")
        yield Pair("min, max, midpoint, lerp, add_all, mul_all", f"max/{a},{b}", [X(a), X(b)], "return max(a, b);",
                   f"return twin::less<{T(a)}, {T(b)}>(a, b) ? (long long)b : (long long)a;")
    for s in NOTCHED:
        for p in ["half_away", "floor"]:
            yield Pair("min, max, midpoint, lerp, add_all, mul_all", f"midpoint/{s}/{p}",
                       [("x", T(s, p)), ("x", T(s, p))], "return midpoint(a, b);",
                       f"return twin::rdiv<twin::mode::{POLICIES[p][2]}>((long long)a + (long long)b, 2LL);")
        yield Pair("min, max, midpoint, lerp, add_all, mul_all", f"add_all/{s}", [X(s), X(s), X(s), X(s)],
                   "return add_all(a, b, c, d);", "return (long long)a + b + c + d;")
        yield Pair("min, max, midpoint, lerp, add_all, mul_all", f"mul_all/{s}", [X(s), X(s), X(s)],
                   "return mul_all(a, b, c);", "return (__int128)a * b * c;", note="twin: raws only")
    for s in ["q88", "dec2", "q114", "u8"]:
        t = "inside<{{0, 1}, per<256>}>"
        yield Pair("min, max, midpoint, lerp, add_all, mul_all", f"lerp/{s}", [X(s), X(s), ("x", t)],
                   "return lerp(a, b, c);", "return (long long)a * 256 + ((long long)b - a) * c;",
                   note="twin: exact a + (b − a)·t in raw units")



# --- loops: mdspan algorithms, ranges, cursors --------------------------------
@family("mdspan and range loops", ["inside sum", "inside dot", "inside matrix_product", "inside matrix_vector_product",
                                   "inside transform_into", "inside indices", "inside end", "range::member begin",
                                   "range::member end", "range::member size", "range::member (constructor)",
                                   "range::member operator=", "cursor::member (constructor)", "cursor::member operator+=",
                                   "cursor::member operator-=", "cursor::member operator+", "cursor::member operator-",
                                   "cursor::member operator=", "cursor::member raw", "cursor::member to",
                                   "cursor::member (conversion)", "cursor::member from_raw", "cursor::member from_step",
                                   "cursor::member try_make", "cursor::member with", "cursor::member on_clamp",
                                   "cursor::member on_wrap", "cursor::member on_error", "cursor::member operator*=",
                                   "cursor::member operator/=", "cursor::member operator%=", "units view_in",
                                   "units sum"])
def f_loops():
    F = "mdspan and range loops"
    for s, raw in [("u8", "std::uint8_t"), ("q88", "std::uint16_t"), ("s32", "std::int32_t"), ("dec2", "std::uint32_t")]:
        t = T(s)
        yield Pair(F, f"sum_mdspan/{s}/1024", [("arr", (t, 1024))],
                   "return sum(std::mdspan{a, std::extents<std::size_t, 1024>{}});",
                   "long long s = 0; for (int i = 0; i < 1024; ++i) s += a[i]; return s;", note="per call of 1024")
        yield Pair(F, f"dot_mdspan/{s}/1024", [("arr", (t, 1024)), ("arr", (t, 1024))],
                   "return dot(std::mdspan{a, std::extents<std::size_t, 1024>{}}, std::mdspan{b, std::extents<std::size_t, 1024>{}});",
                   f"__int128 s = 0; for (int i = 0; i < 1024; ++i) s += (__int128)(long long)a[i] * b[i]; return s;",
                   note="twin in raws (index raws need the Lower terms too)")
        yield Pair(F, f"sum_into/{s}/1024", [("arr", (t, 1024))],
                   f"return sum<inside<{{0, 1000000000}}, clamp>>(std::mdspan{{a, std::extents<std::size_t, 1024>{{}}}});",
                   "long long s = 0; for (int i = 0; i < 1024; ++i) s += a[i]; return s;")
    m = T("q114")
    yield Pair(F, "matrix_product/q114/8x8", [("arr", (m, 64)), ("arr", (m, 64))],
               "static std::array<inside<{{-8, 8}, per<16384>}, clamp | round_half_away>, 64> c; "
               "(void)matrix_product(std::mdspan{a, std::extents<std::size_t, 8, 8>{}}, std::mdspan{b, std::extents<std::size_t, 8, 8>{}}, "
               "std::mdspan{c.data(), std::extents<std::size_t, 8, 8>{}}); return c[5].raw();",
               "static std::array<int, 64> c; for (int i = 0; i < 8; ++i) for (int k = 0; k < 8; ++k) { long long s = 0; "
               "for (int j = 0; j < 8; ++j) s += (long long)(a[i * 8 + j] - 32768) * (b[j * 8 + k] - 32768); "
               "long long q = (s + (s < 0 ? -8192 : 8192)) / 16384 + 32768; c[i * 8 + k] = (int)(q < 0 ? 0 : q > 131072 ? 131072 : q); } return c[5];")
    yield Pair(F, "matrix_vector_product/q114/8", [("arr", (m, 64)), ("arr", (m, 8))],
               "static std::array<inside<{{-16, 16}, per<16384>}, clamp | round_half_away>, 8> c; "
               "(void)matrix_vector_product(std::mdspan{a, std::extents<std::size_t, 8, 8>{}}, std::mdspan{b, std::extents<std::size_t, 8>{}}, "
               "std::mdspan{c.data(), std::extents<std::size_t, 8>{}}); return c[5].raw();",
               "static std::array<int, 8> c; for (int i = 0; i < 8; ++i) { long long s = 0; "
               "for (int j = 0; j < 8; ++j) s += (long long)(a[i * 8 + j] - 32768) * (b[j] - 32768); "
               "c[i] = (int)((s + (s < 0 ? -8192 : 8192)) / 16384 + 262144); } return c[5];")
    u = T("u8")
    yield Pair(F, "transform_into/u8/1024", [("arr", (u, 1024)), ("arr", (u, 1024))],
               "static std::array<inside<{0, 400}>, 1024> c; (void)transform_into(std::mdspan{a, std::extents<std::size_t, 1024>{}}, "
               "std::mdspan{b, std::extents<std::size_t, 1024>{}}, std::mdspan{c.data(), std::extents<std::size_t, 1024>{}}, "
               "[](auto x, auto y) { return x + y; }); return c[7].raw();",
               "static std::array<std::uint16_t, 1024> c; for (int i = 0; i < 1024; ++i) c[i] = (std::uint16_t)(a[i] + b[i]); return c[7];")
    yield Pair(F, "indices/32x32", [("custom", ("int a", "int a", "", "j", "j"))],
               "long long s = 0; std::array<int, 1024> buf{}; std::mdspan m{buf.data(), std::extents<std::size_t, 32, 32>{}}; "
               "for (auto [r, c] : indices(m)) s += static_cast<int>(r) * 32 + static_cast<int>(c) + a; return s;",
               "long long s = 0; for (int r = 0; r < 32; ++r) for (int c = 0; c < 32; ++c) s += r * 32 + c + a; return s;")
    for s in ["u8", "q88", "s32"]:
        yield Pair(F, f"values_loop/{s}", [("custom", ("int a", "int a", "", "j", "j"))],
                   f"long long n = 0; for (auto v : values<inside<{{0, 1000}}>>) n += v.raw() ^ a; return n;",
                   "long long n = 0; for (int v = 0; v <= 1000; ++v) n += v ^ a; return n;")
    yield Pair(F, "cursor_loop/1000", [("custom", ("int a", "int a", "", "j", "j"))],
               "long long n = 0; for (cursor<inside<{0, 1000}>> c; c != end(c); ++c) n += (*c).raw() ^ a; return n;",
               "long long n = 0; for (int v = 0; v < 1000; ++v) n += v ^ a; return n;")


# --- units ----------------------------------------------------------------------
@family("quantities", ["quantity::member (constructor)", "quantity::member (conversion)", "quantity::member in",
                       "quantity::member numerical_value_in", "quantity::member operator+=", "quantity::member operator-=",
                       "quantity::member operator*=", "quantity::member operator/=", "quantity::member operator++",
                       "quantity::member operator--", "quantity::member operator=", "quantity::member on_clamp",
                       "quantity::member on_wrap", "quantity::member on_error", "quantity::member with",
                       "quantity::member to", "quantity::member try_make", "quantity::member numerical_value",
                       "units operator+", "units operator-", "units operator*", "units operator/", "units operator%",
                       "units operator<=>", "units operator==", "units value_cast", "units kind_cast", "units exact_cast",
                       "units min", "units max", "units midpoint", "units lerp", "units add_all", "units mul_all",
                       "units mul", "units div", "units mod", "units divmod", "units to", "units conversion_loses",
                       "units conversion_overflows", "units conversion_rounds", "units pow", "units delta", "units point",
                       "point::member (constructor)", "point::member in", "point::member operator+=",
                       "point::member operator-=", "point::member operator=", "point::member point_for",
                       "point::member point_in", "point::member quantity_from", "point::member quantity_from_origin",
                       "point::member quantity_from_zero"])
def f_units():
    F = "quantities"
    m = "quantity<si::metre, inside<{0, 100000}>>"
    km = "quantity<si::kilo(si::metre), inside<{{0, 100}, per<1000>}>>"
    s_ = "quantity<si::second, inside<{1, 1000}>>"
    mn = "quantity<si::minute, inside<{0, 600}>>"
    deg = "quantity<si::degree, inside<{{0, 360}, per<64>}, round_half_away>>"
    rad = "quantity<si::radian, inside<{{0, 8}, per<(1 << 20)>}, round_half_away>>"
    yield Pair(F, "q_add/m+m", [("qn", m), ("qn", m)], "return a + b;", "return a + b;", "inside")
    yield Pair(F, "q_add/m+km", [("qn", m), ("qn", km)], "return a + b;", "return a + b * just<1000>;", "inside")
    yield Pair(F, "q_sub/km-m", [("qn", km), ("qn", m)], "return a - b;", "return a * just<1000> - b;", "inside")
    yield Pair(F, "q_mul/m*s", [("qn", m), ("qn", s_)], "return a * b;", "return a * b;", "inside")
    yield Pair(F, "q_div/m/s", [("qn", m), ("qn", s_)], "return a / b;", "return a / b;", "inside")
    yield Pair(F, "q_less/m<km", [("qn", m), ("qn", km)], "return a < b;", "return a < b * just<1000>;", "inside")
    yield Pair(F, "q_equal/m==km", [("qn", m), ("qn", km)], "return a == b;", "return a == b * just<1000>;", "inside")
    yield Pair(F, "q_in/km->m", [("qn", km)], "return a.in(si::metre);", "return a * just<1000>;", "inside")
    yield Pair(F, "q_numerical_value_in/km->m", [("qn", km)], "return a.numerical_value_in(si::metre);",
               "return a * just<1000>;", "inside")
    yield Pair(F, "q_ctor_other_unit/min<-s", [("int", (0, 36000))],
               f"return {mn}::try_make<round_floor>(a, si::second);",
               "return inside<{{0, 36000}, 60}, round_floor>::try_make(a);", "inside")
    yield Pair(F, "q_ctor_clamp/m", [("int", (-1000, 101000))], f"return {m}{{a, si::metre, clamp}};",
               "return inside<{0, 100000}>{a, clamp};", "inside")
    yield Pair(F, "q_compound/m+=m/clamp", [("qn", f"quantity<si::metre, inside<{{0, 100000}}, clamp>>"),
                                            ("qn", f"quantity<si::metre, inside<{{0, 100000}}, clamp>>")],
               "a += b; return a;", "a += b; return a;", "inside")
    yield Pair(F, "q_compound/m+=km/clamp", [("qn", f"quantity<si::metre, inside<{{0, 100000}}, clamp>>"), ("qn", km)],
               "a += b; return a;", "a += b * just<1000>; return a;", "inside")
    yield Pair(F, "q_policy_ec/m+=m", [("qn", m), ("qn", m)], "error ec{}; a.on_error(ec) += b; return std::pair{a, ec};",
               "error ec{}; a.on_error(ec) += b; return std::pair{a, ec};", "inside")
    yield Pair(F, "q_scale/m*=n", [("qn", f"quantity<si::metre, inside<{{0, 100000}}, clamp>>"), ("int", (0, 9))],
               "a *= b; return a;", "a *= b; return a;", "inside")
    yield Pair(F, "q_value_cast/m->km", [("qn", m)],
               f"return value_cast<quantity<si::kilo(si::metre), inside<{{0, 100}}, round_half_away>>>(a);",
               "return inside<{0, 100}, round_half_away>{a / just<1000>};", "inside")
    yield Pair(F, "q_value_cast_pi/deg->rad", [("qn", deg)], "return value_cast<si::radian, inside<{{0, 8}, per<(1 << 20)>}, round_half_away>>(a);",
               "return inside<{{0, 8}, per<(1 << 20)>}, round_half_away>::try_make(double(a) * (std::numbers::pi / 180));",
               "inside", note="ideal rounds a double product: not correctly rounded")
    yield Pair(F, "q_kind_cast/height", [("qn", "quantity<isq::length[si::metre], inside<{0, 100000}>>")],
               "return kind_cast<isq::height>(a);", "return a;", "inside")
    yield Pair(F, "q_min/m,km", [("qn", m), ("qn", km)], "return min(a, b);", "return min(a, b * just<1000>);", "inside")
    yield Pair(F, "q_div_mode/m/s", [("qn", m), ("qn", s_)], "return div(a, b, round_floor);",
               "return div(a, b, round_floor);", "inside")
    yield Pair(F, "q_to/m->int", [("qn", m)], "return a.numerical_value_in(si::metre).to<long long>();",
               "return a.to<long long>();", "inside")
    yield Pair(F, "q_pow2/m", [("qn", m)], "return pow<2>(a);", "return a * a;", "inside")
    yield Pair(F, "q_incr/s", [("qn", f"quantity<si::second, inside<{{1, 1000}}, clamp>>")], "++a; return a;",
               "++a; return a;", "inside")
    # points: temperatures
    kp = "quantity_point<si::kelvin, si::absolute_zero, inside<{{0, 400}, per<100>}>>"
    yield Pair(F, "point_plus/K+dK", [("custom", (f"{kp} a", f"inside<{{{{0, 400}}, per<100>}}> a",
               f"std::vector<{kp}> xa(1024); std::vector<inside<{{{{0, 400}}, per<100>}}>> ra(1024); "
               f"for (int j = 0; j < 1024; ++j) {{ ra[j] = sample<inside<{{{{0, 400}}, per<100>}}>>(g); "
               f"xa[j] = {kp}{{iu::detail::quantity_access::make<quantity<si::kelvin, inside<{{{{0, 400}}, per<100>}}>>>(ra[j])}}; }}",
               "xa[j]", "ra[j]"))],
               "return a + just<1> * si::kelvin;", "return a + just<1>;", "inside")
    yield Pair(F, "point_in/K->degC", [("custom", (f"{kp} a", f"inside<{{{{0, 400}}, per<100>}}> a",
               f"std::vector<{kp}> xa(1024); std::vector<inside<{{{{0, 400}}, per<100>}}>> ra(1024); "
               f"for (int j = 0; j < 1024; ++j) {{ ra[j] = sample<inside<{{{{0, 400}}, per<100>}}>>(g); "
               f"xa[j] = {kp}{{iu::detail::quantity_access::make<quantity<si::kelvin, inside<{{{{0, 400}}, per<100>}}>>>(ra[j])}}; }}",
               "xa[j]", "ra[j]"))],
               "return a.quantity_from(si::ice_point);", "return a - just<27315, 100>;", "inside")


# --- geometric algebra -----------------------------------------------------------
@family("geometric algebra", ["units wedge", "units left_contraction", "units right_contraction", "units scalar_product",
                              "units reverse", "units involute", "units conjugate", "units grade", "units squared_norm",
                              "units inverse", "units dual", "units complement", "units uncomplement", "units regressive",
                              "units part", "multivector::member (constructor)", "multivector::member in",
                              "multivector::member operator=", "multivector::member parts"])
def f_ga():
    F = "geometric algebra"
    pre = ("inline constexpr auto e1 = iu::basis_vector<\"e1\", +1>; inline constexpr auto e2 = iu::basis_vector<\"e2\", +1>; "
           "inline constexpr auto e3 = iu::basis_vector<\"e3\", +1>;")
    P = "inside<{-100, 100}>"
    cx = f"iu::multivector<{P}, iu::quantity<e1 * e2, {P}>>"
    vec = f"iu::multivector<iu::quantity<e1, {P}>, iu::quantity<e2, {P}>, iu::quantity<e3, {P}>>"
    def mk(t, n):
        parts = ", ".join(f"r{c}[j]" for c in "abcd"[:n])
        return t, parts
    cx_setup = (f"std::vector<{cx}> xa(1024), xb(1024); std::vector<std::array<{P}, 2>> ra(1024), rb(1024); "
                f"for (int j = 0; j < 1024; ++j) {{ ra[j] = {{sample<{P}>(g), sample<{P}>(g)}}; rb[j] = {{sample<{P}>(g), sample<{P}>(g)}}; "
                f"xa[j] = {cx}{{ra[j][0], ra[j][1] * (e1 * e2)}}; xb[j] = {cx}{{rb[j][0], rb[j][1] * (e1 * e2)}}; }}")
    two = [("custom", (f"{cx} a", f"std::array<{P}, 2> a", cx_setup, "xa[j]", "ra[j]")),
           ("custom", (f"{cx} b", f"std::array<{P}, 2> b", "", "xb[j]", "rb[j]"))]
    yield Pair(F, "ga_mul/complex", two, "return a * b;",
               "return std::pair{a[0] * b[0] - a[1] * b[1], a[0] * b[1] + a[1] * b[0]};", "inside", note=pre)
    yield Pair(F, "ga_add/complex", two, "return a + b;", "return std::pair{a[0] + b[0], a[1] + b[1]};", "inside", note=pre)
    yield Pair(F, "ga_squared_norm/complex", two[:1], "return squared_norm(a);",
               "return a[0] * a[0] + a[1] * a[1];", "inside", note=pre)
    yield Pair(F, "ga_reverse/complex", two[:1], "return reverse(a);", "return std::pair{a[0], -a[1]};", "inside", note=pre)
    v_setup = (f"std::vector<{vec}> xa(1024), xb(1024); std::vector<std::array<{P}, 3>> ra(1024), rb(1024); "
               f"for (int j = 0; j < 1024; ++j) {{ for (int k = 0; k < 3; ++k) {{ ra[j][k] = sample<{P}>(g); rb[j][k] = sample<{P}>(g); }} "
               f"xa[j] = {vec}{{ra[j][0] * e1, ra[j][1] * e2, ra[j][2] * e3}}; xb[j] = {vec}{{rb[j][0] * e1, rb[j][1] * e2, rb[j][2] * e3}}; }}")
    vv = [("custom", (f"{vec} a", f"std::array<{P}, 3> a", v_setup, "xa[j]", "ra[j]")),
          ("custom", (f"{vec} b", f"std::array<{P}, 3> b", "", "xb[j]", "rb[j]"))]
    yield Pair(F, "ga_wedge/vec3", vv, "return wedge(a, b);",
               "return std::tuple{a[0] * b[1] - a[1] * b[0], a[1] * b[2] - a[2] * b[1], a[0] * b[2] - a[2] * b[0]};",
               "inside", note=pre)
    yield Pair(F, "ga_scalar_product/vec3", vv, "return scalar_product(a, b);",
               "return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];", "inside", note=pre)
    yield Pair(F, "ga_mul/vec3", vv, "return a * b;",
               "return std::tuple{a[0] * b[0] + a[1] * b[1] + a[2] * b[2], a[0] * b[1] - a[1] * b[0], "
               "a[1] * b[2] - a[2] * b[1], a[0] * b[2] - a[2] * b[0]};", "inside", note=pre)


# --- text ------------------------------------------------------------------------
@family("text", ["inside to_string", "inside parse", "inside parse_exact", "inside operator<<", "inside operator>>",
                 "units to_string", "units parse", "units parse_exact", "units operator<<"])
def f_text():
    F = "text"
    for s in ["u8", "s32", "q88", "dec2", "q114"]:
        t = T(s)
        kind = SHAPES[s][1]
        if kind == "int-value":
            tw_out = "char buf[24]; auto r = std::to_chars(buf, buf + 24, (long long)a); return std::string(buf, r.ptr);"
            tw_in = ("long long v = 0; auto r = std::from_chars(a.data(), a.data() + a.size(), v); "
                     "if (r.ec != std::errc{}) return std::expected<long long, int>{std::unexpect, 1}; return std::expected<long long, int>{v};")
        else:
            D = {"q88": 256, "dec2": 100, "q114": 16384}[s]
            lo = {"q88": 0, "dec2": 0, "q114": -2}[s]
            if s == "dec2":
                tw_out = ("char buf[32]; long long v = a; auto r = std::to_chars(buf, buf + 32, v / 100); *r.ptr++ = '.'; "
                          "int f = (int)(v % 100); *r.ptr++ = char('0' + f / 10); *r.ptr++ = char('0' + f % 10); return std::string(buf, r.ptr);")
            else:
                tw_out = (f"char buf[40]; long long n = (long long)a + ({lo}LL * {D}); bool neg = n < 0; if (neg) n = -n; "
                          f"char* p = buf; if (neg) *p++ = '-'; auto r = std::to_chars(p, buf + 40, n / {D}); p = r.ptr; "
                          f"long long f = n % {D}; if (f) {{ *p++ = '.'; while (f) {{ f *= 10; *p++ = char('0' + f / {D}); f %= {D}; }} }} "
                          f"return std::string(buf, p);")
            tw_in = (f"long long ip = 0, fp = 0, sc = 1; const char* e = a.data() + a.size(); auto r = std::from_chars(a.data(), e, ip); "
                     f"if (r.ptr != e && *r.ptr == '.') {{ for (const char* p = r.ptr + 1; p != e; ++p) {{ fp = fp * 10 + (*p - '0'); sc *= 10; }} }} "
                     f"return std::expected<long long, int>{{(ip * sc + fp) * {D} / sc}};")
        yield Pair(F, f"to_string/{s}", [X(s)], "return to_string(a);", tw_out, "to_chars")
        yield Pair(F, f"parse/{s}", [("str", t)], f"return parse<{t}>(a);", tw_in, "from_chars")
        yield Pair(F, f"format/{s}", [X(s)], 'return std::format("{}", a);', "return std::format(\"{}\", (long long)a);",
                   "format", note="ideal formats the raw integer")
        yield Pair(F, f"ostream/{s}", [X(s)], "static std::ostringstream os; os.str({}); os << a; return os.tellp();",
                   "static std::ostringstream os; os.str({}); os << (long long)a; return os.tellp();", "ostream")
    m = "quantity<si::metre, inside<{{0, 100000}, per<100>}>>"
    yield Pair(F, "to_string/quantity", [("qn", m)], "return to_string(a);",
               "auto s = to_string(a); s += \" m\"; return s;", "inside")


# --- grids past 64 bits ---------------------------------------------------------
@family("grids past 64 bits", [])
def f_wide():
    F = "grids past 64 bits"
    for bits in [100, 200, 1000]:
        t = f"inside<{{0, bd::grid_rational{{bd::big_int{{1}} << {bits}}}}}>"
        mp = ("custom", (f"{t} a", "const __mpz_struct* a",
                         f"std::vector<{t}> xa(1024); std::vector<mpz_t> ra(1024); for (int j = 0; j < 1024; ++j) {{ "
                         f"xa[j] = sample<{t}>(g); mpz_init(ra[j]); mpz_import(ra[j], sizeof(xa[j].raw()) / 8, -1, 8, 0, 0, &xa[j].raw()); }}",
                         "xa[j]", "ra[j]"))
        mp2 = ("custom", (f"{t} b", "const __mpz_struct* b",
                          f"std::vector<{t}> xb(1024); std::vector<mpz_t> rb(1024); for (int j = 0; j < 1024; ++j) {{ "
                          f"xb[j] = sample<{t}>(g); mpz_init(rb[j]); mpz_import(rb[j], sizeof(xb[j].raw()) / 8, -1, 8, 0, 0, &xb[j].raw()); }}",
                          "xb[j]", "rb[j]"))
        for op, gmp in [("+", "mpz_add"), ("-", "mpz_sub"), ("*", "mpz_mul")]:
            yield Pair(F, f"wide{op}/{bits}", [mp, mp2], f"return a {op} b;",
                       f"static mpz_t r; static bool i = (mpz_init2(r, 2 * {bits} + 64), true); (void)i; {gmp}(r, a, b); return r->_mp_d[0];",
                       "gmp")
        yield Pair(F, f"wide_less/{bits}", [mp, mp2], "return a < b;", "return mpz_cmp(a, b) < 0;", "gmp")
        yield Pair(F, f"wide_div/{bits}", [mp, mp2], "return div(a, b, round_toward_zero);",
                   f"static mpz_t r; static bool i = (mpz_init2(r, {bits} + 64), true); (void)i; if (!mpz_sgn(b)) return 0UL; "
                   f"mpz_tdiv_q(r, a, b); return r->_mp_d[0];", "gmp")
        yield Pair(F, f"wide_to_string/{bits}", [mp], "return to_string(a);",
                   "static char buf[400]; mpz_get_str(buf, 10, a); return std::string(buf);", "gmp")
    for bits in [65, 100, 127]:
        t = f"inside<{{0, bd::grid_rational{{bd::big_int{{1}} << {bits}}}}}>"
        u = ("custom", (f"{t} a", "unsigned __int128 a",
                        f"std::vector<{t}> xa(1024); std::vector<unsigned __int128> ra(1024); for (int j = 0; j < 1024; ++j) {{ "
                        f"xa[j] = sample<{t}>(g); std::memcpy(&ra[j], &xa[j].raw(), 16); }}", "xa[j]", "ra[j]"))
        u2 = ("custom", (f"{t} b", "unsigned __int128 b",
                         f"std::vector<{t}> xb(1024); std::vector<unsigned __int128> rb(1024); for (int j = 0; j < 1024; ++j) {{ "
                         f"xb[j] = sample<{t}>(g); std::memcpy(&rb[j], &xb[j].raw(), 16); }}", "xb[j]", "rb[j]"))
        yield Pair(F, f"wide+/{bits}", [u, u2], "return a + b;",
                   "unsigned __int128 s = a + b; return std::pair{s, s < a};", "int128")
        yield Pair(F, f"wide_less/{bits}", [u, u2], "return a < b;", "return a < b;", "int128")


# --- integer functions, random -----------------------------------------------------
@family("integer functions", ["math factorial", "math binomial", "math falling_factorial", "math rising_factorial",
                              "math multinomial", "math gcd", "math lcm"])
def f_integer():
    F = "integer functions"
    n20 = "inside<{0, 20}>"
    yield Pair(F, "factorial/0..20", [("xn", n20)], "return math::factorial(a);",
               "static constexpr auto t = [] { std::array<unsigned long long, 21> f{1}; for (int i = 1; i < 21; ++i) f[i] = f[i - 1] * i; return f; }(); "
               "return t[a.raw()];", "table")
    n60 = "inside<{0, 60}>"
    yield Pair(F, "binomial/60", [("xn", n60), ("xn", "inside<{0, 30}>")], "return math::binomial(a, b);",
               "unsigned long long n = a.raw(), k = b.raw(); if (k > n) return 0ULL; unsigned long long r = 1; "
               "for (unsigned long long i = 1; i <= k; ++i) r = r * (n - k + i) / i; return r;", "loop")
    yield Pair(F, "falling_factorial/20,5", [("xn", n20), ("xn", "inside<{0, 5}>")], "return math::falling_factorial(a, b);",
               "unsigned long long r = 1; for (int i = 0; i < (int)b.raw(); ++i) r *= a.raw() - i; return r;", "loop")
    yield Pair(F, "rising_factorial/20,5", [("xn", n20), ("xn", "inside<{0, 5}>")], "return math::rising_factorial(a, b);",
               "unsigned long long r = 1; for (int i = 0; i < (int)b.raw(); ++i) r *= a.raw() + i; return r;", "loop")
    yield Pair(F, "multinomial/3x10", [("xn", "inside<{0, 10}>"), ("xn", "inside<{0, 10}>"), ("xn", "inside<{0, 10}>")],
               "return math::multinomial(a, b, c);",
               "auto bin = [](unsigned long long n, unsigned long long k) { unsigned long long r = 1; "
               "for (unsigned long long i = 1; i <= k; ++i) r = r * (n - k + i) / i; return r; }; "
               "return bin(a.raw() + b.raw(), b.raw()) * bin(a.raw() + b.raw() + c.raw(), c.raw());", "loop")
    s32 = "inside<{-100000, 100000}>"
    yield Pair(F, "gcd/s32", [("xn", s32), ("xn", s32)], "return math::gcd(a, b);",
               "return std::gcd((long long)a.raw(), (long long)b.raw());", "std::gcd")
    yield Pair(F, "lcm/1..1000", [("xn", "inside<{1, 1000}>"), ("xn", "inside<{1, 1000}>")], "return math::lcm(a, b);",
               "return std::lcm((long long)a.raw(), (long long)b.raw());", "std::lcm")


@family("random", ["inside uniform", "units uniform"])
def f_random():
    F = "random"
    rng = ("custom", ("int a", "int a", "", "j", "j"))
    for s in ["u8", "s32", "q88", "dec2", "n60"]:
        t = T(s)
        yield Pair(F, f"uniform/{s}", [rng],
                   f"static std::mt19937_64 e{{a}}; return uniform<{t}>(e);",
                   f"static std::mt19937_64 e{{a}}; constexpr unsigned long long n = (unsigned long long)twin::enc<{t}>::hi - twin::enc<{t}>::lo + 1; "
                   f"unsigned __int128 m = (unsigned __int128)e() * n; unsigned long long l = (unsigned long long)m; "
                   f"if (l < n) {{ const unsigned long long th = -n % n; while (l < th) {{ m = (unsigned __int128)e() * n; l = (unsigned long long)m; }} }} "
                   f"return (long long)(m >> 64) + twin::enc<{t}>::lo;", "Lemire")
    yield Pair(F, "uniform/quantity", [rng],
               "static std::mt19937_64 e{a}; return uniform<quantity<si::metre, inside<{0, 1000}>>>(e);",
               "static std::mt19937_64 e{a}; return uniform<inside<{0, 1000}>>(e);", "inside")


# --- math ------------------------------------------------------------------------
MATH1 = [  # name, input grid, glibc, CORE-MATH
    ("sqrt", "{{0, 4}, per<65536>}", "std::sqrt", None),
    ("cbrt", "{{-8, 8}, per<16384>}", "std::cbrt", "cr_cbrt"),
    ("exp", "{{-10, 10}, per<16384>}", "std::exp", "cr_exp"),
    ("exp2", "{{-4, 4}, per<16384>}", "std::exp2", "cr_exp2"),
    ("log", "{{just<1, 256>, 256}, per<256>}", "std::log", "cr_log"),
    ("log2", "{{just<1, 256>, 256}, per<16384>}", "std::log2", "cr_log2"),
    ("log10", "{{just<1, 256>, 256}, per<256>}", "std::log10", "cr_log10"),
    ("sin", "{{-8, 8}, per<16384>}", "std::sin", "cr_sin"),
    ("cos", "{{-8, 8}, per<16384>}", "std::cos", "cr_cos"),
    ("tan", "{{just<-3, 4>, just<3, 4>}, per<16384>}", "std::tan", "cr_tan"),
    ("asin", "{{-1, 1}, per<16384>}", "std::asin", "cr_asin"),
    ("acos", "{{-1, 1}, per<16384>}", "std::acos", "cr_acos"),
    ("atan", "{{-1, 1}, per<16384>}", "std::atan", "cr_atan"),
    ("sinh", "{{-10, 10}, per<16384>}", "std::sinh", "cr_sinh"),
    ("cosh", "{{-10, 10}, per<16384>}", "std::cosh", "cr_cosh"),
    ("tanh", "{{-10, 10}, per<16384>}", "std::tanh", "cr_tanh"),
    ("asinh", "{{-8, 8}, per<16384>}", "std::asinh", "cr_asinh"),
    ("acosh", "{{1, 8}, per<16384>}", "std::acosh", "cr_acosh"),
    ("atanh", "{{just<-15, 16>, just<15, 16>}, per<16384>}", "std::atanh", "cr_atanh"),
]
MATH_OUT = [("2^-20", "per<(1 << 20)>"), ("2^-40", "per<(std::int64_t{1} << 40)>"),
            ("2^-52", "per<(std::int64_t{1} << 52)>"), ("1e-6", "per<1000000>")]


@family("math functions", ["math " + n for n in ["abs", "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh", "cbrt",
                                                 "ceil", "copysign", "cos", "cosh", "exp", "exp2", "floor", "fmod", "hypot",
                                                 "log", "log10", "log2", "pow", "pow_base", "pown", "round", "sign", "sin",
                                                 "sinh", "sqrt", "tan", "tanh", "trunc"]])
def f_math():
    F = "math functions"
    for name, grid, glibc, cr in MATH1:
        t = f"inside<{grid}, round_half_away>"
        refs = [("glibc", glibc)] + ([("core-math", cr)] if cr else [])
        outs = [("deduced", None)] + MATH_OUT
        for out_name, spec in outs:
            for mode in (["half_away", "floor"] if out_name in ("2^-20", "2^-52") else ["half_away"]):
                flag = POLICIES[mode][0]
                call = f"math::{name}(a)" if spec is None else f"math::{name}<{spec}, {flag}>(a)"
                O = f"unexp_t<decltype({call.replace('(a)', f'(std::declval<{t}>())')})>"
                for ref, fn in refs:
                    yield Pair(F, f"{name}/{out_name}/{mode}/{ref}", [X(grid.replace(grid, grid)) if False else ("x", t)],
                               f"return {call};",
                               f"return twin::store_double<{O}, twin::range::none, twin::mode::{mode}>({fn}(twin::to_double<{t}>(a)));",
                               ref, note="the reference rounds its double once more onto the grid")
    for name, a_grid, b_grid, glibc, cr in [("atan2", "{{-1, 1}, per<16384>}", "{{-1, 1}, per<16384>}", "std::atan2", "cr_atan2"),
                                            ("hypot", "{{-8, 8}, per<16384>}", "{{0, 4}, per<16384>}", "std::hypot", "cr_hypot"),
                                            ("pow", "{{1, 16}, per<65536>}", "{{-2, 2}, per<16384>}", "std::pow", "cr_pow")]:
        ta, tb = f"inside<{a_grid}, round_half_away>", f"inside<{b_grid}, round_half_away>"
        for out_name, spec in [("deduced", None), ("2^-20", "per<(1 << 20)>"), ("2^-52", "per<(std::int64_t{1} << 52)>")]:
            call = f"math::{name}(a, b)" if spec is None else f"math::{name}<{spec}>(a, b)"
            O = f"unexp_t<decltype({call.replace('(a, b)', f'(std::declval<{ta}>(), std::declval<{tb}>())')})>"
            for ref, fn in [("glibc", glibc), ("core-math", cr)]:
                yield Pair(F, f"{name}/{out_name}/half_away/{ref}", [("x", ta), ("x", tb)], f"return {call};",
                           f"return twin::store_double<{O}, twin::range::none, twin::mode::half_away>("
                           f"{fn}(twin::to_double<{ta}>(a), twin::to_double<{tb}>(b)));", ref)
    alg = "inside<{{-8, 8}, per<16384>}, round_half_away>"
    for name, tw in [("abs", "return a < 131072 ? 262144 - a : a;"), ("floor", "return (a >> 14);"),
                     ("ceil", "return (a + 16383) >> 14;"), ("round", "return (a + 8192) >> 14;"),
                     ("trunc", "return a < 131072 ? (a + 16383) >> 14 : a >> 14;"),
                     ("sign", "return (a > 131072) - (a < 131072);")]:
        yield Pair(F, f"{name}/q114x", [("x", alg)], f"return math::{name}(a);", tw, "twin")
    yield Pair(F, "pown3/q114x", [("x", alg)], "return math::pown<3>(a);",
               "long long v = (long long)a - 131072; return v * v * v;", "twin")
    yield Pair(F, "copysign/q114x", [("x", alg), ("x", alg)], "return math::copysign(a, b);",
               "long long m = (long long)a - 131072; m = m < 0 ? -m : m; return b < 131072 ? -m : m;", "twin")
    yield Pair(F, "fmod/q114x", [("x", alg), ("x", "inside<{{1, 4}, per<16384>}, round_half_away>")],
               "return math::fmod(a, b);", "long long x = (long long)a - 131072; return x % (long long)b;", "twin")
    yield Pair(F, "pow_base10/q114x", [("x", "inside<{{-4, 4}, per<16384>}, round_half_away>")],
               "return math::pow_base<10>(a);",
               "return twin::store_double<unexp_t<decltype(math::pow_base<10>(std::declval<inside<{{-4, 4}, per<16384>}, round_half_away>>()))>, "
               "twin::range::none, twin::mode::half_away>(std::pow(10.0, twin::to_double<inside<{{-4, 4}, per<16384>}, round_half_away>>(a)));",
               "glibc")
    deg = "quantity<si::degree, inside<{{0, 360}, per<64>}, round_half_away>>"
    yield Pair(F, "sin_degrees/2^-20", [("q", deg)], "return math::sin<per<(1 << 20)>>(a);",
               "return twin::store_double<inside<{{-1, 1}, per<(1 << 20)>}, round_half_away>, twin::range::none, twin::mode::half_away>("
               "std::sin(static_cast<double>(a) * (std::numbers::pi / (64 * 180))));", "glibc")
    area = "quantity<si::metre * si::metre, inside<{{0, 10000}, per<256>}, round_half_away>>"
    yield Pair(F, "sqrt_quantity/area", [("q", area)], "return math::sqrt(a);",
               "return twin::store_double<inside<{{0, 100}, per<256>}, round_half_away>, twin::range::none, twin::mode::half_away>("
               "std::sqrt(static_cast<double>(a) / 256));", "glibc")

GENERATORS = list(FAMILIES)

# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------
PRELUDE = r'''#include <beman/inside/inside.hpp>
#include <beman/inside/inside/formats.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/mdspan.hpp>
#include <beman/inside/inside/random.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/math.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/mdspan.hpp>
#include <beman/inside/units/random.hpp>
#include <beman/inside/math/integer.hpp>
#include "tests/beman/inside/perf_gate/twins.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <numbers>
#include <tuple>
#include <cmath>
#include <format>
#include <gmp.h>
#include <mdspan>
#include <sstream>
#include <string>
#include <string_view>
extern "C" {
double cr_exp(double); double cr_exp2(double); double cr_exp10(double); double cr_log(double); double cr_log2(double);
double cr_log10(double); double cr_sin(double); double cr_cos(double); double cr_tan(double); double cr_asin(double);
double cr_acos(double); double cr_atan(double); double cr_atan2(double, double); double cr_sinh(double);
double cr_cosh(double); double cr_tanh(double); double cr_asinh(double); double cr_acosh(double); double cr_atanh(double);
double cr_cbrt(double); double cr_pow(double, double); double cr_hypot(double, double);
}
#include <cstdint>
#include <cstdio>
#include <expected>
#include <random>
#include <utility>
#include <vector>
using namespace beman::inside;
using namespace beman::inside::units;
namespace bd = beman::inside::detail;
namespace iu = beman::inside::units;
namespace math = beman::inside::math;
#define K extern "C" [[gnu::noinline]]
// The raw a twin gets: an integer raw as long long, a fraction as twin::fraction.
template <class X>
auto raw_arg(const X& x) {
    if constexpr (std::same_as<bd::raw_t<X>, bd::rational>)
        return twin::fraction{x.raw().Num, x.raw().Den};
    else
        return x.raw();
}
template <class X>
using raw_arg_t = decltype(raw_arg(std::declval<const X&>()));
// A quantity whose number is uniform over its slots.
template <class Q>
Q qsample(std::mt19937_64& g);
// Inputs: a value of X uniform over its slots.
template <class X>
X sample(std::mt19937_64& g) {
    if constexpr (std::same_as<bd::raw_t<X>, bd::rational>) {
        constexpr std::int64_t lo = bd::lower_int64<X>, hi = bd::upper_int64<X>;
        const std::int64_t     d  = 1 + static_cast<std::int64_t>(g() % 1000);
        const std::int64_t     n  = lo * d + static_cast<std::int64_t>(g() % static_cast<std::uint64_t>((hi - lo) * d + 1));
        return *X::try_make(bd::rational{n, d});
    } else if constexpr (bd::wide_index_storage<X>) { // a slot of up to 128 random bits, folded into range
        using W = typename X::raw_type;
        W r{g()};
        r = (r << 64) | W{g()};
        return X::from_raw(r % (W{static_cast<W>(bd::raw_hi_exact<X>)} + W{1}), unsafe);
    } else {
        constexpr std::uint64_t m = bd::max_index_of<X>;
        const std::uint64_t     s = m == ~std::uint64_t{0} ? g() : g() % (m + 1);
        return X::from_raw(bd::raw_of_slot<X>(static_cast<std::int64_t>(s)), unsafe);
    }
}
template <class Q>
Q qsample(std::mt19937_64& g) {
    return iu::detail::quantity_access::make<Q>(sample<typename Q::rep>(g));
}
// The value type of a result that may be an expected.
template <class T>
struct unexp {
    using type = T;
};
template <class T, class E>
struct unexp<std::expected<T, E>> {
    using type = T;
};
template <class T>
using unexp_t = typename unexp<std::remove_cvref_t<T>>::type;
inline constexpr auto e1 = iu::basis_vector<"e1", +1>;
inline constexpr auto e2 = iu::basis_vector<"e2", +1>;
inline constexpr auto e3 = iu::basis_vector<"e3", +1>;
'''


def cpp_args(p):
    real, ideal, setup, real_call, ideal_call = [], [], [], [], []
    for i, (kind, ty) in enumerate(p.args):
        v = "abcdefgh"[i]
        if kind == "x":
            real.append(f"{ty} {v}")
            ideal.append(f"raw_arg_t<{ty}> {v}")
            setup.append(f"std::vector<{ty}> x{v}(1024); std::vector<raw_arg_t<{ty}>> r{v}(1024); "
                         f"for (int j = 0; j < 1024; ++j) {{ x{v}[j] = sample<{ty}>(g); r{v}[j] = raw_arg(x{v}[j]); }}")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"r{v}[j]")
        elif kind == "int":
            lo, hi = ty
            real.append(f"long long {v}")
            ideal.append(f"long long {v}")
            setup.append(f"std::vector<long long> x{v}(1024); for (auto& e : x{v}) e = {lo}LL + static_cast<long long>(g() % "
                         f"static_cast<std::uint64_t>({hi}LL - {lo}LL + 1));")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"x{v}[j]")
        elif kind == "intval":  # an integer value of the grid `ty`
            real.append(f"long long {v}")
            ideal.append(f"long long {v}")
            setup.append(f"std::vector<long long> x{v}(1024); for (auto& e : x{v}) e = to<long long, round_floor>(sample<{ty}>(g));")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"x{v}[j]")
        elif kind == "q":  # a quantity; the twin gets its number's raw
            real.append(f"{ty} {v}")
            ideal.append(f"raw_arg_t<typename {ty}::rep> {v}")
            setup.append(f"std::vector<{ty}> x{v}(1024); std::vector<raw_arg_t<typename {ty}::rep>> r{v}(1024); "
                         f"for (int j = 0; j < 1024; ++j) {{ x{v}[j] = qsample<{ty}>(g); r{v}[j] = "
                         f"raw_arg(iu::detail::value_of(x{v}[j])); }}")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"r{v}[j]")
        elif kind == "qn":  # a quantity; the twin gets its number (an inside)
            real.append(f"{ty} {v}")
            ideal.append(f"typename {ty}::rep {v}")
            setup.append(f"std::vector<{ty}> x{v}(1024); std::vector<typename {ty}::rep> r{v}(1024); "
                         f"for (int j = 0; j < 1024; ++j) {{ x{v}[j] = qsample<{ty}>(g); r{v}[j] = iu::detail::value_of(x{v}[j]); }}")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"r{v}[j]")
        elif kind == "xn":  # an inside for both
            real.append(f"{ty} {v}")
            ideal.append(f"{ty} {v}")
            setup.append(f"std::vector<{ty}> x{v}(1024); for (auto& e : x{v}) e = sample<{ty}>(g);")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"x{v}[j]")
        elif kind == "arr":  # N insides; the twin gets their raws
            t, n = ty
            real.append(f"const {t}* {v}")
            ideal.append(f"const raw_arg_t<{t}>* {v}")
            setup.append(f"std::vector<{t}> x{v}({n}); std::vector<raw_arg_t<{t}>> r{v}({n}); "
                         f"for (int j = 0; j < {n}; ++j) {{ x{v}[j] = sample<{t}>(g); r{v}[j] = raw_arg(x{v}[j]); }}")
            real_call.append(f"x{v}.data()")
            ideal_call.append(f"r{v}.data()")
        elif kind == "str":  # the text of a value of `ty`
            real.append(f"std::string_view {v}")
            ideal.append(f"std::string_view {v}")
            setup.append(f"std::vector<std::string> x{v}(1024); for (auto& e : x{v}) e = to_string(sample<{ty}>(g));")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"x{v}[j]")
        elif kind == "custom":  # (real param, ideal param, setup, real arg, ideal arg)
            rp, ip, st, ra, ia = ty
            real.append(rp)
            ideal.append(ip)
            setup.append(st)
            real_call.append(ra)
            ideal_call.append(ia)
        elif kind == "dbl":
            lo, hi = ty
            real.append(f"double {v}")
            ideal.append(f"double {v}")
            setup.append(f"std::vector<double> x{v}(1024); for (auto& e : x{v}) e = {lo}.0 + ({hi}.0 - {lo}.0) * "
                         f"static_cast<double>(g() >> 11) * 0x1p-53;")
            real_call.append(f"x{v}[j]")
            ideal_call.append(f"x{v}[j]")
    return real, ideal, setup, real_call, ideal_call


def gen_batch(pairs, base):
    src = [PRELUDE]
    drv = []
    line = PRELUDE.count("\n") + 1
    for p in pairs:
        n = p.n
        real, ideal, setup, rc, ic = cpp_args(p)
        p.line = line
        line += 3
        src.append(f"// {p.ident}\nK auto k_{n}({', '.join(real)}) {{ {p.real} }}\n"
                   f"K auto t_{n}({', '.join(ideal)}) {{ {p.ideal} }}\n")
        drv.append(f"    if (want({n})) {{ {' '.join(setup)}\n"
                   f"      run(\"k_{n}\", [&](int j) {{ return k_{n}({', '.join(rc)}); }});\n"
                   f"      run(\"t_{n}\", [&](int j) {{ return t_{n}({', '.join(ic)}); }}); }}")
    src.append(r'''
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
#include <cstdlib>
#include <cstring>
static int g_only = -1;
static bool want(int n) { return g_only < 0 || g_only == n; }
K int k_empty(int x) { return x; }
template <class F>
static void run(const char* name, F f) {
    ankerl::nanobench::Bench b;
    b.warmup(1000).minEpochIterations(20000).epochs(9).output(nullptr);
    unsigned j = 0;
    b.run(name, [&] { ankerl::nanobench::doNotOptimizeAway(f(static_cast<int>(++j & 1023))); });
    const auto& r = b.results().front();
    using M = ankerl::nanobench::Result::Measure;
    std::printf("%s %.4f %.3f %.3f\n", name, r.median(M::elapsed) * 1e9, r.median(M::instructions), r.median(M::cpucycles));
}
int main(int argc, char** argv) {
    if (argc > 1) g_only = std::atoi(argv[1]);
    std::mt19937_64 g{42};
    if (g_only < 0) run("empty", [&](int j) { return k_empty(j); });
''')
    src.append("\n".join(drv))
    src.append("\n}\n")
    path = OUT / f"{base}.cpp"
    path.write_text("".join(src))
    return path


def parse_asm(text):
    """Per kernel (k_N, t_N, k_empty) and its .cold clone: the instructions,
    integer and floating divisions, calls, stack protector, stack accesses."""
    funcs, cur, cold = {}, None, False
    for line in text.splitlines():
        m = re.match(r"^([A-Za-z_][\w.$]*):", line)
        if m:
            name = m.group(1)
            k = re.match(r"^([kt]_\d+|k_empty)(\.cold)?$", name)
            if k:
                cur, cold = k.group(1), bool(k.group(2))
                funcs.setdefault(cur, {"ins": 0, "div": 0, "fdiv": 0, "call": 0, "canary": 0, "stack": 0, "cold": 0,
                                       "callees": []})
            else:
                cur = None  # another function
            continue
        if cur is None:
            continue
        s = line.strip()
        if s.startswith(".size") or s.startswith(".section") and not cold and ".text.unlikely" not in s:
            if s.startswith(".size"):
                cur = None
            continue
        if not s or s.startswith(".") or s.endswith(":"):
            continue
        f = funcs[cur]
        op = s.split()[0]
        if op.startswith("call") or op == "jmp" and "@" not in s and not s.split()[-1].startswith(".L"):
            target = s.split(None, 1)[1] if len(s.split(None, 1)) > 1 else ""
            if not target.startswith(".L") and "__stack_chk_fail" not in target:
                f["callees"].append(target.split("@")[0])
        if cold:
            f["cold"] += 1
            continue
        f["ins"] += 1
        op = s.split()[0]
        if re.match(r"^i?div[bwlq]?$", op):
            f["div"] += 1
        if re.match(r"^v?div[sp][sd]$", op):
            f["fdiv"] += 1
        if op.startswith("call"):
            f["call"] += 1
        if "%fs:40" in s:
            f["canary"] = 1
        if "(%rsp)" in s:
            f["stack"] += 1
    return funcs


def shorten(name):
    """A demangled name without template arguments and parameters."""
    out, depth = [], 0
    for ch in name:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        elif depth == 0:
            out.append(ch)
    s = "".join(out)
    s = re.sub(r"\(.*$", "", s)
    s = s.replace("beman::inside::", "").replace("detail::", "d::")
    return s.strip()


def bad_kernels(err, pairs):
    """The kernels a compile error belongs to: by the function GCC names, or
    by the line of the batch it points at."""
    bad = {}
    by_line = sorted((p.line, p.n) for p in pairs)
    cur = None
    for l in err.splitlines():
        m = re.search(r"In function .*?\b[kt]_(\d+)\b", l)
        if m:
            cur = int(m.group(1))
            continue
        m = re.search(r"batch\d+\.cpp:(\d+):\d+:", l)
        if m:
            ln = int(m.group(1))
            hit = [n for start, n in by_line if start <= ln < start + 3]
            if hit:
                cur = hit[0]
        if cur is not None and " error: " in l and cur not in bad:
            bad[cur] = re.sub(r"^.*? error: ", "", l)[:300]
    return bad


def build_and_run(args):
    pairs, base = args
    rejected = {}
    for _ in range(8):
        path = gen_batch(pairs, base)
        stem = path.with_suffix("")
        asm = stem.with_suffix(".s")
        r = subprocess.run(CXX + ["-S", "-o", str(asm), str(path)], capture_output=True, text=True)
        if r.returncode == 0:
            break
        bad = bad_kernels(r.stderr, pairs)
        if not bad:
            (stem.with_suffix(".log")).write_text(r.stderr)
            return base, None, None, rejected, r.stderr[:2000]
        rejected.update({p.ident: bad[p.n] for p in pairs if p.n in bad})
        pairs = [p for p in pairs if p.n not in bad]
    else:
        return base, None, None, rejected, "too many rounds"
    r = subprocess.run(["g++-16", str(asm), "-o", str(stem), str(R / "build/core-math-lib/libcoremath.a"), "-lgmp"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return base, None, None, rejected, r.stderr[:2000]
    static = parse_asm(asm.read_text())
    names = sorted({c for f in static.values() for c in f["callees"]})
    if names:
        dem = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
        short = {n: shorten(d) for n, d in zip(names, dem)}
        for f in static.values():
            f["callees"] = sorted({short[c] for c in f["callees"]})
    if not os.environ.get("KEEP_ASM"):
        asm.unlink()
    return base, static, stem, rejected, ""


def run(pattern=None):
    OUT.mkdir(parents=True, exist_ok=True)
    pairs = [p for name in GENERATORS for p in FAMILIES[name][1]()]
    if pattern:
        pairs = [p for p in pairs if re.search(pattern, p.ident)]
    for i, p in enumerate(pairs):
        p.n = i
    meta = OUT / "pairs.jsonl"
    with open(meta, "w") as f:
        for p in pairs:
            f.write(json.dumps({"n": p.n, "family": p.family, "id": p.ident, "ideal": p.ideal_kind,
                                "note": p.note, "real": p.real, "twin": p.ideal}) + "\n")
    batches = [(pairs[i:i + BATCH], f"batch{i // BATCH:03d}") for i in range(0, len(pairs), BATCH)]
    print(f"{len(pairs)} pairs in {len(batches)} batches", flush=True)
    static_counts, bins, rejected = {}, [], {}
    with cf.ThreadPoolExecutor(JOBS) as ex:
        for base, static, binary, rej, err in ex.map(build_and_run, batches):
            rejected.update(rej)
            if static is None:
                print(f"FAILED {base}: {err.splitlines()[0] if err else ''}", flush=True)
                continue
            static_counts.update(static)
            bins.append(binary)
    with open(OUT / "rejected.tsv", "w") as f:
        for k, v in sorted(rejected.items()):
            f.write(f"{k}\t{v}\n")
    print(f"{len(rejected)} pairs do not compile (rejected.tsv)", flush=True)
    (OUT / "static.json").write_text(json.dumps(static_counts))
    dyn = {}
    for b in bins:  # alone, pinned: no parallel runs
        r = subprocess.run(["taskset", "-c", "3", str(b)], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            name, ns, ins, cyc = line.split()
            dyn[name if name != "empty" else f"empty/{b.name}"] = (float(ns), float(ins), float(cyc))
    (OUT / "dynamic.json").write_text(json.dumps(dyn))
    print("measured", len(dyn), "kernels", flush=True)
    # Merge into the results of earlier runs, by id: a run of a pattern
    # replaces only its own rows.
    res_f = OUT / "results.json"
    results = json.loads(res_f.read_text()) if res_f.exists() else {}
    if not pattern:
        results = {}
    results["__empty__"] = results.get("__empty__", []) + [v for k, v in dyn.items() if k.startswith("empty/")]
    for p in pairs:
        k, t = f"k_{p.n}", f"t_{p.n}"
        rec = {"family": p.family, "ideal": p.ideal_kind, "note": p.note, "real": p.real, "twin": p.ideal}
        if p.ident in rejected:
            rec["rejected"] = rejected[p.ident]
        elif k in dyn and t in dyn:
            rec.update({"k_static": static_counts.get(k, {}), "t_static": static_counts.get(t, {}), "k_dyn": dyn[k], "t_dyn": dyn[t]})
        else:
            continue
        results[p.ident] = rec
    res_f.write_text(json.dumps(results))


# ---------------------------------------------------------------------------
# Inventory and coverage
# ---------------------------------------------------------------------------
MEMBER_FILES = {"inside::member": "include/beman/inside/inside/detail/core.hpp",
                "quantity::member": "include/beman/inside/units/detail/quantity.hpp",
                "point::member": "include/beman/inside/units/detail/point.hpp",
                "multivector::member": "include/beman/inside/units/geoalg.hpp"}
CLASS_OF = {"inside::member": "inside", "quantity::member": "quantity", "point::member": "quantity_point",
            "multivector::member": "multivector"}

# Names with no run-time cost of their own, and why.
NOT_RUNTIME = {
    "inside add": "consteval grid-number arithmetic", "inside sub": "consteval grid-number arithmetic",
    "inside excludes": "interval predicate, compile time", "inside includes": "interval predicate, compile time",
    "inside overlaps": "interval predicate, compile time", "inside hull": "interval function, compile time",
    "inside has_flag": "policy query, compile time", "inside make_policy": "builds an empty or one-reference policy",
    "inside error_message": "a table lookup of string literals", "inside get_error_handler": "an atomic load",
    "inside set_error_handler": "an atomic store", "units dimension_of": "compile time",
    "units kind_of": "compile time", "units magnitude_of": "compile time", "units reference_of": "compile time",
    "units unit_symbol": "compile time", "units square": "unit arithmetic, compile time",
    "units cubic": "unit arithmetic, compile time", "units add": "consteval grid-number arithmetic",
    "units sub": "consteval grid-number arithmetic", "inside operator&": "policy flags, compile time",
    "inside operator|": "policy flags, compile time", "inside operator^": "deleted on insides",
    "inside operator~": "deleted on insides", "inside operator&=": "deleted on insides",
    "inside operator|=": "deleted on insides", "inside operator^=": "deleted on insides",
    "inside operator<<=": "deleted on insides", "inside operator>>=": "deleted on insides",
    "inside to_string_debug": "a debugging aid",
}


def scan_members():
    out = set()
    for scope, f in MEMBER_FILES.items():
        cls, acc = None, "public:"
        for line in (R / f).read_text().splitlines():
            m = re.match(r"^(struct|class) ([a-z_]+)\b", line)
            if m:
                cls, acc = m.group(2), ("public:" if m.group(1) == "struct" else "private:")
            m = re.match(r"^  (public|private|protected):", line)
            if m:
                acc = m.group(1) + ":"
            m = re.match(r"^    (\[\[nodiscard\]\] )?(static )?(constexpr |consteval )?(friend )?(explicit\S* )?auto ([a-z_]+)\(",
                         line)
            if m and cls == CLASS_OF[scope] and acc == "public:":
                out.add(f"{scope} {m.group(6)}")
    return out


def inventory():
    binary = R / "build/perf-gate/bin/inventory"
    subprocess.run(["g++-16", "-std=c++26", "-freflection", f"-I{R}/include",
                    str(R / "tests/beman/inside/perf_gate/inventory.cpp"), "-o", str(binary)], check=True)
    names = set(subprocess.run([str(binary)], capture_output=True, text=True, check=True).stdout.splitlines())
    names |= scan_members()
    covered = set(NOT_RUNTIME)
    for covers, _ in FAMILIES.values():
        covered |= set(covers)
    missing = sorted(n for n in names if n not in covered)
    (OUT / "inventory.txt").write_text("\n".join(sorted(names)) + "\n")
    print(f"{len(names)} public names; {len(names) - len(missing)} covered; {len(missing)} without a family:")
    for n in missing:
        print("  ", n)
    return missing


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
# Why a row misses its ideal: the first rule whose id pattern (and callee
# pattern, if any) matches a gap row. Classes: fixable, semantic (the
# guarantee needs the work), reference (the ideal does less work than the
# operation), artefact (the measurement).
CAUSES = [
    (r"^from_step/", None, "fixable", "a builtin step goes through Lower + i·Notch and the general store",
     "range-check the step and write the raw"),
    (r"/exact_cast$", None, "fixable", "exact_cast evaluates conversion_overflows/rounds on 64-bit fractions, then stores",
     "one try_make, its error mapped"),
    (r"/conversion_(overflows|rounds|loses)$", None, "fixable", "the predicate compares 64-bit fractions",
     "compare against integer limits"),
    (r"on_clamp", None, "fixable", "the clamp hook's store takes the exact overshoot path even in range",
     "test the range first, as the plain clamp store does"),
    (r"^divmod/", None, "fixable", "divmod divides on 64-bit fractions (quotient_remainder)",
     "integer quotient and remainder on value indices"),
    (r"^(compound|store_int|store_inside|store_double)/.*wrap", None, "fixable",
     "a wrap that rounds onto an unanchored lattice, or folds a 128-bit product, still folds exact fractions",
     "the integer fold for these cases"),
    (r"^compound/s64\*=|^mul_all/s64", None, "fixable", "a 128-bit product stored back goes through the exact wide path",
     "__int128 product, one range compare"),
    (r"^store_double/s64", None, "fixable", "limits past 2^53: the double is converted to an exact fraction first",
     "compare in double against the nearest doubles of the limits"),
    (r"^store_double/", None, "fixable",
     "around the exact double slot: NaN, ±2^64 and range checks and the error channel (about 55 instructions to "
     "the twin's 20)", "fewer checks where the limits are exact doubles"),
    (r"^to_double/(un|half)", None, "fixable", "double(x) on an unanchored grid reads the exact value through fractions",
     "the lattice codec's constants and one division"),
    (r"^to_float/", None, "fixable", "to<float> of a non-dyadic value checks the double midpoint through exact fractions",
     "test the midpoint on the double alone first"),
    (r"^numerator/", None, "fixable", "numerator/denominator reduce through the exact path on unanchored grids",
     "gcd with the constant denominator"),
    (r"^(to_int|to_u8)/", None, "fixable", "to<T> on an unanchored grid goes through 64-bit fractions",
     "the q_format-style branch for lattices with a constant denominator"),
    (r"^(mul_onto|div_onto)/", None, "fixable", "mul<Out>/div<Out> off the native product take 64-bit fractions",
     "integer numerator over a constant denominator, one rounded division"),
    (r"^div_mode/(dec2|q88|q114|n60|idx)/(u8|s32|dec2|n60)", None, "reference",
     "on these grids div(a, b, mode) returns the exact quotient (the mode is not applied): the twin rounds, the "
     "library reduces a fraction", "decide what div with a mode means off the native grids"),
    (r"^div_mode/", None, "fixable", "div with a mode reduces the exact quotient (gcd) before rounding",
     "one rounded integer division of the scaled operands"),
    (r"^div_exact/", None, "semantic", "the exact quotient is stored reduced: a gcd is the definition of the raw", ""),
    (r"^(store_int|store_inside)/", "rational_slot|rounded_raw|raw_if_rounds", "fixable",
     "a rounded store onto an unanchored lattice (Lower off the notch's multiples) takes 64-bit fractions",
     "value-space integer rounding for unanchored targets"),
    (r"^(store_int|store_inside|compound|incr)/", "canary", "fixable",
     "a cold path takes a local's address: stack frame and protector on the hot path", "pass by value to the cold call"),
    (r"^(to_string|format|ostream)/", None, "fixable", "the text is built through fraction_to_string (and copied for format/streams)",
     "integer digits by to_chars, the fraction by multiplying by 10"),
    (r"^parse/", None, "fixable", "parse reads through the exact decimal grammar", "from_chars for the integer part"),
    (r"^wide", None, "fixable", "schoolbook limb loops where GMP uses assembly kernels; to_string divides limb by limb",
     "only the small sizes matter; GMP is the bound"),
    (r"^(exp|sinh|cosh)/2\^-(40|52)", None, "reference",
     "values to 2^15 on 2^-52 need 67 bits: past double, so CORE-MATH (53 bits) does less", "dd tier for these widths"),
    (r"/2\^-52/", None, "decision", "the double-double tier and its decision cost 2–7× CORE-MATH's correctly rounded path",
     "kernel work with bound proofs"),
    (r"^sin_degrees", None, "decision", "a degree angle takes the exact π-scaled reduction on every call",
     "a double fast path with the exact one as fallback (math engine, bound proof)"),
    (r"^q_value_cast_pi", None, "decision", "the π product is formed exactly on wide integers",
     "double-double product and a decision (math engine, bound proof)"),
    (r"^(indices|values_loop|cursor_loop)/", None, "artefact", "the twin loop folds to a closed form", ""),
    (r"^uniform/", None, "decision", "the mask-and-reject draw takes 1.7 words on average where Lemire's takes 1",
     "Lemire's multiply: changes the values a seeded engine yields"),
    (r"^(matrix|transform)", None, "fixable", "each element is rounded and stored with a check", ""),
]


def classify(r):
    if r["class"] == "at ideal":
        return "at ideal", "", ""
    ratio = float(r["ratio_cyc"])
    for pat, callee, cls, cause, fix in CAUSES:
        if re.search(pat, r["id"]) and (callee is None or re.search(callee, r["callees"] + " " + r["flags"])):
            if cls == "fixable" and ratio < 2:
                break
            return cls, cause, fix
    if ratio < 2:
        return "small", "within 2× of the twin: a few extra instructions", ""
    return "fixable", "not analysed yet", ""


def report():
    results = json.loads((OUT / "results.json").read_text())
    pairs, static, dyn = [], {}, {}
    for n, (ident, rec) in enumerate(sorted((k, v) for k, v in results.items() if k != "__empty__")):
        if "k_dyn" not in rec:
            continue
        pairs.append({"n": n, "id": ident, "family": rec["family"], "ideal": rec["ideal"], "note": rec["note"]})
        static[f"k_{n}"], static[f"t_{n}"] = rec["k_static"], rec["t_static"]
        dyn[f"k_{n}"], dyn[f"t_{n}"] = rec["k_dyn"], rec["t_dyn"]
    empties = results.get("__empty__", [])
    e_ns = min(v[0] for v in empties) if empties else 0.0
    e_ins = min(v[1] for v in empties) if empties else 0.0
    e_cyc = min(v[2] for v in empties) if empties else 0.0
    rows = []
    for p in pairs:
        k, t = f"k_{p['n']}", f"t_{p['n']}"
        if k not in dyn or t not in dyn:
            continue
        ks, ts = static.get(k, {}), static.get(t, {})
        kd, td = dyn[k], dyn[t]
        net = lambda v, e: max(v - e, 0.25)
        ratio_c = net(kd[2], e_cyc) / net(td[2], e_cyc)
        ratio_i = net(kd[1], e_ins) / net(td[1], e_ins)
        flags = []
        if ks.get("div", 0) > ts.get("div", 0):
            flags.append(f"div+{ks['div'] - ts.get('div', 0)}")
        if ks.get("call", 0) > ts.get("call", 0):
            flags.append(f"call+{ks['call'] - ts.get('call', 0)}")
        if ks.get("canary", 0) > ts.get("canary", 0):
            flags.append("canary")
        if ks.get("stack", 0) > ts.get("stack", 0) + 2:
            flags.append("stack")
        if ratio_c > 1.5 and kd[2] - td[2] > 1.5:
            flags.append("slow")
        twin_slower = td[2] > kd[2]
        cls = "at ideal" if (ratio_c <= 1.2 and ratio_i <= 1.3) or (twin_slower and ratio_i <= 1.3) else "gap"
        fam, ident = p["family"], p["id"]
        parts = ident.split("/")
        rows.append({"family": fam, "id": ident, "op": parts[0], "shape": parts[1] if len(parts) > 1 else "",
                     "policy": parts[2] if len(parts) > 2 else "",
                     "real_ns": round(kd[0], 3), "real_ins": round(kd[1] - e_ins, 1), "real_cyc": round(kd[2] - e_cyc, 2),
                     "ideal_ns": round(td[0], 3), "ideal_ins": round(td[1] - e_ins, 1), "ideal_cyc": round(td[2] - e_cyc, 2),
                     "ratio_cyc": round(ratio_c, 2), "ratio_ins": round(ratio_i, 2),
                     "static_ins": ks.get("ins", 0), "static_div": ks.get("div", 0), "static_call": ks.get("call", 0),
                     "twin_static_ins": ts.get("ins", 0), "twin_div": ts.get("div", 0),
                     "flags": " ".join(flags), "class": cls, "ideal": p["ideal"], "note": p["note"],
                     "callees": " ; ".join(ks.get("callees", []))})
    for r in rows:
        r["class"], r["cause"], r["fix"] = classify(r)
    with open(OUT / "report.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    fams = {}
    for r in rows:
        fams.setdefault(r["family"], []).append(r)
    print(f"{len(rows)} rows; empty call {e_ns:.2f} ns, {e_ins:.1f} ins, {e_cyc:.2f} cyc subtracted")
    print(f"{'family':44} {'rows':>5} {'at ideal':>8} {'gaps':>5} {'worst':>7}  worst row")
    for fam, rs in fams.items():
        worst = max(rs, key=lambda r: r["ratio_cyc"])
        gaps = sum(r["class"] != "at ideal" for r in rs)
        print(f"{fam:44} {len(rs):5} {len(rs) - gaps:8} {gaps:5} {worst['ratio_cyc']:7.2f}  {worst['id']}")
    return rows


def show(pattern=".", sort=None):
    rows = list(csv.DictReader(open(OUT / "report.csv")))
    rows = [r for r in rows if re.search(pattern, r["id"])]
    if sort:
        rows.sort(key=lambda r: -float(r[sort]))
    print(f"{'id':52} {'ns':>7} {'ins':>6} {'cyc':>6} | {'ideal ns':>8} {'ins':>6} {'cyc':>6} | {'cyc×':>5} {'ins×':>5}  "
          f"{'div':>3} {'call':>4}  flags")
    for r in rows:
        print(f"{r['id'][:52]:52} {float(r['real_ns']):7.2f} {float(r['real_ins']):6.1f} {float(r['real_cyc']):6.1f} | "
              f"{float(r['ideal_ns']):8.2f} {float(r['ideal_ins']):6.1f} {float(r['ideal_cyc']):6.1f} | "
              f"{float(r['ratio_cyc']):5.2f} {float(r['ratio_ins']):5.2f}  {r['static_div']:>3} {r['static_call']:>4}  "
              f"{r['flags']}")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "run"
    OUT.mkdir(parents=True, exist_ok=True)
    if cmd == "inventory":
        sys.exit(1 if inventory() else 0)
    elif cmd == "run":
        run(sys.argv[2] if len(sys.argv) > 2 else None)
    elif cmd == "report":
        report()
    elif cmd == "show":
        show(sys.argv[2] if len(sys.argv) > 2 else ".", sys.argv[3] if len(sys.argv) > 3 else None)
    else:
        sys.exit(__doc__)
