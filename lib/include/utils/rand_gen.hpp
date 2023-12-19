#pragma once

#include <random>
#include <cassert>
#include <cmath>
#include <cstdint>

template <class T>
class LightweightPRG {
public:
    LightweightPRG(T begin, T end, int64_t seed) : begin(begin), range(end - begin) {
        assert(range > 0);
        assert(seed != 0);
        a = seed % 51;
        b = (seed * 50001) % 100001;
        current = seed;
    }

    T next() {
        current = (a * current + b) % kPrime;
        return (current % range) + begin;
    }

private:
    T begin;
    T range;
    int64_t a;
    int64_t b;
    int64_t current;
    static constexpr int64_t kPrime = 2'147'483'659;
};