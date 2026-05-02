#include "memtable.h"

void MemTable::Insert(std::string key, std::string val) {
  skip_list_.Insert(key, val);
}

