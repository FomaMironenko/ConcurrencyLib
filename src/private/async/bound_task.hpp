#include <memory>

#include "contract.hpp"
#include "task.hpp"


namespace details {

template <class Ret, class Arg>
class BoundAsyncTask : public ITaskBase {
static_assert(!std::is_same_v<Arg, void>, "BoundAsyncTask is only needed for non-void arguments");

public:
    BoundAsyncTask(FunctionType<Ret, Arg>&& func,
                   Promise<Ret>&& promise,
                   Arg&& arg);

    void run() override;

private:
    FunctionType<Ret, Arg> func_;
    Promise<Ret> promise_;
    Arg arg_;
};


// ======================================================== //
// ==================== IMPLEMENTATION ==================== //
// ======================================================== //


template <class Ret, class Arg>
BoundAsyncTask<Ret, Arg>::BoundAsyncTask(FunctionType<Ret, Arg>&& func,
                                         Promise<Ret>&& promise,
                                         Arg&& arg)
    : func_(std::move(func))
    , promise_(std::move(promise))
    , arg_(std::move(arg))
{   }


template <class Ret, class Arg>
void BoundAsyncTask<Ret, Arg>::run()
{
    try {
        if constexpr (std::is_same_v<Ret, void>) {
            func_(std::move(arg_));
            promise_.setValue(Void{});
        } else {
            Ret value = func_(std::move(arg_));
            promise_.setValue(std::move(value));
        }
    } catch (...) {
        promise_.setError(std::current_exception());
    }
}


template <class Ret, class Arg>
inline std::unique_ptr<BoundAsyncTask<Ret, Arg> >
make_bound_async_task(FunctionType<Ret, Arg>&& func, Promise<Ret>&& promise, Arg&& arg)
{
    return std::make_unique<BoundAsyncTask<Ret, Arg> >(std::move(func), std::move(promise), std::move(arg));
}

}  // namespace details
