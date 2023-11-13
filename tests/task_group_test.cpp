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
#include "task_group.hpp"

using namespace std::chrono_literals;


bool just_works() {
    ThreadPool pool(4);
    GroupAll<int> tg(pool);
    tg.submit([]() { std::this_thread::sleep_for(10ms); return 1; });
    tg.submit([]() { return 2; });
    tg.submit([]() { return 3; });
    tg.submit([]() { return 4; });
    auto results = tg.handle().get();
    std::vector<int> expected = {1, 2, 3, 4};
    ASSERT_EQ(results, expected);
    return true;
}

bool continuation() {
    ThreadPool pool(2);
    GroupAll<int> tg(pool);
    int expected = 0;
    for (int val = 0; val < 100; ++val) {
        expected += val * val;
        tg.submit([val]() { return val * val; });
    }
    auto mapped = tg.handle()
    .then<int>([] (std::vector<int> vals) mutable {
        int sum = 0;
        for (const int val : vals) {
            sum += val;
        }
        return sum;
    });
    ASSERT_EQ(mapped.get(), expected);

    return true;
}

bool finish_before_handle() {
    ThreadPool pool(2);
    GroupAll<bool> tg(pool);
    tg.submit([]() { return true; });
    tg.submit([]() { return true; });
    tg.submit([]() { return true; });
    std::this_thread::sleep_for(50ms);
    auto res = tg.handle();
    ASSERT_EQ(res.get().size(), 3);
    return true;
}

bool finish_after_handle() {
    ThreadPool pool(2);
    std::unique_ptr<AsyncResult<std::vector<bool>>> res;
    auto tg = std::make_unique<GroupAll<bool>>(pool);
    tg->submit([]() { std::this_thread::sleep_for(50ms); return true; });
    tg->submit([]() { std::this_thread::sleep_for(50ms); return true; });
    tg->submit([]() { std::this_thread::sleep_for(50ms); return true; });
    res = std::make_unique<AsyncResult<std::vector<bool>>>(tg->handle());
    tg.reset();
    std::vector<bool> vals = res->get();
    ASSERT_EQ(vals.size(), 3);
    for (bool val : vals) {
        ASSERT_EQ(val, true);
    }
    return true;
}

int main() {
    TEST(just_works, "GroupAll just works");
    TEST(continuation, "Continuation");
    TEST(finish_before_handle, "Finish before handle");
    TEST(finish_after_handle, "Finish after handle");
    
    return EXIT_SUCCESS;
}
