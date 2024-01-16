#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <deque>
#include <optional>
#include <exception>

#include "../../private/shared_state.hpp"
#include "../../private/type_traits.hpp"

#include "async/result.hpp"


// =============================================== //
// ==================== UTILS ==================== //
// =============================================== //


template <class T>
using GroupAllType = typename std::conditional_t<
    std::is_same_v<T, void>,
    void, std::vector<T>
>;

template <class T>
using GroupFirstType = T;


namespace details {


template <class T>
class GroupState {
public:
    enum Type { kPending = 0, kReadyAll = 1, kReadyFirst = 2, kProduced = 3 };

    explicit GroupState()
        : num_pending_(0)
        , first_value_(nullptr)
        , first_error_(nullptr)
        , last_error_(nullptr)
        , results_()
        , group_type_(kPending)
        , promise_all_(std::nullopt)
        , promise_first_(std::nullopt)
    {   }

    Result<T>* attach();
    void detach();

    void registerValue(Result<T>*);
    void registerError(Result<T>*);

    Future<GroupAllType<T> > subscribeToAll();
    Future<GroupFirstType<T> > subscribeToFirst();

private:
    void produceAll();
    void produceFirst();

private:
    // Counters and last / first result
    std::atomic<int64_t> num_pending_;
    std::atomic<Result<T>* > first_value_;
    std::atomic<Result<T>* > first_error_;
    std::atomic<Result<T>* > last_error_;
    // An array of all results
    std::deque<Result<T> > results_;
    // Group state and promises
    std::atomic<int> group_type_;
    std::optional<Promise<GroupAllType<T> > > promise_all_;
    std::optional<Promise<GroupFirstType<T> > > promise_first_;
};


// ==================== PRODUCERS ==================== //

template <class T>
void GroupState<T>::produceAll() {
    assert(promise_all_.has_value());
    // Check for errors
    Result<T>* fst_err_result = first_error_.load(std::memory_order_relaxed);
    if (fst_err_result) {
        promise_all_->setError(std::move(fst_err_result->err));
        return;
    }
    // Fill in values
    std::vector<PhysicalType<T>> values;
    values.reserve(results_.size());
    for (auto& result : results_) {
        if (result.val) {
            values.push_back(std::move(*result.val));
        } else {
            LOG_ERR << "No result was produced in produceAll";
            assert(false);
        }
    }
    promise_all_->setValue(std::move(values));
}

template <>
void GroupState<void>::produceAll() {
    assert(promise_all_.has_value());
    Result<void>* fst_err_result = first_error_.load(std::memory_order_relaxed);
    if (fst_err_result) {
        promise_all_->setError(std::move(fst_err_result->err));
    } else {
        promise_all_->setValue(Void{});
    }
}

template <class T>
void GroupState<T>::produceFirst() {
    assert(promise_first_.has_value());
    // Try to get first value
    Result<T>* first_val_result = first_value_.load(std::memory_order_relaxed);
    if (first_val_result) {
        if constexpr (std::is_same_v<T, void>) {
            promise_first_->setValue(Void{});
        } else {
            assert(first_val_result->val);
            promise_first_->setValue(std::move(*first_val_result->val));
        }
        return;
    }
    // Check for errors
    Result<T>* last_err_result = last_error_.load(std::memory_order_relaxed);
    assert(last_err_result);
    promise_first_->setError(std::move(last_err_result->err));
}


// ==================== REGISTRTORS ==================== //

template <class T>
void GroupState<T>::registerValue(Result<T>* result) {
    if (first_value_.load(std::memory_order_relaxed) == nullptr) {
        details::Result<T>* expected = nullptr;
        first_value_.compare_exchange_strong(expected, result, std::memory_order_acq_rel);
    }
    num_pending_.fetch_add(-1, std::memory_order_acq_rel);
}

template <class T>
void GroupState<T>::registerError(Result<T>* result) {
    if (first_error_.load(std::memory_order_relaxed) == nullptr) {
        details::Result<T>* expected = nullptr;
        first_error_.compare_exchange_strong(expected, result, std::memory_order_acq_rel);
    }
    last_error_.store(result, std::memory_order_release);
    num_pending_.fetch_add(-1, std::memory_order_acq_rel);
}


// ==================== SUBSCRIPTION ==================== //

template <class T>
Future<GroupAllType<T> > GroupState<T>::subscribeToAll() {
    assert(group_type_.load(std::memory_order_relaxed) == kPending);
    auto [promise, future] = contract<GroupAllType<T> >();
    promise_all_.emplace(std::move(promise));
    group_type_.store(kReadyAll, std::memory_order_release);
    return std::move(future);
}

template <class T>
Future<GroupFirstType<T> > GroupState<T>::subscribeToFirst() {
    assert(group_type_.load(std::memory_order_relaxed) == kPending);
    auto [promise, future] = contract<GroupFirstType<T> >();
    promise_first_.emplace(std::move(promise));
    group_type_.store(kReadyFirst, std::memory_order_release);
    return std::move(future);
}


// ==================== ATTACH / DETACH ==================== //

template <class T>
Result<T>* GroupState<T>::attach() {
    results_.emplace_back();
    num_pending_.fetch_add(1, std::memory_order_acq_rel);
    return &(results_.back());
}

template <class T>
void GroupState<T>::detach() {
    auto num_pending = num_pending_.load(std::memory_order_acquire);
    auto group_type = group_type_.load(std::memory_order_acquire);
    // subscribeToAll already executed
    if (group_type == kReadyAll) {
        Result<T>* fst_error = first_error_.load(std::memory_order_acquire);
        if (num_pending == 0 || fst_error != nullptr) {
            if (group_type_.exchange(kProduced, std::memory_order_acq_rel) != kProduced) {
                produceAll();
            }
        }
    }
    // subscribeToFirst already executed
    if (group_type == kReadyFirst) {
        Result<T>* fst_value = first_value_.load(std::memory_order_acquire);
        if (num_pending == 0 || fst_value != nullptr) {
            if (group_type_.exchange(kProduced, std::memory_order_acq_rel) != kProduced) {
                produceFirst();
            }
        }
    }
}

}  // namespace details


// ================================================== //
// ==================== TaskGroup ==================== //
// ================================================== //

template <class T>
class TaskGroup {

template <class U> friend class JoinSubscription;

public:
    TaskGroup() {
        auto [promise, future] = contract<GroupAllType<T> >();
        state_ = std::make_shared<details::GroupState<T> >();
    }

    void join(AsyncResult<T> result);

    AsyncResult<GroupAllType<T>> all();
    AsyncResult<GroupFirstType<T> > first();

private:
    std::shared_ptr<details::GroupState<T> > state_;
};


// ============================================== //
// ==================== JOIN ==================== //
// ============================================== //

template <class T>
class JoinSubscription : public ISubscription<PhysicalType<T>> {
public:
    JoinSubscription(std::shared_ptr<details::GroupState<T> > state)
        : state_(std::move(state))
    {
        local_result_ = state_->attach();
    }

    void resolveValue([[maybe_unused]] PhysicalType<T> value, ResolvedBy) override {
        if constexpr (!std::is_same_v<T, void>) {
            local_result_->val.emplace(std::move(value));
        }
        state_->registerValue(local_result_);
        state_->detach();
        state_.reset();
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        local_result_->err = std::move(err);
        state_->registerError(local_result_);
        state_->detach();
        state_.reset();
    }

private:
    std::shared_ptr<details::GroupState<T> > state_;
    details::Result<T>* local_result_;
};

template <class T>
void TaskGroup<T>::join(AsyncResult<T> res) {
    res.fut_.subscribe(std::make_unique<JoinSubscription<T>>(state_));
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
