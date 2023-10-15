#pragma once

#include <utility>
#include <memory>
#include <vector>
#include <queue>

#include <condition_variable>
#include <mutex>

#include "../private/thread_pool_task.hpp"
#include "contract.hpp"
#include "async_result.hpp"


class ThreadPool {
    using ThreadPoolTask = std::unique_ptr<details::ITaskBase>;
public:
    ThreadPool() : stopped_(false) {    }
    explicit ThreadPool(int num_workers) : ThreadPool() { start(num_workers); }
    ~ThreadPool() { stop(); }

    void start(int num_threads);
    void stop();

    template <class Ret, class Fun, class ...Args>
    AsyncResult<Ret> submit(Fun func, Args &&...args);

private:
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable queue_cv_;
    std::queue<ThreadPoolTask> tasks_;
    bool stopped_;

friend void runWorkerLoop(ThreadPool*);
};


template <class Ret, class Fun, class ...Args>
AsyncResult<Ret> ThreadPool::submit(Fun func, Args &&...args) {
    auto [promise, future] = contract<Ret>();
    std::function<Ret()> task = std::bind(std::forward<Fun>(func), std::forward<Args>(args)...);
    ThreadPoolTask pool_task = std::make_unique<details::Task<Ret>>(std::move(task), std::move(promise));
    {
        std::lock_guard guard(mtx_);
        tasks_.push(std::move(pool_task));
        queue_cv_.notify_one();
    }
    return AsyncResult<Ret>(std::move(future));
}
