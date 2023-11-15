#include "utils/logger.hpp"
#include "thread_pool.hpp"


void runWorkerLoop(ThreadPool *pool) {
    for (;;) {
        ThreadPool::Task task = nullptr;
        {
            std::unique_lock guard(pool->mtx_);
            pool->queue_cv_.wait(guard, [pool]() {
                return !pool->tasks_.empty() || pool->stopped_;
            });
            if (pool->stopped_) {
                break;
            }
            task = std::move(pool->tasks_.front());
            pool->tasks_.pop();
        }
        if (!task) {
            LOG_ERR << "Empty task was returned from task queue";
            continue;
        }
        task->run();
    }
}

void ThreadPool::start(int num_threads) {
    if (!workers_.empty() || stopped_) {
        LOG_ERR << "Attempting to start thread an already running thread pool";
        throw std::runtime_error("ThreadPool::start twice");
    }
    LOG_INFO << "Starting a thread pool with " << num_threads << " workers";
    for (int i_thread = 0; i_thread < num_threads; ++i_thread) {
        workers_.push_back(std::thread(runWorkerLoop, this));
    }
}

void ThreadPool::stop() {
    {
        std::unique_lock guard(mtx_);
        stopped_ = true;
        // notify workers in worker loop that pool was stopped
        queue_cv_.notify_all();
    }
    for (auto &worker : workers_) {
        worker.join();
    }
    workers_.clear();
    stopped_ = false;
}

void ThreadPool::submit(ThreadPool::Task task) {
    std::unique_lock guard(mtx_);
    tasks_.push(std::move(task));
    guard.unlock();
    queue_cv_.notify_one();
}
