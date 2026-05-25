# Benchmarks

## Overview

Benchmarks use Google Benchmark and target the same production storage
components used by the CLI. Hot-path benchmarks isolate the `KVStore` API where
possible, while recovery benchmarks include snapshot load plus WAL tail replay.

Build in Release mode before recording numbers:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kv_store_benchmark
```

Run the full suite:

```bash
./build/kv_store_benchmark
```

Run one benchmark family:

```bash
./build/kv_store_benchmark --benchmark_filter=BM_MixedReadWrite70_30
```

Export JSON:

```bash
./build/kv_store_benchmark --benchmark_format=json > benchmarks/results/kv_store_results.json
```

Create `benchmarks/results/` locally when saving artifacts. Result files are
ignored by default unless a future release intentionally checks in a curated
baseline.

## Current Benchmark Targets

| Benchmark | What It Measures | Notes |
| --- | --- | --- |
| `BM_Put` | In-memory inserts/overwrites | Constructs a fresh store per iteration. |
| `BM_Get` | Successful lookups | Preloads deterministic keys before timing. |
| `BM_Delete` | Delete throughput | Preload is outside timed region. |
| `BM_MixedReadWrite70_30` | 70% read / 30% write flow | Deterministic bounded keyspace. |
| `BM_RecoveryFromSnapshotAndWalTail` | Snapshot load plus WAL tail replay | Uses an uncompacted WAL with covered offset replay. |
| `BM_RecoveryFromCompactedSnapshotAndWalTail` | Snapshot plus rotated WAL tail recovery | Models the normal compacted recovery path. |
| `BM_SnapshotCompaction` | Snapshot verification plus WAL rotation | Times full-state snapshot lifecycle cleanup. |

## Environment Template

Record this metadata with every official run:

```text
Date:
Commit:
Build type:
Compiler:
CMake:
OS:
CPU:
Memory:
Command:
Notes:
```

## Results Template

| Benchmark | Count | Time | CPU | Throughput |
| --- | ---: | ---: | ---: | ---: |
| `BM_Put/1000` | TBD | TBD | TBD | TBD |
| `BM_Get/1000` | TBD | TBD | TBD | TBD |
| `BM_Delete/1000` | TBD | TBD | TBD | TBD |
| `BM_MixedReadWrite70_30/1000` | TBD | TBD | TBD | TBD |
| `BM_RecoveryFromSnapshotAndWalTail/1000` | TBD | TBD | TBD | TBD |
| `BM_RecoveryFromCompactedSnapshotAndWalTail/1000` | TBD | TBD | TBD | TBD |
| `BM_SnapshotCompaction/1000` | TBD | TBD | TBD | TBD |

## Interpretation Notes

- `BM_Put` without WAL measures the in-memory storage path, not durable write
  latency.
- Recovery benchmarks include filesystem work and should be compared only
  across similar storage media.
- Compacted recovery should replay less WAL than full WAL recovery, while
  snapshot compaction pays the cost of writing and verifying the full map.
- Benchmarks are deterministic but not isolated from OS scheduling. Use a quiet
  machine and Release builds for publishable numbers.
- Future durable write benchmarks should separate buffered append cost from
  flush policy cost.
