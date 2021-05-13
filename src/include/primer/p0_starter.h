//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// p0_starter.h
//
// Identification: src/include/primer/p0_starter.h
//
// Copyright (c) 2015-2020, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>

namespace bustub {

/*
 * The base class defining a Matrix
 */
template <typename T>
class Matrix {
 protected:
  Matrix(int r, int c) : rows(r), cols(c), linear(new T[r * c]) {
    for (int i = 0; i < r * c; i++) {
      linear[i] = 0;
    }
  }

  // # of rows in the matrix
  int rows;
  // # of Columns in the matrix
  int cols;
  // Flattened array containing the elements of the matrix
  // Allocate the array in the constructor. Don't forget to free up
  // the array in the destructor.
  T *linear;

 public:
  // Return the # of rows in the matrix
  virtual int GetRows() = 0;

  // Return the # of columns in the matrix
  virtual int GetColumns() = 0;

  // Return the (i,j)th  matrix element
  virtual T GetElem(int i, int j) = 0;

  // Sets the (i,j)th  matrix element to val
  virtual void SetElem(int i, int j, T val) = 0;

  // Sets the matrix elements based on the array arr
  virtual void MatImport(T *arr) = 0;

  virtual ~Matrix() { delete[] linear; }
};

template <typename T>
class RowMatrix : public Matrix<T> {
 public:
  RowMatrix(int r, int c) : Matrix<T>(r, c) {
    data_ = new T *[r];
    for (int i = 0; i < r; i++) {
      data_[i] = new T[c];
    }
    for (int i = 0; i < r; i++) {
      for (int j = 0; j < c; j++) {
        data_[i][j] = 0;
      }
    }
  }

  int GetRows() override { return this->rows; }

  int GetColumns() override { return this->cols; }

  T GetElem(int i, int j) override {
    if (i < 0 || i >= this->GetRows() || j < 0 || j >= this->GetColumns()) {
      return 0;
    }
    return data_[i][j];
  }

  void SetElem(int i, int j, T val) override {
    if (i < 0 || i >= this->GetRows() || j < 0 || j >= this->GetColumns()) {
      return;
    }
    this->linear[i * this->GetColumns() + j] = val;
    data_[i][j] = val;
  }

  void MatImport(T *arr) override {
    int k = 0;
    for (int i = 0; i < this->GetRows(); i++) {
      for (int j = 0; j < this->GetColumns(); j++) {
        this->linear[k] = arr[k];
        data_[i][j] = arr[k++];
      }
    }
  }

  ~RowMatrix() override {
    for (int i = 0; i < this->GetRows(); i++) {
      delete[] data_[i];
    }
    delete[] data_;
  }

 private:
  // 2D array containing the elements of the matrix in row-major format
  // Allocate the array of row pointers in the constructor. Use these pointers
  // to point to corresponding elements of the 'linear' array.
  // Don't forget to free up the array in the destructor.
  T **data_;
};

template <typename T>
class RowMatrixOperations {
 public:
  // Compute (mat1 + mat2) and return the result.
  // Return nullptr if dimensions mismatch for input matrices.
  static std::unique_ptr<RowMatrix<T>> AddMatrices(std::unique_ptr<RowMatrix<T>> mat1,
                                                   std::unique_ptr<RowMatrix<T>> mat2) {
    if (mat1->GetRows() != mat2->GetRows() || mat1->GetColumns() != mat2->GetColumns()) {
      return std::unique_ptr<RowMatrix<T>>(nullptr);
    }
    int row = mat1->GetRows();
    int col = mat1->GetColumns();
    std::unique_ptr<RowMatrix<T>> mat(new RowMatrix<T>(row, col));
    for (int i = 0; i < row; i++) {
      for (int j = 0; j < col; j++) {
        mat->SetElem(i, j, mat1->GetElem(i, j) + mat2->GetElem(i, j));
      }
    }
    return mat;
  }

  // Compute matrix multiplication (mat1 * mat2) and return the result.
  // Return nullptr if dimensions mismatch for input matrices.
  static std::unique_ptr<RowMatrix<T>> MultiplyMatrices(std::unique_ptr<RowMatrix<T>> mat1,
                                                        std::unique_ptr<RowMatrix<T>> mat2) {
    if (mat1->GetColumns() != mat2->GetRows()) {
      return std::unique_ptr<RowMatrix<T>>(nullptr);
    }

    int row = mat1->GetRows();
    int col = mat2->GetColumns();

    std::unique_ptr<RowMatrix<T>> mat(new RowMatrix<T>(row, col));
    for (int i = 0; i < row; i++) {
      for (int j = 0; j < col; j++) {
        for (int k = 0; k < mat1->GetColumns(); k++) {
          mat->SetElem(i, j, mat->GetElem(i, j) + mat1->GetElem(i, k) * mat2->GetElem(k, j));
        }
      }
    }
    return mat;
  }

  // Simplified GEMM (general matrix multiply) operation
  // Compute (matA * matB + matC). Return nullptr if dimensions mismatch for input matrices
  static std::unique_ptr<RowMatrix<T>> GemmMatrices(std::unique_ptr<RowMatrix<T>> matA,
                                                    std::unique_ptr<RowMatrix<T>> matB,
                                                    std::unique_ptr<RowMatrix<T>> matC) {
    std::unique_ptr<RowMatrix<T>> mat_tmp = std::move(MultiplyMatrices(matA, matB));
    if (mat_tmp == nullptr) {
      return std::unique_ptr<RowMatrix<T>>(nullptr);
    }

    return AddMatrices(matA, matB);
    // std::unique_ptr<RowMatrix<T>> mat = std::move(AddMatrices(matA,matB));
    // return mat;
  }
};
}  // namespace bustub