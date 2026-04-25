#include "store.h"
#include <iostream>

int main() {
  const char *db_file = "./db/wal.log";
  Store db(db_file);

  while (1) {
    std::string op;
    std::string k, v;

    std::cout << "Op: ";
    std::cin >> op;
    if (op == "get" || op == "del") {
      std::cout << "key: ";
      std::cin >> k;
      if (op == "get")
        std::cout << db.get(k) << std::endl;
      else
        db.del(k);
    } else if (op == "put") {
      std::cout << "key: ";
      std::cin >> k;
      std::cout << "val: ";
      std::cin >> v;
      db.put(k, v);
    } else {
      std::cout << "Exiting...\n";
      break;
    }
  }
}