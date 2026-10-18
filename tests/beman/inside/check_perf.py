#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Cachegrind instruction-count regression check (run by hand; CONTRIBUTING.md).

Runs the perf workload under `valgrind --tool=cachegrind`, reads the total
retired-instruction count (Ir) from the cachegrind output's `summary:` line, and
compares it to the committed baseline within a tolerance. Instruction counts are
deterministic regardless of host load, unlike wall-clock timing. The committed
baseline comes from one machine and build: compare before/after on the same
build (or refresh the baseline with --update) rather than across machines.

A key with no baseline entry is recorded and passes.

Usage:
  check_perf.py --binary build/<preset>/tests/beman/inside/beman.inside.perf_workload --baseline tests/beman/inside/perf_baseline.json
                [--key integer_qformat | --all] [--tol 0.05] [--update]
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile


def run_cachegrind(binary: str, key: str) -> int:
    """Run the binary under cachegrind and return total Ir (instructions)."""
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "cg.out")
        cmd = [
            "valgrind", "--tool=cachegrind",
            "--cache-sim=no", "--branch-sim=no",   # Ir only: faster, fewer moving parts
            f"--cachegrind-out-file={out}",
            binary,
            key,                                   # workload selector (argv[1])
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            sys.stderr.write(proc.stderr)
            raise SystemExit(f"valgrind exited {proc.returncode}")
        # The out file carries a `summary: <Ir>` line; that total is the stable
        # signal. (cachegrind's stderr also prints "I   refs: N" but with commas.)
        text = open(out, encoding="utf-8", errors="replace").read()
        m = re.search(r"^summary:\s+(\d+)", text, re.MULTILINE)
        if not m:
            raise SystemExit("could not find a 'summary:' line in cachegrind output")
        return int(m.group(1))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", required=True)
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--key", default="integer_qformat",
                    help="workload key (baseline JSON entry AND binary argv[1])")
    ap.add_argument("--tol", type=float, default=0.05,
                    help="allowed fractional growth before failing (default 5%%)")
    ap.add_argument("--update", action="store_true",
                    help="overwrite the baseline entry with the measured value")
    ap.add_argument("--all", action="store_true",
                    help="check every key of the baseline file")
    args = ap.parse_args()

    baseline = {}
    if os.path.exists(args.baseline):
        with open(args.baseline, encoding="utf-8") as f:
            baseline = json.load(f)

    keys = sorted(baseline) if args.all else [args.key]
    failed = [key for key in keys if not check(args, baseline, key)]
    if args.update or any(key not in baseline for key in keys):
        with open(args.baseline, "w", encoding="utf-8") as f:
            json.dump(baseline, f, indent=2, sort_keys=True)
            f.write("\n")
    return 1 if failed else 0


def check(args, baseline: dict, key: str) -> bool:
    ir = run_cachegrind(args.binary, key)
    print(f"[perf] {key}: Ir = {ir:,}")
    if args.update or key not in baseline:
        reason = "updated" if args.update else "recorded (no prior entry)"
        baseline[key] = ir
        print(f"[perf] baseline {reason}: {args.baseline} [{key}] = {ir:,}")
        return True

    ref = int(baseline[key])
    limit = int(ref * (1.0 + args.tol))
    growth = (ir - ref) / ref * 100.0
    print(f"[perf] baseline = {ref:,}  limit = {limit:,} (+{args.tol*100:.0f}%)  "
          f"delta = {growth:+.2f}%")
    if ir > limit:
        print(f"[perf] FAIL: {key} grew {growth:+.2f}% (> {args.tol*100:.0f}% tolerance)")
        print("[perf] If this is an intentional change, re-run with --update and commit.")
        return False
    print(f"[perf] OK: {key} within tolerance")
    return True

if __name__ == "__main__":
    sys.exit(main())
