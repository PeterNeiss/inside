// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// JSON numbers in and out, exactly: parse_exact takes each number's text straight into its field's grid
// (no double in between: a 64-bit ID and a price's cents survive),
// a bad field is reported by name and cause, and to_string writes the same text back.
// The fields carry units, so the total is USD by its own arithmetic.

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

using order_id      = inside<{0, 9'000'000'000'000'000'000}>;                                     // 64-bit IDs
using usd_per_item  = quantity<USD / item, inside<{{0, 1'000'000}, per<100>}, round_half_away>>; // to the cent
using items         = quantity<item, inside<{1, 1000}>>;                                          // whole items
using discount_rate = inside<{{0, 1}, per<100>}>;                                                 // to 1 %

struct order
{
  order_id      id;
  usd_per_item  price;
  items         quantity;
  discount_rate discount;
};

// Every field parsed, or the errors by key.
std::expected<order, std::vector<std::pair<std::string, error>>> read_order(std::string_view json)
{
  const auto fields{scan(json)};
  std::vector<std::pair<std::string, error>> errors;
  auto get = [&]<typename B>(const char* key, B& out)
  {
    const auto it{fields.find(key)};
    const auto v {it == fields.end() ? std::expected<B, error>{std::unexpected(error::invalid_format)}
                                     : parse_exact<B>(it->second)};
    if (v)
      out = *v;
    else
      errors.emplace_back(key, v.error());
  };
  order o{order_id{0}, usd_per_item{0, USD / item}, items{1, item}, discount_rate{0}};
  get("id"      , o.id);
  get("price"   , o.price);
  get("quantity", o.quantity);
  get("discount", o.discount);
  if (!errors.empty())
    return std::unexpected(errors);
  return o;
}

// The JSON text holds the bare numbers, written in the fields' units.
std::string write_order(const order& o)
{
  return R"({"id": )" + to_string(o.id)
       + R"(, "price": )" + to_string(o.price.numerical_value_in(USD / item))
       + R"(, "quantity": )" + to_string(o.quantity.numerical_value_in(item))
       + R"(, "discount": )" + to_string(o.discount) + "}";
}

int main()
{
  // 1. A valid order: 2^53 + 1 survives, and the total is exact until its one rounding.
  using dollars = quantity<USD, inside<{{0, 1'000'000'000}, per<100>}, round_half_away>>;
  constexpr std::string_view good = R"({"id": 9007199254740993, "price": 19.99, "quantity": 3, "discount": 0.15})";
  const auto o{read_order(good)};
  if (!o)
    return 1;
  const dollars total{o->price * o->quantity * (just<1> - o->discount)}; // 50.9745 → 50.97
  std::cout << "read   " << good << "\n"
            << "write  " << write_order(*o) << "\n"
            << "total  " << total << "\n"
            << "as double the id is " << static_cast<long long>(std::stod("9007199254740993")) << "\n";
  if (write_order(*o) != good || to_string(total) != "50.97 USD")
    return 1;

  // 2. A bad order: each failing field reported with its cause.
  constexpr std::string_view bad = R"({"id": 12x, "price": 19.999, "quantity": 0, "discount": 0.15})";
  const auto b{read_order(bad)};
  std::cout << "\nread   " << bad << "\n";
  for (const auto& [key, e] : b.error())
    std::cout << "  " << key << ": " << error_message(e) << "\n";
  return !b && b.error().size() == 3 ? 0 : 1; // id malformed; price off the cent grid; quantity below 1
}
