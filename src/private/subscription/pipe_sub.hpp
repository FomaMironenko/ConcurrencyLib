#pragma once

#include "../core/promise.hpp"
#include "base_sub.hpp"


namespace details {

template <class Ret, class Arg>
class PipeSubscription : public ISubscription<PhysicalType<Arg> > {
public:
    PipeSubscription(Promise<Ret> promise): promise_(std::move(promise)) {   }

    virtual void resolveError(std::exception_ptr err, ResolvedBy) override;

protected:
    Promise<Ret> promise_;
};


template <class Ret, class Arg>
void PipeSubscription<Ret, Arg>::resolveError(std::exception_ptr err, ResolvedBy)
{
    promise_.setError(std::move(err));
}

}  // namespace details
