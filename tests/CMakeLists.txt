cmake_minimum_required(VERSION 3.7)

project (ConcurrencyLibTests)

add_executable(ParallelSumTest
    parallel_sum.cpp
)
target_link_libraries (
    ParallelSumTest
    ConcurrencyLib
)

