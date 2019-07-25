#pragma once

/*
 * Time/Clock conversion functions
 * 
 * Function here requires C++14
 * 
 * Source: https://embeddedartistry.com/blog/2019/1/31/converting-between-timespec-amp-stdchrono
 */

#include <chrono>

using namespace std::chrono; // for example brevity

namespace tools {
namespace time {

/**************************************************************************
 * Conversion functions
 * 
 * Source: https://embeddedartistry.com/blog/2019/1/31/converting-between-timespec-amp-stdchrono
 */

/*
 * timespec to std::chrono::duration
 */
constexpr nanoseconds timespecToDuration(timespec ts)
{
    auto duration = seconds{ts.tv_sec} 
        + nanoseconds{ts.tv_nsec};

    return duration_cast<nanoseconds>(duration);
}

/*
 * std::chrono::duration to timespec
 */
#if __cplusplus > 201402L
    // C++17
    constexpr
#else
    // C++14
    inline
#endif
 timespec durationToTimespec(nanoseconds dur)
{
    auto secs = duration_cast<seconds>(dur);
    dur -= secs;

    return timespec{secs.count(), dur.count()};
}

/*
 * timespec to std::chrono::timepoint
 */
constexpr time_point<system_clock, nanoseconds> timespecToTimePoint(timespec ts)
{
    return time_point<system_clock, nanoseconds>{
        duration_cast<system_clock::duration>(timespecToDuration(ts))};
}

/*
 * std::chrono::time_point to timespec
 */
constexpr timespec timepointToTimespec(time_point<system_clock, nanoseconds> tp)
{
    auto secs = time_point_cast<seconds>(tp);
    auto ns = time_point_cast<nanoseconds>(tp) -
              time_point_cast<nanoseconds>(secs);

    return timespec{secs.time_since_epoch().count(), ns.count()};
}

/**************************************************************************
 * Various useful functions
 */

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


/*
 * Returns a string representation of local time with nanoseconds precision
 */
inline std::string localtime()
{
    // Get current time
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    
    // Convert to std::time_t type (Epoch time)
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    
    // Thread safe convert to local time
    struct tm now_local;
    localtime_r( &now_time_t, &now_local );

    // Convert to string representation
    std::ostringstream ss;
    ss << std::put_time(&now_local, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(9) << std::setfill('0') << tools::time::timepointToTimespec(now).tv_nsec;
    
    return ss.str();
}

} // namespace time
} // namespace tools
