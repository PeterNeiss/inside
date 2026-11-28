// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal: include <inside/inside.hpp> (the umbrella), not this directly.
#ifndef INSIDE_INSIDE_DETAIL_ID_TEXT_HPP
#define INSIDE_INSIDE_DETAIL_ID_TEXT_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/grid.hpp>
#include <inside/inside/policy_flag.hpp>
#include <inside/inside/detail/grid_rational.hpp>
#include <inside/inside/detail/number_text.hpp>

#include <bit> // std::countr_zero, std::bit_width
#include <cstddef>
#include <cstdint>
#include <iterator> // std::size
#include <limits>
#include <numeric> // std::gcd
#include <string_view>

// The text of an id: ins::id<"[0..10) 1/4 clamp"> is a grid and a policy.
//   id       := interval [step] [quantity] [flags]       fields apart by spaces (optional where nothing is ambiguous)
//   quantity := "[" unit "]" ["{" kind "}"] ["@" origin]   a quantity, its kind, a point and its origin
//   interval := ("[" | "(") num ".." num ("]" | ")")    closed or open limits
//             | "[" num "]"                            a point
//   step     := num                                    the notch
//             | "<" num ">"                            a continuous grid's max denominator
//   num      := ["-"] number ["/" number]              number: as the _ins literal reads it (0.25, 1e9, 0x1p-4, 1'000)
//   unit     := a unit's text (km/h, kg*m^2/s^2, [1/381*ft]) (units/detail/unit_key.hpp reads these three)
//   kind     := none | any | [any] kinds (height, length*duration): the kind when not the unit's own
//   origin   := a name (ice_point), or 0 for the zero of the dimension
//   flags    := flag {"|" flag}                        clamp, wrap, a rounding mode, unsafe
// Without a step the notch is the one the written limits derive, gcd(1, lo, hi), as grid{lo, hi} derives it,
// and an open limit moves in by one notch, as bounds::open and bounds::half_open do:
// "[0..10)" is the grid {0, 9}, "(0..1) 1/4" is {{1/4, 3/4}, per<4>}.
//
// The canonical text, one per grid and policy (write_id_text), which parse_id_text reads back:
//   a notched grid    the limits with the smallest common denominator: [L..U], [L..U+n), (L−n..U] or (L−n..U+n),
//                     the first of them on a tie (so an integer grid is closed);
//                     then the notch, unless the written limits derive it
//                     ([1..9], [0..10) 1/4, [2/5..3/5] 1/10, [1/2..21/2] 1)
//   a continuous      [L..U] <D>
//   a point           [v]
//   numbers           integers in decimal while they fit 64 bits, else 0x and 16 hexadecimal digits a limb;
//                     a fraction is N/D
//   flags             clamp or wrap, the rounding mode, unsafe, joined by "|"; the default policy has none
namespace ins::detail
{
  // The capacity of an id's text: a type whose text passes it fails to build.
  inline constexpr std::size_t key_capacity = 2048;

  // ---- Writing ----

  // Appends to a fixed buffer, and fails the build past its end.
  // (One check and one loop per piece: every call costs a lot in a constant evaluation.)
  struct text_out
  {
    char*       Out;
    std::size_t Cap;
    std::size_t Size{0};

    constexpr void room(std::size_t n) const
    {
      if (Size + n >= Cap)
        constexpr_error<"inside: an id's text passes 2048 characters (detail::key_capacity)">();
    }
    constexpr void put(char c)
    {
      room(1);
      Out[Size++] = c;
    }
    constexpr void put(std::string_view s)
    {
      room(s.size());
      for (std::size_t i = 0; i < s.size(); ++i)
        Out[Size++] = s[i];
    }
    // A builtin magnitude in decimal: the common case, without a big value.
    constexpr void put(std::uint64_t v)
    {
      std::size_t n{1};
      for (std::uint64_t t{v}; t >= 10; t /= 10)
        ++n;
      room(n);
      Size += n;
      for (std::size_t i = Size; i-- > Size - n; v /= 10)
        Out[i] = static_cast<char>('0' + v % 10);
    }
    // An integer past one limb in hexadecimal, 16 digits a limb (linear in the limbs, where decimal digits would
    // cost a division each), or as m·2^e, 0x…p…, where that is shorter: 2^20000 is 0x1p20000.
    constexpr void put(const grid_int& v)
    {
      if (v.negative())
        put('-');
      if (v.fits_limb())
        return put(v.Small);
      std::size_t zeros{0}, limb{0}; // the trailing zero bits
      for (; v.Limbs[limb] == 0; ++limb)
        zeros += 64;
      zeros += static_cast<std::size_t>(std::countr_zero(v.Limbs[limb]));
      const std::size_t bits{(v.Size - 1) * 64 + static_cast<std::size_t>(std::bit_width(v.Limbs[v.Size - 1]))};
      const std::size_t plain{(bits + 3) / 4}, digits{(bits - zeros + 3) / 4};
      std::size_t       exp_digits{1};
      for (std::size_t t{zeros}; t >= 10; t /= 10)
        ++exp_digits;
      put("0x");
      if (digits + 1 + exp_digits >= plain)
      {
        for (std::size_t i = v.Size; i-- > 0;)
          put_hex(v.Limbs[i], i + 1 != v.Size);
        return;
      }
      // The hexadecimal digits of v >> zeros, from the top.
      room(digits);
      for (std::size_t d = digits; d-- > 0;)
      {
        const std::size_t at{zeros + 4 * d}, i{at / 64}, s{at % 64};
        std::uint64_t     nibble{v.Limbs[i] >> s};
        if (s > 60 && i + 1 < v.Size)
          nibble |= v.Limbs[i + 1] << (64 - s);
        Out[Size++] = "0123456789abcdef"[nibble & 15];
      }
      put('p');
      put(static_cast<std::uint64_t>(zeros));
    }
    constexpr void put_hex(std::uint64_t x, bool pad)
    {
      std::size_t n{16};
      if (!pad)
      {
        n = 1;
        for (std::uint64_t t{x}; t >= 16; t >>= 4)
          ++n;
      }
      room(n);
      Size += n;
      for (std::size_t i = Size; i-- > Size - n; x >>= 4)
        Out[i] = "0123456789abcdef"[x & 15];
    }
    // An integer, or N/D.
    constexpr void put(const grid_number& r)
    {
      if (r.fits_rational()) // inline: Num the magnitude, Den the denominator with the sign (0 for zero)
      {
        if (r.Den < 0)
          put('-');
        put(r.Num);
        if (const std::uint64_t den{r.Den == 0 ? 1 : abs_den(r.Den)}; den != 1)
        {
          put('/');
          put(den);
        }
        return;
      }
      put(grid_numerator(r));
      if (const grid_int d{grid_denominator(r)}; !(d == grid_int{1}))
      {
        put('/');
        put(d);
      }
    }
  };

  // gcd(1, lo, hi), the notch grid{lo, hi} derives: 1 / the lcm of the limits' denominators.
  // In 64 bits while they fit, else as grid numbers.
  constexpr grid_number derived_notch(const grid_number& lo, const grid_number& hi)
  {
    if (lo.fits_rational() && hi.fits_rational())
    {
      const auto den = [](const grid_number& x) { return x.Den == 0 ? std::uint64_t{1} : abs_den(x.Den); };
      const std::uint64_t a{den(lo)}, b{den(hi)}, q{a / std::gcd(a, b)};
      if (b <= std::uint64_t{std::numeric_limits<std::int64_t>::max()} / q)
      {
        grid_number n;
        n.Num = 1;
        n.Den = static_cast<std::int64_t>(q * b);
        return n;
      }
    }
    return gcd(gcd(grid_number{1}, lo), hi);
  }

  // The words of the flags, in the order the canonical text writes them, and their bits.
  struct id_flag
  {
    std::string_view Word;
    policy_flag      Bits;
    bool             Mode; // a rounding mode: one at most
  };
  inline constexpr id_flag id_flags[]{
    {"clamp"            , clamp            , false},
    {"wrap"             , wrap             , false},
    {"round_half_away"  , round_half_away  , true },
    {"round_half_even"  , round_half_even  , true },
    {"round_floor"      , round_floor      , true },
    {"round_ceil"       , round_ceil       , true },
    {"round_toward_zero", round_toward_zero, true },
    {"unsafe"           , unsafe           , false},
    {"cursor"           , cursor_marker    , false}, // the library's mark of a cursor's grid
  };
  inline constexpr std::string_view id_flag_list{
    "a flag: clamp, wrap, round_half_away, round_half_even, round_floor, round_ceil, round_toward_zero or unsafe"};

  // The flags of p: each word whose own bits p holds (a mode and unsafe carry snap, round_toward_zero's bit,
  // so round_toward_zero is written only for snap alone); bits no word names as a number.
  consteval void write_flags(text_out& t, policy_flag p)
  {
    if (p == 0)
      return; // checked, the default
    policy_flag named{0};
    std::size_t start{t.Size};
    const auto  word = [&](const id_flag& f)
    {
      t.put(t.Size == start ? ' ' : '|');
      t.put(f.Word);
      named |= f.Bits;
    };
    const bool carries_snap{(p & (mode_bits | unsafe_marker)) != 0};
    for (const id_flag& f : id_flags)
    {
      const bool toward_zero{f.Bits == policy_flag{round_toward_zero}};
      const policy_flag own{toward_zero ? f.Bits : f.Bits & ~policy_flag{snap}};
      if ((p & own) == own && !(toward_zero && carries_snap))
        word(f);
    }
    if (named != p) // a mode without snap, or bits no flag names: the bits themselves, bits_N
    {
      for (std::size_t i = start; i < t.Size; ++i) // (the words written: a key holds what it reads)
        t.Out[i] = '\0';
      t.Size = start;
      t.put(" bits_");
      t.put(static_cast<std::uint64_t>(p));
    }
  }

  // The canonical text of a grid and a policy.
  // (unit, kind, origin: a quantity's, their canonical texts, written before the flags: [km/h]{speed} @origin.)
  consteval void write_id_text(text_out& t, const grid& g, policy_flag p, std::string_view unit = {},
                               std::string_view kind = {}, std::string_view origin = {})
  {
    const auto unit_and_flags = [&]
    {
      if (!unit.empty())
      {
        t.put(" [");
        t.put(unit);
        t.put(']');
        if (!kind.empty())
        {
          t.put('{');
          t.put(kind);
          t.put('}');
        }
        if (!origin.empty())
        {
          t.put(" @");
          t.put(origin);
        }
      }
      write_flags(t, p);
    };
    if (g.Notch.is_zero())
    {
      t.put('[');
      t.put(g.Lower);
      if (g.Lower != g.Upper || !g.MaxDenominator.is_zero()) // continuous (or a point with a bound: no grid)
      {
        t.put("..");
        t.put(g.Upper);
        t.put("] <");
        t.put(g.MaxDenominator);
        t.put('>');
      }
      else
        t.put(']');
      unit_and_flags();
      return;
    }
    // The limits as written: the four spellings, the smallest common denominator first, closed on a tie.
    // Integer limits are closed outright (nothing beats a denominator of 1).
    grid_number lo{g.Lower}, hi{g.Upper};
    char        open{'['}, close{']'};
    if (!(g.Lower.is_integer() && g.Upper.is_integer()))
    {
      const grid_number below{grid_number::sub(g.Lower, g.Notch)}, above{grid_number::add(g.Upper, g.Notch)};
      const auto        den = [](const grid_number& a, const grid_number& b)
      {
        const grid_int x{grid_denominator(a)}, y{grid_denominator(b)};
        return x / gcd(x, y) * y;
      };
      grid_int best{den(lo, hi)};
      const auto try_form = [&](const grid_number& l, const grid_number& h, char o, char c)
      {
        if (const grid_int d{den(l, h)}; d < best)
        {
          best  = d;
          lo    = l;
          hi    = h;
          open  = o;
          close = c;
        }
      };
      try_form(g.Lower, above, '[', ')');
      try_form(below, g.Upper, '(', ']');
      try_form(below, above, '(', ')');
    }
    t.put(open);
    t.put(lo);
    t.put("..");
    t.put(hi);
    t.put(close);
    if (g.Notch != derived_notch(lo, hi))
    {
      t.put(' ');
      t.put(g.Notch);
    }
    unit_and_flags();
  }

  // ---- Reading ----

  // What a text spells: a grid and a policy,
  // or where the reading stopped (Stop, an index into the text) and what was expected there (Expected, else empty).
  struct id_spec
  {
    grid             Grid{};
    policy_flag      Policy{0};
    std::size_t      Stop{0};
    std::string_view Expected{};
    std::string_view Unit{}; // a quantity's unit, as written (empty: an inside); within the text read
    std::size_t      UnitAt{0};
    std::string_view Kind{}; // its kind, between braces (empty: the unit's own)
    std::size_t      KindAt{0};
    std::string_view Origin{}; // a point's origin, after @ (empty: a quantity)
    std::size_t      OriginAt{0};
  };

  struct id_reader
  {
    std::string_view Text;
    std::size_t      At{0};

    static constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
    static constexpr bool is_letter(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

    [[nodiscard]] constexpr char peek() const noexcept { return At < Text.size() ? Text[At] : '\0'; }
    constexpr void               skip_spaces() noexcept
    {
      while (At < Text.size() && (Text[At] == ' ' || Text[At] == '\t'))
        ++At;
    }
    // Reads s after any spaces, if it comes next.
    constexpr bool eat(std::string_view s) noexcept
    {
      skip_spaces();
      if (!Text.substr(At).starts_with(s))
        return false;
      At += s.size();
      return true;
    }

    // An unsigned number token from At: digits and letters, ' separators, a point (not "..") and an exponent's sign.
    consteval bool unsigned_number(grid_number& out)
    {
      const std::size_t first{At};
      if (!is_digit(peek()))
        return false;
      std::uint64_t small{0};
      bool          plain{true}; // decimal digits only, at most 19: the value without a big number
      for (; At < Text.size(); ++At)
      {
        const char c{Text[At]};
        if (is_digit(c))
        {
          if (At - first == 19)
            plain = false;
          small = small * 10 + static_cast<std::uint64_t>(c - '0');
          continue;
        }
        if (is_letter(c) || c == '\'')
          plain = false;
        else if (c == '.' && !(At + 1 < Text.size() && Text[At + 1] == '.'))
          plain = false;
        else if ((c == '+' || c == '-') && At > first
                 && (Text[At - 1] == 'e' || Text[At - 1] == 'E' || Text[At - 1] == 'p' || Text[At - 1] == 'P'))
          plain = false;
        else
          break;
      }
      if (plain)
      {
        out = grid_number{small};
        return true;
      }
      // (An exponent to 100000, as scan_number reads it: 0x1p20000 is a canonical number.)
      if (scan_exact(Text.data() + first, Text.data() + At, out, 100000) != parse_fail::none)
      {
        At = first;
        return false;
      }
      return true;
    }
    // ["-"] number ["/" number]: true with the value, or false where it stops, with what it expected.
    consteval bool number(grid_number& out, std::string_view& expected)
    {
      skip_spaces();
      const bool neg{peek() == '-'};
      if (neg)
        ++At;
      if (!unsigned_number(out))
      {
        expected = "a number";
        return false;
      }
      if (eat("/"))
      {
        skip_spaces();
        grid_number d;
        const std::size_t at{At};
        if (!unsigned_number(d) || d.is_zero())
        {
          At       = at;
          expected = "a denominator above 0";
          return false;
        }
        out = grid_number::div(out, d);
      }
      if (neg)
        out = grid_number::neg(out);
      return true;
    }
  };

  // Reads any spelling of an id's text (see above) into its grid and policy.
  // check: whether the grid must be one an inside holds. A key's own canonical text is read without it,
  // so that it reads back every grid a type names, also one no inside has (a result past a bound, which an
  // operator's constraint tests by its grid, and which grid::validate rejects when the type is used).
  consteval id_spec parse_id_text(std::string_view text, bool check = true)
  {
    id_reader r{text};
    id_spec   s;
    const auto stop = [&](std::size_t at, std::string_view expected)
    {
      s.Stop     = at;
      s.Expected = expected;
      return s;
    };

    // The interval.
    r.skip_spaces();
    const std::size_t open_at{r.At};
    const bool        open_lo{r.eat("(")};
    if (!open_lo && !r.eat("["))
      return stop(r.At, "'[' or '(' to open the interval");
    grid_number      lo, hi;
    std::string_view expected;
    if (!r.number(lo, expected))
      return stop(r.At, expected);
    bool point{false}, open_hi{false};
    if (r.eat(".."))
    {
      if (!r.number(hi, expected))
        return stop(r.At, expected);
      open_hi = r.eat(")");
      if (!open_hi && !r.eat("]"))
        return stop(r.At, "']' or ')' to close the interval");
    }
    else if (!open_lo && r.eat("]"))
    {
      point = true;
      hi    = lo;
    }
    else
      return stop(r.At, open_lo ? "'..'" : "'..', or ']' for a point");
    if (check && hi < lo)
      return stop(open_at, "an upper limit at least the lower");

    // The step, or a continuous grid's bound.
    r.skip_spaces();
    const std::size_t step_at{r.At};
    grid_number       notch, bound;
    bool              has_notch{false}, has_bound{false};
    if (r.eat("<"))
    {
      if (!r.number(bound, expected))
        return stop(r.At, expected);
      if (!r.eat(">"))
        return stop(r.At, "'>' to close the max denominator");
      if (point)
        return stop(step_at, "no max denominator on a point");
      has_bound = true;
    }
    if (has_bound && check)
    {
      if (!(bound > 0) || !bound.is_integer())
        return stop(step_at + 1, "a max denominator that is a whole number above 0");
      if (bound < grid_number{grid_denominator(lo)} || bound < grid_number{grid_denominator(hi)})
        return stop(step_at + 1, "a max denominator at least the limits' denominators");
      if (open_lo || open_hi)
        return stop(open_lo ? open_at : step_at, "closed limits: a continuous grid has no next point to move in to");
      if (lo == hi)
        return stop(open_at, "an upper limit above the lower on a continuous grid");
    }
    else if (!has_bound && (r.peek() == '-' || id_reader::is_digit(r.peek())))
    {
      if (!r.number(notch, expected))
        return stop(r.At, expected);
      if (point)
        return stop(step_at, "no step on a point");
      if (check && !(notch > 0))
        return stop(step_at, "a step above 0");
      has_notch = true;
    }

    // A unit: what lies between "[" and its "]" (a scaled unit has brackets of its own: [[1/381*ft]]).
    r.skip_spaces();
    bool has_unit{false};
    if (r.peek() == '[')
    {
      std::size_t       depth{0}, end{r.At};
      const std::size_t first{r.At + 1};
      for (; end < text.size(); ++end)
        if (text[end] == '[')
          ++depth;
        else if (text[end] == ']' && --depth == 0)
          break;
      if (end == text.size())
        return stop(r.At, "']' to close the unit");
      std::size_t ub{first}, ue{end}; // (by index: a sanitized build rejects pointer arithmetic across views)
      while (ub < ue && text[ub] == ' ')
        ++ub;
      while (ue > ub && text[ue - 1] == ' ')
        --ue;
      if (ub == ue)
        return stop(first, "a unit");
      s.Unit   = text.substr(ub, ue - ub);
      s.UnitAt = ub;
      r.At     = end + 1;
      has_unit = true;
      // Its kind, right after it.
      if (r.peek() == '{')
      {
        const std::size_t kb{r.At + 1};
        std::size_t       ke{kb};
        while (ke < text.size() && text[ke] != '}')
          ++ke;
        if (ke == text.size())
          return stop(r.At, "'}' to close the kind");
        std::size_t b{kb}, e{ke};
        while (b < e && text[b] == ' ')
          ++b;
        while (e > b && text[e - 1] == ' ')
          --e;
        if (b == e)
          return stop(kb, "a kind (none, any, or kinds: height, length*duration)");
        s.Kind   = text.substr(b, e - b);
        s.KindAt = b;
        r.At     = ke + 1;
      }
      // A point's origin.
      if (r.eat("@"))
      {
        r.skip_spaces();
        const std::size_t ob{r.At};
        std::size_t       oe{ob};
        while (oe < text.size() && (id_reader::is_letter(text[oe]) || id_reader::is_digit(text[oe]) || text[oe] == '_'))
          ++oe;
        if (oe == ob)
          return stop(ob, "an origin: its name, or 0");
        s.Origin   = text.substr(ob, oe - ob);
        s.OriginAt = ob;
        r.At       = oe;
      }
    }

    // The flags.
    r.skip_spaces();
    const bool has_flags{id_reader::is_letter(r.peek())};
    if (has_flags)
    {
      int modes{0};
      do
      {
        r.skip_spaces();
        const std::size_t at{r.At};
        std::size_t       end{at};
        while (end < text.size() && (id_reader::is_letter(text[end]) || text[end] == '_'))
          ++end;
        const std::string_view w{text.substr(at, end - at)};
        if (w == "bits_") // bits_N: the bits themselves (bits no flag names, as a type may hold them)
        {
          r.At = end;
          grid_number bits;
          if (!r.unsigned_number(bits) || !bits.is_integer() || !(bits <= grid_number{~policy_flag{0}}))
            return stop(end, "the bits of a policy");
          s.Policy |= static_cast<policy_flag>(bits.Num);
          continue;
        }
        // (By index: GCC under -fsanitize=pointer-compare rejects a pointer test in a constant evaluation.)
        std::size_t i{0};
        while (i < std::size(id_flags) && id_flags[i].Word != w)
          ++i;
        if (i == std::size(id_flags))
          return stop(at, id_flag_list);
        const id_flag& found{id_flags[i]};
        if (check && found.Mode && ++modes > 1)
          return stop(at, "one rounding mode");
        if (check
            && ((found.Bits == policy_flag{clamp} && (s.Policy & wrap) != 0)
                || (found.Bits == policy_flag{wrap} && (s.Policy & clamp) != 0)))
          return stop(at, "clamp or wrap, not both");
        s.Policy |= found.Bits;
        r.At      = end;
      } while (r.eat("|"));
    }
    r.skip_spaces();
    if (r.At != text.size())
      return stop(r.At, has_flags                ? "'|' and a flag, or the end"
                        : has_unit               ? "a flag, or the end"
                        : has_notch || has_bound ? "a [unit], a flag, or the end"
                                                 : "a step, <max denominator>, a [unit], a flag, or the end");

    // The grid.
    if (point)
    {
      s.Grid = grid{lo};
      return s;
    }
    if (has_bound)
    {
      s.Grid                = grid{ins::interval{lo, hi}, grid_number{0}};
      s.Grid.MaxDenominator = bound;
      return s;
    }
    if (!has_notch)
      notch = derived_notch(lo, hi);
    if (check && !grid_divides_evenly(grid_number::sub(hi, lo), notch))
      return stop(step_at, "a step that divides upper - lower");
    if (open_lo)
      lo = grid_number::add(lo, notch);
    if (open_hi)
      hi = grid_number::sub(hi, notch);
    if (check && hi < lo)
      return stop(open_at, "a grid point between the open limits");
    s.Grid = grid{ins::interval{lo, hi}, notch};
    return s;
  }
} // namespace ins::detail

#endif // INSIDE_INSIDE_DETAIL_ID_TEXT_HPP
