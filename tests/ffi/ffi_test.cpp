#include "gtest/gtest.h"
#include <dice/ffi/metall.h>

#include <random>
#include <string>

TEST(FFI, SanityCheck) {
  char const *obj_name = "obj";
  std::string const path = "/tmp/metall-ffi" + std::to_string(std::random_device{}());
  std::string const snap_path = path + "-snap";

  {
    metall_manager *manager = metall_create(path.c_str());
    if (manager == nullptr) {
      std::cerr << "failed to create: " << strerror(errno) << std::endl;
      FAIL();
    }

    {
      auto *ptr = static_cast<size_t *>(metall_named_malloc(manager, obj_name, sizeof(size_t)));
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(size_t), 0);

      *ptr = 55;
      ASSERT_EQ(*ptr, 55);
    }

    metall_close(manager);
  }

    {
      metall_manager *manager = metall_open(path.c_str());
      if (manager == nullptr) {
        std::cerr << "failed to create: " << strerror(errno) << std::endl;
        FAIL();
      }

      auto *ptr = static_cast<size_t *>(metall_find(manager, obj_name));
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(size_t), 0);

      *ptr = 66;
      ASSERT_EQ(*ptr, 66);

      if (!metall_snapshot(manager, snap_path.c_str())) {
        std::cerr << "failed to snapshot: " << strerror(errno) << std::endl;
        FAIL();
      }

      metall_close(manager);
    }

    auto check = [obj_name](auto const &path) {
    {
      metall_manager *manager = metall_open_read_only(path.c_str());
      if (manager == nullptr) {
        std::cerr << "failed to open: " << strerror(errno) << std::endl;
        FAIL();
      }

      auto *ptr = static_cast<size_t *>(metall_find(manager, obj_name));
      if (ptr == nullptr) {
        std::cerr << "failed to load: " << strerror(errno) << std::endl;
        FAIL();
      }

      ASSERT_EQ(*ptr, 66);

      ASSERT_FALSE(metall_named_free(manager, obj_name));

      metall_close(manager);
    }

    {
        metall_manager *manager = metall_open(path.c_str());
        if (manager == nullptr) {
          std::cerr << "failed to open: " << strerror(errno) << std::endl;
          FAIL();
        }

        if (!metall_named_free(manager, obj_name)) {
          std::cerr << "failed to dealloc: " << strerror(errno) << std::endl;
          FAIL();
        }

        metall_close(manager);

        ASSERT_TRUE(metall_remove(path.c_str()));
        ASSERT_TRUE(!metall_open(path.c_str()));
    }
  };

  check(snap_path);
  check(path);
}

TEST(FFI, PreventOpenSameDatastoreTwice) {
  std::string const path = "/tmp/" + std::to_string(std::random_device{}());
  metall_manager *manager = metall_create(path.c_str());
  if (manager == nullptr) {
    std::cerr << "failed to create datastore: " << strerror(errno) << std::endl;
    FAIL();
  }

  metall_manager *manager2 = metall_open(path.c_str());
  ASSERT_EQ(manager2, nullptr);
  ASSERT_EQ(errno, ENOTRECOVERABLE);

  metall_manager *manager3 = metall_open(path.c_str());
  ASSERT_EQ(manager3, nullptr);
  ASSERT_EQ(errno, ENOTRECOVERABLE);

  metall_close(manager);
  ASSERT_TRUE(metall_remove(path.c_str()));
}
