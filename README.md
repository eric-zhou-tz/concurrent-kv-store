# Concurrent KV Store in Modern C++

A key-value store built from first principles in modern C++, with WAL
persistence, snapshot recovery, correctness-first concurrency, benchmarking,
and an eventual storage-engine architecture.

The current implementation is intentionally small and inspectable: a
single-process CLI routes parsed commands into an in-memory `KVStore`, while
the persistence layer records durable mutations and restores state from
snapshots plus WAL tail replay.

Current release status: `v0.5.0` adds coarse-grained reader/writer
synchronization with `std::shared_mutex`. Concurrent readers can proceed
together, while writes, snapshots, compaction, recovery, clear, and persistence
reset are exclusive. The CLI reports this through `INFO`, `VERSION`, or
`STATUS`.

## Table of Contents

- [Performance Highlights](#performance-highlights)
- [Current Status](#current-status)
- [Architecture](#architecture)
- [Features](#features)
- [Quick Start](#quick-start)
- [Repository Tour](#repository-tour)
- [Benchmarks](#benchmarks)
- [Engineering Notes](#engineering-notes)

## Performance Highlights

Latest focused EC2 Release run: AWS `c7i-flex.large`, Ubuntu Linux, Intel Xeon
Platinum 8488C, GCC/G++ 15.2.0, CMake Release with `-O3 -DNDEBUG`, five Google
Benchmark repetitions. This KV-store run was not CPU-pinned, and the numbers
below should not be compared directly to matching-engine results because the
workloads are different.

| Workload | What It Measures | Count / Size | Throughput / Latency |
| --- | --- | ---: | ---: |
| Mixed 70/30 read-write | Direct in-memory `KVStore` flow | 1,000-key working set | `55.09M ops/sec` |
| Get | Successful in-memory lookup after preload | 1,000-key preload | `48.05M ops/sec`, `20.8 ns/op` |
| Delete | In-memory erase after deterministic preload | 1,000 deletes/batch | `29.66M ops/sec` |
| Set | In-memory insert/overwrite | 1,000 writes/batch | `17.11M ops/sec` |
| Durable Set | WAL-backed `Set` with flush behavior | 1,000 writes/batch | `1.66M ops/sec` |
| WAL replay | Checksum-framed WAL recovery path | 10,000 records | `3.75 ms`, `2.67M records/sec` |
| Snapshot load | Full snapshot restore into memory | 10,000 entries | `1.21 ms`, `8.28M entries/sec` |
| Snapshot + WAL-tail recovery | Snapshot-assisted recovery path | 10,000 base entries + 10% tail | `1.72 ms`, `6.38M entries/sec` |

See [docs/Benchmarks.md](docs/Benchmarks.md) for methodology, caveats, raw
artifact paths, and benchmark history.

## Current Status

| Area | Current Behavior |
| --- | --- |
| Version | `v0.5.0` |
| Concurrency | Coarse `std::shared_mutex`: concurrent reads, serialized writes and durability operations |
| Durability | WAL append happens before in-memory mutation; snapshots and compaction are exclusive |
| Recovery | Startup loads snapshot first, then replays the checksum-verified WAL tail |
| CLI | `INFO`, `VERSION`, and `STATUS` print version, entry count, concurrency model, and durability notes |
| Validation | Latest local validation: Release build plus `85/85` CTest cases passing |
| Benchmarks | Published EC2 numbers are the pre-concurrency baseline; no official contention rows yet |

## Architecture

```text
Command text -> CliParser -> CliServer -> KVStore -> WAL + Snapshot
```

`KVStore` owns the live map and exposes the core `Set`, `Get`, and `Delete`
API. `WriteAheadLog` stores ordered mutation records before in-memory mutation.
`Snapshot` stores full point-in-time materialized state and records the WAL byte
offset covered by the checkpoint.

Additional docs:

- [Architecture](docs/Architecture.md)
- [Benchmarks](docs/Benchmarks.md)
- [Benchmark History](docs/Benchmark_History.md)
- [Changelog](docs/CHANGELOG.md)
- [Roadmap](docs/Roadmap.md)

## Features

- Modern C++20 build through CMake
- In-memory `SET`, `GET`, and `DELETE`
- Overwrite and missing-key semantics
- Binary append-only WAL with CRC32 payload checksums
- Corruption-aware WAL replay with safe truncation to the last valid record
- Verified full-state snapshot checkpoints with WAL rotation compaction
- Startup recovery from snapshot plus checksum-verified WAL tail
- Coarse-grained reader/writer synchronization for concurrent readers and
  serialized writes
- Interactive CLI with `INFO`/`VERSION`/`STATUS`
- GoogleTest coverage for storage and persistence behavior
- Google Benchmark hot-path benchmarks

## Quick Start

### Prerequisites

- CMake 3.20 or newer
- C++20 compiler such as GCC, Clang, or Apple Clang
- Network access during first configure so CMake can fetch GoogleTest and
  Google Benchmark

### Native Release Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run the CLI:

```bash
./build/kv_store
```

Example session:

```text
Concurrent KV Store v0.5.0
Concurrency: coarse shared_mutex: concurrent reads, serialized writes and durability
Loading snapshot...
Loaded 0 snapshot entrie(s)
Replaying WAL...
Recovered 0 operation(s)
kv-store> INFO
Concurrent KV Store v0.5.0
entries: 0
concurrency: coarse shared_mutex: concurrent reads, serialized writes and durability
durability: WAL appends are serialized before memory mutation; snapshot, compaction, and recovery are exclusive
kv-store> SET language cpp
OK
kv-store> GET language
cpp
kv-store> DELETE language
1
kv-store> GET language
(nil)
kv-store> EXIT
Bye
```

### Tests

```bash
ctest --test-dir build --output-on-failure -C Release
```

Focused targets:

```bash
cmake --build build --target kv_store_tests
./build/kv_store_tests

cmake --build build --target kv_store_stress_tests
./build/kv_store_stress_tests
```

ThreadSanitizer validation is opt-in and intended for Linux Clang/GCC Debug
builds:

```bash
cmake -S . -B build-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCONCURRENT_KV_STORE_ENABLE_TSAN=ON \
  -DCONCURRENT_KV_STORE_BUILD_TESTS=ON \
  -DCONCURRENT_KV_STORE_BUILD_BENCHMARKS=OFF
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

### Benchmarks

```bash
cmake --build build --target kv_store_benchmark
./build/kv_store_benchmark
./build/kv_store_benchmark --benchmark_filter=BM_Get
```

See [docs/Benchmarks.md](docs/Benchmarks.md) for methodology and result
tables.

### Benchmarking on EC2

Publication benchmark runs should happen on the target EC2 instance, not on a
local development machine. Use the existing EC2 host at public IPv4
`3.20.238.237`:

```bash
ssh ubuntu@3.20.238.237
cd ~/concurrent-kv-store
git pull
chmod +x scripts/run_ec2_benchmarks.sh
./scripts/run_ec2_benchmarks.sh
```

The script writes raw text, JSON, and metadata files under
`benchmark_results/`. Summarize those results manually in
[docs/Benchmarks.md](docs/Benchmarks.md) and
[docs/Benchmark_History.md](docs/Benchmark_History.md).

## Repository Tour

```text
include/        Public headers for store, persistence, parser, and CLI server
src/            Implementation files
tests/          GoogleTest unit, integration, and stress suites
benchmarks/     Google Benchmark hot-path benchmarks
docs/           Architecture, benchmark, changelog, and roadmap notes
scripts/        CMake convenience scripts
```

## Benchmarks

The benchmark suite currently covers:

| Benchmark | What It Measures |
| --- | --- |
| `BM_Put` | In-memory insert/overwrite path |
| `BM_Get` | Successful in-memory lookup path |
| `BM_Delete` | Delete path after deterministic preload |
| `BM_MixedReadWrite70_30` | Deterministic 70% read / 30% write flow |
| `BM_DurableSetWithWalFlush` | WAL-backed Set path |
| `BM_SnapshotSave` | Full snapshot write and verification |
| `BM_SnapshotLoad` | Snapshot load into memory |
| `BM_WalReplay` | Replay checksum-framed WAL records |
| `BM_RecoveryFromSnapshotAndWalTail` | Snapshot load plus WAL tail replay |
| `BM_RecoveryFromCompactedSnapshotAndWalTail` | Snapshot plus rotated WAL recovery |
| `BM_SnapshotCompaction` | Snapshot verification and WAL rotation |

Latest EC2 results, methodology, caveats, and raw artifact paths are documented
in [docs/Benchmarks.md](docs/Benchmarks.md). Benchmark results are
machine-specific; record compiler, build type, CPU, OS, commit, and command line
with every published run. The current published EC2 baseline predates the
`v0.5.0` lock insertion, so it should be treated as a pre-concurrency baseline
until a clean refresh is published.

## Engineering Notes

- The storage core uses a correctness-first `std::shared_mutex` model:
  concurrent readers can proceed together, while writes, snapshots,
  compaction, recovery, clear, and persistence reset are exclusive.
- WAL appends remain serialized through `KVStore`, preserving the existing
  write-ahead durability contract under concurrent callers.
- Sharing one WAL or Snapshot object across multiple stores or using it
  directly from another thread is outside the synchronization contract.
- WAL records are length-framed, checksum-protected, and bounded to avoid
  unbounded allocation while recovering corrupted files.
- WAL replay applies only complete checksum-verified records. It stops at the
  first untrusted frame and can truncate a corrupted crash tail to the last
  known-good byte offset.
- `COMPACT`/`SNAPSHOT` writes and verifies a full snapshot before rotating the
  WAL to an empty log. If snapshot writing or verification fails, WAL history
  remains untouched.
- Snapshots duplicate full state by design. Incremental checkpoints are future
  storage-engine work.
- Current concurrency is coarse-grained, not sharded or lock-free. Future
  performance work may add sharded maps, per-shard locks, a single WAL writer,
  batched flush/group commit, segmented WAL, or an LSM/memtable-style engine.
- The CLI is an integration boundary, not the storage API. Tests and benchmarks
  exercise `KVStore` and persistence components directly where possible.
