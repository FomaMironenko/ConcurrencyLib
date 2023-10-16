#include <cmath>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"
#include "utils/matrix.hpp"

#include "thread_pool.hpp"


template <class T>
T computeElement(const RowView<T> row, const ColumnVec<T>& col, T rhs, int64_t idx) {
    T sum = 0;
    for (int64_t jdx = 0; jdx < col.size(); ++jdx) {
        sum += row[jdx] * col[jdx];
    }
    constexpr T eps = 0.01;
    return col[idx] - eps * (sum - rhs);
}

template <class T>
ColumnVec<T> resolveViaIterations(const Matrix<T>& mtx, const ColumnVec<T> rhs, ThreadPool& pool) {
    if (mtx.cols() != mtx.rows()) {
        throw std::runtime_error("Non-sqare matrixes not supported");
    }
    if (mtx.rows() != rhs.size()) {
        throw std::runtime_error("Mismatching matrix and rhs size");
    }
    int64_t size = rhs.size();
    ColumnVec<T> result(size);
    ColumnVec<T> tmp_result(size);
    for (int iter = 0; iter < 1000; ++iter) {
        std::vector<AsyncResult<T>> tmp;
        for (int64_t idx = 0; idx < size; ++idx) {
            auto fut = pool.submit<T>(computeElement<T>, mtx[idx], std::cref(result), rhs[idx], idx);
            tmp.push_back(std::move(fut));
        }
        for (int64_t idx = 0; idx < size; ++idx) {
            tmp_result[idx] = tmp[idx].get();
        }
        // Two different loops to prevent data race
        for (int64_t idx = 0; idx < size; ++idx) {
            result[idx] = tmp_result[idx];
        }
    }
    return result;
}


int main() {
    ThreadPool pool(4);

    Matrix<double> mtx(3, 3);
    mtx[0][0] = mtx[1][1] = mtx[2][2] = 3;
    ColumnVec<double> rhs(3);
    rhs[0] = rhs[1] = rhs[2] = 1;

    ColumnVec<double> ans = resolveViaIterations<double>(mtx, rhs, pool);
    std::cout << "[";
    for (int idx = 0; idx < 3; ++idx) {
        std::cout << ans[idx];
        std::cout << ", ";
    }
    std::cout << "]" << std::endl;
}
