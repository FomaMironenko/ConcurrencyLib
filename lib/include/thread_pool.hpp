#pragma once

#include <type_traits>
#include <utility>
#include <memory>
#include <vector>
#include <queue>

#include <condition_variable>
#include <mutex>

#include "../private/thread_pool_task.hpp"
#include "contract.hpp"


class ThreadPool;

template <class T>
class AsyncResult {
private:
    AsyncResult(Future<T> fut, ThreadPool& pool)
        : fut_(std::move(fut))
        , parent_pool_(&pool)
    {    }

public:
    static AsyncResult instant(T value, ThreadPool& pool);
    static AsyncResult instantFail(std::exception_ptr error, ThreadPool& pool);

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
};


template <>
class AsyncResult<void> {
private:
    AsyncResult(Future<Void> fut, ThreadPool& pool)
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
};



class ThreadPool {
    using ThreadPoolTask = std::unique_ptr<details::ITaskBase>;
public:
    ThreadPool() : stopped_(false) {    }
    explicit ThreadPool(int num_workers) : ThreadPool() { start(num_workers); }
    ~ThreadPool() { stop(); }

    void start(int num_threads);
    void stop();

    template <class Ret, class Fun, class ...Args>
    AsyncResult<Ret> submit(Fun&& func, Args&& ...args);

private:
    void enqueueTask(ThreadPoolTask task);

private:
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable queue_cv_;
    std::queue<ThreadPoolTask> tasks_;
    bool stopped_;

friend void runWorkerLoop(ThreadPool*);
template <class T>
friend class AsyncResult;
};


template <class Ret, class Fun, class ...Args>
AsyncResult<Ret> ThreadPool::submit(Fun&& func, Args&& ...args) {
    using ContractType = typename std::conditional_t<
        std::is_same_v<Ret, void>,
        Void, Ret
    >;
    auto [promise, future] = contract<ContractType>();
    std::function<Ret()> task = std::bind(std::forward<Fun>(func), std::forward<Args>(args)...);
    ThreadPoolTask pool_task = std::make_unique<details::Task<Ret>>(std::move(task), std::move(promise));
    enqueueTask(std::move(pool_task));
    return {std::move(future), *this};
}


template <class T>
AsyncResult<T> AsyncResult<T>::instant(T value, ThreadPool& pool) {
    return AsyncResult<T>{Future<T>::instantValue(std::move(value)), pool};
}

template <class T>
AsyncResult<T> AsyncResult<T>::instantFail(std::exception_ptr error, ThreadPool& pool) {
    return AsyncResult<T>{Future<T>::instantError(std::move(error)), pool};
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
            ThreadPool::ThreadPoolTask pool_task = std::make_unique<details::Task<Ret>>(std::move(task), promise_box->get());
            pool->enqueueTask(std::move(pool_task));
        },
        [promise_box] (std::exception_ptr err) {
            promise_box->get().setError(err);
        }
    );
    return {std::move(future), *parent_pool_};
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
    return {std::move(future), *parent_pool_};
}


template <class Ret>
AsyncResult<Ret> AsyncResult<void>::then(std::function<Ret()> func) {
    auto [promise, future] = contract<Void>();
    auto promise_box = std::make_shared<details::PromiseBox<Void>>(std::move(promise));
    fut_.subscribe(
        [pool = parent_pool_, promise_box, func = std::move(func)] (Void) {
            ThreadPool::ThreadPoolTask pool_task = std::make_unique<details::Task<Ret>>(std::move(func), promise_box->get());
            pool->enqueueTask(std::move(pool_task));
        },
        [promise_box] (std::exception_ptr err) {
            promise_box->get().setError(err);
        }
    );
    return {std::move(future), *parent_pool_};
}
