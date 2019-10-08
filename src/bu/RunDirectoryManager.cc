#include "tools/log.h"
#include "bu/RunDirectoryManager.h"


namespace bu {

/**************************************************************************
 * PUBLIC
 */


RunDirectoryManager::RunDirectoryManager() {}


std::tuple< FileInfo, RunDirectoryObserver::State, int > RunDirectoryManager::popRunFile(int runNumber, int stopLS)
{
    RunDirectoryObserverPtr observer = getRunDirectoryObserver( runNumber );
    return observer->popRunFile( stopLS );
}

/*
 * This function is not meant to run many times
 */
const std::string RunDirectoryManager::getStats() 
{
    std::ostringstream os;
    os << "runNumbers=" << runDirectoryObservers_.size() << '\n';
    {
        std::lock_guard<std::mutex> lock(runDirectoryManagerLock_);

        // Get the list of run numbers and sort them descending
        std::vector<int> runNumbers( runDirectoryObservers_.size() );
        std::transform(runDirectoryObservers_.begin(), runDirectoryObservers_.end(), runNumbers.begin(), [](auto pair){return pair.first;});
        std::sort( runNumbers.begin(), runNumbers.end(), std::greater<int>() );

        for (const int runNumber : runNumbers) {
            const auto iter = runDirectoryObservers_.find( runNumber );
            // Since we are running locked, we don't have to take the ownership of the shared pointer
            const RunDirectoryObserverPtr& observer = iter->second;
            os << observer->getStats();
        }
    }
    return os.str();
}


const std::string RunDirectoryManager::getStats(int runNumber) 
{
    RunDirectoryObserverPtr observer = getRunDirectoryObserver( runNumber );
    return observer->getStats();
}


const std::string& RunDirectoryManager::getError(int runNumber) {
    RunDirectoryObserverPtr observer = getRunDirectoryObserver( runNumber );
    return observer->getError();
}


void RunDirectoryManager::restartRunDirectoryObserver(int runNumber) 
{
    std::lock_guard<std::mutex> lock(runDirectoryManagerLock_);

    // FIXME: Just to allow restart, we remove any existing runObserver from the map. Obviously, this is creating a memory leak!!!
    if (runDirectoryObservers_.erase( runNumber ) > 0) {
        LOG(WARNING) << "runDirectoryObserver erased for runNumber: " << runNumber << ", this caused a resource leak! Use only for debugging!!!";
    }

    // TODO: The runDirectoryObserver has to be stop

    createRunDirectoryObserver_unlocked( runNumber );
}


/**************************************************************************
 * PRIVATE
 */


RunDirectoryObserverPtr RunDirectoryManager::getRunDirectoryObserver(int runNumber) 
{
    {
        std::lock_guard<std::mutex> lock(runDirectoryManagerLock_);

        // Check if we already have observer for that run
        const auto iter = runDirectoryObservers_.find( runNumber );
        if (iter != runDirectoryObservers_.end()) {
            return iter->second;
        }

        // No, we don't have. We create a new one
        return createRunDirectoryObserver_unlocked( runNumber );
    }
}


RunDirectoryObserverPtr RunDirectoryManager::createRunDirectoryObserver_unlocked(int runNumber)
{
    // Constructs a new runDirectoryObserver directly in the map directly
    auto emplaceResult = runDirectoryObservers_.emplace( runNumber, std::make_shared<RunDirectoryObserver>(runNumber) );

    // Normally, the runNumber was not in the map before, but better to be safe
    // It would be a fault to create a new observer if one already exists
    assert( emplaceResult.second == true );

    auto iter = emplaceResult.first;
    RunDirectoryObserverPtr observer = iter->second;
    
    LOG(DEBUG) << "runDirectoryObserver created for runNumber: " << iter->first;

    // Start the runner thread
    observer->start();

    return observer;
}


} // namespace bu