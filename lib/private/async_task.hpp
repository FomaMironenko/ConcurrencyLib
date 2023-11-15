#pragma once

#include <functional>

#include "thread_pool_task_base.hpp"
#include "contract.hpp"
#include "void.hpp"


namespace details {

template <class T>
class AsyncTask : public ITaskBase {
public:
    AsyncTask(std::function<T()>&& func, Promise<T> promise)
        : func_(std::move(func))
        , promise_(std::move(promise))
    {   }

    void run() {
        try {
            T value = func_();
            promise_.setValue(std::move(value));
        } catch (...) {
            promise_.setError(std::current_exception());
        }
    }

private:
    std::function<T()> func_;
    Promise<T> promise_;
};


template <>
class AsyncTask<void> : public ITaskBase {
public:
    AsyncTask(std::function<void()>&& func, Promise<Void> promise)
        : func_(std::move(func))
        , promise_(std::move(promise))
    {   }

    void run() {
        try {
            func_();
            promise_.setValue(Void{});
        } catch (...) {
            promise_.setError(std::current_exception());
        }
    }

private:
    std::function<void()> func_;
    Promise<Void> promise_;
};

}  // namespace details
