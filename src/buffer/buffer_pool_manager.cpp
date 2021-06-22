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
  // LOG_DEBUG("\n");
  std::lock_guard<std::mutex> lock(latch_);

  auto ans = page_table_.find(page_id);
  frame_id_t frame_id = -1;
  // if exist
  if (ans != page_table_.end()) {
    frame_id = page_table_[page_id];
    pages_[frame_id].pin_count_++;
    replacer_->Pin(frame_id);
    return &pages_[frame_id];
  }
  // not exist
  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (replacer_->Victim(&frame_id)) {
      assert(pages_[frame_id].GetPageId() != INVALID_PAGE_ID);
      if (pages_[frame_id].IsDirty()) {
        // 必须保证能取出来的victim的pin_count_都为0
        pages_[frame_id].is_dirty_ = false;
        disk_manager_->WritePage(pages_[frame_id].GetPageId(), pages_[frame_id].GetData());
        if (pages_[frame_id].pin_count_ != 0) {
          LOG_DEBUG("page id is %d, pin_count is %d.", page_id, pages_[frame_id].pin_count_);
          throw new Exception("wo gua le ");
        }
      }
      // 是不是脏的都要进行写回
      page_table_.erase(pages_[frame_id].GetPageId());
      // LOG_DEBUG("Fetch Page:%d not exist but victim:%d", (int)page_id,(int)pages_[frame_id].GetPageId());
    } else {
      return nullptr;
    }
  }
  pages_[frame_id].ResetMemory();
  disk_manager_->ReadPage(page_id, pages_[frame_id].GetData());
  //  pages_[frame_id].is_dirty_ = false;
  // LOG_DEBUG("Fetch Page:%d Success ,Fetch Page data:%s", pages_[frame_id].GetPageId(),pages_[frame_id].GetData());
  pages_[frame_id].page_id_ = page_id;
  page_table_[page_id] = frame_id;
  pages_[frame_id].pin_count_++;
  return &pages_[frame_id];
}

bool BufferPoolManager::UnpinPageImpl(page_id_t page_id, bool is_dirty) {
  // LOG_DEBUG("\n");
  std::lock_guard<std::mutex> lock(latch_);
  auto ans = page_table_.find(page_id);
  if (ans == page_table_.end()) {
    return false;
  }

  frame_id_t frame_id = page_table_[page_id];
  if (pages_[frame_id].pin_count_ <= 0) {
    return false;
  }

  if (pages_[frame_id].pin_count_ > 0) {
    pages_[frame_id].pin_count_--;
    // LOG_DEBUG("Unpin Page:%d pin_count > 0",(int)page_id);
    if (pages_[frame_id].pin_count_ == 0) {
      replacer_->Unpin(frame_id);
    }
  }

  pages_[frame_id].is_dirty_ |= is_dirty;
  // LOG_DEBUG("Unpin Page %d and is it dirty :%d", (int)page_id,(int)is_dirty);
  return true;
}

// Make sure you call DiskManager::WritePage!
bool BufferPoolManager::FlushPageImpl(page_id_t page_id) {
  std::lock_guard<std::mutex> lock(latch_);
  frame_id_t frame_id = page_table_[page_id];
  pages_[frame_id].is_dirty_ = false;
  disk_manager_->WritePage(page_id, pages_[frame_id].GetData());

  return true;
}

/** new a page in disk , and then write something to this page, if we set is_dirty true ,
 *  after we flush page to the disk, we should write it to the disks
 * */
Page *BufferPoolManager::NewPageImpl(page_id_t *page_id) {
  // LOG_DEBUG("\n");
  // 0.   Make sure you call DiskManager::AllocatePage!
  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  // 4.   Set the page ID output parameter. Return a pointer to P.

  std::lock_guard<std::mutex> lock(latch_);
  frame_id_t frame_id;
  *page_id = disk_manager_->AllocatePage();

  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (replacer_->Victim(&frame_id)) {
      assert(pages_[frame_id].GetPageId() != INVALID_PAGE_ID);
      if (pages_[frame_id].is_dirty_) {
        // LOG_DEBUG("I write a dirty page: %d,it's data is
        // %s",pages_[frame_id].GetPageId(),pages_[frame_id].GetData());
        pages_[frame_id].is_dirty_ = false;
        disk_manager_->WritePage(pages_[frame_id].GetPageId(), pages_[frame_id].GetData());
      }
      page_table_.erase(pages_[frame_id].GetPageId());
    } else {
      return nullptr;
    }
  }

  page_table_[*page_id] = frame_id;
  pages_[frame_id].ResetMemory();
  pages_[frame_id].page_id_ = *page_id;
  pages_[frame_id].pin_count_ = 1;
  pages_[frame_id].is_dirty_ = false;
  return &pages_[frame_id];
}

bool BufferPoolManager::DeletePageImpl(page_id_t page_id) {
  // LOG_DEBUG("\n");
  // 0.   Make sure you call DiskManager::DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  // auto ans = page_table_.find(page_id);
  std::lock_guard<std::mutex> lock(latch_);
  frame_id_t frame_id = page_table_[page_id];
  if (page_table_.find(page_id) == page_table_.end()) {
    return true;
  }
  if (pages_[frame_id].pin_count_ != 0) {
    return false;
  }
  //  success in lab1
  //  pages_[frame_id].ResetMemory();
  //  free_list_.push_front(frame_id);
  //  pages_[frame_id].page_id_ = INVALID_PAGE_ID;
  //  page_table_.erase(page_id);
  pages_[frame_id].is_dirty_ = false;
  pages_[frame_id].pin_count_ = 0;
  pages_[frame_id].page_id_ = INVALID_PAGE_ID;
  pages_[frame_id].ResetMemory();
  replacer_->Pin(frame_id);
  free_list_.push_back(frame_id);
  disk_manager_->DeallocatePage(page_id);

  return true;
}

void BufferPoolManager::FlushAllPagesImpl() {
  // LOG_DEBUG("\n");
  // You can do it!

  std::lock_guard<std::mutex> lock(latch_);
  for (auto it : page_table_) {
    FlushPageImpl(it.first);
  }
}

}  // namespace bustub
