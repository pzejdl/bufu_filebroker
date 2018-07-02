#pragma once

#include <unordered_map>

#include "bu/RunDirectoryObserver.h"


namespace bu {

typedef std::function<void(RunDirectoryObserver&)> RunDirectoryRunner_t;

class RunDirectoryManager {
public:

    struct RunDirectoryStatus {
        RunDirectoryObserver::State state;
        int lastEoLS;
        int lastIndex;
        bool isEoR;
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
    RunDirectoryManager(RunDirectoryRunner_t runner);

    //TODO: Candidate for structured binding with std::optional in C++17
    std::tuple< FileInfo, RunDirectoryStatus > popRunFile(int runNumber);

    // Get statistics for all runs
    const std::string getStats();

    // Get statistics for a particular run
    const std::string getStats(int runNumber);

    // FIXME: This will create a resource leak
    void restartRunDirectoryObserver(int runNumber);

private:
    RunDirectoryObserver& getRunDirectoryObserver(int runNumber);
    RunDirectoryObserver& createRunDirectoryObserver(int runNumber);
    void startRunner(RunDirectoryObserver& observer) const;

private:
    RunDirectoryRunner_t runner_;

    std::unordered_map< int, RunDirectoryObserver > runDirectoryObservers_;

    // Would be better to use shared_mutex, but for the moment there is only one reader, so the spinlock is the best
    std::mutex runDirectoryManagerLock_;    
};

} // namespace bu
