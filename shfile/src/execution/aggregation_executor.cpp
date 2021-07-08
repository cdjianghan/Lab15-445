//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <vector>

#include "execution/executors/aggregation_executor.h"

namespace bustub {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_(std::move(child)),
      aht_(plan_->GetAggregates(), plan_->GetAggregateTypes()),
      aht_iterator_(aht_.Begin()) {}

const AbstractExecutor *AggregationExecutor::GetChildExecutor() const { return child_.get(); }

void AggregationExecutor::Init() {
  child_->Init();
  aht_iterator_ = aht_.Begin();
  //    Tuple tuple;
  //    RID rid;
}

bool AggregationExecutor::Next(Tuple *tuple, RID *rid) {
  bool first = false;
  while (child_->Next(tuple, rid)) {
    // init the hashtable
    first = true;
    aht_.InsertCombine(MakeKey(tuple), MakeVal(tuple));
  }
  if (first) {
    const auto *having = plan_->GetHaving();
    const Schema *output_schema = plan_->OutputSchema();
    const auto &output_columns = output_schema->GetColumns();
    std::vector<Value> values(output_schema->GetColumnCount());
    for (aht_iterator_ = aht_.Begin(); aht_iterator_ != aht_.End(); ++aht_iterator_) {
      const auto &group_bys = aht_iterator_.Key().group_bys_;
      const auto &aggregates = aht_iterator_.Val().aggregates_;
      if (nullptr == having || having->EvaluateAggregate(group_bys, aggregates).GetAs<bool>()) {
        for (size_t i = 0; i < values.size(); ++i) {
          values[i] = output_columns[i].GetExpr()->EvaluateAggregate(group_bys, aggregates);
        }
        *tuple = Tuple(values, output_schema);
        return true;
      }
    }
    return false;
  }
  return false;
}

}  // namespace bustub
