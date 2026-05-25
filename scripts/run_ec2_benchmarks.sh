#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

TIMESTAMP="$(date -u +"%Y%m%d_%H%M%S")"
PUBLIC_IPV4="3.20.238.237"
RESULTS_ROOT="${ROOT_DIR}/benchmark_results"
RESULTS_DIR="${RESULTS_ROOT}/${TIMESTAMP}"
BUILD_DIR="${ROOT_DIR}/build-ec2-benchmarks"
BENCHMARK_EXE="${BUILD_DIR}/kv_store_benchmark"
REPETITIONS="${BENCHMARK_REPETITIONS:-5}"

mkdir -p "${RESULTS_DIR}"

TEXT_OUT="${RESULTS_DIR}/benchmarks.txt"
JSON_OUT="${RESULTS_DIR}/benchmarks.json"
METADATA_OUT="${RESULTS_DIR}/metadata.txt"

print_command() {
  printf '$'
  printf ' %q' "$@"
  printf '\n'
}

run_step() {
  local description="$1"
  shift

  echo "${description}..."
  if ! "$@"; then
    echo "ERROR: ${description} failed" >&2
    exit 1
  fi
}

{
  echo "Benchmark metadata"
  echo "=================="
  echo "date_utc: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  echo "hostname: $(hostname)"
  echo "public_ipv4_note: ${PUBLIC_IPV4}"
  echo "uname: $(uname -a)"
  echo "build_type: Release"
  echo "build_flags: -DCMAKE_BUILD_TYPE=Release"
  echo "benchmark_executable: ${BENCHMARK_EXE}"
  echo

  echo "OS release"
  echo "----------"
  if [[ -f /etc/os-release ]]; then
    cat /etc/os-release
  else
    echo "/etc/os-release not found"
  fi
  echo

  echo "CPU summary"
  echo "-----------"
  if command -v lscpu >/dev/null 2>&1; then
    lscpu
  else
    echo "lscpu not found"
  fi
  echo

  echo "CPU model"
  echo "---------"
  if [[ -f /proc/cpuinfo ]]; then
    grep -m 1 "model name" /proc/cpuinfo || true
  else
    echo "/proc/cpuinfo not found"
  fi
  echo

  echo "Core/thread count"
  echo "-----------------"
  if command -v nproc >/dev/null 2>&1; then
    echo "nproc: $(nproc)"
  fi
  if command -v lscpu >/dev/null 2>&1; then
    lscpu | grep -E "^(CPU\\(s\\)|Core\\(s\\) per socket|Thread\\(s\\) per core|Socket\\(s\\)):" || true
  fi
  echo

  echo "Memory summary"
  echo "--------------"
  if command -v free >/dev/null 2>&1; then
    free -h
  else
    grep -E "MemTotal|MemAvailable" /proc/meminfo || true
  fi
  echo

  echo "Toolchain"
  echo "---------"
  echo "compiler:"
  "${CXX:-c++}" --version | head -n 1
  echo "cmake:"
  cmake --version | head -n 1
  echo

  echo "Git"
  echo "---"
  echo "commit: $(git rev-parse HEAD)"
  echo "branch: $(git branch --show-current)"
  echo "status:"
  git status --short
  echo

  echo "Benchmark command"
  echo "-----------------"
  print_command "${BENCHMARK_EXE}" \
    "--benchmark_repetitions=${REPETITIONS}" \
    "--benchmark_report_aggregates_only=true" \
    "--benchmark_out=${JSON_OUT}" \
    "--benchmark_out_format=json"
} > "${METADATA_OUT}"

rm -rf "${BUILD_DIR}"
run_step "Configuring clean Release benchmark build" \
  cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

run_step "Building benchmark target" \
  cmake --build "${BUILD_DIR}" --config Release --target kv_store_benchmark

if [[ ! -x "${BENCHMARK_EXE}" ]]; then
  echo "ERROR: benchmark executable not found or not executable: ${BENCHMARK_EXE}" >&2
  exit 1
fi

echo "Running Google Benchmark suite..."
"${BENCHMARK_EXE}" \
  "--benchmark_repetitions=${REPETITIONS}" \
  "--benchmark_report_aggregates_only=true" \
  "--benchmark_out=${JSON_OUT}" \
  "--benchmark_out_format=json" | tee "${TEXT_OUT}"

echo
echo "Benchmark outputs:"
echo "  text:     ${TEXT_OUT}"
echo "  json:     ${JSON_OUT}"
echo "  metadata: ${METADATA_OUT}"
