#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>

#include <unordered_map>
#include <vector>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"

#include "thread_pool.hpp"

using namespace std::chrono_literals;


bool just_works() {
    ThreadPool pool(4);
    auto fut_bool = pool.submit<bool>([]() {return true; });
    auto fut_int = pool.submit<int>([]() { return 42; });
    auto fut_double = pool.submit<double>([]() { return 3.14; });
    auto fut_string = pool.submit<std::string>([]() { return "string"; });
    ASSERT_EQ(fut_bool.get(), true);
    ASSERT_EQ(fut_int.get(), 42);
    ASSERT_EQ(fut_double.get(), 3.14);
    ASSERT_EQ(fut_string.get(), "string");
    return true;
}

bool subscription_just_works() {
    ThreadPool pool(2);
    int source = 3;
    auto fut_string = pool.submit<int>(
        [](int input) { return input; },
        source
    ).then<int>(
        [](int result) { return result * result; }
    ).then<int>(
        [](int result) { return result + 1; }
    ).then<std::string>(
        [](int result) { return std::to_string(result); }
    );
    ASSERT_EQ(fut_string.get(), "10");
    return true;
}

bool subscription_error() {
    ThreadPool pool(2);
    bool poisoned = false;
    auto fut = pool.submit<int>(
        []() { return 42; }
    ).then<int>(
        [](int result) {
            throw std::runtime_error("Oops...");
            return result * result;
        }
    ).then<int>(
        [&poisoned](int result) {
            poisoned = true;
            std::this_thread::sleep_for(100s);
            return result + 1;
        }
    ).then<int>(
        [&poisoned](int result) {
            poisoned = true;
            std::this_thread::sleep_for(100s);
            return result / 2;
        }
    );
    try {
        fut.get();
        ASSERT(false);  // must throw
    } catch (const std::runtime_error& err) {
        ASSERT_EQ(err.what(), std::string("Oops..."));
    } catch (...) {
        ASSERT(false);
    }
    ASSERT(!poisoned);
    return true;
}

bool map_reduce() {
    ThreadPool pool(4);
    std::vector<AsyncResult<uint32_t>> mapped;
    uint32_t expected = 0;  // unsigned integer overflow is not a UB
    constexpr uint32_t num_iters = 10'000;
    for (uint32_t iter = 0; iter < num_iters; ++iter) {
        auto async_res = pool.submit<uint32_t>(
            std::pow<uint32_t>,
            iter, 2
        ).then<uint32_t>(
            [](uint32_t val) { return val * val * val; }
        );
        mapped.push_back(std::move(async_res));
        int32_t cube = iter * iter * iter;  // do not use std::pow to allow overflows
        expected += cube * cube;
    }
    uint32_t reduced = 0;
    for (auto & fut : mapped) {
        reduced += fut.get();
    }
    ASSERT_EQ(reduced, expected);
    return true;
}

template <size_t num_workers>
bool test_starvation() {
    ThreadPool pool(num_workers);
    std::unordered_map<std::thread::id, size_t> worker_cnt;
    std::mutex mtx;
    constexpr size_t num_iters = 10'000;

    std::vector<AsyncResult<void>> task_handles;
    for (size_t iter = 0; iter < num_iters; ++iter) {
        auto result = pool.submit<void>(
            [&worker_cnt, &mtx]() mutable {
                std::lock_guard guard(mtx);
                ++worker_cnt[std::this_thread::get_id()];
            }
        );
        task_handles.push_back(std::move(result));
    }
    for (auto & handle : task_handles) {
        handle.get();
    }
    ASSERT_EQ(worker_cnt.size(), num_workers);
    for (const auto & [id, cnt] : worker_cnt) {
        LOG_INFO << cnt << " / " << num_iters;
        ASSERT(cnt >= (num_iters / num_workers) / 3);
    }

    return true;
}

template <size_t num_workers>
bool test_then_starvation() {
    ThreadPool pool(num_workers);
    std::unordered_map<std::thread::id, size_t> worker_cnt;
    constexpr size_t num_iters = 100'000;

    AsyncResult<size_t> fut = pool.submit<size_t>([]() { return 0; });
    for (size_t iter = 0; iter < num_iters; ++iter) {
        fut = fut.then<size_t>([&worker_cnt](size_t val) mutable {
            // Synchronized via continuation
            ++worker_cnt[std::this_thread::get_id()];
            return val + 1;
        });
    }

    ASSERT_EQ(fut.get(), num_iters);
    ASSERT_EQ(worker_cnt.size(), num_workers);
    for (const auto & [id, cnt] : worker_cnt) {
        LOG_INFO << cnt << " / " << num_iters;
        ASSERT(cnt >= (num_iters / num_workers) / 3);
    }

    return true;
}


int main() {
    TEST(just_works, "Just works");
    TEST(subscription_just_works, "Subscription just works");
    TEST(subscription_error, "Error in subscription");
    TEST(map_reduce, "Map reduce");
    TEST(test_starvation<2>, "Starvation test with 2 workers");
    TEST(test_starvation<5>, "Starvation test with 6 workers");
    TEST(test_then_starvation<2>, "Continuation starvation test with 2 workers");
    TEST(test_then_starvation<5>, "Continuation starvation test with 6 workers");
    return EXIT_SUCCESS;
}
