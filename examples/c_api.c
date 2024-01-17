// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <assert.h>
#include <stdint.h>
#include <dice/ffi/copperr.h>

int main(void) {
  // Basic allocation
  {
    copperr_manager *manager = copperr_create("/tmp/copperr1");

    uint64_t *x = copperr_malloc(manager, sizeof(uint64_t));
    x[0] = 1;

    copperr_free(manager, x);
    copperr_close(manager);
  }

  // Allocate named object
  {
    copperr_manager *manager = copperr_create("/tmp/copperr1");

    uint64_t *array = copperr_named_malloc(manager, "array", sizeof(uint64_t) * 10);

    array[0] = 0;
    array[1] = 1;

    copperr_close(manager);
  }

  // Retrieve named object
  {
    copperr_manager *manager = copperr_open("/tmp/copperr1");

    uint64_t *array = copperr_find(manager, "array");

    assert(array[0] == 0);
    assert(array[1] == 1);

    copperr_named_free(manager, "array");
    copperr_close(manager);
    copperr_remove("/tmp/copperrl1");
  }

  return 0;
}