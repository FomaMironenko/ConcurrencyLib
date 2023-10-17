#pragma once

#include "thread_pool.hpp"


namespace details {

class Awaiter {
public:
    Awaiter() = default;
};

template <class T>
T operator|(AsyncResult<T> result, const Awaiter&) {
    return result.get();
}

};  // namespace details


template <class T, class Fun>
AsyncResult<std::invoke_result_t<Fun, T>> operator|(AsyncResult<T> async_res, Fun continuation) {
    return async_res.template then<std::invoke_result_t<Fun, T>>(continuation);
}

template <class Fun>
AsyncResult<std::invoke_result_t<Fun>>
operator|(ThreadPool& pool, Fun func) {
    return pool.submit<std::invoke_result_t<Fun>>(func);
}

static details::Awaiter await;