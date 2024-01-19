#pragma once

#include "base_sub.hpp"


template <class Ret, class Arg>
class PipeSubscription : public ISubscription<PhysicalType<Arg> > {
public:
    PipeSubscription(Promise<Ret> promise)
        : promise_(std::move(promise)) {   }

    virtual void resolveError(std::exception_ptr err, ResolvedBy) override {
        promise_.setError(std::move(err));
    }

protected:
    Promise<Ret> promise_;
};
