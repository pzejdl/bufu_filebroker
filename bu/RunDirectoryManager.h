#pragma once

#include <unordered_map>

#include "bu/RunDirectoryObserver.h"


namespace bu {

typedef std::function<void(RunDirectoryObserver&)> RunDirectoryRunner_t;

class RunDirectoryManager {

public:
    RunDirectoryManager(RunDirectoryRunner_t runner);

    /*synchronized*/ RunDirectoryObserver& getRunDirectoryObserver(int runNumber);

    /*synchronized*/ const std::string getStats();
    /*synchronized*/ const std::string getStats(int runNumber);

private:
    RunDirectoryObserver& createRunDirectoryObserver(int runNumber);
    void startRunner(RunDirectoryObserver& observer);

private:
    RunDirectoryRunner_t runner_;

    std::unordered_map< int, RunDirectoryObserver > runDirectoryObservers_;

    // Would be better to use shared_mutex, but for the moment there is only one reader, so the spinlock is the best
    tools::synchronized::spinlock runDirectoryObserversLock_;    
};

} // namespace bu
