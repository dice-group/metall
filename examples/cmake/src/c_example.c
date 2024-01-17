// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <dice/copperr/c_api/copperr.h>

int main(void) {
  copperr_open(METALL_CREATE_ONLY, "/tmp/dir");
  copperr_close();

  return 0;
}