#pragma once

#include <cmath>
#include <memory>
#include <vector>
#include <iostream>
#include <cstdint>

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

    int64_t size() const {
        return size_;
    }

private:
    int64_t size_;
    T* data_;

template <class U>
friend class Matrix;
};

template <class T>
void dump(const RowView<T> row) {
    for (int64_t idx = 0; idx < row.size(); ++idx) {
        std::cout << row[idx] << ", ";
    }
    std::cout << std::endl;
}



template <class T>
class Matrix {
public:
    Matrix(int64_t rows, int64_t cols) : n_rows_(rows), n_cols_(cols) {
        if (rows <= 0 || cols <= 0) {
            throw std::runtime_error("Matrix dimensions must be positive integers");
        }
        data_ = std::vector<T>(rows * cols, 0);
    }

    RowView<T> operator[](int64_t idx) {
        if (idx < 0 || idx >= n_rows_) {
            LOG_ERR << "Index exceeds number of rows: " << idx << " / " << n_rows_;
            throw std::runtime_error("Index exceeds number of rows");
        }
        return RowView<T>(data_.data() + (n_cols_ * idx), n_cols_);
    }

    const RowView<T> operator[](int64_t idx) const {
        if (idx < 0 || idx >= n_rows_) {
            LOG_ERR << "Index exceeds number of rows: " << idx << " / " << n_rows_;
            throw std::runtime_error("Index exceeds number of rows");
        }
        return RowView<T>(const_cast<T*>(data_.data()) + (n_cols_ * idx), n_cols_);
    }

    int64_t rows() const { return n_rows_; }
    int64_t cols() const { return n_cols_; }

private:
    int64_t n_rows_;
    int64_t n_cols_;
    std::vector<T> data_;
};

template <class T>
void dump(const Matrix<T>& mtx) {
    for (int64_t row = 0; row < mtx.rows(); ++row) {
        dump(mtx[row]);
    }
    std::cout << std::endl;
}


template <class T>
class ColumnVec {
public:
    explicit ColumnVec(int64_t size) {
        if (size <= 0) {
            throw std::runtime_error("Vector size must be positive integer");
        }
        data_ = std::vector<T>(size, 0);
    }
    explicit ColumnVec(std::vector<T> vec) : data_(std::move(vec)) {    }
    
    T& operator[](int64_t idx) {
        return data_.at(idx);
    }

    const T& operator[](int64_t idx) const {
        return data_.at(idx);
    }

    const int64_t size() const { return static_cast<int64_t>(data_.size()); }

private:
    std::vector<T> data_;
};

template <class T>
void dump(const ColumnVec<T> col) {
    for (int64_t idx = 0; idx < col.size(); ++idx) {
        std::cout << col[idx] << ", ";
    }
    std::cout << std::endl;
}
