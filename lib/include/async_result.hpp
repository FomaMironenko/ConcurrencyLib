#pragma once

#include <type_traits>
#include <utility>
#include <memory>

#include "../private/async_task.hpp"
#include "../private/type_traits.hpp"
#include "contract.hpp"
#include "thread_pool.hpp"


template <class T>
class AsyncResult {

friend class ThreadPool;
template <class U> friend class AsyncResult;
template <class U> friend class GroupAll;
template <class U> friend class FlattenSubscription;
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);

private:
    AsyncResult(ThreadPool& pool, Future<T> fut)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:
    AsyncResult(const AsyncResult&) = delete;
    AsyncResult(AsyncResult&&) = default;
    AsyncResult& operator=(AsyncResult&&) = default;
    AsyncResult& operator=(const AsyncResult&) = delete;

    // Create a ready-to-use AsyncResult filled with value
    static AsyncResult instant(ThreadPool& pool, T value);

    // Create a ready-to-use AsyncResult filled with error
    static AsyncResult instantFail(ThreadPool& pool, std::exception_ptr error);

    // Synchronously wait for the result to be produced.
    // Does not invalidate the object.
    void wait();

    // Synchronously get the result.
    // Invalidates the object.
    T get();

    // Asynchronously unwrap nested AsyncResult.
    // Can be called only with AsyncResult< AsyncResult<...> > types.
    // Invalidates the object.
    T flatten();

    // Continue task execution in parent ThreadPool.
    // Invalidates the object.
    template <class Ret>
    AsyncResult<Ret> then(std::function<Ret(T)> func);

private:
    Future<T> fut_;
    ThreadPool* parent_pool_;
};


template <>
class AsyncResult<void> {

friend class ThreadPool;
template <class U> friend class AsyncResult;
template <class U> friend class GroupAll;
template <class U> friend class FlattenSubscription;
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);

private:
    AsyncResult(ThreadPool& pool, Future<Void> fut)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:
    AsyncResult(const AsyncResult&) = delete;
    AsyncResult(AsyncResult&&) = default;
    AsyncResult& operator=(AsyncResult&&) = default;
    AsyncResult& operator=(const AsyncResult&) = delete;

    // Create a ready-to-use AsyncResult<void>
    static AsyncResult instant(ThreadPool& pool);

    // Create a ready-to-use AsyncResult filled with error
    static AsyncResult instantFail(ThreadPool& pool, std::exception_ptr error);

    // Synchronously wait for the result to be produced.
    // Does not invalidate the object.
    void wait();

    // Synchronously wait for result.
    // Invalidates the object.
    void get();

    // Continue task execution in parent ThreadPool.
    // Invalidates the object.
    template <class Ret>
    AsyncResult<Ret> then(std::function<Ret()> func);

private:
    Future<Void> fut_;
    ThreadPool* parent_pool_;
};


// ==================================================== //
// ==================== WAIT & GET ==================== //
// ==================================================== //

template <class T>
void AsyncResult<T>::wait() {
    fut_.wait();
}

void AsyncResult<void>::wait() {
    fut_.wait();
}

template <class T>
T AsyncResult<T>::get() {
    return fut_.get();
}

void AsyncResult<void>::get() {
    fut_.get();
}


// ========================================================= //
// ==================== INSTANT RESULTS ==================== //
// ========================================================= //

template <class T>
AsyncResult<T> AsyncResult<T>::instant(ThreadPool& pool, T value) {
    return AsyncResult<T>{pool, Future<T>::instantValue(std::move(value))};
}

AsyncResult<void> AsyncResult<void>::instant(ThreadPool& pool) {
    return AsyncResult<void>{pool, Future<Void>::instantValue(Void{})};
}

template <class T>
AsyncResult<T> AsyncResult<T>::instantFail(ThreadPool& pool, std::exception_ptr error) {
    return AsyncResult<T>{pool, Future<T>::instantError(std::move(error))};
}

AsyncResult<void> AsyncResult<void>::instantFail(ThreadPool& pool, std::exception_ptr error) {
    return AsyncResult<void>{pool, Future<Void>::instantError(std::move(error))};
}


// ============================================== //
// ==================== THEN ==================== //
// ============================================== //

template <class Ret, class Arg>
class ThenSubscription : public ISubscription<PhysicalType<Arg> > {
public:
    ThenSubscription(FunctionType<Ret, Arg> func,
                     Promise<PhysicalType<Ret> > promise,
                     ThreadPool * continuation_pool)
        : func_(std::move(func))
        , promise_(std::move(promise))
        , continuation_pool_(continuation_pool)
    {   }

    void resolveValue([[maybe_unused]] PhysicalType<Arg> value) override {
        std::function<Ret()> task;
        if constexpr (std::is_same_v<Arg, void>) {
            task = std::move(func_);
        } else {
            task = std::bind(std::move(func_), std::move(value));
        }
        ThreadPool::Task pool_task =
            details::make_async_task<Ret>(std::move(task), std::move(promise_));
        continuation_pool_->submit(std::move(pool_task));
    }

    void resolveError(std::exception_ptr err) override {
        promise_.setError(err);
    }

private:
    FunctionType<Ret, Arg> func_;
    Promise<PhysicalType<Ret> > promise_;
    ThreadPool * continuation_pool_;
};

template <class T>
template <class Ret>
AsyncResult<Ret> AsyncResult<T>::then(std::function<Ret(T)> func) {
    auto [promise, future] = contract<PhysicalType<Ret> >();
    fut_.subscribe(std::make_unique<ThenSubscription<Ret, T> >(
        std::move(func), std::move(promise), parent_pool_));
    return AsyncResult<Ret>{*parent_pool_, std::move(future)};
}

template <class Ret>
AsyncResult<Ret> AsyncResult<void>::then(std::function<Ret()> func) {
    auto [promise, future] = contract<PhysicalType<Ret> >();
    fut_.subscribe(std::make_unique<ThenSubscription<Ret, void> >(
        std::move(func), std::move(promise), parent_pool_));
    return AsyncResult<Ret>{*parent_pool_, std::move(future)};
}


// ================================================= //
// ==================== FLATTEN ==================== //
// ================================================= //

template <class Ret>
class FlattenSubscription : public ISubscription<AsyncResult<Ret> > {
public:
    FlattenSubscription(Promise<PhysicalType<Ret> > promise)
        : promise_(std::move(promise))
    {   }

    void resolveValue(AsyncResult<Ret> async_val) override {
        async_val.fut_.subscribe(
            std::make_unique<ProducerSubscription<PhysicalType<Ret> > >(std::move(promise_))
        );
    }

    void resolveError(std::exception_ptr err) override {
        promise_.setError(err);
    }

private:
    Promise<PhysicalType<Ret> > promise_;
};

template <class T>
T AsyncResult<T>::flatten() {
    // Cannot be compiled for non AsyncResult type T
    static_assert(is_async_result<T>::value, "flatten cannot be used with non nested AsyncResults");
    using Ret = typename async_type<T>::type;
    // Utilize duck typing
    auto [promise, future] = contract<PhysicalType<Ret> >();
    fut_.subscribe( std::make_unique<FlattenSubscription<Ret> >(std::move(promise)) );
    return AsyncResult<Ret>{*parent_pool_, std::move(future)};
}
