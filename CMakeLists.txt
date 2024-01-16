cmake_minimum_required(VERSION 3.7)

project (ConcurrencyProj LANGUAGES CXX)
set (CMAKE_CXX_STANDARD 17)


if (WIN32)
    # Strict compile checks for Visual Studio compiler
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -WX -W3")
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -WX -W3")
else ()
    # Strict compile checks
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -pedantic -Wextra")
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -pedantic -Wextra")
endif()


# Sanitizer builds configuration: available builds are:
# Asan, Tsan, UBsan, AsanWithUBsan, TsanWithUBsan

# Common flags
if (CMAKE_BUILD_TYPE MATCHES "^(Asan|Tsan|UBsan|AsanWithUBsan|TsanWithUBsan)$")

    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fno-omit-frame-pointer")
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fno-omit-frame-pointer")

endif()


# Undefined Behavior sanitizer flags
if (CMAKE_BUILD_TYPE MATCHES "^(UBsan|AsanWithUBsan|TsanWithUBsan)$")

    message(STATUS "Building with UndefinedBehavior Sanitizer enabled")
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")

endif()


# Thread sanitizer flags
if (CMAKE_BUILD_TYPE MATCHES "^(Tsan|TsanWithUBsan)$")

    if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.4)
        message(FATAL_ERROR, "GCC versions older than GCC 8.4 are not supported")
    endif()

    message(STATUS "Building with Thread Sanitizer enabled")
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")

endif()


# Address sanitizer flags
if (CMAKE_BUILD_TYPE MATCHES "^(Asan|AsanWithUBsan)$")

    message(STATUS "Building with Address Sanitizer enabled")
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")

    if (APPLE)
        message(WARNING "Leak sanitizer is not supported by Darwin architecture")
    else()

        SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
        SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")

    endif()
endif()



include_directories(src/include)

add_subdirectory(src)
add_subdirectory(tests)
