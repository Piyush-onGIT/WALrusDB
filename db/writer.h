#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

class Writer {
public:
  Writer(std::mutex *mtx);
  bool Write(std::string key, std::string val);
  void Wait();
  void Notify();

  std::string key_;
  std::string val_;
  bool done_;

private:
  std::mutex *mtx_;
  std::condition_variable cv_;
};
