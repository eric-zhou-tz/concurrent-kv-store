#include <benchmark/benchmark.h>

#include "persistence/snapshot.h"
#include "persistence/wal.h"
#include "store/kv_store.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

class BenchmarkTempDir {
 public:
  BenchmarkTempDir() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto base = std::filesystem::temp_directory_path();

    for (int attempt = 0; attempt < 100; ++attempt) {
      const auto candidate =
          base / ("concurrent_kv_store_benchmark_" + std::to_string(stamp) +
                  "_" + std::to_string(attempt));
      std::error_code error;
      if (std::filesystem::create_directory(candidate, error)) {
        path_ = candidate;
        return;
      }
    }

    throw std::runtime_error("failed to create temporary benchmark directory");
  }

  ~BenchmarkTempDir() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  std::string FilePath(const std::string& filename) const {
    return (path_ / filename).string();
  }

 private:
  std::filesystem::path path_;
};

std::vector<std::string> MakeKeys(std::size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back("key-" + std::to_string(index));
  }
  return keys;
}

std::vector<std::string> MakeValues(std::size_t count) {
  std::vector<std::string> values;
  values.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    values.push_back("value-" + std::to_string(index));
  }
  return values;
}

void BM_Put(benchmark::State& state) {
  const auto keys = MakeKeys(static_cast<std::size_t>(state.range(0)));
  const auto values = MakeValues(keys.size());

  for (auto _ : state) {
    kv::store::KVStore store;
    std::size_t index = 0;
    for (; index < keys.size(); ++index) {
      store.Set(keys[index], values[index]);
    }
    benchmark::DoNotOptimize(store.Size());
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * keys.size());
}

void BM_Get(benchmark::State& state) {
  const auto keys = MakeKeys(static_cast<std::size_t>(state.range(0)));
  const auto values = MakeValues(keys.size());
  kv::store::KVStore store;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    store.Set(keys[index], values[index]);
  }

  std::size_t index = 0;
  for (auto _ : state) {
    const auto& key = keys[index++ % keys.size()];
    const auto value = store.Get(key);
    benchmark::DoNotOptimize(value.has_value());
    if (value.has_value()) {
      benchmark::DoNotOptimize(value->data());
    }
  }

  state.SetItemsProcessed(state.iterations());
}

void BM_Delete(benchmark::State& state) {
  const auto keys = MakeKeys(static_cast<std::size_t>(state.range(0)));
  const auto values = MakeValues(keys.size());

  for (auto _ : state) {
    state.PauseTiming();
    kv::store::KVStore store;
    for (std::size_t index = 0; index < keys.size(); ++index) {
      store.Set(keys[index], values[index]);
    }
    state.ResumeTiming();

    for (const auto& key : keys) {
      benchmark::DoNotOptimize(store.Delete(key));
    }
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * keys.size());
}

void BM_MixedReadWrite70_30(benchmark::State& state) {
  const auto key_count = static_cast<std::size_t>(state.range(0));
  const auto keys = MakeKeys(key_count);
  const auto values = MakeValues(key_count);
  kv::store::KVStore store;
  for (std::size_t index = 0; index < key_count; ++index) {
    store.Set(keys[index], values[index]);
  }

  std::size_t index = 0;
  for (auto _ : state) {
    const std::size_t current = index++ % key_count;
    if ((index % 10) < 7) {
      const auto value = store.Get(keys[current]);
      benchmark::DoNotOptimize(value.has_value());
      if (value.has_value()) {
        benchmark::DoNotOptimize(value->data());
      }
    } else {
      store.Set(keys[current], values[(current + index) % key_count]);
    }
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

void BM_RecoveryFromSnapshotAndWalTail(benchmark::State& state) {
  const auto base_count = static_cast<std::size_t>(state.range(0));
  const std::size_t tail_count = base_count / 10;
  const auto keys = MakeKeys(base_count + tail_count);
  const auto values = MakeValues(base_count + tail_count);

  BenchmarkTempDir temp_dir;
  const std::string wal_path = temp_dir.FilePath("kv_store.wal");
  const std::string snapshot_path = temp_dir.FilePath("kv_store.snapshot");

  {
    kv::persistence::WriteAheadLog wal(wal_path);
    kv::persistence::Snapshot snapshot(snapshot_path);
    kv::store::KVStore store(&wal, &snapshot);
    for (std::size_t index = 0; index < base_count; ++index) {
      store.Set(keys[index], values[index]);
    }
    if (!store.SaveSnapshot()) {
      throw std::runtime_error("failed to save benchmark snapshot");
    }

    for (std::size_t index = 0; index < tail_count; ++index) {
      const std::size_t key_index = base_count + index;
      wal.AppendSet(keys[key_index], values[key_index]);
    }
  }

  for (auto _ : state) {
    kv::persistence::WriteAheadLog wal(wal_path);
    kv::persistence::Snapshot snapshot(snapshot_path);
    kv::store::KVStore recovered(&wal, &snapshot);
    const auto result = recovered.LoadSnapshot(snapshot);
    benchmark::DoNotOptimize(recovered.ReplayFromWal(wal, result.wal_offset));
    benchmark::DoNotOptimize(recovered.Size());
  }

  state.SetItemsProcessed(state.iterations() * (base_count + tail_count));
}

BENCHMARK(BM_Put)->Arg(1000)->Arg(10000);
BENCHMARK(BM_Get)->Arg(1000)->Arg(10000);
BENCHMARK(BM_Delete)->Arg(1000)->Arg(10000);
BENCHMARK(BM_MixedReadWrite70_30)->Arg(1000)->Arg(10000);
BENCHMARK(BM_RecoveryFromSnapshotAndWalTail)->Arg(1000)->Arg(10000);

}  // namespace
