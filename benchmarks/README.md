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
