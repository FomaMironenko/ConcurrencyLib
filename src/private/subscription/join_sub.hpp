#pragma once

#include <memory>
#include <exception>

#include "base_sub.hpp"
#include "../type_traits.hpp"
#include "../async/group_state.hpp"


namespace details {

template <class T>
class JoinSubscription : public ISubscription<PhysicalType<T>> {
public:
    JoinSubscription(std::shared_ptr<details::GroupState<T> > state);

    void resolveValue(PhysicalType<T> value, ResolvedBy) override;
    void resolveError(std::exception_ptr err, ResolvedBy) override;

private:
    std::shared_ptr<details::GroupState<T> > state_;
    details::Result<T>* local_result_;
};


// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //

template <class T>
JoinSubscription<T>::JoinSubscription(std::shared_ptr<details::GroupState<T> > state)
    : state_(std::move(state))
{
    local_result_ = state_->attach();
}

template <class T>
void JoinSubscription<T>::resolveValue([[maybe_unused]] PhysicalType<T> value, ResolvedBy)
{
    if constexpr (!std::is_same_v<T, void>) {
        local_result_->val.emplace(std::move(value));
    }
    state_->registerValue(local_result_);
    state_->detach();
    state_.reset();
}

template <class T>
void JoinSubscription<T>::resolveError(std::exception_ptr err, ResolvedBy)
{
    local_result_->err = std::move(err);
    state_->registerError(local_result_);
    state_->detach();
    state_.reset();
}

}  // namespace details
