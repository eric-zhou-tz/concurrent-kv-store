# Changelog

All notable changes to this project will be documented in this file.

## v0.4.0 -> Snapshot Compaction and WAL Rotation

- Added verified snapshot compaction with WAL rotation. `COMPACT`/`SNAPSHOT`
  writes a full snapshot, verifies the committed file can be loaded and matches
  in-memory state, then rotates the WAL to an empty log.
- Preserved recovery correctness after compaction: startup loads the compacted
  snapshot and replays the current WAL tail from offset zero.
- Added failure-safety coverage for snapshot write failure, crash before WAL
  rotation, missing WAL after a valid snapshot, corrupted snapshots with valid
  WAL fallback, and repeated compaction idempotence.
- Added benchmark coverage for compacted snapshot recovery and snapshot
  compaction cost.

## v0.3.0 -> Modern CMake + C++20 Project Foundation

- Added WAL v2 records with CRC32 checksums over payload bytes. Replay now
  applies only complete checksum-verified records and reports detailed stop
  statuses for clean EOF, torn records, invalid lengths, bad opcodes, checksum
  mismatches, partial payloads, and invalid payloads.
- Added WAL truncation to the last known-good offset after corrupted crash-tail
  replay, with deterministic tests for torn writes, oversized lengths, bad
  opcodes, partial key/value payloads, checksum mismatch, safe truncate, and
  snapshot-plus-WAL-tail recovery.
- Renamed the active project identity to `concurrent-kv-store` and rewrote the README around the plain KV store purpose: modern C++, WAL persistence, snapshot recovery, benchmarking, and eventual concurrent storage-engine work.
- Replaced the legacy Makefile build with a CMake 3.20 project using C++20, a reusable `kv_store_core` library, a `kv_store` CLI executable, discovered GoogleTest targets, and a Google Benchmark executable.
- Added CMake `FetchContent` dependencies for GoogleTest `v1.14.0` and Google Benchmark `v1.8.3`, keeping external dependencies explicit and limited to test/benchmark tooling.
- Replaced the custom benchmark harness under `bench/` with Google Benchmark sources under `benchmarks/core_hot_path/`, covering `Put`, `Get`, `Delete`, deterministic 70/30 read-write flow, and snapshot-plus-WAL-tail recovery.
- Added documentation under `docs/` for architecture, benchmark methodology, benchmark history, roadmap, and changelog so root-level files stay focused on onboarding.
- Updated Docker and helper scripts to use the CMake Release build path and `ctest` validation.
- Isolated the imported matching-engine reference zip under ignored `reference/` so it remains available locally without becoming product source.
- Verified the refactor with a clean Release configure/build, 58/58 CTest cases passing, a CLI smoke test, and Google Benchmark smoke runs.

## v0.2.1 -> Core KV Store Baseline Reset

- Removed the previous AI/agent JSON action-frame layer so the repository returns to a focused base key-value store.
- Removed the dormant `--json` validation path, action validation code, JSON enforcer tests, tracked runtime WAL artifact, and agent-specific architecture graphic.
- Rewrote the README to describe only the KV store, persistence model, CLI commands, tests, and benchmarks.
- Added ignored runtime durability files for `kv_store.wal` and `kv_store.snapshot`.
- Verified the simplified store with the full unit/integration suite, stress suite, CLI smoke test, and whitespace diff check.

## v0.2.0 -> WAL Persistence and Snapshot Recovery

- Added append-only binary write-ahead logging for `SET` and `DELETE` records.
- Integrated the WAL into the store write path so mutations are logged before in-memory state changes.
- Added startup recovery by replaying WAL records into a fresh in-memory store.
- Added snapshot persistence with format magic, version, covered WAL byte offset, and full key/value state.
- Added snapshot-aware recovery so startup loads the latest snapshot first and replays only WAL records after the covered offset.
- Added `CLEAR PERSISTENCE` for removing durable files while preserving live in-memory state.
- Added `KVStore::SaveSnapshot()` as the public checkpoint API.
- Expanded GoogleTest coverage for core KV behavior, WAL replay, malformed WAL records, snapshot save/load, corrupted snapshot handling, recovery flows, and stress workloads.

## v0.1.0 -> Initial In-Memory KV Store

- Added the first single-threaded in-memory key-value store backed by `std::unordered_map`.
- Implemented `SET`, `GET`, and `DELETE` semantics including overwrite, missing-key, empty-key, and empty-value behavior.
- Added an interactive CLI loop with parser and server boundaries.
- Separated public headers and implementation files across `include/` and `src/`.
- Added the first core storage tests for basic correctness.
