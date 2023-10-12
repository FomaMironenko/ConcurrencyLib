#pragma once

#include <cassert>
#include <memory>
#include <utility>
#include <stdexcept>

#include "../private/shared_state.hpp"


template <class T>
class StateProducer {
private:
    StateProducer(std::shared_ptr<details::SharedState<T>> state) : state_(std::move(state)) {    }

public:
    StateProducer(const StateProducer&) = delete;
    StateProducer(StateProducer&&) = default;
    StateProducer& operator=(const StateProducer&) = delete;
    StateProducer& operator=(StateProducer&&) = delete;

    void setValue(T value);
    void setError(std::exception_ptr err);

private:
    std::shared_ptr<details::SharedState<T>> state_;

template <class Y>
friend Contract<Y> contract();
};


template <class T>
class StateConsumer {
private:
    StateConsumer(std::shared_ptr<details::SharedState<T>> state) : state_(std::move(state)) {    }

public:
    StateConsumer(const StateConsumer&) = delete;
    StateConsumer(StateConsumer&&) = default;
    StateConsumer& operator=(const StateConsumer&) = delete;
    StateConsumer& operator=(StateConsumer&&) = delete;

    T get();

private:
    std::shared_ptr<details::SharedState<T>> state_;

template <class Y>
friend Contract<Y> contract();
};


template <class T>
struct Contract {
    StateProducer<T> producer;
    StateConsumer<T> consumer;
};

template <class T>
Contract<T> contract() {
    auto state = std::make_shared<details::SharedState<T>>();
    return {StateProducer<T>{state}, StateConsumer<T>{state}};
}



// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //

template <class T>
void StateProducer<T>::setValue(T value) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set value twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::lock_guard guard(state->mtx_);
    assert(!state->produced_);
    state->value_ = std::move(value);
    state->produced_ = true;
    state->cv_.notify_all();
}

template <class T>
void StateProducer<T>::setError(std::exception_ptr err) {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set value twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::lock_guard guard(state->mtx_);
    assert(!state->produced_);
    state->error_ = std::move(err);
    state->produced_ = true;
    state->cv_.notify_all();
}

template <class T>
T StateConsumer<T>::get() {
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to get state twice");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::unique_lock guard(state->mtx_);
    while (!state->produced_) {
        state->cv_.wait(guard);
    }
    state->consumed_ = true;
    if (state->value_.has_value()) {
        return std::move(*state->value_);
    } else if (state->error_) {
        std::rethrow_exception(state->error_);
    }
    assert(false);
}
