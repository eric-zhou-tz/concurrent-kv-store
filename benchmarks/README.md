# Benchmarks

Google Benchmark sources live under workload-focused directories:

```text
benchmarks/core_hot_path/
```

Build and run manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kv_store_benchmark
./build/kv_store_benchmark
```

Current project status is `v0.5.0`: `KVStore` uses a coarse
`std::shared_mutex` for concurrent readers and serialized writes/durability
operations. The benchmark executable builds against that code, but the
published EC2 tables in `docs/Benchmarks.md` are still the pre-concurrency
baseline until a clean refresh is recorded.

Publication runs should use `scripts/run_ec2_benchmarks.sh` on the target EC2
instance. Raw result artifacts are written under `benchmark_results/` and are
ignored by default unless a curated baseline is intentionally promoted into
documentation.

The current published baseline is documented in `docs/Benchmarks.md` and
`docs/Benchmark_History.md`. It separates direct in-memory `KVStore` hot-path
rows from WAL, snapshot, and recovery-path rows so durability costs are not
blended into core storage API numbers.

The current benchmark sources do not yet include official multi-threaded
contention rows. Phase 0.5 concurrency uses a coarse reader/writer lock; future
benchmark additions should label read-only and mixed read/write thread tests as
measurements of that design rather than sharded or lock-free storage.

Useful future rows:

- `BM_ConcurrentReadOnly`
- `BM_ConcurrentMixedReadWrite90_10`
- `BM_ConcurrentMixedReadWrite70_30`
