#pragma once

/*
 * Functions that are described here are (more options can be selected):
 *   - Usefull
 *   - Don't have a better place (yet)
 *   - Are too hacky 
 *   - Are waiting to be properly implemented in C++
 */

#include <chrono>
#include <iomanip>      // std::put_time
#include <sstream>

#include <unistd.h>
#include <sys/syscall.h>

//#include <iostream>
/*
 * Priniting thread TID for debugging
 * TODO: Replace + with a variadic template function for arguments merging
 */
//#define THREAD_DEBUG() (printf("Thread ID = %u, function: '%s'\n", tools::gettid(), __PRETTY_FUNCTION__))
#define TOOLS_THREAD_INFO() ((std::string)("Thread ID = " + std::to_string(tools::gettid()) + ", function: '" + __PRETTY_FUNCTION__))


namespace tools {

/*
 * Return current thread ID (TID)
 */
inline pid_t gettid (void)
{
    return syscall(__NR_gettid);
}

} // namespace tools
