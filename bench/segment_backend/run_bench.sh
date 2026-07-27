#!/bin/bash
# Runs the segment backend comparison the way its numbers can be read: one
# process per arm. Two arms in one process are not comparable, because the
# first arm leaves its datastore and its snapshots in the page cache and its
# resident high water mark in the process, and the second arm pays for both.
#
# Environment:
#   BENCH        path of the benchmark binary
#   ROOT         where the datastores go, on a real disk, never tmpfs
#   OUT          directory for the JSON results
#   BLOCK_SIZES  optional list, runs the privateer arm once per size
# Everything else is passed through to the benchmark.

set -eu

BENCH=${BENCH:-./run_segment_backend_bench}
ROOT=${ROOT:-./segment_backend_bench}
OUT=${OUT:-./segment_backend_results}
BLOCK_SIZES=${BLOCK_SIZES:-}

mkdir -p "${OUT}"

run_arm() {
  local name=$1
  shift
  echo "== ${name}"
  "${BENCH}" --root "${ROOT}" --json "${OUT}/${name}.json" "$@"
  sync
  rm -rf "${ROOT}"
}

run_arm default --backend default "$@"

if [ -z "${BLOCK_SIZES}" ]; then
  run_arm privateer --backend privateer "$@"
else
  for size in ${BLOCK_SIZES}; do
    run_arm "privateer-${size}" --backend privateer --block-size "${size}" "$@"
  done
fi

echo "results in ${OUT}"
