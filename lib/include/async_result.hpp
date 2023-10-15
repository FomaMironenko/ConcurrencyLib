#pragma once

#include "contract.hpp"
#include "../private/void.hpp"

template <class T>
class AsyncResult {
private:
    explicit AsyncResult(Future<T> fut) : fut_(std::move(fut)) {    }

public:
    T get() {
        return fut_.get();
    }

private:
    Future<T> fut_;

friend class ThreadPool;
};


template <>
class AsyncResult<void> {
private:
    explicit AsyncResult(Future<Void> fut) : fut_(std::move(fut)) {    }

public:
    void get() {
        fut_.get();
    }

private:
    Future<Void> fut_;

friend class ThreadPool;
};

