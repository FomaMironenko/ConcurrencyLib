#pragma once

#include <cassert>
#include <memory>
#include <utility>
#include <stdexcept>
#include <functional>

#include "../private/shared_state.hpp"


template <class T>
class Promise {
private:
    Promise(std::shared_ptr<details::SharedState<T>> state) : state_(std::move(state)) {    }

public:
    Promise(const Promise&) = delete;
    Promise(Promise&&) = default;
    Promise& operator=(const Promise&) = delete;
    Promise& operator=(Promise&&) = delete;

    void setValue(T value);
    void setError(std::exception_ptr err);

private:
    std::shared_ptr<details::SharedState<T>> state_;

template <class Y>
friend Contract<Y> contract();
};


template <class T>
class Future {
private:
    Future(std::shared_ptr<details::SharedState<T>> state) : state_(std::move(state)) {    }

public:
    Future(const Future&) = delete;
    Future(Future&&) = default;
    Future& operator=(const Future&) = delete;
    Future& operator=(Future&&) = default;

    T get();
    void subscribe(
        std::function<void(T)> val_callback,
        std::function<void(std::exception_ptr)> err_callback = nullptr
    );

private:
    std::shared_ptr<details::SharedState<T>> state_;

template <class Y>
friend Contract<Y> contract();
};


template <class T>
struct Contract {
    Promise<T> producer;
    Future<T> consumer;
};

template <class T>
Contract<T> contract() {
    auto state = std::make_shared<details::SharedState<T>>();
    return {Promise<T>{state}, Future<T>{state}};
}



// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //

template <class T>
void Promise<T>::setValue(T value) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set value twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::lock_guard guard(state->mtx_);
    assert(!state->produced_);
    state->value_ = std::move(value);
    if (state->subscribed_) {
        state->resolveSubscription();
    } else {
        state->produced_ = true;
        state->cv_.notify_one();  // there are no more than one waiters
    }
}

template <class T>
void Promise<T>::setError(std::exception_ptr err) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set value twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::lock_guard guard(state->mtx_);
    assert(!state->produced_);
    state->error_ = std::move(err);
    if (state->subscribed_) {
        state->resolveSubscription();
    } else {
        state->produced_ = true;
        state->cv_.notify_one();  // there are no more than one waiters
    }
}

template <class T>
T Future<T>::get() {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to get state twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::unique_lock guard(state->mtx_);
    while (!state->produced_) {
        state->cv_.wait(guard);
    }
    if (state->value_.has_value()) {
        return std::move(*state->value_);
    } else if (state->error_) {
        std::rethrow_exception(state->error_);
    }
    assert(false);
}

template <class T>
void Future<T>::subscribe(
        std::function<void(T)> val_callback,
        std::function<void(std::exception_ptr)> err_callback
) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to subscribe to state twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::unique_lock guard(state->mtx_);
    state->val_callback_ = std::move(val_callback);
    state->err_callback_ = std::move(err_callback);
    if (state->produced_) {
        // Scenario 1: state has already been produced and the callback will be executed in current thread
        // this is the only owning thread, so guard causes no contention
        state->resolveSubscription();
    } else {
        // Scenario 2: state has not yet been produced and the callback will be executed by producer
        state->subscribed_ = true;
    }
}
