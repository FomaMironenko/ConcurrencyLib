#pragma once

#include <chrono>

class Timer {
public:
    Timer() {
        start();
    }

    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    double elapsedMilliseconds() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count() / 1'000'000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_; 
};
