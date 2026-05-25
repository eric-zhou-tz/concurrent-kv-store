# Concurrent KV Store in Modern C++

A key-value store built from first principles in modern C++, with WAL
persistence, snapshot recovery, benchmarking, and an eventual concurrent
storage-engine architecture.

The current implementation is intentionally small and inspectable: a
single-process CLI routes parsed commands into an in-memory `KVStore`, while
the persistence layer records durable mutations and restores state from
snapshots plus WAL tail replay.

## Table of Contents

- [Architecture](#architecture)
- [Features](#features)
- [Quick Start](#quick-start)
- [Repository Tour](#repository-tour)
- [Benchmarks](#benchmarks)
- [Engineering Notes](#engineering-notes)

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
- Interactive CLI
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

### Benchmarks

```bash
cmake --build build --target kv_store_benchmark
./build/kv_store_benchmark
./build/kv_store_benchmark --benchmark_filter=BM_Get
```

See [docs/Benchmarks.md](docs/Benchmarks.md) for methodology and result
templates.

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

Benchmark results are machine-specific. Record compiler, build type, CPU, OS,
commit, and command line with every published run.

## Engineering Notes

- The storage core is deliberately single-threaded today. Concurrency will be
  added after the single-writer persistence contract stays stable under tests.
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
- The CLI is an integration boundary, not the storage API. Tests and benchmarks
  exercise `KVStore` and persistence components directly where possible.
