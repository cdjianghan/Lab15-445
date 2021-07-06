//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// delete_executor.cpp
//
// Identification: src/execution/delete_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>

#include "execution/executors/delete_executor.h"

namespace bustub {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      table_meta_(exec_ctx->GetCatalog()->GetTable(plan_->TableOid())),
      table_indexes_(exec_ctx->GetCatalog()->GetTableIndexes(table_meta_->name_)) {}

void DeleteExecutor::Init() {}

bool DeleteExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) {
  if (child_executor_->Next(tuple, rid)) {
    if (table_meta_->table_->MarkDelete(*rid, exec_ctx_->GetTransaction())) {
      // delete the index
      for (auto index_info : table_indexes_) {
        auto index = index_info->index_.get();
        Tuple target_tuple(tuple->KeyFromTuple(table_meta_->schema_, index_info->key_schema_, index->GetKeyAttrs()));
        index->DeleteEntry(target_tuple, *rid, exec_ctx_->GetTransaction());
      }
      return true;
    }
  }
  return false;
}

}  // namespace bustub
