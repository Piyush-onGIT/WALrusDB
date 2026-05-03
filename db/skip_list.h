#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include "writer_batch.h"
#include <iostream>

#define MAX_LEVEL 16

class SkipList {
 public:
  SkipList();
  void Insert(std::string key, std::string val);
  void InsertBatch(WriterBatch &batch);
  std::optional<std::string> Search(std::string key);
  void Delete(std::string key);
  void PrintSkipList();

 private:
  struct SkipListNode {
    std::string key_;
    std::string val_;
    bool is_deleted_;
    SkipListNode* next_[MAX_LEVEL];

    SkipListNode() {
      is_deleted_ = false;
      for (int i = 0; i < MAX_LEVEL; ++i) {
        next_[i] = nullptr;
      }
    }
  };

  int LevelPromoter();
  std::optional<SkipListNode*> SearchNode(std::string key);
 
  SkipListNode* head_;
  int current_top_level_;
};
