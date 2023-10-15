#pragma once

#include <optional>
#include <exception>
#include <functional>

#include <mutex>
#include <condition_variable>

#include "utils/logger.hpp"


// Forward declare
template <class T>
class Promise;
template <class T>
class Future;
template <class T>
struct Contract;


namespace details {

template <class T>
struct SharedState {
private:
    std::mutex mtx_;
    std::condition_variable cv_;

    std::optional<T> value_ = std::nullopt;
    std::exception_ptr error_ = nullptr;
    std::function<void(T)> val_callback_;
    std::function<void(std::exception_ptr)> err_callback_;

    bool produced_ = false;
    bool subscribed_ = false;

    void resolveSubscription() {
        if (error_) {
            if (err_callback_) {
                err_callback_(error_);
            } else {
                LOG_ERR << "Unhandled exception prior to subscription";
            }
        } else {
            val_callback_(std::move(*value_));
        }
    }

friend class Promise<T>;
friend class Future<T>;
};

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
