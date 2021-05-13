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
  Page *page = FindLeafPage(key, false);
  LeafPage *leaf = reinterpret_cast<LeafPage *>(page->GetData());
  ValueType v;
  if (leaf->Lookup(key, &v, comparator_)) {
    result->push_back(v);
    return true;
  }
  return false;
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
  if (root_page_id_ == INVALID_PAGE_ID) {
    StartNewTree(key, value);
    return true;
  }
  if (InsertIntoLeaf(key, value, transaction)) {
    return true;
  }
  return false;
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
    throw Exception("out of memory");}
    root_page_id_ = pid;
    LeafPage *leaf = reinterpret_cast<LeafPage *>(page->GetData());
    leaf->Init(pid, INVALID_PAGE_ID, leaf_max_size_);
    leaf->Insert(key, value, comparator_);

    buffer_pool_manager_->UnpinPage(pid, true);
    UpdateRootPageId(true);
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
  Page *page = FindLeafPage(key, false);
  if (page == nullptr) {
    buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    throw Exception("out of memory");
  }
  LeafPage *leaf = reinterpret_cast<LeafPage *>(page->GetData());

  ValueType v;
  if (leaf->Lookup(key, &v, comparator_)) {
    buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    return false;
  }

  int size = leaf->Insert(key, value, comparator_);
  if (size >= leaf->GetMaxSize()) {
    LeafPage *recipient = Split(leaf);
    KeyType ikey = recipient->array[1].first;
    //这里选array[1]的key是因为array[0]的key非法
    InsertIntoParent(leaf, ikey, recipient, transaction);
    buffer_pool_manager_->UnpinPage(leaf->GetPageId());
    buffer_pool_manager_->UnpinPage(recipient->GetPageId());
  }
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
    throw Exception("out of memory");
  }
  N *recipient = reinterpret_cast<N *>(page->GetData());
  recipient->Init(pid, node->GetParentPageId(), node->GetMaxSize());
  if (recipient.IsLeafPage()) {
    node.MoveHalfTo(recipient);
  }
  if (!recipient.IsLeafPage()) {
    node.MoveHalfTo(recipient, buffer_pool_manager_);
  }
  return recipient;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key           这里我的实现是将new_node的最左边的那个key传过来
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,
                                      Transaction *transaction) {
  //首先需要分old_node是否是根节点的情况
  if (old_node->GetPageId() == root_page_id_) {
    //当需要分裂的节点是根节点时，重新从buffer_manager中获取一个页，将其作为父节点
    page_id_t ppid;
    Page *page = buffer_pool_manager_->NewPage(&ppid);
    if (page == nullptr) {
      throw Exception("out of memory");
    }
    InternalPage *parent = reinterpret_cast<InternalPage *>(page->GetData());

    parent->Init(ppid, INVALID_PAGE_ID, internal_max_size_);
    //更新两个分裂节点的父节点为新产生的那个节点
    old_node->SetParentPageId(ppid);
    new_node->SetParentPageId(ppid);
    root_page_id_ = ppid;
    UpdateRootPageId(true);
    parent->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());

  } else {
    //当需要分裂的节点不是根节点时，直接获取父节点
    page_id_t ppid = old_node->GetParentPageId();
    Page *page = buffer_pool_manager_->FetchPage(ppid);
    if (page == nullptr) {
      buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
      throw Exception("out of memory");
    }
    InternalPage *parent = reinterpret_cast<InternalPage *>(page->GetData());
    int size = parent->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    if (size >= internal_max_size_) {
      new_node = Split(parent);
      InsertIntoParent(parent, new_node->array[1].first, new_node, transaction);
    }
  }

  buffer_pool_manager_->UnpinPage(ppid, true);
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
  Page *page = FindLeafPage(key, false);
  LeafPage *leaf = reinterpret_cast<LeafPage *>(page->GetData());
  int size = leaf->RemoveAndDeleteRecord(key, comparator_);
  //接下来应该需要加个一个判断来判断怎么处理；
  if (size < leaf->GetMinSize()) {
    CoalesceOrRedistribute(leaf, transaction);
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
  //这里实验要求其实是简化了的，要求如果node节点在父节点中定位为0则取右边为sibling，其他情况都是取左边为sibling

  if (node->IsRootPage()) {
    return AdjustRoot(node);
  }
  if (node->GetSize() > node->GetMinSize() && !node->IsRootPage()) {
    return false;
  }

  //获取sibling节点
  page_id_t ppid = node.GetParentPageId();
  page_id_t pid = node.GetPageId();
  page_id_t neighbor_id;
  Page *page = buffer_pool_manager_->FetchPage(ppid);
  if (page == nullptr) {
    throw Exception("all page are pinned while FindLeafPage");
  }
  InternalPage *parent = reinterpret_cast<InternalPage *>(page->GetData());
  int index = parent->ValueIndex(pid);
  if (index == 0) {
    neighbor_id = parent->ValueAt(1);
  } else {
    neighbor_id = parent->ValueAt(index - 1);
  }
  Page *neighbor_page = buffer_pool_manager_->FetchPage(neighbor_id);
  //这里这个neighbor_id好像有问题，vscode报错
  if (page == nullptr) {
    throw Exception("all page are pinned while FindLeafPage");
  }

  //判断是否需要Redistribute还是Coalesce
  N *neighbor_node = reinterpret_cast<N *>(neighbor_page->GetData());

  if (node.GetSize() + neighbor_node.GetSize() > node.GetMaxSize()) {
    Redistribute(neighbor_node, node, index);
    return false;
  }
  Coalesce(&neighbor_node, &node, &parent, index, transaction);
  return true;

  //这几个释放逻辑要再想想
  buffer_pool_manager_->UnpinPage(ppid, true);
  buffer_pool_manager_->UnpinPage(neighbor_id, true);
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
  //这里有点矛盾感觉没想明白sibling和node在调用MoveAllTo中的情况
  //还有这里为什么要用双指针
  // index！=0的情况下，sibling在左边，所以是从右往左进行Merge，但是当index==0时，sibling在右边，
  //这里没交代明白怎么合并。只有当index为零的时候交换一下node 和neighbor_node的角色了

  //只能从右边向左边合并，如果index为0的情况的话，就交换。
  if (index == 0) {
    N **tmp;
    tmp = neighbor_node;
    neighbor_node = node;
    node = tmp;
    index = 1;
  }

  if (*node->IsLeafPage()) {
    *node->MoveAllTo(*neighbor_node);
    *parent->Remove(index);
  }
  if (!*node->IsLeafPage()) {
    KeyType middle_key = *parent->KeyAt(index);

    *node->MoveAllTo(*neighbor_node, middle_key, buffer_pool_manager_);
  }
  buffer_pool_manager_->DeletePage(*node->GetPageId());

  if (*parent->GetSize() < *parent->GetMinSize()) {
    return CoalesceOrRedistribute(*parent, transaction);
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
  if (index == 0) {
    if (node.IsLeafPage()) {
      neighbor_node.MoveFirstToEndOf(node);
    }
    if (!node.IsLeafPage()) {
      neighbor_node.MoveFirstToEndOf(node, neighbor_node->KeyAt(1), buffer_pool_manager_);
    }
  }
  if (index != 0) {
    if (node.IsLeafPage()) {
      neighbor_node.MoveLastToFrontOf(node);
    }
    if (!node.IsLeafPage()) {
      int neighbor_size = neighbor_node.GetSize();
      neighbor_node.MoveLastToFrontOf(node, neighbor_node->KeyAt(neighbor_size - 1), buffer_pool_manager_);
    }
  }
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
  // case 2
  if (old_root_node->IsLeafPage()) {
    if (old_root_node->GetSize() == 0) {
      root_page_id_ = INVALID_PAGE_ID;
      UpdateRootPageId(false);
      return true;
    }
    return false;
  }
  // case 1
  if (old_root_node->GetSize() == 1) {
    Page *page = buffer_pool_manager_->FetchPage(old_root_node->GetPageId());
    page_id_t rpid = old_root_node->GetPageId();
    //这里只会从右往左合并，所以应该判断左边
    InternalPage *node = reinterpret_cast<InternalPage *>(page->GetData());
    page_id_t new_root_id = node->ValueAt(0);
    page = buffer_pool_manager_->FetchPage(new_root_id);
    InternalPage *new_root_node = reinterpret_cast<InternalPage *>(page->GetData());
    new_root_node->SetParentPageId(INVALID_PAGE_ID);
    root_page_id_ = new_root_id;
    UpdateRootPageId(false);
  }
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
INDEXITERATOR_TYPE BPLUSTREE_TYPE::begin() { return INDEXITERATOR_TYPE(); }

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) { return INDEXITERATOR_TYPE(); }

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::end() { return INDEXITERATOR_TYPE(); }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 */
INDEX_TEMPLATE_ARGUMENTS
Page *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, bool leftMost) {
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  if (page == nullptr) {
    throw Exception(MISMATCH_TYPE, "all page are pinned while FindLeafPage");
  }
  BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  page_id_t child_id;

  while (!node->IsLeafPage()) {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page->GetData());

    if (leftMost) {
      child_id = internal->array[0].second;
    } else {
      child_id = internal->Lookup(key, comparator_);
    }
    buffer_pool_manager_->UnpinPage(internal->GetPageId);
    page = buffer_pool_manager_->FetchPage(child_id);
    if (page == nullptr) {
      throw Exception(MISMATCH_TYPE, "all page are pinned while FindLeafPage");
    }
    BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  //注意：这里调用这个函数后返回的page仍然让在bufferpool中的
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
