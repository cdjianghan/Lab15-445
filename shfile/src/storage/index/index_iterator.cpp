/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(page_id_t page_id, int index, BufferPoolManager *bufferPoolManager) {
  page_id_ = page_id;
  index_ = index;
  bufferPoolManager_ = bufferPoolManager;
  node_ = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(
      bufferPoolManager_->FetchPage(page_id_)->GetData());
}

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() { bufferPoolManager_->UnpinPage(page_id_, false); }

INDEX_TEMPLATE_ARGUMENTS
bool INDEXITERATOR_TYPE::isEnd() { return node_->GetNextPageId() == INVALID_PAGE_ID && node_->GetSize() <= index_; }

INDEX_TEMPLATE_ARGUMENTS
const MappingType &INDEXITERATOR_TYPE::operator*() { return node_->GetItem(index_); }

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE &INDEXITERATOR_TYPE::operator++() {
  index_++;
  if (index_ == node_->GetSize() && node_->GetNextPageId() != INVALID_PAGE_ID) {
    page_id_t n_page_id_ = node_->GetNextPageId();
    node_ = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(
        bufferPoolManager_->FetchPage(n_page_id_)->GetData());
    bufferPoolManager_->UnpinPage(page_id_, false);
    page_id_ = n_page_id_;
    index_ = 0;
  }
  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
