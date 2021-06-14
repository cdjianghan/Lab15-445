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
  // LOG_DEBUG("\n");
  std::lock_guard<std::mutex> lock(latch_);

  // for(int i = 0 ; i < (int)pool_size_;i++){
  //   LOG_DEBUG(" in buffer pool page : %d",pages_[i].GetPageId());
  // }
  // for(auto i = page_table_.begin();i != page_table_.end();i++){
  //   LOG_DEBUG("walk page_table key:%d value:%d",i->first,i->second);
  // }

  auto ans = page_table_.find(page_id);
  frame_id_t frame_id = -1;
  // bool allpined = true;
  // exist
  if (ans != page_table_.end()) {
    frame_id = page_table_[page_id];
    pages_[frame_id].pin_count_++;
    replacer_->Pin(frame_id);
    pages_[frame_id].is_dirty_ = true;
    // LOG_DEBUG("Fetch Page:%d exist ", (int)page_id);
    return &pages_[frame_id];
  }
  // not exist
  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
    // LOG_DEBUG("Fetch Page:%d not exist but free_list ", (int)page_id);
  } else {
    // too slow ------------------------------------------------------------------------------------
    // bool allpined = true;

    // for (size_t i = 0; i < pool_size_; i++) {
    //   if (pages_[i].pin_count_ == 0) {
    //     allpined = false;
    //   }
    // }
    // too slow ------------------------------------------------------------------------------------

    // if (allpined) {
    //   // LOG_DEBUG("Fetch Page:%d not exist and allpined ", (int)page_id);
    //   return nullptr;
    // }

    if (replacer_->Victim(&frame_id)) {
      if (pages_[frame_id].IsDirty()) {
        pages_[frame_id].is_dirty_ = false;
        disk_manager_->WritePage(pages_[frame_id].GetPageId(), pages_[frame_id].GetData());
      }
      page_table_.erase(pages_[frame_id].GetPageId());
      // LOG_DEBUG("Fetch Page:%d not exist but victim:%d", (int)page_id,(int)pages_[frame_id].GetPageId());
    } else {
      return nullptr;
    }
  }

  pages_[frame_id].ResetMemory();
  disk_manager_->ReadPage(page_id, pages_[frame_id].GetData());
  pages_[frame_id].is_dirty_ = false;
  // LOG_DEBUG("Fetch Page:%d Success ,Fetch Page data:%s", pages_[frame_id].GetPageId(),pages_[frame_id].GetData());
  pages_[frame_id].page_id_ = page_id;
  page_table_[page_id] = frame_id;
  pages_[frame_id].pin_count_ = 1;

  return &pages_[frame_id];

  return nullptr;
}

bool BufferPoolManager::UnpinPageImpl(page_id_t page_id, bool is_dirty) {
  // LOG_DEBUG("\n");
  std::lock_guard<std::mutex> lock(latch_);

  auto ans = page_table_.find(page_id);
  if (ans == page_table_.end()) {
    // LOG_DEBUG("Unpin Page:%d is not in the page_table",(int)page_id);
    return false;
  }

  frame_id_t frame_id = page_table_[page_id];
  if (pages_[frame_id].pin_count_ <= 0) {
    // LOG_DEBUG("Unpin Page:%d pin_count <= 0",(int)page_id);
    return false;
  }

  if (pages_[frame_id].pin_count_ > 0) {
    pages_[frame_id].pin_count_--;
    // LOG_DEBUG("Unpin Page:%d pin_count > 0",(int)page_id);
    replacer_->Unpin(frame_id);
  }

  if (is_dirty) {
    pages_[frame_id].is_dirty_ = true;
  } else {
    pages_[frame_id].is_dirty_ = false;
  }
  // LOG_DEBUG("Unpin Page %d and is it dirty :%d", (int)page_id,(int)is_dirty);
  return true;
}

bool BufferPoolManager::FlushPageImpl(page_id_t page_id) {
  std::lock_guard<std::mutex> lock(latch_);
  // LOG_DEBUG("\n");

  // Make sure you call DiskManager::WritePage!
  // if(page_table_.find(page_id) == page_table_.end()){
  //   return false;
  // }
  frame_id_t frame_id = page_table_[page_id];

  // LOG_DEBUG("Flush Page:%d Success ,Flush Page data:%s", pages_[frame_id].GetPageId(),pages_[frame_id].GetData());

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

  // too slow ------------------------------------------------------------------------------------
  // bool allpined = true;
  // for (size_t i = 0; i < pool_size_; i++) {
  //   if (pages_[i].pin_count_ == 0) {
  //     allpined = false;
  //   }
  // }
  // if (allpined) {
  //   // LOG_DEBUG("New Page:%d but allpined ", (int)*page_id);
  //   return nullptr;
  // }
  // too slow ------------------------------------------------------------------------------------

  // LOG_DEBUG("New Page: %d", (int)*page_id);
  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (replacer_->Victim(&frame_id)) {
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

  pages_[frame_id].ResetMemory();
  free_list_.push_front(frame_id);
  pages_[frame_id].page_id_ = INVALID_PAGE_ID;
  page_table_.erase(page_id);

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
