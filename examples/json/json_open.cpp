#include <iostream>
#include <dice/metall/metall.hpp>
#include <dice/metall/json/json.hpp>

using metall_value_type =
    dice::metall::json::value<dice::metall::manager::allocator_type<std::byte>>;

int main() {
  std::cout << "Open" << std::endl;
  {
    dice::metall::manager manager(dice::metall::open_read_only, "./test");
    auto *value =
        manager.find<metall_value_type>(dice::metall::unique_instance).first;
    dice::metall::json::pretty_print(std::cout, *value);
  }

  {
    dice::metall::manager manager(dice::metall::open_only, "./test");
    manager.destroy<metall_value_type>(dice::metall::unique_instance);
  }

  return 0;
}
