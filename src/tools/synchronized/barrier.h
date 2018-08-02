#ifndef BARRIER_H
#define BARRIER_H

#include <atomic>

/*
 * Compiler only barriers
 * 
 * From: Is Parallel Programming Hard, And, If So, What Can You Do About It? (2010) by Paul E. McKenney
 * 
 * NOTE: Use only when you know exactly what are you doing! Or read the book... 
 */

/*
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#define READ_ONCE(x) ACCESS_ONCE(x)
#define WRITE_ONCE(x, val) ({ ACCESS_ONCE(x) = (val); })
#define barrier() __asm__ __volatile__("": : :"memory")
*/

/*
 * This is a replacement with C++11 alternatives
 * NOTE: Requires variable to be std::atomic type
 */

#define READ_ONCE(x)       ( x.load(std::memory_order_relaxed) )
#define WRITE_ONCE(x, val) ( x.store(val, std::memory_order_relaxed) )

#endif // BARRIER_H
