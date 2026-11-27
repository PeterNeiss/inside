// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Every unit, kind and origin tag of the catalog with the text a key spells it by ("unit si::metre m"), the table
// tools/convert_ids.py reads:
//   g++-16 -std=c++26 -freflection -I include tools/convert_ids_symbols.cpp -o symtab && ./symtab > symtab.txt
#include <inside/units.hpp>
#include <inside/units/constants.hpp>
#include <cstdio>

template <std::meta::info Ns>
void dump()
{
  template for (constexpr std::meta::info m :
                std::define_static_array(std::meta::members_of(Ns, std::meta::access_context::current())))
  {
    if constexpr (std::meta::is_variable(m) && std::meta::has_identifier(m))
    {
      constexpr std::meta::info t{std::meta::remove_cvref(std::meta::type_of(m))};
      constexpr std::string_view ns{std::meta::identifier_of(Ns)};
      constexpr std::string_view id{std::meta::identifier_of(m)};
      if constexpr (std::meta::has_template_arguments(t))
      {
        constexpr std::meta::info of{std::meta::template_of(t)};
        if constexpr (of == ^^ins::unit_t)
        {
          constexpr ins::unit_value u{std::meta::extract<ins::unit_value>(std::meta::template_arguments_of(t)[0])};
          constexpr auto n{ins::detail::quantity_name_for(ins::reference_of(u), &ins::origin_def::none, false)};
          if constexpr (n.Ok)
            std::printf("unit %.*s%s%.*s %s\n", int(ns.size()), ns.data(), "::", int(id.size()), id.data(), n.Name.Unit.Text);
        }
        else if constexpr (of == ^^ins::kind_t)
        {
          constexpr ins::kind_ref k{std::meta::extract<ins::kind_ref>(std::meta::template_arguments_of(t)[0])};
          std::printf("kind %.*s::%.*s %.*s\n", int(ns.size()), ns.data(), int(id.size()), id.data(), int(k->Name.size()), k->Name.data());
        }
        else if constexpr (of == ^^ins::origin_t)
        {
          constexpr ins::origin_ref o{std::meta::extract<ins::origin_ref>(std::meta::template_arguments_of(t)[0])};
          std::printf("origin %.*s::%.*s %.*s\n", int(ns.size()), ns.data(), int(id.size()), id.data(), int(o->Name.size()), o->Name.data());
        }
      }
    }
  }
}
int main()
{
  dump<^^ins>(); dump<^^ins::si>(); dump<^^ins::isq>(); dump<^^ins::international>();
  dump<^^ins::si2019>(); dump<^^ins::codata2022>();
}
