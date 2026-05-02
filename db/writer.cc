#include "writer.h"

Writer::Writer(std::mutex* mtx): mtx_(mtx) {}

bool Writer::Write(std::string key, std::string val) {
  // add to WAL
  // insert into memtable
  // cv notify all
  // done
}

void Writer::Wait() {
  std::unique_lock<std::mutex> lock(*mtx_, std::adopt_lock);
  cv_.wait(lock);
  lock.release();
}

void Writer::Notify() {
  cv_.notify_one();
}