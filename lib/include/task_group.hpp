#pragma once

#include <atomic>
#include <memory>
#include <list>
#include <optional>
#include <exception>

#include "thread_pool.hpp"
#include "async_result.hpp"


// =============================================== //
// ==================== UTILS ==================== //
// =============================================== //

namespace details {

    template <class T>
    struct ResultBox {
        std::optional<T> val = std::nullopt;
        std::exception_ptr err = nullptr;
    };

    template <class T>
    struct GroupState {
        using ContractType = typename std::conditional_t<
            std::is_same_v<T, void>,
            Void, T
        >;
        explicit GroupState(Promise<std::vector<ContractType>> promise)
            : num_submitted(0)
            , num_ready(0)
            , all_submitted(false)
            , fired(false)
            , promise(std::move(promise))
        {   }

        std::atomic<size_t> num_submitted;
        std::atomic<size_t> num_ready;
        std::atomic<bool> all_submitted;
        std::atomic<bool> fired;
        std::list<std::shared_ptr<ResultBox<T>>> results;
        Promise<std::vector<ContractType>> promise;
    };

}  // namespace details


// ================================================== //
// ==================== GroupAll ==================== //
// ================================================== //

template <class T>
class GroupAll {

template <class U> friend class JoinSubscription;

    using ContractType = typename std::conditional_t<
        std::is_same_v<T, void>,
        Void, T
    >;

public:
    GroupAll() {
        auto [promise, future] = contract<std::vector<ContractType>>();
        state_ = std::make_shared<details::GroupState<T>>(std::move(promise));
        future_opt_ = std::move(future);
    }

    void join(AsyncResult<T> result);

    AsyncResult<std::vector<T>> merge(ThreadPool& pool);

private:
    static void onAllReady(Promise<std::vector<ContractType>>& promise,
                           std::list<std::shared_ptr<details::ResultBox<T>>>&& results);

private:
    std::shared_ptr<details::GroupState<T>> state_;
    std::optional<Future<std::vector<ContractType>>> future_opt_;
};


// ============================================== //
// ==================== JOIN ==================== //
// ============================================== //

template <class T>
class JoinSubscription : public ISubscription<T> {
public:
    JoinSubscription(std::shared_ptr<details::GroupState<T>> state)
        : state_(state)
    {
        result_ = std::make_shared<details::ResultBox<T>>();
        state_->results.push_back(result_);
    }

    void resolveValue(T value) override {
        result_->val = std::move(value);
        resolve();
    }

    void resolveError(std::exception_ptr err) override {
        result_->err = std::move(err);
        resolve();
    }

private:
    void resolve() {
        state_->num_ready.fetch_add(1);
        if (state_->all_submitted.load() &&
            state_->num_ready.load() == state_->num_submitted.load() &&
            !state_->fired.exchange(true)
        ) {
            GroupAll<T>::onAllReady(state_->promise, std::move(state_->results));
        }
    }

private:
    std::shared_ptr<details::GroupState<T>> state_;
    std::shared_ptr<details::ResultBox<T>> result_;
};

template <class T>
void GroupAll<T>::join(AsyncResult<T> res) {
    state_->num_submitted.fetch_add(1);
    res.fut_.subscribe(std::make_unique<JoinSubscription<T>>(state_));
}


// ============================================================== //
// ==================== MERGE & ON ALL READY ==================== //
// ============================================================== //

template <class T>
AsyncResult<std::vector<T>> GroupAll<T>::merge(ThreadPool& pool) {
    if (!future_opt_.has_value()) {
        throw std::runtime_error("Trying to get merge GroupAll twice");
    }
    Future<std::vector<T>> fut = std::move(*future_opt_);
    future_opt_ = std::nullopt;
    state_->all_submitted.store(true);
    if (state_->num_ready.load() == state_->num_submitted.load() &&
        !state_->fired.exchange(true)
    ) {
        onAllReady(state_->promise, std::move(state_->results));
    }
    return {pool, std::move(fut)};
}

template <class T>
void GroupAll<T>::onAllReady(Promise<std::vector<ContractType>>& promise,
                             std::list<std::shared_ptr<details::ResultBox<T>>>&& results
) {
    std::vector<T> values;
    std::exception_ptr err = nullptr;
    for (auto& res_ptr : results) {
        if (res_ptr->err) {
            err = std::move(res_ptr->err);
            break;
        } else if (res_ptr->val) {
            values.push_back(std::move(*res_ptr->val));
            res_ptr->val = std::nullopt;
        } else {
            LOG_ERR << "No result was produced prior to onAllReady";
            assert(false);
        }
    }
    results.clear();
    if (err) {
        promise.setError(std::move(err));
    } else {
        promise.setValue(std::move(values));
    }
}
