cmake_minimum_required(VERSION 3.7)

project (ConcurrencyProj LANGUAGES CXX)
set (CMAKE_CXX_STANDARD 17)

include_directories(lib/include)

add_subdirectory(lib)
add_subdirectory(tests)
