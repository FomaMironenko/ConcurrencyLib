#include <cmath>
#include <functional>
#include <random>

#include "utils/timer.hpp"
#include "utils/tester.hpp"
#include "utils/matrix.hpp"

#include "async_function.hpp"
#include "thread_pool.hpp"
#include "task_group.hpp"


Matrix<int64_t> simpleMultiply(const Matrix<int64_t>& lhs, const Matrix<int64_t>& rhs) {
    auto rows = lhs.rows();
    auto cols = rhs.cols();
    if (lhs.cols() != rhs.rows()) {
        throw std::runtime_error("Invalid matrix sizes");
    }
    Matrix<int64_t> result(rows, cols);
    auto mid = lhs.cols();
    for (int col = 0; col < cols; ++col) {
        for (int row = 0; row < rows; ++row) {
            for (int idx = 0; idx < mid; ++idx) {
                result[row][col] += lhs[row][idx] * rhs[idx][col];
            }
        }
    }
    return result;
}

ColumnVec<int64_t> multiplyRowByMtx(const RowView<int64_t> lhs,
                                    const Matrix<int64_t>& rhs_t
) {
    ColumnVec<int64_t> result(rhs_t.rows());
    for (int row = 0; row < rhs_t.rows(); ++row) {
        for (int col = 0; col < rhs_t.cols(); ++col) {
            result[row] += lhs[col] * rhs_t[row][col];
        }
    }
    return result;
}

Matrix<int64_t> parallelMultiply(const Matrix<int64_t>& lhs, const Matrix<int64_t>& rhs, ThreadPool& tp) {
    auto rows = lhs.rows();
    auto cols = rhs.cols();
    if (lhs.cols() != rhs.rows()) {
        throw std::runtime_error("Invalid matrix sizes");
    }
    // Transpose in order to prevent cache misses
    Matrix<int64_t> rhs_t(rhs.cols(), rhs.rows());
    for (int row = 0; row < rhs.rows(); ++row) {
        for (int col = 0; col < rhs.cols(); ++col) {
            rhs_t[col][row] = rhs[row][col];
        }
    }
    // Asynchronously work out result
    auto asyncComputeRow = make_async(tp, multiplyRowByMtx);
    GroupAll<ColumnVec<int64_t>> out_rows;
    for (int64_t row = 0; row < rows; ++row) {
        out_rows.join(asyncComputeRow(lhs[row], std::cref(rhs_t)));
    }
    return out_rows
        .merge(tp)
        .then<Matrix<int64_t>>([rows, cols](std::vector<ColumnVec<int64_t>> prod_rows) {
            Matrix<int64_t> result(rows, cols);
            if (static_cast<int64_t>(prod_rows.size()) != rows) {
                throw std::logic_error("Unexpected number of results from pool");
            }
            for (int row = 0; row < rows; ++row) {
                RowView<int64_t> dst_row = result[row];
                ColumnVec<int64_t> src_row = std::move(prod_rows[row]);
                for (int col = 0; col < cols; ++col) {
                    dst_row[col] = src_row[col];
                }
            }
            return result;
        })
        .get();
}


template <int num_workers>
DEFINE_TEST(testParallelMultiplication) {
    ThreadPool tp(num_workers);
    constexpr int NUM_ITER = 1000;

    std::mt19937 PRG;
    std::uniform_int_distribution<int> size_dist(1, 50);
    std::uniform_int_distribution<int64_t> elt_dist(-10, 10);

    for (int iter = 0; iter < NUM_ITER; ++iter) {
        int rows = size_dist(PRG);
        int mid  = size_dist(PRG);
        int cols = size_dist(PRG);
        Matrix<int64_t> lhs(rows, mid);
        Matrix<int64_t> rhs(mid, cols);
        // Generate random matrices
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < mid; ++col) {
                lhs[row][col] = elt_dist(PRG);
            }
        }
        for (int row = 0; row < mid; ++row) {
            for (int col = 0; col < cols; ++col) {
                rhs[row][col] = elt_dist(PRG);
            }
        }
        // Multiply
        Matrix<int64_t> expected = simpleMultiply(lhs, rhs);
        Matrix<int64_t> actual = parallelMultiply(lhs, rhs, tp);
        ASSERT_EQ(expected.rows(), actual.rows());
        ASSERT_EQ(expected.cols(), actual.cols());
        for (int row = 0; row < expected.rows(); ++row) {
            for (int col = 0; col < expected.cols(); ++col) {
                ASSERT_EQ(expected[row][col], actual[row][col]);
            }
        }
    }
}


int main() {
    RUN_TEST(testParallelMultiplication<1>, "1 Worker");
    RUN_TEST(testParallelMultiplication<2>, "2 Workers");
    RUN_TEST(testParallelMultiplication<4>, "4 Workers");
    COMPLETE();
}
