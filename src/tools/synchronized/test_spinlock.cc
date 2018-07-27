
#include <future>
#include <iostream>
#include <vector>
#include <cassert>

#include "boost/smart_ptr/detail/spinlock.hpp"

#include "spinlock.h"
#include "../tools.h"


// Synchronize std::cout
//tools::synchronized::spinlock outLock;

volatile int value = 0;

template<typename Mutex>
int loop(int index, bool inc, int limit)
{
    (void)index;
    static Mutex lock;

    //outLock.lock();
    //std::cout << "Started " << index << ": " << inc << " " << limit << std::endl;
    //outLock.unlock();

    for (int i = 0; i < limit; ++i) {
        std::lock_guard<Mutex> guard(lock);
        if (inc) {
            ++value;
        } else {
            --value;
        }
    }
    return 0;
}


template<typename Mutex>
void test_lock()
{
    value = 0;
    /*
    auto f0 = std::async(std::launch::async, std::bind( loop<Mutex>, 0, true, 10000000));
    auto f1 = std::async(std::launch::async, std::bind( loop<Mutex>, 1, true, 10000000));
    auto f2 = std::async(std::launch::async, std::bind( loop<Mutex>, 2, false, 10000000));

    f0.wait();
    f1.wait();
    f2.wait();
    */

    std::vector<std::thread> threads;

    for (int i=1; i<=10; ++i) {
        threads.push_back( std::thread( loop<Mutex>, i, (i <= 6 ? true : false), 1000000 ) );
    }

    for (auto& th : threads) th.join();

    //std::cout << value << std::endl;
    assert( value == 2000000 );
}


int main()
{
    std::cout << "Test with custom spinlock: " << tools::timeFunction( test_lock<tools::synchronized::spinlock> ) << std::endl;
    std::cout << "Test with boost spinlock:  " << tools::timeFunction( test_lock<boost::detail::spinlock> ) << std::endl;
    std::cout << "Test with mutex:           " << tools::timeFunction( test_lock<std::mutex> ) << std::endl;

    std::cout << "Test with custom spinlock: " << tools::timeFunction( test_lock<tools::synchronized::spinlock> ) << std::endl;
    std::cout << "Test with boost spinlock:  " << tools::timeFunction( test_lock<boost::detail::spinlock> ) << std::endl;
    std::cout << "Test with mutex:           " << tools::timeFunction( test_lock<std::mutex> ) << std::endl;   
}
