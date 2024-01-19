#pragma once

#include <memory>

#include "../../private/async/group_state.hpp"
#include "../../private/subscription/join_sub.hpp"

#include "async/result.hpp"


template <class T>
class TaskGroup {

template <class U> friend class JoinSubscription;

public:
    TaskGroup() {
        auto [promise, future] = contract<GroupAllType<T> >();
        state_ = std::make_shared<details::GroupState<T> >();
    }

    void join(AsyncResult<T> result);

    AsyncResult<GroupAllType<T> > all();
    AsyncResult<GroupFirstType<T> > first();

private:
    std::shared_ptr<details::GroupState<T> > state_;
};


// ============================================== //
// ==================== JOIN ==================== //
// ============================================== //

template <class T>
void TaskGroup<T>::join(AsyncResult<T> res) {
    res.fut_.subscribe(std::make_unique<JoinSubscription<T> >(state_));
}


// =============================================== //
// ==================== MERGE ==================== //
// =============================================== //

template <class T>
AsyncResult<GroupAllType<T> > TaskGroup<T>::all() {
    if (!state_) {
        throw std::runtime_error("Trying to merge all TaskGroup twice");
    }
    auto future = state_->subscribeToAll();
    state_->detach();
    state_ = std::make_shared<details::GroupState<T> >();
    return {nullptr, std::move(future)};
}

template <class T>
AsyncResult<GroupFirstType<T> > TaskGroup<T>::first() {
    if (!state_) {
        throw std::runtime_error("Trying to merge first TaskGroup twice");
    }
    auto future = state_->subscribeToFirst();
    state_->detach();
    state_ = std::make_shared<details::GroupState<T> >();
    return {nullptr, std::move(future)};
}
