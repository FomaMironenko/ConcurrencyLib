#pragma once

#include <memory>
#include <exception>

#include "base_sub.hpp"
#include "../type_traits.hpp"
#include "../async/group_state.hpp"


template <class T>
class JoinSubscription : public ISubscription<PhysicalType<T>> {
public:
    JoinSubscription(std::shared_ptr<details::GroupState<T> > state)
        : state_(std::move(state))
    {
        local_result_ = state_->attach();
    }

    void resolveValue([[maybe_unused]] PhysicalType<T> value, ResolvedBy) override {
        if constexpr (!std::is_same_v<T, void>) {
            local_result_->val.emplace(std::move(value));
        }
        state_->registerValue(local_result_);
        state_->detach();
        state_.reset();
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        local_result_->err = std::move(err);
        state_->registerError(local_result_);
        state_->detach();
        state_.reset();
    }

private:
    std::shared_ptr<details::GroupState<T> > state_;
    details::Result<T>* local_result_;
};
