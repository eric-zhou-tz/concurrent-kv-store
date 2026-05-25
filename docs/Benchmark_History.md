# Benchmark History

This file records curated benchmark runs worth preserving. Raw Google Benchmark
JSON/text artifacts stay outside git; published rows here should be copied from
intentional EC2 runs with environment metadata.

Current project status: `v0.5.0` has added coarse reader/writer synchronization
and CLI status output. The only published EC2 run below predates that change,
so it is a pre-concurrency baseline and should not be used to claim Phase 0.5
performance.

## 2026-05-25T08:35:47Z - First EC2 Baseline

| Field | Value |
| --- | --- |
| Date | `2026-05-25T08:35:47Z` |
| Commit recorded by metadata | `c9fb546a6f9d720d5184bc657fcbdd65096bd16b` |
| Published commit containing benchmark workflow changes | `d7adcb4` |
| Branch | `main` |
| EC2 public IPv4 | `3.20.238.237` |
| Instance type | AWS EC2 `c7i-flex.large` |
| Hostname | `ip-172-31-44-213` |
| CPU | Intel Xeon Platinum 8488C |
| vCPU | 2 |
| Memory | 3.7 GiB |
| OS | Ubuntu 26.04 LTS, Linux `7.0.0-1004-aws` |
| Compiler | GCC/G++ 15.2.0 |
| CMake | 4.2.3 |
| Build flags | `-DCMAKE_BUILD_TYPE=Release`; EC2 cache recorded `-O3 -DNDEBUG` |
| Benchmark script | `scripts/run_ec2_benchmarks.sh` |
| Raw result path | `/home/ubuntu/concurrent-kv-store/benchmark_results/20260525_083522/` |
| Google Benchmark settings | 5 repetitions, aggregate rows only, JSON output |
| CTest before benchmark | Not recorded by the benchmark script |

Summary:

| Benchmark | Input Size | Mean Time | Throughput |
| --- | ---: | ---: | ---: |
| Mixed 70/30 read-write | 1,000-key working set | `18.2 ns/op` | `55.09M ops/sec` |
| Get | 1,000-key preload | `20.8 ns/op` | `48.05M ops/sec` |
| Set | 1,000 writes/batch | `58.5 us/batch` | `17.11M ops/sec` |
| Delete | 1,000 deletes/batch | `33.7 us/batch` | `29.66M ops/sec` |
| Durable Set with WAL flush | 1,000 writes/batch | `603.2 us/batch` | `1.66M ops/sec` |
| WAL replay | 10,000 records | `3.75 ms` | `2.67M records/sec` |
| Snapshot load | 10,000 entries | `1.21 ms` | `8.28M entries/sec` |
| Snapshot + WAL-tail recovery | 10,000 base entries + 10% tail | `1.72 ms` | `6.38M entries/sec` |
| Snapshot compaction | 10,000 entries | `2.46 ms` | `4.07M entries/sec` |

Improvements/regressions:

- First official EC2 KV-store baseline; no prior curated EC2 KV benchmark
  history exists for apples-to-apples comparison.

Caveats:

- The run was not CPU-pinned.
- The benchmark script did not run or record CTest before executing benchmarks.
- Metadata recorded a dirty working tree. The benchmark workflow/source changes
  were committed afterward as `d7adcb4`; rerun from a clean commit if strict
  release provenance is required.
- Filesystem-sensitive rows may vary with EC2 storage/cache state.
- This run predates the Phase 0.5 `std::shared_mutex` lock insertion and CLI
  status updates.

Next steps:

- Add CTest execution and pass-count capture to the EC2 runner.
- Rerun from a clean commit for a stricter publication baseline.
- Add CLI/public-boundary benchmarks for parser/server/output formatting cost.
- Add read-only and mixed read/write contention rows for the coarse
  reader/writer-lock implementation.

## Entry Template

Copy this block when publishing a new EC2 benchmark run:

```text
Date:
Commit:
Branch:
EC2 public IPv4: 3.20.238.237
Instance type:
CPU:
vCPU:
Memory:
OS:
Compiler:
CMake:
Build flags:
Benchmark script:
Raw result path:
CTest before benchmark:
Summary:
Improvements:
Regressions:
Caveats:
Concurrency model:
CLI status surface:
Next steps:
```

## Publication Checklist

1. Run correctness tests before the benchmark, then record the pass count.
2. Run the EC2 workflow from the repository root:

   ```bash
   ssh ubuntu@3.20.238.237
   cd ~/concurrent-kv-store
   git pull
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ctest --test-dir build --output-on-failure -C Release
   chmod +x scripts/run_ec2_benchmarks.sh
   ./scripts/run_ec2_benchmarks.sh
   ```

3. Copy environment metadata from
   `benchmark_results/<YYYYMMDD_HHMMSS>/metadata.txt` into
   `docs/Benchmarks.md`.
4. Summarize aggregate Google Benchmark rows from
   `benchmark_results/<YYYYMMDD_HHMMSS>/benchmarks.json`.
5. Add one dated entry here with the commit, EC2 instance type, build flags,
   high-level summary, notable improvements/regressions, and raw artifact path.
6. Commit only curated docs updates unless a raw artifact is intentionally
   archived elsewhere.
