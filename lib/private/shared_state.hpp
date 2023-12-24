#pragma once

#include <optional>
#include <exception>
#include <functional>

#include <mutex>
#include <condition_variable>

#include "subscription.hpp"
#include "utils/logger.hpp"


// Forward declare
template <class T> class Promise;
template <class T> class Future;
template <class T> struct Contract;


namespace details {

template <class T>
struct SharedState {

friend class Promise<T>;
friend class Future<T>;

private:
    std::mutex mtx_;
    std::condition_variable cv_;

    std::optional<T> value_ = std::nullopt;
    std::exception_ptr error_ = nullptr;
    SubscriptionPtr<T> subscription_ = 0;

    bool produced_ = false;
    bool subscribed_ = false;

    void resolveSubscription();
};

template <class T>
void SharedState<T>::resolveSubscription() {
    if (error_) {
        subscription_->resolveError(error_);
    } else {
        subscription_->resolveValue(std::move(*value_));
    }
    subscription_.reset();
}


}  // namespace details
