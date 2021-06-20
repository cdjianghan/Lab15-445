#!/bin/bash

echo "batch copy..."
mkdir -p src/include/buffer
mkdir -p src/buffer

mkdir -p src/include/storage/page
mkdir -p src/storage/page
mkdir -p src/storage/index
mkdir -p src/include/storage/index

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
