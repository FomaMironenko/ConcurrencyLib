#include <iostream>
#include <chrono>
#include <thread>

#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/tester.hpp"

#include "contract.hpp"

using namespace std::chrono_literals;

bool waitBlocks() {
    auto [producer, consumer] = contract<int>();

    Timer timer;
    std::thread prod_thread([producer = std::move(producer)] () mutable {
        std::this_thread::sleep_for(100ms);
        producer.setValue(42);
    });
    int val = consumer.get();
    double ms = timer.elapsedMilliseconds();
    prod_thread.join();

    ASSERT(val == 42);
    ASSERT(ms >= 100.0);
    ASSERT(ms <= 110.0);
    return true;
}

bool subscription1() {
    int dst = 0;
    auto [producer, consumer] = contract<int>();
    consumer.subscribe([&dst] (int result) { dst = result; });
    producer.setValue(42);
    ASSERT_EQ(dst, 42);
    return true;
}

bool subscription2() {
    int dst = 0;
    auto [producer, consumer] = contract<int>();
    producer.setValue(42);
    consumer.subscribe([&dst] (int result) { dst = result; });
    ASSERT_EQ(dst, 42);
    return true;
}


int main() {
    TEST(waitBlocks, "Get blocks");
    TEST(subscription1, "Subscribe before set");
    TEST(subscription2, "Subscribe after set");
    return EXIT_SUCCESS;
}
