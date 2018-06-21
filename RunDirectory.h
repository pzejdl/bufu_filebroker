#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <cassert>

#include "tools/synchronized/queue.h"


/* 
 * The main queue is here.
 * 
 * Actually, we need a SPMS (single producer - multiple consumers) queue,
 * but because I'm lazy and before I find a better solution I use a simple wrapper
 * of std::dequeue. 
 * 
 * Notes about boost: Unfortunately, boost::lockfree::queue cannot be used
 * because it requires trivial assignment operator and trivial destructor.
 * And that std::string doesn't have.
 *
 * TODO: check this: http://moodycamel.com/blog/2014/detailed-design-of-a-lock-free-queue
 */
typedef tools::synchronized::queue<std::string> FileNameQueue_t;


struct RunDirectoryObserver {
    // Note that this class cannot be copied or moved because of queue

    enum class State { INIT, STARTING, READY, STOP };

    friend std::ostream& operator<< (std::ostream& os, RunDirectoryObserver::State state)
    {
        switch (state)
        {
            case RunDirectoryObserver::State::INIT:       return os << "INIT" ;
            case RunDirectoryObserver::State::STARTING:   return os << "STARTING";
            case RunDirectoryObserver::State::READY:      return os << "READY";
            case RunDirectoryObserver::State::STOP:       return os << "STOP";
            // Omit default case to trigger compiler warning for missing cases
        };
        return os;
    }


    RunDirectoryObserver(int runNumber) : runNumber(runNumber) {}

    int runNumber;
    FileNameQueue_t queue;

    std::atomic<State> state { State::INIT };
    std::atomic<bool> running { true };
    std::thread runner;

    //TODO
    struct Statistics {
        std::atomic<std::uint32_t> indexSeenOnBU {0};
        std::atomic<std::uint32_t> indexGivenToFU {0};
        // When lastEoLS == 0 then there was no jsn files found
        std::atomic<std::uint32_t> lastEoLS {0};
        std::atomic<bool> isEoR {false};
    } stats;
};



typedef std::function<void(RunDirectoryObserver&)> RunDirectoryRunner_t;


class RunDirectoryManager {

public:
    RunDirectoryManager(RunDirectoryRunner_t runner) : runner_(runner) {}

public:
    RunDirectoryObserver& getRunDirectoryObserver(int runNumber)
    {
        {
            std::lock_guard<tools::synchronized::spinlock> lock(runDirectoryObserversLock_);

            // Check if we already have observer for that run
            const auto iter = runDirectoryObservers_.find( runNumber );
            if (iter != runDirectoryObservers_.end()) {
                return iter->second;
            }
        }

        // No, we don't have. We create a new one
        return createRunDirectoryObserver( runNumber );
    }

private:

    void startRunner(RunDirectoryObserver& observer) 
    {
        assert( observer.state == RunDirectoryObserver::State::INIT );
        observer.runner = std::thread(runner_, std::ref(observer));
        observer.runner.detach();
        observer.state = RunDirectoryObserver::State::STARTING;
    }


    RunDirectoryObserver& createRunDirectoryObserver(int runNumber)
    {
        std::unique_lock<tools::synchronized::spinlock> lock(runDirectoryObserversLock_);
        //{
            // Constructs a new runDirectoryObserver in the map directly
            auto emplaceResult = runDirectoryObservers_.emplace(std::piecewise_construct,
                std::forward_as_tuple(runNumber), 
                std::forward_as_tuple(runNumber)
            );
            // Normally, the runNumber was not in the map before, but better be safe
            assert( emplaceResult.second == true );

            auto iter = emplaceResult.first;
            RunDirectoryObserver& observer = iter->second;
        //}
        lock.unlock();

        std::cout << "DEBUG: runDirectoryObserver created for runNumber: " << iter->first << std::endl;

        //TODO: observer.start();
        startRunner(observer);

        return observer;
    }


private:
    RunDirectoryRunner_t runner_;

    std::unordered_map< int, RunDirectoryObserver > runDirectoryObservers_;

    // Would be better to use shared_mutex, but for the moment there is only one reader, so the spinlock is the best
    tools::synchronized::spinlock runDirectoryObserversLock_;    
};


/*
class RunDirectoryObserver {

public:    

    RunDirectoryObserver(int runNumber) : runNumber_(runNumber) {}
    ~RunDirectoryObserver()
    {
        std::cout << "DEBUG: RunDirectoryObserver: Destructor called" << std::endl;
        running_ = false;
        if (runner_.joinable()) {
            runner_.join();
        }
    }

    RunDirectoryObserver(const RunDirectoryObserver&) = delete;
    RunDirectoryObserver& operator=(const RunDirectoryObserver&) = delete;


    void start() 
    {
        assert( state_ == State::INIT );
        runner_ = std::thread(&RunDirectoryObserver::run, this);
        runner_.detach();
        state_ = State::STARTING;
    }


    void stopRequest()
    {
        running_ = false;    
    }


    State state() const 
    {
        return state_;
    }

    FileNameQueue_t& queue()
    {
        return queue_;
    }

private:

    void run()
    {
        while (running_) {
            std::cout << "RunDirectoryObserver: Alive" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        std::cout << "RunDirectoryObserver: Finished" << std::endl;
        state_ = State::STOP;
    }

private:
    std::atomic<State> state_ { State::INIT };
    std::atomic<bool> running_{ true };

    int runNumber_;
    std::thread runner_ {};

    FileNameQueue_t queue_;
};
*/
