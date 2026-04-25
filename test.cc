#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include "store.h"

using Clock = std::chrono::high_resolution_clock;

// ---------------- TIMER ----------------
long long now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()
    ).count();
}

// ---------------- STATS ----------------
void print_stats(std::vector<long long>& latencies, const std::string& name) {
    std::sort(latencies.begin(), latencies.end());

    long long sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    double avg = sum / (double)latencies.size();

    auto p50 = latencies[latencies.size() * 0.5];
    auto p95 = latencies[latencies.size() * 0.95];
    auto p99 = latencies[latencies.size() * 0.99];

    std::cout << "\n=== " << name << " ===\n";
    std::cout << "Ops: " << latencies.size() << "\n";
    // printing latencies in micro seconds
    std::cout << "Avg: " << (avg / 1000.0) << " us\n";
    std::cout << "P50: " << (p50 / 1000.0) << " us\n";
    std::cout << "P95: " << (p95 / 1000.0) << " us\n";
    std::cout << "P99: " << (p99 / 1000.0) << " us\n";
    // std::cout << "Avg: " << (avg / 1e6) << " ms\n";
    // std::cout << "P50: " << (p50 / 1e6) << " ms\n";
    // std::cout << "P95: " << (p95 / 1e6) << " ms\n";
    // std::cout << "P99: " << (p99 / 1e6) << " ms\n";
}

// ---------------- BASIC TEST ----------------
void test_basic(Store& db) {
    db.put("a", "1");
    db.put("b", "2");

    assert(db.get("a") == "1");
    assert(db.get("b") == "2");

    db.del("a");
    assert(db.get("a") == "");

    std::cout << "Basic test passed\n";
}

// ---------------- SINGLE THREAD ----------------
void test_single_thread_latency(Store& db, int N) {
    std::vector<long long> latencies;

    for (int i = 0; i < N; i++) {
        std::string key = "k" + std::to_string(i);
        std::string val = "v" + std::to_string(i);

        auto start = now_ns();
        db.put(key, val);
        auto end = now_ns();

        latencies.push_back(end - start);
    }

    print_stats(latencies, "Single Thread PUT");
}

// ---------------- READ LATENCY ----------------
void test_read_latency(Store& db, int N) {
    for (int i = 0; i < N; i++) {
        db.put("k" + std::to_string(i), "v");
    }

    std::vector<long long> latencies;

    for (int i = 0; i < N; i++) {
        auto start = now_ns();
        db.get("k" + std::to_string(i));
        auto end = now_ns();

        latencies.push_back(end - start);
    }

    print_stats(latencies, "GET latency");
}

// ---------------- MULTI THREAD (NO CONTENTION) ----------------
void test_multi_thread_no_contention(Store& db, int threads, int ops) {
    std::vector<std::thread> workers;
    std::vector<std::vector<long long>> all_lat(threads);

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < ops; i++) {
                std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);

                auto start = now_ns();
                db.put(key, "v");
                auto end = now_ns();

                all_lat[t].push_back(end - start);
            }
        });
    }

    for (auto& th : workers) th.join();

    std::vector<long long> merged;
    for (auto& v : all_lat)
        merged.insert(merged.end(), v.begin(), v.end());

    print_stats(merged, "Multi-thread PUT (no contention)");
}

// ---------------- HIGH CONTENTION ----------------
void test_high_contention(Store& db, int threads, int ops) {
    std::vector<std::thread> workers;
    std::vector<std::vector<long long>> all_lat(threads);

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < ops; i++) {
                auto start = now_ns();
                db.put("shared_key", "v");
                auto end = now_ns();

                all_lat[t].push_back(end - start);
            }
        });
    }

    for (auto& th : workers) th.join();

    std::vector<long long> merged;
    for (auto& v : all_lat)
        merged.insert(merged.end(), v.begin(), v.end());

    print_stats(merged, "High Contention PUT");
}

// ---------------- MIXED WORKLOAD ----------------
void test_mixed(Store& db, int threads, int ops) {
    std::vector<std::thread> workers;

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < ops; i++) {
                int r = rand() % 100;

                if (r < 50) {
                    db.get("k" + std::to_string(rand() % 1000));
                } else if (r < 80) {
                    db.put("k" + std::to_string(rand() % 1000), "v");
                } else {
                    db.del("k" + std::to_string(rand() % 1000));
                }
            }
        });
    }

    for (auto& th : workers) th.join();

    std::cout << "Mixed workload completed\n";
}

// ---------------- THROUGHPUT ----------------
void test_throughput(Store& db, int threads, int ops) {
    auto start = now_ns();

    std::vector<std::thread> workers;

    for (int t = 0; t < threads; t++) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < ops; i++) {
                db.put("k" + std::to_string(rand()), "v");
            }
        });
    }

    for (auto& th : workers) th.join();

    auto end = now_ns();

    double seconds = (end - start) / 1e9;
    long long total_ops = (long long)threads * ops;

    std::cout << "\nThroughput: "
              << (total_ops / seconds)
              << " ops/sec\n";
}

// ---------------- MAIN ----------------
int main() {
    srand(time(nullptr));

    Store db("./db/test_wal.log");

    test_basic(db);

    test_single_thread_latency(db, 1000);
    test_read_latency(db, 1000);

    test_multi_thread_no_contention(db, 4, 1000);
    test_high_contention(db, 4, 1000);

    test_mixed(db, 4, 5000);

    test_throughput(db, 4, 5000);

    return 0;
}
