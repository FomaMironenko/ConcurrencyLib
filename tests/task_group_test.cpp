#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdint>

#include <vector>
#include <map>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"

#include "thread_pool.hpp"
#include "async_function.hpp"
#include "task_group.hpp"

using namespace std::chrono_literals;


DEFINE_TEST(just_works) {
    ThreadPool pool(4);
    GroupAll<int> tg;
    // Order of values in result only depends on the order of join and
    // does not depend on when the value was physically produced.
    tg.join(call_async<int>(pool, []() {
        // Even though it sleeps, 1 must be the first one
        std::this_thread::sleep_for(50ms);
        return 1;
    }));
    tg.join(call_async<int>(pool, []() { return 2; }));
    tg.join(call_async<int>(pool, []() { return 3; }));
    tg.join(call_async<int>(pool, []() { return 4; }));
    auto results = tg.merge(pool).get();
    std::vector<int> expected = {1, 2, 3, 4};
    ASSERT_EQ(results, expected);
}


struct WorstType {
    WorstType() = delete;
    explicit WorstType(int val) : val(val) {    }

    WorstType(const WorstType&) = delete;
    WorstType(WorstType&&) = default;
    WorstType& operator=(const WorstType&) = delete;
    WorstType& operator=(WorstType&&) = default;

    int val;
};

DEFINE_TEST(worst_type) {
    ThreadPool pool(1);
    GroupAll<WorstType> tg;
    auto async_make_unique = make_async(pool, [](int val) { return WorstType(val); });
    tg.join(async_make_unique(21));
    tg.join(async_make_unique(42));
    auto results = tg.merge(pool).get();
    ASSERT_EQ(results.size(), 2u);
    ASSERT_EQ(results[0].val, 21);
    ASSERT_EQ(results[1].val, 42);
}


DEFINE_TEST(void_group_all) {
    ThreadPool pool(4);
    GroupAll<void> tg;
    auto increment = make_async(pool, [](std::atomic<int>& state) { state.fetch_add(1); });

    std::atomic<int> state { 0 };
    constexpr int NUM_ITERS = 100;
    for (int i = 0; i < NUM_ITERS; ++i) {
        tg.join( increment(std::ref(state)) );
    }
    AsyncResult<void> all = tg.merge(pool);
    all.wait();
    ASSERT_EQ(state.load(), NUM_ITERS);
}


DEFINE_TEST(continuation) {
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
}


DEFINE_TEST(finish_before_merge) {
    ThreadPool pool(2);
    GroupAll<bool> tg;
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    std::this_thread::sleep_for(50ms);
    auto res = tg.merge(pool);
    ASSERT_EQ(res.get().size(), 3u);
}


DEFINE_TEST(finish_after_merge) {
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
    ASSERT_EQ(vals.size(), 3u);
    for (bool val : vals) {
        ASSERT_EQ(val, true);
    }
}


DEFINE_TEST(prod_cons_pools) {
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
}


template <size_t num_workers>
DEFINE_TEST(perfect_parallelization) {
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
}


int main() {
    RUN_TEST(just_works, "GroupAll just works");
    RUN_TEST(worst_type, "GroupAll with moveonly & non-default-constructible type")
    RUN_TEST(void_group_all, "GroupAll<void> just works");
    RUN_TEST(continuation, "Continuation");
    RUN_TEST(finish_before_merge, "Finish before merge");
    RUN_TEST(finish_after_merge, "Finish after merge");
    RUN_TEST(prod_cons_pools, "Producer and consumer pools in single TaskGroup");
    RUN_TEST(perfect_parallelization<1>, "Parallelization 1");
    RUN_TEST(perfect_parallelization<2>, "Parallelization 2");
    RUN_TEST(perfect_parallelization<4>, "Parallelization 4");
    RUN_TEST(perfect_parallelization<8>, "Parallelization 8");
    COMPLETE();
}
