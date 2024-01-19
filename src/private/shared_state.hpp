#pragma once

#include <optional>
#include <exception>
#include <functional>

#include <mutex>
#include <condition_variable>

#include "utils/logger.hpp"
#include "subscription/base_sub.hpp"


// Forward declare
template <class T> class Promise;
template <class T> class Future;


namespace details {


// ================================================ //
// ==================== RESULT ==================== //
// ================================================ //

template <class T>
struct Result {
    std::optional<T> val = std::nullopt;
    std::exception_ptr err = nullptr;
};

template <>
struct Result<void> {
    std::exception_ptr err = nullptr;
};


// ====================================================== //
// ==================== SHARED STATE ==================== //
// ====================================================== //

template <class T>
struct SharedState {

template <class U> friend class ::Promise;
template <class U> friend class ::Future;

private:
    std::mutex mtx_;
    std::condition_variable cv_;

    std::optional<T> value_ = std::nullopt;
    std::exception_ptr error_ = nullptr;
    SubscriptionPtr<T> subscription_ = 0;

    bool produced_ = false;
    bool subscribed_ = false;

    void resolveSubscription(ResolvedBy by);
};

template <class T>
void SharedState<T>::resolveSubscription(ResolvedBy by) {
    if (error_) {
        subscription_->resolveError(error_, by);
    } else {
        subscription_->resolveValue(std::move(*value_), by);
    }
    subscription_.reset();
}


}  // namespace details
