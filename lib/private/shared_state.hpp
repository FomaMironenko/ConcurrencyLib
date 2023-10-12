#pragma once

#include <optional>
#include <exception>

#include <mutex>
#include <condition_variable>


// Forward declare
template <class T>
class StateProducer;
template <class T>
class StateConsumer;
template <class T>
struct Contract;


namespace details {

template <class T>
struct SharedState {
private:
    std::mutex mtx_;
    std::condition_variable cv_;

    std::optional<T> value_ = std::nullopt;
    std::exception_ptr error_ = nullptr;

    bool produced_ = false;
    bool consumed_ = false;

friend class StateProducer<T>;
friend class StateConsumer<T>;
};

}  // namespace details
