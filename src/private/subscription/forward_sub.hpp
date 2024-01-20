#pragma once

#include "pipe_sub.hpp"


namespace details {

template <class T>
class ForwardSubscription : public PipeSubscription<T, T> {
public:
    ForwardSubscription(Promise<T> promise) : PipeSubscription<T, T>(std::move(promise)) {  }

    virtual void resolveValue(PhysicalType<T> value, ResolvedBy) override;

private:
    using PipeSubscription<T, T>::promise_;
};


template <class T>
void ForwardSubscription<T>::resolveValue(PhysicalType<T> value, ResolvedBy)
{
    promise_.setValue(std::move(value));
}

}  // namespace details
