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
    // A quantity's unit (or its kind, or a point's origin) as a key holds it, its canonical text
    // (units/detail/unit_key.hpp): by value, so that a function of it is one call per unit.
    struct unit_name
    {
      char Text[256]{};
      [[nodiscard]] constexpr std::string_view view() const { return Text; }
      friend constexpr bool operator==(const unit_name&, const unit_name&) = default;
    };
    // The canonical texts of a quantity's unit, its kind (empty: the unit's own) and a point's origin (empty: a
    // quantity): [km/h]{speed} @origin.
    struct quantity_name
    {
      unit_name Unit;
      unit_name Kind;
      unit_name Origin;
      friend constexpr bool operator==(const quantity_name&, const quantity_name&) = default;
    };

    consteval key key_of(grid g, policy_flag p);
    consteval key key_of(grid g, policy_flag p, quantity_name q);
    template <class Hook>
    consteval key key_of_text(key raw);
    consteval key raw_key(std::string_view text);
    consteval key marked_key(const key& raw);

    struct id_spec;

    // How a key reads a quantity's text (its unit, kind and origin): canonical_quantity_text(text_hook<N>{}, …),
    // found by argument-dependent lookup where the key is formed, so that units/detail/unit_key.hpp, included
    // after this header, provides it. Without it, a unit is an error. (at: where the text went wrong.)
    template <std::size_t N>
    struct text_hook
    {
    };
    template <class Hook>
    consteval bool canonical_quantity_text(Hook, const id_spec& s, quantity_name&, std::string_view& expected,
                                           std::size_t& at)
    {
      expected = "no unit: a quantity's type needs <inside/units.hpp>";
      at       = s.UnitAt;
      return false;
    }
  } // namespace detail

  struct key
  {
    char Text[detail::key_capacity]{};

    // Any spelling of a grid and a policy, written as its canonical text; a text that spells none is kept with a
    // mark, and the type it names fails with where the reading stopped (id's first static_assert).
    // (Through detail::key_of_text, a function of the text alone, whose results GCC keeps:
    // each text is read once however often a type names it.)
    template <std::size_t N>
    consteval key(const char (&text)[N])
      : key{detail::key_of_text<detail::text_hook<N>>(detail::raw_key({text, N - 1}))} {}
    // A grid and a policy as values (inside<G, P>, and the result types of the operators).
    consteval key(const grid& g, policy_flag p = checked) : key{detail::key_of(g, p)} {}

    [[nodiscard]] constexpr std::string_view text() const { return Text; }
    // Whether the text spells a grid and a policy.
    [[nodiscard]] constexpr bool ok() const { return Text[0] != '!'; }

    friend constexpr bool operator==(const key&, const key&) = default;

  private:
    struct written
    {
    };
    consteval key(written) {}

    friend consteval key detail::key_of(grid g, policy_flag p);
    friend consteval key detail::key_of(grid g, policy_flag p, detail::quantity_name q);
    friend consteval key detail::raw_key(std::string_view text);
    friend consteval key detail::marked_key(const key& raw);
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
    consteval key key_of(grid g, policy_flag p, quantity_name q)
    {
      key      k{key::written{}};
      text_out t{k.Text, sizeof k.Text};
      write_id_text(t, g, p, q.Unit.view(), q.Kind.view(), q.Origin.view());
      return k;
    }
    // A text's quantity, canonical (nothing for an inside), or where it went wrong.
    template <class Hook>
    consteval bool read_quantity_of(const id_spec& s, quantity_name& q, std::string_view& expected, std::size_t& at)
    {
      return s.Unit.empty() || canonical_quantity_text(Hook{}, s, q, expected, at);
    }
    template <class Hook>
    consteval key key_of_text(key raw)
    {
      const id_spec    s{parse_id_text(raw.text())};
      quantity_name    q;
      std::string_view expected;
      std::size_t      at{0};
      if (!s.Expected.empty() || !read_quantity_of<Hook>(s, q, expected, at))
        return marked_key(raw);
      return s.Unit.empty() ? key_of(s.Grid, s.Policy) : key_of(s.Grid, s.Policy, q);
    }
    // The characters as given (a literal without its terminating zero, which a diagnostic would print).
    consteval key raw_key(std::string_view text)
    {
      if (text.size() >= key_capacity)
        constexpr_error<"inside: an id's text passes 2048 characters (detail::key_capacity)">();
      key k{key::written{}};
      for (std::size_t i = 0; i < text.size() && text[i] != '\0'; ++i)
        k.Text[i] = text[i];
      return k;
    }
    // A text that spells no type, kept with the mark.
    consteval key marked_key(const key& raw)
    {
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
    // A key with a unit names a quantity (units/detail/quantity.hpp), one without an inside.
    template <key K>
    concept unit_key = !key_spec<K>.Unit.empty() && key_spec<K>.Origin.empty();
    // A key with an origin names a point.
    template <key K>
    concept point_key = !key_spec<K>.Origin.empty();
    // A key without a unit names an inside.
    template <key K>
    concept inside_key = key_spec<K>.Unit.empty();

    // Where a text stopped making sense, and what it expected there.
    template <class Hook>
    consteval diag_text key_error(const key& k)
    {
      const std::string_view text{k.text().substr(1)};
      id_spec                s{parse_id_text(text)};
      quantity_name          q;
      if (s.Expected.empty())
        (void)read_quantity_of<Hook>(s, q, s.Expected, s.Stop);
      return diag_text{} << "inside: id<\"" << text << "\">: expected " << s.Expected << " at "
                         << grid_number{s.Stop} << " (\"" << text.substr(0, s.Stop) << "\" read)";
    }
  } // namespace detail

  namespace detail
  {
    // The text of a value literal, "42 [1..100)"_ins (inside/detail/core.hpp): a number, as id's text reads one,
    // then the text of its type, or nothing for the point. A plain class, so the literal deduces nothing.
    // A number that does not read keeps the whole text in Key, with the mark, and NumberOk false.
    struct value_text
    {
      grid_number Value{};
      key         Key;
      bool        Typed{false};
      bool        NumberOk{true};

      template <std::size_t N>
      consteval value_text(const char (&literal)[N]) : Key{raw_key({literal, N - 1})}
      {
        const std::string_view text{literal, N - 1};
        id_reader              r{text};
        std::string_view expected;
        if (!r.number(Value, expected))
        {
          Value    = grid_number{};
          Key      = marked_key(Key);
          NumberOk = false;
          return;
        }
        r.skip_spaces();
        Typed = r.At != text.size();
        if (!Typed)
        {
          Key = key_of(grid{Value}, 0);
          return;
        }
        // A unit alone ("42 [km/h]"): the point in that unit, [42] [km/h].
        const std::string_view rest{text.substr(r.At)};
        if (const id_spec s{parse_id_text(rest)}; rest.starts_with('[') && s.Stop == 1 && !s.Expected.empty())
        {
          char     typed[key_capacity]{};
          text_out t{typed, sizeof typed};
          t.put('[');
          t.put(Value);
          t.put("] ");
          t.put(rest);
          Key = key_of_text<text_hook<N>>(raw_key({typed, t.Size}));
          return;
        }
        Key = key_of_text<text_hook<N>>(raw_key(rest));
      }
    };

    // Where a value literal's number stopped.
    consteval diag_text value_text_error(const key& k)
    {
      const std::string_view text{k.text().substr(1)};
      id_reader              r{text};
      grid_number            v;
      std::string_view       expected;
      (void)r.number(v, expected);
      return diag_text{} << "inside: \"" << text << "\"_ins: expected " << expected << " at " << grid_number{r.At};
    }
  } // namespace detail

  // An inside type: the grid and the policy its key spells (inside/detail/core.hpp).
  template <key Key>
  struct id;

  // The grid and policy as values: the same type as the id whose key spells them.
  template <grid G = grid{{0, 0}, 0}, policy_flag P = checked>
  using inside = id<key{G, P}>;
} // namespace ins

#endif // INSIDE_INSIDE_KEY_HPP
