#include <type_traits>

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


template <class T>
struct async_type { };

template <class T>
struct async_type< AsyncResult<T> > {
    using type = T;
};
