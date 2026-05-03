#include "writer.h"

Writer::Writer(std::mutex* mtx): mtx_(mtx), done_(false), is_tombstone_(false) {}

void Writer::Wait() {
  std::unique_lock<std::mutex> lock(*mtx_, std::adopt_lock);
  cv_.wait(lock);
  lock.release();
}

void Writer::Notify() {
  cv_.notify_one();
}