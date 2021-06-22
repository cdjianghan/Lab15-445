#!/bin/bash

echo "batch copy..."
mkdir -p src/include/buffer
mkdir -p src/buffer


cp ../src/include/buffer/lru_replacer.h src/include/buffer/lru_replacer.h
cp ../src/buffer/lru_replacer.cpp src/buffer/lru_replacer.cpp
cp ../src/include/buffer/buffer_pool_manager.h src/include/buffer/buffer_pool_manager.h
cp ../src/buffer/buffer_pool_manager.cpp src/buffer/buffer_pool_manager.cpp



