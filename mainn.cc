#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

int main() {
  int fd = open("./db/wal.db", O_RDWR | O_CREAT, 0644);
  uint8_t op;
  read(fd, &op, 1);
  std::cout << (int)op << std::endl;

  uint32_t k;
  read(fd, &k, 1);
  std::cout << k << std::endl;
}