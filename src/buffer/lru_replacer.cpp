//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer.cpp
//
// Identification: src/buffer/lru_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_replacer.h"
#include <iostream>
namespace bustub {

LRUReplacer::LRUReplacer(size_t num_pages) {
  this->numsPages = num_pages;
  this->head = new node(0);
}

LRUReplacer::~LRUReplacer() {
  node *p = head->next;
  while (p != head) {
    head->next = p->next;
    delete p;
    p = head->next;
  }
  delete head;
}

bool LRUReplacer::Victim(frame_id_t *frame_id) {
  std::lock_guard<std::mutex> lock(mutex);
  if (head->next == head) {
    return false;
  }
  node *victim = head->prev;
  *frame_id = victim->id;
  head->prev = victim->prev;
  victim->prev->next = head;
  victim->next = victim->prev = victim;
  tables.erase(tables.find(*frame_id));
  head->id--;
  delete victim;
  return true;
}

void LRUReplacer::Pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock(mutex);
  if (tables.find(frame_id) != tables.end()) {
    tables[frame_id]->next->prev = tables[frame_id]->prev;
    tables[frame_id]->prev->next = tables[frame_id]->next;
    delete tables[frame_id];
    tables.erase(tables.find(frame_id));
    head->id--;
  }
}

void LRUReplacer::Unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock(mutex);
  if (tables.find(frame_id) != tables.end()) {
    return;
  }
  node *n = new node(frame_id);
  n->next = head->next;
  head->next = n;
  n->prev = head;
  n->next->prev = n;
  tables[frame_id] = n;
  head->id++;
}

size_t LRUReplacer::Size() {
  std::lock_guard<std::mutex> lock(mutex);
  return head->id;
}

}  // namespace bustub
