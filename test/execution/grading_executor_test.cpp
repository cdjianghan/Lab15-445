//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// executor_test.cpp
//
// Identification: test/execution/executor_test.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/table_generator.h"
#include "concurrency/transaction_manager.h"
#include "execution/execution_engine.h"
#include "execution/executor_context.h"
#include "execution/executors/aggregation_executor.h"
#include "execution/executors/insert_executor.h"
#include "execution/executors/nested_loop_join_executor.h"
#include "execution/expressions/aggregate_value_expression.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/delete_plan.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/limit_plan.h"
#include "execution/plans/nested_index_join_plan.h"
#include "execution/plans/seq_scan_plan.h"
#include "execution/plans/update_plan.h"
#include "gtest/gtest.h"
#include "storage/b_plus_tree_test_util.h"  // NOLINT
#include "type/value_factory.h"

namespace bustub {

class GradingExecutorTest : public ::testing::Test {
 public:
  // This function is called before every test.
  void SetUp() override {
    ::testing::Test::SetUp();
    // For each test, we create a new DiskManager, BufferPoolManager, TransactionManager, and Catalog.
    disk_manager_ = std::make_unique<DiskManager>("executor_test.db");
    bpm_ = std::make_unique<BufferPoolManager>(2560, disk_manager_.get());
    page_id_t page_id;
    bpm_->NewPage(&page_id);
    lock_manager_ = std::make_unique<LockManager>();
    txn_mgr_ = std::make_unique<TransactionManager>(lock_manager_.get(), log_manager_.get());
    catalog_ = std::make_unique<Catalog>(bpm_.get(), lock_manager_.get(), log_manager_.get());
    // Begin a new transaction, along with its executor context.
    txn_ = txn_mgr_->Begin();
    exec_ctx_ =
        std::make_unique<ExecutorContext>(txn_, catalog_.get(), bpm_.get(), txn_mgr_.get(), lock_manager_.get());
    // Generate some test tables.
    TableGenerator gen{exec_ctx_.get()};
    gen.GenerateTestTables();

    execution_engine_ = std::make_unique<ExecutionEngine>(bpm_.get(), txn_mgr_.get(), catalog_.get());
  }

  // This function is called after every test.
  void TearDown() override {
    // Commit our transaction.
    txn_mgr_->Commit(txn_);
    // Shut down the disk manager and clean up the transaction.
    disk_manager_->ShutDown();
    remove("executor_test.db");
    delete txn_;
  };

  /** @return the executor context in our test class */
  ExecutorContext *GetExecutorContext() { return exec_ctx_.get(); }
  ExecutionEngine *GetExecutionEngine() { return execution_engine_.get(); }
  Transaction *GetTxn() { return txn_; }
  TransactionManager *GetTxnManager() { return txn_mgr_.get(); }
  Catalog *GetCatalog() { return catalog_.get(); }
  BufferPoolManager *GetBPM() { return bpm_.get(); }
  LockManager *GetLockManager() { return lock_manager_.get(); }

  // The below helper functions are useful for testing.

  const AbstractExpression *MakeColumnValueExpression(const Schema &schema, uint32_t tuple_idx,
                                                      const std::string &col_name) {
    uint32_t col_idx = schema.GetColIdx(col_name);
    auto col_type = schema.GetColumn(col_idx).GetType();
    allocated_exprs_.emplace_back(std::make_unique<ColumnValueExpression>(tuple_idx, col_idx, col_type));
    return allocated_exprs_.back().get();
  }

  const AbstractExpression *MakeConstantValueExpression(const Value &val) {
    allocated_exprs_.emplace_back(std::make_unique<ConstantValueExpression>(val));
    return allocated_exprs_.back().get();
  }

  const AbstractExpression *MakeComparisonExpression(const AbstractExpression *lhs, const AbstractExpression *rhs,
                                                     ComparisonType comp_type) {
    allocated_exprs_.emplace_back(std::make_unique<ComparisonExpression>(lhs, rhs, comp_type));
    return allocated_exprs_.back().get();
  }

  const AbstractExpression *MakeAggregateValueExpression(bool is_group_by_term, uint32_t term_idx) {
    allocated_exprs_.emplace_back(
        std::make_unique<AggregateValueExpression>(is_group_by_term, term_idx, TypeId::INTEGER));
    return allocated_exprs_.back().get();
  }

  const Schema *MakeOutputSchema(const std::vector<std::pair<std::string, const AbstractExpression *>> &exprs) {
    std::vector<Column> cols;
    cols.reserve(exprs.size());
    for (const auto &input : exprs) {
      if (input.second->GetReturnType() != TypeId::VARCHAR) {
        cols.emplace_back(input.first, input.second->GetReturnType(), input.second);
      } else {
        cols.emplace_back(input.first, input.second->GetReturnType(), MAX_VARCHAR_SIZE, input.second);
      }
    }
    allocated_output_schemas_.emplace_back(std::make_unique<Schema>(cols));
    return allocated_output_schemas_.back().get();
  }

 private:
  std::unique_ptr<TransactionManager> txn_mgr_;
  Transaction *txn_{nullptr};
  std::unique_ptr<DiskManager> disk_manager_;
  std::unique_ptr<LogManager> log_manager_ = nullptr;
  std::unique_ptr<LockManager> lock_manager_;
  std::unique_ptr<BufferPoolManager> bpm_;
  std::unique_ptr<Catalog> catalog_;
  std::unique_ptr<ExecutorContext> exec_ctx_;
  std::unique_ptr<ExecutionEngine> execution_engine_;
  std::vector<std::unique_ptr<AbstractExpression>> allocated_exprs_;
  std::vector<std::unique_ptr<Schema>> allocated_output_schemas_;
  static constexpr uint32_t MAX_VARCHAR_SIZE = 128;
};

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleSeqScanTest) {
  // SELECT colA, colB FROM test_1 WHERE colA > 600

  // Construct query plan
  TableMetadata *table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
  Schema &schema = table_info->schema_;
  auto *colA = MakeColumnValueExpression(schema, 0, "colA");
  auto *colB = MakeColumnValueExpression(schema, 0, "colB");
  auto *const600 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(600));
  auto *predicate = MakeComparisonExpression(colA, const600, ComparisonType::GreaterThan);
  auto *out_schema = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
  SeqScanPlanNode plan{out_schema, predicate, table_info->oid_};

  // Execute
  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(&plan, &result_set, GetTxn(), GetExecutorContext());

  // Verify

  for (const auto &tuple : result_set) {
    ASSERT_TRUE(tuple.GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>() > 600);
    ASSERT_TRUE(tuple.GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>() < 10);
  }
  ASSERT_EQ(result_set.size(), 399);
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleIndexScanTest) {
  // SELECT colA, colB FROM test_1 WHERE colA > 500

  // Construct query plan
  TableMetadata *table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
  Schema &schema = table_info->schema_;

  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);
  auto index_info = GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
      GetTxn(), "index1", "test_1", table_info->schema_, *key_schema, {0}, 8);

  auto *colA = MakeColumnValueExpression(schema, 0, "colA");
  auto *colB = MakeColumnValueExpression(schema, 0, "colB");
  auto *const600 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(600));
  auto *predicate = MakeComparisonExpression(colA, const600, ComparisonType::GreaterThan);
  auto *out_schema = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
  IndexScanPlanNode plan{out_schema, predicate, index_info->index_oid_};

  // Execute
  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(&plan, &result_set, GetTxn(), GetExecutorContext());

  // Verify
  for (const auto &tuple : result_set) {
    ASSERT_TRUE(tuple.GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>() > 600);
    ASSERT_TRUE(tuple.GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>() < 10);
  }
  ASSERT_EQ(result_set.size(), 399);

  delete key_schema;
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleRawInsertWithIndexTest) {
  // INSERT INTO empty_table2 VALUES (200, 20), (201, 21), (202, 22)
  // Create Values to insert
  std::vector<Value> val1{ValueFactory::GetIntegerValue(200), ValueFactory::GetIntegerValue(20)};
  std::vector<Value> val2{ValueFactory::GetIntegerValue(201), ValueFactory::GetIntegerValue(21)};
  std::vector<Value> val3{ValueFactory::GetIntegerValue(202), ValueFactory::GetIntegerValue(22)};
  std::vector<std::vector<Value>> raw_vals{val1, val2, val3};
  // Create insert plan node
  auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
  InsertPlanNode insert_plan{std::move(raw_vals), table_info->oid_};

  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);
  auto index_info = GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
      GetTxn(), "index1", "empty_table2", table_info->schema_, *key_schema, {0}, 8);

  GetExecutionEngine()->Execute(&insert_plan, nullptr, GetTxn(), GetExecutorContext());

  // Iterate through table make sure that values were inserted.
  // SELECT * FROM empty_table2;
  auto &schema = table_info->schema_;
  auto colA = MakeColumnValueExpression(schema, 0, "colA");
  auto colB = MakeColumnValueExpression(schema, 0, "colB");
  auto out_schema = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
  SeqScanPlanNode scan_plan{out_schema, nullptr, table_info->oid_};

  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(&scan_plan, &result_set, GetTxn(), GetExecutorContext());

  // First value
  ASSERT_EQ(result_set[0].GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>(), 200);
  ASSERT_EQ(result_set[0].GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>(), 20);

  // Second value
  ASSERT_EQ(result_set[1].GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>(), 201);
  ASSERT_EQ(result_set[1].GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>(), 21);

  // Third value
  ASSERT_EQ(result_set[2].GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>(), 202);
  ASSERT_EQ(result_set[2].GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>(), 22);

  // Size
  ASSERT_EQ(result_set.size(), 3);
  std::vector<RID> rids;

  // Get RID from index, fetch tuple and then compare
  for (size_t i = 0; i < result_set.size(); ++i) {
    index_info->index_->ScanKey(result_set[i], &rids, GetTxn());
    Tuple indexed_tuple;
    auto fetch_tuple = table_info->table_->GetTuple(rids[i], &indexed_tuple, GetTxn());

    ASSERT_TRUE(fetch_tuple);
    ASSERT_EQ(indexed_tuple.GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>(),
              result_set[i].GetValue(out_schema, out_schema->GetColIdx("colA")).GetAs<int32_t>());
    ASSERT_EQ(indexed_tuple.GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>(),
              result_set[i].GetValue(out_schema, out_schema->GetColIdx("colB")).GetAs<int32_t>());
  }
  delete key_schema;
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleSelectInsertTest) {
  // INSERT INTO empty_table2 SELECT colA, colB FROM test_1 WHERE colA > 500
  std::unique_ptr<AbstractPlanNode> scan_plan1;
  const Schema *out_schema1;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    auto const600 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(600));
    auto predicate = MakeComparisonExpression(colA, const600, ComparisonType::GreaterThan);
    out_schema1 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan1 = std::make_unique<SeqScanPlanNode>(out_schema1, predicate, table_info->oid_);
  }
  std::unique_ptr<AbstractPlanNode> insert_plan;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
    insert_plan = std::make_unique<InsertPlanNode>(scan_plan1.get(), table_info->oid_);
  }

  GetExecutionEngine()->Execute(insert_plan.get(), nullptr, GetTxn(), GetExecutorContext());
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);
  auto index_info = GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
      GetTxn(), "index1", "empty_table2", GetExecutorContext()->GetCatalog()->GetTable("empty_table2")->schema_,
      *key_schema, {0}, 8);

  // Now iterate through both tables, and make sure they have the same data
  std::unique_ptr<AbstractPlanNode> scan_plan2;
  const Schema *out_schema2;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    out_schema2 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan2 = std::make_unique<SeqScanPlanNode>(out_schema2, nullptr, table_info->oid_);
  }

  std::vector<Tuple> result_set1;
  std::vector<Tuple> result_set2;
  GetExecutionEngine()->Execute(scan_plan1.get(), &result_set1, GetTxn(), GetExecutorContext());
  GetExecutionEngine()->Execute(scan_plan2.get(), &result_set2, GetTxn(), GetExecutorContext());

  ASSERT_EQ(result_set1.size(), result_set2.size());
  for (size_t i = 0; i < result_set1.size(); ++i) {
    ASSERT_EQ(result_set1[i].GetValue(out_schema1, out_schema1->GetColIdx("colA")).GetAs<int32_t>(),
              result_set2[i].GetValue(out_schema2, out_schema2->GetColIdx("colA")).GetAs<int32_t>());
    ASSERT_EQ(result_set1[i].GetValue(out_schema1, out_schema1->GetColIdx("colB")).GetAs<int32_t>(),
              result_set2[i].GetValue(out_schema2, out_schema2->GetColIdx("colB")).GetAs<int32_t>());
  }
  ASSERT_EQ(result_set1.size(), 399);

  std::vector<RID> rids;
  for (size_t i = 0; i < result_set2.size(); ++i) {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
    index_info->index_->ScanKey(result_set2[i], &rids, GetTxn());
    Tuple indexed_tuple;
    auto fetch_tuple = table_info->table_->GetTuple(rids[i], &indexed_tuple, GetTxn());

    ASSERT_TRUE(fetch_tuple);
    ASSERT_EQ(indexed_tuple.GetValue(out_schema2, out_schema2->GetColIdx("colA")).GetAs<int32_t>(),
              result_set2[i].GetValue(out_schema2, out_schema2->GetColIdx("colA")).GetAs<int32_t>());
    ASSERT_EQ(indexed_tuple.GetValue(out_schema2, out_schema2->GetColIdx("colB")).GetAs<int32_t>(),
              result_set2[i].GetValue(out_schema2, out_schema2->GetColIdx("colB")).GetAs<int32_t>());
  }

  delete key_schema;
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleUpdateTest) {
  // INSERT INTO empty_table2 SELECT colA, colA FROM test_1 WHERE colA < 50
  // UPDATE empty_table2 SET colA = colA+10 WHERE colA < 50
  std::unique_ptr<AbstractPlanNode> scan_plan1;
  const Schema *out_schema1;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto const600 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(50));
    auto predicate = MakeComparisonExpression(colA, const600, ComparisonType::LessThan);
    out_schema1 = MakeOutputSchema({{"colA", colA}, {"colA", colA}});
    scan_plan1 = std::make_unique<SeqScanPlanNode>(out_schema1, predicate, table_info->oid_);
  }
  std::unique_ptr<AbstractPlanNode> insert_plan;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
    insert_plan = std::make_unique<InsertPlanNode>(scan_plan1.get(), table_info->oid_);
  }

  GetExecutionEngine()->Execute(insert_plan.get(), nullptr, GetTxn(), GetExecutorContext());

  // Construct query plan
  auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
  auto &schema = table_info->schema_;
  auto colA = MakeColumnValueExpression(schema, 0, "colA");
  auto colB = MakeColumnValueExpression(schema, 0, "colB");
  auto const50 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(50));
  auto predicate = MakeComparisonExpression(colA, const50, ComparisonType::LessThan);
  auto out_empty_schema = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
  auto scan_empty_plan = std::make_unique<SeqScanPlanNode>(out_empty_schema, predicate, table_info->oid_);

  // Create Indexes for col1 and col2
  Schema *key_schema = ParseCreateStatement("a int");
  GenericComparator<8> comparator(key_schema);
  GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
      GetTxn(), "index1", "empty_table2", GetExecutorContext()->GetCatalog()->GetTable("empty_table2")->schema_,
      *key_schema, {0}, 8);
  auto index_info_2 = GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
      GetTxn(), "index2", "empty_table2", GetExecutorContext()->GetCatalog()->GetTable("empty_table2")->schema_,
      *key_schema, {1}, 8);

  std::unique_ptr<AbstractPlanNode> scan_plan2;
  const Schema *out_schema2;
  {
    out_schema2 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan2 = std::make_unique<SeqScanPlanNode>(out_schema2, nullptr, table_info->oid_);
  }

  std::vector<Tuple> result_set2;
  GetExecutionEngine()->Execute(scan_plan2.get(), &result_set2, GetTxn(), GetExecutorContext());

  std::unordered_map<uint32_t, UpdateInfo> update_attrs;
  update_attrs.insert(std::make_pair(0, UpdateInfo(UpdateType::Add, 10)));
  std::unique_ptr<AbstractPlanNode> update_plan;
  { update_plan = std::make_unique<UpdatePlanNode>(scan_empty_plan.get(), table_info->oid_, update_attrs); }

  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(update_plan.get(), &result_set, GetTxn(), GetExecutorContext());

  std::vector<RID> rids;
  for (int32_t i = 0; i < 50; ++i) {
    Tuple key = Tuple({Value(TypeId::INTEGER, i)}, key_schema);
    index_info_2->index_->ScanKey(key, &rids, GetTxn());
    Tuple indexed_tuple;
    auto fetch_tuple = table_info->table_->GetTuple(rids[0], &indexed_tuple, GetTxn());
    ASSERT_TRUE(fetch_tuple);
    auto cola_val = indexed_tuple.GetValue(&schema, 0).GetAs<uint32_t>();
    auto colb_val = indexed_tuple.GetValue(&schema, 1).GetAs<uint32_t>();
    ASSERT_TRUE(cola_val == colb_val + 10);
  }
  delete key_schema;
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleDeleteTest) {
  // SELECT colA FROM test_1 WHERE colA < 50
  // DELETE FROM test_1 WHERE colA < 50
  // SELECT colA FROM test_1 WHERE colA < 50

  // Construct query plan
  auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
  auto &schema = table_info->schema_;
  auto colA = MakeColumnValueExpression(schema, 0, "colA");
  auto const50 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(50));
  auto predicate = MakeComparisonExpression(colA, const50, ComparisonType::LessThan);
  auto out_schema1 = MakeOutputSchema({{"colA", colA}});
  auto scan_plan1 = std::make_unique<SeqScanPlanNode>(out_schema1, predicate, table_info->oid_);
  // index
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);
  auto index_info = GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
      GetTxn(), "index1", "test_1", GetExecutorContext()->GetCatalog()->GetTable("test_1")->schema_, *key_schema, {0},
      8);

  // Execute
  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(scan_plan1.get(), &result_set, GetTxn(), GetExecutorContext());

  // Verify
  for (const auto &tuple : result_set) {
    ASSERT_TRUE(tuple.GetValue(out_schema1, out_schema1->GetColIdx("colA")).GetAs<int32_t>() < 50);
  }
  ASSERT_EQ(result_set.size(), 50);
  Tuple index_key = Tuple(result_set[0]);

  std::unique_ptr<AbstractPlanNode> delete_plan;
  { delete_plan = std::make_unique<DeletePlanNode>(scan_plan1.get(), table_info->oid_); }
  GetExecutionEngine()->Execute(delete_plan.get(), nullptr, GetTxn(), GetExecutorContext());

  result_set.clear();
  GetExecutionEngine()->Execute(scan_plan1.get(), &result_set, GetTxn(), GetExecutorContext());
  ASSERT_TRUE(result_set.empty());

  std::vector<RID> rids;

  index_info->index_->ScanKey(index_key, &rids, GetTxn());
  ASSERT_TRUE(rids.empty());

  delete key_schema;
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleNestedLoopJoinTest) {
  // SELECT test_1.colA, test_1.colB, test_2.col1, test_2.col3 FROM test_1 JOIN test_2 ON test_1.colA = test_2.col1 AND
  // test_1.colA < 50
  std::unique_ptr<AbstractPlanNode> scan_plan1;
  const Schema *out_schema1;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    auto const50 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(50));
    auto predicate = MakeComparisonExpression(colA, const50, ComparisonType::LessThan);
    out_schema1 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan1 = std::make_unique<SeqScanPlanNode>(out_schema1, predicate, table_info->oid_);
  }
  std::unique_ptr<AbstractPlanNode> scan_plan2;
  const Schema *out_schema2;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_2");
    auto &schema = table_info->schema_;
    auto col1 = MakeColumnValueExpression(schema, 0, "col1");
    auto col3 = MakeColumnValueExpression(schema, 0, "col3");
    out_schema2 = MakeOutputSchema({{"col1", col1}, {"col3", col3}});
    scan_plan2 = std::make_unique<SeqScanPlanNode>(out_schema2, nullptr, table_info->oid_);
  }
  std::unique_ptr<NestedLoopJoinPlanNode> join_plan;
  const Schema *out_final;
  {
    // colA and colB have a tuple index of 0 because they are the left side of the join
    auto colA = MakeColumnValueExpression(*out_schema1, 0, "colA");
    auto colB = MakeColumnValueExpression(*out_schema1, 0, "colB");
    // col1 and col2 have a tuple index of 1 because they are the right side of the join
    auto col1 = MakeColumnValueExpression(*out_schema2, 1, "col1");
    auto col3 = MakeColumnValueExpression(*out_schema2, 1, "col3");
    auto predicate = MakeComparisonExpression(colA, col1, ComparisonType::Equal);
    out_final = MakeOutputSchema({{"colA", colA}, {"colB", colB}, {"col1", col1}, {"col3", col3}});
    join_plan = std::make_unique<NestedLoopJoinPlanNode>(
        out_final, std::vector<const AbstractPlanNode *>{scan_plan1.get(), scan_plan2.get()}, predicate);
  }

  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(join_plan.get(), &result_set, GetTxn(), GetExecutorContext());
  ASSERT_EQ(result_set.size(), 50);

  for (const auto &tuple : result_set) {
    auto col_a_val = tuple.GetValue(out_final, out_final->GetColIdx("colA")).GetAs<int32_t>();
    auto col_1_val = tuple.GetValue(out_final, out_final->GetColIdx("col1")).GetAs<int16_t>();
    ASSERT_EQ(col_a_val, col_1_val);
    ASSERT_LT(col_a_val, 50);
  }
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleAggregationTest) {
  // SELECT COUNT(colA), SUM(colA), min(colA), max(colA) from test_1;
  std::unique_ptr<AbstractPlanNode> scan_plan;
  const Schema *scan_schema;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    scan_schema = MakeOutputSchema({{"colA", colA}});
    scan_plan = std::make_unique<SeqScanPlanNode>(scan_schema, nullptr, table_info->oid_);
  }

  std::unique_ptr<AbstractPlanNode> agg_plan;
  const Schema *agg_schema;
  {
    const AbstractExpression *colA = MakeColumnValueExpression(*scan_schema, 0, "colA");
    const AbstractExpression *countA = MakeAggregateValueExpression(false, 0);
    const AbstractExpression *sumA = MakeAggregateValueExpression(false, 1);
    const AbstractExpression *minA = MakeAggregateValueExpression(false, 2);
    const AbstractExpression *maxA = MakeAggregateValueExpression(false, 3);

    agg_schema = MakeOutputSchema({{"countA", countA}, {"sumA", sumA}, {"minA", minA}, {"maxA", maxA}});
    agg_plan = std::make_unique<AggregationPlanNode>(
        agg_schema, scan_plan.get(), nullptr, std::vector<const AbstractExpression *>{},
        std::vector<const AbstractExpression *>{colA, colA, colA, colA},
        std::vector<AggregationType>{AggregationType::CountAggregate, AggregationType::SumAggregate,
                                     AggregationType::MinAggregate, AggregationType::MaxAggregate});
  }
  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(agg_plan.get(), &result_set, GetTxn(), GetExecutorContext());

  auto countA_val = result_set[0].GetValue(agg_schema, agg_schema->GetColIdx("countA")).GetAs<int32_t>();
  auto sumA_val = result_set[0].GetValue(agg_schema, agg_schema->GetColIdx("sumA")).GetAs<int32_t>();
  auto minA_val = result_set[0].GetValue(agg_schema, agg_schema->GetColIdx("minA")).GetAs<int32_t>();
  auto maxA_val = result_set[0].GetValue(agg_schema, agg_schema->GetColIdx("maxA")).GetAs<int32_t>();
  // Should count all tuples
  ASSERT_EQ(countA_val, TEST1_SIZE);
  // Should sum from 0 to TEST1_SIZE
  ASSERT_EQ(sumA_val, TEST1_SIZE * (TEST1_SIZE - 1) / 2);
  // Minimum should be 0
  ASSERT_EQ(minA_val, 0);
  // Maximum should be TEST1_SIZE - 1
  ASSERT_EQ(maxA_val, TEST1_SIZE - 1);

  ASSERT_EQ(result_set.size(), 1);
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleGroupByAggregation) {
  // SELECT count(colA), colB, sum(colC) FROM test_1 Group By colB HAVING count(colA) > 100
  std::unique_ptr<AbstractPlanNode> scan_plan;
  const Schema *scan_schema;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    auto colC = MakeColumnValueExpression(schema, 0, "colC");
    scan_schema = MakeOutputSchema({{"colA", colA}, {"colB", colB}, {"colC", colC}});
    scan_plan = std::make_unique<SeqScanPlanNode>(scan_schema, nullptr, table_info->oid_);
  }

  std::unique_ptr<AbstractPlanNode> agg_plan;
  const Schema *agg_schema;
  {
    const AbstractExpression *colA = MakeColumnValueExpression(*scan_schema, 0, "colA");
    const AbstractExpression *colB = MakeColumnValueExpression(*scan_schema, 0, "colB");
    const AbstractExpression *colC = MakeColumnValueExpression(*scan_schema, 0, "colC");
    // Make group bys
    std::vector<const AbstractExpression *> group_by_cols{colB};
    const AbstractExpression *groupbyB = MakeAggregateValueExpression(true, 0);
    // Make aggregates
    std::vector<const AbstractExpression *> aggregate_cols{colA, colC};
    std::vector<AggregationType> agg_types{AggregationType::CountAggregate, AggregationType::SumAggregate};
    const AbstractExpression *countA = MakeAggregateValueExpression(false, 0);
    const AbstractExpression *sumC = MakeAggregateValueExpression(false, 1);
    // Make having clause
    const AbstractExpression *having = MakeComparisonExpression(
        countA, MakeConstantValueExpression(ValueFactory::GetIntegerValue(100)), ComparisonType::GreaterThan);

    // Create plan
    agg_schema = MakeOutputSchema({{"countA", countA}, {"colB", groupbyB}, {"sumC", sumC}});
    agg_plan = std::make_unique<AggregationPlanNode>(agg_schema, scan_plan.get(), having, std::move(group_by_cols),
                                                     std::move(aggregate_cols), std::move(agg_types));
  }

  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(agg_plan.get(), &result_set, GetTxn(), GetExecutorContext());

  std::unordered_set<int32_t> encountered;
  for (const auto &tuple : result_set) {
    // Should have countA > 100
    ASSERT_GT(tuple.GetValue(agg_schema, agg_schema->GetColIdx("countA")).GetAs<int32_t>(), 100);
    // Should have unique colBs.
    auto colB = tuple.GetValue(agg_schema, agg_schema->GetColIdx("colB")).GetAs<int32_t>();
    ASSERT_EQ(encountered.count(colB), 0);
    encountered.insert(colB);
    // Sanity check: ColB should also be within [0, 10).
    ASSERT_TRUE(0 <= colB && colB < 10);
  }
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SimpleNestedIndexJoinTest) {
  // SELECT test_1.colA, test_1.colB, test_3.col1, test_3.col3 FROM test_1 JOIN test_3 ON test_1.colA = test_3.col1
  std::unique_ptr<AbstractPlanNode> scan_plan1;
  const Schema *outer_schema1;
  auto &schema_outer = GetExecutorContext()->GetCatalog()->GetTable("test_1")->schema_;
  auto outer_colA = MakeColumnValueExpression(schema_outer, 0, "colA");
  auto outer_colB = MakeColumnValueExpression(schema_outer, 0, "colB");
  auto outer_colC = MakeColumnValueExpression(schema_outer, 0, "colC");
  auto outer_colD = MakeColumnValueExpression(schema_outer, 0, "colD");
  const Schema *outer_out_schema1 =
      MakeOutputSchema({{"colA", outer_colA}, {"colB", outer_colB}, {"colC", outer_colC}, {"colD", outer_colD}});

  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    outer_schema1 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan1 = std::make_unique<SeqScanPlanNode>(outer_out_schema1, nullptr, table_info->oid_);
  }
  const Schema *out_schema2;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_3");
    auto &schema = table_info->schema_;
    auto col1 = MakeColumnValueExpression(schema, 0, "col1");
    auto col3 = MakeColumnValueExpression(schema, 0, "col3");
    out_schema2 = MakeOutputSchema({{"col1", col1}, {"col3", col3}});
  }
  std::unique_ptr<NestedIndexJoinPlanNode> join_plan;
  const Schema *out_final;
  Schema *key_schema = ParseCreateStatement("a int");
  {
    // colA and colB have a tuple index of 0 because they are the left side of the join
    auto colA = MakeColumnValueExpression(*outer_schema1, 0, "colA");
    auto colB = MakeColumnValueExpression(*outer_schema1, 0, "colB");
    // col1 and col2 have a tuple index of 1 because they are the right side of the join
    auto col1 = MakeColumnValueExpression(*out_schema2, 1, "col1");
    auto col3 = MakeColumnValueExpression(*out_schema2, 1, "col3");
    auto predicate = MakeComparisonExpression(colA, col1, ComparisonType::Equal);
    out_final = MakeOutputSchema({{"colA", colA}, {"colB", colB}, {"col1", col1}, {"col3", col3}});

    auto inner_table_info = GetExecutorContext()->GetCatalog()->GetTable("test_3");
    auto inner_table_oid = inner_table_info->oid_;
    GenericComparator<8> comparator(key_schema);
    // Create index for inner table
    auto index_info = GetExecutorContext()->GetCatalog()->CreateIndex<GenericKey<8>, RID, GenericComparator<8>>(
        GetTxn(), "index1", "test_3", inner_table_info->schema_, *key_schema, {0}, 1);

    join_plan = std::make_unique<NestedIndexJoinPlanNode>(
        out_final, std::vector<const AbstractPlanNode *>{scan_plan1.get()}, predicate, inner_table_oid,
        index_info->name_, outer_schema1, out_schema2);
  }

  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(join_plan.get(), &result_set, GetTxn(), GetExecutorContext());
  ASSERT_EQ(result_set.size(), 100);

  for (const auto &tuple : result_set) {
    ASSERT_EQ(tuple.GetValue(out_final, out_final->GetColIdx("colA")).GetAs<int32_t>(),
              tuple.GetValue(out_final, out_final->GetColIdx("col1")).GetAs<int16_t>());
  }

  delete key_schema;
}

// NOLINTNEXTLINE
TEST_F(GradingExecutorTest, SchemaChangeSeqScan) {
  // INSERT INTO empty_table2 SELECT colA, colB FROM test_1 WHERE colA > 600
  // compare: SELECT colA as outA, colB as outB FROM empty_table2
  std::unique_ptr<AbstractPlanNode> scan_plan1;
  const Schema *out_schema1;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    auto const600 = MakeConstantValueExpression(ValueFactory::GetIntegerValue(600));
    auto predicate = MakeComparisonExpression(colA, const600, ComparisonType::GreaterThan);
    out_schema1 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan1 = std::make_unique<SeqScanPlanNode>(out_schema1, predicate, table_info->oid_);
  }

  std::unique_ptr<AbstractPlanNode> insert_plan;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
    insert_plan = std::make_unique<InsertPlanNode>(scan_plan1.get(), table_info->oid_);
  }

  std::vector<Tuple> result_set;
  GetExecutionEngine()->Execute(insert_plan.get(), &result_set, GetTxn(), GetExecutorContext());
  ASSERT_TRUE(result_set.empty());

  // Now iterate through both tables, and make sure they have the same data
  std::unique_ptr<AbstractPlanNode> scan_plan2;
  const Schema *out_schema2;
  {
    auto table_info2 = GetExecutorContext()->GetCatalog()->GetTable("empty_table2");
    auto table_info3 = GetExecutorContext()->GetCatalog()->GetTable("empty_table3");
    auto &schema3 = table_info3->schema_;
    auto outA = MakeColumnValueExpression(schema3, 0, "outA");
    auto outB = MakeColumnValueExpression(schema3, 0, "outB");
    out_schema2 = MakeOutputSchema({{"outA", outA}, {"outB", outB}});
    scan_plan2 = std::make_unique<SeqScanPlanNode>(out_schema2, nullptr, table_info2->oid_);
  }

  std::vector<Tuple> result_set1;
  GetExecutionEngine()->Execute(scan_plan1.get(), &result_set1, GetTxn(), GetExecutorContext());

  std::vector<Tuple> result_set2;
  GetExecutionEngine()->Execute(scan_plan2.get(), &result_set2, GetTxn(), GetExecutorContext());

  ASSERT_EQ(result_set1.size(), 399);
  ASSERT_EQ(result_set2.size(), 399);
  for (size_t i = 0; i < result_set1.size(); ++i) {
    ASSERT_EQ(result_set1[i].GetValue(out_schema1, out_schema1->GetColIdx("colA")).GetAs<int32_t>(),
              result_set2[i].GetValue(out_schema2, out_schema2->GetColIdx("outA")).GetAs<int32_t>());
    ASSERT_EQ(result_set1[i].GetValue(out_schema1, out_schema1->GetColIdx("colB")).GetAs<int32_t>(),
              result_set2[i].GetValue(out_schema2, out_schema2->GetColIdx("outB")).GetAs<int32_t>());
  }
}

TEST_F(GradingExecutorTest, IntegratedTest) {
  // scan -> join -> aggregate
  std::unique_ptr<AbstractPlanNode> scan_plan1;
  const Schema *out_schema1;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_1");
    auto &schema = table_info->schema_;
    auto colA = MakeColumnValueExpression(schema, 0, "colA");
    auto colB = MakeColumnValueExpression(schema, 0, "colB");
    out_schema1 = MakeOutputSchema({{"colA", colA}, {"colB", colB}});
    scan_plan1 = std::make_unique<SeqScanPlanNode>(out_schema1, nullptr, table_info->oid_);
  }
  std::unique_ptr<AbstractPlanNode> scan_plan2;
  const Schema *out_schema2;
  {
    auto table_info = GetExecutorContext()->GetCatalog()->GetTable("test_2");
    auto &schema = table_info->schema_;
    auto col1 = MakeColumnValueExpression(schema, 0, "col1");
    auto col2 = MakeColumnValueExpression(schema, 0, "col2");
    out_schema2 = MakeOutputSchema({{"col1", col1}, {"col2", col2}});
    scan_plan2 = std::make_unique<SeqScanPlanNode>(out_schema2, nullptr, table_info->oid_);
  }
  std::unique_ptr<NestedLoopJoinPlanNode> join_plan;
  const Schema *out_final;
  {
    // colA and colB have a tuple index of 0 because they are the left side of the join
    auto colA = MakeColumnValueExpression(*out_schema1, 0, "colA");
    auto colB = MakeColumnValueExpression(*out_schema1, 0, "colB");
    // col1 and col2 have a tuple index of 1 because they are the right side of the join
    auto col1 = MakeColumnValueExpression(*out_schema2, 1, "col1");
    auto col2 = MakeColumnValueExpression(*out_schema2, 1, "col2");
    std::vector<const AbstractExpression *> left_keys{colA};
    std::vector<const AbstractExpression *> right_keys{col1};
    auto predicate = MakeComparisonExpression(colA, col1, ComparisonType::Equal);
    out_final = MakeOutputSchema({{"colA", colA}, {"colB", colB}, {"col1", col1}, {"col2", col2}});
    join_plan = std::make_unique<NestedLoopJoinPlanNode>(
        out_final, std::vector<const AbstractPlanNode *>{scan_plan1.get(), scan_plan2.get()}, predicate);
  }

  std::unique_ptr<AbstractPlanNode> agg_plan;
  const Schema *agg_schema;
  {
    const AbstractExpression *colA = MakeColumnValueExpression(*out_final, 0, "colA");
    const AbstractExpression *countA = MakeAggregateValueExpression(false, 0);
    const AbstractExpression *sumA = MakeAggregateValueExpression(false, 1);
    const AbstractExpression *minA = MakeAggregateValueExpression(false, 2);
    const AbstractExpression *maxA = MakeAggregateValueExpression(false, 3);

    agg_schema = MakeOutputSchema({{"countA", countA}, {"sumA", sumA}, {"minA", minA}, {"maxA", maxA}});
    agg_plan = std::make_unique<AggregationPlanNode>(
        agg_schema, join_plan.get(), nullptr, std::vector<const AbstractExpression *>{},
        std::vector<const AbstractExpression *>{colA, colA, colA, colA},
        std::vector<AggregationType>{AggregationType::CountAggregate, AggregationType::SumAggregate,
                                     AggregationType::MinAggregate, AggregationType::MaxAggregate});
  }

  std::vector<Tuple> result_set1;
  GetExecutionEngine()->Execute(agg_plan.get(), &result_set1, GetTxn(), GetExecutorContext());

  ASSERT_EQ(result_set1.size(), 1);
  auto tuple = result_set1[0];
  auto countA_val = tuple.GetValue(agg_schema, agg_schema->GetColIdx("countA")).GetAs<int32_t>();
  auto sumA_val = tuple.GetValue(agg_schema, agg_schema->GetColIdx("sumA")).GetAs<int32_t>();
  auto minA_val = tuple.GetValue(agg_schema, agg_schema->GetColIdx("minA")).GetAs<int32_t>();
  auto maxA_val = tuple.GetValue(agg_schema, agg_schema->GetColIdx("maxA")).GetAs<int32_t>();
  // Should count all tuples
  ASSERT_EQ(countA_val, TEST2_SIZE);
  // Should sum from 0 to TEST2_SIZE
  ASSERT_EQ(sumA_val, TEST2_SIZE * (TEST2_SIZE - 1) / 2);
  // Minimum should be 0
  ASSERT_EQ(minA_val, 0);
  // Maximum should be TEST2_SIZE - 1
  ASSERT_EQ(maxA_val, TEST2_SIZE - 1);
}

}  // namespace bustub