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
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);

private:
    AsyncResult(ThreadPool& pool, Future<T> fut)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:

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
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);

private:
    AsyncResult(ThreadPool& pool, Future<Void> fut)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:
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

template <class T>
template <class Ret>
AsyncResult<Ret> AsyncResult<T>::then(std::function<Ret(T)> func) {
    using ContractType = typename std::conditional_t<
        std::is_same_v<Ret, void>,
        Void, Ret
    >;
    auto [promise, future] = contract<ContractType>();
    auto promise_box = std::make_shared<details::PromiseBox<ContractType>>(std::move(promise));
    fut_.subscribe(
        [pool = parent_pool_, promise_box, func = std::move(func)] (T value) {
            std::function<Ret()> task = std::bind(std::move(func), std::move(value));
            ThreadPool::Task pool_task = std::make_unique<details::AsyncTask<Ret>>(std::move(task), promise_box->get());
            pool->submit(std::move(pool_task));
        },
        [promise_box] (std::exception_ptr err) {
            promise_box->get().setError(err);
        }
    );
    return AsyncResult<Ret>{*parent_pool_, std::move(future)};
}


template <class Ret>
AsyncResult<Ret> AsyncResult<void>::then(std::function<Ret()> func) {
    auto [promise, future] = contract<Void>();
    auto promise_box = std::make_shared<details::PromiseBox<Void>>(std::move(promise));
    fut_.subscribe(
        [pool = parent_pool_, promise_box, func = std::move(func)] (Void) {
            ThreadPool::Task pool_task = std::make_unique<details::AsyncTask<Ret>>(std::move(func), promise_box->get());
            pool->submit(std::move(pool_task));
        },
        [promise_box] (std::exception_ptr err) {
            promise_box->get().setError(err);
        }
    );
    return {std::move(future), *parent_pool_};
}


// ================================================= //
// ==================== FLATTEN ==================== //
// ================================================= //

template <class T>
T AsyncResult<T>::flatten() {
    // Cannot be compiled for non AsyncResult type T
    static_assert(is_async_result<T>::value, "flatten cannot be used with non nested AsyncResults");
    using Ret = typename async_type<T>::type;
    // Utilize duck typing
    auto [promise, future] = contract<Ret>();
    auto promise_box = std::make_shared<details::PromiseBox<Ret>>(std::move(promise));
    fut_.subscribe(
        [promise_box] (AsyncResult<Ret> child_res) {
            child_res.fut_.subscribe(
                [promise_box](Ret ret) {
                    promise_box->get().setValue(std::move(ret));
                },
                [promise_box](std::exception_ptr err) {
                    promise_box->get().setError(err);
                }
            );
        },
        [promise_box] (std::exception_ptr err) {
            promise_box->get().setError(err);
        }
    );
    return AsyncResult<Ret>{*parent_pool_, std::move(future)};
}
