#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <deque>
#include <optional>
#include <exception>

#include "../private/shared_state.hpp"
#include "../private/type_traits.hpp"
#include "async_result.hpp"


// =============================================== //
// ==================== UTILS ==================== //
// =============================================== //


template <class T>
using GroupAllType = typename std::conditional_t<
    std::is_same_v<T, void>,
    void, std::vector<T>
>;


namespace details {


template <class T>
class GroupState {
public:
    explicit GroupState(Promise<GroupAllType<T> > promise)
        : num_references(1)
        , results()
        , promise(std::move(promise))
    {   }

    Result<T>* attach();
    void detach();

private:
    void produce();

private:
    std::atomic<int64_t> num_references;
    std::deque<Result<T>> results;
    Promise<GroupAllType<T> > promise;
};

template <class T>
void GroupState<T>::produce() {
    std::vector<PhysicalType<T>> values;
    values.reserve(results.size());
    for (auto& result : results) {
        if (result.err) {
            promise.setError(result.err);
            return;
        } else if (result.val) {
            values.push_back(std::move(*result.val));
        } else {
            LOG_ERR << "No result was produced prior to onAllReady";
            assert(false);
        }
    }
    promise.setValue(std::move(values));
}

template <>
void GroupState<void>::produce() {
    for (auto& result : results) {
        if (result.err) {
            promise.setError(result.err);
            break;
        }
    }
    promise.setValue(Void{});
}

template <class T>
Result<T>* GroupState<T>::attach() {
    results.emplace_back();
    num_references.fetch_add(1, std::memory_order_acq_rel);
    return &(results.back());
}

template <class T>
void GroupState<T>::detach() {
    auto prev_counter = num_references.fetch_add(-1, std::memory_order_acq_rel);
    if (prev_counter == 1) {
        produce();
    }
}

}  // namespace details


// ================================================== //
// ==================== GroupAll ==================== //
// ================================================== //

template <class T>
class GroupAll {

template <class U> friend class JoinSubscription;

public:
    GroupAll() {
        auto [promise, future] = contract<GroupAllType<T> >();
        state_ = std::make_shared<details::GroupState<T> >(std::move(promise));
        future_ = std::move(future);
    }

    void join(AsyncResult<T> result);

    AsyncResult<GroupAllType<T>> merge();

private:
    std::shared_ptr<details::GroupState<T>> state_;
    Future<GroupAllType<T> > future_;
};


// ============================================== //
// ==================== JOIN ==================== //
// ============================================== //

template <class T>
class JoinSubscription : public ISubscription<PhysicalType<T>> {
public:
    JoinSubscription(std::shared_ptr<details::GroupState<T>> state)
        : state_(std::move(state))
    {
        local_result_ = state_->attach();
    }

    void resolveValue([[maybe_unused]] PhysicalType<T> value, ResolvedBy) override {
        if constexpr (!std::is_same_v<T, void>) {
            local_result_->val = std::move(value);
        }
        state_->detach();
        state_.reset();
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        local_result_->err = std::move(err);
        state_->detach();
        state_.reset();
    }

private:
    std::shared_ptr<details::GroupState<T>> state_;
    details::Result<T>* local_result_;
};

template <class T>
void GroupAll<T>::join(AsyncResult<T> res) {
    res.fut_.subscribe(std::make_unique<JoinSubscription<T>>(state_));
}


// =============================================== //
// ==================== MERGE ==================== //
// =============================================== //

template <class T>
AsyncResult<GroupAllType<T>> GroupAll<T>::merge() {
    if (!state_) {
        throw std::runtime_error("Trying to merge GroupAll twice");
    }
    state_->detach();
    state_.reset();
    return {nullptr, std::move(future_)};
}
