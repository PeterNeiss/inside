#!/usr/bin/env python3
"""Cachegrind instruction-count regression gate.

Runs a workload binary under `valgrind --tool=cachegrind`, reads the total
retired-instruction count (Ir) from the cachegrind output's `summary:` line, and
compares it to a committed baseline within a tolerance. Instruction counts are
deterministic regardless of host load, so this gives a stable regression signal
on noisy shared CI runners where wall-clock timing would be useless.

Bootstrap: if the baseline file does not exist, this writes it from the current
run and exits 0 with a notice — so the first CI run produces the baseline as an
artifact to review and commit. Subsequent runs gate against it.

Usage:
  check_perf.py --binary build/perf_workload --baseline tests/perf_baseline.json
                [--key integer_qformat] [--tol 0.05] [--update]
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile


def run_cachegrind(binary: str) -> int:
    """Run the binary under cachegrind and return total Ir (instructions)."""
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "cg.out")
        cmd = [
            "valgrind", "--tool=cachegrind",
            "--cache-sim=no", "--branch-sim=no",   # Ir only: faster, fewer moving parts
            f"--cachegrind-out-file={out}",
            binary,
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
    ap.add_argument("--key", default="default",
                    help="workload key in the baseline JSON")
    ap.add_argument("--tol", type=float, default=0.05,
                    help="allowed fractional growth before failing (default 5%%)")
    ap.add_argument("--update", action="store_true",
                    help="overwrite the baseline entry with the measured value")
    args = ap.parse_args()

    ir = run_cachegrind(args.binary)
    print(f"[perf] {args.key}: Ir = {ir:,}")

    baseline = {}
    if os.path.exists(args.baseline):
        with open(args.baseline, encoding="utf-8") as f:
            baseline = json.load(f)

    if args.update or args.key not in baseline:
        baseline[args.key] = ir
        with open(args.baseline, "w", encoding="utf-8") as f:
            json.dump(baseline, f, indent=2, sort_keys=True)
            f.write("\n")
        reason = "updated" if args.update else "bootstrapped (no prior entry)"
        print(f"[perf] baseline {reason}: {args.baseline} [{args.key}] = {ir:,}")
        print("[perf] review and commit this baseline; the gate is active once it exists.")
        return 0

    ref = int(baseline[args.key])
    limit = int(ref * (1.0 + args.tol))
    growth = (ir - ref) / ref * 100.0
    print(f"[perf] baseline = {ref:,}  limit = {limit:,} (+{args.tol*100:.0f}%)  "
          f"delta = {growth:+.2f}%")
    if ir > limit:
        print(f"[perf] FAIL: {args.key} grew {growth:+.2f}% (> {args.tol*100:.0f}% tolerance)")
        print("[perf] If this is an intentional change, re-run with --update and commit.")
        return 1
    print(f"[perf] OK: {args.key} within tolerance")
    return 0


if __name__ == "__main__":
    sys.exit(main())
