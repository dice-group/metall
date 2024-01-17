[![Documentation Status](https://readthedocs.org/projects/metall/badge/?version=latest)](https://copperr.readthedocs.io/en/latest/?badge=latest)
[![Deploy API Doc](https://github.com/LLNL/copperr/actions/workflows/deploy-api-doc.yml/badge.svg?branch=master)](https://github.com/LLNL/metall/actions/workflows/deploy-api-doc.yml)

Copperr: A Persistent Memory Allocator for Data-Centric Analytics based on Metall
===============================================

* Copper is based on [Metall](https://github.com/LLNL/metall) but will be adapted to the needs of dice-group over time
* Provides rich memory allocation interfaces for C++ applications that
  use persistent memory devices to persistently store heap data on such
  devices.
* Creates files in persistent memory and maps them into virtual memory
  space so that users can access the mapped region just as normal memory
  regions allocated in DRAM.
* Actual persistent memory hardware could be any non-volatile memory (NVM) with file system support.
* To provide persistent memory allocation, Copperr employs concepts and
  APIs developed by
  [Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess.html).
* Supports multi-thread
* Also provides a space-efficient snapshot/versioning, leveraging reflink
  copy mechanism in filesystem. In case reflink is not supported, Copperr
  automatically falls back to regular copy.
* See details: [Metall overview slides](docs/publications/metall_101.pdf).
  

# Getting Started

Copperr consists of only header files and requires some header files in Boost C++ Libraries.

All core files exist under
[copperr/include/copperr/](https://github.com/dice-group/copperr/tree/master/include/copperr).

## Required

- Boost C++ Libraries >=1.64.
  - Build is not required; needs only their header files.
  - To use JSON containers in Copperr, Boost C++ Libraries >=1.75 is required.
- C++20 compiler
  - Tested with GCC>=13 / Clang>=15

## Usage with conan

You need the package manager Conan installed and set up. You can add the DICE artifactory with:
```shell
conan remote add dice-group https://conan.dice-research.org/artifactory/api/conan/tentris
```

To use copperr, add it to your conanfile.txt:
```shell
[requires]
copperr/0.0.x
```

### Defining a sink for the logger
To Use Copperr your application must define a logger sink for Copperr.
A reasonable default sink is provided via the Copperr::default_logger cmake target you can link against that
if you do not want to provide your own logger implementation.

The interface you need to implement is defined in `include/logger_interface.h`.
For an example how to define such a sink see `src/default_logger.cpp`.

If you don't you will get the following linker error:
```
Error: Undefined reference to `copperr_log`
```

## Use Copperr from Another CMake Project

To download and/or link Copperr package from a CMake project,
see example CMake files placed [here](./example/cmake).

# Build Example Programs

Copperr repository contains some example programs under [example directory](./example).
One can use CMake to build the examples.
For more details, see a page
[here](https://copperr.readthedocs.io/en/latest/advanced_build/cmake/).


# Documentation

- [Full documentation](https://copperr.readthedocs.io/)
- [API documentation](https://software.llnl.gov/copperr/api/)

## Generate API documentation using Doxygen

A Doxygen configuration file is [here](docs/Doxyfile.in).

To generate API document:

```bash
cd copperr
mkdir build_doc
cd build_doc
doxygen ../docs/Doxyfile.in
```


# Publication related to Metall

```
Keita Iwabuchi, Karim Youssef, Kaushik Velusamy, Maya Gokhale, Roger Pearce,
Copperr: A persistent memory allocator for data-centric analytics,
Parallel Computing, 2022, 102905, ISSN 0167-8191, https://doi.org/10.1016/j.parco.2022.102905.
```

* [Parallel Computing](https://www.sciencedirect.com/science/article/abs/pii/S0167819122000114) (journal)

* [arXiv](https://arxiv.org/abs/2108.07223) (preprint)

# About

## Contact
- [GitHub Issues](https://github.com/dice-group/copperr/issues) are open.

## License

Copperr is distributed under the terms of both the MIT license and the
Apache License (Version 2.0). Users may choose either license, at their
option.

All new contributions must be made under both the MIT and Apache-2.0
licenses.

See [LICENSE-MIT](LICENSE-MIT), [LICENSE-APACHE](LICENSE-APACHE),
[NOTICE](NOTICE), and [COPYRIGHT](COPYRIGHT) for details.

SPDX-License-Identifier: (Apache-2.0 OR MIT)


## Release

LLNL-CODE-768617
