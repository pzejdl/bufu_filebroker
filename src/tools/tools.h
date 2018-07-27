#pragma once

/*
 * Functions that are described here are (more options can be selected):
 *   - Usefull
 *   - Don't have a better place (yet)
 *   - Are too hacky 
 *   - Are waiting to be properly implemented in C++
 */


#include <unistd.h>
#include <sys/syscall.h>

//#include <iostream>
/*
 * Priniting thread TID for debugging
 * TODO: Replace + with a variadic template function for arguments merging
 */
//#define THREAD_DEBUG() (printf("Thread ID = %u, function: '%s'\n", tools::gettid(), __PRETTY_FUNCTION__))
#define TOOLS_THREAD_INFO() ((std::string)("Thread ID = " + std::to_string(tools::gettid()) + ", function: '" + __PRETTY_FUNCTION__))


#include <chrono>

namespace tools {



/*
 * Return current thread ID (TID)
 */
inline pid_t gettid (void)
{
    return syscall(__NR_gettid);
}


/*
 * This will time execution of function F.
 * Return value: Number of seconds in double.
 */
template<
/*    typename timeType = std::chrono::microseconds, */
    typename F, typename... Args
>
/*typename timeType::rep*/ double timeFunction(F func, Args&&... args){
    auto start = std::chrono::high_resolution_clock::now();

    func(std::forward<Args>(args)...);

    auto duration = std::chrono::duration_cast</*timeType*/std::chrono::microseconds>(std::chrono::system_clock::now() - start);

    /*return duration.count();*/
    return duration.count() / 1000000.0d;
}

} // namespace tools
