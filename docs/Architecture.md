# Architecture

## Overview

Concurrent KV Store is currently a single-process C++20 key-value store with a
small storage API and durable recovery path. The design favors correctness,
traceability, and clear module boundaries before introducing concurrency or a
disk-optimized storage engine.

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

When constructed with persistence dependencies, mutating operations append to
the WAL before updating memory. This keeps successful in-memory writes
recoverable after a process crash.

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

### CLI Boundary

The CLI parser and server are intentionally separate from storage. Parser tests
can stay focused on command framing, while storage tests exercise the API
directly.

## Complexity

| Operation | Complexity | Notes |
| --- | ---: | --- |
| `Set` | O(1) average | Hash insert/overwrite; persisted mode also appends WAL. |
| `Get` | O(1) average | Hash lookup. |
| `Delete` | O(1) average | Hash erase; delete is idempotent during replay. |
| WAL append | O(K + V) | Writes framed key/value bytes and flushes. |
| WAL replay | O(N) | Applies ordered mutation records. |
| Snapshot save | O(N) | Writes the full materialized map. |
| Snapshot load | O(N) | Replaces live map after successful parse. |

`K` is key size, `V` is value size, and `N` is stored entry or WAL record count,
depending on the operation.

## Tradeoffs

| Choice | Tradeoff |
| --- | --- |
| `std::unordered_map` live state | Simple average-case hot path, no sorted iteration guarantees. |
| Flush on every WAL append | Conservative durability, lower write throughput. |
| Full snapshots | Easy recovery model, higher checkpoint write amplification. |
| Single process / single writer | Clean invariants while persistence matures, no concurrent clients yet. |
| Host-endian binary format | Simple early implementation, future portability work needed. |
| CRC32 WAL records | Dependency-free corruption detection, not cryptographic integrity. |

## Future Direction

The next architecture steps are reader/writer synchronization, segmented WAL
files, snapshot compaction, and eventually a log-structured storage engine.
