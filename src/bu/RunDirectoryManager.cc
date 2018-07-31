// #include <thread>
// #include <unordered_map>

#include "tools/log.h"
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
        //const FileInfo& file = observer->queue.front();
        const FileInfo& file = observer->queue.top();
        if  ( 
            ( (long)file.lumiSection == stopLS && file.type == FileInfo::FileType::EOLS ) ||
            ( (long)file.lumiSection > stopLS ) 
            ) { 
            return true;
        }    
    } else if (observer->stats.fu.lastEoLS >= stopLS) {
        return true;
    }

    return false;
}

// TODO: Move this function to RunDirectoryObserver
std::tuple< FileInfo, RunDirectoryObserver::State, int > RunDirectoryManager::popRunFile(int runNumber, int stopLS)
{
    static const FileInfo emptyFile; 
    FileInfo file;  // Is empty on construction
    RunDirectoryObserver::State state;
    int lastEoLS;

    RunDirectoryObserverPtr observer = getRunDirectoryObserver( runNumber );
    {    
        std::lock_guard<std::mutex> lock(observer->runDirectoryObserverLock);

        observer->stats.fu.nbRequests++;

        if (isStopLS(observer, stopLS)) {
            observer->stats.fu.stopLS = stopLS;
            return std::make_tuple( emptyFile, RunDirectoryObserver::State::EOR, stopLS );
        }

        while (!observer->queue.empty()) {
            //file = std::move( observer->queue.front() );
            const FileInfo& peekFile = observer->queue.top();

            //LOG(DEBUG) << "peekFile=" << peekFile.fileName();

            // Consistency checks
            if (peekFile.type != FileInfo::FileType::EOR) {
                assert( (int)peekFile.lumiSection > observer->stats.fu.lastEoLS );

                // If we receive a file for a having larger LS than the expected, we keep it in the queue and wait for EoLS
                if ((int)peekFile.lumiSection > (observer->stats.fu.lastEoLS + 1)) {
                    observer->stats.fu.nbWaitsForEoLS++;
                    file.type = FileInfo::FileType::EMPTY;
                    break;
                }

            }
            
            //file = std::move( observer->queue.top() );
            file = std::move( peekFile );
            observer->queue.pop();

            // Consistency check
            if ( observer->stats.fu.lastPoppedFile.type != FileInfo::FileType::EMPTY &&
                 file.type != FileInfo::FileType::EOR && 
                 observer->stats.fu.lastPoppedFile.lumiSection > file.lumiSection ) 
            {
                std::ostringstream os;
                os  << "Consistency check failed, file order is broken:\n"
                    << "  Going to give file:            " << file.fileName() << '\n' 
                    << "  But the last file given to FU: " << observer->stats.fu.lastPoppedFile.fileName();
                LOG(FATAL) << os.str();
                THROW( std::runtime_error, os.str() );
            }

            observer->updateFUStats( file );

            // Is EoR then we can free the queue
            if (file.type == FileInfo::FileType::EOR) {
                // At this moment the queue has to be empty!
                assert( observer->queue.empty() );

                // C++11 allows us to move in a new empty queue, as a side effect the memory occupied by the previous is freed...  
                FileQueue_t tempQueue;
                observer->queue = std::move( tempQueue );
            }

            // Skip EoLS and EoR
            if (file.type == FileInfo::FileType::EOLS || file.type == FileInfo::FileType::EOR) {
                file.type = FileInfo::FileType::EMPTY;
                observer->stats.nbJsnFilesOptimized++; 
                continue;
            }
            break;
        }
        state = observer->stats.fu.state;
        lastEoLS = observer->stats.fu.lastEoLS;
    }

    if ( file.type == FileInfo::FileType::EMPTY ) {
        observer->stats.fu.nbEmptyReplies++; 
    }

    return std::make_tuple( file, state, lastEoLS );
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


// void RunDirectoryManager::setError(int runNumber, const std::string& errorMessage) {
//     RunDirectoryObserverPtr observer = getRunDirectoryObserver( runNumber );
//     observer->setError( errorMessage );
// }


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
    assert( observer->stats.run.state == RunDirectoryObserver::State::INIT );

    observer->runnerThread = std::thread(&RunDirectoryObserver::run, observer);
    observer->runnerThread.detach();
    observer->stats.run.state = RunDirectoryObserver::State::STARTING;
    observer->stats.fu.state = RunDirectoryObserver::State::STARTING;
}


// FIXME
RunDirectoryObserverPtr RunDirectoryManager::createRunDirectoryObserver(int runNumber)
{
    std::unique_lock<std::mutex> lock(runDirectoryManagerLock_);

        // FIXME: Just to allow restart, we remove any existing runObserver from the map. Obviously, this is creating a memory leak!!!
        if (runDirectoryObservers_.erase( runNumber ) > 0) {
            LOG(DEBUG) << "runDirectoryObserver erased for runNumber: " << runNumber;
        }

        // Constructs a new runDirectoryObserver directly in the map directly
        auto emplaceResult = runDirectoryObservers_.emplace( runNumber, std::make_shared<RunDirectoryObserver>(runNumber) );

        // Normally, the runNumber was not in the map before, but better be safe
        assert( emplaceResult.second == true );

        auto iter = emplaceResult.first;
        RunDirectoryObserverPtr observer = iter->second;
    
    lock.unlock();

    LOG(DEBUG) << "runDirectoryObserver created for runNumber: " << iter->first;

    //TODO: observer.start();
    startRunner(observer);

    return observer;
}


} // namespace bu