#include <dice/ffi/copperr.h>
#include <dice/ffi/copperr_internal.hpp>

using copperr_manager_t = dice::copperr::ffi::internal::copperr_manager;

template <auto open_mode>
copperr_manager *open_impl(char const *path) {
  if (!dice::copperr::manager::consistent(path)) {
    // prevents opening the same datastore twice
    // (because opening removes the properly_closed_mark and this checks for it)
    errno = ENOTRECOVERABLE;
    return nullptr;
  }

  auto *manager = new copperr_manager_t{open_mode, path};
  if (!manager->check_sanity()) {
    delete manager;
    errno = ENOTRECOVERABLE;
    return nullptr;
  }

  return reinterpret_cast<copperr_manager *>(manager);
}

copperr_manager *copperr_open(char const *path) {
  return open_impl<dice::copperr::open_only>(path);
}

copperr_manager *copperr_open_read_only(char const *path) {
  return open_impl<dice::copperr::open_read_only>(path);
}

copperr_manager *copperr_create(char const *path) {
  if (std::filesystem::exists(path)) {
    // prevent accidental overwrite
    errno = EEXIST;
    return nullptr;
  }

  auto *manager = new copperr_manager_t{dice::copperr::create_only, path};
  if (!manager->check_sanity()) {
    delete manager;
    errno = ENOTRECOVERABLE;
    return nullptr;
  }

  return reinterpret_cast<copperr_manager *>(manager);
}

bool copperr_snapshot(copperr_manager *manager, char const *dst_path) {
  return reinterpret_cast<copperr_manager_t *>(manager)->snapshot(dst_path);
}

void copperr_close(copperr_manager *manager) {
  delete reinterpret_cast<copperr_manager_t *>(manager);
}

bool copperr_remove(char const *path) {
  return dice::copperr::manager::remove(path);
}

void *copperr_named_malloc(copperr_manager *manager, char const *name,
                          size_t size) {
  auto *ptr =
      reinterpret_cast<copperr_manager_t *>(manager)->construct<unsigned char>(
          name)[size]();
  if (ptr == nullptr) {
    errno = ENOMEM;
  }

  return ptr;
}

void *copperr_find(copperr_manager *manager, char const *name) {
  auto *ptr = reinterpret_cast<copperr_manager_t *>(manager)
                  ->find<unsigned char>(name)
                  .first;
  if (ptr == nullptr) {
    errno = ENOENT;
  }

  return ptr;
}

bool copperr_named_free(copperr_manager *manager, char const *name) {
  auto const res =
      reinterpret_cast<copperr_manager_t *>(manager)->destroy<unsigned char>(
          name);
  if (!res) {
    errno = ENOENT;
  }

  return res;
}

void *copperr_malloc(copperr_manager *manager, size_t size) {
  auto *ptr = reinterpret_cast<copperr_manager_t *>(manager)->allocate(size);
  if (ptr == nullptr) {
    errno = ENOMEM;
  }

  return ptr;
}

void copperr_free(copperr_manager *manager, void *addr) {
  reinterpret_cast<copperr_manager_t *>(manager)->deallocate(addr);
}

void copperr_flush(copperr_manager *manager) {
  reinterpret_cast<copperr_manager_t *>(manager)->flush();
}
