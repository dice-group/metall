#include "gtest/gtest.h"
#include <dice/ffi/copperr.h>

#include <random>
#include <string>

TEST(FFI, SanityCheck) {
  char const *obj_name = "obj";
  std::string const path = "/tmp/copperr-ffi" + std::to_string(std::random_device{}());
  std::string const snap_path = path + "-snap";

  {
    copperr_manager *manager = copperr_create(path.c_str());
    if (manager == nullptr) {
      std::cerr << "failed to create: " << strerror(errno) << std::endl;
      FAIL();
    }

    {
      auto *ptr = static_cast<size_t *>(copperr_named_malloc(manager, obj_name, sizeof(size_t)));
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(size_t), 0);

      *ptr = 55;
      ASSERT_EQ(*ptr, 55);
    }

    copperr_close(manager);
  }

    {
      copperr_manager *manager = copperr_open(path.c_str());
      if (manager == nullptr) {
        std::cerr << "failed to create: " << strerror(errno) << std::endl;
        FAIL();
      }

      auto *ptr = static_cast<size_t *>(copperr_find(manager, obj_name));
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(size_t), 0);

      *ptr = 66;
      ASSERT_EQ(*ptr, 66);

      if (!copperr_snapshot(manager, snap_path.c_str())) {
        std::cerr << "failed to snapshot: " << strerror(errno) << std::endl;
        FAIL();
      }

      copperr_close(manager);
    }

    auto check = [obj_name](auto const &path) {
    {
      copperr_manager *manager = copperr_open_read_only(path.c_str());
      if (manager == nullptr) {
        std::cerr << "failed to open: " << strerror(errno) << std::endl;
        FAIL();
      }

      auto *ptr = static_cast<size_t *>(copperr_find(manager, obj_name));
      if (ptr == nullptr) {
        std::cerr << "failed to load: " << strerror(errno) << std::endl;
        FAIL();
      }

      ASSERT_EQ(*ptr, 66);

      ASSERT_FALSE(copperr_named_free(manager, obj_name));

      copperr_close(manager);
    }

    {
        copperr_manager *manager = copperr_open(path.c_str());
        if (manager == nullptr) {
          std::cerr << "failed to open: " << strerror(errno) << std::endl;
          FAIL();
        }

        if (!copperr_named_free(manager, obj_name)) {
          std::cerr << "failed to dealloc: " << strerror(errno) << std::endl;
          FAIL();
        }

        copperr_close(manager);

        ASSERT_TRUE(copperr_remove(path.c_str()));
        ASSERT_TRUE(!copperr_open(path.c_str()));
    }
  };

  check(snap_path);
  check(path);
}

TEST(FFI, PreventOpenSameDatastoreTwice) {
  std::string const path = "/tmp/" + std::to_string(std::random_device{}());
  copperr_manager *manager = copperr_create(path.c_str());
  if (manager == nullptr) {
    std::cerr << "failed to create datastore: " << strerror(errno) << std::endl;
    FAIL();
  }

  copperr_manager *manager2 = copperr_open(path.c_str());
  ASSERT_EQ(manager2, nullptr);
  ASSERT_EQ(errno, ENOTRECOVERABLE);

  copperr_manager *manager3 = copperr_open(path.c_str());
  ASSERT_EQ(manager3, nullptr);
  ASSERT_EQ(errno, ENOTRECOVERABLE);

  copperr_close(manager);
  ASSERT_TRUE(copperr_remove(path.c_str()));
}
