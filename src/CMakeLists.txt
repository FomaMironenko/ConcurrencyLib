cmake_minimum_required(VERSION 3.7)

project (ConcurrencyLib)

add_library (
    ConcurrencyLib
    rand_gen.hpp
    table.hpp table.cpp
)
