#include "skip_list.h"
#include <iostream>

SkipList::SkipList() {
  head_ = new SkipListNode();
  current_top_level_ = 0;

  for (int i = 0; i < MAX_LEVEL; i++) {
    head_->next_[i] = nullptr;
  }
}

int SkipList::LevelPromoter() {
  int level = 0;

  while (level < MAX_LEVEL) {
    int coin = rand() % 2;
    if (coin == 0) break;
    level++;
  }

  return level;
}

void SkipList::Insert(std::string key, std::string val) {
  int level = LevelPromoter();
  current_top_level_ = std::max(current_top_level_, level);
  current_top_level_ = std::min(current_top_level_, static_cast<int>(MAX_LEVEL));
  int level_itr_ = 0;

  while (level_itr_ <= level) {
    SkipListNode* itr_node = head_->next_[level_itr_];
    SkipListNode* update_node = head_;
    SkipListNode* new_node = new SkipListNode();
    new_node->key_ = key;
    new_node->val_ = val;

    if (itr_node == nullptr) {
      head_->next_[level_itr_] = new_node;
      level_itr_++;
      continue;
    }

    while (itr_node && itr_node->key_ < key) {
      update_node = itr_node;
      itr_node = itr_node->next_[level_itr_];
    }

    SkipListNode* tmp = update_node->next_[level_itr_];
    update_node->next_[level_itr_] = new_node;
    new_node->next_[level_itr_] = tmp;

    level_itr_++;
  }
}

void SkipList::Delete(std::string key) {

}

void SkipList::PrintSkipList() {
  for (int i = current_top_level_; i >= 0; i--) {
    std::cout << "Level " << i << ": ";
    SkipListNode* itr = head_->next_[i];
    while (itr) {
      std::cout << "(" << itr->key_ << ":" << itr->val_ << ")->";
      itr = itr->next_[i];
    }
    std::cout << "nullptr" << std::endl;
  }
}
