#pragma once

#include <unordered_map>

#include "bu/RunDirectoryObserver.h"


namespace bu {

class RunDirectoryManager {
public:
    RunDirectoryManager();

    /*
     * Returns a tuple of:
     *   file, state, lastEoLS
     * 
     * TODO: Candidate for structured binding with std::optional in C++17
     */
    std::tuple< FileInfo, RunDirectoryObserver::State, int > popRunFile(int runNumber, int stopLS = -1);

    // Get statistics for all runs
    const std::string getStats();

    // Get statistics for a particular run
    const std::string getStats(int runNumber);

    // Return the error message for a particular run
    const std::string& getError(int runNumber);

    // FIXME: This will create a resource leak, use only for debugging
    void restartRunDirectoryObserver(int runNumber);

private:
    RunDirectoryObserverPtr getRunDirectoryObserver(int runNumber);
    RunDirectoryObserverPtr createRunDirectoryObserver_unlocked(int runNumber);

private:
    // Maps runNumbers to RunDirectoryObservers
    std::unordered_map< int, RunDirectoryObserverPtr > runDirectoryObservers_;

    // Would be better to use shared_mutex (i.e. read lock), but for the moment there is only one reader, so the mutex or spinlock is the best
    std::mutex runDirectoryManagerLock_;    
};

} // namespace bu
