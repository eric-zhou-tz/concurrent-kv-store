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
Each record is length-framed:

```text
[record_length][op][key_size][key][value_size][value]
[record_length][op][key_size][key]
```

`DELETE` records omit the value field. Replay applies valid records in order,
skips malformed bounded records, and stops at incomplete trailing records.

### Snapshot

`Snapshot` writes full materialized state to a temp file and atomically replaces
the committed snapshot. Each snapshot stores:

- format magic
- format version
- covered WAL byte offset
- entry count
- length-prefixed key/value entries

Startup recovery loads the latest snapshot first, then replays WAL records after
the covered byte offset.

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

## Future Direction

The next architecture steps are reader/writer synchronization, segmented WAL
files, snapshot compaction, and eventually a log-structured storage engine.
