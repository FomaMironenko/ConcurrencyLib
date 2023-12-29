#pragma once

#include <string>
#include <iostream>
#include <atomic>
#include "logger.hpp"


inline std::atomic<size_t> __NUM_FAILS { 0 };
inline std::atomic<size_t> __NUM_CASES { 0 };
inline std::atomic<bool>   __SUCCESS { true };


// =======================================================
// ==================== TESTER KERNEL ====================
// =======================================================


#define DEFINE_TEST(function_name) \
void function_name()


#define RUN_TEST(function_name, test_name) \
__SUCCESS.store(true); \
function_name(); \
if (!__SUCCESS.load()) { \
    LOG_ERR  << "== ERR == " << test_name << "\n"; \
} else { \
    LOG_INFO << "== OK  == " << test_name << "\n"; \
} \


#define COMPLETE() \
if (__NUM_FAILS.load() > 0) { \
    LOG_INFO << "(*)"; \
    LOG_INFO << ">- ((  ASSERTIONS:  TOTAL [ " << __NUM_CASES.load() << " ]  FAILED [ " << __NUM_FAILS.load() << " ]"; \
    LOG_INFO << "(*)" << "\n"; \
    return EXIT_FAILURE; \
} \
LOG_INFO << "(*)"; \
LOG_INFO << " - ))  ASSERTIONS:  TOTAL [ " << __NUM_CASES.load() << " ]  FAILED [ " << __NUM_FAILS.load() << " ]"; \
LOG_INFO << "(*)" << "\n"; \
return EXIT_SUCCESS;


// =======================================================
// ==================== TESTER CHECKS ====================
// =======================================================


#define __FAIL \
__NUM_FAILS.fetch_add(1); \
__SUCCESS.store(false); \
return;


#define FAIL() \
__NUM_CASES.fetch_add(1); \
LOG_ERR << "Test case failed"; \
__FAIL


#define ASSERT(bool_val) \
__NUM_CASES.fetch_add(1); \
if ( !(bool_val) ) { \
    LOG_ERR << "Assertion failed: \"" << #bool_val << '"'; \
    __FAIL \
}


#define ASSERT_EQ(lhs, rhs) \
{ \
    __NUM_CASES.fetch_add(1); \
    auto tmp_lhs = lhs; \
    auto tmp_rhs = rhs; \
    if ( (tmp_lhs) != (tmp_rhs) ) { \
        LOG_ERR << "Assertion failed: \"" << #lhs << " == " << #rhs << "\"; with expansion " << tmp_lhs << " == " << tmp_rhs; \
        __FAIL \
    } \
}


#define ASSERT_INEQ(lhs, rhs) \
{ \
    __NUM_CASES.fetch_add(1); \
    auto tmp_lhs = lhs; \
    auto tmp_rhs = rhs; \
    if ( (tmp_lhs) == (tmp_rhs) ) { \
        LOG_ERR << "Assertion failed: \"" << #lhs << " != " << #rhs << "\"; with expansion " << tmp_lhs << " != " << tmp_rhs; \
        __FAIL \
    } \
}
