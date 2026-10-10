# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Keep the proposal's code listings equal to the programs that compile them.

A listing in inside.md is a ```cpp block preceded by `<!-- listing: NAME -->`;
its body must equal the region of examples/NAME.cpp between `// [listing]` and
`// [/listing]`, dedented.

    python3 listings.py check   # exit 1 and name each listing that differs
    python3 listings.py update  # rewrite inside.md from the programs
"""

import pathlib
import re
import sys
import textwrap

HERE = pathlib.Path(__file__).resolve().parent
PAPER = HERE / "inside.md"
BLOCK = re.compile(r"(<!-- listing: (\w+) -->\n```cpp\n)(.*?)(```)", re.S)


def region(name):
    text = (HERE / "examples" / f"{name}.cpp").read_text()
    match = re.search(r"// \[listing\]\n(.*?)\n[ \t]*// \[/listing\]", text, re.S)
    if not match:
        sys.exit(f"examples/{name}.cpp has no // [listing] region")
    return textwrap.dedent(match.group(1)) + "\n"


def main(mode):
    paper = PAPER.read_text()
    stale = [m.group(2) for m in BLOCK.finditer(paper) if m.group(3) != region(m.group(2))]
    if mode == "update":
        PAPER.write_text(BLOCK.sub(lambda m: m.group(1) + region(m.group(2)) + m.group(4), paper))
        return 0
    for name in stale:
        print(f"inside.md: listing '{name}' differs from examples/{name}.cpp")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "check"))
