# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Keep the proposals' code listings equal to the programs that compile them.

A listing in a paper (any *.md here) is a ```cpp block preceded by
`<!-- listing: NAME -->`; its body must equal the region of examples/NAME.cpp
between `// [listing]` and `// [/listing]`, dedented.

    python3 listings.py check   # exit 1 and name each listing that differs
    python3 listings.py update  # rewrite the papers from the programs
"""

import pathlib
import re
import sys
import textwrap

HERE = pathlib.Path(__file__).resolve().parent
PAPERS = sorted(HERE.glob("*.md"))
BLOCK = re.compile(r"(<!-- listing: (\w+) -->\n```cpp\n)(.*?)(```)", re.S)


def region(name):
    path = HERE / "examples" / f"{name}.cpp"
    if not path.exists():
        sys.exit(f"examples/{name}.cpp does not exist")
    match = re.search(r"// \[listing\]\n(.*?)\n[ \t]*// \[/listing\]", path.read_text(encoding="utf-8"), re.S)
    if not match:
        sys.exit(f"examples/{name}.cpp has no // [listing] region")
    return textwrap.dedent(match.group(1)) + "\n"


def main(mode):
    stale, seen = [], {}
    for paper in PAPERS:
        text = paper.read_text(encoding="utf-8")
        for m in BLOCK.finditer(text):
            name = m.group(2)
            if name in seen and seen[name] != paper.name:
                sys.exit(f"listing '{name}' is in both {seen[name]} and {paper.name}")
            seen[name] = paper.name
            if m.group(3) != region(name):
                stale.append((paper.name, name))
        if mode == "update":
            paper.write_text(BLOCK.sub(lambda m: m.group(1) + region(m.group(2)) + m.group(4), text), encoding="utf-8")
    if mode == "update":
        return 0
    for paper, name in stale:
        print(f"{paper}: listing '{name}' differs from examples/{name}.cpp")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "check"))
