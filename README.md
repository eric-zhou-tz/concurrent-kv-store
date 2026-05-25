# KV Store

A small C++17 key-value store with an in-memory core, an interactive CLI,
append-only write-ahead logging, snapshots, and startup recovery.

## Features

- `SET`, `GET`, and `DELETE` operations
- Single-threaded in-memory storage built on `std::unordered_map`
- Binary write-ahead log for durable mutations
- Snapshot files with covered WAL offsets
- Startup recovery from snapshot plus WAL tail
- CLI interface for local use
- GoogleTest unit, integration, and stress tests
- Benchmark harness for write, read, mixed, recovery, and snapshot workloads

## Architecture

```text
CLI input
   |
   v
CliParser
   |
   v
CliServer
   |
   v
KVStore
   |
   v
WriteAheadLog + Snapshot
```

The storage API stays small: the store owns in-memory state, while persistence
components record mutations and provide recovery data. Mutating commands append
to the WAL before updating memory.

## Build

```bash
make
```

Run the CLI:

```bash
./bin/kv_store
```

Example:

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

## CLI Commands

- `SET <key> <value>`
- `GET <key>`
- `DEL <key>` or `DELETE <key>`
- `CLEAR PERSISTENCE`
- `HELP`
- `EXIT` or `QUIT`

## Persistence

By default the application stores durability files in the current working
directory:

- `kv_store.wal`
- `kv_store.snapshot`

Recovery loads the snapshot first when one exists, then replays only WAL records
written after the snapshot's covered byte offset. If no snapshot exists, recovery
replays the WAL from the beginning.

The WAL and snapshot readers bound record and field sizes so corrupted files do
not trigger unbounded allocations. Partial trailing WAL records are treated as
the result of an interrupted write and are ignored after earlier valid records
are applied.

## Tests

```bash
make test
make test_verbose
make test_stress
```

If GoogleTest is not installed, vendor it locally:

```bash
./scripts/bootstrap_gtest.sh
```

## Benchmarks

```bash
make benchmark
./benchmark
./benchmark 100000
```

The baseline report lives in `benchmark.md`. Treat checked-in benchmark numbers
as local reference data and rerun them on the target machine before publishing
new results.

## Repository Layout

```text
include/store/        Public KV store interface
include/persistence/  WAL and snapshot interfaces
include/parser/       CLI command parser
include/server/       CLI server interface
src/store/            KV store implementation
src/persistence/      Durable storage implementation
src/parser/           CLI parser implementation
src/server/           CLI loop and command dispatch
tests/                GoogleTest suites
bench/                Benchmark harness
scripts/              Build and run helpers
```
