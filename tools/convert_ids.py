#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# convert_ids.py: rewrites the old type spellings as ids.
#
#   inside<{0, 360, bounds::half_open}, wrap>            -> id<"[0..360) wrap">
#   inside<{{0, 1}, per<4>}, clamp | round_half_away>     -> id<"[0..1] 1/4 clamp|round_half_away">
#   quantity<si::kilo(si::metre) / si::hour, I>           -> id<"[0..100] [km/h]">  (I a local alias it can read)
#   quantity_point<si::degree_Celsius, si::ice_point, I>  -> id<"[-40..125] [`C] @ice_point">
#
# Each spelling is written as its author wrote it (the open limit stays open): every spelling of a grid is one
# type, and the key prints the canonical text. A spelling with a part it cannot read (a template parameter, a
# name of the program's own, a computed number) becomes the computed form:
#   inside<G, P>          -> id<key{G, P}>          inside<{lo, hi}>  -> id<key{grid{lo, hi}}>
#   quantity<R, Rep>      -> id<quantity_key<R, Rep>>
#   quantity_point<R, O, Rep> -> id<point_key<R, O, Rep>>
#
# --check writes ins::detail::checked_same<OLD, NEW> for code (not comments; checked_old<OLD>::as<NEW> in a macro's
# argument), so that a build proves every rewrite names the same type; the plain run then writes NEW.
#
# --computed writes the computed forms only: for tests of what the old spellings name.
#
# Usage: convert_ids.py [--check] [--diff] [--computed] --symbols <symtab.txt> files...
#   symtab.txt: "unit si::metre m", "kind isq::height height", "origin si::ice_point ice_point" lines (the catalog's
#   tags with the texts a key spells them by), as tools/convert_ids_symbols.cpp prints them.
import re
import sys

FLAGS = ['clamp', 'wrap', 'round_half_away', 'round_floor', 'round_ceil', 'round_half_even', 'round_toward_zero',
         'unsafe']
BOUNDS = {'bounds::closed': ('[', ']'), 'bounds::half_open': ('[', ')'), 'bounds::open': ('(', ')')}
PREFIXES = {'quecto': 'q', 'ronto': 'r', 'yocto': 'y', 'zepto': 'z', 'atto': 'a', 'femto': 'f', 'pico': 'p',
            'nano': 'n', 'micro': 'u', 'milli': 'm', 'centi': 'c', 'deci': 'd', 'deca': 'da', 'hecto': 'h',
            'kilo': 'k', 'mega': 'M', 'giga': 'G', 'tera': 'T', 'peta': 'P', 'exa': 'E', 'zetta': 'Z',
            'yotta': 'Y', 'ronna': 'R', 'quetta': 'Q'}


def strip_ins(s):
    s = s.strip()
    while s.startswith('ins::'):
        s = s[5:]
    return s


# ---- Brackets ----

def match_angle(text, i):
    """text[i] == '<': the index of its '>', counting angles outside parentheses only (1 << 8 in per<(1 << 8)>)."""
    depth = {'(': 0, '{': 0, '[': 0}
    close = {')': '(', '}': '{', ']': '['}
    angle = 0
    j = i
    while j < len(text):
        c = text[j]
        if c in depth:
            depth[c] += 1
        elif c in close:
            depth[close[c]] -= 1
            if depth[close[c]] < 0:
                return -1
        elif c == '"':
            j = text.index('"', j + 1)
        elif c == '<' and text[j + 1:j + 2] == '<':  # a shift (1 << 20), never two template openings
            j += 2
            continue
        elif depth['('] == 0 and c == '<':
            angle += 1
        elif depth['('] == 0 and c == '>':
            angle -= 1
            if angle == 0:
                return j
        elif c in ';':
            return -1
        j += 1
    return -1


def split_top(s):
    out, depth, cur, i = [], 0, '', 0
    while i < len(s):
        c = s[i]
        if c == '"':
            k = s.index('"', i + 1)
            cur += s[i:k + 1]
            i = k + 1
            continue
        if c in '({[':
            depth += 1
        elif c in ')}]':
            depth -= 1
        elif c == '<' and s[i + 1:i + 2] == '<':
            cur += '<<'
            i += 2
            continue
        elif c == '<' and depth >= 0:
            k = match_angle(s, i)
            if k > 0 and not s[i - 1:i].isspace():
                cur += s[i:k + 1]
                i = k + 1
                continue
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
        i += 1
    if cur.strip():
        out.append(cur.strip())
    return out


# ---- Numbers, steps, policies ----

INT = r"[0-9][0-9']*"


def number(s):
    """A number's text, or None."""
    s = strip_ins(s)
    if re.fullmatch(r'[0-9\s()+\-*<]+', s) and re.search(r'[0-9]\s*[-+*<]', s):  # an integer expression
        try:
            v = eval(s.replace("'", ''), {})
        except Exception:
            return None
        return str(v) if isinstance(v, int) else None
    neg = ''
    if s.startswith('-'):
        neg, s = '-', s[1:].strip()
    elif s.startswith('+'):
        s = s[1:].strip()
    if s.startswith('(') and s.endswith(')'):
        inner = number(s[1:-1])
        if inner is None:
            return None
        return (neg + inner).replace('--', '') if neg else inner
    m = re.fullmatch(r'(?:just<|rational\{)\s*(-?' + INT + r')\s*(?:,\s*(' + INT + r')\s*)?[>}]', s)
    if m and (s[-1] == '>') == s.startswith('just'):
        n, d = m.group(1), m.group(2)
        v = n if d is None or d == '1' else f'{n}/{d}'
        return (neg + v).replace('--', '') if neg else v
    m = re.fullmatch(r'(0[xX]1(?:\.0*)?[pP][+-]?[0-9]+)(_g|_r|_ins)?', s)  # a power of two
    if m:
        return neg + m.group(1)
    m = re.fullmatch(r'(' + INT + r'(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)(_g|_r|_ins)?', s)
    if m:
        v = m.group(1)
        if m.group(2) is None and ('.' in v or 'e' in v.lower()) and not exact_double(v):
            return None
        return neg + v
    m = re.fullmatch(r'(' + INT + r')[uUlLzZ]*', s)
    if m:
        return neg + m.group(1)
    m = re.fullmatch(r'0[xX][0-9a-fA-F\']+', s)
    if m:
        return neg + s
    return None


def exact_double(v):
    """Whether the decimal v is a binary fraction a double holds exactly (0.5, 10.25, not 0.1)."""
    from fractions import Fraction
    f = Fraction(v)
    return f.denominator & (f.denominator - 1) == 0 and abs(f.numerator) < 2 ** 53


def step(s):
    s = strip_ins(s)
    m = re.fullmatch(r'per<\s*(.*)\s*>', s)
    if m:
        d = number(m.group(1))
        return None if d is None or '/' in d or d.startswith('-') else f'1/{d}'
    m = re.fullmatch(r'max_denominator\{\s*(.*)\s*\}', s)
    if m:
        d = number(m.group(1))
        return None if d is None or '/' in d else f'<{d}>'
    return number(s)


def policy(s):
    """The policy's words, '' for the default, or None."""
    words = []
    for w in s.split('|'):
        w = strip_ins(w)
        if w == 'checked':
            continue
        if w not in FLAGS:
            return None
        words.append(w)
    return '|'.join(words)


# ---- Grids ----

def grid_text(g):
    """The text of a grid spelled as {…}, grid{…} or {{…}, …}, or None."""
    g = strip_ins(g)
    if g.startswith('grid{'):
        g = g[4:]
    if not (g.startswith('{') and g.endswith('}')):
        return None
    parts = split_top(g[1:-1])
    br = ('[', ']')
    if parts and strip_ins(parts[-1]) in BOUNDS:
        br = BOUNDS[strip_ins(parts[-1])]
        parts = parts[:-1]
    note = None
    if len(parts) == 1:
        v = number(parts[0])
        return None if v is None or br != ('[', ']') else f'[{v}]'
    if len(parts) == 2 and (parts[0].startswith('{') or strip_ins(parts[0]).startswith('interval{')):
        iv = strip_ins(parts[0])
        iv = iv[8:] if iv.startswith('interval') else iv
        lim = split_top(iv[1:-1])
        if len(lim) != 2:
            return None
        lo, hi = number(lim[0]), number(lim[1])
        note = step(parts[1])
        if note is None:
            return None
        if note.startswith('<') and br != ('[', ']'):
            return None
    elif len(parts) == 2:
        lo, hi = number(parts[0]), number(parts[1])
    elif len(parts) == 3:
        lo, hi, note = number(parts[0]), number(parts[1]), step(parts[2])
        if note is None or note.startswith('<'):
            return None
    else:
        return None
    if lo is None or hi is None:
        return None
    return f'{br[0]}{lo}..{hi}{br[1]}' + (f' {note}' if note else '')


def inside_parts(args):
    """(grid text, policy words) of inside<…>'s arguments, or None."""
    if len(args) not in (1, 2):
        return None
    g = grid_text(args[0])
    p = policy(args[1]) if len(args) == 2 else ''
    if g is None or p is None:
        return None
    return g, p


FLAG_RE = r'(?:' + '|'.join(FLAGS) + r')(?:\|(?:' + '|'.join(FLAGS) + r'))*'


def rep_parts(rep, aliases):
    """(grid text, policy words) of a quantity's number type: inside<…>, id<"…">, or a local alias of one."""
    rep = strip_ins(rep)
    m = re.fullmatch(r'inside<(.*)>', rep, re.S)
    if m:
        return inside_parts(split_top(m.group(1)))
    m = re.fullmatch(r'id<"([^"]*)">', rep)
    if m:
        t = m.group(1).strip()
        if '[' in t[1:] and re.search(r'\]\s*\[', t):  # a unit in it
            return None
        fm = re.search(r'\s(' + FLAG_RE + r')$', t)
        return (t[:fm.start()].strip(), fm.group(1)) if fm else (t, '')
    if re.fullmatch(r'[A-Za-z_]\w*', rep) and rep in aliases:
        return rep_parts(aliases[rep], {})
    return None


# ---- Units ----

class Units:
    def __init__(self, symtab, text):
        self.unit, self.kind, self.origin = {}, {}, {}
        for line in open(symtab):
            what, name, t = line.rstrip('\n').split(' ', 2)
            {'unit': self.unit, 'kind': self.kind, 'origin': self.origin}[what][strip_ins(name)] = t
        # The file's own: unit<"SYM", …{.ascii = "A"}>, base_unit<"SYM", …>, basis_vector<"SYM", …>,
        # unit_def X_def{"SYM", …} with defined_by<X_def>; kind<"name", …>; origin<"name", …>.
        defs = {}
        for m in re.finditer(r'\b(?:unit_def|basis_vector_def)\s+(\w+)\s*\{\s*"([^"]*)"([^;]*);', text):
            a = re.search(r'\.ascii\s*=\s*"([^"]*)"', m.group(3))
            defs[m.group(1)] = a.group(1) if a else m.group(2)
        kdefs = dict(re.findall(r'\bkind_def\s+(\w+)\s*\{\s*"([^"]*)"', text))
        odefs = dict(re.findall(r'\borigin_def\s+(\w+)\s*\{\s*"([^"]*)"', text))
        for m in re.finditer(r'\b(?:inline\s+)?constexpr\s+auto\s+(\w+)\s*=\s*([^;]*);', text):
            name, init = m.group(1), strip_ins(m.group(2))
            u = re.match(r'(?:unit|base_unit|basis_vector)<\s*"([^"]*)"', init)
            if u:
                a = re.search(r'\.ascii\s*=\s*"([^"]*)"', init)
                self.unit.setdefault(name, a.group(1) if a else u.group(1))
                continue
            d = re.fullmatch(r'defined_by<\s*(\w+)\s*>', init)
            if d and d.group(1) in defs:
                self.unit.setdefault(name, defs[d.group(1)])
            elif d and d.group(1) in kdefs:
                self.kind.setdefault(name, kdefs[d.group(1)])
            elif d and d.group(1) in odefs:
                self.origin.setdefault(name, odefs[d.group(1)])
            k = re.match(r'kind<\s*"([^"]*)"', init)
            if k:
                self.kind.setdefault(name, k.group(1))
            o = re.match(r'origin<\s*"([^"]*)"', init)
            if o:
                self.origin.setdefault(name, o.group(1))
            # A tag of a prefixed or compound unit of the catalog's: its text.
            if name not in self.unit and not u and not d:
                t = self.text(init)
                if t is not None and t[1] is None:
                    self.unit.setdefault(name, t[0])

    # A unit expression as factors [(symbol, exponent)], or None.
    def factors(self, e):
        e = strip_ins(e.strip())
        terms = self.split_mul(e)
        if terms is None:
            return None
        if len(terms) > 1:
            out = []
            for sign, t in terms:
                f = self.factors(t)
                if f is None:
                    return None
                out += [(s, x * sign) for s, x in f]
            return out
        if e.startswith('(') and e.endswith(')') and self.closes(e, 0) == len(e) - 1:
            return self.factors(e[1:-1])
        m = re.fullmatch(r'(?:si::)?(\w+)\((.*)\)', e, re.S)
        if m and m.group(1) in PREFIXES:
            f = self.factors(m.group(2))
            if f is None or len(f) != 1 or f[0][1] != 1:
                return None
            return [(PREFIXES[m.group(1)] + f[0][0], 1)]
        m = re.fullmatch(r'(square|cubic)\((.*)\)', e, re.S)
        if m:
            f = self.factors(m.group(2))
            n = 2 if m.group(1) == 'square' else 3
            return None if f is None else [(s, x * n) for s, x in f]
        m = re.fullmatch(r'pow<\s*(-?\d+)\s*>\((.*)\)', e, re.S)
        if m:
            f = self.factors(m.group(2))
            return None if f is None else [(s, x * int(m.group(1))) for s, x in f]
        if e in self.unit:
            t = self.unit[e]
            if t == '1':
                return []
            if re.search(r'[*/^\[\]() ]', t):
                return None
            return [(t, 1)]
        return None

    @staticmethod
    def closes(e, i):
        d = 0
        for j in range(i, len(e)):
            d += e[j] == '('
            d -= e[j] == ')'
            if d == 0:
                return j
        return -1

    @staticmethod
    def split_mul(e):
        """The top-level factors of a product or quotient: [(+1 or -1, text)], or None for a bracket mismatch."""
        out, cur, depth, sign = [], '', 0, 1
        for c in e:
            if c in '([<{':
                depth += 1
            elif c in ')]>}':
                depth -= 1
            if depth == 0 and c in '*/':
                out.append((sign, cur))
                cur, sign = '', (1 if c == '*' else -1)
            else:
                cur += c
        out.append((sign, cur))
        return None if depth != 0 else out

    def text(self, r):
        """(unit text, kind text or None) of a reference expression, or None."""
        r = strip_ins(r.strip())
        m = re.fullmatch(r'([\w:]+)\[(.*)\]', r, re.S)
        kind = None
        if m:
            k = strip_ins(m.group(1))
            if k not in self.kind:
                return None
            kind, r = self.kind[k], m.group(2)
        f = self.factors(r)
        if f is None:
            return None
        merged = {}
        for s, x in f:
            merged[s] = merged.get(s, 0) + x
        f = [(s, x) for s, x in merged.items() if x != 0]
        if not f:
            return '1', kind

        def put(s, x):
            return s if x == 1 else f'{s}^{x}'
        pos = [put(s, x) for s, x in f if x > 0]
        neg = [put(s, -x) for s, x in f if x < 0]
        if not pos:
            return '*'.join(put(s, x) for s, x in f), kind
        t = '*'.join(pos)
        if neg:
            t += '/' + (neg[0] if len(neg) == 1 else '(' + '*'.join(neg) + ')')
        return t, kind


# ---- The rewrite ----

WORD = re.compile(r'(?<![\w:.])((?:ins::)?)(inside|quantity|quantity_point)<')


def aliases_of(text):
    """Local aliases of inside types defined once in the file (or alike every time): I -> inside<…>."""
    seen = {}
    for m in re.finditer(r'\busing\s+(\w+)\s*=\s*([^;]*);', text):
        seen.setdefault(m.group(1), set()).add(re.sub(r'\s+', ' ', m.group(2).strip()))
    return {k: next(iter(v)) for k, v in seen.items()
            if len(v) == 1 and re.fullmatch(r'(?:ins::)?inside<.*>', next(iter(v)), re.S)}


COMPUTED = False  # --computed: the computed forms only (for tests of what the old spellings name)


def rewrite(old_name, args, units, aliases):
    """The id spelling of one use, or None for one to leave."""
    if old_name == 'inside':
        p = None if COMPUTED else inside_parts(args)
        if p:
            return f'id<"{p[0]}' + (f' {p[1]}' if p[1] else '') + '">'
        if len(args) not in (1, 2):
            return None
        g = strip_ins(args[0])
        g = 'grid' + g if g.startswith('{') else args[0]
        return 'id<key{' + g + (', ' + args[1] if len(args) == 2 else '') + '}>'
    if old_name == 'quantity' and len(args) == 2:
        u = units.text(args[0])
        r = rep_parts(args[1], aliases)
        if u and r and not COMPUTED:
            k = f'{{{u[1]}}}' if u[1] else ''
            return f'id<"{r[0]} [{u[0]}]{k}' + (f' {r[1]}' if r[1] else '') + '">'
        return f'id<quantity_key<{args[0]}, {args[1]}>>'
    if old_name == 'quantity_point' and len(args) == 3:
        u = units.text(args[0])
        o = strip_ins(args[1])
        o = re.sub(r'\.value$', '', o)
        r = rep_parts(args[2], aliases)
        if u and r and o in units.origin and not COMPUTED:
            k = f'{{{u[1]}}}' if u[1] else ''
            return f'id<"{r[0]} [{u[0]}]{k} @{units.origin[o]}' + (f' {r[1]}' if r[1] else '') + '">'
        return f'id<point_key<{args[0]}, {args[1]}, {args[2]}>>'
    return None


def in_comment(text, i):
    line = text.rfind('\n', 0, i) + 1
    return '//' in text[line:i] or text[line:i].lstrip().startswith('*')


def in_macro(text, i):
    """Whether text[i] is a macro's argument (EXPECT_EQ(…)) outside any parentheses of its own."""
    depth = 0
    for j in range(i - 1, max(0, i - 4000), -1):
        c = text[j]
        if c == ')':
            depth += 1
        elif c == '(':
            if depth == 0:
                m = re.search(r'([A-Za-z_]\w*)\s*$', text[max(0, j - 80):j])
                return bool(m) and re.fullmatch(r'[A-Z][A-Z0-9_]+', m.group(1)) is not None
            depth -= 1
        elif c in ';{}' and depth == 0 and c != '{':
            return False
    return False


def in_string(text, i):
    """Whether text[i] is inside a string literal on its line (an expected type name in a test)."""
    line = text.rfind('\n', 0, i) + 1
    s = re.sub(r"\\.|'.'", '', text[line:i])
    return s.count('"') % 2 == 1


def convert(text, units, check):
    aliases = aliases_of(text)
    out, i, n = [], 0, 0
    while True:
        m = WORD.search(text, i)
        if not m:
            out.append(text[i:])
            break
        lt = m.end() - 1
        gt = match_angle(text, lt)
        if gt < 0:
            out.append(text[i:m.end()])
            i = m.end()
            continue
        if in_string(text, m.start()):
            out.append(text[i:m.end()])
            i = m.end()
            continue
        inner = text[lt + 1:gt]
        # The arguments as the plain rewrite gives them; a use it cannot rewrite keeps its own, rewritten inside.
        plain = convert(inner, units, False)[0] if WORD.search(inner) else inner
        new = rewrite(m.group(2), split_top(plain), units, aliases)
        if new is None:
            ci, k = convert(inner, units, check) if WORD.search(inner) else (inner, 0)
            out.append(text[i:lt + 1] + ci + '>')
            n += k
            i = gt + 1
            continue
        if m.group(1):  # ins::inside<…>: the computed form's names qualified too
            new = re.sub(r'^id<(key|quantity_key|point_key)\b', r'id<ins::\1', new)
            new = re.sub(r'^id<ins::key\{grid\{', 'id<ins::key{ins::grid{', new)
        new = m.group(1) + new
        old = text[m.start():gt + 1]
        if check and not in_comment(text, m.start()):
            # (In a macro's argument without a comma: checked_old<OLD>::as<NEW>.)
            new = (f'ins::detail::checked_old<{old}>::as<{new}>' if in_macro(text, m.start())
                   else f'ins::detail::checked_same<{old}, {new}>')
        out.append(text[i:m.start()] + new)
        n += 1
        i = gt + 1
    return ''.join(out), n


def main():
    argv = sys.argv[1:]
    check = '--check' in argv
    diff = '--diff' in argv
    global COMPUTED
    COMPUTED = '--computed' in argv
    argv = [a for a in argv if a not in ('--check', '--diff', '--computed')]
    k = argv.index('--symbols')
    symtab = argv[k + 1]
    files = argv[:k] + argv[k + 2:]
    for p in files:
        text = open(p).read()
        t, n = convert(text, Units(symtab, text), check)
        if diff:
            import difflib
            sys.stdout.writelines(difflib.unified_diff(text.splitlines(True), t.splitlines(True), p, p, n=0))
        elif n:
            open(p, 'w').write(t)
        if not diff:
            print(f'{p}: {n}')


if __name__ == '__main__':
    main()
