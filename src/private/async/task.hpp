#pragma once

#include <functional>

#include "../type_traits.hpp"

#include "tp/thread_pool_task_base.hpp"
#include "../core/promise.hpp"


namespace details {

template <class Ret>
class AsyncTask : public ITaskBase {
public:
    AsyncTask(FunctionType<Ret, void>&& func,
              Promise<Ret>&& promise);

    void run() override;

private:
    FunctionType<Ret, void> func_;
    Promise<Ret> promise_;
};


// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //


template <class Ret>
AsyncTask<Ret>::AsyncTask(FunctionType<Ret, void>&& func,
                          Promise<Ret>&& promise)
    : func_(std::move(func))
    , promise_(std::move(promise))
{   }


template <class Ret>
void AsyncTask<Ret>::run()
{
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


template <class Ret>
inline std::unique_ptr<AsyncTask<Ret> >
make_async_task(FunctionType<Ret, void>&& func, Promise<Ret>&& promise)
{
    return std::make_unique<AsyncTask<Ret> >(std::move(func), std::move(promise));
}

}  // namespace details
