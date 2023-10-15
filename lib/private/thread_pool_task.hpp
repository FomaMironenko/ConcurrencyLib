#pragma once

#include <functional>

#include "contract.hpp"
#include "void.hpp"


namespace details {

class ITaskBase {
public:
    virtual ~ITaskBase() = default;
    virtual void run() = 0;
};


template <class T>
class Task : public ITaskBase {
public:
    Task(std::function<T()>&& func, Promise<T> promise)
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
class Task<void> : public ITaskBase {
public:
    Task(std::function<void()>&& func, Promise<Void> promise)
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
