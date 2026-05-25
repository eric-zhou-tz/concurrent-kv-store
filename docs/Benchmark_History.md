# Benchmark History

This file is the curated history for benchmark runs worth preserving. The
project currently has no official CMake/Google Benchmark baseline after the
refactor.

## Baseline Slots

| Date | Version/Commit | Environment | Notes |
| --- | --- | --- | --- |
| TBD | TBD | TBD | First Release CMake benchmark run. |

## Update Checklist

1. Build with `-DCMAKE_BUILD_TYPE=Release`.
2. Run `./build/kv_store_benchmark`.
3. Save JSON and text artifacts under `benchmarks/results/` if the run should
   be retained.
4. Copy headline rows into `docs/Benchmarks.md`.
5. Add a dated summary row here with commit, hardware, compiler, and notes.
