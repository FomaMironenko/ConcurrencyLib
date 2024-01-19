#pragma once

#include "base_sub.hpp"
#include "utils/logger.hpp"


template <class T>
class SimpleSubscription : public ISubscription<T> {
public:

    explicit SimpleSubscription(ValueCallback<T> val_callback,
                                ErrorCallback err_callback = nullptr)
        : on_value_(std::move(val_callback))
        , on_error_(std::move(err_callback))
    {   }

    void resolveValue(T value, ResolvedBy) override;
    
    void resolveError(std::exception_ptr error, ResolvedBy) override;

private:
    ValueCallback<T> on_value_;
    ErrorCallback on_error_;
};



template <class T>
void SimpleSubscription<T>::resolveValue(T value, ResolvedBy) {
    on_value_(std::move(value));
}

template <class T>
void SimpleSubscription<T>::resolveError(std::exception_ptr error, ResolvedBy) {
    if (on_error_ == nullptr) {
        LOG_ERR << "Unhandled subscription exception";
        return;
    }
    on_error_(error);
}
