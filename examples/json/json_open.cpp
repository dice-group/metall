#include <iostream>
#include <dice/metall/metall.hpp>
#include <dice/metall/json/json.hpp>

using metall_value_type =
    dice::copperr::json::value<dice::copperr::manager::allocator_type<std::byte>>;

int main() {
  std::cout << "Open" << std::endl;
  {
    dice::copperr::manager manager(dice::copperr::open_read_only, "./test");
    auto *value =
        manager.find<metall_value_type>(dice::copperr::unique_instance).first;
    dice::copperr::json::pretty_print(std::cout, *value);
  }

  {
    dice::copperr::manager manager(dice::copperr::open_only, "./test");
    manager.destroy<metall_value_type>(dice::copperr::unique_instance);
  }

  return 0;
}
