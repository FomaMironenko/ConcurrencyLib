cmake_minimum_required(VERSION 3.7)

project (ConcurrencyProj LANGUAGES CXX)
set (CMAKE_CXX_STANDARD 17)

include_directories(lib/include)

add_subdirectory(lib)
add_subdirectory(tests)


# Sanitizer builds configuration: available builds are:
# Asan, Tsan, UBsan, AsanWithUBsan, TsanWithUBsan

if(CMAKE_BUILD_TYPE MATCHES "^(Asan|Tsan|UBsan|AsanWithUBsan|TsanWithUBsan)$")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fno-omit-frame-pointer")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fno-omit-frame-pointer")
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(UBsan|AsanWithUBsan|TsanWithUBsan)$")
    message(STATUS "Building with UndefinedBehavior Sanitizer enabled")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Tsan|TsanWithUBsan)$")
    if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.4)
        # For older gcc versions prebuilt shared libraries need to be compiled with -fPIC flag in order to use Tsan
        # See https://github.com/google/sanitizers/issues/701#issuecomment-235534987
        # You can try to manyally set up tsan by commenting this check out and recompiling rxdeps-native with -fPIC
        message(FATAL_ERROR, "GCC versions older than GCC 8.4 are not supported")
    endif()
    message(STATUS "Building with Thread Sanitizer enabled")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Asan|AsanWithUBsan)$")
    message(STATUS "Building with Address Sanitizer enabled")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-recover=address")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-recover=address")
    if(APPLE)
        message(WARNING "Leak sanitizer is not supported by Darwin architecture")
    else()
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
    endif()
endif()
