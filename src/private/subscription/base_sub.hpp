#pragma once

#include <exception>
#include <functional>


template <class T>
using ValueCallback = std::function<void(T)>;
using ErrorCallback = std::function<void(std::exception_ptr)>;


namespace details {

enum class ResolvedBy { kProducer, kConsumer };

template <class T>
class ISubscription {
public:

    virtual void resolveValue(T value, ResolvedBy by) = 0;
    virtual void resolveError(std::exception_ptr error, ResolvedBy by) = 0;

    virtual ~ISubscription() = default;

};


template <class T>
using SubscriptionPtr = std::unique_ptr<ISubscription<T> >;

}  // namespace details
