#pragma once

#include <utility>
#include <functional>
#include <type_traits>

#include "async_result.hpp"
#include "thread_pool.hpp"


template <class Ret, class Fun, class ...Args>
inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args);

template <class Fun>
inline auto make_async(ThreadPool& pool, Fun&& fun);


template <class Fun>
class AsyncFunction {
private:
    AsyncFunction(ThreadPool& pool, std::function<Fun> fun) : parent_pool_(&pool), callable_(std::move(fun)) { }

public:
    template <class Ret, class ...Args>
    AsyncResult<Ret> invoke(Args &&...args) {
        return call_async<Ret>(*parent_pool_, callable_, std::forward<Args>(args)...);
    }

    template <class ...Args>
    AsyncResult<std::invoke_result_t<Fun, Args...> > operator()(Args &&...args) {
        return call_async<std::invoke_result_t<Fun, Args...> >(*parent_pool_, callable_, std::forward<Args>(args)...);
    }

private:
    ThreadPool* parent_pool_;
    std::function<Fun> callable_;

template <class F>
friend inline auto make_async(ThreadPool& pool, F&& fun);
};



template <class Ret, class Fun, class ...Args>
inline AsyncResult<Ret> call_async(ThreadPool& pool, Fun&& fun, Args &&...args) {
    auto [promise, future] = contract<Ret>();
    FunctionType<Ret, void> task = std::bind(std::forward<Fun>(fun), std::forward<Args>(args)...);
    ThreadPool::Task pool_task = std::make_unique<details::AsyncTask<Ret> >(std::move(task), std::move(promise));
    pool.submit(std::move(pool_task));
    return AsyncResult<Ret>{&pool, std::move(future)};
}

template <class Fun>
inline auto make_async(ThreadPool& pool, Fun&& fun) {
    return AsyncFunction{pool, std::function(fun)};
}
