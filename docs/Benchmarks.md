# Benchmarks

This page is the recruiter-readable benchmark publication template. Do not fill
in result numbers until the benchmark suite has been run on the intended EC2
machine.

Raw benchmark artifacts are local run outputs and should stay under
`benchmark_results/`. Commit curated summaries here and in
`docs/Benchmark_History.md`, not every raw JSON/text file.

## EC2 Benchmark Workflow

Target EC2 public IPv4:

```text
3.20.238.237
```

Manual command flow:

```bash
ssh ubuntu@3.20.238.237
cd ~/concurrent-kv-store
git pull
chmod +x scripts/run_ec2_benchmarks.sh
./scripts/run_ec2_benchmarks.sh
```

Optional repetition override:

```bash
BENCHMARK_REPETITIONS=10 ./scripts/run_ec2_benchmarks.sh
```

The script configures a clean Release build in `build-ec2-benchmarks/`, builds
`kv_store_benchmark`, records machine metadata, and writes timestamped outputs
under `benchmark_results/`.

## Benchmark Environment

| Field | Value |
| --- | --- |
| EC2 instance type | TBD |
| Public IPv4 | `3.20.238.237` |
| Hostname | TBD |
| CPU model | TBD |
| vCPU count | TBD |
| Memory | TBD |
| OS | TBD |
| Compiler | TBD |
| CMake | TBD |
| Build type | Release |
| Build flags | `-DCMAKE_BUILD_TYPE=Release` |
| Git commit | TBD |
| Git branch | TBD |
| Date | TBD |
| Same environment as matching-engine benchmarks | TBD |
| Notes | TBD |

## Existing Benchmark Targets

The current Google Benchmark executable is:

```text
build-ec2-benchmarks/kv_store_benchmark
```

Current benchmark names:

| Benchmark function | Publication row |
| --- | --- |
| `BM_Get` | Get |
| `BM_Put` | Set |
| `BM_Delete` | Delete |
| `BM_MixedReadWrite70_30` | Mixed 70/30 read-write |
| `BM_DurableSetWithWalFlush` | Durable Set with WAL flush |
| `BM_SnapshotSave` | Snapshot save |
| `BM_SnapshotLoad` | Snapshot load |
| `BM_WalReplay` | WAL replay |
| `BM_RecoveryFromSnapshotAndWalTail` | Snapshot + WAL tail recovery |
| `BM_RecoveryFromCompactedSnapshotAndWalTail` | Compacted snapshot + WAL tail recovery |
| `BM_SnapshotCompaction` | Snapshot verification + WAL rotation |

## Results Summary

| Benchmark | Workload | Mean time | Throughput / ops/sec | Repetitions | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| Get | `BM_Get/1000`, `BM_Get/10000` | TBD | TBD | TBD | In-memory successful lookup after preload. |
| Set | `BM_Put/1000`, `BM_Put/10000` | TBD | TBD | TBD | In-memory insert/overwrite path. |
| Delete | `BM_Delete/1000`, `BM_Delete/10000` | TBD | TBD | TBD | Delete after deterministic preload. |
| Mixed 70/30 read-write | `BM_MixedReadWrite70_30/1000`, `BM_MixedReadWrite70_30/10000` | TBD | TBD | TBD | Deterministic bounded keyspace. |
| Durable Set with WAL flush | `BM_DurableSetWithWalFlush/1000`, `BM_DurableSetWithWalFlush/10000` | TBD | TBD | TBD | Persisted `Set` path through WAL-backed `KVStore`. |
| Snapshot save | `BM_SnapshotSave/1000`, `BM_SnapshotSave/10000` | TBD | TBD | TBD | Full snapshot write and verification. |
| Snapshot load | `BM_SnapshotLoad/1000`, `BM_SnapshotLoad/10000` | TBD | TBD | TBD | Full snapshot load into a map. |
| WAL replay | `BM_WalReplay/1000`, `BM_WalReplay/10000` | TBD | TBD | TBD | Replays checksum-framed WAL records. |
| Snapshot + WAL tail recovery | `BM_RecoveryFromSnapshotAndWalTail/1000`, `BM_RecoveryFromSnapshotAndWalTail/10000` | TBD | TBD | TBD | Snapshot load plus uncompacted WAL tail replay. |

## Raw Results

The EC2 script writes:

```text
benchmark_results/<YYYYMMDD_HHMMSS>/metadata.txt
benchmark_results/<YYYYMMDD_HHMMSS>/benchmarks.txt
benchmark_results/<YYYYMMDD_HHMMSS>/benchmarks.json
```

Use the JSON and text output to fill the summary table manually. Keep raw files
local unless there is a specific reason to archive one outside git.

## Caveats

- Results should be collected only from Release builds.
- EC2 instances can have noisy-neighbor effects.
- Burstable instances may throttle if CPU credits are exhausted.
- Results should include commit hash and environment metadata.
- The same EC2 environment is used to keep methodology consistent with the
  low-latency matching-engine project.
- KV-store results should not be interpreted as direct performance comparisons
  with matching-engine results, because the systems exercise different
  workloads and bottlenecks.
- Results are not directly comparable across machines unless environment
  metadata is recorded with the run.
- Filesystem-sensitive benchmarks such as durable Set, snapshot save, WAL
  replay, and recovery depend on the EC2 storage configuration.

## TODOs

- Run the EC2 benchmark script on `3.20.238.237`.
- Copy the machine metadata into the Benchmark Environment table.
- Summarize aggregate rows from the Google Benchmark JSON output.
- Add a dated entry to `docs/Benchmark_History.md`.
