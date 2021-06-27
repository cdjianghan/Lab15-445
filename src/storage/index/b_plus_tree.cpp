//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/index/b_plus_tree.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <string>

#include "common/exception.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) {
  bool root_isLocked = true;
  root_Latch_.lock();
  if (IsEmpty()) {
    root_Latch_.unlock();
    return false;
  }
  // FindLeafPage---------------------------

  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    throw Exception("out of memory1");
  }
  page->RLatch();
  BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  page_id_t child_id;
  while (!node->IsLeafPage()) {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page->GetData());
    child_id = internal->Lookup(key, comparator_);
    page = buffer_pool_manager_->FetchPage(child_id);
    if (page == nullptr) {
      throw Exception("out of memory1.2");
    }
    page->RLatch();
    if (root_isLocked) {
      root_isLocked = false;
      root_Latch_.unlock();
    }
    reinterpret_cast<Page *>(internal)->RUnlatch();
    buffer_pool_manager_->UnpinPage(internal->GetPageId(), false);
    node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  // FindLeafPage---------------------------

  LeafPage *leaf = reinterpret_cast<LeafPage *>(page->GetData());
  ValueType v;
  bool res = false;
  if (leaf->Lookup(key, &v, comparator_)) {
    result->push_back(v);
    res = true;
  }
  if (root_isLocked) {
    root_Latch_.unlock();
  }
  page->RUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
  return res;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) {
  root_Latch_.lock();
  if (IsEmpty()) {
    StartNewTree(key, value);
    root_Latch_.unlock();
    return true;
  }
  return InsertIntoLeaf(key, value, transaction);
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  page_id_t pid;
  Page *page = buffer_pool_manager_->NewPage(&pid);
  if (page == nullptr) {
    throw Exception("out of memory1");
  }
  LeafPage *leaf = reinterpret_cast<LeafPage *>(page->GetData());
  leaf->Init(pid, INVALID_PAGE_ID, leaf_max_size_);
  leaf->Insert(key, value, comparator_);
  root_page_id_ = pid;
  buffer_pool_manager_->UnpinPage(pid, true);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immdiately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction) {
  bool root_isLocked = true;
  // FindLeafPage---------------------------
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    throw Exception("out of memory2");
  }
  page->WLatch();
  transaction->AddIntoPageSet(page);
  BPlusTreePage *tnode = reinterpret_cast<BPlusTreePage *>(page->GetData());
  page_id_t child_id;
  while (!tnode->IsLeafPage()) {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page->GetData());
    child_id = internal->Lookup(key, comparator_);
    page = buffer_pool_manager_->FetchPage(child_id);
    if (page == nullptr) {
      throw Exception("out of memory3");
    }
    page->WLatch();
    tnode = reinterpret_cast<BPlusTreePage *>(page->GetData());
    // IsSafe
    if (tnode->GetSize() < tnode->GetMaxSize() - 1) {
      auto PageSet = transaction->GetPageSet();
      while (!PageSet->empty()) {
        auto Locked_Page = PageSet->front();
        Locked_Page->WUnlatch();
        if (Locked_Page->GetPageId() == root_page_id_) {
          root_isLocked = false;
          root_Latch_.unlock();
        }
        PageSet->pop_front();
        buffer_pool_manager_->UnpinPage(Locked_Page->GetPageId(), false);
      }
    }
    transaction->AddIntoPageSet(page);
  }
  // FindLeafPage---------------------------

  LeafPage *node = reinterpret_cast<LeafPage *>(page->GetData());
  ValueType v;
  bool res = true;
  // 如果存在，直接返回
  if (node->Lookup(key, &v, comparator_)) {
    res = false;
  } else {
    int size = node->Insert(key, value, comparator_);
    if (size >= node->GetMaxSize()) {
      LeafPage *recipient = Split(node);
      InsertIntoParent(node, recipient->KeyAt(0), recipient, transaction);
      buffer_pool_manager_->UnpinPage(recipient->GetPageId(), true);
    }
  }
  auto PageSet = transaction->GetPageSet();
  while (!PageSet->empty()) {
    auto Locked_Page = PageSet->front();
    PageSet->pop_front();
    Locked_Page->WUnlatch();
    buffer_pool_manager_->UnpinPage(Locked_Page->GetPageId(), res);
  }
  if (root_isLocked) {
    root_Latch_.unlock();
  }
  return res;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
N *BPLUSTREE_TYPE::Split(N *node) {
  page_id_t pid;
  Page *page = buffer_pool_manager_->NewPage(&pid);
  if (page == nullptr) {
    throw Exception("out of memory4");
  }
  N *recipient = reinterpret_cast<N *>(page->GetData());

  if (node->IsLeafPage()) {
    reinterpret_cast<LeafPage *>(recipient)->Init(pid, reinterpret_cast<LeafPage *>(node)->GetParentPageId(),
                                                  leaf_max_size_);
    recipient->SetParentPageId(node->GetParentPageId());
    reinterpret_cast<LeafPage *>(node)->MoveHalfTo(reinterpret_cast<LeafPage *>(recipient));
  } else {
    reinterpret_cast<InternalPage *>(recipient)->Init(pid, reinterpret_cast<InternalPage *>(node)->GetParentPageId(),
                                                      internal_max_size_);
    recipient->SetParentPageId(node->GetParentPageId());
    reinterpret_cast<InternalPage *>(node)->MoveHalfTo(reinterpret_cast<InternalPage *>(recipient),
                                                       buffer_pool_manager_);
  }
  return recipient;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,
                                      Transaction *transaction) {
  if (old_node->IsRootPage()) {
    page_id_t ppid;
    Page *page = buffer_pool_manager_->NewPage(&ppid);
    if (page == nullptr) {
      throw Exception("out of memory4");
    }
    InternalPage *parent = reinterpret_cast<InternalPage *>(page->GetData());
    parent->Init(ppid, INVALID_PAGE_ID, internal_max_size_);
    parent->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
    old_node->SetParentPageId(ppid);
    new_node->SetParentPageId(ppid);
    root_page_id_ = ppid;
    buffer_pool_manager_->UnpinPage(ppid, true);
  } else {
    page_id_t ppid = old_node->GetParentPageId();
    Page *page = buffer_pool_manager_->FetchPage(ppid);
    if (page == nullptr) {
      throw Exception("out of memory5");
    }
    InternalPage *parent = reinterpret_cast<InternalPage *>(page->GetData());
    int size = parent->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    if (size >= internal_max_size_) {
      new_node = Split<InternalPage>(parent);
      InsertIntoParent(parent, reinterpret_cast<InternalPage *>(new_node)->KeyAt(0), new_node, transaction);
      buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);
    }
    buffer_pool_manager_->UnpinPage(old_node->GetPageId(), true);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if (root_page_id_ == INVALID_PAGE_ID) {
    return;
  }

  // FindLeafPage---------------------------
  root_Latch_.lock();
  bool root_isLocked = true;
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    throw Exception("out of memory5");
  }
  page->WLatch();
  transaction->AddIntoPageSet(page);
  BPlusTreePage *tnode = reinterpret_cast<BPlusTreePage *>(page->GetData());
  page_id_t child_id;
  while (!tnode->IsLeafPage()) {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page->GetData());
    child_id = internal->Lookup(key, comparator_);
    // 获取了子节点的id就释放父亲节点的id
    page = buffer_pool_manager_->FetchPage(child_id);
    if (page == nullptr) {
      throw Exception("out of memory6");
    }
    page->WLatch();
    tnode = reinterpret_cast<BPlusTreePage *>(page->GetData());
    // IsSafe
    if (tnode->GetSize() > tnode->GetMinSize()) {
      auto PageSet = transaction->GetPageSet();
      while (!PageSet->empty()) {
        auto Locked_Page = PageSet->front();
        PageSet->pop_front();
        Locked_Page->WUnlatch();
        if (Locked_Page->GetPageId() == root_page_id_) {
          root_isLocked = false;
          root_Latch_.unlock();
        }
        buffer_pool_manager_->UnpinPage(Locked_Page->GetPageId(), false);
      }
    }
    transaction->AddIntoPageSet(page);
  }
  // FindLeafPage---------------------------

  LeafPage *node = reinterpret_cast<LeafPage *>(page->GetData());
  int size = node->RemoveAndDeleteRecord(key, comparator_);
  if (size < node->GetMinSize()) {
    CoalesceOrRedistribute(node, transaction);
  }

  auto PageSet = transaction->GetPageSet();
  auto DeletePageSet = transaction->GetDeletedPageSet();

  while (!PageSet->empty()) {
    auto Locked_Page = PageSet->front();
    auto Locked_PageId = Locked_Page->GetPageId();
    PageSet->pop_front();
    Locked_Page->WUnlatch();
    buffer_pool_manager_->UnpinPage(Locked_Page->GetPageId(), true);
    if (DeletePageSet->count(Locked_PageId) != 0) {
      buffer_pool_manager_->DeletePage(Locked_PageId);
    }
  }
  if (root_isLocked) {
    root_Latch_.unlock();
  }
}

/*
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) {
  if (node->IsRootPage()) {
    bool res = false;
    if (AdjustRoot(node)) {
      transaction->AddIntoDeletedPageSet(node->GetPageId());
      res = true;
    }
    // 如果是根节点
    return res;
  }
  // 父节点id，获取该节点id
  page_id_t ppid = node->GetParentPageId();
  page_id_t pid = node->GetPageId();
  page_id_t neighbor_id;
  Page *page = buffer_pool_manager_->FetchPage(ppid);
  if (page == nullptr) {
    throw Exception("out of memory7");
  }
  InternalPage *parent = reinterpret_cast<InternalPage *>(page->GetData());
  // index是node在parent的索引值，如果node在父节点的索引是0，则sibling是1，其他情况下都是node左侧的节点。
  int index = parent->ValueIndex(pid);
  if (index == 0) {
    neighbor_id = parent->ValueAt(1);
  } else {
    neighbor_id = parent->ValueAt(index - 1);
  }
  Page *neighbor_page = buffer_pool_manager_->FetchPage(neighbor_id);
  if (neighbor_page == nullptr) {
    throw Exception("out of memory8");
  }
  // 判断是否需要Redistribute还是Coalesce
  N *neighbor_node = reinterpret_cast<N *>(neighbor_page->GetData());
  reinterpret_cast<Page *>(neighbor_node)->WLatch();
  if (node->GetSize() + neighbor_node->GetSize() >= node->GetMaxSize()) {
    Redistribute(neighbor_node, node, index);
    reinterpret_cast<Page *>(neighbor_node)->WUnlatch();
    buffer_pool_manager_->UnpinPage(ppid, true);
    buffer_pool_manager_->UnpinPage(neighbor_id, true);
    return false;
  }

  Coalesce(&neighbor_node, &node, &parent, index, transaction);
  reinterpret_cast<Page *>(neighbor_node)->WUnlatch();
  buffer_pool_manager_->UnpinPage(ppid, true);
  buffer_pool_manager_->UnpinPage(neighbor_id, true);
  return true;
  // 这几个释放逻辑要再想想
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::Coalesce(N **neighbor_node, N **node,
                              BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> **parent, int index,
                              Transaction *transaction) {
  if (index == 0) {
    N **tmp;
    tmp = neighbor_node;
    neighbor_node = node;
    node = tmp;
    index = 1;
  }

  if ((*node)->IsLeafPage()) {
    reinterpret_cast<LeafPage *>((*node))->MoveAllTo(reinterpret_cast<LeafPage *>((*neighbor_node)));
    reinterpret_cast<InternalPage *>((*parent))->Remove(index);
  } else {
    KeyType middle_key = reinterpret_cast<InternalPage *>((*parent))->KeyAt(index);
    reinterpret_cast<InternalPage *>((*node))->MoveAllTo(reinterpret_cast<InternalPage *>((*neighbor_node)), middle_key,
                                                         buffer_pool_manager_);
    reinterpret_cast<InternalPage *>((*parent))->Remove(index);
  }
  transaction->AddIntoDeletedPageSet(reinterpret_cast<Page *>(*node)->GetPageId());
  if ((*parent)->GetSize() < (*parent)->GetMinSize()) {
    return CoalesceOrRedistribute((*parent), transaction);
  }
  return false;
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node, int index) {
  // index是node在parent中的索引
  Page *page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  if (page == nullptr) {
    throw Exception("out of memory 472");
  }
  InternalPage *parent_node = reinterpret_cast<InternalPage *>(page->GetData());
  if (index == 0) {
    if (node->IsLeafPage()) {
      reinterpret_cast<LeafPage *>((neighbor_node))->MoveFirstToEndOf(reinterpret_cast<LeafPage *>(node));
    }
    if (!node->IsLeafPage()) {
      reinterpret_cast<InternalPage *>(neighbor_node)
          ->MoveFirstToEndOf(reinterpret_cast<InternalPage *>(node),
                             reinterpret_cast<InternalPage *>(neighbor_node)->KeyAt(1), buffer_pool_manager_);
    }
    parent_node->SetKeyAt(index + 1, reinterpret_cast<LeafPage *>((neighbor_node))->KeyAt(0));
  }
  if (index != 0) {
    if (node->IsLeafPage()) {
      reinterpret_cast<LeafPage *>(neighbor_node)->MoveLastToFrontOf(reinterpret_cast<LeafPage *>(node));
    }
    if (!node->IsLeafPage()) {
      int neighbor_size = reinterpret_cast<InternalPage *>(neighbor_node)->GetSize();
      reinterpret_cast<InternalPage *>(neighbor_node)
          ->MoveLastToFrontOf(reinterpret_cast<InternalPage *>(node), neighbor_node->KeyAt(neighbor_size - 1),
                              buffer_pool_manager_);
    }
    parent_node->SetKeyAt(index, reinterpret_cast<LeafPage *>(node)->KeyAt(0));
  }
  buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  // case1:最后b+树剩下两层所支持最少的节点数，此时删除任意一个节点会导致根节点删除，根节点会转换
  // 为其子节点所合成的节点
  // case2：表示所有的节点都没了，这个时候删除整个b+树
  // 感觉永远也不会返回false

  // case 2
  if (old_root_node->GetSize() == 0 && old_root_node->IsLeafPage()) {
    root_page_id_ = INVALID_PAGE_ID;
    return true;
  }
  if (old_root_node->GetSize() == 1 && !old_root_node->IsLeafPage()) {
    // 这里只会从右往左合并，所以应该判断左边
    InternalPage *node = reinterpret_cast<InternalPage *>(old_root_node);
    page_id_t new_root_id = node->RemoveAndReturnOnlyChild();
    InternalPage *new_root_node =
        reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(new_root_id)->GetData());
    if (new_root_node == nullptr) {
      throw Exception("out of memory527");
    }
    new_root_node->SetParentPageId(INVALID_PAGE_ID);
    root_page_id_ = new_root_id;
    buffer_pool_manager_->UnpinPage(new_root_id, true);
    //    UpdateRootPageId(false);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leaftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::begin() {
  Page *page = FindLeafPage(KeyType{}, true);
  INDEXITERATOR_TYPE iterator =
      IndexIterator<KeyType, ValueType, KeyComparator>(page->GetPageId(), 0, buffer_pool_manager_);
  buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
  return iterator;
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) {
  Page *page = FindLeafPage(key, false);
  LeafPage *node = reinterpret_cast<LeafPage *>(page->GetData());
  INDEXITERATOR_TYPE iterator = IndexIterator<KeyType, ValueType, KeyComparator>(
      page->GetPageId(), node->KeyIndex(key, comparator_), buffer_pool_manager_);
  buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
  return iterator;
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::end() {
  //这里实现的是从根节点一直取最右边的节点，也可以实现从叶节点一个一个跑到最后一个节点
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  page_id_t child_id;
  while (!node->IsLeafPage()) {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page->GetData());
    child_id = internal->ValueAt(internal->GetSize() - 1);
    page = buffer_pool_manager_->FetchPage(child_id);
    buffer_pool_manager_->UnpinPage(internal->GetPageId(), false);
    node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  INDEXITERATOR_TYPE iterator =
      IndexIterator<KeyType, ValueType, KeyComparator>(page->GetPageId(), node->GetSize(), buffer_pool_manager_);
  buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
  return iterator;
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 */
INDEX_TEMPLATE_ARGUMENTS
Page *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, bool leftMost) {
  // (+)
  bool root_isLocked = true;
  root_Latch_.lock();
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    return nullptr;
  }
  page->RLatch();
  BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  page_id_t child_id;

  while (!node->IsLeafPage()) {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page->GetData());
    if (leftMost) {
      child_id = internal->ValueAt(0);
    } else {
      child_id = internal->Lookup(key, comparator_);
    }
    // 获取了子节点的id就释放父亲节点的id
    page = buffer_pool_manager_->FetchPage(child_id);
    if (page == nullptr) {
      return nullptr;
    }
    page->RLatch();
    if (root_isLocked) {
      root_Latch_.unlock();
    }
    reinterpret_cast<Page *>(internal)->RUnlatch();
    buffer_pool_manager_->UnpinPage(internal->GetPageId(), false);
    node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  //  buffer_pool_manager_->UnpinPage(page->GetPageId(),false);
  // 注意：这里调用这个函数后返回的page仍然让在pin中的,所以要在之后的逻辑进行释放
  if (root_isLocked) {
    root_Latch_.unlock();
  }
  page->RUnlatch();
  return page;
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  HeaderPage *header_page = static_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't  need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    LeafPage *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    InternalPage *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    LeafPage *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
