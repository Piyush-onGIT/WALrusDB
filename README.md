# 🚀 WALrusDB

**A lightweight embedded key-value database with write-ahead logging, crash-safe persistence, and background compaction.**

---

## 🧠 Overview

WALrusDB is a simple yet powerful key-value store built from scratch to explore core database internals.
It focuses on **durability**, **correctness**, and **systems-level design** using a write-ahead log (WAL) as the primary storage mechanism.

This project demonstrates how real-world databases ensure data safety while maintaining performance.

---

## ⚙️ Features

* 📝 **Write-Ahead Logging (WAL)**
  All writes are appended to a log before being applied in-memory.

* 💾 **Crash Recovery**
  Database state is rebuilt by replaying the WAL on startup.

* 🔒 **Thread-safe operations**
  Safe concurrent access within a single process using mutexes.

* 🔄 **Background WAL Flush (fsync)**
  Periodic flushing ensures durability without blocking every write.

* 🧹 **WAL Compaction**
  Reduces log size by rewriting only the latest state of keys.

* 📦 **Embedded Design**
  No external server required — runs directly inside your application.

---

## 🏗️ Architecture

```text
PUT(key, value)
    ↓
Append to WAL  ───────────────┐
    ↓                         │
Update in-memory map          │
                              │
Background thread             │
    ↓                         │
fsync()  → durability         │
                              │
Compaction thread             │
    ↓                         │
Rewrite WAL → smaller log ◀───┘
```

---

## 🔑 Core Concepts

### Write-Ahead Log (WAL)

Every mutation is first written to disk before updating memory.
This guarantees recovery after crashes.

---

### Crash Recovery

On startup:

```text
Read WAL → Replay operations → Rebuild in-memory state
```

---

### Compaction

Over time, WAL grows with redundant updates:

```text
PUT a=1
PUT a=2
PUT a=3
```

Compaction rewrites it to:

```text
PUT a=3
```

---

## 📊 Performance

The database is designed to explore:

* Latency of `fsync`-based durability
* Impact of background flushing
* Behavior under contention (multi-threaded writes)
* WAL growth and compaction efficiency

Benchmarking includes:

* Single-thread latency
* Multi-thread throughput
* High-contention scenarios

---

## 🚧 Current Limitations

* Single-process ownership of the database (no multi-process support)
* In-memory index uses `std::unordered_map` (no range queries)
* No group commit yet (each write waits for durability window)
* Limited read optimization (no caching layers)

---

## 🛣️ Future Work

* ⚡ Group commit for batching fsync operations
* 📚 Replace unordered_map with ordered structure (skip list / tree)
* 📦 SSTable-based storage (LSM-tree design)
* 🔄 Multi-reader support
* 📈 Performance tuning and lock optimization

---

## 🧪 Example Usage

```cpp
Store db("./db/wal.log");

db.put("key", "value");

std::string val = db.get("key");

db.del("key");
```

---

## 🎯 Goal

This project is built to deeply understand:

* File I/O and durability
* Concurrency and synchronization
* Storage engine internals
* Trade-offs in real database systems

---

## 📌 Key Takeaway

> This is not just a key-value store — it's a step toward building a real storage engine.

---

## 📜 License

MIT License
