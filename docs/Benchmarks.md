# Benchmarks

## Executive Summary

Latest official EC2 benchmark baseline: `2026-05-25T08:35:47Z` on AWS
`c7i-flex.large`.

These are Linux Release numbers from Google Benchmark with five repetitions and
aggregate reporting enabled. The run was not CPU-pinned, and correctness tests
were not recorded as part of the benchmark script. Raw artifacts remain outside
git under the EC2 checkout's `benchmark_results/20260525_083522/` directory.

| Item | Latest EC2 Run |
| --- | --- |
| Public IPv4 | `3.20.238.237` |
| Hostname | `ip-172-31-44-213` |
| Instance | AWS EC2 `c7i-flex.large` |
| CPU | Intel Xeon Platinum 8488C |
| vCPU | 2 |
| Memory | 3.7 GiB |
| OS | Ubuntu 26.04 LTS, Linux `7.0.0-1004-aws` |
| Compiler | GCC/G++ 15.2.0 |
| CMake | 4.2.3 |
| Build | Release, `-DCMAKE_BUILD_TYPE=Release` |
| Release flags | `-O3 -DNDEBUG` |
| CPU pinning | Not used / not recorded |
| Benchmark framework | Google Benchmark 1.8.3 |
| Repetitions | 5, aggregate rows only |
| CTest before benchmark | Not recorded by the benchmark script |
| Recorded commit | `c9fb546a6f9d720d5184bc657fcbdd65096bd16b` with dirty benchmark/doc changes |
| Published commit containing those changes | `d7adcb4` |

Headline metrics:

| Highlight | Metric |
| --- | ---: |
| Mixed 70/30 in-memory flow, 1,000-key working set | `55.09M ops/sec` |
| Successful Get, 1,000-key preload | `48.05M ops/sec` |
| Delete after preload, 1,000-key batches | `29.66M ops/sec` |
| In-memory Set, 1,000-key batches | `17.11M ops/sec` |
| Durable WAL-backed Set, 1,000-key batches | `1.66M ops/sec` |
| WAL replay, 10,000 records | `3.75 ms`, `2.67M records/sec` |
| Snapshot load, 10,000 entries | `1.21 ms`, `8.28M entries/sec` |
| Snapshot + WAL-tail recovery, 10,000 base entries | `1.72 ms`, `6.38M entries/sec` |

The suite separates in-memory storage cost from durability and recovery cost.
Hot-path rows exercise `KVStore` directly. Durability rows include WAL or
snapshot filesystem work by design.

## Benchmark Methodology

Publication runs use the EC2 script:

```bash
ssh ubuntu@3.20.238.237
cd ~/concurrent-kv-store
git pull
chmod +x scripts/run_ec2_benchmarks.sh
./scripts/run_ec2_benchmarks.sh
```

The script configures a clean build directory, builds `kv_store_benchmark`, and
writes text, JSON, and metadata artifacts:

```text
benchmark_results/20260525_083522/metadata.txt
benchmark_results/20260525_083522/benchmarks.txt
benchmark_results/20260525_083522/benchmarks.json
```

Controls and boundaries:

| Control | Practice |
| --- | --- |
| Build mode | CMake Release with `-O3 -DNDEBUG` from the EC2 CMake cache |
| Benchmark command | `--benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_out_format=json` |
| Workload repeatability | Deterministic key/value generation and fixed operation patterns |
| Timing boundaries | Preload/setup is outside timed loops where the benchmark is measuring operation cost; persistence benchmarks include the filesystem work they are named for |
| I/O | No filesystem I/O in in-memory hot-path rows; WAL/snapshot/recovery rows explicitly measure persistence I/O |
| Compiler barriers | Benchmarks use `benchmark::DoNotOptimize(...)` and `benchmark::ClobberMemory()` where needed |
| Validation | CTest status before this EC2 benchmark run was not recorded; rerun tests before future publication refreshes |
| Raw artifacts | Kept out of git; curated tables live in this document and `docs/Benchmark_History.md` |

## In-Memory Hot Path

These rows measure the storage API directly without CLI parsing, formatting, or
filesystem persistence.

| Benchmark | Description | Input Size | Mean Time | Throughput |
| --- | --- | ---: | ---: | ---: |
| `BM_Get/1000` | Successful lookup after preload | 1,000-key preload | `20.8 ns/op` | `48.05M ops/sec` |
| `BM_Get/10000` | Successful lookup after preload | 10,000-key preload | `26.3 ns/op` | `38.03M ops/sec` |
| `BM_Put/1000` | Insert/overwrite into a fresh in-memory store | 1,000 writes/batch | `58.5 us/batch` | `17.11M ops/sec` |
| `BM_Put/10000` | Insert/overwrite into a fresh in-memory store | 10,000 writes/batch | `695.0 us/batch` | `14.39M ops/sec` |
| `BM_Delete/1000` | Delete after deterministic preload | 1,000 deletes/batch | `33.7 us/batch` | `29.66M ops/sec` |
| `BM_Delete/10000` | Delete after deterministic preload | 10,000 deletes/batch | `389.4 us/batch` | `25.69M ops/sec` |
| `BM_MixedReadWrite70_30/1000` | 70% Get / 30% Set against a bounded keyspace | 1,000-key working set | `18.2 ns/op` | `55.09M ops/sec` |
| `BM_MixedReadWrite70_30/10000` | 70% Get / 30% Set against a bounded keyspace | 10,000-key working set | `25.4 ns/op` | `39.40M ops/sec` |

Engineering notes:

| Observation | Why It Matters |
| --- | --- |
| Hot-path rows isolate `KVStore` from persistence. | They show hash-map storage cost, not durable write cost. |
| Set/Delete rows are batch-shaped. | The reported throughput is item throughput across the batch; the mean time column is batch time. |
| Mixed flow uses a deterministic bounded keyspace. | It is useful for regression tracking, but it is not a production traffic model. |

## Durability Path

These rows measure WAL-backed behavior. They should not be compared directly to
in-memory rows because they include serialization, file append, and flush work.

| Benchmark | Description | Input Size | Mean Time | Throughput |
| --- | --- | ---: | ---: | ---: |
| `BM_DurableSetWithWalFlush/1000` | `KVStore::Set` with WAL persistence enabled | 1,000 durable writes/batch | `603.2 us/batch` | `1.66M ops/sec` |
| `BM_DurableSetWithWalFlush/10000` | `KVStore::Set` with WAL persistence enabled | 10,000 durable writes/batch | `6.20 ms/batch` | `1.61M ops/sec` |
| `BM_WalReplay/1000` | Replay checksum-framed WAL records into memory | 1,000 records | `352.0 us` | `2.84M records/sec` |
| `BM_WalReplay/10000` | Replay checksum-framed WAL records into memory | 10,000 records | `3.75 ms` | `2.67M records/sec` |

Engineering notes:

| Observation | Why It Matters |
| --- | --- |
| WAL records are CRC32-protected and length-framed. | Replay verifies complete records before applying mutations. |
| Durable Set is intentionally lower than in-memory Set. | It includes write-ahead logging and flush behavior. |
| WAL replay is a recovery-path benchmark. | It measures startup/log-recovery work, not steady-state Get/Set latency. |

## Snapshot And Compaction Path

These rows measure full-state snapshots, snapshot-assisted recovery, and WAL
rotation compaction.

| Benchmark | Description | Input Size | Mean Time | Throughput |
| --- | --- | ---: | ---: | ---: |
| `BM_SnapshotSave/1000` | Write and verify a full snapshot | 1,000 entries | `191.0 us` | `5.24M entries/sec` |
| `BM_SnapshotSave/10000` | Write and verify a full snapshot | 10,000 entries | `2.14 ms` | `4.67M entries/sec` |
| `BM_SnapshotLoad/1000` | Load a full snapshot into memory | 1,000 entries | `117.3 us` | `8.53M entries/sec` |
| `BM_SnapshotLoad/10000` | Load a full snapshot into memory | 10,000 entries | `1.21 ms` | `8.28M entries/sec` |
| `BM_RecoveryFromSnapshotAndWalTail/1000` | Load snapshot plus uncompacted WAL tail | 1,000 base entries + 10% tail | `160.0 us` | `6.87M entries/sec` |
| `BM_RecoveryFromSnapshotAndWalTail/10000` | Load snapshot plus uncompacted WAL tail | 10,000 base entries + 10% tail | `1.72 ms` | `6.38M entries/sec` |
| `BM_RecoveryFromCompactedSnapshotAndWalTail/1000` | Load compacted snapshot plus rotated WAL tail | 1,000 base entries + 10% tail | `159.3 us` | `6.91M entries/sec` |
| `BM_RecoveryFromCompactedSnapshotAndWalTail/10000` | Load compacted snapshot plus rotated WAL tail | 10,000 base entries + 10% tail | `1.46 ms` | `7.56M entries/sec` |
| `BM_SnapshotCompaction/1000` | Save verified snapshot and rotate WAL | 1,000 entries | `220.6 us` | `4.53M entries/sec` |
| `BM_SnapshotCompaction/10000` | Save verified snapshot and rotate WAL | 10,000 entries | `2.46 ms` | `4.07M entries/sec` |

Engineering notes:

| Observation | Why It Matters |
| --- | --- |
| Snapshot compaction bounds future WAL replay. | Recovery can load the snapshot and replay only the current WAL generation. |
| Snapshot save verifies before WAL rotation. | The WAL is not discarded unless the replacement snapshot can be loaded and checked. |
| Filesystem cache state can affect snapshot and WAL rows. | Treat these as repeatable EC2 baselines, not universal storage-device claims. |

## CLI And Public Boundary

No CLI/end-to-end benchmark was recorded in the `2026-05-25T08:35:47Z` EC2
baseline.

| Benchmark | Description | Status |
| --- | --- | --- |
| CLI command path | Command parsing, server routing, KV operation, and output formatting | Not measured |

Future public-boundary benchmarks should be reported separately from direct
`KVStore` rows so parser/formatting cost does not get blended into hot-path
storage metrics.

## Caveats

- EC2 results can vary due to noisy neighbors and host placement.
- Burstable instances may throttle if CPU credits are exhausted. This run used
  `c7i-flex.large`; record credit state explicitly if a future run uses a
  burstable instance family.
- The run was not CPU-pinned. Matching-engine docs use pinned-core runs, so this
  KV-store baseline follows the same EC2 methodology style but not identical
  affinity controls.
- Results are only comparable when environment, commit, build flags, benchmark
  command, and workload shape are recorded.
- In-memory hot-path numbers should not be confused with durable write numbers.
- Snapshot and WAL benchmarks involve filesystem and cache effects.
- The KV-store and matching-engine benchmark numbers are not direct
  comparisons. They exercise different systems, workloads, and bottlenecks.
- The recorded Git metadata showed a dirty working tree. The dirty benchmark
  workflow/source changes were later committed as `d7adcb4`; rerun from a clean
  commit if strict release provenance is required.

## Reproducibility

Run a future refresh on EC2:

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

The current script does not run `ctest` itself. For the next formal baseline,
run tests explicitly before the benchmark and copy the pass count into this
document and `docs/Benchmark_History.md`.

See also: `docs/Benchmark_History.md`.
