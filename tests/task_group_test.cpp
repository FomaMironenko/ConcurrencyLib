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
    TaskGroup<int> tg;
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
    auto results = tg.all().get();
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
    TaskGroup<WorstType> tg;
    auto async_make_unique = make_async(pool, [](int val) { return WorstType(val); });
    tg.join(async_make_unique(21));
    tg.join(async_make_unique(42));
    auto results = tg.all().get();
    ASSERT_EQ(results.size(), 2u);
    ASSERT_EQ(results[0].val, 21);
    ASSERT_EQ(results[1].val, 42);
}


DEFINE_TEST(void_group_all) {
    ThreadPool pool(4);
    TaskGroup<void> tg;
    auto increment = make_async(pool, [](std::atomic<int>& state) { state.fetch_add(1); });

    std::atomic<int> state { 0 };
    constexpr int NUM_ITERS = 100;
    for (int i = 0; i < NUM_ITERS; ++i) {
        tg.join( increment(std::ref(state)) );
    }
    AsyncResult<void> all = tg.all();
    all.wait();
    ASSERT_EQ(state.load(), NUM_ITERS);
}


DEFINE_TEST(continuation) {
    ThreadPool pool(2);
    TaskGroup<int> tg;
    auto pow2 = [](int val) { return val * val; };
    AsyncFunction<int(int)> async_pow2 = make_async(pool, pow2);

    int expected = 0;
    for (int val = 0; val < 100; ++val) {
        expected += pow2(val);
        tg.join( async_pow2(val) );
    }
    auto mapped = tg.all().then<int>([] (std::vector<int> vals) {
        int sum = 0;
        for (const int val : vals) sum += val;
        return sum;
    }, ThenPolicy::NoSchedule);
    ASSERT_EQ(mapped.get(), expected);
}


DEFINE_TEST(error_in_group_all) {
    ThreadPool pool(2);
    constexpr int NUM_ITERS = 100;
    auto hazardous_async = make_async(pool, [](int x, int y) {
        if (x == y) {
            throw x;
        }
        return y;
    });
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        TaskGroup<int> tg;
        for (int elt = 0; elt < NUM_ITERS; ++elt) {
            if (elt == iter) {
                tg.join(hazardous_async(iter, elt));
            }
        }
        auto res = tg.all();
        res.wait();
        try {
            res.get();
            FAIL();
        } catch (int err) {
            ASSERT_EQ(err, iter);
        }
    }
}


DEFINE_TEST(finish_before_merge) {
    ThreadPool pool(2);
    TaskGroup<bool> tg;
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    std::this_thread::sleep_for(50ms);
    auto res = tg.all();
    ASSERT_EQ(res.get().size(), 3u);
}


DEFINE_TEST(finish_after_merge) {
    ThreadPool pool(2);
    auto tg = std::make_unique<TaskGroup<bool>>();
    AsyncFunction<bool()> async_fun = make_async(pool, []() {
        std::this_thread::sleep_for(50ms);
        return true;
    });
    tg->join(async_fun());
    tg->join(async_fun());
    tg->join(async_fun());
    AsyncResult<std::vector<bool> > res = tg->all();
    #ifndef WIN32
    asm("");  // prohibit reordering
    #endif
    tg.reset();  // destroy task group
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
    static constexpr int MIN = 0; // static here is needed for Visual Studio compiler
    static constexpr int MAX = 6; // static here is needed for Visual Studio compiler
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

    TaskGroup<int> tg;
    constexpr int NUM_ITERS = 100'000;
    // Two pools guarantee there's no deadlock
    for (int iter = 0; iter < NUM_ITERS; ++iter) tg.join(produce());
    // Consumers would have never started if they were submitted to the same pool as producers
    for (int iter = 0; iter < NUM_ITERS; ++iter) tg.join(consume());
    std::vector<int> history = tg.all().get();

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


template <size_t num_workers, size_t jobMs>
DEFINE_TEST(perfect_parallelization) {
    ThreadPool pool(num_workers);
    constexpr int NUM_CYCLES = 50;
    constexpr int NUM_TASKS = NUM_CYCLES * num_workers;
    constexpr auto wait_time = std::chrono::milliseconds(jobMs);
    AsyncFunction<Void()> async_job = make_async(pool, [wait_time]() {
        std::this_thread::sleep_for(wait_time);
        return Void{};
    });

    Timer timer;
    TaskGroup<Void> tg;
    for (int i_task = 0; i_task < NUM_TASKS; ++i_task) tg.join(async_job());
    tg.all().get();
    double elapsedMs = timer.elapsedMilliseconds();

    double coef = elapsedMs / (NUM_CYCLES * jobMs);
    LOG_INFO << NUM_TASKS << " sleeping tasks "
             << jobMs << " ms each on " << num_workers << " workers; "
             << "overhead: " << std::fixed << std::setprecision(2) << 100 * (coef - 1) << "%";
    ASSERT(coef < 1.3);
}


int main() {
    RUN_TEST(just_works, "TaskGroup just works");
    RUN_TEST(worst_type, "TaskGroup with moveonly & non-default-constructible type")
    RUN_TEST(void_group_all, "TaskGroup<void> just works");
    RUN_TEST(continuation, "Continuation");
    RUN_TEST(error_in_group_all, "Error in TaskGroup");
    RUN_TEST(finish_before_merge, "Finish before merge");
    RUN_TEST(finish_after_merge, "Finish after merge");
    RUN_TEST(prod_cons_pools, "Producer and consumer pools in single TaskGroup");
    RUN_TEST((perfect_parallelization<2, 10>), "Parallelization 2; 10ms");
    RUN_TEST((perfect_parallelization<8, 10>), "Parallelization 8; 10ms");
    RUN_TEST((perfect_parallelization<2, 50>), "Parallelization 2; 50ms");
    RUN_TEST((perfect_parallelization<8, 50>), "Parallelization 8; 50ms");
    COMPLETE();
}
