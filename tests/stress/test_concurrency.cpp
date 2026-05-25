#include "store/kv_store.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

using kv::store::KVStore;

void WaitForStart(const std::atomic<bool>& start) {
  while (!start.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

void WaitForReady(const std::atomic<int>& ready, int expected) {
  while (ready.load(std::memory_order_acquire) < expected) {
    std::this_thread::yield();
  }
}

std::string StressKey(int thread_index, int key_index) {
  return "stress-" + std::to_string(thread_index) + "-" +
         std::to_string(key_index);
}

std::string StressValue(int thread_index, int key_index, int round) {
  return "value-" + std::to_string(thread_index) + "-" +
         std::to_string(key_index) + "-" + std::to_string(round);
}

TEST(KVStoreConcurrencyStressTest,
     DeterministicDisjointKeyMixedWorkloadMatchesReference) {
  constexpr int kThreads = 8;
  constexpr int kKeysPerThread = 96;
  constexpr int kRounds = 32;

  KVStore store;
  std::atomic<bool> start{false};
  std::atomic<int> ready{0};
  std::atomic<bool> valid_reads{true};
  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int thread_index = 0; thread_index < kThreads; ++thread_index) {
    threads.emplace_back([&, thread_index] {
      ready.fetch_add(1, std::memory_order_release);
      WaitForStart(start);

      for (int round = 0; round < kRounds; ++round) {
        for (int key_index = 0; key_index < kKeysPerThread; ++key_index) {
          const std::string key = StressKey(thread_index, key_index);
          const std::string value =
              StressValue(thread_index, key_index, round);
          store.Set(key, value);

          const std::optional<std::string> observed = store.Get(key);
          if (!observed.has_value() || observed.value() != value) {
            valid_reads.store(false, std::memory_order_release);
          }

          if (((key_index + round) % 11) == 0) {
            store.Delete(key);
          }
        }
      }

      for (int key_index = 0; key_index < kKeysPerThread; ++key_index) {
        const std::string key = StressKey(thread_index, key_index);
        if ((key_index % 4) == 0) {
          store.Delete(key);
        } else {
          store.Set(key, StressValue(thread_index, key_index, kRounds));
        }
      }
    });
  }

  WaitForReady(ready, kThreads);
  start.store(true, std::memory_order_release);
  for (std::thread& thread : threads) {
    thread.join();
  }

  ASSERT_TRUE(valid_reads.load(std::memory_order_acquire));
  EXPECT_EQ(static_cast<std::size_t>(kThreads * kKeysPerThread * 3 / 4),
            store.Size());

  for (int thread_index = 0; thread_index < kThreads; ++thread_index) {
    for (int key_index = 0; key_index < kKeysPerThread; ++key_index) {
      const std::string key = StressKey(thread_index, key_index);
      if ((key_index % 4) == 0) {
        EXPECT_FALSE(store.Contains(key)) << key;
      } else {
        ASSERT_TRUE(store.Get(key).has_value()) << key;
        EXPECT_EQ(StressValue(thread_index, key_index, kRounds),
                  store.Get(key).value())
            << key;
      }
    }
  }
}

}  // namespace
