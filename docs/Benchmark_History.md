# Benchmark History

This file is the curated history for benchmark runs worth preserving. It should
contain summaries from intentional benchmark runs only. Do not add local or fake
results.

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
Summary:
Improvements:
Regressions:
Caveats:
Next steps:
```

## Publication Checklist

1. Run the EC2 workflow from the repository root:

   ```bash
   ssh ubuntu@3.20.238.237
   cd ~/concurrent-kv-store
   git pull
   chmod +x scripts/run_ec2_benchmarks.sh
   ./scripts/run_ec2_benchmarks.sh
   ```

2. Copy environment metadata from
   `benchmark_results/<YYYYMMDD_HHMMSS>/metadata.txt` into
   `docs/Benchmarks.md`.
3. Summarize aggregate Google Benchmark rows from
   `benchmark_results/<YYYYMMDD_HHMMSS>/benchmarks.json`.
4. Add one dated entry here with the commit, EC2 instance type, build flags,
   high-level summary, notable improvements/regressions, and raw artifact path.
5. Commit only curated docs updates unless a raw artifact is intentionally
   archived elsewhere.
