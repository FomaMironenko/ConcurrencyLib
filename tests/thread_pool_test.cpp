#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

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


int main() {
    TEST(just_works, "Just works");
    TEST(subscription_just_works, "Subscription just works");
    TEST(subscription_error, "Error in subscription");
    return EXIT_SUCCESS;
}
