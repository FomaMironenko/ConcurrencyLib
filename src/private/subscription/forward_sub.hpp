#pragma once

#include "pipe_sub.hpp"


template <class T>
class ForwardSubscription : public PipeSubscription<T, T> {
public:
    ForwardSubscription(Promise<T> promise)
        : PipeSubscription<T, T>(std::move(promise)) {  }

    virtual void resolveValue(PhysicalType<T> value, ResolvedBy) override {
        promise_.setValue(std::move(value));
    }

private:
    using PipeSubscription<T, T>::promise_;
};
