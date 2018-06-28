#pragma once

#include <thread>

#include "tools/synchronized/queue.h"
#include "bu/FileInfo.h"


namespace bu {

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

    //#include <boost/lockfree/queue.hpp>
    //boost::lockfree::queue<bu::FileInfo> queue_test;

    // Changed from std::string to bu::FileInfo
    //typedef tools::synchronized::queue<std::string> FileNameQueue_t;
    typedef tools::synchronized::queue<bu::FileInfo> FileQueue_t;


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
        FileQueue_t queue;

        std::atomic<State> state { State::INIT };
        std::atomic<bool> running { true };
        std::thread runner;

        //TODO
        struct Statistics {
            struct Inotify {
                uint32_t nbAllFiles = 0;                    // Number of all files INotify saw
                uint32_t nbJsnFiles = 0;                    // Number of proper .jsn files (after applying a filter to all files)
                uint32_t nbJsnFilesDuplicated = 0;          // Number of duplicated .jsn files received from Inotify
            };

            struct Startup {
                uint32_t nbJsnFiles = 0;                    // Number of proper .jsn files seen in run directory when observer was started
                Inotify inotify;                            // Inotify statistics during observer start
            } startup;
    
            Inotify inotify;                                // Inotify statistics during observer run

            uint32_t nbJsnFilesQueued = 0;                  // Number of all .jsn files put into the queue

            uint32_t lastEoLS = 0;                          // When lastEoLS == 0 then there was no jsn files found (yet)
            uint32_t lastIndex = 0;
            bool isEoR = false;
        } stats;
    };

} // namespace bu


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
