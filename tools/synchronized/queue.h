#ifndef SYNCHRONIZED_QUEUE_H
#define SYNCHRONIZED_QUEUE_H

#include <deque>
#include <mutex>

#include "spinlock.h"

namespace tools {
    namespace synchronized {
        /*
        * Simple synchronized non-blocking queue based on std::dequeue.
        * It is using spinlock for locking, but can be changed though template.
        */
        template< 
            typename T,
            class Mutex = tools::synchronized::spinlock
        > class queue {

        public:
            queue() = default;
            ~queue() = default;

            queue(const queue&) = delete;
            queue& operator=(const queue&) = delete;

            // Clears the deque
            void clear( void )
            {
                std::lock_guard<Mutex> lock(mutex_);
                queue_.clear();
            }
            
            /*
            * It is using move semantics.
            * Use as queue.push( std::move(your_data) )
            */
            bool push(T&& data) {
                std::lock_guard<Mutex> lock(mutex_);
                return unsynchronized_push( std::move(data) );
            }

            bool pop(T& data) {
                std::lock_guard<Mutex> lock(mutex_);
                return unsynchronized_pop( data );
            }

            // Forcing people to use move semantics 
            bool unsynchronized_push(T&& data) {
                queue_.push_back( std::move(data) );
                return true;
            }

            bool unsynchronized_pop(T& data) {
                if ( queue_.empty() ) {
                    return false;
                } else {
                    data = std::move( queue_.front() );
                    queue_.pop_front();
                    return true;
                }
            }

        private:
            std::deque<T> queue_;
            Mutex mutex_;
        };
    }
}

#endif // SYNCHRONIZED_QUEUE_H
