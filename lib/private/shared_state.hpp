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


template <class T>
struct PromiseBox {

    PromiseBox(Promise<T> promise) : promise_(std::move(promise)) {  }

    Promise<T> get() {
        if (!promise_.has_value()) {
            LOG_ERR << "Attempting to get from PromiseBox twice";
            throw std::runtime_error("Double access to PromiseBox");
        }
        Promise<T> promise = std::move(*promise_);
        promise_ = std::nullopt;
        return promise;
    }
    
private:
    std::optional<Promise<T>> promise_;
};

}  // namespace details
