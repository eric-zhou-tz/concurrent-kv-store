#include "store/kv_store.h"

#include "persistence/snapshot.h"
#include "persistence/wal.h"

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace kv {
namespace store {

KVStore::KVStore(persistence::WriteAheadLog* wal,
                 persistence::Snapshot* snapshot)
    : wal_(wal), snapshot_(snapshot) {}

KVStore::KVStore(const KVStore& other) {
  std::shared_lock<std::shared_mutex> lock(other.mutex_);
  data_ = other.data_;
  wal_ = other.wal_;
  snapshot_ = other.snapshot_;
  writes_since_snapshot_ = other.writes_since_snapshot_;
}

KVStore& KVStore::operator=(const KVStore& other) {
  if (this == &other) {
    return *this;
  }

  std::unique_lock<std::shared_mutex> self_lock(mutex_, std::defer_lock);
  std::shared_lock<std::shared_mutex> other_lock(other.mutex_,
                                                std::defer_lock);
  std::lock(self_lock, other_lock);
  data_ = other.data_;
  wal_ = other.wal_;
  snapshot_ = other.snapshot_;
  writes_since_snapshot_ = other.writes_since_snapshot_;
  return *this;
}

KVStore::KVStore(KVStore&& other) {
  std::unique_lock<std::shared_mutex> lock(other.mutex_);
  data_ = std::move(other.data_);
  wal_ = other.wal_;
  snapshot_ = other.snapshot_;
  writes_since_snapshot_ = other.writes_since_snapshot_;
  other.wal_ = nullptr;
  other.snapshot_ = nullptr;
  other.writes_since_snapshot_ = 0;
}

KVStore& KVStore::operator=(KVStore&& other) {
  if (this == &other) {
    return *this;
  }

  std::unique_lock<std::shared_mutex> self_lock(mutex_, std::defer_lock);
  std::unique_lock<std::shared_mutex> other_lock(other.mutex_,
                                                std::defer_lock);
  std::lock(self_lock, other_lock);
  data_ = std::move(other.data_);
  wal_ = other.wal_;
  snapshot_ = other.snapshot_;
  writes_since_snapshot_ = other.writes_since_snapshot_;
  other.wal_ = nullptr;
  other.snapshot_ = nullptr;
  other.writes_since_snapshot_ = 0;
  return *this;
}

void KVStore::MaybeSnapshotLocked() {
  if (snapshot_ != nullptr && writes_since_snapshot_ >= kSnapshotInterval) {
    CompactPersistenceLocked();
  }
}

void KVStore::Set(const std::string& key, const std::string& value) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  if (wal_ != nullptr) {
    // Persist before mutating memory so a successful in-memory write has an
    // earlier durable record to replay after a crash.
    wal_->AppendSet(key, value);
  }
  data_[key] = value;
  ++writes_since_snapshot_;
  MaybeSnapshotLocked();
}

std::optional<std::string> KVStore::Get(const std::string& key) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  const auto it = data_.find(key);
  if (it == data_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool KVStore::Delete(const std::string& key) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  if (wal_ != nullptr) {
    // Log delete attempts even when the key is absent; replaying the same
    // operation is idempotent and preserves command ordering.
    wal_->AppendDelete(key);
  }
  const bool erased = data_.erase(key) > 0;
  ++writes_since_snapshot_;
  MaybeSnapshotLocked();
  return erased;
}

bool KVStore::Contains(const std::string& key) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return data_.find(key) != data_.end();
}

std::size_t KVStore::Size() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return data_.size();
}

void KVStore::Clear() {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  data_.clear();
  writes_since_snapshot_ = 0;
}

void KVStore::ClearPersistence() {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  // This is an administrative durability reset: it removes persisted recovery
  // files but intentionally leaves the live in-memory map untouched.
  if (wal_ != nullptr) {
    wal_->Clear();
  }
  if (snapshot_ != nullptr) {
    snapshot_->Clear();
  }
  writes_since_snapshot_ = 0;
}

bool KVStore::SaveSnapshot() {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  return SaveSnapshotLocked();
}

bool KVStore::SaveSnapshotLocked() {
  if (snapshot_ == nullptr) {
    return false;
  }

  const std::uint64_t wal_offset =
      (wal_ != nullptr) ? wal_->CurrentOffset() : 0;
  snapshot_->SaveVerified(data_, wal_offset);
  writes_since_snapshot_ = 0;
  return true;
}

bool KVStore::CompactPersistence() {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  return CompactPersistenceLocked();
}

bool KVStore::CompactPersistenceLocked() {
  if (snapshot_ == nullptr) {
    return false;
  }

  // WAL rotation resets the replay position to zero. Write and verify the
  // snapshot before touching WAL history so a failed snapshot cannot discard
  // the only durable copy of recent mutations.
  snapshot_->SaveVerified(data_, 0);
  if (wal_ != nullptr) {
    wal_->Rotate();
  }
  writes_since_snapshot_ = 0;
  return true;
}

kv::persistence::SnapshotLoadResult KVStore::LoadSnapshot(
    const persistence::Snapshot& snapshot) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  // Snapshot loading writes directly into the backing map without going through
  // Set/Delete, because recovery should not append recovered data back to WAL.
  return snapshot.Load(data_);
}

std::size_t KVStore::ReplayFromWal(const persistence::WriteAheadLog& wal,
                                   std::uint64_t offset) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  // Recovery applies directly to the backing map so it does not append the
  // recovered operations back into the WAL.
  return wal.ReplayFrom(offset, data_);
}

}  // namespace store
}  // namespace kv
