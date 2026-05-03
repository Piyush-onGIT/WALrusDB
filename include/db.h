#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include "writer.h"
#include "skip_list.h"
#include "wal.h"

class DB {
public:
  DB(const char *db_file);
  ~DB();
  std::string get(const std::string key);
  void del(const std::string key);
  void put(const std::string key, const std::string value);

private:
  void PutWriter(Writer &w);
  void background_flush();
  void background_compact_wal();

  int wal_fd_;
  Log *log_;
  bool stop_flush_thread_;
  bool stop_compact_thread_;
  std::mutex writer_mtx_;
  SkipList *skip_list_;
  std::mutex flush_thread_mtx_;
  std::mutex compact_thread_mtx_;
  std::deque<Writer *> writer_queue_;
  [[maybe_unused]] std::condition_variable cv_;
  [[maybe_unused]] uint64_t last_fsynced_id_ = 0;
  std::thread background_flush_thread_;
  std::thread background_compact_wal_thread_;
  [[maybe_unused]] std::atomic<uint64_t> global_commit_id_{0};
  std::unordered_map<std::string, std::string> kv_;
};
