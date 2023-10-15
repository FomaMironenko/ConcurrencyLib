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


int main() {
    TEST(just_works, "Just works");
    return EXIT_SUCCESS;
}
