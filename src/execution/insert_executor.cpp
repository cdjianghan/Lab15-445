//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>

#include "execution/executors/insert_executor.h"

namespace bustub {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),plan_(plan),
    table_meta_(exec_ctx_-> GetCatalog()->GetTable(plan_->TableOid())),
    child_executor_(std::move(child_executor)),
    table_indexes_(exec_ctx_ -> GetCatalog()->GetTableIndexes(table_meta_->name_)){}

void InsertExecutor::Init() {
    if (child_executor_ != nullptr) {
        child_executor_->Init();
    }
}

bool InsertExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) {

    if (plan_->IsRawInsert()) {
        std::vector<std::vector<Value>> raw_values = plan_->RawValues();
        for(const auto& value :raw_values){
            *tuple = Tuple(value , &table_meta_->schema_);
            // start insert
            if (table_meta_->table_->InsertTuple(*tuple,rid,exec_ctx_->GetTransaction())) {
                // to add entry to every index with table
                for ( auto index_info : table_indexes_) {
                    auto index = index_info->index_.get();
                    Tuple key(tuple->KeyFromTuple(table_meta_->schema_, index_info->key_schema_, index->GetKeyAttrs()));
                    index->InsertEntry(key, *rid, exec_ctx_->GetTransaction());
                }
            }else{
                throw Exception("INSERT, tuple to be inserted is bigger than a page");
            }

        }
        return false;
    }
    while (child_executor_->Next(tuple,rid)) {
        // if there child_exector is not nullptr
        if (table_meta_->table_->InsertTuple(*tuple,rid,exec_ctx_->GetTransaction())) {
            // to add entry to every index with table
            for ( auto index_info : table_indexes_) {
                auto index = index_info->index_.get();
                Tuple key(tuple->KeyFromTuple(table_meta_->schema_, index_info->key_schema_, index->GetKeyAttrs()));
                index->InsertEntry(key, *rid, exec_ctx_->GetTransaction());
            }
        }else{
            throw Exception("INSERT, tuple to be inserted is bigger than a page");
        }
    }
    return false;

}

}  // namespace bustub
