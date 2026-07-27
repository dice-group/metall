# Segment backend comparison

Compares metall's default segment backend against the privateer backend on one
workload: a graph of small nodes in the shape a hypertrie has, read by pointer
chases and written by path copies, with checkpoints, snapshots and a retention
window.

Both backends live in the same binary. The translation unit does not define
`METALL_USE_PRIVATEER`, so `metall::manager` stays the default backend and
`metall::manager_privateer` is the other arm; `--backend` selects which one
runs.

## Build

```sh
conan install --requires=privateer/0.2.0@dice-group/<branch> \
  -g CMakeDeps -g CMakeToolchain -s build_type=Release --build=missing \
  --format=json -of pv-deps > pv-deps/graph.json
BOOST=$(jq -r '.graph.nodes[] | select(.name == "boost") | .package_folder' \
  pv-deps/graph.json)/include
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/pv-deps/conan_toolchain.cmake \
  -DUSE_PRIVATEER=ON -DBUILD_BENCH=ON -DBOOST_INCLUDE_ROOT=${BOOST}
cmake --build build --target run_segment_backend_bench -j
```

Three things that cost time when they are missed. The conan `build_type` and
`CMAKE_BUILD_TYPE` have to match, because CMakeDeps writes configuration
specific properties and a mismatch silently drops the include directories. Use
gcc: metall forces `-stdlib=libc++` for clang on Linux, which needs conan
packages built for libc++. And `BOOST_INCLUDE_ROOT` has to be the package
folder of the Boost the engine brings, which is what the `jq` line above reads;
`conan cache path boost/<version>` returns the recipe folder, which holds no
headers, and a wrong value here only surfaces in a build without the privateer
backend, where nothing else provides Boost.

The benchmark also builds without `USE_PRIVATEER`. It then runs the default arm
alone, which is what keeps it compiling in a plain metall build.

## Run

```sh
BENCH=build/bench/segment_backend/run_segment_backend_bench \
ROOT=/scratch/bench OUT=results \
  bench/segment_backend/run_bench.sh --nodes 8000000 --updates 200000
```

Rules for numbers that mean something:

- One process per arm. `run_bench.sh` does that. `--backend both` exists for a
  smoke run only: the first arm leaves its datastore and its snapshots in the
  page cache, and the second arm pays for it. Measured cost of ignoring this:
  the privateer arm looked 2 times slower on checkpoints than it is.
- The datastore belongs on a real disk. On tmpfs every durability barrier is a
  no-op, and a checkpoint measures nothing. The results carry the file system
  name so a tmpfs run is visible afterwards.
- Nothing else on the machine. The reader threads and the writer are meant to
  compete with each other, not with a build.
- `--quick` is the smoke run. It finishes in seconds and its numbers are too
  small to compare.

## Phases

| Phase | What it measures |
|---|---|
| load | building the graph in a fresh datastore, then one checkpoint |
| read | root to leaf chases with the datastore still open |
| reopen | closing and opening again, then chasing: open cost and first touch reads |
| mixed | readers chasing while a writer path-copies and a collector frees, with a checkpoint every `--checkpoint-every` updates |
| retention | a series of snapshots, and deleting the oldest once the series is longer than `--retain` |

The read phase right after the load is not a steady state for the privateer
arm: the load's checkpoint writes the dirty blocks back and remaps them, so the
first read of a block faults it in from the page cache again. The phase after
the reopen is the comparable one.

## What the numbers mean

- `write_amplification` is the block bytes the backend wrote divided by the
  bytes the workload dirtied. Only a block-granular backend reports it. The
  dirty side counts the stores the benchmark makes itself, not the allocator's
  own metadata writes, so the ratio is an upper bound.
- `unique_allocated_bytes` in the retention phase counts every file once, even
  when several snapshots share it through a hard link. That is what makes a
  block store snapshot series comparable to a series of copies.
- `rss_peak_bytes` is the high water mark of the whole process, so it is a
  phase number only for the phase that reaches the peak first.
- `engine_counters` are the privateer counters behind
  `privateer::region::statistics()`: how many blocks were hashed, skipped as
  value-identical, deduplicated, and written.
