# Benchmarks

Google Benchmark sources live under workload-focused directories:

```text
benchmarks/core_hot_path/
```

Build and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target kv_store_benchmark
./build/kv_store_benchmark
```

Save local output under `benchmarks/results/` when needed. Result artifacts are
ignored by default unless a curated baseline is intentionally promoted into
documentation.
