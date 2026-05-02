#include <string>
#include <filesystem>
#include "include/types.h"
#include "writer_batch.h"

namespace fs = std::filesystem;

class Log {
 public:
  Log(const std::string db_name);
  void AddRecord(WriterBatch &batch);
  void Flush();

 private:
  int fd_;
};