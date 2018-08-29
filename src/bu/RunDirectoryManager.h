#pragma once

#include <unordered_map>
//#include <forward_list>

#include "bu/RunDirectoryObserver.h"


namespace bu {

class RunDirectoryManager {
public:

    // struct RunFileInfo {
    //     FileInfo file;
    //     RunDirectoryObserver::State state;
    //     int lastEoLS;
    // };

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
    void startRunner(const RunDirectoryObserverPtr& observer) const;
    bool isStopLS(const RunDirectoryObserverPtr& observer, int stopLS) const;

private:
    std::unordered_map< int, RunDirectoryObserverPtr > runDirectoryObservers_;

    //TODO: faster would be to use forward_list, but check if we can add new elements (with locking) and do iteration without locking...
    //std::forward_list< std::pair< int, RunDirectoryObserverPtr > > runDirectoryObservers_;

    // Would be better to use shared_mutex, but for the moment there is only one reader, so the spinlock is the best
    std::mutex runDirectoryManagerLock_;    
};

} // namespace bu
