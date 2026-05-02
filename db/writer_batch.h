#include "include/types.h"
#include <string>

#define MAX_BATCH_SIZE 10

class WriterBatch {
public:
  bool Put(op_t op, const std::string key, const std::string val);
  void Get(int itr, op_t &op, std::string &key, std::string &val);
  int Size();

private:
  struct Updates {
    op_t op_;
    const std::string key_;
    const std::string val_;

    Updates(op_t op, const std::string key, const std::string val)
        : key_(key), val_(val), op_(op) {}
  };
  int current_batch_size_ = 0;
  Updates* updates_batch_[MAX_BATCH_SIZE];
};