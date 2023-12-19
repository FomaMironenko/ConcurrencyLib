#pragma once

#include <cmath>
#include <memory>
#include <vector>
#include <iostream>
#include <cstdint>


namespace Linalg {

template <class LhsVec, class RhsVec>
double dot(const LhsVec& lhs, const RhsVec& rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::runtime_error("Mismatching input sizes");
    }
    int64_t size = static_cast<int64_t>(lhs.size());
    double dot_prod = 0;
    for (int64_t idx = 0; idx < size; ++idx) {
        dot_prod += static_cast<double>(lhs[idx]) * static_cast<double>(rhs[idx]);
    }
    return dot_prod;
}

template <class Vec>
double eud(const Vec& vec) {
    double sq_norm = dot(vec, vec);
    return std::sqrt(sq_norm);
}

}  // namespace Linalg
