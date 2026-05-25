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

## 0.5.x - Durability Tuning

- Add CTest execution and pass-count capture to the EC2 benchmark script
- Rerun benchmark baseline from a clean committed tree
- CLI/public-boundary benchmark coverage
- Configurable durability policies
- Explicit endianness for binary formats
- More corruption and recovery fixtures

## 0.6.x - Concurrency

- Reader/writer synchronization
- Single-writer durability contract
- Concurrent read tests and stress runs
- Thread sanitizer validation target

## 0.7.x - Storage Engine Evolution

- Segmented WAL files
- Optional WAL prefix truncation or multi-generation WAL metadata
- Optional memory-mapped or log-structured storage experiments

## 1.0.0 - Stable Local KV Store

- Documented persistence format
- Repeatable benchmark baseline
- Stable CLI and C++ API
- Clear operational recovery behavior
