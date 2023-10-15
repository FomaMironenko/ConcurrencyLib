cmake_minimum_required(VERSION 3.7)

project (ConcurrencyLib)

include_directories(include private)

add_library (
    ConcurrencyLib
    "src/table.cpp"
    "src/thread_pool.cpp"
)
