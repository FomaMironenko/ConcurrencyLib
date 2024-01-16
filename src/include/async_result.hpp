#pragma once

#include <type_traits>
#include <utility>
#include <future>
#include <memory>

#include "../private/async_task.hpp"
#include "../private/type_traits.hpp"
#include "contract.hpp"
#include "thread_pool.hpp"


enum class ThenPolicy { Lazy, Eager, NoSchedule };

template <class T, class Err>
using ErrorHandler = std::function<T(const Err&)>;


template <class T>
class AsyncResult {

friend class ThreadPool;
template <class U> friend class AsyncResult;
template <class U> friend class TaskGroup;
template <class U> friend class FlattenSubscription;
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);

private:
    AsyncResult(ThreadPool* pool, Future<T> fut)
        : fut_(std::move(fut))
        , parent_pool_(pool)
    {    }

public:
    AsyncResult() : fut_(), parent_pool_(nullptr) { }
    AsyncResult(const AsyncResult&) = delete;
    AsyncResult(AsyncResult&&) = default;
    AsyncResult& operator=(AsyncResult&&) = default;
    AsyncResult& operator=(const AsyncResult&) = delete;

    // Synchronously wait for the result to be produced.
    // Does not invalidate the object.
    void wait();

    // Synchronously get the result.
    // Invalidates the object.
    T get();

    // Converts AsyncResult to std::furute
    // Invalidates the object.
    std::future<T> to_std();

    // Asynchronously unwrap nested AsyncResult.
    // Can be called only with AsyncResult< AsyncResult<...> > types.
    // Invalidates the object.
    template <class U = T>
    std::enable_if_t<is_async_result<U>::value, T> flatten();

    // Create a ready-to-use AsyncResult filled with value
    template <class U = T>
    static std::enable_if_t< std::is_same_v<U, void>, AsyncResult<U>> instant();

    template <class U = T>
    static std::enable_if_t<!std::is_same_v<U, void>, AsyncResult<U>> instant(U value);

    // Create a ready-to-use AsyncResult filled with error
    static AsyncResult<T> instantFail(std::exception_ptr error);

    // Continue task execution in parent ThreadPool.
    // Invalidates the object.
    template <class Ret>
    AsyncResult<Ret> then(FunctionType<Ret, T> func, ThenPolicy policy = ThenPolicy::Lazy);

    // Handle an error if one of this type exists.
    // Invalidates the object.
    template <class Err>
    AsyncResult<T> catch_err(ErrorHandler<T, Err> handler);

    // Schedules subsequent execution to another ThreadPool.
    // Invalidates the object.
    AsyncResult<T> in(ThreadPool& pool);

private:
    Future<T> fut_;
    ThreadPool* parent_pool_;
};


// ==================================================== //
// ==================== WAIT & GET ==================== //
// ==================================================== //

template <class T>
void AsyncResult<T>::wait() {
    fut_.wait();
}

template <class T>
T AsyncResult<T>::get() {
    if constexpr (std::is_same_v<T, void>) {
        fut_.get();
    } else {
        return fut_.get();
    }
}


// ================================================ //
// ==================== TO STD ==================== //
// ================================================ //

template <class T>
class ToStdSubscription : public ISubscription<PhysicalType<T> > {
public:
    explicit ToStdSubscription(std::promise<T> std_promise)
        : std_promise_(std::move(std_promise)) {   }

    void resolveValue([[maybe_unused]] PhysicalType<T> val, ResolvedBy) override {
        if constexpr (std::is_same_v<T, void>) {
            std_promise_.set_value();
        } else {
            std_promise_.set_value(std::move(val));
        }
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        std_promise_.set_exception(err);
    }

private:
    std::promise<T> std_promise_;
};

template <class T>
std::future<T> AsyncResult<T>::to_std() {
    auto std_promise = std::promise<T>();
    auto std_future = std_promise.get_future();
    fut_.subscribe(std::make_unique<ToStdSubscription<T> >(std::move(std_promise)));
    return std_future;
}


// ========================================================= //
// ==================== INSTANT RESULTS ==================== //
// ========================================================= //

template <class T>
template <class U>
std::enable_if_t<std::is_same_v<U, void>, AsyncResult<U>> AsyncResult<T>::instant() {
    static_assert(std::is_same_v<T, U>, "Cannot call instant with non-default template argument");
    return AsyncResult<void>{nullptr, Future<void>::instantValue(Void{})};
}

template <class T>
template <class U>
std::enable_if_t<!std::is_same_v<U, void>, AsyncResult<U>> AsyncResult<T>::instant(U value) {
    static_assert(std::is_same_v<T, U>, "Cannot call instant with non-default template argument");
    return AsyncResult<U>{nullptr, Future<U>::instantValue(std::move(value))};
}


template <class T>
AsyncResult<T> AsyncResult<T>::instantFail(std::exception_ptr error) {
    return AsyncResult<T>{nullptr, Future<T>::instantError(std::move(error))};
}


// ============================================ //
// ==================== IN ==================== //
// ============================================ //

template <class T>
AsyncResult<T> AsyncResult<T>::in(ThreadPool& pool) {
    return AsyncResult<T>{&pool, std::move(fut_)};
}


// =============================================== //
// ==================== CATCH ==================== //
// =============================================== //

template <class T, class Err>
class CatchSubscription : public PipeSubscription<T, T> {
public:
    explicit CatchSubscription(ErrorHandler<T, Err> handler, Promise<T> promise)
        : PipeSubscription<T, T>(std::move(promise))
        , handler_(std::move(handler)) {   }

    void resolveValue(PhysicalType<T> val, ResolvedBy) override {
        promise_.setValue(std::move(val));
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        try {
            std::rethrow_exception(err);
        } catch (const Err& err) {
            try {
                // Don't trust user handler
                if constexpr (std::is_same_v<T, void>) {
                    handler_(err);
                    promise_.setValue(Void{});
                } else {
                    T new_val = handler_(err);
                    promise_.setValue(std::move(new_val));
                }
            } catch (...) {
                promise_.setError(std::current_exception());
            }
        } catch (...) {
            promise_.setError(std::current_exception());
        }
    }

private:
    using PipeSubscription<T, T>::promise_;
    ErrorHandler<T, Err> handler_;
};

template <class T>
template <class Err>
AsyncResult<T> AsyncResult<T>::catch_err(ErrorHandler<T, Err> handler) {
    auto [promise, future] = contract<T>();
    fut_.subscribe(std::make_unique<CatchSubscription<T, Err> >(
        std::move(handler), std::move(promise)));
    return AsyncResult<T>{parent_pool_, std::move(future)};
}


// ============================================== //
// ==================== THEN ==================== //
// ============================================== //

template <class Ret, class Arg>
class ThenSubscription : public PipeSubscription<Ret, Arg> {
public:
    ThenSubscription(FunctionType<Ret, Arg> func,
                     Promise<Ret> promise,
                     ThreadPool* continuation_pool,
                     ThenPolicy policy
    )
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

    void resolveValue([[maybe_unused]] PhysicalType<Arg> value, ResolvedBy by) override {
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

private:
    using PipeSubscription<Ret, Arg>::promise_;
    FunctionType<Ret, Arg> func_;
    ThreadPool * continuation_pool_;
    ThenPolicy execution_policy_;
};

template <class T>
template <class Ret>
AsyncResult<Ret> AsyncResult<T>::then(FunctionType<Ret, T> func, ThenPolicy policy) {
    auto [promise, future] = contract<Ret>();
    fut_.subscribe(std::make_unique<ThenSubscription<Ret, T> >(
        std::move(func), std::move(promise), parent_pool_, policy));
    return AsyncResult<Ret>{parent_pool_, std::move(future)};
}


// ================================================= //
// ==================== FLATTEN ==================== //
// ================================================= //

template <class Ret>
class FlattenSubscription : public PipeSubscription<Ret, AsyncResult<Ret>> {
public:
    FlattenSubscription(Promise<Ret> promise)
        : PipeSubscription<Ret, AsyncResult<Ret>>(std::move(promise))
    {   }

    void resolveValue(AsyncResult<Ret> async_val, ResolvedBy) override {
        async_val.fut_.subscribe(
            std::make_unique<ForwardSubscription<Ret> >(std::move(promise_))
        );
    }

private:
    using PipeSubscription<Ret, AsyncResult<Ret> >::promise_;
};

template <class T>
template <class U>
std::enable_if_t<is_async_result<U>::value, T> AsyncResult<T>::flatten() {
    static_assert(std::is_same_v<T, U>, "Cannot call flatten with non-default template argument");
    using Ret = typename async_type<T>::type;
    // Utilize duck typing
    auto [promise, future] = contract<Ret>();
    fut_.subscribe( std::make_unique<FlattenSubscription<Ret> >(std::move(promise)) );
    return AsyncResult<Ret>{parent_pool_, std::move(future)};
}
