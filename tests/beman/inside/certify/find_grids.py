#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Find the math grid pairs a build uses, and certify them.

Every call of a beman.inside math function instantiates
beman::inside::math::adaptive::<fn>_into<Out, In...>, and an unoptimized build
keeps that instantiation as a symbol whose demangled name spells both grids
exactly: deduced outputs, aliases and quantities' numbers included. This
script scans a folder and its subfolders for object files, libraries and
executables, reads their symbols with nm, and lists every distinct pair.

    find_grids.py build/                      list the pairs
    find_grids.py build/ --emit pairs.cpp     write a certify program for them
    find_grids.py build/ --run                write it, compile it, run it
    find_grids.py build/ --run -- --estimate  ... and only estimate the time

Arguments after `--` go to the certify program (certify.hpp lists them:
--estimate, --max-slots N, --max-seconds S, --markdown FILE, --check FILE, ...).

Build the project with -O0 (a Debug build) or -fkeep-inline-functions first:
an inlined call leaves no symbol. docs/certify.md has the whole workflow.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_INCLUDE = HERE.parents[3] / "include"

# <fn>_into's template arguments after Out: the kinds of its inputs.
UNARY = {
    "exp", "exp2", "sin", "cos", "tan", "atan", "sinh", "cosh", "tanh", "asinh", "cbrt",
    "log", "log2", "log10", "asin", "acos", "acosh", "atanh", "sqrt",
}
BINARY = {"atan2", "hypot", "pow"}  # atan2<Out, Y, X>, hypot<Out, X, Y>, pow<Out, B, E>
INTO = re.compile(r"beman::inside::math::adaptive::(\w+?)_into<")
PI_TRIG = re.compile(r"beman::inside::units::detail::pi_trig_into<")
NUMBER = r"(-?\d+)[uUlL]*"
# A grid number: big_rational{big_int{…}, big_int{…}}. A big_int prints its
# fields with the trailing defaults left out: Small, then the interned limbs
# (a mangled array, least significant first) and their count, then Negative.
BIG_INT = r"(?:[\w:]*::)?big_int\{([^{}]*)\}"
BIG_RATIONAL = re.compile(r"big_rational\{" + BIG_INT + r", " + BIG_INT + r"\}")
LIMB = re.compile(r"Lm(\d+)E")
FLAGS = re.compile(r"^\s*" + NUMBER + r"\s*$")
OBJECT_SUFFIXES = {".o", ".obj", ".a", ".so", ".dylib", ".lib"}
MAGICS = (b"\x7fELF", b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf", b"\xce\xfa\xed\xfe", b"\xcf\xfa\xed\xfe",
          b"!<arch>")


def is_binary(path):
    if path.suffix in OBJECT_SUFFIXES:
        return True
    try:
        with open(path, "rb") as f:
            head = f.read(8)
    except OSError:
        return False
    return any(head.startswith(m) for m in MAGICS)


def symbols(nm, path):
    """The demangled defined symbols of one file that mention the math engine."""
    for command in ([nm, "-C", "--defined-only", str(path)], [nm, "-C", str(path)]):
        try:
            out = subprocess.run(command, capture_output=True, text=True, errors="replace")
        except FileNotFoundError:
            sys.exit(f"find_grids.py: {nm} not found (pass --nm)")
        if out.returncode == 0:
            return [line for line in out.stdout.splitlines() if "beman::inside::" in line]
    return []


def template_args(text, start):
    """The top-level arguments of the template argument list opening at text[start] == '<'."""
    args, depth, braces, begin = [], 0, 0, start + 1
    for i in range(start, len(text)):
        c = text[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
            if depth == 0:
                args.append(text[begin:i].strip())
                return args
        elif c == "{":
            braces += 1
        elif c == "}":
            braces -= 1
        elif c == "," and depth == 1 and braces == 0:
            args.append(text[begin:i].strip())
            begin = i + 1
    return None


class Unsupported(Exception):
    pass


def parse_inside(arg):
    """An inside<grid{...}, flags> argument: the grid's raw fields and the flags."""
    if "inside<" not in arg or "grid{" not in arg:
        raise Unsupported("not an inside")
    open_ = arg.index("grid{") + len("grid")
    depth = 0
    for close in range(open_, len(arg)):
        depth += {"{": 1, "}": -1}.get(arg[close], 0)
        if depth == 0:
            break
    literal = arg[open_:close + 1]
    parts = [rational_field(big_int(n), big_int(d)) for n, d in BIG_RATIONAL.findall(literal)]
    if len(parts) != 4:
        raise Unsupported(f"unexpected grid {literal!r}")
    rest = arg[close + 1:].lstrip(", ").rstrip(">").strip()
    flags = FLAGS.match(rest)
    if not flags:
        raise Unsupported(f"unexpected policy {rest!r}")
    return {"fields": parts, "flags": int(flags.group(1))}


def big_int(body):
    """The value of a big_int's printed fields."""
    fields = [f.strip() for f in body.split(",")] if body.strip() else []
    if len(fields) > 1 and fields[1].startswith("&"):
        magnitude = sum(int(limb) << (64 * i) for i, limb in enumerate(LIMB.findall(fields[1])))
    else:
        magnitude = int(re.match(r"\d+", fields[0]).group()) if fields else 0
    return -magnitude if len(fields) > 3 and fields[3] == "true" else magnitude


def rational_field(n, d):
    """A grid number as the 64-bit rational's fields: the magnitude and the
    denominator with the sign. Past them the pair has no certify spelling."""
    if abs(n) >= 1 << 64 or d >= 1 << 63:
        raise Unsupported("a grid number past the 64-bit rational")
    return (abs(n), -d if n < 0 else d)


def value(field):
    n, d = field
    return Fraction(-n if d < 0 else n, abs(d))


def slots(t):
    lo, hi, notch, max_den = (value(f) for f in t["fields"])
    if notch == 0:
        if lo != hi:
            raise Unsupported("a continuous input grid has no finite set of inputs")
        return 1
    return int((hi - lo) / notch) + 1


FLAG_NAMES = [(1 << 34, "checked"), (1 << 32, "clamp"), (1 << 33, "wrap")]
ROUNDING = [((1 << 8) | 16, "round_half_even"), ((1 << 7) | 16, "round_ceil"), ((1 << 6) | 16, "round_floor"),
            ((1 << 5) | 16, "round_nearest"), (16, "round_toward_zero")]


def flags_text(f):
    """The policy by name, as certify.hpp prints it."""
    names = []
    unsafe = (1 << 36) | 16
    if f & (1 << 36):
        names.append("unsafe")
        f &= ~unsafe
    for bits, name in ROUNDING:
        if f & bits == bits:
            names.append(name)
            break
    f &= ~((1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | 16)
    for bits, name in FLAG_NAMES:
        if f & bits:
            names.append(name)
            f &= ~bits
    if f:
        names.append(hex(f))
    return ", ".join(names) or "none"


def grid_text(t):
    def num(f):
        v = value(f)
        return str(v.numerator) if v.denominator == 1 else f"{v.numerator}/{v.denominator}"

    lo, hi, notch, max_den = t["fields"]
    extra = f", den ≤ {num(max_den)}" if value(max_den) != 0 else ""
    return f"{{[{num(lo)}, {num(hi)}], {num(notch)}{extra}}} {flags_text(t['flags'])}"


def spelling(t):
    raw = ", ".join(f"{n}ull, {d}ll" for n, d in t["fields"])
    return f"bi::inside<bi::certify::grid_from({raw}), {t['flags']}ull>"


def scan(folders, nm):
    pairs, unsupported, pi_trig, files, scanned = {}, {}, 0, set(), 0
    for folder in folders:
        for root, _, names in os.walk(folder):
            for name in names:
                path = Path(root) / name
                if not is_binary(path):
                    continue
                scanned += 1
                for line in symbols(nm, path):
                    if PI_TRIG.search(line):
                        pi_trig += 1
                    for m in INTO.finditer(line):
                        fn = m.group(1)
                        if fn not in UNARY and fn not in BINARY and fn != "pow_base":
                            continue
                        args = template_args(line, m.end() - 1)
                        if not args:
                            continue
                        key = (fn, tuple(args))
                        if key in pairs or key in unsupported:
                            (pairs.get(key) or unsupported[key])["files"].add(str(path))
                            continue
                        try:
                            types = [parse_inside(a) for a in args if not re.fullmatch(NUMBER, a)]
                            base = next((int(a.rstrip("uUlL")) for a in args if re.fullmatch(NUMBER, a)), None)
                            ins = types[1:]
                            count = 1
                            for t in ins:
                                count *= slots(t)
                            pairs[key] = {"function": fn, "out": types[0], "inputs": ins, "base": base,
                                          "slots": count, "files": {str(path)}}
                        except Unsupported as e:
                            unsupported[key] = {"function": fn, "reason": str(e), "files": {str(path)}}
                        files.add(str(path))
    return list(pairs.values()), list(unsupported.values()), pi_trig, scanned


def emit(pairs, path, source):
    types = {}

    def alias(t):
        s = spelling(t)
        if s not in types:
            types[s] = f"t{len(types)}"
        return types[s]

    lines = []
    for p in pairs:
        out = alias(p["out"])
        ins = [alias(t) for t in p["inputs"]]
        fn = p["function"]
        if fn == "pow_base":
            lines.append(f'    r.unary<{out}, {ins[0]}>("pow_base<{p["base"]}>", []({ins[0]} x) '
                         f'{{ return am::pow_base_into<{out}, {p["base"]}>(x); }});')
        elif len(ins) == 1:
            lines.append(f'    r.unary<{out}, {ins[0]}>("{fn}", []({ins[0]} x) {{ return am::{fn}_into<{out}>(x); }});')
        else:
            lines.append(f'    r.binary<{out}, {ins[0]}, {ins[1]}>("{fn}", []({ins[0]} a, {ins[1]} b) '
                         f'{{ return am::{fn}_into<{out}>(a, b); }});')
    with open(path, "w") as f:
        f.write(f"// Generated by find_grids.py from {source}: the math pairs that build uses.\n")
        f.write('#include "certify.hpp"\n\n')
        f.write("namespace bi = beman::inside;\nnamespace am = beman::inside::math::adaptive;\n\n")
        for s, a in types.items():
            f.write(f"using {a} = {s};\n")
        f.write("\nvoid pairs(bi::certify::runner& r) {\n" + "\n".join(lines) + "\n}\n\n")
        f.write("int main(int argc, char** argv) { return bi::certify::main(argc, argv, pairs); }\n")


def main():
    argv = sys.argv[1:]
    passthrough = []
    if "--" in argv:
        i = argv.index("--")
        argv, passthrough = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folders", nargs="+", type=Path, help="folders to scan (recursively)")
    ap.add_argument("--nm", default=os.environ.get("NM", "nm"), help="nm to read symbols with (default: nm)")
    ap.add_argument("--json", type=Path, help="write the pairs as JSON")
    ap.add_argument("--emit", type=Path, help="write a certify program for the pairs")
    ap.add_argument("--run", action="store_true", help="emit, compile and run the certify program")
    ap.add_argument("--cxx", default=os.environ.get("CXX", "c++"), help="compiler for --run (default: $CXX or c++)")
    ap.add_argument("-I", dest="includes", action="append", type=Path, default=[],
                    help="include directory with beman/inside (default: this repository's)")
    ap.add_argument("--std", default="c++26", help="language standard for --run (default: c++26)")
    ap.add_argument("--cxxflags", default="", help="more compiler flags for --run")
    ap.add_argument("--min-pairs", type=int, default=0, help="fail when fewer pairs are found (for tests)")
    args = ap.parse_args(argv)

    pairs, unsupported, pi_trig, scanned = scan(args.folders, args.nm)
    pairs.sort(key=lambda p: (p["function"], p["slots"]))
    print(f"find_grids: {scanned} files scanned, {len(pairs)} math pairs found")
    for p in pairs:
        ins = " x ".join(grid_text(t) for t in p["inputs"])
        name = p["function"] + (f"<{p['base']}>" if p["base"] is not None else "")
        print(f"  {name:<12} {p['slots']:>12} inputs  {ins} -> {grid_text(p['out'])}")
    for u in unsupported:
        print(f"  {u['function']:<12} not certifiable: {u['reason']}")
    if pi_trig:
        print(f"  {pi_trig} symbols of sin_into/cos_into on a pi-scaled angle (degrees, turns): not certifiable "
              "by this tool; they run the integer path alone")
    if len(pairs) < args.min_pairs:
        print(f"find_grids: expected at least {args.min_pairs} pairs")
        return 1
    if not pairs:
        print("find_grids: no math calls found. Build with -O0 (a Debug build) or -fkeep-inline-functions: "
              "an inlined call leaves no symbol.")
        return 0 if not args.run else 1

    if args.json:
        with open(args.json, "w") as f:
            json.dump([dict(p, files=sorted(p["files"])) for p in pairs], f, indent=2)
    program = args.emit or (Path("certify_pairs.cpp") if args.run else None)
    if program:
        emit(pairs, program, " ".join(str(f) for f in args.folders))
        print(f"find_grids: wrote {program}")
    if not args.run:
        return 0

    exe = program.with_suffix(".exe" if os.name == "nt" else "")
    includes = args.includes or [REPO_INCLUDE]
    command = [args.cxx, f"-std={args.std}", "-freflection", "-O2", "-DNDEBUG", f"-I{HERE}"] + args.cxxflags.split()
    command += [f"-I{i}" for i in includes] + [str(program), "-o", str(exe)]
    print("find_grids: " + " ".join(command), flush=True)
    if subprocess.run(command).returncode != 0:
        return 1
    return subprocess.run([str(exe.resolve())] + passthrough).returncode


if __name__ == "__main__":
    sys.exit(main())
