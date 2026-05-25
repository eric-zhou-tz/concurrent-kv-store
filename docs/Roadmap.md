# Roadmap

## 0.3.x - Modern Project Foundation

- CMake-first C++20 build
- GoogleTest suites wired through `ctest`
- Google Benchmark hot-path suite
- Documentation under `docs/`

## 0.4.x - Storage Safety

- WAL checksums
- Verified snapshot compaction with WAL rotation
- EC2 benchmark workflow
- First Release EC2 benchmark baseline
- README performance highlights and benchmark history publication

## 0.5.x - Correctness-First Concurrency

- Reader/writer synchronization with `std::shared_mutex`
- Single-writer durability contract through serialized `KVStore` writes
- Concurrent read/write, snapshot, compaction, and WAL recovery tests
- Deterministic concurrency stress run
- ThreadSanitizer validation target
- CLI startup/help/status text for the current concurrency contract
- `INFO`/`VERSION`/`STATUS` command aliases

## 0.6.x - Durability Tuning

- Add CTest execution and pass-count capture to the EC2 benchmark script
- Rerun benchmark baseline from a clean committed tree
- CLI/public-boundary benchmark coverage
- Publish post-0.5 benchmark baseline with lock overhead called out
- Add read/write contention benchmark rows
- Configurable durability policies
- Explicit endianness for binary formats
- More corruption and recovery fixtures

## 0.7.x - Storage Engine Evolution

- Segmented WAL files
- Optional WAL prefix truncation or multi-generation WAL metadata
- Optional memory-mapped or log-structured storage experiments
- Sharded maps with per-shard locks
- Single WAL writer queue, batched WAL flush, or group commit
- Read/write latency benchmarks under contention
- Eventual LSM/memtable-style storage-engine design

## 1.0.0 - Stable Local KV Store

- Documented persistence format
- Repeatable benchmark baseline
- Stable CLI and C++ API
- Clear operational recovery behavior
