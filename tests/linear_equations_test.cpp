#include <cmath>

#include "utils/logger.hpp"
#include "test_utils/timer.hpp"
#include "test_utils/tester.hpp"
#include "test_utils/matrix.hpp"
#include "test_utils/linalg.hpp"

#include "async_function.hpp"
#include "thread_pool.hpp"
#include "task_group.hpp"


template <class T>
double computeElement(const RowView<T> row, const ColumnVec<T>& col, T rhs, int64_t idx) {
    constexpr T step = 0.01;
    double dot_prod = Linalg::dot(row, col);
    return col[idx] - step * (dot_prod - rhs);
}

template <class T>
ColumnVec<double> resolveViaIterations(const Matrix<T>& mtx, const ColumnVec<T> rhs, ThreadPool& pool) {
    if (mtx.cols() != mtx.rows()) {
        throw std::runtime_error("Non-sqare matrixes not supported");
    }
    if (mtx.rows() != rhs.size()) {
        throw std::runtime_error("Mismatching matrix and rhs size");
    }
    int64_t size = rhs.size();
    auto asyncComputeElement = make_async(pool, computeElement<T>);
    ColumnVec<double> result(size);
    for (int iter = 0; iter < 1000; ++iter) {
        TaskGroup<double> vector_elements;
        for (int64_t idx = 0; idx < size; ++idx) {
            vector_elements.join( asyncComputeElement(mtx[idx], std::cref(result), rhs[idx], idx) );
        }
        vector_elements
            .all()
            .in(pool)
            .template then<void>([&result](std::vector<T> updated) {
                if (static_cast<int64_t>(updated.size()) != result.size()) {
                    LOG_ERR << "Mismatching result (" << result.size() << ") and output (" << updated.size() << ") sizes";
                    throw std::logic_error("Wrong vector update size");
                }
                for (size_t idx = 0; idx < updated.size(); ++idx) {
                    result[idx] = updated[idx];
                }
            }, ThenPolicy::Eager)
            .get();
    }
    return result;
}

template <class T>
ColumnVec<double> resolveViaConjugateGrads(const Matrix<T>& mtx, const ColumnVec<T> rhs, ThreadPool& pool) {
    if (mtx.cols() != mtx.rows()) {
        throw std::runtime_error("Non-sqare matrixes not supported");
    }
    if (mtx.rows() != rhs.size()) {
        throw std::runtime_error("Mismatching matrix and rhs size");
    }
    int64_t size = rhs.size();
    auto asyncDot = make_async( pool, Linalg::dot<RowView<T>, ColumnVec<double>> );

    double alpha = 0, beta = 0;
    ColumnVec<double> x(size), r = rhs, z = rhs;

    for (int iter = 0; iter < size * 2; ++iter) {
        TaskGroup<double> mtx_mul_tasks;
        for (int idx = 0; idx < size; ++idx) {
            mtx_mul_tasks.join( asyncDot(mtx[idx], r) );
        }
        double prev_rr = Linalg::dot(r, r);
        ColumnVec<double> Az = mtx_mul_tasks
            .all()
            .then<ColumnVec<double>>([](std::vector<double> res) {
                return ColumnVec<double>(std::move(res));
            }, ThenPolicy::NoSchedule)
            .get();
        double Azz = Linalg::dot(Az, z);
        if (prev_rr == 0) {
            throw std::runtime_error("r_{k-1} * r_{k-1} == 0");
        }
        if (Azz == 0) {
            throw std::runtime_error("Az * z == 0");
        }

        alpha = prev_rr / Azz;
        for (int idx = 0; idx < size; ++idx) {
            x[idx] = x[idx] + alpha * z[idx];
            r[idx] = r[idx] - alpha * Az[idx];
        }
        beta = Linalg::dot(r, r) / prev_rr;
        for (int idx = 0; idx < size; ++idx) {
            z[idx] = r[idx] + beta * z[idx];
        }
    }
    return x;
}

int main() {
    ThreadPool pool(4);

    Matrix<double> mtx(3, 3);
    mtx[0][0] = mtx[1][1] = mtx[2][2] = 3;
    ColumnVec<double> rhs(3);
    rhs[0] = rhs[1] = rhs[2] = 1;

    ColumnVec<double> iter_ans = resolveViaIterations<double>(mtx, rhs, pool);
    ColumnVec<double> grad_ans = resolveViaConjugateGrads<double>(mtx, rhs, pool);
    std::cout << "Iterations answer:" << std::endl;
    dump(iter_ans);
    std::cout << "Conj Grad  answer:" << std::endl;
    dump(grad_ans);
}
