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
#include "async_function.hpp"

using namespace std::chrono_literals;


bool just_works() {
    ThreadPool pool(4);
    auto fut_bool = call_async<bool>(pool, [](){return true; });
    auto fut_int = call_async<int>(pool, []() { return 42; });
    auto fut_double = call_async<double>(pool, []() { return 3.14; });
    auto fut_string = call_async<std::string>(pool, []() { return "string"; });
    ASSERT_EQ(fut_bool.get(), true);
    ASSERT_EQ(fut_int.get(), 42);
    ASSERT_EQ(fut_double.get(), 3.14);
    ASSERT_EQ(fut_string.get(), "string");
    return true;
}

bool subscription_just_works() {
    ThreadPool pool(2);
    int source = 3;
    auto fut_string = call_async<int>(pool,
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

template <class T>
T binPow(T base, T pow) {
    if (pow == 0) {
        return 1;
    } else if (pow % 2 == 0) {
        int tmp = binPow(base, pow / 2);
        return tmp * tmp;
    } else {
        return base * binPow(base, pow - 1);
    }
}

bool make_async_just_works() {
    ThreadPool pool(2);

    auto async_pow = make_async(pool, binPow<int64_t>);
    std::vector<AsyncResult<int64_t>> results;
    int64_t expected = 0;
    for (int64_t i = 0; i < 100; ++i) {
        results.push_back(async_pow(i, 2));
        expected += std::pow<int64_t>(i, 2);
    }

    int64_t actual = 0;
    for (auto & asunc_res : results) {
        actual += asunc_res.get();
    }
    ASSERT_EQ(actual, expected);

    return true;
}

bool flatten_is_async() {
    ThreadPool pool(2);
    Timer time;
    AsyncResult<int> fut = call_async<int>(pool, []() {
        std::this_thread::sleep_for(50ms);
        return 42;
    }).then<AsyncResult<int>>([&pool, &time](int input) {
        double elapsedMs = time.elapsedMilliseconds();
        if (elapsedMs < 50 || elapsedMs > 50 + 20) {
            throw std::runtime_error("Schedule error 1");
        }
        std::this_thread::sleep_for(50ms);
        return call_async<int>(pool, [&time](int val) {
            double elapsedMs = time.elapsedMilliseconds();
            if (elapsedMs < 100 || elapsedMs > 100 + 20) {
                throw std::runtime_error("Schedule error 2");
            }
            std::this_thread::sleep_for(50ms);
            return val * 2;
        }, input);
    }).flatten();
    double elapsedMs = time.elapsedMilliseconds();
    ASSERT(elapsedMs < 20);
    int value = 0;
    try {
        value = fut.get();
    } catch (const std::exception & err) {
        LOG_ERR << err.what();
        FAIL();
    }
    elapsedMs = time.elapsedMilliseconds();
    ASSERT(value == 42 * 2);
    ASSERT(elapsedMs > 150);
    return true;
}

bool flatten_error() {
    ThreadPool pool(2);
    // Error in the first level
    auto fut1 = call_async<AsyncResult<int>>(pool, [&pool]() {
        throw std::runtime_error("First level err");
        return AsyncResult<int>::instant(pool, 0);
    }).flatten();
    try {
        fut1.get();
        FAIL();
    } catch (const std::exception & err) {
        ASSERT_EQ(err.what(), std::string("First level err"))
    }
    // Error in the second level
    auto fut2 = call_async<AsyncResult<int>>(pool, [&pool]() {
        return call_async<int>(pool, []() {
            throw std::runtime_error("Second level err");
            return 0;
        });
    }).flatten();
    try {
        fut2.get();
        FAIL();
    } catch (const std::exception & err) {
        ASSERT_EQ(err.what(), std::string("Second level err"))
    }
    return true;
}

bool subscription_error() {
    ThreadPool pool(2);
    bool poisoned = false;
    auto fut = call_async<int>(pool,
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
        FAIL();  // must throw
    } catch (const std::runtime_error& err) {
        ASSERT_EQ(err.what(), std::string("Oops..."));
    } catch (...) {
        FAIL();
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
        auto async_res = call_async<uint32_t>(pool, 
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

    AsyncFunction<void()> vote_for_worker = make_async(pool, 
        [&worker_cnt, &mtx]() mutable {
            std::lock_guard guard(mtx);
            ++worker_cnt[std::this_thread::get_id()];
        });
    std::vector<AsyncResult<void>> task_handles;
    for (size_t iter = 0; iter < num_iters; ++iter) {
        task_handles.push_back(vote_for_worker());
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

    AsyncResult<size_t> fut = AsyncResult<size_t>::instant(pool, 0);
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
    TEST(flatten_is_async, "Flatten is async");
    TEST(make_async_just_works, "make_async just works");
    TEST(subscription_error, "Error in subscription");
    TEST(flatten_error, "Error in flatten");
    TEST(map_reduce, "Map reduce");
    TEST(test_starvation<2>, "Starvation test with 2 workers");
    TEST(test_starvation<5>, "Starvation test with 5 workers");
    TEST(test_then_starvation<2>, "Continuation starvation test with 2 workers");
    TEST(test_then_starvation<5>, "Continuation starvation test with 5 workers");
    return EXIT_SUCCESS;
}
