#pragma once

#include <cmath>
#include <memory>
#include <iostream>

template <class T>
class RowView {
private:
    RowView(T* data, int64_t size) : size_(size), data_(data) { }

public:
    T& operator[](int64_t idx) {
        if (idx < 0 || idx >= size_) {
            LOG_ERR << "Index exceeds row size: " << idx << " / " << size_;
            throw std::runtime_error("Index exceeds row size");
        }
        return data_[idx];
    }

    const T& operator[](int64_t idx) const {
        if (idx < 0 || idx >= size_) {
            LOG_ERR << "Index exceeds row size: " << idx << " / " << size_;
            throw std::runtime_error("Index exceeds row size");
        }
        return data_[idx];
    }

private:
    int64_t size_;
    T* data_;

template <class Y>
friend class Matrix;
};


template <class T>
class Matrix {
public:
    Matrix(int64_t rows, int64_t cols) : n_rows_(rows), n_cols_(cols) {
        if (rows <= 0 || cols <= 0) {
            throw std::runtime_error("Matrix dimensions must be positive integers");
        }
        data_ = std::unique_ptr<T[]>(new T[rows * cols]);
        memset(data_.get(), 0, rows * cols);
    }

    RowView<T> operator[](int64_t idx) {
        if (idx < 0 || idx >= n_rows_) {
            LOG_ERR << "Index exceeds number of rows: " << idx << " / " << n_rows_;
            throw std::runtime_error("Index exceeds number of rows");
        }
        return RowView<T>(data_.get() + n_cols_ * (idx), n_cols_);
    }

    const RowView<T> operator[](int64_t idx) const {
        if (idx < 0 || idx >= n_rows_) {
            LOG_ERR << "Index exceeds number of rows: " << idx << " / " << n_rows_;
            throw std::runtime_error("Index exceeds number of rows");
        }
        return RowView<T>(data_.get() + n_cols_ * (idx), n_cols_);
    }

    int64_t rows() const { return n_rows_; }
    int64_t cols() const { return n_cols_; }

private:
    int64_t n_rows_;
    int64_t n_cols_;
    std::unique_ptr<T[]> data_;
};


template <class T>
class ColumnVec {
public:
    explicit ColumnVec(int64_t size) : size_(size) {
        if (size_ <= 0) {
            throw std::runtime_error("Vector size must be positive integer");
        }
        data_ = std::vector<T>(size_, 0);
    }
    
    T& operator[](int64_t idx) {
        return data_.at(idx);
    }

    const T& operator[](int64_t idx) const {
        return data_.at(idx);
    }

    const int64_t size() const { return size_; }

private:
    int64_t size_;
    std::vector<T> data_;
};
