#pragma once

#include <future>
#include <exception>

#include "base_sub.hpp"
#include "../type_traits.hpp"


template <class T>
class ToStdSubscription : public ISubscription<PhysicalType<T> > {
public:
    explicit ToStdSubscription(std::promise<T> std_promise)
        : std_promise_(std::move(std_promise)) {   }

    void resolveValue([[maybe_unused]] PhysicalType<T> val, ResolvedBy) override {
        if constexpr (std::is_same_v<T, void>) {
            std_promise_.set_value();
        } else {
            std_promise_.set_value(std::move(val));
        }
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        std_promise_.set_exception(err);
    }

private:
    std::promise<T> std_promise_;
};
