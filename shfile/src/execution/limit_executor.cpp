//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// limit_executor.cpp
//
// Identification: src/execution/limit_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/limit_executor.h"

namespace bustub {

LimitExecutor::LimitExecutor(ExecutorContext *exec_ctx, const LimitPlanNode *plan,
                             std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_executor_(std::move(child_executor)),
      limit_(plan->GetLimit()),
      offset_(plan->GetOffset()),
      idx(0) {}

void LimitExecutor::Init() {
  child_executor_->Init();
  idx = 0;
}

bool LimitExecutor::Next(Tuple *tuple, RID *rid) {
  while (idx < offset_) {
    child_executor_->Next(tuple, rid);
  }
  if (idx++ - offset_ < limit_) {
    return child_executor_->Next(tuple, rid);
  }
  return false;
}
}  // namespace bustub
