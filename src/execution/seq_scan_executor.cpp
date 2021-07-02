//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// seq_scan_executor.cpp
//
// Identification: src/execution/seq_scan_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "execution/executors/seq_scan_executor.h"

namespace bustub {

SeqScanExecutor::SeqScanExecutor(ExecutorContext *exec_ctx, const SeqScanPlanNode *plan)
        : AbstractExecutor(exec_ctx),
          plan_{plan},
          table_meta_{exec_ctx->GetCatalog()->GetTable(plan->GetTableOid())},
          iter_(exec_ctx->GetCatalog()->GetTable(plan->GetTableOid())->table_->Begin(exec_ctx->GetTransaction())),
          end_iter_(exec_ctx->GetCatalog()->GetTable(plan->GetTableOid())->table_->End()),
          predicate_{plan->GetPredicate()} {}

void SeqScanExecutor::Init() {
    iter_ = table_meta_->table_->Begin(exec_ctx_->GetTransaction());
    end_iter_ = table_meta_->table_->End();}

bool SeqScanExecutor::Next(Tuple *tuple, RID *rid) {
    while (iter_ != end_iter_) {
        *rid = iter_->GetRid();
        *tuple = *iter_;
        ++iter_;
        if (nullptr == predicate_ || predicate_->Evaluate(tuple, &table_meta_->schema_).GetAs<bool>()) {
            const Schema *output_scheme = plan_->OutputSchema();
            std::vector<Value> values(output_scheme->GetColumnCount());
            const auto &output_columns = output_scheme->GetColumns();
            for (size_t i = 0; i < values.size(); ++i) {
                values[i] = output_columns[i].GetExpr()->Evaluate(tuple, &table_meta_->schema_);
            }
            *tuple = Tuple(values, output_scheme);
            return true;
        }
    }
    return false;
}

}  // namespace bustub
