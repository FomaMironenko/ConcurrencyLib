#pragma once

#include <future>
#include <exception>

#include "base_sub.hpp"
#include "../type_traits.hpp"


namespace details {

template <class T>
class ToStdSubscription : public ISubscription<PhysicalType<T> > {
public:
    explicit ToStdSubscription(std::promise<T> std_promise)
        : std_promise_(std::move(std_promise)) {   }

    void resolveValue(PhysicalType<T> val, ResolvedBy) override;
    void resolveError(std::exception_ptr err, ResolvedBy) override;

private:
    std::promise<T> std_promise_;
};


// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //

template <class T>
void ToStdSubscription<T>::resolveValue([[maybe_unused]] PhysicalType<T> val, ResolvedBy)
{
    if constexpr (std::is_same_v<T, void>) {
        std_promise_.set_value();
    } else {
        std_promise_.set_value(std::move(val));
    }
}

template <class T>
void ToStdSubscription<T>::resolveError(std::exception_ptr err, ResolvedBy)
{
    std_promise_.set_exception(err);
}

}  // namespace details
