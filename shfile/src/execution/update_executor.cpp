//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// update_executor.cpp
//
// Identification: src/execution/update_executor.cpp
//
// Copyright (c) 2015-20, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>

#include "execution/executors/update_executor.h"

namespace bustub {

UpdateExecutor::UpdateExecutor(ExecutorContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void UpdateExecutor::Init() {
  table_meta_ = exec_ctx_->GetCatalog()->GetTable(plan_->TableOid());
  table_indexes_ = exec_ctx_->GetCatalog()->GetTableIndexes(table_meta_->name_);
  if (child_executor_ != nullptr) {
    child_executor_->Init();
  }
}

bool UpdateExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) {
  if (child_executor_->Next(tuple, rid)) {
    Tuple n_tuple = GenerateUpdatedTuple(*tuple);
    if (table_meta_->table_->UpdateTuple(n_tuple, *rid, exec_ctx_->GetTransaction())) {
      // update the index
      for (auto index_info : table_indexes_) {
        auto index = index_info->index_.get();
        Tuple old_tuple(tuple->KeyFromTuple(table_meta_->schema_, index_info->key_schema_, index->GetKeyAttrs()));
        Tuple new_tuple(n_tuple.KeyFromTuple(table_meta_->schema_, index_info->key_schema_, index->GetKeyAttrs()));
        index->DeleteEntry(old_tuple, *rid, exec_ctx_->GetTransaction());
        index->InsertEntry(new_tuple, *rid, exec_ctx_->GetTransaction());
      }
      return true;
    }
  }
  return false;
}
}  // namespace bustub
