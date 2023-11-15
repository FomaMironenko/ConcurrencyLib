#pragma once

#include <type_traits>
#include <utility>
#include <memory>

#include "../private/async_task.hpp"
#include "contract.hpp"
#include "thread_pool.hpp"


template <class T>
class AsyncResult {
private:
    AsyncResult(ThreadPool& pool, Future<T> fut)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:
    static AsyncResult instant(ThreadPool& pool, T value);
    static AsyncResult instantFail(ThreadPool& pool, std::exception_ptr error);

    T get() { return fut_.get(); }

    template <class Ret>
    AsyncResult<Ret> then(std::function<Ret(T)> func);

    template <class Ret>
    AsyncResult<Ret> flatten(std::function<AsyncResult<Ret>(T)> func);

private:
    Future<T> fut_;
    ThreadPool* parent_pool_;

friend class ThreadPool;
template <class U>
friend class AsyncResult;
template <class U>
friend class GroupAll;
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);
};


template <>
class AsyncResult<void> {
private:
    AsyncResult(ThreadPool& pool, Future<Void> fut)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:
    void get() { fut_.get(); }

    template <class Ret>
    AsyncResult<Ret> then(std::function<Ret()> func);

private:
    Future<Void> fut_;
    ThreadPool* parent_pool_;

friend class ThreadPool;
template <class U>
friend class AsyncResult;
template <class U>
friend class GroupAll;
template <class Ret, class Fun, class ...Args>
friend inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);
};



template <class T>
AsyncResult<T> AsyncResult<T>::instant(ThreadPool& pool, T value) {
    return AsyncResult<T>{pool, Future<T>::instantValue(std::move(value))};
}

template <class T>
AsyncResult<T> AsyncResult<T>::instantFail(ThreadPool& pool, std::exception_ptr error) {
    return AsyncResult<T>{pool, Future<T>::instantError(std::move(error))};
}


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

template <class T>
template <class Ret>
AsyncResult<Ret> AsyncResult<T>::flatten(std::function<AsyncResult<Ret>(T)> func) {
    auto [promise, future] = contract<Ret>();
    auto promise_box = std::make_shared<details::PromiseBox<Ret>>(std::move(promise));
    fut_.subscribe(
        [pool = parent_pool_, promise_box, func = std::move(func)] (T value) {
            auto res = func(std::move(value));
            res.fut_.subscribe(
                [promise_box](Ret ret) { promise_box->get().setValue(std::move(ret)); },
                [promise_box](std::exception_ptr err) { promise_box->get().setError(err); }
            );
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