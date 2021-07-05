//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.cpp
//
// Identification: src/execution/index_scan_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "execution/executors/index_scan_executor.h"

namespace bustub {
IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx),
    predicate_{plan->GetPredicate()},
    index_info_(exec_ctx->GetCatalog()->GetIndex(plan->GetIndexOid())),
    table_meta_(exec_ctx_->GetCatalog()->GetTable(index_info_->table_name_)){}

void IndexScanExecutor::Init() {
    auto *bplustree_index =
            dynamic_cast<BPlusTreeIndex<GenericKey<8>, RID, GenericComparator<8>> *>(index_info_->index_.get());
    iter_ = bplustree_index->GetBeginIterator();

}

bool IndexScanExecutor::Next(Tuple *tuple, RID *rid) {
    while(!iter_.isEnd()){
        *rid = (*iter_).second;
        ++iter_;
        table_meta_->table_->GetTuple(*rid,tuple,exec_ctx_->GetTransaction());
        if( predicate_ == nullptr || predicate_->Evaluate(tuple,&table_meta_->schema_).GetAs<bool>()) {
            const Schema * output_schema = plan_ -> OutputSchema();
            std::vector<Value> values( output_schema -> GetColumnCount());
            auto output_columns = output_schema -> GetColumns();
            for (size_t i = 0; i < values.size(); ++i) {
                values[i] = output_columns[i].GetExpr()->Evaluate(tuple, &table_meta_->schema_);
            }
            *tuple = Tuple(values, output_schema);
            return true;
        }
    }
    return false;

}

}  // namespace bustub
