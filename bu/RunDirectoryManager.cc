// #include <thread>
// #include <unordered_map>

#include "bu/RunDirectoryManager.h"

// #include "bu/RunDirectoryObserver.h"


namespace bu {

RunDirectoryManager::RunDirectoryManager(RunDirectoryRunner_t runner) : runner_(runner) {}

/*synchronized*/ RunDirectoryObserver& RunDirectoryManager::getRunDirectoryObserver(int runNumber)
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


/*synchronized*/ const std::string RunDirectoryManager::getStats()
{
    std::ostringstream os;
    os << "runNumbers=" << runDirectoryObservers_.size() << std::endl;
    {
        std::lock_guard<tools::synchronized::spinlock> lock(runDirectoryObserversLock_);
        for (const auto& pair : runDirectoryObservers_) {
            const bu::RunDirectoryObserver& observer = pair.second;
            os << observer.getStats();
        }
    }
    return os.str();
}

/*synchronized*/ const std::string RunDirectoryManager::getStats(int runNumber)
{
    bu::RunDirectoryObserver& observer = getRunDirectoryObserver( runNumber );
    return observer.getStats();
}


void RunDirectoryManager::startRunner(RunDirectoryObserver& observer) 
{
    assert( observer.state == RunDirectoryObserver::State::INIT );
    observer.runner = std::thread(runner_, std::ref(observer));
    observer.runner.detach();
    observer.state = RunDirectoryObserver::State::STARTING;
}


RunDirectoryObserver& RunDirectoryManager::createRunDirectoryObserver(int runNumber)
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

} // namespace bu