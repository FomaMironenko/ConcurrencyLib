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

}  // namespace details
