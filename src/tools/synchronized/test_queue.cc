#include <string>
#include <thread>
#include <iostream>
#include <mutex>
#include <atomic>

#include <boost/smart_ptr/detail/spinlock.hpp>
#include "queue.h"

tools::synchronized::queue<std::string> queue;

std::atomic<bool> done(false);

void consumer()
{
    std::string message;
    int messages = 0;
    while (!done) {
        if (!queue.pop(message)) {
            std::cout << "Consumer thread: EMPTY" << std::endl;
            std::this_thread::sleep_for(std::chrono::microseconds(50000));
        } else {
            messages++;
            std::cout << "Consumer thread: " << message << std::endl;
        }
    }
    std::cout << "Consumer finished" << std::endl;
}

#include "../noisy.h"

typedef tools::noisy::string string;
//typedef std::string string;

void test_move()
{
    //NOTE: With boost::detail::spinlock the queue will deadlock!
    //tools::synchronized::queue<string, boost::detail::spinlock> queue;
    tools::synchronized::queue<string> queue;
    
    string a = "word1";
    string b = "word2";

    queue.push( std::move(a) );
    queue.push( std::move(b) );

    queue.pop( b );
    queue.pop( a );

    std::cout << "string a: " << a << std::endl;
    std::cout << "string b: " << b << std::endl;
}


int main(int, char **)
{
    test_move();

    for (int i = 0; i < 100; ++i){
        queue.push("Message from main thread: " + std::to_string(i) );
    }

    std::thread t( consumer );

    std::cout << "Main Thread: " << 1 << " DONE" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    done = true;
    t.join();
    return 0;
}
