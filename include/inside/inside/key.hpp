// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal: include <inside/inside.hpp> (the umbrella), not this directly.
#ifndef INSIDE_INSIDE_KEY_HPP
#define INSIDE_INSIDE_KEY_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/diag_text.hpp>
#include <inside/inside/detail/id_text.hpp>
#include <inside/inside/grid.hpp>
#include <inside/inside/policy_flag.hpp>

#include <cstddef>
#include <string_view>

// key: the template argument of an inside type, its grid and policy as text, ins::id<"[0..10) 1/4 clamp">.
// The type holds the canonical text (inside/detail/id_text.hpp), so that it reads in diagnostics as it is written,
// ins::id<ins::key{"[0..10) 1/4 clamp"}>, and every spelling of one grid and policy is the same type:
// id<"[0..10)"> is id<"[0..9]">. (GCC prints a class-typed template argument member by member, a char array as a
// string; only the characters written are stored, so a mangled name holds only those.)
// inside<G, P>, the grid and policy as values, names the same type: inside<{0, 9}> is id<"[0..9]">.
namespace ins
{
  struct key;
  namespace detail
  {
    consteval key key_of(grid g, policy_flag p);
    consteval key key_of_text(key raw);
  } // namespace detail

  struct key
  {
    char Text[detail::key_capacity]{};

    // Any spelling of a grid and a policy, written as its canonical text; a text that spells none is kept with a
    // mark, and the type it names fails with where the reading stopped (id's first static_assert).
    // (Through detail::key_of_text, a function of the text alone, whose results GCC keeps:
    // each text is read once however often a type names it.)
    template <std::size_t N>
    consteval key(const char (&text)[N]) : key{detail::key_of_text(key{raw{}, text})} {}
    // A grid and a policy as values (inside<G, P>, and the result types of the operators).
    consteval key(const grid& g, policy_flag p = checked) : key{detail::key_of(g, p)} {}

    [[nodiscard]] constexpr std::string_view text() const { return Text; }
    // Whether the text spells a grid and a policy.
    [[nodiscard]] constexpr bool ok() const { return Text[0] != '!'; }

    friend constexpr bool operator==(const key&, const key&) = default;

  private:
    struct raw
    {
    };
    struct written
    {
    };
    // The characters as given (without the terminating zero, which a diagnostic would print).
    template <std::size_t N>
    consteval key(raw, const char (&text)[N])
    {
      if (N > detail::key_capacity)
        detail::constexpr_error<"inside: an id's text passes 2048 characters (detail::key_capacity)">();
      for (std::size_t i = 0; i + 1 < N && text[i] != '\0'; ++i)
        Text[i] = text[i];
    }
    consteval key(written) {}

    friend consteval key detail::key_of(grid g, policy_flag p);
    friend consteval key detail::key_of_text(key raw);
  };

  namespace detail
  {
    // (By value, so that equal values are one call.)
    consteval key key_of(grid g, policy_flag p)
    {
      key      k{key::written{}};
      text_out t{k.Text, sizeof k.Text};
      write_id_text(t, g, p);
      return k;
    }
    consteval key key_of_text(key raw)
    {
      const id_spec s{parse_id_text(raw.text())};
      if (s.Expected.empty())
        return key_of(s.Grid, s.Policy);
      key k{key::written{}};
      k.Text[0] = '!';
      for (std::size_t i = 0; i + 2 < sizeof k.Text && raw.Text[i] != '\0'; ++i)
        k.Text[i + 1] = raw.Text[i];
      return k;
    }

    // A key's grid and policy, read once (unchecked: the type's static_asserts check the grid).
    // A text that spells none reads as the point 0, so that the type still forms and its static_assert says where
    // the text went wrong.
    consteval id_spec read_key(const key& k)
    {
      if (!k.ok())
        return parse_id_text("[0]");
      const id_spec s{parse_id_text(k.text(), false)};
      if (!s.Expected.empty())
        constexpr_error<"inside: a key's canonical text does not read back">();
      return s;
    }
    template <key K>
    inline constexpr id_spec key_spec = read_key(K);
    template <key K>
    inline constexpr grid key_grid = key_spec<K>.Grid;
    template <key K>
    inline constexpr policy_flag key_policy = key_spec<K>.Policy;

    // Where a text stopped making sense, and what it expected there.
    consteval diag_text key_error(const key& k)
    {
      const std::string_view text{k.text().substr(1)};
      const id_spec          s{parse_id_text(text)};
      return diag_text{} << "inside: id<\"" << text << "\">: expected " << s.Expected << " at "
                         << grid_number{s.Stop} << " (\"" << text.substr(0, s.Stop) << "\" read)";
    }
  } // namespace detail

  // An inside type: the grid and the policy its key spells (inside/detail/core.hpp).
  template <key K>
  struct id;

  // The grid and policy as values: the same type as the id whose key spells them.
  template <grid G = grid{{0, 0}, 0}, policy_flag P = checked>
  using inside = id<key{G, P}>;
} // namespace ins

#endif // INSIDE_INSIDE_KEY_HPP
