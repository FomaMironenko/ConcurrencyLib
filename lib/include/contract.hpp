#pragma once

#include <cassert>
#include <memory>
#include <utility>
#include <stdexcept>
#include <functional>

#include "../private/subscription.hpp"
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

template <class U>
friend Contract<U> contract();
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

    // Create a ready-to-use Future filled with a value.
    static Future instantValue(T value);
    // Create a ready-to-use Future filled with an exception.
    static Future instantError(std::exception_ptr error);

    // Wait for the Promise to be resolved, but does not invalidate the Future.
    void wait();

    // Wait for the Promise to be resolved and return value. Invalidates the Future.
    T get();

    // Subscribe to the result. An appropriate callback will be executed instantly
    // by this thread in case a result has been produced already, and by the producer
    // thread when calling setValue / setError on Promise object otherwise.
    // Invalidates the Future.
    void subscribe(ValueCallback<T> on_value, ErrorCallback on_error = nullptr);
    void subscribe(SubscriptionPtr<T> subscription);

private:
    std::shared_ptr<details::SharedState<T>> state_;

template <class U>
friend Contract<U> contract();
};


// ================================================== //
// ==================== CONTRACT ==================== //
// ================================================== //

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


// ============================================================== //
// ==================== PROMISE SUBSCRIPTION ==================== //
// ============================================================== //

template <class T>
class ProducerSubscription : public ISubscription<T> {
public:
    ProducerSubscription(Promise<T> promise)
        : promise_(std::move(promise)) {   }

    virtual void resolveValue(T value) override {
        promise_.setValue(std::move(value));
    }

    virtual void resolveError(std::exception_ptr err) override {
        promise_.setError(err);
    }

private:
    Promise<T> promise_;
};


// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //

template <class T>
void Promise<T>::setValue(T value) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set value in a produced state");
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
        throw std::runtime_error("Trying to set error in a produced state");
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
Future<T> Future<T>::instantValue(T value) {
    auto state = std::make_shared<details::SharedState<T>>();
    state->value_ = std::move(value);
    state->produced_ = true;
    return Future<T>{state};
}

template <class T>
Future<T> Future<T>::instantError(std::exception_ptr error) {
    auto state = std::make_shared<details::SharedState<T>>();
    state->error_ = std::move(error);
    state->produced_ = true;
    return Future<T>{state};
}

template <class T>
T Future<T>::get() {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to get a spoiled state");
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
void Future<T>::wait() {
    if (!state_) {
        throw std::runtime_error("Trying to wait for spoiled state");
    }
    std::unique_lock guard(state_->mtx_);
    while (!state_->produced_) {
        state_->cv_.wait(guard);
    }
}

template <class T>
void Future<T>::subscribe(ValueCallback<T> on_value, ErrorCallback on_error) {
    subscribe(std::make_unique<SimpleSubscription<T>>(std::move(on_value), std::move(on_error)));
}

template <class T>
void Future<T>::subscribe(SubscriptionPtr<T> subscription) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to subscribe to spoiled state");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::unique_lock guard(state->mtx_);
    state->subscription_ = std::move(subscription);
    if (state->produced_) {
        // Scenario 1: state has already been produced and the callback will be executed in current thread
        // this is the only owning thread, so guard causes no contention
        state->resolveSubscription();
    } else {
        // Scenario 2: state has not yet been produced and the callback will be executed by producer
        state->subscribed_ = true;
    }
}
