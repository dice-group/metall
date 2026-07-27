#!/usr/bin/env python3
"""Turns the JSON of segment backend runs into comparison tables.

Usage:
    summarize.py results/*.json
    summarize.py results/            # every .json in the directory

One row per arm. A file that holds two arms is a single-process run and is
marked, because such a run is a smoke test and not a comparison: the first arm
leaves its datastore in the page cache and the second one pays for it.
"""

import json
import sys
from pathlib import Path

MIB = 1024 * 1024


def load(paths):
    arms = []
    for path in sorted(paths):
        with open(path) as handle:
            run = json.load(handle)
        for arm in run.get("arms", []):
            arm["_file"] = Path(path).name
            arm["_shared_process"] = run.get("arms_in_one_process", False)
            arm["_config"] = run
            arms.append(arm)
    return arms


def row(values, widths):
    return "  ".join(str(value).ljust(width) for value, width in zip(values, widths))


def table(header, rows):
    widths = [max(len(str(cell)) for cell in column) for column in zip(header, *rows)] \
        if rows else [len(cell) for cell in header]
    print(row(header, widths))
    print("  ".join("-" * width for width in widths))
    for entry in rows:
        print(row(entry, widths))
    print()


def arm_name(arm):
    name = arm["backend"]
    if arm.get("block_size"):
        name += f"/{arm['block_size'] // MIB}M"
    return name


def summarize(arms):
    print("=== setup ===")
    rows = []
    for arm in arms:
        config = arm["_config"]
        rows.append([
            arm["_file"], arm_name(arm), arm.get("filesystem", "?"),
            "clone" if arm.get("file_clone") else "copy",
            config.get("nodes", "?"), config.get("updates", "?"),
            config.get("checkpoint_every", "?"),
            "SHARED PROCESS" if arm["_shared_process"] else "",
        ])
    table(["file", "arm", "fs", "clone", "nodes", "updates", "ckpt every", "note"], rows)

    print("=== load and read ===")
    rows = []
    for arm in arms:
        load_phase = arm.get("load", {})
        read = arm.get("read_after_reopen", {})
        rows.append([
            arm["_file"], arm_name(arm),
            f"{load_phase.get('seconds', 0):.2f}s",
            f"{load_phase.get('bytes_allocated', 0) / MIB:.0f}MiB",
            f"{load_phase.get('checkpoint_seconds', 0):.3f}s",
            f"{read.get('ops_per_second', 0) / 1e6:.2f}M/s",
            f"{read.get('p99_ns', 0) / 1000:.1f}us",
            f"{load_phase.get('rss_peak_bytes', 0) / MIB:.0f}MiB",
        ])
    table(["file", "arm", "load", "allocated", "checkpoint", "chases", "read p99",
           "peak rss"], rows)

    print("=== mixed: writer, readers, checkpoints ===")
    rows = []
    for arm in arms:
        mixed = arm.get("mixed")
        if not mixed:
            continue
        amplification = mixed.get("write_amplification")
        rows.append([
            arm["_file"], arm_name(arm),
            f"{mixed.get('updates_per_second', 0):.0f}",
            f"{mixed.get('reader_ops_per_second', 0) / 1e6:.2f}M/s",
            f"{mixed.get('checkpoint_p50_seconds', 0) * 1000:.1f}ms",
            f"{mixed.get('checkpoint_p99_seconds', 0) * 1000:.1f}ms",
            f"{mixed.get('dirty_bytes_total', 0) / MIB:.1f}MiB",
            f"{mixed.get('written_bytes_total', 0) / MIB:.0f}MiB" if amplification else "-",
            f"{amplification:.1f}" if amplification else "-",
        ])
    table(["file", "arm", "updates/s", "reader ops", "ckpt p50", "ckpt p99", "dirtied",
           "written", "amp"], rows)

    print("=== retention: cost of a snapshot, and of deleting one ===")
    rows = []
    for arm in arms:
        series = arm.get("retention") or []
        if not series:
            continue
        taken = [entry for entry in series if entry.get("taken")]
        deleted = [entry for entry in series if entry.get("deleted_oldest")]
        if not taken:
            continue
        mean_seconds = sum(entry["snapshot_seconds"] for entry in taken) / len(taken)
        mean_cost = sum(entry["filesystem_cost_bytes"] for entry in taken) / len(taken)
        rows.append([
            arm["_file"], arm_name(arm), arm.get("filesystem", "?"),
            "clone" if arm.get("file_clone") else "copy",
            len(taken),
            f"{mean_seconds:.3f}s",
            f"{mean_cost / MIB:.1f}MiB",
            f"{sum(entry['delete_seconds'] for entry in deleted) / len(deleted):.3f}s"
            if deleted else "-",
            f"{sum(entry['delete_freed_bytes'] for entry in deleted) / len(deleted) / MIB:.1f}MiB"
            if deleted else "-",
        ])
    table(["file", "arm", "fs", "clone", "snapshots", "per snapshot", "fs cost",
           "delete", "freed"], rows)


def main():
    arguments = sys.argv[1:]
    if not arguments:
        print(__doc__)
        return 1
    paths = []
    for argument in arguments:
        path = Path(argument)
        paths.extend(sorted(path.glob("*.json")) if path.is_dir() else [path])
    if not paths:
        print("no JSON files given")
        return 1
    summarize(load(paths))
    return 0


if __name__ == "__main__":
    sys.exit(main())
