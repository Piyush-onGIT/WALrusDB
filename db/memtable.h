#pragma once

#include "skip_list.h"

enum MemtableState {
  ACTIVE,
  SEALED
};

class MemTable {
 public:
  std::optional<std::string> Search(std::string key);
  void Insert(std::string key, std::string val);
  void Delete(std::string key);

 private:
  MemtableState state_ = MemtableState::ACTIVE;
  int memory_usage_ = 0;
  int flush_threshold_ = 80;
  SkipList skip_list_;
};