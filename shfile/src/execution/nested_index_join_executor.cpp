//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_index_join_executor.cpp
//
// Identification: src/execution/nested_index_join_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_index_join_executor.h"
#include <execution/expressions/column_value_expression.h>

namespace bustub {

NestIndexJoinExecutor::NestIndexJoinExecutor(ExecutorContext *exec_ctx, const NestedIndexJoinPlanNode *plan,
                                             std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor(std::move(child_executor)) {}

void NestIndexJoinExecutor::Init() {
  inner_table_meta_ = exec_ctx_->GetCatalog()->GetTable(plan_->GetInnerTableOid());
  inner_table_index = exec_ctx_->GetCatalog()->GetIndex(plan_->GetIndexName(), inner_table_meta_->name_);
  if (child_executor != nullptr) {
    child_executor->Init();
  }
  auto child_predicate = plan_->Predicate()->GetChildren();
  auto out_predicate = dynamic_cast<const ColumnValueExpression *>(child_predicate[0]);
  idx = out_predicate->GetColIdx();
}

bool NestIndexJoinExecutor::Next(Tuple *tuple, RID *rid) {
  auto *outer_table_schema = plan_->OuterTableSchema();
  auto *inner_table_schema = plan_->InnerTableSchema();
  auto output_schema = plan_->OutputSchema();
  while (true) {
    if (child_executor->Next(tuple, rid)) {
      Tuple Key({tuple->GetValue(plan_->OuterTableSchema(), idx)}, &inner_table_index->key_schema_);
      std::vector<RID> result;
      inner_table_index->index_->ScanKey(Key, &result, exec_ctx_->GetTransaction());
      if (!result.empty()) {
        Tuple inner_Tuple;
        inner_table_meta_->table_->GetTuple(result[0], &inner_Tuple, exec_ctx_->GetTransaction());
        std::vector<Value> value(output_schema->GetColumnCount());
        auto output_columns = output_schema->GetColumns();
        for (size_t i = 0; i < value.size(); i++) {
          value[i] =
              output_columns[i].GetExpr()->EvaluateJoin(tuple, outer_table_schema, &inner_Tuple, inner_table_schema);
        }
        *tuple = Tuple(value, output_schema);
        return true;
      }
      continue;
    }
    return false;
  }
}

}  // namespace bustub
