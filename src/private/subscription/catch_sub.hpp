#pragma once

#include <exception>
#include <functional>

#include "pipe_sub.hpp"
#include "../type_traits.hpp"


template <class T, class Err>
using ErrorHandler = std::function<T(const Err&)>;


template <class T, class Err>
class CatchSubscription : public PipeSubscription<T, T> {
public:
    explicit CatchSubscription(ErrorHandler<T, Err> handler, Promise<T> promise)
        : PipeSubscription<T, T>(std::move(promise))
        , handler_(std::move(handler)) {   }

    void resolveValue(PhysicalType<T> val, ResolvedBy) override {
        promise_.setValue(std::move(val));
    }

    void resolveError(std::exception_ptr err, ResolvedBy) override {
        try {
            std::rethrow_exception(err);
        } catch (const Err& err) {
            try {
                // Don't trust user handler
                if constexpr (std::is_same_v<T, void>) {
                    handler_(err);
                    promise_.setValue(Void{});
                } else {
                    T new_val = handler_(err);
                    promise_.setValue(std::move(new_val));
                }
            } catch (...) {
                promise_.setError(std::current_exception());
            }
        } catch (...) {
            promise_.setError(std::current_exception());
        }
    }

private:
    using PipeSubscription<T, T>::promise_;
    ErrorHandler<T, Err> handler_;
};
