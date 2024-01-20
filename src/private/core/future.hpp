#pragma once

#include <exception>
#include <cassert>
#include <memory>

#include "../type_traits.hpp"
#include "shared_state.hpp"

#include "../subscription/base_sub.hpp"
#include "../subscription/simple_sub.hpp"


// Forward declare
template <class T> struct Contract;


template <class T>
class Future {

using StateType = details::SharedState<PhysicalType<T> >;

template <class U> friend Contract<U> contract();

private:
    Future(std::shared_ptr<StateType> state) : state_(std::move(state)) {    }

public:
    Future() = default;
    Future(const Future&) = delete;
    Future(Future&&) = default;
    Future& operator=(const Future&) = delete;
    Future& operator=(Future&&) = default;

    // Create a ready-to-use Future filled with a value.
    static Future instantValue(PhysicalType<T> value);
    // Create a ready-to-use Future filled with an exception.
    static Future instantError(std::exception_ptr error);

    // Wait for the Promise to be resolved.
    // Does not invalidate the Future.
    void wait();

    // Wait for the Promise to be resolved and return value.
    // Invalidates the Future.
    PhysicalType<T> get();

    // Subscribe to the result. An appropriate callback will be executed instantly
    // by this thread in case a result has been produced already, and by the producer
    // thread when calling setValue / setError on Promise object otherwise.
    // Invalidates the Future.
    void subscribe(ValueCallback<PhysicalType<T> > on_value, ErrorCallback on_error = nullptr);
    void subscribe(details::SubscriptionPtr<PhysicalType<T> > subscription);

    // Rejects the result.
    // Invalidates the Future.
    void reject();

private:
    std::shared_ptr<StateType> state_;
};


template <class T>
Future<T> Future<T>::instantValue(PhysicalType<T> value)
{
    auto state = std::make_shared<StateType>();
    state->value_ = std::move(value);
    state->produced_ = true;
    return Future<T>{state};
}

template <class T>
Future<T> Future<T>::instantError(std::exception_ptr error)
{
    auto state = std::make_shared<StateType>();
    state->error_ = std::move(error);
    state->produced_ = true;
    return Future<T>{state};
}

template <class T>
PhysicalType<T> Future<T>::get()
{
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to get a spoiled state");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::unique_lock guard(state->mtx_);
    while (!state->produced_) {
        state->cv_.wait(guard);
    }
    if (state->error_) {
        std::rethrow_exception(state->error_);
    }
    assert(state->value_.has_value());
    return std::move(*state->value_);
}

template <class T>
void Future<T>::wait()
{
    if (!state_) {
        throw std::runtime_error("Trying to wait for spoiled state");
    }
    std::unique_lock guard(state_->mtx_);
    while (!state_->produced_) {
        state_->cv_.wait(guard);
    }
}

template <class T>
void Future<T>::subscribe(ValueCallback<PhysicalType<T> > on_value, ErrorCallback on_error)
{
    subscribe(std::make_unique<details::SimpleSubscription<T> >(
        std::move(on_value),
        std::move(on_error)
    ));
}

template <class T>
void Future<T>::subscribe(details::SubscriptionPtr<PhysicalType<T> > subscription)
{
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
        state->resolveSubscription(details::ResolvedBy::kConsumer);
    } else {
        // Scenario 2: state has not yet been produced and the callback will be executed by producer
        state->subscribed_ = true;
    }
}

template <class T>
void Future<T>::reject() {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to reject to spoiled state");
    }
    std::unique_lock guard(state->mtx_);
    state->rejected_ = true;
}