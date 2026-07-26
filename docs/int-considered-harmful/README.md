# Programs for "int considered harmful"

Every listing and every output block in [`../int-considered-harmful.pdf`](../int-considered-harmful.pdf)
comes from a program in this directory. Nothing in the paper was written by
hand; each block was captured by compiling and running the corresponding file.

## Building

The `h*` (int hazard) and `d*` (double hazard) programs need nothing but a
compiler:

```bash
g++ -std=c++23 -O2 h01_overflow_ub.cpp -o h01
g++ -std=c++23 -O0 h01_overflow_ub.cpp -o h01_O0    # compare the two
```

The `b*` (bound), `m_after` and `p01` programs need the library on the include
path:

```bash
g++ -std=c++23 -O2 -I ../../include b01_no_overflow.cpp -o b01
```

Or against the committed single header, with no other `-I`:

```bash
g++ -std=c++23 -O2 -I ../../single_include b01_no_overflow.cpp -o b01
```

## Programs that are meant to fail

`x01_bare_int_rejected.cpp` and `x02_range_rejected.cpp` are **expected not to
compile** — they demonstrate errors that `bound` moves to build time. Their
diagnostics are quoted in the paper.

## Programs that crash or hang, on purpose

| File | Behaviour |
|---|---|
| `h02_overflow_loop.cpp` | At `-O2` GCC emits no instructions for `main`; the binary segfaults. At `-O0` it prints `iterations = 3`. |
| `h04_divzero.cpp` | Raises `SIGFPE`. Takes an argument: `0` for `x / 0`, `1` for `INT_MIN / -1`. |

`h01`, `h05` and `h08` rely on undefined behaviour and may legitimately produce
different output on another compiler or platform — that is the point being
made. Output in the paper is from GCC 15.2.0 on x86-64 Linux.

## Benchmarks

`p02_fair_bench.cpp` compares `bound<checked>` against native code providing
the *same* guarantee (clamped, and a hand-rolled checked struct), not against
unchecked native. `p03_check_elision.cpp` shows where `bound`'s compile-time
range information actually pays: on a chain of operations it emits zero runtime
checks where the native checked type needs three.

Both print a table; run them a few times, and prefer a quiet machine.

## The codegen exhibit

`p01_codegen.cpp` is compiled, not run:

```bash
g++ -std=c++23 -O2 -I ../../include -c p01_codegen.cpp -o p01.o
objdump -d --no-show-raw-insn -C p01.o
```

`bound_add` and `native_add` lower to identical instructions. The repository
enforces the same property in CI via `tests/check_codegen.sh`.

## Rebuilding the paper

```bash
cd ..                                    # docs/
pdflatex int-considered-harmful.tex      # run three times for the TOC
```
