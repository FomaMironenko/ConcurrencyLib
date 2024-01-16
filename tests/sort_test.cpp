#include <cmath>
#include <algorithm>
#include <random>

#include "utils/logger.hpp"
#include "test_utils/timer.hpp"
#include "test_utils/tester.hpp"
#include "test_utils/table.hpp"

#include "async_function.hpp"
#include "thread_pool.hpp"
#include "task_group.hpp"


namespace details {

template <class Iterator>
std::pair<Iterator, Iterator> split(Iterator begin, Iterator end) {
    Iterator pivot = begin + std::distance(begin, end) / 2;
    std::swap(*begin, *pivot);
    Iterator first_pivot = begin;
    Iterator first_after_pivot = std::next(first_pivot);
    for (Iterator iter = first_after_pivot; iter < end; ++iter) {
        if (*first_pivot < *iter) {
            continue;
        } else if (*iter < *first_pivot) {
            std::swap(*first_pivot, *iter);
            std::swap(*iter, *first_after_pivot);
            ++first_pivot;
            ++first_after_pivot;
        } else /* first_pivot == *pivot */ {
            std::swap(*first_after_pivot, *iter);
            ++first_after_pivot;
        }
    }
    return {first_pivot, first_after_pivot};
}

template <class Iterator>
AsyncResult<void> divideAndSort(Iterator begin, Iterator end, ThreadPool& pool) {
    if (std::distance(begin, end) <= 1) {
        return AsyncResult<void>::instant();
    }
    return call_async<std::pair<Iterator, Iterator>>(pool,
            split<Iterator>, begin, end
        ).template then<AsyncResult<void>>([begin, end, &pool](std::pair<Iterator, Iterator> middle) {
            TaskGroup<void> sort_halves;
            sort_halves.join(divideAndSort(begin, middle.first, pool));
            sort_halves.join(divideAndSort(middle.second, end, pool));
            return sort_halves.all();
        }).flatten();
}

}  // namespace details


template <class Iterator>
void parallelQuickSort(Iterator begin, Iterator end, ThreadPool& pool) {
    details::divideAndSort(begin, end, pool).wait();
}


DEFINE_TEST(test_sort) {
    Timer timer;
    StatsTable table(10, 5);
    table.addHeader();

    std::mt19937 PRG;
    std::uniform_int_distribution<int> elt_dist(-100, 100);
    constexpr int NUM_ITERS = 10;
    constexpr int SIZE = 500'000;

    std::vector<int> num_workers_list = {1, 2, 4, 6};
    for (int num_workers : num_workers_list) {

        ThreadPool pool(num_workers);
        double sum_time = 0;
        double max_time = std::numeric_limits<double>::min();
        double min_time = std::numeric_limits<double>::max();
        for (int iter = 0; iter < NUM_ITERS; ++iter) {
            std::vector<int> my_sort(SIZE, 0);
            for (int & elt : my_sort) {
                elt = elt_dist(PRG);
            }
            std::vector stl_sort = my_sort;
            std::sort(stl_sort.begin(), stl_sort.end());

            timer.start();
            parallelQuickSort(my_sort.begin(), my_sort.end(), pool);
            double ms = timer.elapsedMilliseconds();
            // Process
            sum_time += ms;
            max_time = std::max<double>(max_time, ms);
            min_time = std::min<double>(min_time, ms);

            ASSERT_EQ(my_sort, stl_sort);
        }
        std::string name = std::to_string(num_workers) + " Workers";
        table.addEntry(name, min_time, sum_time / NUM_ITERS, max_time);

    }
    table.dump();
}

int main() {
    RUN_TEST(test_sort, "Test sort");
    COMPLETE();
}
