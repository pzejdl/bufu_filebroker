#pragma once

#include <thread>
#include <queue>
#include <atomic>
#include <mutex>

//#include "tools/synchronized/queue.h"
#include "bu/FileInfo.h"
#include "bu.h"


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

    enum class State { INIT, STARTING, READY, EOLS, EOR, STOP, ERROR };
    friend std::ostream& operator<< (std::ostream& os, const RunDirectoryObserver::State state);


    RunDirectoryObserver(int runNumber);
    ~RunDirectoryObserver();


    // RunDirectoryObserver(const RunDirectoryObserver&) = delete;
    // RunDirectoryObserver& operator=(const RunDirectoryObserver&) = delete;
    

    std::string getStats() const;

    const std::string& getError() const;

    void run();

    void main();

    void pushFile(bu::FileInfo file);
    void updateStats(const bu::FileInfo& file);
    void optimizeAndPushFiles(const files_t& files);


    int runNumber;
    FileQueue_t queue;

    // This error message is valid only if state is ERROR
    std::string errorMessage;

    //std::atomic<State> state { State::INIT };
    std::atomic<bool> running { true };
    std::thread runner;

    struct RunDirectory {
        std::atomic<State> state { State::INIT };
        int lastEoLS = -1;
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
            uint32_t nbJsnFilesOptimized = 0;           // Number of .jsn files skipped during optimizations
            Inotify inotify;                            // Inotify statistics during observer start
        } startup;

        Inotify inotify;                                // Inotify statistics during observer run

        uint32_t nbJsnFilesProcessed = 0;               // Number of all .jsn files put into the queue
        uint32_t nbJsnFilesOptimized = 0;

        uint32_t queueSizeMax = 0;                      // The largest queue size ever seen

        FileInfo fuLastPoppedFile;                       // Last file given to FU
    } stats;

    std::mutex runDirectoryObserverLock;                // Synchronize updates
};

typedef std::shared_ptr<RunDirectoryObserver> RunDirectoryObserverPtr;

} // namespace bu
