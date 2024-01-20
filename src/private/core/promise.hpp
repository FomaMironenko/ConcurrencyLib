#pragma once

#include <exception>
#include <cassert>
#include <memory>

#include "../type_traits.hpp"
#include "shared_state.hpp"


// Forward declare
template <class T> struct Contract;


template <class T>
class Promise {

using StateType = details::SharedState<PhysicalType<T> >;
template <class U> friend Contract<U> contract();

private:
    Promise(std::shared_ptr<StateType> state) : state_(std::move(state)) {    }

public:
    Promise(const Promise&) = delete;
    Promise(Promise&&) = default;
    Promise& operator=(const Promise&) = delete;
    Promise& operator=(Promise&&) = delete;

    void setValue(PhysicalType<T> value);
    void setError(std::exception_ptr err);

    bool isRejected();

private:
    std::shared_ptr<StateType> state_;
};


template <class T>
void Promise<T>::setValue(PhysicalType<T> value)
{
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set value in a produced state");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::lock_guard guard(state->mtx_);
    assert(!state->produced_);
    state->value_ = std::move(value);
    if (state->subscribed_) {
        state->resolveSubscription(details::ResolvedBy::kProducer);
    } else {
        state->produced_ = true;
        state->cv_.notify_one();  // there are no more than one waiters
    }
}

template <class T>
void Promise<T>::setError(std::exception_ptr err)
{
    auto state = std::move(state_);
    if (!state) {
        throw std::runtime_error("Trying to set error in a produced state");
    }
    // Guard will be destructed before state. Thus no use after free is available.
    std::lock_guard guard(state->mtx_);
    assert(!state->produced_);
    state->error_ = std::move(err);
    if (state->subscribed_) {
        state->resolveSubscription(details::ResolvedBy::kProducer);
    } else {
        state->produced_ = true;
        state->cv_.notify_one();  // there are no more than one waiters
    }
}

template <class T>
bool Promise<T>::isRejected() {
    if (!state_) {
        throw std::runtime_error("Trying to check for rejected after producing");
    }
    std::lock_guard guard(state_->mtx_);
    return state_->rejected_;
}
