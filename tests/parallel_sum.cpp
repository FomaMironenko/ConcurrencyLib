#include <limits>
#include <cmath>
#include <cassert>
#include <chrono>

#include <vector>
#include <string>

#include <thread>
#include <future>

#include "table.hpp"
#include "rand_gen.hpp"


template <class T>
T computeSum(const std::vector<T>& data_vec) {
    T sum = 0;
    for (const T& element: data_vec) {
        sum += element;
    }
    return sum;
}

template <class T>
T computeSumWithRawData(const std::vector<T>& data_vec) {
    const T* raw_data = data_vec.data();
    int size = static_cast<int>(data_vec.size());
    T sum = 0;
    for (int idx = 0; idx < size; ++idx) {
        sum += raw_data[idx];
    }
    return sum;
}

template <int64_t num_workers, class T>
T parallelComputeSum(const std::vector<T>& data_vec) {
    std::vector<std::future<T>> batch_results(num_workers);
    int64_t batch_size = (data_vec.size() / num_workers) + 1;
    const T* raw_data = data_vec.data();
    for (int i_worker = 0; i_worker < num_workers; ++i_worker) {
        int64_t begin = i_worker * batch_size;
        int64_t end = (i_worker + 1) * batch_size;
        end = std::min<int64_t>(data_vec.size(), end);

        batch_results[i_worker] = std::async(std::launch::async, [raw_data, begin, end] {
            T local_sum = 0;
            for (int64_t idx = begin; idx < end; ++idx) {
                local_sum += raw_data[idx];
            }
            return local_sum;
        });

    }
    T sum = 0;
    for (auto& result : batch_results) {
        sum += result.get();
    }
    return sum;
}



StatsTable table(10, 5);

template <class Func>
void gatherPerformanceStats(Func sumCalc, size_t array_size, const std::string& method_name) {

    assert(array_size <= 100'000'000'000 && "Arrays greater than 10^9 are not supported");
    using DataType = int;

    // Guarantee that all functions will run with the same input data
    constexpr int seed = 72874;
    LightweightPRG<int> PRG(-100, 101, seed);

    double sum_time = 0;
    double max_time = std::numeric_limits<double>::min();
    double min_time = std::numeric_limits<double>::max();
    constexpr size_t num_iters = 2;
    std::vector<DataType> data_vec(array_size);

    for (size_t iter = 0; iter < num_iters; ++iter) {
        // Generate
        for (size_t idx = 0; idx < array_size; ++idx) {
            data_vec[idx] = PRG.next();
        }
        // Run
        auto start = std::chrono::high_resolution_clock::now();
        sumCalc(data_vec);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1'000'000.0;
        // Process
        sum_time += ms;
        max_time = std::max<double>(max_time, ms);
        min_time = std::min<double>(min_time, ms);
    }

    // Update table
    table.addEntry(method_name, min_time, sum_time / num_iters, max_time);
}


int main() {
    table.addHeader();
    table.dumpAndFlush();
    constexpr size_t array_size = 100'000'000;
    gatherPerformanceStats(computeSumWithRawData<int>, array_size, "Vecotor sum");
    table.dumpAndFlush();
    gatherPerformanceStats(computeSum<int>, array_size, "Raw array sum");
    table.dumpAndFlush();
    gatherPerformanceStats(parallelComputeSum<2, int>, array_size, "2 workers");
    table.dumpAndFlush();
    gatherPerformanceStats(parallelComputeSum<4, int>, array_size, "4 workers");
    table.dumpAndFlush();
    gatherPerformanceStats(parallelComputeSum<8, int>, array_size, "8 workers");
    table.dumpAndFlush();
}
