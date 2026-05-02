#include "writer_batch.h"

bool WriterBatch::Put(op_t op, const std::string key, const std::string val) {
  if (current_batch_size_ >= MAX_BATCH_SIZE) {
    return false;
  }
  Updates *update = new Updates(op, key, val);
  updates_batch_[current_batch_size_] = update;
  current_batch_size_++;
  return true;
}

void WriterBatch::Get(int itr, op_t &op, std::string &key, std::string &val) {
  op = updates_batch_[itr]->op_;
  key = updates_batch_[itr]->key_;
  val = updates_batch_[itr]->val_;
}

int WriterBatch::Size() {
  return current_batch_size_;
}