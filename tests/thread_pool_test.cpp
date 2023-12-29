#include <iostream>
#include <iomanip>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdint>
#include <string>
#include <sstream>

#include <unordered_map>
#include <vector>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"

#include "thread_pool.hpp"
#include "async_function.hpp"

using namespace std::chrono_literals;


DEFINE_TEST(just_works) {
    ThreadPool pool(4);
    auto fut_bool = call_async<bool>(pool, [](){ return true; });
    auto fut_int = call_async<int>(pool, []() { return 42; });
    auto fut_double = call_async<double>(pool, []() { return 3.14; });
    auto fut_string = call_async<std::string>(pool, []() { return "string"; });
    ASSERT_EQ(fut_bool.get(), true);
    ASSERT_EQ(fut_int.get(), 42);
    ASSERT_EQ(fut_double.get(), 3.14);
    ASSERT_EQ(fut_string.get(), "string");
}


DEFINE_TEST(to_std_just_works) {
    ThreadPool pool(4);
    auto std_fut_bool = call_async<bool>(pool, [](){ return true; }).to_std();
    auto std_fut_int = call_async<int>(pool, []() { return 42; }).to_std();
    auto std_fut_double = call_async<double>(pool, []() { return 3.14; }).to_std();
    auto std_fut_string = call_async<std::string>(pool, []() { return "string"; }).to_std();
    auto std_fut_void = call_async<void>(pool, []() { return; }).to_std();
    ASSERT_EQ(std_fut_bool.get(), true);
    ASSERT_EQ(std_fut_int.get(), 42);
    ASSERT_EQ(std_fut_double.get(), 3.14);
    ASSERT_EQ(std_fut_string.get(), "string");
    std_fut_void.get();
}


template <class T>
struct WorstType {
    WorstType() = delete;
    explicit WorstType(T val) : val(val) {    }

    WorstType(const WorstType&) = delete;
    WorstType(WorstType&&) = default;
    WorstType& operator=(const WorstType&) = delete;
    WorstType& operator=(WorstType&&) = default;

    T val;
};

DEFINE_TEST(worst_type) {
    ThreadPool pool(1);
    auto fut = call_async<WorstType<int>>(pool, [](){ return WorstType<int>(42); });
    ASSERT_EQ(fut.get().val, 42);
}


DEFINE_TEST(subscription_just_works) {
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

    int flag = 0;
    call_async<void>(pool,
        [&flag]() { flag += 1; }
    ).then<void>(
        [&flag]() { flag += 1; }
    ).then<int>(
        []() { return 42; }
    ).then<void>(
        [&flag](int val) { flag += val; }
    ).wait();
    ASSERT_EQ(flag, 1 + 1 + 42);
}


DEFINE_TEST(moveonly_arguments_in_subscription) {
    ThreadPool pool(2);
    auto fut_string = call_async<int>(pool,
        []() { return 42; }
    ).then<WorstType<int>>(
        [](int result) { return WorstType<int>(result); }
    ).then<WorstType<double>>(
        [](auto result) {
            auto double_res = WorstType<double>(result.val);
            double_res.val += 0.5;
            return double_res;
        }
    ).then<std::string>(
        [](auto result) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << result.val;
            return std::string(ss.str());
        }
    );
    ASSERT_EQ(fut_string.get(), "42.50");
}


template <class T>
T binPow(T base, T pow) {
    if (pow == 0) {
        return 1;
    }
    if (pow % 2 == 0) {
        T tmp = binPow(base, pow / 2);
        return tmp * tmp;
    }
    return base * binPow(base, pow - 1);
}


DEFINE_TEST(make_async_just_works) {
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
}


DEFINE_TEST(flatten_is_async) {
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
}


DEFINE_TEST(flatten_void) {
    ThreadPool pool(2);
    int value = 0;
    auto fut = call_async<void>(pool,
        [&value]() { ++value; }
    ).then<AsyncResult<void>>(
        [&value, &pool]() {
            ++value;
            return call_async<int>(pool,
                []() { return 1; }
            ).then<void>(
                [&value](int val) { value += val; }
            );
        }
    ).then<AsyncResult<void>>(
        [&value] (AsyncResult<void> async_void) {
            return async_void.then<void>([&value]() { ++value; });
        }
    );
    auto fut_void = fut.flatten();
    fut_void.wait();
    ASSERT_EQ(value, 4);
}


DEFINE_TEST(flatten_error) {
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
}


DEFINE_TEST(then_with_options) {
    ThreadPool pool_one(1);
    std::atomic<bool> fired { false };
    auto fut = call_async<void>(pool_one, []() {
        std::this_thread::sleep_for(50ms);
    });
    auto worker = call_async<void>(pool_one, [&fired]() {
        fired.store(true);
    });
    bool no_reschedule = false;
    fut = fut.then<void>([&no_reschedule, &fired]() {
        no_reschedule = !fired.load();
    }, ThenPolicy::NoSchedule);
    worker.wait();
    fut.wait();
    ASSERT(no_reschedule);

    AsyncResult<std::thread::id> executed_on;
    // Lazy policy must reschedule task
    executed_on = AsyncResult<void>::instant(pool_one)
        .then<std::thread::id>([](){ return std::this_thread::get_id(); }, ThenPolicy::Lazy);
    ASSERT_INEQ(executed_on.get(), std::this_thread::get_id());
    // Eager policy must not block calling thread if result has been already produced
    executed_on = AsyncResult<void>::instant(pool_one)
        .then<std::thread::id>([](){ return std::this_thread::get_id(); }, ThenPolicy::Eager);
    ASSERT_INEQ(executed_on.get(), std::this_thread::get_id());
    // NoSchedule policy must not reschedule anyway
    executed_on = AsyncResult<void>::instant(pool_one)
        .then<std::thread::id>([](){ return std::this_thread::get_id(); }, ThenPolicy::NoSchedule);
    ASSERT_EQ(executed_on.get(), std::this_thread::get_id());
    pool_one.stop();

    ThreadPool pool_two(4);
    constexpr int NUM_ITERS = 10000;
    std::vector<AsyncResult<bool>> just_then_res;
    auto main_tid = std::this_thread::get_id();
    for (int i = 0; i < NUM_ITERS; ++i) {
        just_then_res.push_back(
            call_async<std::thread::id>(pool_two, []() {
                return std::this_thread::get_id();
            }).then<bool>([&main_tid](std::thread::id prev_tid) {
                auto current_tid = std::this_thread::get_id();
                return (current_tid == main_tid) || (current_tid == prev_tid);
            }, ThenPolicy::NoSchedule)
        );
        just_then_res.push_back(
            call_async<std::thread::id>(pool_two, []() {
                return std::this_thread::get_id();
            }).then<bool>([&main_tid](std::thread::id) {
                auto current_tid = std::this_thread::get_id();
                return current_tid != main_tid;
            }, ThenPolicy::Eager)
        );
    }
    bool all_good = true;
    for (int i = 0; i < NUM_ITERS; ++i) {
        all_good = all_good && just_then_res[i].get();
    }
    ASSERT(all_good);
}


DEFINE_TEST(subscription_error) {
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
}


DEFINE_TEST(map_reduce) {
    ThreadPool pool(4);
    std::vector<AsyncResult<uint32_t>> mapped;
    uint32_t expected = 0;  // unsigned integer overflow is not a UB
    constexpr uint32_t num_iters = 10'000;
    for (uint32_t iter = 0; iter < num_iters; ++iter) {
        auto async_res = call_async<uint32_t>(pool, 
            binPow<uint32_t>,
            iter, 2
        ).then<uint32_t>(
            [](uint32_t val) { return binPow<uint32_t>(val, 3); }
        );
        mapped.push_back(std::move(async_res));
        uint32_t cube = iter * iter * iter;  // do not use std::pow to allow overflows
        expected += cube * cube;
    }
    uint32_t reduced = 0;
    for (auto & fut : mapped) {
        reduced += fut.get();
    }
    ASSERT_EQ(reduced, expected);
}


DEFINE_TEST(in_does_transfer) {
    ThreadPool pool_1(1);
    ThreadPool pool_2(1);
    auto tid_1 = call_async<std::thread::id>(pool_1, []() { return std::this_thread::get_id(); }).get();
    auto tid_2 = call_async<std::thread::id>(pool_2, []() { return std::this_thread::get_id(); }).get();
    ASSERT(tid_1 != tid_2);
    
    constexpr int NUM_ITERS = 1000;
    std::vector<AsyncResult<void>> results;
    std::atomic<bool> ok_1{true}, ok_2{true};
    auto check_good_1 = [&ok_1, tid_1]() { if (std::this_thread::get_id() != tid_1) ok_1.store(false); };
    auto check_good_2 = [&ok_2, tid_2]() { if (std::this_thread::get_id() != tid_2) ok_2.store(false); };
    for (int i = 0; i < NUM_ITERS; ++i) {
        results.push_back(
            AsyncResult<void>::instant(pool_1)
            .in(pool_1).then<void>(check_good_1).then<void>(check_good_1)
            .in(pool_2).then<void>(check_good_2)
            .in(pool_1).then<void>(check_good_1)
            .in(pool_2).then<void>(check_good_2)
            .in(pool_2).then<void>(check_good_2)
        );
    }
    for (auto & result : results) {
        result.wait();
    }
    ASSERT(ok_1.load());
    ASSERT(ok_2.load());
}


template <size_t num_workers>
DEFINE_TEST(test_starvation) {
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
}


template <size_t num_workers>
DEFINE_TEST(test_then_starvation) {
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
}


int main() {
    RUN_TEST(just_works, "Just works")
    RUN_TEST(to_std_just_works, "to_std just works")
    RUN_TEST(worst_type, "Async call with moveonly & non-default-constructible type")
    RUN_TEST(subscription_just_works, "Subscription just works")
    RUN_TEST(moveonly_arguments_in_subscription, "Subscription with moveonly arguments")
    RUN_TEST(flatten_is_async, "Flatten is async")
    RUN_TEST(flatten_void, "Flatten works with void")
    RUN_TEST(then_with_options, "just_then is eager")
    RUN_TEST(make_async_just_works, "make_async just works")
    RUN_TEST(subscription_error, "Error in subscription")
    RUN_TEST(flatten_error, "Error in flatten")
    RUN_TEST(map_reduce, "Map reduce")
    RUN_TEST(in_does_transfer, "In transfers execution to thread pool");
    RUN_TEST(test_starvation<2>, "Starvation test with 2 workers")
    RUN_TEST(test_starvation<5>, "Starvation test with 5 workers")
    RUN_TEST(test_then_starvation<2>, "Continuation starvation test with 2 workers")
    RUN_TEST(test_then_starvation<5>, "Continuation starvation test with 5 workers")
    COMPLETE()
}
