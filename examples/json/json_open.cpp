#include <iostream>
#include <dice/copperr/copperr.hpp>
#include <dice/copperr/json/json.hpp>

using copperr_value_type =
    dice::copperr::json::value<dice::copperr::manager::allocator_type<std::byte>>;

int main() {
  std::cout << "Open" << std::endl;
  {
    dice::copperr::manager manager(dice::copperr::open_read_only, "./test");
    auto *value =
        manager.find<copperr_value_type>(dice::copperr::unique_instance).first;
    dice::copperr::json::pretty_print(std::cout, *value);
  }

  {
    dice::copperr::manager manager(dice::copperr::open_only, "./test");
    manager.destroy<copperr_value_type>(dice::copperr::unique_instance);
  }

  return 0;
}
