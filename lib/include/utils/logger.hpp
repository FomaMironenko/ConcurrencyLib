#pragma once

#include <thread>
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <string.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


class SimpleLogger {
public:
    SimpleLogger(const std::string& prefix, const char* file, int line) {
        ss_ << "TID-" << std::this_thread::get_id() << " [" << prefix << "]: "
            << file << ":" << line << " - ";
    }

    ~SimpleLogger() {
        ss_ << "\n";
        std::cout << ss_.str();
    }

    template <class T>
    SimpleLogger& operator<<(const T& value) {
        ss_ << value;
        return *this;
    }

    template <class T>
    SimpleLogger& operator<<(const std::vector<T>& vec) {
        ss_ << "[";
        for (const T& val : vec) {
            ss_ << val << ", ";
        }
        ss_ << "]";
        return *this;
    }

private:
    std::stringstream ss_;
};

#define LOG_INFO SimpleLogger("INF", __FILENAME__, __LINE__) 
#define LOG_ERR  SimpleLogger("ERR", __FILENAME__, __LINE__) 
#define LOG_WARN SimpleLogger("WRN", __FILENAME__, __LINE__) 
