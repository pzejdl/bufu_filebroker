#pragma once

#include <thread>
#include <queue>

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
//typedef tools::synchronized::queue<bu::FileInfo> FileQueue_t;
typedef std::queue<bu::FileInfo> FileQueue_t;


struct RunDirectoryObserver {
    // Note that this class cannot be copied or moved because of queue

    enum class State { INIT, STARTING, READY, /*EOLS, EOR,*/ STOP, ERROR };

    friend std::ostream& operator<< (std::ostream& os, const RunDirectoryObserver::State state);

    RunDirectoryObserver(int runNumber);

    std::string getStats() const;


    int runNumber;
    FileQueue_t queue;

    //std::atomic<State> state { State::INIT };
    std::atomic<bool> running { true };
    std::thread runner;

    struct RunDirectory {
        std::atomic<State> state { State::INIT };
        int lastEoLS = -1;
        int lastIndex = -1;
        bool isEoR = false;
    } runDirectory;

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

    } stats;

    std::mutex runDirectoryObserverLock;                // Synchronize updates
};

} // namespace bu
