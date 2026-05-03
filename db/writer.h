#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

class Writer {
public:
  Writer(std::mutex *mtx);
  void Wait();
  void Notify();

  std::string key_;
  std::string val_;
  bool is_tombstone_;
  bool done_;

private:
  std::mutex *mtx_;
  std::condition_variable cv_;
};
