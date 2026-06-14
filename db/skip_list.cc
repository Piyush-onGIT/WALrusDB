#include "skip_list.h"

SkipList::SkipList() {
  head_ = new SkipListNode();
  current_top_level_ = 0;

  for (int i = 0; i < MAX_LEVEL; i++) {
    head_->next_[i] = nullptr;
  }
}

int SkipList::LevelPromoter() {
  int level = 0;

  while (level < MAX_LEVEL - 1) {
    int coin = rand() % 2;
    if (coin == 0)
      break;
    level++;
  }

  return level;
}

void SkipList::Insert(std::string key, std::string val) {
  std::optional<SkipListNode *> search_node = SearchNode(key);
  if (search_node) {
    (*search_node)->is_deleted_ = false;
    (*search_node)->val_ = val;
    return;
  }
  int level = LevelPromoter();
  current_top_level_ = std::max(current_top_level_, level);
  int level_itr = current_top_level_;

  SkipListNode *node_itr = head_;
  SkipListNode *new_node = new SkipListNode();
  new_node->key_ = key;
  new_node->val_ = val;

  for (level_itr; level_itr >= 0; level_itr--) {
    while (node_itr->next_[level_itr] != nullptr &&
           node_itr->next_[level_itr]->key_ < key) {
      node_itr = node_itr->next_[level_itr];
    }
    if (level_itr > level)
      continue;

    SkipListNode *tmp = node_itr->next_[level_itr];
    node_itr->next_[level_itr] = new_node;
    new_node->next_[level_itr] = tmp;
  }
}

void SkipList::InsertBatch(WriterBatch &batch) {
  std::string key, val;
  op_t op;

  int batch_itr = batch.Size() - 1;
  if (batch_itr < 0) return;

  while (batch_itr >= 0) {
    batch.Get(batch_itr, op, key, val);
    if (op == op_t::PUT) {
      this->Insert(key, val);
    } else {
      this->Delete(key);
    }
    batch_itr--;
  }
}

std::optional<std::string> SkipList::Search(std::string key) {
  std::optional<SkipListNode *> search_node = SearchNode(key);
  if (!search_node.has_value())
    return std::nullopt;

  if ((*search_node)->is_deleted_)
    return std::nullopt;
  return (*search_node)->val_;
}

std::optional<SkipList::SkipListNode *> SkipList::SearchNode(std::string key) {
  if (head_ == nullptr)
    return std::nullopt;

  int level_itr = current_top_level_;
  SkipListNode *node_itr = head_;

  for (level_itr; level_itr >= 0; level_itr--) {
    while (node_itr->next_[level_itr] != nullptr &&
           node_itr->next_[level_itr]->key_ < key) {
      node_itr = node_itr->next_[level_itr];
    }
  }

  if (node_itr->next_[0] != nullptr && node_itr->next_[0]->key_ == key)
    return node_itr->next_[0];
  return std::nullopt;
}

void SkipList::Delete(std::string key) {
  std::optional<SkipListNode *> search_node = SearchNode(key);
  if (!search_node.has_value())
    return;

  (*search_node)->is_deleted_ = true;
}

void SkipList::PrintSkipList() {
  for (int i = current_top_level_; i >= 0; i--) {
    std::cout << "Level " << i << ": ";
    SkipListNode *itr = head_->next_[i];
    while (itr) {
      if (itr->is_deleted_) {
        std::cout << "(" << itr->key_ << ": X)->";
      } else {
        std::cout << "(" << itr->key_ << ":" << itr->val_ << ")->";
      }
      itr = itr->next_[i];
    }
    std::cout << "nullptr" << std::endl;
  }
}

int SkipList::SizeOfNode() {
  return sizeof(SkipListNode);
}
