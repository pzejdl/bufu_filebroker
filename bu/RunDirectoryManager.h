#pragma once

#include <unordered_map>
//#include <forward_list>

#include "bu/RunDirectoryObserver.h"


namespace bu {

class RunDirectoryManager {
public:

    struct RunDirectoryStatus {
        RunDirectoryObserver::State state;
        int lastEoLS;
    };

    // struct RunFileInfo {
    //     FileInfo fileInfo;
    //      struct RunDirectory {
    //         RunDirectoryObserver::State state;
    //         int lastEoLS;
    //         int lastIndex;
    //      } runDirectory;
    // };
    // Will return FileInfo and RunDirectoryInfo 

public:
    RunDirectoryManager();

    //TODO: Candidate for structured binding with std::optional in C++17
    std::tuple< FileInfo, RunDirectoryStatus > popRunFile(int runNumber);

    // Get statistics for all runs
    const std::string getStats();

    // Get statistics for a particular run
    const std::string getStats(int runNumber);

    // Return the error message for a particular run
    const std::string& getError(int runNumber);

    // FIXME: This will create a resource leak
    void restartRunDirectoryObserver(int runNumber);

private:
    RunDirectoryObserverPtr getRunDirectoryObserver(int runNumber);
    RunDirectoryObserverPtr createRunDirectoryObserver(int runNumber);
    void startRunner(const RunDirectoryObserverPtr& observer) const;

private:
    std::unordered_map< int, RunDirectoryObserverPtr > runDirectoryObservers_;

    //TODO: faster would be to use forward_list, but check if we can add new elements (with locking) and do iteration without locking...
    //std::forward_list< std::pair< int, RunDirectoryObserverPtr > > runDirectoryObservers_;

    // Would be better to use shared_mutex, but for the moment there is only one reader, so the spinlock is the best
    std::mutex runDirectoryManagerLock_;    
};

} // namespace bu
