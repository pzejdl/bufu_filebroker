#pragma once

#include <iostream>

#include "tools/exception.h"    // For TOOLS_DEBUG_INFO
/*
 * Simple logging macros, replace later with boost when we have a version with logging...
 */

//#include <boost/log/trivial.hpp>

enum LOG_LEVEL {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

//#define LOG(severity)   ( tools::log::debug() << "[0x" << std::hex << std::this_thread::get_id() << std::dec << "] " << severity << " [" TOOLS_DEBUG_INFO ", " << __PRETTY_FUNCTION__ << "]: " ) 
#define LOG(severity)   ( tools::log::log() << "[0x" << std::hex << std::this_thread::get_id() << std::dec << "] " << severity << __func__ << ": " ) 


namespace tools {
namespace log {


std::ostream& operator<< (std::ostream& os, enum LOG_LEVEL severity)
{
    switch (severity)
    {
        case TRACE:     return os << "TRACE   " ;
        case DEBUG:     return os << "DEBUG   ";
        case INFO:      return os << "INFO    ";
        case WARNING:   return os << "WARNING ";
        case ERROR:     return os << "ERROR   ";
        case FATAL:     return os << "FATAL   ";
    };
    return os;
}


// From: https://stackoverflow.com/questions/2179623/how-does-qdebug-stuff-add-a-newline-automatically/2179782#2179782

static std::mutex lock;

struct log {
    log() {
    }

    ~log() {
        std::lock_guard<std::mutex> guard(lock);
        std::cerr << os.str() << std::endl;
    }

public:
    // accepts just about anything
    template<class T>
    log& operator<<(const T& x) {
        os << x;
        return *this;
    }
private:
    std::ostringstream os;
};


} // namespace log
} // namespace tools
