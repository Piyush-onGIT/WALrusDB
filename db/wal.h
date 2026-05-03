#pragma once

#include <string>
#include <filesystem>
#include "writer_batch.h"
#include "skip_list.h"

namespace fs = std::filesystem;

class Log {
 public:
  Log(const std::string db_name);
  void AddRecord(WriterBatch &batch);
  void Replay(SkipList &skip_list);
  void Flush();

 private:
  int fd_;
};