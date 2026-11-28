// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// JSON numbers in and out, exactly and with their units: a key names its field's unit in brackets
// ("price[USD/item]": 19.99), parse_exact takes the number's text straight into the field's grid in that unit
// (no double in between: a 64-bit ID and a price's cents survive), converting any other unit exactly
// (¢/item, ‰), a bad field is reported by name and cause, and the writer names each field's unit.

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/inside/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto USD  = base_unit<"USD" , "dollar">;
  inline constexpr auto cent = unit<"¢", 0.01_ins * USD, {.ascii = "c"}>;
  inline constexpr auto item = base_unit<"item", "count">;
} // namespace

// A flat JSON object of numbers, key → the number's text:
// no nesting, strings or escapes, just enough for parse.
std::map<std::string, std::string_view> scan(std::string_view json)
{
  std::map<std::string, std::string_view> fields;
  std::size_t i{0};
  while ((i = json.find('"', i)) != std::string_view::npos)
  {
    const std::size_t k_end{json.find('"', i + 1)};
    const std::string key  {json.substr(i + 1, k_end - i - 1)};
    const std::size_t v    {json.find_first_not_of(" :", k_end + 1)};
    const std::size_t v_end{json.find_first_of(",}", v)};
    fields[key] = json.substr(v, json.find_last_not_of(' ', v_end - 1) + 1 - v);
    i = v_end;
  }
  return fields;
}

using order_id      = id<"[0..9'000'000'000'000'000'000]">;                                     // 64-bit IDs
using usd_per_item  = id<"[0..1'000'000] 1/100 [USD/item] round_half_away">; // to the cent
using items         = id<"[1..1000] [item]">;                                                     // whole items
using discount_rate = id<"[0..100] [%]">;                                                         // whole percent

struct order
{
  order_id      id;
  usd_per_item  price;
  items         quantity;
  discount_rate discount;
};

// Every field parsed, or the errors by key. A field's key is its name, and for a quantity its unit in brackets.
std::expected<order, std::vector<std::pair<std::string, error>>> read_order(std::string_view json)
{
  const auto fields{scan(json)};
  std::vector<std::pair<std::string, error>> errors;
  auto get = [&]<typename B>(std::string_view name, B& out)
  {
    std::expected<B, error> v{std::unexpected(error::invalid_format)};
    for (const auto& [key, text] : fields)
    {
      if constexpr (quantity_like<B>)
      {
        if (key.starts_with(name) && key.size() > name.size() + 1 && key[name.size()] == '[' && key.ends_with(']'))
          v = parse_exact<B>(text, std::string_view{key}.substr(name.size() + 1, key.size() - name.size() - 2));
      }
      else if (key == name)
        v = parse_exact<B>(text);
    }
    if (v)
      out = *v;
    else
      errors.emplace_back(name, v.error());
  };
  order o{order_id{0}, usd_per_item{0, USD / item}, items{1, item}, discount_rate{0, percent}};
  get("id"      , o.id);
  get("price"   , o.price);
  get("quantity", o.quantity);
  get("discount", o.discount);
  if (!errors.empty())
    return std::unexpected(errors);
  return o;
}

// The JSON text holds bare numbers, each key naming its unit.
template <class Q>
std::string field(std::string_view name, const Q& q)
{
  return "\"" + std::string{name} + "[" + unit_symbol(Q::unit)
         + "]\": " + to_string(q.numerical_value_in(unit_t<Q::unit>{}));
}
std::string write_order(const order& o)
{
  return R"({"id": )" + to_string(o.id) + ", " + field("price", o.price) + ", " + field("quantity", o.quantity)
         + ", " + field("discount", o.discount) + "}";
}

int main()
{
  // 1. A valid order: 2^53 + 1 survives, and the total is exact until its one rounding.
  using dollars = id<"[0..1'000'000'000] 1/100 [USD] round_half_away">;
  constexpr std::string_view good =
    R"({"id": 9007199254740993, "price[USD/item]": 19.99, "quantity[item]": 3, "discount[%]": 15})";
  const auto o{read_order(good)};
  if (!o)
    return 1;
  const dollars total{o->price * o->quantity * (just<100> * percent - o->discount)}; // 50.9745 → 50.97
  std::cout << "read   " << good << "\n"
            << "write  " << write_order(*o) << "\n"
            << "total  " << total << "\n"
            << "as double the id is " << static_cast<long long>(std::stod("9007199254740993")) << "\n";
  if (write_order(*o) != good || to_string(total) != "50.97 USD")
    return 1;

  // 2. The same order from a system that counts in cents and per mille: converted exactly into the fields' units.
  constexpr std::string_view other =
    R"({"id": 9007199254740993, "price[¢/item]": 1999, "quantity[item]": 3, "discount[‰]": 150})";
  const auto p{read_order(other)};
  std::cout << "\nread   " << other << "\nwrite  " << (p ? write_order(*p) : "error") << "\n";
  if (!p || write_order(*p) != good)
    return 1;

  // 3. A bad order: each failing field reported with its cause.
  constexpr std::string_view bad =
    R"({"id": 12x, "price[USD/item]": 19.999, "quantity[s]": 3, "discount[%%]": 15})";
  const auto b{read_order(bad)};
  std::cout << "\nread   " << bad << "\n";
  for (const auto& [key, e] : b.error())
    std::cout << "  " << key << ": " << error_message(e) << "\n";
  // id malformed; price off the cent grid; quantity in seconds; discount in no unit known
  return !b && b.error().size() == 4 ? 0 : 1;
}
