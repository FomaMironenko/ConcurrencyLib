#pragma once

#include <memory>

#include "future.hpp"
#include "promise.hpp"


template <class T>
struct Contract {
    Promise<T> producer;
    Future<T> consumer;
};

template <class T>
Contract<T> contract()
{
    auto state = std::make_shared<details::SharedState<PhysicalType<T> > >();
    return {Promise<T>{state}, Future<T>{state}};
}
