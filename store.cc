#include "store.h"
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include "db/writer_batch.h"

Store::Store(const char *db_name) : log_(Log(db_name)) {
  if (!fs::exists(db_name)) {
    fs::create_directory(db_name);
  }

  stop_compact_thread_ = false;
  stop_flush_thread_ = false;
  background_flush_thread_ = std::thread(&Store::background_flush, this);
  background_compact_wal_thread_ =
      std::thread(&Store::background_compact_wal, this);

  while (1) {
    uint8_t op;
    uint32_t key_size;
    uint32_t value_size;

    // read header safely
    if (read(wal_fd_, &op, sizeof(op)) != sizeof(op))
      break;
    if (read(wal_fd_, &key_size, sizeof(key_size)) != sizeof(key_size))
      break;

    // allocate buffers
    std::string key(key_size, '\0');

    if (op == static_cast<uint8_t>(op_t::PUT)) {
      if (read(wal_fd_, &value_size, sizeof(value_size)) != sizeof(value_size))
        break;

      if (read(wal_fd_, key.data(), key_size) != key_size)
        break;

      std::string val(value_size, '\0');
      if (read(wal_fd_, val.data(), value_size) != value_size)
        break;

      kv_[key] = val;
    } else {
      if (read(wal_fd_, key.data(), key_size) != key_size)
        break;
      kv_.erase(key);
    }
  }
}

Store::~Store() {
  {
    std::lock_guard<std::mutex> lock(compact_thread_mtx_);
    stop_compact_thread_ = true;
  }
  {
    std::lock_guard<std::mutex> lock(flush_thread_mtx_);
    stop_flush_thread_ = true;
  }
  if (background_compact_wal_thread_.joinable()) {
    background_compact_wal_thread_.join();
  }
  if (background_flush_thread_.joinable()) {
    background_flush_thread_.join();
  }
  cv_.notify_all();
}

std::string Store::get(const std::string key) {
  {
    // std::lock_guard<std::mutex> lock(kv_mtx_);
    if (kv_.find(key) == kv_.end())
      return "";
    return kv_[key];
  }
}

bool Store::put(const std::string key, const std::string val) {
  Writer w(&writer_mtx_);
  WriterBatch batch;
  w.key_ = key;
  w.val_ = val;

  std::unique_lock<std::mutex> writer_lock(writer_mtx_);
  writer_queue_.push_back(&w);
  while (!w.done_ && writer_queue_.front() != &w) {
    w.Wait();
  }

  std::deque<Writer*>::iterator iter = writer_queue_.begin();
  Writer* last_writer = &w;
  for (iter; iter != writer_queue_.end(); iter++) {
    if (!batch.Put(op_t::PUT, key, val)) break;
    last_writer = *iter;
  }
  writer_lock.unlock();
  log_.AddRecord(batch);
  skip_list_.InsertBatch(batch);
  writer_lock.lock();

  while (true) {
    Writer* front = writer_queue_.front();
    writer_queue_.pop_front();

    if (front != &w) {
      front->done_ = true;
      front->Notify();
    }
    if (front == last_writer) break;
  }

  if (!writer_queue_.empty()) {
    writer_queue_.front()->Notify();
  }
}

void Store::del(const std::string key) {
  Writer w(&writer_mtx_);
  w.key_ = key;
  w.val_ = "";std::unique_lock<std::mutex> lock(writer_mtx_);
  
  std::unique_lock<std::mutex> lock(writer_mtx_);
  writer_queue_.push_back(&w);
  while (writer_queue_.front() != &w) {
    w.Wait();
  }

  {
    // std::lock_guard<std::mutex> lock(kv_mtx_);
    if (kv_.find(key) == kv_.end())
      return;
  }

  std::string del_buffer;
  uint8_t op = static_cast<uint8_t>(op_t::DELETE);
  uint32_t key_size = key.size();

  del_buffer.append(reinterpret_cast<char *>(&op), sizeof(op));
  del_buffer.append(reinterpret_cast<char *>(&key_size), sizeof(key_size));
  del_buffer.append(key);

  {
    // std::lock_guard<std::mutex> lock(kv_mtx_);
    write(wal_fd_, del_buffer.data(), del_buffer.size());
    kv_.erase(key);
  }
}

void Store::background_flush() {
  while (1) {
    std::unique_lock<std::mutex> lock(flush_thread_mtx_);

    cv_.wait_for(lock, std::chrono::milliseconds(100),
                 [&]() { return stop_flush_thread_; });
    if (stop_flush_thread_)
      break;

    lock.unlock();
    if (wal_fd_ <= 0)
      continue;

    if (fsync(wal_fd_) != 0) {
      perror("fsync failed");
      break;
    }
  }
}

void Store::background_compact_wal() {
  while (!stop_compact_thread_) {
    std::unique_lock<std::mutex> lock(compact_thread_mtx_);

    cv_.wait_for(lock, std::chrono::milliseconds(100),
                 [&]() { return stop_compact_thread_; });
    if (stop_compact_thread_)
      break;

    lock.unlock();
    if (wal_fd_ <= 0)
      continue;

    uint8_t op;
    uint32_t key_size;
    uint32_t value_size;
    std::string compact_buffer;
    int tmp_wal_fd;
    int dir_fd;

    for (auto x : kv_) {
      op = static_cast<uint8_t>(op_t::PUT);
      key_size = x.first.size();
      value_size = x.second.size();

      compact_buffer.append(reinterpret_cast<char *>(&op), sizeof(op));
      compact_buffer.append(reinterpret_cast<char *>(&key_size),
                            sizeof(key_size));
      compact_buffer.append(reinterpret_cast<char *>(&value_size),
                            sizeof(value_size));
      compact_buffer.append(x.first);
      compact_buffer.append(x.second);
    }

    tmp_wal_fd =
        open("./db/tmp_compact_wal.log", O_RDWR | O_CREAT | O_TRUNC, 0644);
    write(tmp_wal_fd, compact_buffer.data(), compact_buffer.size());
    if (fsync(tmp_wal_fd) != 0) {
      perror("fsync failed");
      close(tmp_wal_fd);
      return;
    }
    close(tmp_wal_fd);

    // atomic replace
    if (rename("./db/tmp_compact_wal.log", db_file_) != 0) {
      perror("rename failed");
      return;
    }

    // flush directory metadata
    dir_fd = open("./db", O_DIRECTORY);
    if (dir_fd >= 0) {
      fsync(dir_fd);
      close(dir_fd);
    }
  }
}
