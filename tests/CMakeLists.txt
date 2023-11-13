cmake_minimum_required(VERSION 3.7)

project (ConcurrencyLibTests)

add_executable(ParallelSumTest
    parallel_sum_test.cpp
)
target_link_libraries (
    ParallelSumTest
    ConcurrencyLib
)

add_executable(ContractTest
    contract_test.cpp
)
target_link_libraries (
    ContractTest
    ConcurrencyLib
)

add_executable(ThreadPoolTest
    thread_pool_test.cpp
)
target_link_libraries (
    ThreadPoolTest
    ConcurrencyLib
)

add_executable(TaskGroupTest
    task_group_test.cpp
)
target_link_libraries (
    TaskGroupTest
    ConcurrencyLib
)

add_executable(LinearEquationsTest
    linear_equations_test.cpp
)
target_link_libraries (
    LinearEquationsTest
    ConcurrencyLib
)

add_executable(SortTest
    sort_test.cpp
)
target_link_libraries (
    SortTest
    ConcurrencyLib
)
