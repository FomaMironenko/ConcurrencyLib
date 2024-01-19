#pragma once

#include <functional>

#include "../type_traits.hpp"

#include "tp/thread_pool_task_base.hpp"
#include "contract.hpp"


namespace details {

template <class Ret>
class AsyncTask : public ITaskBase {
public:
    AsyncTask(FunctionType<Ret, void>&& func, Promise<Ret>&& promise)
        : func_(std::move(func))
        , promise_(std::move(promise))
    {   }

    void run() override {
        try {
            if constexpr (std::is_same_v<Ret, void>) {
                func_();
                promise_.setValue(Void{});
            } else {
                Ret value = func_();
                promise_.setValue(std::move(value));
            }
        } catch (...) {
            promise_.setError(std::current_exception());
        }
    }

private:
    FunctionType<Ret, void> func_;
    Promise<Ret> promise_;
};

template <class Ret>
inline std::unique_ptr<AsyncTask<Ret> >
make_async_task(FunctionType<Ret, void>&& func, Promise<Ret>&& promise)
{
    return std::make_unique<AsyncTask<Ret> >(std::move(func), std::move(promise));
}


template <class Ret, class Arg>
class BoundAsyncTask : public ITaskBase {
static_assert(!std::is_same_v<Arg, void>, "BoundAsyncTask is only needed for non-void arguments");

public:
    BoundAsyncTask(FunctionType<Ret, Arg>&& func,
                   Promise<Ret>&& promise,
                   Arg&& arg)
        : func_(std::move(func))
        , promise_(std::move(promise))
        , arg_(std::move(arg))
    {   }

    void run() override {
        try {
            if constexpr (std::is_same_v<Ret, void>) {
                func_(std::move(arg_));
                promise_.setValue(Void{});
            } else {
                Ret value = func_(std::move(arg_));
                promise_.setValue(std::move(value));
            }
        } catch (...) {
            promise_.setError(std::current_exception());
        }
    }

private:
    FunctionType<Ret, Arg> func_;
    Promise<Ret> promise_;
    Arg arg_;
};

template <class Ret, class Arg>
inline std::unique_ptr<BoundAsyncTask<Ret, Arg> >
make_bound_async_task(FunctionType<Ret, Arg>&& func, Promise<Ret>&& promise, Arg&& arg)
{
    return std::make_unique<BoundAsyncTask<Ret, Arg> >(std::move(func), std::move(promise), std::move(arg));
}


}  // namespace details
