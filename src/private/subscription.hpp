#pragma once

#include <exception>
#include <functional>

#include "utils/logger.hpp"


enum class ResolvedBy { kProducer, kConsumer };

template <class T>
using ValueCallback = std::function<void(T)>;
using ErrorCallback = std::function<void(std::exception_ptr)>;


template <class T>
class ISubscription {
public:

    virtual void resolveValue(T value, ResolvedBy by) = 0;

    virtual void resolveError(std::exception_ptr error, ResolvedBy by) = 0;

    virtual ~ISubscription() = default;

};

template <class T>
using SubscriptionPtr = std::unique_ptr< ISubscription<T> >;



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
