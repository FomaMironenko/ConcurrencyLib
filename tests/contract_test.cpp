#include <iostream>
#include <chrono>
#include <thread>

#include "utils/logger.hpp"
#include "contract.hpp"

using namespace std::chrono_literals;

int main() {
    auto [producer, consumer] = contract<int>();

    std::thread prod_thread([producer = std::move(producer)] () mutable {
        LOG_INFO << "Entering producer work";
        std::this_thread::sleep_for(2s);
        producer.setValue(42);
        LOG_INFO << "Producer work done";
    });

    std::thread cons_thread([consumer = std::move(consumer)] () mutable {
        LOG_INFO << "Entering consumer work";
        int val = consumer.get();
        LOG_INFO << "Consumer work done: consumed " << val;
    });

    prod_thread.join();
    cons_thread.join();

    return EXIT_SUCCESS;
}
