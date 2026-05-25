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

Publication runs should use `scripts/run_ec2_benchmarks.sh` on the target EC2
instance. Raw result artifacts are written under `benchmark_results/` and are
ignored by default unless a curated baseline is intentionally promoted into
documentation.

The current published baseline is documented in `docs/Benchmarks.md` and
`docs/Benchmark_History.md`. It separates direct in-memory `KVStore` hot-path
rows from WAL, snapshot, and recovery-path rows so durability costs are not
blended into core storage API numbers.
