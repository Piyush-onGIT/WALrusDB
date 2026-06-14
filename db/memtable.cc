#include "memtable.h"

MemTable::MemTable() {
  skip_list_ = new SkipList();
}

std::optional<std::string> MemTable::Get(std::string key) {
  return skip_list_->Search(key);
}

void MemTable::Insert(std::string key, std::string val) {
  skip_list_->Insert(key, val);
}

void MemTable::Delete(std::string key) {
  skip_list_->Delete(key);
}
