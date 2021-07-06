//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"

namespace bustub {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx), left_executor(std::move(left_executor)), right_executor(std::move(right_executor)) {}

void NestedLoopJoinExecutor::Init() {
  left_executor->Init();
  right_executor->Init();
  left_end = false;
  right_end = true;
}

bool NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) {
  auto *left_schema = plan_->GetLeftPlan()->OutputSchema();
  auto *right_schema = plan_->GetRightPlan()->OutputSchema();
  auto *output_schema = plan_->OutputSchema();
  auto predicate = plan_->Predicate();
  Tuple left_tuple;
  RID left_rid;
  while (left_executor->Next(&left_tuple, &left_rid)) {
    Tuple right_tuple;
    RID right_rid;
    while (right_executor->Next(&right_tuple, &right_rid)) {
      if (predicate == nullptr ||
          predicate->EvaluateJoin(&left_tuple, left_schema, &right_tuple, right_schema).GetAs<bool>()) {
        std::vector<Value> values(output_schema->GetColumnCount());
        const auto &output_columns = output_schema->GetColumns();
        for (size_t i = 0; i < values.size(); i++) {
          values[i] = output_columns[i].GetExpr()->EvaluateJoin(&left_tuple, left_schema, &right_tuple, right_schema);
        }
        *tuple = Tuple(values, output_schema);
        return true;
      }
    }
  }
  return false;
}

}  // namespace bustub
