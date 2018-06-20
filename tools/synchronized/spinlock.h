#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <unistd.h>

/*
 * IMPORTANT NOTE: 
 *   Don't use BOOST boost::detail::spinlock. It may cause a deadlock (tested in v1.53.0 and g++ 4.8.5) when used in function using move semantics.
 *   Reproduce with test_queue.cc and boost spin lock. 
 * 
 * TODO:
 *   boost::detail::spinlock may scale better, always check performance with test_spinlock.cc.
 *   #include <boost/smart_ptr/detail/spinlock.hpp>
 * 
 * On Intel Sandy Bridge:
 *   <= 2 threads: boost spinlock performs better
 *   >  2 threads: this spinlock performs better
 */


#include <atomic>

namespace tools {
    namespace synchronized {

        /*
        * spinlock implementation using C++ atomic operations
        * Sources: 
        *   http://en.cppreference.com/w/cpp/atomic/atomic_flag
        *   https://www.boost.org/doc/libs/1_67_0/doc/html/atomic/usage_examples.html
        */
        class spinlock {
        private:
            std::atomic_flag locked_ = ATOMIC_FLAG_INIT;

        public:
            spinlock() noexcept = default;
            ~spinlock() = default;

            spinlock(const spinlock&) = delete;
            spinlock& operator=(const spinlock&) = delete;
            
            void lock() noexcept {
                while (locked_.test_and_set(std::memory_order_acquire)) { 
                    /* busy-wait
                     * Don't put empty body here, otherwise the spinlock will be very very slow during lock contention...
                     * 
                     * Notes for improvement:
                     *   - An exponential back-off should be implemented
                     *   - If lock is very contented then do std::this_thread::yield()
                     */
                    usleep(0);
                }
            }
            void unlock() noexcept {
                locked_.clear(std::memory_order_release);
            }
        };

    }
}

#endif // SPINLOCK_H
