#pragma once

#include <functional>

#include "pipe_sub.hpp"
#include "../type_traits.hpp"
#include "../async/task.hpp"
#include "../async/bound_task.hpp"

#include "tp/thread_pool.hpp"


enum class ThenPolicy { Lazy, Eager, NoSchedule };


namespace details {

template <class Ret, class Arg>
class ThenSubscription : public PipeSubscription<Ret, Arg> {
public:
    ThenSubscription(FunctionType<Ret, Arg> func,
                     Promise<Ret> promise,
                     ThreadPool* continuation_pool,
                     ThenPolicy policy);

    void resolveValue(PhysicalType<Arg> value, ResolvedBy by) override;

private:
    using PipeSubscription<Ret, Arg>::promise_;
    FunctionType<Ret, Arg> func_;
    ThreadPool * continuation_pool_;
    ThenPolicy execution_policy_;
};


// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //

template <class Ret, class Arg>
ThenSubscription<Ret, Arg>::ThenSubscription(FunctionType<Ret, Arg> func,
                                             Promise<Ret> promise,
                                             ThreadPool* continuation_pool,
                                             ThenPolicy policy)
    : PipeSubscription<Ret, Arg> (std::move(promise))
    , func_(std::move(func))
    , continuation_pool_(continuation_pool)
    , execution_policy_(policy)
{
    if (continuation_pool_ == nullptr && execution_policy_ != ThenPolicy::NoSchedule) {
        LOG_WARN << "Enforcing ThenPolicy::NoSchedule due to empty thread pool";
        execution_policy_ = ThenPolicy::NoSchedule;
    }
}

template <class Ret, class Arg>
void ThenSubscription<Ret, Arg>::resolveValue([[maybe_unused]] PhysicalType<Arg> value, ResolvedBy by)
{
    ThreadPool::Task pool_task = nullptr;
    if constexpr (std::is_same_v<Arg, void>) {
        pool_task = details::make_async_task<Ret>(std::move(func_), std::move(promise_));
    } else {
        pool_task = details::make_bound_async_task<Ret, Arg>(std::move(func_), std::move(promise_), std::move(value));
    }
    if (execution_policy_ == ThenPolicy::NoSchedule) {
        pool_task->run();
    } else if (execution_policy_ == ThenPolicy::Eager && by == ResolvedBy::kProducer) {
        pool_task->run();
    } else {
        continuation_pool_->submit(std::move(pool_task));
    }
}

}  // namespace details
