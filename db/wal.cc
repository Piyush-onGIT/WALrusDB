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

  int batch_itr = batch.Size();
  if (batch_itr == MAX_BATCH_SIZE) batch_itr--;

  while (batch_itr >= 0) {
    batch.Get(batch_itr, op, key, val);
    op_int = static_cast<uint8_t>(op);
    key_size = key.size();
    val_size = val.size();
    put_buffer.append(reinterpret_cast<char *>(&op_int), sizeof(op_int));
    put_buffer.append(reinterpret_cast<char *>(&key_size), sizeof(key_size));
    put_buffer.append(reinterpret_cast<char *>(&val_size), sizeof(val_size));
    put_buffer.append(key);
    put_buffer.append(val);
    batch_itr--;
  }
  write(fd_, put_buffer.data(), put_buffer.size());
}

void Log::Flush() {
  if (fsync(fd_) != 0) {
    perror("fsync failed");
  }
}