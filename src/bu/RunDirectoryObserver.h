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
//typedef std::queue<bu::FileInfo> FileQueue_t;


// TODO: We can make a queue, internally having multiple queues based on lumisection numbers, that would perform better than priority_queue
typedef std::priority_queue< bu::FileInfo, std::vector<bu::FileInfo>, std::greater<bu::FileInfo> > FileQueue_t;


class RunDirectoryObserver {
public:
    // Note that this class cannot be copied or moved because of the queue

    /* 
     * File mode defines what files we are expecting in the run directory
     *   JSN - we expect *.jsn files
     *   RAW   - we expect *.raw files with a binary header (describing the same information previously present in .jsn file)
     */
    enum class FileMode { JSN, RAW };
    friend std::ostream& operator<< (std::ostream& os, const RunDirectoryObserver::FileMode fileMode);

    /*
     * State defines current state of directory observer
     *   INIT     - Initial state before inotify thread is started, this state is not visible outside.
     *   STARTING - Inotify thread was started, run direcotry is being scanned for files.
     *   READY    - Waiting for new files.
     *   EOLS     - EoLS file was found.
     *   EOR      - EoR file was found.
     *   ERROR    - Any error detected. This state is non recoverable.
     *   NORUN    - Special error case, when run directory doesn't exist. This state is non recoverable. 
     */
    enum class State { INIT, STARTING, READY, EOLS, EOR, ERROR, NORUN };
    friend std::ostream& operator<< (std::ostream& os, const RunDirectoryObserver::State state);

    RunDirectoryObserver(int runNumber/*, FileMode fileMode*/);
    ~RunDirectoryObserver();

    RunDirectoryObserver(const RunDirectoryObserver&) = delete;
    RunDirectoryObserver& operator=(const RunDirectoryObserver&) = delete;
    
    std::string getStats() const;
    const std::string& getError() const;

    // Start inotify thread
    void start();
    void stopAndWait();
    /*
     * Returns a tuple of:
     *   file, state, lastEoLS
     * 
     * TODO: Candidate for structured binding with std::optional in C++17
     */
    std::tuple< FileInfo, RunDirectoryObserver::State, int > popRunFile(int stopLS = -1);

private:
    bool isStopLS(int stopLS) const;
    // The main runner that will call inotifyRunner()
    void runner();
    void inotifyRunner();
    void pushFile(bu::FileInfo file);
    void updateRunDirectoryStats(const bu::FileInfo& file);
    void updateFUStats(const bu::FileInfo& file);
    void optimizeAndPushFiles(const files_t& files);

private:
    int runNumber;
    FileQueue_t queue;

    // This error message is valid only if the state is ERROR
    std::string errorMessage;

    std::thread runnerThread;
    std::atomic<bool> isRunning { false };
    std::atomic<bool> stopRequest { false };

    struct Statistics {
        struct Inotify {
            uint32_t nbInotifyReadCalls = 0;            // Number of read calls executed for reading inotify events 
            uint32_t nbAllFiles = 0;                    // Number of all files inotify saw
            uint32_t nbJsnFiles = 0;                    // Number of proper .jsn files (after applying a filter to all files)
            uint32_t nbJsnFilesDuplicated = 0;          // Number of duplicated .jsn files received from inotify
        };

        struct Startup {
            uint32_t nbJsnFiles = 0;                    // Number of proper .jsn files seen in run directory when observer was started
            uint32_t nbJsnFilesOptimized = 0;           // Number of .jsn files skipped during optimizations
            Inotify inotify;                            // Inotify statistics during observer start
        } startup;

        Inotify inotify;                                // Inotify statistics during observer run

        uint32_t nbJsnFilesProcessed = 0;               // Number of all .jsn files put into the queue
        uint32_t nbJsnFilesOptimized = 0;

        struct RunDirectory {
            //TODO: HACK: RAW file mode is hardcoded for the moment 
            FileMode fileMode {FileMode::RAW};          // Which files to expect (INDEX or RAW)
            State state { State::INIT };
            int nbOutOfOrderIndexFiles = 0;             // How many index files were received out of order (lower LS number after higher LS number)
            FileInfo lastProcessedFile;                 // Last processed file
            int lastEoLS = 0;                           // Last EoLS seen in the run directory (the next expected is 1)
        } run;

        uint32_t queueSizeMax = 0;                      // The largest queue size ever seen

        struct FU {
            State state { State::INIT };
            int nbRequests = 0;                         // How many requests we got from FUs
            int nbEmptyReplies = 0;                     // How many times we had no index file to return
            int nbWaitsForEoLS = 0;                     // How many FU requests were postponed because we received 
            FileInfo lastPoppedFile;                    // Last file given to FU
            int lastEoLS = 0;                           // Last EoLS FU saw (the next expected is 1)
            // TODO: The following counter should be counted per FU (maybe)
            int stopLS = -1;                            // Remembers is stopLS was specified in the request from FU
        } fu;
    } stats;

    std::mutex runDirectoryObserverLock;                // Synchronize updates
};

typedef std::shared_ptr<RunDirectoryObserver> RunDirectoryObserverPtr;

} // namespace bu
