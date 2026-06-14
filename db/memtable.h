#pragma once

#include "skip_list.h"

enum MemtableState {
  ACTIVE,
  FLUSHING
};

class MemTable {
 public:
  MemTable();
  std::optional<std::string> Get(std::string key);
  void Insert(std::string key, std::string val);
  void Delete(std::string key);

 private:
  MemtableState state_ = MemtableState::ACTIVE;
  int memory_usage_ = 0;
  int flush_threshold_ = (64 * 1024 * 1024);  // 64MB
  SkipList* skip_list_;
};
