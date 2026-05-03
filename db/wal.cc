#include "wal.h"
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include "writer_batch.h"

Log::Log(const std::string db_name) {
  if (!fs::exists(db_name)) {
    fs::create_directory(db_name);
  }
  std::string wal_file_name = db_name + "/" + "wal.log";
  fd_ = open(wal_file_name.data(), O_RDWR | O_CREAT | O_APPEND, 0644);
}

void Log::AddRecord(WriterBatch &batch) {
  std::string key, val, put_buffer;
  op_t op;
  uint8_t op_int;
  uint32_t key_size, val_size;

  int batch_itr = batch.Size() - 1;
  if (batch_itr < 0) return;

  while (batch_itr >= 0) {
    batch.Get(batch_itr, op, key, val);
    op_int = static_cast<uint8_t>(op);
    key_size = key.size();
    val_size = val.size();
    put_buffer.append(reinterpret_cast<char *>(&op_int), sizeof(op_int));
    put_buffer.append(reinterpret_cast<char *>(&key_size), sizeof(key_size));
    if (op == op_t::DELETE) {
      put_buffer.append(key);
      goto next_update;
    }
    put_buffer.append(reinterpret_cast<char *>(&val_size), sizeof(val_size));
    put_buffer.append(key);
    put_buffer.append(val);

    next_update:
      batch_itr--;
  }
  write(fd_, put_buffer.data(), put_buffer.size());
}

void Log::Replay(SkipList &skip_list) {
  WriterBatch batch;
  while (1) {
    uint8_t op;
    uint32_t key_size;
    uint32_t value_size;

    // read header safely
    if (read(fd_, &op, sizeof(op)) != sizeof(op))
      break;
    if (read(fd_, &key_size, sizeof(key_size)) != sizeof(key_size))
      break;

    // allocate buffers
    std::string key(key_size, '\0');

    if (op == static_cast<uint8_t>(op_t::PUT)) {
      if (read(fd_, &value_size, sizeof(value_size)) != sizeof(value_size))
        break;

      if (read(fd_, key.data(), key_size) != key_size)
        break;

      std::string val(value_size, '\0');
      if (read(fd_, val.data(), value_size) != value_size)
        break;

      skip_list.Insert(key, val);
    } else {
      if (read(fd_, key.data(), key_size) != key_size)
        break;
      skip_list.Delete(key);
    }
  }
}

void Log::Flush() {
  if (fsync(fd_) != 0) {
    perror("fsync failed");
  }
}
