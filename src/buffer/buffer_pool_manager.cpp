//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

#include <list>
#include <mutex>
#include <unordered_map>

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // We allocate a consecutive memory space for the buffer pool.
  pages_ = new Page[pool_size];
  replacer_ = new LRUReplacer(pool_size);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() {
  delete[] pages_;
  delete replacer_;
}

Page *BufferPoolManager::FetchPageImpl(page_id_t page_id) {
  // 1.     Search the page table for the requested page (P).
  // 1.1    If P exists, pin it and return it immediately.
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  // 2.     If R is dirty, write it back to the disk.
  // 3.     Delete R from the page table and insert P.
  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.
  std::lock_guard<std::mutex> lock(latch_);

  auto ans = page_table_.find(page_id);

  if (ans != page_table_.end()) {
    pages_[page_table_[page_id]].pin_count_++;
    replacer_->Pin(page_table_[page_id]);
    return &pages_[page_table_[page_id]];
  } else if (ans == page_table_.end()) {
    page_id_t victim = -1;

    if (free_list_.empty() && replacer_->Victim(&victim)) {
      if (pages_[page_table_[victim]].IsDirty()) {
        disk_manager_->WritePage(victim, pages_[page_table_[victim]].GetData());
      }
      page_table_[page_id] = page_table_[victim];
      page_table_.erase(page_table_.find(victim));
    } else if (free_list_.size() != 0) {
      frame_id_t frame_id = free_list_.front();
      free_list_.pop_front();
      page_table_[page_id] = frame_id;
    }
    if (victim == -1) {
      return nullptr;
    }
    pages_[page_table_[page_id]].ResetMemory();
    disk_manager_->ReadPage(page_id, pages_[page_table_[page_id]].data_);
    pages_[page_table_[page_id]].page_id_ = page_id;
    pages_[page_table_[page_id]].pin_count_++;

    return &pages_[page_table_[page_id]];
  }
  return nullptr;
}

bool BufferPoolManager::UnpinPageImpl(page_id_t page_id, bool is_dirty) {
  std::lock_guard<std::mutex> lock(latch_);
  auto ans = page_table_.find(page_id);

  if (ans == page_table_.end()) {
    return false;
  }

  if (pages_[page_table_[page_id]].pin_count_ <= 0) return false;
  if (is_dirty) {
    pages_[page_table_[page_id]].is_dirty_ = true;
  }

  pages_[page_table_[page_id]].pin_count_--;

  if (pages_[page_table_[page_id]].pin_count_ == 0) {
    replacer_->Unpin(page_id);
  }
  return true;
}

bool BufferPoolManager::FlushPageImpl(page_id_t page_id) {
  // std::lock_guard<std::mutex> lock(latch_);
  // Make sure you call DiskManager::WritePage!
  if (page_id == INVALID_PAGE_ID || page_table_.find(page_id) == page_table_.end()) return false;

  disk_manager_->WritePage(page_id, pages_[page_table_[page_id]].GetData());
  return true;
}

/** new a page in disk , and then write something to this page, if we set is_dirty true ,
 *  after we flush page to the disk, we should write it to the disks
 * */
Page *BufferPoolManager::NewPageImpl(page_id_t *page_id) {
  // 0.   Make sure you call DiskManager::AllocatePage!
  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  // 4.   Set the page ID output parameter. Return a pointer to P.
  std::lock_guard<std::mutex> lock(latch_);
  size_t i;
  for (i = 0; i < pool_size_; i++) {
    if (pages_[i].pin_count_ == 0) {
      break;
    }
  }
  if (i == pool_size_) return nullptr;
  *page_id = disk_manager_->AllocatePage();
  page_id_t victim = -1;

  if (!free_list_.empty()) {
    frame_id_t frame_id = free_list_.front();
    free_list_.pop_front();
    page_table_[*page_id] = frame_id;
  } else if (free_list_.empty() && replacer_->Victim(&victim)) {
    if (pages_[page_table_[victim]].is_dirty_) {
      disk_manager_->WritePage(victim, pages_[page_table_[victim]].GetData());
    }
    page_table_[*page_id] = page_table_[victim];
    page_table_.erase(page_table_.find(victim));
  }
  pages_[page_table_[*page_id]].ResetMemory();
  pages_[page_table_[*page_id]].page_id_ = *page_id;
  pages_[page_table_[*page_id]].pin_count_ = 1;
  return &pages_[page_table_[*page_id]];
}

bool BufferPoolManager::DeletePageImpl(page_id_t page_id) {
  // 0.   Make sure you call DiskManager::DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  // auto ans = page_table_.find(page_id);
  if (page_table_.find(page_id) == page_table_.end()) {
    return true;
  }
  if (pages_[page_table_[page_id]].pin_count_ != 0) {
    return false;
  }
  pages_[page_table_[page_id]].ResetMemory();
  free_list_.push_back(page_table_[page_id]);
  return true;
}

void BufferPoolManager::FlushAllPagesImpl() {
  // You can do it!
  std::lock_guard<std::mutex> lock(latch_);
  for (auto it : page_table_) {
    FlushPageImpl(it.first);
  }
}

}  // namespace bustub
