#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdint>

#include <vector>
#include <map>

#include "utils/logger.hpp"
#include "test_utils/timer.hpp"
#include "test_utils/tester.hpp"

#include "thread_pool.hpp"
#include "async_function.hpp"
#include "task_group.hpp"

using namespace std::chrono_literals;


DEFINE_TEST(all_just_works) {
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


DEFINE_TEST(first_just_works) {
    ThreadPool pool(4);
    TaskGroup<int> tg;
    auto busy_async = make_async(pool, [](int val) {
        std::this_thread::sleep_for(50ms);
        return val;
    });
    // Order of values in result only depends on the order of join and
    // does not depend on when the value was physically produced.
    tg.join(busy_async(1));
    tg.join(busy_async(2));
    tg.join(busy_async(3));
    std::this_thread::sleep_for(10ms);
    tg.join(call_async<int>(pool, []() { return 4; }));
    auto results = tg.first().get();
    ASSERT_EQ(results, 4);
}


DEFINE_TEST(first_doesnt_wait_all) {
    ThreadPool pool(2);
    TaskGroup<int> tg;
    std::atomic<bool> in { false }, out { false }; 
    
    tg.join(call_async<int>(pool, [&in, &out]() {
        while(!in.load(std::memory_order_acquire));
        out.store(true, std::memory_order_release);
        return 2;
    }));
    tg.join(AsyncResult<int>::instant(42));

    ASSERT_EQ(tg.first().get(), 42);
    std::this_thread::sleep_for(10ms);
    ASSERT(!out.load(std::memory_order_acquire));
    in.store(true, std::memory_order_release);
    std::this_thread::sleep_for(10ms);
    ASSERT(out.load(std::memory_order_acquire));
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
    auto mapped_all = tg.all().then<int>([] (std::vector<int> vals) {
        int sum = 0;
        for (const int val : vals) sum += val;
        return sum;
    }, ThenPolicy::NoSchedule);
    ASSERT_EQ(mapped_all.get(), expected);

    for (int val = 0; val < 100; ++val) {
        tg.join( async_pow2(val) );
    }
    auto mapped_first = tg.first().then<bool>([] (int val) {
        return val == 0 || val == 1 || val == 4 || val == 9;
    });
    ASSERT(mapped_first.get());
}


DEFINE_TEST(error_in_group_all) {
    ThreadPool pool(2);
    constexpr int NUM_ITERS = 100;
    auto hazardous_eq_async = make_async(pool, [](int x, int y) {
        if (x == y) {
            throw y;
        }
        return y;
    });

    TaskGroup<int> tg;
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        for (int elt = 0; elt < NUM_ITERS; ++elt) {
            tg.join(hazardous_eq_async(iter, elt));
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


DEFINE_TEST(error_in_group_first) {
    ThreadPool pool(2);
    constexpr int NUM_ITERS = 100;
    auto hazardous_neq_async = make_async(pool, [](int x, int y) {
        if (x != y) {
            throw y;
        }
        return y;
    });

    TaskGroup<int> tg;
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        for (int elt = 0; elt < NUM_ITERS; ++elt) {
            tg.join(hazardous_neq_async(iter, elt));
        }
        auto res = tg.first();
        res.wait();
        try {
            int good_val = res.get();
            ASSERT(good_val == iter);
        } catch (...) {
            FAIL();
        }
    }

    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        tg.join(hazardous_neq_async(-1, iter));
    }
    auto res = tg.first();
    try {
        res.get();
        FAIL();
    } catch (...) {
        // pass
    }
}


DEFINE_TEST(finish_before_merge) {
    ThreadPool pool(2);
    TaskGroup<bool> tg;

    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    std::this_thread::sleep_for(50ms);
    ASSERT_EQ(tg.all().get().size(), 3u);

    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    tg.join(call_async<bool>(pool, []() { return true; }));
    std::this_thread::sleep_for(50ms);
    ASSERT_EQ(tg.first().get(), true);
}


DEFINE_TEST(finish_after_merge) {
    ThreadPool pool(2);
    AsyncFunction<bool()> async_fun = make_async(pool, []() {
        std::this_thread::sleep_for(50ms);
        return true;
    });

    auto tg1 = std::make_unique<TaskGroup<bool>>();
    tg1->join(async_fun());
    tg1->join(async_fun());
    tg1->join(async_fun());
    AsyncResult<std::vector<bool> > res_all = tg1->all();
    #ifndef WIN32
    asm("");  // prohibit reordering
    #endif
    tg1.reset();  // destroy task group
    std::vector<bool> vals = res_all.get();
    ASSERT_EQ(vals.size(), 3u);
    for (bool val : vals) {
        ASSERT_EQ(val, true);
    }

    auto tg2 = std::make_unique<TaskGroup<bool>>();
    tg2->join(async_fun());
    tg2->join(async_fun());
    tg2->join(async_fun());
    AsyncResult<bool> res_first = tg2->first();
    #ifndef WIN32
    asm("");  // prohibit reordering
    #endif
    tg2.reset();  // destroy task group
    ASSERT_EQ(res_first.get(), true);
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


DEFINE_TEST(all_first) {
    ThreadPool pool_one(2);
    ThreadPool pool_two(2);
    TaskGroup<int> tg_first;
    TaskGroup<int> tg_all;
    auto task = [](int val) {
        std::this_thread::sleep_for(100us);
        return val;
    };

    constexpr int NUM_ITERS = 1000;
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        tg_first.join(call_async<int>(pool_one, task, 1));
        tg_first.join(call_async<int>(pool_two, task, 2));
        tg_all.join(tg_first.first());
    }
    auto result = tg_all.all().get();
    bool has_ones = false, has_twos = false;
    for (int val : result) {
        has_ones = has_ones || (val == 1);
        has_twos = has_twos || (val == 2);
    }
    ASSERT(has_ones);
    ASSERT(has_twos);
}


DEFINE_TEST(first_all) {
    ThreadPool pool_one(2);
    ThreadPool pool_two(2);
    TaskGroup<std::vector<int> > tg_first;
    TaskGroup<int> tg_all_1, tg_all_2;
    auto identity = [](int val) { return val; };

    constexpr int NUM_ITERS = 1000;
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        tg_all_1.join(call_async<int>(pool_one, identity, iter));
        tg_all_2.join(call_async<int>(pool_two, identity, iter));
    }
    tg_first.join(tg_all_1.all());
    tg_first.join(tg_all_2.all());
    
    auto result = tg_first.first().get();
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        ASSERT_EQ(result[iter], iter);
    }
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
    RUN_TEST(all_just_works, "TaskGroup::all just works");
    RUN_TEST(first_just_works, "TaskGroup::first just works");
    RUN_TEST(first_doesnt_wait_all, "TaskGroup::first doesnt wait for all tasks to finish");
    RUN_TEST(worst_type, "TaskGroup with moveonly & non-default-constructible type")
    RUN_TEST(void_group_all, "TaskGroup<void> just works");
    RUN_TEST(continuation, "Continuation");
    RUN_TEST(error_in_group_all, "Error in TaskGroup::all");
    RUN_TEST(error_in_group_first, "Error in TaskGroup::first");
    RUN_TEST(finish_before_merge, "Finish before merge");
    RUN_TEST(finish_after_merge, "Finish after merge");
    RUN_TEST(prod_cons_pools, "Producer and consumer pools in single TaskGroup");
    RUN_TEST(all_first, "Wait for all tasks, where each is TaskGroup::first");
    RUN_TEST(first_all, "Wait for first task, where each is TaskGroup::all");
    RUN_TEST((perfect_parallelization<2, 10>), "Parallelization 2; 10ms");
    RUN_TEST((perfect_parallelization<8, 10>), "Parallelization 8; 10ms");
    RUN_TEST((perfect_parallelization<2, 50>), "Parallelization 2; 50ms");
    RUN_TEST((perfect_parallelization<8, 50>), "Parallelization 8; 50ms");
    COMPLETE();
}
