// #include <thread>
// #include <unordered_map>

#include "bu/RunDirectoryManager.h"

// #include "bu/RunDirectoryObserver.h"


namespace bu {

/**************************************************************************
 * PUBLIC
 */


RunDirectoryManager::RunDirectoryManager() {}

/*
 * Test whether we have to artificially force EOLS if stopLS is greater then the current lumiSection
 * or we have EoLS in the stopLS lumiSection.
 */
bool RunDirectoryManager::isStopLS(const RunDirectoryObserverPtr& observer, int stopLS) const
{
    if (stopLS < 0) 
        return false;
    
    if (!observer->queue.empty()) {
        const FileInfo& file = observer->queue.front();
        if  ( 
            ( (long)file.lumiSection == stopLS && file.type == FileInfo::FileType::EOLS ) ||
            ( (long)file.lumiSection > stopLS ) 
            ) { 
            return true;
        }    
    } else if (observer->runDirectory.lastEoLS >= stopLS) {
        return true;
    }

    return false;
}


std::tuple< FileInfo, RunDirectoryManager::RunDirectoryStatus > RunDirectoryManager::popRunFile(int runNumber, int stopLS)
{
    static const FileInfo emptyFile; 
    FileInfo file;
    RunDirectoryStatus run;

    RunDirectoryObserverPtr observer = getRunDirectoryObserver( runNumber );
    {    
        std::lock_guard<std::mutex> lock(observer->runDirectoryObserverLock);

        if (isStopLS(observer, stopLS)) {
            observer->stats.fu.stopLS = stopLS;
            run.state = RunDirectoryObserver::State::EOR;
            run.lastEoLS = stopLS;
            return std::tie( emptyFile, run );
        }

        while (!observer->queue.empty()) {
            file = std::move( observer->queue.front() );
            observer->updateStats( file, /*updateFU*/ true );
            observer->queue.pop();

            // Skip EoLS and EoR
            if (file.type == FileInfo::FileType::EOLS || file.type == FileInfo::FileType::EOR) {
                file.type = FileInfo::FileType::EMPTY;
                observer->stats.nbJsnFilesOptimized++; 
                continue;
            }
            break;
        }

        run.state = observer->runDirectory.state;
        run.lastEoLS = observer->runDirectory.lastEoLS;
    }
    return std::tie( file, run );
}


const std::string RunDirectoryManager::getStats() 
{
    int runNumbers = 0;
    std::ostringstream os;
    //os << "runNumbers=" << runDirectoryObservers_.size() << std::endl;
    {
        std::lock_guard<std::mutex> lock(runDirectoryManagerLock_);
        for (const auto& pair : runDirectoryObservers_) {
            // Since we are running locked, we don't have to take the ownership of the shared pointer
            const RunDirectoryObserverPtr& observer = pair.second;
            os << observer->getStats();
            runNumbers++;
        }
    }
    //return os.str();
    return "runNumbers=" + std::to_string(runNumbers) + '\n' + os.str();
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
     createRunDirectoryObserver( runNumber );
}


/**************************************************************************
 * PRIVATE
 */


RunDirectoryObserverPtr RunDirectoryManager::getRunDirectoryObserver(int runNumber) 
{
    {
        std::lock_guard<std::mutex> lock(runDirectoryManagerLock_);

        //Check if we already have observer for that run
        const auto iter = runDirectoryObservers_.find( runNumber );
        if (iter != runDirectoryObservers_.end()) {
            return iter->second;
        }
    }

    // No, we don't have. We create a new one
    return createRunDirectoryObserver( runNumber );
}


// TODO: Move to RunDirectoryObserver
void RunDirectoryManager::startRunner(const RunDirectoryObserverPtr& observer) const
{
    assert( observer->runDirectory.state == RunDirectoryObserver::State::INIT );

    observer->runner = std::thread(&RunDirectoryObserver::run, observer);
    observer->runner.detach();
    observer->runDirectory.state = RunDirectoryObserver::State::STARTING;
}


// FIXME
RunDirectoryObserverPtr RunDirectoryManager::createRunDirectoryObserver(int runNumber)
{
    std::unique_lock<std::mutex> lock(runDirectoryManagerLock_);

        // FIXME: Just to allow restart, we remove any existing runObserver from the map. Obviously, this is creating a memory leak!!!
        if (runDirectoryObservers_.erase( runNumber ) > 0) {
            std::cout << "DEBUG: runDirectoryObserver erased for runNumber: " << runNumber << std::endl;
        }

        // Constructs a new runDirectoryObserver directly in the map directly
        auto emplaceResult = runDirectoryObservers_.emplace( runNumber, std::make_shared<RunDirectoryObserver>(runNumber) );

        // Normally, the runNumber was not in the map before, but better be safe
        assert( emplaceResult.second == true );

        auto iter = emplaceResult.first;
        RunDirectoryObserverPtr observer = iter->second;
    
    lock.unlock();

    std::cout << "DEBUG: runDirectoryObserver created for runNumber: " << iter->first << std::endl;

    //TODO: observer.start();
    startRunner(observer);

    return observer;
}


} // namespace bu