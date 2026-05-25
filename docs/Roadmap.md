# Roadmap

## 0.3.x - Modern Project Foundation

- CMake-first C++20 build
- GoogleTest suites wired through `ctest`
- Google Benchmark hot-path suite
- Documentation under `docs/`

## 0.4.x - Storage Safety

- WAL checksums
- Explicit endianness for binary formats
- More corruption and recovery fixtures
- Configurable durability policies

## 0.5.x - Concurrency

- Reader/writer synchronization
- Single-writer durability contract
- Concurrent read tests and stress runs
- Thread sanitizer validation target

## 0.6.x - Storage Engine Evolution

- Segmented WAL files
- Snapshot compaction
- Log truncation after covered checkpoints
- Optional memory-mapped or log-structured storage experiments

## 1.0.0 - Stable Local KV Store

- Documented persistence format
- Repeatable benchmark baseline
- Stable CLI and C++ API
- Clear operational recovery behavior
