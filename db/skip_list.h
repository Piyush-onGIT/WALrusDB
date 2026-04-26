#include <string>
#include <cstdint>

#define MAX_LEVEL 16

class SkipList {
 public:
  SkipList();
  void Insert(std::string key, std::string val);
  void Delete(std::string key);
  void PrintSkipList();
  

 private:
  struct SkipListNode {
    std::string key_;
    std::string val_;
    SkipListNode* next_[MAX_LEVEL];

    SkipListNode() {
      for (int i = 0; i < MAX_LEVEL; ++i) {
        next_[i] = nullptr;
      }
    }
  };

  int LevelPromoter();
 
  SkipListNode* head_;
  int current_top_level_;
};
