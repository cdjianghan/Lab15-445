#!/bin/bash

echo "copy start"
mkdir -p src/include/buffer
mkdir -p src/buffer

mkdir -p src/include/storage/page
mkdir -p src/storage/page
mkdir -p src/storage/index
mkdir -p src/include/storage/index

mkdir -p src/include/catalog
mkdir -p src/include/execution
mkdir -p src/include/execution/executors
mkdir -p src/execution



cp ../src/include/buffer/lru_replacer.h src/include/buffer/lru_replacer.h
cp ../src/buffer/lru_replacer.cpp src/buffer/lru_replacer.cpp
cp ../src/include/buffer/buffer_pool_manager.h src/include/buffer/buffer_pool_manager.h
cp ../src/buffer/buffer_pool_manager.cpp src/buffer/buffer_pool_manager.cpp



cp ../src/include/storage/page/b_plus_tree_page.h src/include/storage/page/b_plus_tree_page.h
cp ../src/storage/page/b_plus_tree_page.cpp src/storage/page/b_plus_tree_page.cpp
cp ../src/include/storage/page/b_plus_tree_internal_page.h src/include/storage/page/b_plus_tree_internal_page.h
cp ../src/storage/page/b_plus_tree_internal_page.cpp src/storage/page/b_plus_tree_internal_page.cpp
cp ../src/include/storage/page/b_plus_tree_leaf_page.h src/include/storage/page/b_plus_tree_leaf_page.h
cp ../src/storage/page/b_plus_tree_leaf_page.cpp src/storage/page/b_plus_tree_leaf_page.cpp
cp ../src/include/storage/index/b_plus_tree.h src/include/storage/index/b_plus_tree.h
cp ../src/storage/index/b_plus_tree.cpp src/storage/index/b_plus_tree.cpp
cp ../src/include/storage/index/index_iterator.h src/include/storage/index/index_iterator.h
cp ../src/storage/index/index_iterator.cpp src/storage/index/index_iterator.cpp


cp ../src/include/catalog/catalog.h src/include/catalog/catalog.h
cp ../src/include/execution/execution_engine.h src/include/execution/execution_engine.h
cp ../src/include/execution/executor_factory.h src/include/execution/executor_factory.h
cp ../src/include/execution/executors/seq_scan_executor.h src/include/execution/executors/seq_scan_executor.h
cp ../src/include/execution/executors/index_scan_executor.h src/include/execution/executors/index_scan_executor.h
cp ../src/include/execution/executors/insert_executor.h src/include/execution/executors/insert_executor.h
cp ../src/include/execution/executors/update_executor.h src/include/execution/executors/update_executor.h
cp ../src/include/execution/executors/delete_executor.h src/include/execution/executors/delete_executor.h
cp ../src/include/execution/executors/nested_loop_join_executor.h src/include/execution/executors/nested_loop_join_executor.h
cp ../src/include/execution/executors/nested_index_join_executor.h src/include/execution/executors/nested_index_join_executor.h
cp ../src/include/execution/executors/limit_executor.h src/include/execution/executors/limit_executor.h
cp ../src/include/execution/executors/aggregation_executor.h src/include/execution/executors/aggregation_executor.h
cp ../src/include/storage/index/b_plus_tree_index.h src/include/storage/index/b_plus_tree_index.h
cp ../src/include/storage/index/index.h src/include/storage/index/index.h
cp ../src/execution/executor_factory.cpp src/execution/executor_factory.cpp
cp ../src/execution/seq_scan_executor.cpp src/execution/seq_scan_executor.cpp
cp ../src/execution/index_scan_executor.cpp src/execution/index_scan_executor.cpp
cp ../src/execution/insert_executor.cpp src/execution/insert_executor.cpp
cp ../src/execution/update_executor.cpp src/execution/update_executor.cpp
cp ../src/execution/delete_executor.cpp src/execution/delete_executor.cpp
cp ../src/execution/nested_loop_join_executor.cpp src/execution/nested_loop_join_executor.cpp
cp ../src/execution/nested_index_join_executor.cpp src/execution/nested_index_join_executor.cpp
cp ../src/execution/limit_executor.cpp src/execution/limit_executor.cpp
cp ../src/execution/aggregation_executor.cpp src/execution/aggregation_executor.cpp
cp ../src/storage/index/b_plus_tree_index.cpp src/storage/index/b_plus_tree_index.cpp

echo "copy success"


