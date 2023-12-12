// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <assert.h>
#include <stdint.h>
#include <dice/ffi/metall.h>

int main(void) {
  // Basic allocation
  {
    metall_manager *manager = metall_create("/tmp/metall1");

    uint64_t *x = metall_malloc(manager, sizeof(uint64_t));
    x[0] = 1;

    metall_free(manager, x);
    metall_close(manager);
  }

  // Allocate named object
  {
    metall_manager *manager = metall_create("/tmp/metall1");

    uint64_t *array = metall_named_malloc(manager, "array", sizeof(uint64_t) * 10);

    array[0] = 0;
    array[1] = 1;

    metall_close(manager);
  }

  // Retrieve named object
  {
    metall_manager *manager = metall_open("/tmp/metall1");

    uint64_t *array = metall_find(manager, "array");

    assert(array[0] == 0);
    assert(array[1] == 1);

    metall_named_free(manager, "array");
    metall_close(manager);
    metall_remove("/tmp/metalll1");
  }

  return 0;
}