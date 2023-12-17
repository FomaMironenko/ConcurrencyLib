#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"

#include "contract.hpp"

using namespace std::chrono_literals;

bool get_blocks() {
    auto [promise, future] = contract<int>();

    Timer timer;
    std::thread prod_thread([promise = std::move(promise)] () mutable {
        std::this_thread::sleep_for(100ms);
        promise.setValue(42);
    });
    int val = future.get();
    double ms = timer.elapsedMilliseconds();
    prod_thread.join();

    ASSERT(val == 42);
    ASSERT(ms >= 100.0);
    ASSERT(ms <= 110.0);
    return true;
}

bool subscription1() {
    int dst = 0;
    auto [promise, future] = contract<int>();
    future.subscribe([&dst] (int result) { dst = result; });
    promise.setValue(42);
    ASSERT_EQ(dst, 42);
    return true;
}

bool subscription2() {
    int dst = 0;
    auto [promise, future] = contract<int>();
    promise.setValue(42);
    future.subscribe([&dst] (int result) { dst = result; });
    ASSERT_EQ(dst, 42);
    return true;
}

bool futures_are_oneshot() {
    // Future::get consumes the state
    auto [pro1, fut1] = contract<int>();
    pro1.setValue(42);
    ASSERT_EQ(fut1.get(), 42);
    try {
        fut1.get();
        FAIL();
    } catch (...) { /*ok*/ }
    try {
        fut1.subscribe([](int){}, nullptr);
        FAIL();
    } catch (...) { /*ok*/ }
    try {
        fut1.wait();
        FAIL();
    } catch (...) { /*ok*/ }
    // Future::subscribe consumes the state
    auto [pro2, fut2] = contract<int>();
    fut2.subscribe([](int){}, nullptr);
    pro2.setValue(42);
    try {
        fut2.get();
        FAIL();
    } catch (...) { /*ok*/ }
    try {
        fut2.subscribe([](int){}, nullptr);
        FAIL();
    } catch (...) { /*ok*/ }
    try {
        fut2.wait();
        FAIL();
    } catch (...) { /*ok*/ }
    return true;
}

bool wait_does_not_consume() {
    // Can get after wait
    auto [pro1, fut1] = contract<int>();
    pro1.setValue(42);
    fut1.wait();
    try {
        ASSERT_EQ(fut1.get(), 42);
    } catch (...) {
        FAIL();
    }
    // Can subscribe after wait
    auto [pro2, fut2] = contract<int>();
    pro2.setValue(42);
    fut2.wait();
    try {
        fut2.subscribe([](int){}, nullptr);
    } catch (...) {
        FAIL();
    }
    return true;
}

bool moveonly_value() {
    auto [promise, future] = contract<std::unique_ptr<std::vector<int> > >();
    std::thread producer([promise = std::move(promise)] () mutable {
        auto vec = std::make_unique<std::vector<int>>(std::initializer_list<int>{1, 2, 3, 4, 5});
        promise.setValue(std::move(vec));
    });
    auto vec = future.get();
    producer.join();
    std::vector expected = {1, 2, 3, 4, 5};
    ASSERT_EQ(*vec, expected);
    return true;
}

bool exception_in_get() {
    auto [promise, future] = contract<int >();
    std::thread producer([promise = std::move(promise)]() mutable {
        try {
            throw std::runtime_error("Producer error");
        } catch (...) {
            promise.setError(std::current_exception());
        }
    });
    producer.join();
    try {
        int val = future.get();
        ASSERT(false);  // must be unreachable
    } catch (const std::exception& err) {
        ASSERT_EQ(err.what(), std::string("Producer error"));
    }
    return true;
}

bool exception_in_subscribe() {
    auto [promise, future] = contract<int >();
    std::thread producer([promise = std::move(promise)]() mutable {
        try {
            throw std::runtime_error("Producer error");
        } catch (...) {
            promise.setError(std::current_exception());
        }
    });
    producer.join();
    bool has_value = false, has_error = false;
    future.subscribe(
        [&has_value] (int) { has_value = true; },
        [&has_error] (std::exception_ptr) { has_error = true; }
    );
    ASSERT(!has_value);
    ASSERT(has_error);
    return true;
}

bool map_reduce() {
    constexpr int num_iters = 1000;
    std::vector<Promise<int>> to_map;
    std::vector<Future<int>> mapped;
    for (int i = 0; i < num_iters; ++i) {
        auto [promise, future] = contract<int>();
        to_map.push_back(std::move(promise));
        mapped.push_back(std::move(future));
    }

    auto [promise, result] = contract<int>();
    int expected = 0;

    std::thread mapper([&expected, to_map = std::move(to_map)]() mutable {
        for (int i = 0; i < num_iters; ++i) {
            expected += i * i;
            to_map[i].setValue(i * i);
        }
    });

    std::thread reducer([promise = std::move(promise), mapped = std::move(mapped)]() mutable {
        int sum_of_squares = 0;
        for (int i = 0; i < num_iters; ++i) {
            sum_of_squares += mapped[i].get();
        }
        promise.setValue(sum_of_squares);
    });

    int sum_of_squares = result.get();
    mapper.join();
    reducer.join();
    ASSERT_EQ(sum_of_squares, expected);
    return true;
}

bool subscibes_stress() {
    constexpr int num_iters = 1'000'000;
    std::vector<Promise<int>> to_map;
    std::vector<Future<int>> mapped;
    std::mutex mtx;
    std::atomic<int> counter = 0;

    for (int i = 0; i < num_iters; ++i) {
        auto [promise, future] = contract<int>();
        to_map.push_back(std::move(promise));
        mapped.push_back(std::move(future));
    }

    auto worker_task = [&to_map, &mapped, &mtx, &counter]() {
        auto fetcher = [&counter] (int value) { counter.fetch_add(value); };
        while (true) {
            std::unique_lock guard(mtx);
            if (to_map.empty() && mapped.empty()) {
                break;
            } else if (to_map.size() >= mapped.size()) {
                auto promise = std::move(to_map.back());
                to_map.pop_back();
                guard.unlock();
                promise.setValue(1);
            } else {
                auto future = std::move(mapped.back());
                mapped.pop_back();
                guard.unlock();
                future.subscribe(fetcher);
            }
        }
    };
    std::thread t1(worker_task);
    std::thread t2(worker_task);
    t1.join();
    t2.join();
    ASSERT_EQ(counter.load(), num_iters);
    return true;
}


int main() {
    TEST(get_blocks, "Get blocks");
    TEST(subscription1, "Subscribe before set");
    TEST(subscription2, "Subscribe after set");
    TEST(futures_are_oneshot, "Futures are oneshot");
    TEST(wait_does_not_consume, "Wait does not invalidate the Future");
    TEST(moveonly_value, "Moveonly value");
    TEST(exception_in_get, "Exception in get");
    TEST(exception_in_subscribe, "Exception in subscribe");
    TEST(map_reduce, "Map reduce");
    TEST(subscibes_stress, "Concurrent subscribes");
    return EXIT_SUCCESS;
}
