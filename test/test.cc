/**
 * test.cc
 *
 * Comprehensive test suite for WalrusDB.
 * All reads and writes go through DBWrapper
 *
 * Build:
 *  g++ -I ../include/ -I ../db/ test.cc -L ../build/ -lwalrusdb -lpthread -std=c++17 -o test
 *
 * Run:
 *   ./test
 */

#include <db.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ─────────────────────────────────────────────
// WRAPPER — swap this out to test walrusdb
// ─────────────────────────────────────────────

class DBWrapper {
 public:
  explicit DBWrapper(const std::string& path) {
    db_ = new DB(path.data());
  }

  ~DBWrapper() { delete db_; }

  bool Put(const std::string& key, const std::string& val) {
    db_->put(key, val);
    return true;
  }

  bool Get(const std::string& key, std::string& val) {
    val = db_->get(key);
    return (val != "");
  }

  bool Delete(const std::string& key) {
    db_->del(key);
    return true;
  }

 private:
  DB* db_;
};

// ─────────────────────────────────────────────
// LATENCY TRACKER
//
// records per-op latency in microseconds.
// thread-safe: multiple threads can call record() simultaneously.
// call finalize() after all threads join — sorts once so percentiles are O(1).
// ─────────────────────────────────────────────

class LatencyTracker {
 public:
  // record one sample (microseconds)
  void record(double us) {
    std::lock_guard<std::mutex> lk(mu_);
    samples_.push_back(us);
  }

  // must be called after all threads are done
  void finalize() {
    std::lock_guard<std::mutex> lk(mu_);
    std::sort(samples_.begin(), samples_.end());
  }

  // percentile: pct in [0, 100]
  double p(double pct) const {
    if (samples_.empty()) return 0.0;
    size_t idx = static_cast<size_t>(pct / 100.0 * samples_.size());
    if (idx >= samples_.size()) idx = samples_.size() - 1;
    return samples_[idx];
  }

  double min()   const { return samples_.empty() ? 0.0 : samples_.front(); }
  double max()   const { return samples_.empty() ? 0.0 : samples_.back();  }
  size_t count() const { return samples_.size(); }

  double avg() const {
    if (samples_.empty()) return 0.0;
    double sum = 0.0;
    for (double v : samples_) sum += v;
    return sum / samples_.size();
  }

  std::string summary() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1)
       << "p50=" << p(50)  << "us"
       << "  p95=" << p(95)  << "us"
       << "  p99=" << p(99)  << "us"
       << "  min=" << min()  << "us"
       << "  max=" << max()  << "us"
       << "  avg=" << avg()  << "us"
       << "  n="   << count();
    return ss.str();
  }

 private:
  std::vector<double> samples_;
  std::mutex          mu_;
};

// RAII helper — records elapsed micros into a LatencyTracker on destruction
struct ScopedLatency {
  using Clock = std::chrono::steady_clock;
  LatencyTracker&   tracker;
  Clock::time_point t0;

  explicit ScopedLatency(LatencyTracker& lt)
      : tracker(lt), t0(Clock::now()) {}

  ~ScopedLatency() {
    double us = std::chrono::duration<double, std::micro>(
                    Clock::now() - t0).count();
    tracker.record(us);
  }
};

// ─────────────────────────────────────────────
// UTILITIES
// ─────────────────────────────────────────────

struct TestResult {
  std::string name;
  bool        passed;
  double      elapsed_ms;
  std::string note;
  std::string latency;
};

std::vector<TestResult> g_results;
std::mutex              g_results_mtx;
using Clock = std::chrono::steady_clock;

void record_result(const std::string& name, bool passed, double ms,
                   const std::string& note    = "",
                   const std::string& latency = "") {
  std::lock_guard<std::mutex> lk(g_results_mtx);
  g_results.push_back({name, passed, ms, note, latency});
  std::cout << (passed ? "  [PASS]" : "  [FAIL]") << "  " << name;
  if (!note.empty())    std::cout << "  —  " << note;
  std::cout << "  (" << std::fixed << std::setprecision(5) << ms << " ms)\n";
  if (!latency.empty()) std::cout << "         " << latency << "\n";
}

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

std::string make_key(const std::string& pfx, int i) {
  return pfx + std::to_string(i);
}
std::string make_val(int i) { return "value_" + std::to_string(i); }

// ─────────────────────────────────────────────
// 1. BASIC TESTS
// ─────────────────────────────────────────────

void test_basic(DBWrapper& db) {
  std::cout << "\n[1] Basic tests\n";

  {
    auto t0 = Clock::now();
    bool ok = db.Put("hello", "world");
    std::string val;
    ok = ok && db.Get("hello", val) && (val == "world");
    record_result("put and get", ok, ms_since(t0));
  }
  {
    auto t0 = Clock::now();
    db.Put("key", "first");
    db.Put("key", "second");
    std::string val; db.Get("key", val);
    record_result("overwrite key", val == "second", ms_since(t0));
  }
  {
    auto t0 = Clock::now();
    std::string val;
    record_result("get missing key returns false",
                  !db.Get("does_not_exist", val), ms_since(t0));
  }
  {
    auto t0 = Clock::now();
    db.Put("todelete", "yes");
    db.Delete("todelete");
    std::string val;
    record_result("delete key", !db.Get("todelete", val), ms_since(t0));
  }
  {
    auto t0 = Clock::now();
    record_result("delete missing key no crash",
                  db.Delete("never_existed"), ms_since(t0));
  }
  {
    auto t0 = Clock::now();
    std::string big(1024 * 64, 'X');  // 64 KB
    db.Put("bigval", big);
    std::string val; db.Get("bigval", val);
    record_result("large value 64KB", val == big, ms_since(t0));
  }
  {
    auto t0 = Clock::now();
    std::string bkey = "bin\x00\x01\x02key";
    db.Put(bkey, "binval");
    std::string val;
    record_result("binary key",
                  db.Get(bkey, val) && val == "binval", ms_since(t0));
  }
}

// ─────────────────────────────────────────────
// 2. SINGLE THREADED SEQUENTIAL
// ─────────────────────────────────────────────

void test_single_threaded(DBWrapper& db) {
  std::cout << "\n[2] Single threaded tests\n";
  const int N = 10000;

  // sequential write
  {
    LatencyTracker lt;
    auto t0 = Clock::now();
    for (int i = 0; i < N; i++) {
      ScopedLatency sl(lt);
      db.Put(make_key("st_", i), make_val(i));
    }
    double ms = ms_since(t0);
    lt.finalize();
    record_result("sequential write 10k", true, ms,
                  std::to_string(int(N / ms * 1000)) + " ops/sec",
                  lt.summary());
  }

  // sequential read
  {
    LatencyTracker lt;
    auto t0   = Clock::now();
    int  hits = 0;
    for (int i = 0; i < N; i++) {
      std::string val;
      ScopedLatency sl(lt);
      if (db.Get(make_key("st_", i), val) && val == make_val(i)) hits++;
    }
    double ms = ms_since(t0);
    lt.finalize();
    record_result("sequential read 10k", hits == N, ms,
                  std::to_string(hits) + "/" + std::to_string(N) + " correct",
                  lt.summary());
  }

  // random read
  {
    std::mt19937                    rng(42);
    std::uniform_int_distribution<> dist(0, N - 1);
    LatencyTracker lt;
    auto t0   = Clock::now();
    int  hits = 0;
    for (int i = 0; i < N; i++) {
      int k = dist(rng);
      std::string val;
      ScopedLatency sl(lt);
      if (db.Get(make_key("st_", k), val) && val == make_val(k)) hits++;
    }
    double ms = ms_since(t0);
    lt.finalize();
    record_result("random read 10k", hits == N, ms,
                  std::to_string(int(N / ms * 1000)) + " ops/sec",
                  lt.summary());
  }

  // sequential delete
  {
    LatencyTracker lt;
    auto t0 = Clock::now();
    for (int i = 0; i < N; i++) {
      ScopedLatency sl(lt);
      db.Delete(make_key("st_", i));
    }
    int remaining = 0;
    for (int i = 0; i < N; i++) {
      std::string v;
      if (db.Get(make_key("st_", i), v)) remaining++;
    }
    lt.finalize();
    record_result("sequential delete 10k", remaining == 0, ms_since(t0), "", lt.summary());
  }
}

// ─────────────────────────────────────────────
// 3. MULTITHREADED — NO CONTENTION
// ─────────────────────────────────────────────

void test_mt_no_contention(DBWrapper& db) {
  std::cout << "\n[3] Multithreaded — no contention (disjoint keyspaces)\n";
  const int THREADS = 8, PER = 2000;

  // concurrent writes
  {
    LatencyTracker           lt;
    std::vector<std::thread> pool;
    std::atomic<int>         errors{0};
    auto t0 = Clock::now();

    for (int t = 0; t < THREADS; t++) {
      pool.emplace_back([&db, &lt, &errors, t]() {
        for (int i = 0; i < PER; i++) {
          std::string k = "nc_t" + std::to_string(t) + "_" + std::to_string(i);
          ScopedLatency sl(lt);
          if (!db.Put(k, make_val(i))) errors++;
        }
      });
    }
    for (auto& th : pool) th.join();
    double ms = ms_since(t0);
    lt.finalize();
    record_result("concurrent writes disjoint", errors == 0, ms,
                  std::to_string(int(THREADS * PER / ms * 1000)) + " ops/sec",
                  lt.summary());
  }

  // verify writes
  {
    auto t0      = Clock::now();
    int  correct = 0;
    for (int t = 0; t < THREADS; t++) {
      for (int i = 0; i < PER; i++) {
        std::string k = "nc_t" + std::to_string(t) + "_" + std::to_string(i);
        std::string val;
        if (db.Get(k, val) && val == make_val(i)) correct++;
      }
    }
    record_result("verify disjoint writes", correct == THREADS * PER, ms_since(t0),
                  std::to_string(correct) + "/" + std::to_string(THREADS * PER));
  }

  // concurrent reads
  {
    LatencyTracker           lt;
    std::vector<std::thread> pool;
    std::atomic<int>         errors{0};
    auto t0 = Clock::now();

    for (int t = 0; t < THREADS; t++) {
      pool.emplace_back([&db, &lt, &errors, t]() {
        for (int i = 0; i < PER; i++) {
          std::string k = "nc_t" + std::to_string(t) + "_" + std::to_string(i);
          std::string val;
          ScopedLatency sl(lt);
          if (!db.Get(k, val) || val != make_val(i)) errors++;
        }
      });
    }
    for (auto& th : pool) th.join();
    double ms = ms_since(t0);
    lt.finalize();
    record_result("concurrent reads disjoint", errors == 0, ms,
                  std::to_string(int(THREADS * PER / ms * 1000)) + " ops/sec",
                  lt.summary());
  }
}

// ─────────────────────────────────────────────
// 4. MULTITHREADED — WITH CONTENTION
// ─────────────────────────────────────────────

void test_mt_contention(DBWrapper& db) {
  std::cout << "\n[4] Multithreaded — with contention (shared keyspace)\n";
  const int THREADS = 8, KEYSPACE = 100, OPS = 5000;

  for (int i = 0; i < KEYSPACE; i++) db.Put(make_key("ct_", i), make_val(i));

  // mixed read + write on same keys
  {
    LatencyTracker           write_lt, read_lt;
    std::vector<std::thread> pool;
    std::atomic<int>         write_ok{0}, read_ok{0};
    auto t0 = Clock::now();

    for (int t = 0; t < THREADS; t++) {
      pool.emplace_back([&]( int tid) {
        std::mt19937                    rng(tid * 1000);
        std::uniform_int_distribution<> kdist(0, KEYSPACE - 1);
        std::uniform_int_distribution<> opdist(0, 1);
        for (int i = 0; i < OPS; i++) {
          int k = kdist(rng);
          if (opdist(rng) == 0) {
            ScopedLatency sl(write_lt);
            if (db.Put(make_key("ct_", k), make_val(k))) write_ok++;
          } else {
            std::string val;
            ScopedLatency sl(read_lt);
            if (db.Get(make_key("ct_", k), val)) read_ok++;
          }
        }
      }, t);
    }
    for (auto& th : pool) th.join();
    write_lt.finalize(); read_lt.finalize();
    double ms = ms_since(t0);
    record_result("contention mixed read+write", true, ms,
                  "writes=" + std::to_string(write_ok.load()) +
                  "  reads=" + std::to_string(read_ok.load()),
                  "writes: " + write_lt.summary() +
                  "\n         reads:  " + read_lt.summary());
  }

  // hotkey stress
  {
    LatencyTracker           lt;
    std::vector<std::thread> pool;
    std::atomic<bool>        crashed{false};
    auto t0 = Clock::now();

    for (int t = 0; t < THREADS; t++) {
      pool.emplace_back([&](int tid) {
        for (int i = 0; i < 1000; i++) {
          ScopedLatency sl(lt);
          if (!db.Put("hotkey", std::to_string(tid * 1000 + i))) crashed = true;
        }
      }, t);
    }
    for (auto& th : pool) th.join();
    std::string val; bool exists = db.Get("hotkey", val);
    lt.finalize();
    record_result("single hotkey concurrent writes", !crashed && exists,
                  ms_since(t0), "final val=" + val, lt.summary());
  }
}

// ─────────────────────────────────────────────
// 5. KEYSPACE DISTRIBUTION TESTS
// ─────────────────────────────────────────────

void test_keyspace(DBWrapper& db) {
  std::cout << "\n[5] Keyspace distribution tests\n";
  const int N = 20000;

  // uniform random
  {
    std::mt19937                    rng(99);
    std::uniform_int_distribution<> dist(0, N);
    LatencyTracker lt;
    auto t0 = Clock::now();
    for (int i = 0; i < N; i++) {
      ScopedLatency sl(lt);
      db.Put(make_key("ks_uni_", dist(rng)), make_val(i));
    }
    lt.finalize();
    record_result("uniform random keyspace write 20k", true, ms_since(t0), "", lt.summary());
  }

  // sequential (worst case for plain BST)
  {
    LatencyTracker lt;
    auto t0 = Clock::now();
    for (int i = 0; i < N; i++) {
      ScopedLatency sl(lt);
      db.Put(make_key("ks_seq_", i), make_val(i));
    }
    lt.finalize();
    record_result("sequential keyspace write 20k", true, ms_since(t0), "", lt.summary());
  }

  // hotspot 80/20
  {
    std::mt19937                     rng(77);
    std::uniform_real_distribution<> p(0.0, 1.0);
    std::uniform_int_distribution<>  hot(0, N / 5);
    std::uniform_int_distribution<>  cold(N / 5, N);
    LatencyTracker lt;
    auto t0 = Clock::now(); int writes = 0;
    for (int i = 0; i < N; i++) {
      int key = (p(rng) < 0.8) ? hot(rng) : cold(rng);
      ScopedLatency sl(lt);
      if (db.Put(make_key("ks_hot_", key), make_val(i))) writes++;
    }
    lt.finalize();
    record_result("hotspot 80/20 write 20k", writes == N, ms_since(t0), "", lt.summary());
  }

  // read verify sequential
  {
    LatencyTracker lt;
    auto t0 = Clock::now(); int correct = 0;
    for (int i = 0; i < N; i++) {
      std::string val;
      ScopedLatency sl(lt);
      if (db.Get(make_key("ks_seq_", i), val) && val == make_val(i)) correct++;
    }
    lt.finalize();
    record_result("sequential keyspace read verify", correct == N, ms_since(t0),
                  std::to_string(correct) + "/" + std::to_string(N), lt.summary());
  }
}

// ─────────────────────────────────────────────
// 6. READ HEAVY  (90% read / 10% write)
// ─────────────────────────────────────────────

void test_read_heavy(DBWrapper& db) {
  std::cout << "\n[6] Read-heavy workload (90% read / 10% write)\n";
  const int THREADS = 8, KEYSPACE = 5000, OPS = 10000;

  for (int i = 0; i < KEYSPACE; i++) db.Put(make_key("rh_", i), make_val(i));

  LatencyTracker           read_lt, write_lt;
  std::atomic<int>         reads{0}, writes{0}, read_errors{0};
  std::vector<std::thread> pool;
  auto t0 = Clock::now();

  for (int t = 0; t < THREADS; t++) {
    pool.emplace_back([&](int tid) {
      std::mt19937                    rng(tid);
      std::uniform_int_distribution<> kdist(0, KEYSPACE - 1);
      std::uniform_int_distribution<> opdist(0, 9);  // 0=write, 1-9=read
      for (int i = 0; i < OPS; i++) {
        int k = kdist(rng);
        if (opdist(rng) == 0) {
          ScopedLatency sl(write_lt);
          db.Put(make_key("rh_", k), make_val(k)); writes++;
        } else {
          std::string val;
          ScopedLatency sl(read_lt);
          if (db.Get(make_key("rh_", k), val)) reads++;
          else read_errors++;
        }
      }
    }, t);
  }
  for (auto& th : pool) th.join();
  read_lt.finalize(); write_lt.finalize();
  double ms = ms_since(t0);

  record_result("read heavy 90/10", read_errors == 0, ms,
                "reads=" + std::to_string(reads.load()) +
                "  writes=" + std::to_string(writes.load()) +
                "  throughput=" + std::to_string(int(THREADS * OPS / ms * 1000)) + " ops/sec",
                "reads:  " + read_lt.summary() +
                "\n         writes: " + write_lt.summary());
}

// ─────────────────────────────────────────────
// 7. WRITE HEAVY  (90% write / 10% read)
// ─────────────────────────────────────────────

void test_write_heavy(DBWrapper& db) {
  std::cout << "\n[7] Write-heavy workload (90% write / 10% read)\n";
  const int THREADS = 8, KEYSPACE = 5000, OPS = 10000;

  LatencyTracker           read_lt, write_lt;
  std::atomic<int>         reads{0}, writes{0}, write_errors{0};
  std::vector<std::thread> pool;
  auto t0 = Clock::now();

  for (int t = 0; t < THREADS; t++) {
    pool.emplace_back([&](int tid) {
      std::mt19937                    rng(tid * 333);
      std::uniform_int_distribution<> kdist(0, KEYSPACE - 1);
      std::uniform_int_distribution<> opdist(0, 9);  // 0=read, 1-9=write
      for (int i = 0; i < OPS; i++) {
        int k = kdist(rng);
        if (opdist(rng) == 0) {
          std::string val;
          ScopedLatency sl(read_lt);
          db.Get(make_key("wh_", k), val); reads++;
        } else {
          ScopedLatency sl(write_lt);
          if (!db.Put(make_key("wh_", k), make_val(k))) write_errors++;
          else writes++;
        }
      }
    }, t);
  }
  for (auto& th : pool) th.join();
  read_lt.finalize(); write_lt.finalize();
  double ms = ms_since(t0);

  record_result("write heavy 90/10", write_errors == 0, ms,
                "writes=" + std::to_string(writes.load()) +
                "  reads=" + std::to_string(reads.load()) +
                "  throughput=" + std::to_string(int(THREADS * OPS / ms * 1000)) + " ops/sec",
                "writes: " + write_lt.summary() +
                "\n         reads:  " + read_lt.summary());
}

// ─────────────────────────────────────────────
// 8. HIGH PERFORMANCE THROUGHPUT BENCHMARK
// ─────────────────────────────────────────────

void test_high_perf(DBWrapper& db) {
  std::cout << "\n[8] High performance throughput benchmark\n";
  const int THREADS = 16, PER = 5000;

  // partitioned write
  {
    LatencyTracker           lt;
    std::vector<std::thread> pool;
    std::atomic<int>         total_written{0};
    auto t0 = Clock::now();

    for (int t = 0; t < THREADS; t++) {
      pool.emplace_back([&](int tid) {
        int base = tid * PER;
        for (int i = 0; i < PER; i++) {
          ScopedLatency sl(lt);
          if (db.Put(make_key("hp_", base + i), make_val(base + i))) total_written++;
        }
      }, t);
    }
    for (auto& th : pool) th.join();
    double ms = ms_since(t0);
    lt.finalize();
    int total = THREADS * PER;
    record_result("high perf partitioned write", total_written == total, ms,
                  std::to_string(int(total / ms * 1000)) + " ops/sec  (" +
                  std::to_string(total) + " keys)", lt.summary());
  }

  // partitioned read
  {
    LatencyTracker           lt;
    std::vector<std::thread> pool;
    std::atomic<int>         correct{0};
    auto t0 = Clock::now();

    for (int t = 0; t < THREADS; t++) {
      pool.emplace_back([&](int tid) {
        int base = tid * PER;
        for (int i = 0; i < PER; i++) {
          std::string val;
          ScopedLatency sl(lt);
          if (db.Get(make_key("hp_", base + i), val) &&
              val == make_val(base + i)) correct++;
        }
      }, t);
    }
    for (auto& th : pool) th.join();
    double ms = ms_since(t0);
    lt.finalize();
    int total = THREADS * PER;
    record_result("high perf partitioned read", correct == total, ms,
                  std::to_string(int(total / ms * 1000)) + " ops/sec",
                  lt.summary());
  }
}

// ─────────────────────────────────────────────
// 9. CONCURRENT READ DURING ACTIVE WRITE
//
// This is the true stress test for memory ordering.
// Reader threads start immediately — they traverse
// the skip list while the writer is mid-splice.
// No barrier between writer and readers.
//
// Pass condition:
//   - no crash / segfault
//   - every key that was fully written (writer done)
//     is correctly readable after the writer finishes
//
// What it catches:
//   - torn pointer reads (crash)
//   - reader sees uninitialized node contents
//   - missing keys due to lost wakeup in writer queue
// ─────────────────────────────────────────────

void test_concurrent_read_write(DBWrapper& db) {
  std::cout << "\n[9] Concurrent read during active write\n";

  const int KEYSPACE    = 10000;
  const int READ_THREADS = 4;

  // ── sub-test A: no crash while reading during writes ──
  {
    std::atomic<bool> writer_done{false};
    std::atomic<int>  crashes{0};
    LatencyTracker    write_lt, read_lt;
    auto              t0 = Clock::now();

    // single writer — inserts all keys sequentially
    std::thread writer([&]() {
      for (int i = 0; i < KEYSPACE; i++) {
        ScopedLatency sl(write_lt);
        db.Put(make_key("crw_", i), make_val(i));
      }
      writer_done = true;
    });

    // reader threads — start immediately, read while writer is active
    // key may or may not exist yet — we only check for crash/corruption
    std::vector<std::thread> readers;
    for (int t = 0; t < READ_THREADS; t++) {
      readers.emplace_back([&](int tid) {
        std::mt19937                    rng(tid * 7);
        std::uniform_int_distribution<> dist(0, KEYSPACE - 1);
        while (!writer_done) {
          std::string val;
          ScopedLatency sl(read_lt);
          // result not checked — key may not be written yet
          // we are testing that this does NOT crash or corrupt
          db.Get(make_key("crw_", dist(rng)), val);
        }
      }, t);
    }

    writer.join();
    for (auto& r : readers) r.join();
    write_lt.finalize(); read_lt.finalize();
    double ms = ms_since(t0);

    record_result("no crash during concurrent read+write", crashes == 0, ms,
                  "writer inserted " + std::to_string(KEYSPACE) + " keys"
                  "  readers ran " + std::to_string(READ_THREADS) + " threads",
                  "writes: " + write_lt.summary() +
                  "\n         reads:  " + read_lt.summary());
  }

  // ── sub-test B: after writer finishes, all keys must be readable ──
  // writer is fully done at this point — expected value is deterministic.
  // both existence AND exact value match are required to count as correct.
  {
    LatencyTracker lt;
    auto           t0       = Clock::now();
    int            correct  = 0;
    int            missing  = 0;
    int            mismatch = 0;

    for (int i = 0; i < KEYSPACE; i++) {
      std::string val;
      ScopedLatency sl(lt);
      if (!db.Get(make_key("crw_", i), val)) {
        missing++;
      } else if (val != make_val(i)) {
        mismatch++;
      } else {
        correct++;
      }
    }
    lt.finalize();
    double ms  = ms_since(t0);
    bool   ok  = (correct == KEYSPACE);

    record_result("all keys readable after concurrent write", ok, ms,
                  std::to_string(correct)  + "/" + std::to_string(KEYSPACE) + " correct"
                  "  missing=" + std::to_string(missing) +
                  "  value_mismatch=" + std::to_string(mismatch),
                  lt.summary());
  }

  // ── sub-test C: sustained concurrent read+write, no key loss ──
  // writer continuously overwrites keys while readers verify values
  {
    const int       SUSTAIN_KEYS = 1000;
    const int       ROUNDS       = 5;
    std::atomic<int> round{0};
    std::atomic<bool> stop{false};
    std::atomic<int>  mismatches{0};
    LatencyTracker    write_lt, read_lt;

    // seed keys first
    for (int i = 0; i < SUSTAIN_KEYS; i++)
      db.Put(make_key("sus_", i), make_val(0));

    auto t0 = Clock::now();

    // writer: overwrites all keys N rounds with a new value each round
    std::thread writer([&]() {
      for (int r = 1; r <= ROUNDS; r++) {
        for (int i = 0; i < SUSTAIN_KEYS; i++) {
          ScopedLatency sl(write_lt);
          db.Put(make_key("sus_", i), make_val(r));
        }
        round = r;
      }
      stop = true;
    });

    // readers: read keys and verify value is one of the valid round values
    // (any round value is valid — we just cannot see a value outside [0..ROUNDS])
    std::vector<std::thread> readers;
    for (int t = 0; t < READ_THREADS; t++) {
      readers.emplace_back([&](int tid) {
        std::mt19937                    rng(tid * 13);
        std::uniform_int_distribution<> dist(0, SUSTAIN_KEYS - 1);
        while (!stop) {
          std::string val;
          ScopedLatency sl(read_lt);
          if (db.Get(make_key("sus_", dist(rng)), val)) {
            // extract round number from "value_N"
            int v = std::stoi(val.substr(6));
            if (v < 0 || v > ROUNDS) mismatches++;
          }
        }
      }, t);
    }

    writer.join();
    for (auto& r : readers) r.join();
    write_lt.finalize(); read_lt.finalize();
    double ms = ms_since(t0);

    record_result("sustained concurrent read+write no corruption",
                  mismatches == 0, ms,
                  "rounds=" + std::to_string(ROUNDS) +
                  "  keys=" + std::to_string(SUSTAIN_KEYS) +
                  "  mismatches=" + std::to_string(mismatches.load()),
                  "writes: " + write_lt.summary() +
                  "\n         reads:  " + read_lt.summary());
  }
}

// ─────────────────────────────────────────────
// SUMMARY
// ─────────────────────────────────────────────

void print_summary() {
  int pass = 0, fail = 0;
  for (auto& r : g_results) r.passed ? pass++ : fail++;
  std::cout << "\n══════════════════════════════════════\n";
  std::cout << "  results: " << pass << " passed";
  if (fail > 0) std::cout << "  " << fail << " FAILED";
  std::cout << "\n══════════════════════════════════════\n";
  if (fail > 0) {
    std::cout << "\nfailed tests:\n";
    for (auto& r : g_results) if (!r.passed) std::cout << "  - " << r.name << "\n";
  }
}

// ─────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────

int main() {
  const std::string PATH = "test_db";

  std::cout << "opening db at " << PATH << "\n";
  DBWrapper db(PATH);

  test_basic(db);
  test_single_threaded(db);
  test_mt_no_contention(db);
  test_mt_contention(db);
  test_keyspace(db);
  test_read_heavy(db);
  test_write_heavy(db);
  test_high_perf(db);
  test_concurrent_read_write(db);

  print_summary();
  return 0;
}