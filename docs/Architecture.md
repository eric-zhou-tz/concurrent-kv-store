# Architecture

## Overview

Concurrent KV Store is currently a single-process C++20 key-value store with a
small storage API, coarse-grained reader/writer synchronization, and a durable
recovery path. The design favors correctness, traceability, and clear module
boundaries before introducing a disk-optimized storage engine.

Phase 0.5 is a correctness-first concurrency phase. It does not implement
sharding, lock-free data structures, async WAL queues, networking, or
distributed replication.

```text
Command text
    |
    v
CliParser
    |
    v
CliServer
    |
    v
KVStore
    |
    +--> WriteAheadLog
    |
    +--> Snapshot
```

## Core Components

### KVStore

`KVStore` owns the live in-memory map. It exposes:

- `Set(key, value)`
- `Get(key)`
- `Delete(key)`
- `Contains(key)`
- `Size()`
- `Clear()`
- `CompactPersistence()`

When constructed with persistence dependencies, mutating operations append to
the WAL before updating memory. This keeps successful in-memory writes
recoverable after a process crash.

`KVStore` is thread-safe through one `std::shared_mutex`:

- `Get()`, `Contains()`, and `Size()` take a shared lock, so multiple readers
  may proceed concurrently.
- `Set()`, `Delete()`, `Clear()`, `ClearPersistence()`, snapshot save,
  compaction, snapshot load, and WAL replay take an exclusive lock.
- Write operations are serialized, including WAL appends.
- Snapshot and compaction observe one consistent map state because snapshot
  iteration happens while the exclusive store lock is held.
- Recovery methods are internally exclusive, but operationally they should run
  during startup before serving concurrent traffic.

The synchronization boundary is one `KVStore` instance. Passing the same
`WriteAheadLog` or `Snapshot` object to other threads and using it directly, or
sharing the same persistence object across multiple store instances, is
unsupported because those objects are protected only by the store instance that
is currently calling them.

### WriteAheadLog

`WriteAheadLog` is an append-only binary log of `SET` and `DELETE` mutations.
WAL record format v2 is a breaking change from the original length-only
framing; old WAL files must be discarded or migrated before replay.

Each v2 record is:

```text
[uint32 payload_length][uint32 crc32(payload)][payload bytes]
```

The checksum is a dependency-free CRC32 using polynomial `0xEDB88320`. It
covers the payload bytes only, not the length prefix or checksum field.

Payload layouts:

```text
SET:
[uint8 op=1][uint32 key_size][key][uint32 value_size][value]

DELETE:
[uint8 op=2][uint32 key_size][key]
```

Replay reads one complete frame at a time, verifies the CRC32, parses the full
payload, and only then applies the mutation. It tracks the byte offset after
the last fully validated record as `last_good_offset`.

Replay distinguishes these stop conditions internally:

- `CleanEof`: normal end of file after a valid record boundary.
- `PartialRecord`: torn length, checksum, or payload bytes at the file tail.
- `InvalidLength`: zero-length or larger-than-allowed record length.
- `InvalidOpcode`: unknown operation byte.
- `ChecksumMismatch`: payload bytes do not match the stored CRC32.
- `PartialPayload`: payload-internal key/value length claims bytes that are not
  present in the payload.
- `InvalidPayload`: payload has trailing bytes or another structural mismatch.

On any non-EOF stop, replay keeps all records up to `last_good_offset` and does
not apply the bad record. Recovery may call `ReplayFromAndTruncate()` to remove
the untrusted suffix after `last_good_offset`; truncation happens only after
replay has identified a validated record boundary.

Snapshot compaction uses WAL rotation rather than prefix truncation. After a
verified compacted snapshot is committed, the current WAL is replaced with a
new empty file. Future mutations append to the new WAL from offset zero.

### Snapshot

`Snapshot` writes full materialized state to a temp file and atomically replaces
the committed snapshot. Each snapshot stores:

- format magic
- format version
- covered WAL byte offset
- entry count
- length-prefixed key/value entries

Startup recovery loads the latest snapshot first, then replays WAL records
after the covered byte offset. With WAL v2, the post-snapshot tail is
checksum-verified record by record before mutations are applied.

Snapshot files are written to a temporary path first and renamed over the
committed snapshot only after the write completes. `SaveVerified()` then loads
the committed snapshot and checks the expected entry count, covered WAL offset,
and key/value contents before callers are allowed to clean up WAL history.

## Snapshot Compaction

The project currently chooses WAL rotation for compaction:

1. Flush the current WAL and snapshot the in-memory map with covered WAL offset
   `0`, because the next WAL generation starts at byte zero.
2. Verify the committed snapshot by loading it and comparing metadata and
   contents against memory.
3. Rotate the WAL to a new empty file only after verification succeeds.
4. Continue appending new mutations to the empty WAL.

Automatic snapshot checks happen on the write path after a mutation increments
the write counter. The implementation avoids recursive locking by using private
helpers that assume the caller already holds the exclusive `KVStore` lock:
`MaybeSnapshotLocked()`, `SaveSnapshotLocked()`, and
`CompactPersistenceLocked()`. Public snapshot and compaction methods acquire
the lock once, then delegate to those helpers.

Normal recovery after compaction is:

1. Load the verified snapshot.
2. Replay the current WAL from the snapshot's stored offset, normally zero.

Failure behavior:

- Snapshot write failure: the committed snapshot is not replaced, and the WAL
  is not rotated.
- Snapshot verification failure: the WAL is not rotated.
- Crash before WAL rotation: recovery may replay still-present covered WAL
  records, but the deterministic `SET`/`DELETE` log preserves final state.
- Crash after WAL rotation: recovery loads the compacted snapshot and replays
  the new WAL from zero.
- Missing WAL after a valid compacted snapshot: recovery loads the snapshot and
  treats the missing WAL as an empty tail.
- Corrupted snapshot with valid WAL: snapshot loading fails without touching the
  WAL, so operators/tests can fall back to full WAL replay from offset zero.

### CLI Boundary

The CLI parser and server are intentionally separate from storage. Parser tests
can stay focused on command framing, while storage tests exercise the API
directly.

The interactive CLI is a single-process REPL over the thread-safe store. It now
prints the project version and the Phase 0.5 concurrency contract at startup,
and exposes `INFO`, `VERSION`, and `STATUS` aliases for the same runtime
summary. The CLI does not create multiple client sessions by itself; concurrent
access is provided by the `KVStore` API for callers that use the store from
multiple threads.

Current CLI commands:

- `SET <key> <value>`
- `GET <key>`
- `DEL|DELETE <key>`
- `COMPACT|SNAPSHOT`
- `CLEAR PERSISTENCE`
- `INFO|VERSION|STATUS`
- `HELP`
- `EXIT`

`INFO` reports the build version, current entry count, concurrency model, and
durability serialization contract. Startup recovery runs before the REPL begins,
so replay does not race with live CLI commands.

## Complexity

| Operation | Complexity | Notes |
| --- | ---: | --- |
| `Set` | O(1) average | Hash insert/overwrite; persisted mode also appends WAL. |
| `Get` | O(1) average | Hash lookup under a shared reader lock. |
| `Delete` | O(1) average | Hash erase; delete is idempotent during replay. |
| WAL append | O(K + V) | Writes framed key/value bytes and flushes. |
| WAL replay | O(N) | Applies ordered mutation records. |
| Snapshot save | O(N) | Writes the full materialized map. |
| Snapshot load | O(N) | Replaces live map after successful parse. |
| Snapshot compaction | O(N) | Writes/verifies full snapshot, then rotates WAL. |

`K` is key size, `V` is value size, and `N` is stored entry or WAL record count,
depending on the operation.

## Benchmarking Model

Benchmarks are grouped by architecture boundary:

- In-memory hot-path benchmarks measure `KVStore` directly, without CLI or
  persistence overhead.
- Durability-path benchmarks measure WAL-backed writes and checksum-verified
  WAL replay.
- Snapshot-path benchmarks measure snapshot save/load, snapshot-assisted
  recovery, and compaction through WAL rotation.
- CLI/public-boundary benchmarks are not yet recorded and should remain
  separate when added so parser and formatting costs are visible.

Snapshot compaction is benchmarked as a recovery-shaping feature: it does not
make individual `Get` or `Set` operations faster, but it limits long-term
recovery work by replacing unbounded WAL replay with snapshot load plus a
shorter current WAL tail.

See [Benchmarks](Benchmarks.md) for the current EC2 Release baseline,
methodology, and caveats. The published EC2 tables are pre-Phase-0.5
concurrency results until a clean refresh is recorded.

## Tradeoffs

| Choice | Tradeoff |
| --- | --- |
| `std::unordered_map` live state | Simple average-case hot path, no sorted iteration guarantees. |
| Flush on every WAL append | Conservative durability, lower write throughput. |
| Full snapshots | Easy recovery model, higher checkpoint write amplification. |
| WAL rotation after snapshots | Simple cleanup policy, but crash before rotation can replay covered records. |
| Coarse reader/writer lock | Correct concurrent access with simple invariants, but writers and durability operations are serialized. |
| Host-endian binary format | Simple early implementation, future portability work needed. |
| CRC32 WAL records | Dependency-free corruption detection, not cryptographic integrity. |

## Future Direction

The next architecture steps are contention benchmarks, sharded maps with
per-shard locks, a single WAL writer or group commit path, segmented WAL files,
explicit binary endianness, and eventually a log-structured storage engine.
