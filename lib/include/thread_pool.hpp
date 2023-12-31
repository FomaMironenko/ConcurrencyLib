#pragma once

#include <utility>
#include <memory>
#include <vector>
#include <queue>

#include <condition_variable>
#include <mutex>

#include "thread_pool_task_base.hpp"
#include "contract.hpp"


class ThreadPool {
public:
    using Task = std::unique_ptr<ITaskBase>;

    ThreadPool() : stopped_(false) {    }
    explicit ThreadPool(int num_workers) : ThreadPool() { start(num_workers); }
    ~ThreadPool() { stop(); }

    void start(int num_threads);
    void stop();

    void submit(Task task);

private:
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable queue_cv_;
    std::queue<Task> tasks_;
    bool stopped_;

friend void runWorkerLoop(ThreadPool*);
template <class T>
friend class AsyncResult;
};
