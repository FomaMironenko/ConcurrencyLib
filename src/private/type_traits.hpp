#pragma once

#include <type_traits>


struct Void {};

// ============================================================== //
// ==================== Convert void to Void ==================== //
// ============================================================== //

template <class T>
using PhysicalType = typename std::conditional_t<
    std::is_same_v<T, void>,
    Void, T
>;


// ================================================================== //
// ==================== std::function<Ret(void)> ==================== //
// ================================================================== //

template <class Ret, class Arg>
struct Function {
    using type = std::function<Ret(Arg)>;
};

template <class Ret>
struct Function<Ret, void> {
    using type = std::function<Ret()>;
};

template <class Ret, class Arg>
using FunctionType = typename Function<Ret, Arg>::type;


// =============================================================== //
// ==================== Check for AsyncResult ==================== //
// =============================================================== //

// FWD declaration
template <class T>
class AsyncResult;

template <class T>
struct is_async_result {
    static constexpr bool value = false;
};

template <class T>
struct is_async_result< AsyncResult<T> > {
    static constexpr bool value = true;
};


// ========================================================================= //
// ==================== Get underlying AsyncResult type ==================== //
// ========================================================================= //

template <class T>
struct async_type { };

template <class T>
struct async_type< AsyncResult<T> > {
    using type = T;
};
