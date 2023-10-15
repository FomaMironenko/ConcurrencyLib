#pragma once

#include "logger.hpp"
#include "string"
#include "iostream"


#define ASSERT(bool_val) \
if ( !(bool_val) ) { \
    LOG_ERR << "Assertion failed: \"" << #bool_val << '"'; \
    return false; \
}


#define ASSERT_EQ(lhs, rhs) \
{ \
    auto tmp_lhs = lhs; \
    auto tmp_rhs = rhs; \
    if ( (tmp_lhs) != (tmp_rhs) ) { \
        LOG_ERR << "Assertion failed: \"" << #lhs << " == " << #rhs << "\"; with expansion " << tmp_lhs << " == " << tmp_rhs; \
        return false; \
    } \
}


#define TEST(bool_func, test_name) \
if (!bool_func()) { \
    std::cout << "!! ERR !! " << test_name << "\n"; \
} else { \
    std::cout << "== OK === " << test_name << "\n"; \
}
