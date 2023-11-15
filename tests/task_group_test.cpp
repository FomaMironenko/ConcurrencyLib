#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>

#include <vector>
#include <map>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"

#include "thread_pool.hpp"
#include "async_function.hpp"
#include "task_group.hpp"

using namespace std::chrono_literals;


bool just_works() {
    ThreadPool pool(4);
    GroupAll<int> tg;
    tg.join(call_async<int>(pool, []() {
        std::this_thread::sleep_for(10ms);
        return 1;
    }));
    tg.join(call_async<int>(pool, []() { return 2; }));
    tg.join(call_async<int>(pool, []() { return 3; }));
    tg.join(call_async<int>(pool, []() { return 4; }));
    auto results = tg.merge(pool).get();
    std::vector<int> expected = {1, 2, 3, 4};
    ASSERT_EQ(results, expected);
    return true;
}

bool continuation() {
    ThreadPool pool(2);
    GroupAll<int> tg;
    auto pow2 = [](int val) { return val * val; };
    AsyncFunction<int(int)> async_pow2 = make_async(pool, pow2);

    int expected = 0;
    for (int val = 0; val < 100; ++val) {
        expected += pow2(val);
        tg.join( async_pow2(val) );
    }
    auto mapped = tg.merge(pool).then<int>([] (std::vector<int> vals) {
        int sum = 0;
        for (const int val : vals) sum += val;
        return sum;
    });
    ASSERT_EQ(mapped.get(), expected);

    return true;
}

bool finish_before_merge() {
    ThreadPool pool(2);
    GroupAll<bool> tg;
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    std::this_thread::sleep_for(50ms);
    auto res = tg.merge(pool);
    ASSERT_EQ(res.get().size(), 3);
    return true;
}

bool finish_after_merge() {
    ThreadPool pool(2);
    auto tg = std::make_unique<GroupAll<bool>>();
    AsyncFunction<bool()> async_fun = make_async(pool, []() {
        std::this_thread::sleep_for(50ms);
        return true;
    });
    tg->join(async_fun());
    tg->join(async_fun());
    tg->join(async_fun());
    AsyncResult<std::vector<bool> > res = tg->merge(pool);
    asm("");    // prohibit reordering
    tg.reset(); // destroy task group
    std::vector<bool> vals = res.get();
    ASSERT_EQ(vals.size(), 3);
    for (bool val : vals) {
        ASSERT_EQ(val, true);
    }
    return true;
}

bool prod_cons_pools() {
    ThreadPool prod_pool(2);
    ThreadPool cons_pool(2);

    std::atomic<int> state{0};
    constexpr int MIN = 0;
    constexpr int MAX = 6;
    AsyncFunction<int()> produce = make_async(prod_pool, [&state]() {
        int current = 0;
        for(;;) {
            current = state.load(std::memory_order_acquire);
            if (current < MAX && state.compare_exchange_strong(current, current + 1)) {
                break;
            }
        }
        return current + 1;
    });
    AsyncFunction<int()> consume = make_async(cons_pool, [&state]() {
        int current = 0;
        for(;;) {
            current = state.load(std::memory_order_acquire);
            if (current > MIN && state.compare_exchange_strong(current, current - 1)) {
                break;
            }
        }
        return current - 1;
    });

    GroupAll<int> tg;
    constexpr int NUM_ITERS = 100'000;
    // Two pools guarantee there's no deadlock
    for (int iter = 0; iter < NUM_ITERS; ++iter) tg.join(produce());
    // Consumers would have never started if they were submitted to the same pool as producers
    for (int iter = 0; iter < NUM_ITERS; ++iter) tg.join(consume());
    std::vector<int> history = tg.merge(prod_pool).get();

    ASSERT(state.load() == 0);
    std::map<int, int> freq;
    for (int value : history) ++freq[value];
    // No value from span is missing
    LOG_INFO << "State value historical frequency:";
    for (int value = MIN; value <= MAX; ++value) {
        LOG_INFO << "[" << value << "] : " << freq[value];
        ASSERT(freq[value] > 0);
    }
    // No value out of span is present
    ASSERT_EQ(static_cast<int>(freq.size()), MAX - MIN + 1);
    return true;
}


template <size_t num_workers>
bool perfect_parallelization() {
    ThreadPool pool(num_workers);
    constexpr int NUM_CYCLES = 50;
    constexpr int NUM_TASKS = NUM_CYCLES * num_workers;
    constexpr auto wait_time = 20ms;
    double jobMs = static_cast<double>(wait_time.count());
    AsyncFunction<Void()> async_job = make_async(pool, [wait_time]() {
        std::this_thread::sleep_for(wait_time);
        return Void{};
    });

    Timer timer;
    GroupAll<Void> tg;
    for (int i_task = 0; i_task < NUM_TASKS; ++i_task) tg.join(async_job());
    tg.merge(pool).get();
    double elapsedMs = timer.elapsedMilliseconds();

    double coef = elapsedMs / (NUM_CYCLES * jobMs);
    LOG_INFO << NUM_TASKS << " sleeping tasks " << jobMs << " ms each took " << elapsedMs << " ms on " << num_workers << " workers";
    ASSERT(coef < 1.3);
    return true;
}


int main() {
    TEST(just_works, "GroupAll just works");
    TEST(continuation, "Continuation");
    TEST(finish_before_merge, "Finish before merge");
    TEST(finish_after_merge, "Finish after merge");
    TEST(prod_cons_pools, "Producer and consumer pools in single TaskGroup");
    TEST(perfect_parallelization<1>, "Parallelization 1");
    TEST(perfect_parallelization<2>, "Parallelization 2");
    TEST(perfect_parallelization<4>, "Parallelization 4");
    TEST(perfect_parallelization<8>, "Parallelization 8");
    
    return EXIT_SUCCESS;
}
