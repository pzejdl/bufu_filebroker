#include <thread>

#include "tools/synchronized/queue.h"
#include "tools/synchronized/barrier.h"
#include "tools/inotify/INotify.h"
#include "tools/tools.h"
#include "tools/log.h"
#include "bu/FileInfo.h"
#include "bu/RunDirectoryObserver.h"


namespace bu {

RunDirectoryObserver::RunDirectoryObserver(int runNumber) : runNumber(runNumber) {}


RunDirectoryObserver::~RunDirectoryObserver()
{
    LOG(DEBUG) << "RunDirectoryObserver::~RunDirectoryObserver().";
}


std::string RunDirectoryObserver::getStats() const
{
    const char *sep = "  ";
    std::ostringstream os;

    os << "runNumber="                                      << runNumber << '\n';
    os << sep << "startup.nbJsnFiles="                      << stats.startup.nbJsnFiles << '\n';
    os << sep << "startup.nbJsnFilesOptimized="             << stats.startup.nbJsnFilesOptimized << '\n';
    os << sep << "startup.inotify.nbInotifyReadCalls="      << stats.startup.inotify.nbInotifyReadCalls << '\n';
    os << sep << "startup.inotify.nbAllFiles="              << stats.startup.inotify.nbAllFiles << '\n';
    os << sep << "startup.inotify.nbJsnFiles="              << stats.startup.inotify.nbJsnFiles << '\n';
    os << sep << "startup.inotify.nbJsnFilesDuplicated="    << stats.startup.inotify.nbJsnFilesDuplicated << '\n';
    os << '\n';
    os << sep << "inotify.nbInotifyReadCalls="              << stats.inotify.nbInotifyReadCalls << '\n';
    os << sep << "inotify.nbAllFiles="                      << stats.inotify.nbAllFiles << '\n';
    os << sep << "inotify.nbJsnFiles="                      << stats.inotify.nbJsnFiles << '\n';
    os << '\n';
    os << sep << "nbJsnFilesProcessed="                     << stats.nbJsnFilesProcessed << '\n';
    os << sep << "nbJsnFilesOptimized="                     << stats.nbJsnFilesOptimized << '\n';
    os << '\n';
    os << sep << "errorMessage=\""                          << errorMessage << "\"\n";
    os << '\n';
    os << sep << "run.fileMode="                            << stats.run.fileMode << '\n';
    os << sep << "run.state="                               << stats.run.state << '\n';
    os << sep << "run.nbOutOfOrderIndexFiles="              << stats.run.nbOutOfOrderIndexFiles << '\n';
    os << sep << "run.lastProcessedFile=\""                 << stats.run.lastProcessedFile.fileName() << "\"\n";
    os << sep << "run.lastEoLS="                            << stats.run.lastEoLS << '\n';
    os << '\n';
    os << sep << "queueSizeMax="                            << stats.queueSizeMax << '\n';
    // NOTE: queue.size() is not protected by lock or memory barrier, therefore we will get a number that is not up-to-date, but it doesn't matter
    os << sep << "queueSize="                               << queue.size() << '\n';
    os << '\n';
    os << sep << "fu.state="                                << stats.fu.state << '\n';
    os << sep << "fu.nbRequests="                           << stats.fu.nbRequests << '\n';
    os << sep << "fu.nbEmptyReplies="                       << stats.fu.nbEmptyReplies  << '\n';
    os << sep << "fu.nbWaitsForEoLS="                       << stats.fu.nbWaitsForEoLS << '\n';
    os << sep << "fu.lastPoppedFile=\""                     << stats.fu.lastPoppedFile.fileName() << "\"\n";
    os << sep << "fu.lastEoLS="                             << stats.fu.lastEoLS << '\n';
    os << sep << "fu.stopLS="                               << stats.fu.stopLS << '\n';
    os << '\n';

    return os.str();
}


const std::string& RunDirectoryObserver::getError() const
{
    return errorMessage;
}


/**************************************************************************
 * PRIVATE
 */


void RunDirectoryObserver::pushFile(bu::FileInfo file)
{
    std::lock_guard<std::mutex> lock(runDirectoryObserverLock);
    queue.push( std::move(file) );
    stats.nbJsnFilesProcessed++;
    uint32_t size = queue.size();
    if (size > stats.queueSizeMax) {
        stats.queueSizeMax = size;
    }
}


template< class T >
void updateStats(int runNumber, const bu::FileInfo& file, T& s)
{
    // Sanity check. In principle always OK.
    assert( (uint32_t)runNumber == file.runNumber );
    assert( file.type != FileInfo::FileType::EMPTY );

    if (file.isEoLS()) {
        s.lastEoLS = file.lumiSection;
        s.state = bu::RunDirectoryObserver::State::EOLS;
    } else if (file.isEoR()) {
        s.state = bu::RunDirectoryObserver::State::EOR;
    } else {
        assert( file.type == bu::FileInfo::FileType::INDEX );
        s.state = bu::RunDirectoryObserver::State::READY;
    }    
}


void RunDirectoryObserver::updateRunDirectoryStats(const bu::FileInfo& file)
{
    updateStats(runNumber, file, stats.run);
    stats.run.lastProcessedFile = file;
}


void RunDirectoryObserver::updateFUStats(const bu::FileInfo& file)
{
    updateStats(runNumber, file, stats.fu);
    stats.fu.lastPoppedFile = file;
}


// Skip empty lumisections
void RunDirectoryObserver::optimizeAndPushFiles(const bu::files_t& files) 
{
    bool sawIndexFile = false;

    for (auto&& file : files) {

        // Skip EoLS and EoR, but only if we didn't see any index file yet
        if (!sawIndexFile && (file.type == bu::FileInfo::FileType::EOLS || file.type == bu::FileInfo::FileType::EOR)) {
            // We can update statistics only for files that we skip here
            // Later, the statistics is updated when FU asks for a file 
            stats.startup.nbJsnFilesOptimized++; 
            updateRunDirectoryStats( file );
    
            // If we are skipping files, we have to update FU lastEoLS statistics here so it can be correctly reported when FU asks for a file for the first time
            stats.fu.lastEoLS = stats.run.lastEoLS;
            continue;
        }
        sawIndexFile = true;

        updateRunDirectoryStats( file );
        pushFile( std::move(file) );
    }
}


void RunDirectoryObserver::runner()
{
    try {
        inotifyRunner();
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Exception detected." );
    } 
}


/*
 * The main function responsible for finding files on BU.
 * Is run in a separate thread for each run directory that FU asks. 
 */
void RunDirectoryObserver::inotifyRunner()
{
    LOG(INFO) << TOOLS_THREAD_INFO();
    WRITE_ONCE(isRunning, true);

    /*
     * .jsn file filter definition
     *  Examples:
     *    run1000030354_ls0000_EoR.jsn
     *    run1000030354_ls0017_EoLS.jsn
     *    run1000030348_ls0511_index020607.jsn
     *    run1000030348_ls0511_index020607.raw
     * 
     * NOTE: std::regex is broken until gcc 4.9.0. For that compiler one has to use boost::regex
     */
<<<<<<< HEAD
    //const std::regex fileFilter( "run[0-9]+_ls[0-9]+_.*\\.jsn" );
=======
    static const std::regex fileFilter( "run[0-9]+_ls[0-9]+_.*\\.jsn" );
>>>>>>> develop

    //static const boost::regex fileFilter( "run[0-9]+_ls[0-9]+_.*\\.jsn" );

    //TODO: HACK: Allow RAW files for the moment
    static const boost::regex fileFilter( "run[0-9]+_ls[0-9]+_(EoR.jsn|EoLS.jsn|.*\\.raw)" );


    const std::string runDirectoryPath = bu::getRunDirectory( runNumber ).string(); 
    LOG(INFO) << "DirectoryObserver: Watching: " << runDirectoryPath;

    /**************************************************************************
     * PHASE I - Startup: Inotify is started and the run directory is searched for existing .jsn files
     */

    // INotify has to be set before we list the directory content, otherwise we have a race condition...
    tools::INotify inotify;
    try {
        inotify.add_watch( runDirectoryPath, IN_CLOSE_WRITE | IN_MOVED_TO );
    }
    catch(const std::system_error& e) {
        if (e.code().value() == static_cast<int>(std::errc::no_such_file_or_directory)) {
            // Special handling for a case when the run directory doesn't exists (Srecko's request)
            stats.run.state = bu::RunDirectoryObserver::State::NORUN;
            stats.fu.state = bu::RunDirectoryObserver::State::NORUN;
        } else {
            stats.run.state = bu::RunDirectoryObserver::State::ERROR;
            stats.fu.state = bu::RunDirectoryObserver::State::ERROR;
        }
        errorMessage = e.what();
        LOG(ERROR) << "DirectoryObserver: ERROR: \"" << errorMessage << "\", error code: " << e.code();
        LOG(INFO)  << "DirectoryObserver: Finished";
        return;
    }
    LOG(DEBUG) << "DirectoryObserver: INotify started.";

    //sleep(5);

    // List files in the run directory
    bu::files_t files = bu::listFilesInRunDirectory( runDirectoryPath, fileFilter );
    stats.startup.nbJsnFiles = files.size();
    LOG(DEBUG) << "DirectoryObserver: Found " << files.size() << " files in run directory.";

    //sleep(5);

    // Now, we have to read the first batch events from INotify. 
    // And we have to make sure they are not the same we obtained in listing the run directory before.
    while ( inotify.hasEvent() ) {
        stats.startup.inotify.nbInotifyReadCalls++;
        LOG(DEBUG) << "DirectoryObserver: INotify has something.";
        
        for (auto&& event : inotify.read()) {
            stats.startup.inotify.nbAllFiles++;

            if ( std::regex_match( event.name, fileFilter) ) {
                stats.startup.inotify.nbJsnFiles++;

                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );

                // Add files that are not duplicates
                if ( std::find(files.cbegin(), files.cend(), file) == files.cend() ) {
                    files.push_back( std::move( file ));
                } else {
                    stats.startup.inotify.nbJsnFilesDuplicated++;
                    LOG(DEBUG) << "Duplicates from inotify: \"" << file.fileName() << '\"';
                }
            }
        }
    }

    // Sort the files according LS and INDEX numbers
    std::sort(files.begin(), files.end());

    // LOG(DEBUG) << "Dumping the content of the queue:";
    // for (const auto& file: files) {
    //     LOG(DEBUG) << file.fileName();
    // }

    /**************************************************************************
     * PHASE II - Optimize: Determine the first usable .jsn file (and skip empty lumisections)
     */

    optimizeAndPushFiles(files);

    // FUs can start reading from our queue NOW

    if ( queue.empty() ) {
        // If the queue is empty then FU state is the same like the run directory state
        stats.fu.state = stats.run.state;
    } else {
        // If we have some files in the queue then we are ready for requests
        stats.fu.state = bu::RunDirectoryObserver::State::READY;
    }

    LOG(DEBUG) 
        << "DirectoryObserver statistics:\n" 
        << getStats();

    /**************************************************************************
     * PHASE III - The main loop: Now, we rely on the Inotify
     */


    // Process any new file receives through INotify
    while ( ! READ_ONCE(stopRequest) ) {

        for (auto&& event : inotify.read()) {
            stats.inotify.nbAllFiles++;

            //TODO: Make it optional
            //LOG(DEBUG) << "INOTIFY: '" << event.name << '\'';

            if ( std::regex_match( event.name, fileFilter) ) {
                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );
                //LOG(DEBUG) << file.fileName();

                stats.inotify.nbJsnFiles++;

                // Count out of order index files
                if ( 
                    stats.run.lastProcessedFile.lumiSection > file.lumiSection &&
                    stats.run.lastProcessedFile.type == FileInfo::FileType::INDEX &&
                    file.type == FileInfo::FileType::INDEX 
                ) {
                    stats.run.nbOutOfOrderIndexFiles++;
                }

                updateRunDirectoryStats( file );
                pushFile( std::move(file) );
            }
        }
        stats.inotify.nbInotifyReadCalls++;

        // If we get EOR then we don't expect any new files to appear and we can stop this thread
        if ( stats.run.state == bu::RunDirectoryObserver::State::EOR ) {
            break;
        }
    }

    // Hola, finito!
    LOG(INFO) << "DirectoryObserver: Finished";
    LOG(DEBUG) 
        << "DirectoryObserver statistics:\n" 
        << getStats();

    WRITE_ONCE(isRunning, false);
}


/*
 * Test whether we have to artificially force EOLS if stopLS is greater then the current lumiSection
 * or we have EoLS in the stopLS lumiSection.
 */
bool RunDirectoryObserver::isStopLS(int stopLS) const
{
    if (stopLS < 0) 
        return false;
    
    if (!queue.empty()) {
        const FileInfo& file = queue.top();
        if  ( 
            ( (long)file.lumiSection == stopLS && file.type == FileInfo::FileType::EOLS ) ||
            ( (long)file.lumiSection > stopLS ) 
            ) { 
            return true;
        }    
    } else if (stats.fu.lastEoLS >= stopLS) {
        return true;
    }

    return false;
}


/**************************************************************************
 * PUBLIC
 */


// Starts the inotify thread
void RunDirectoryObserver::start()
{
    assert( stats.run.state == RunDirectoryObserver::State::INIT );

    runnerThread = std::thread(&RunDirectoryObserver::runner, this);
    // FIXME: We detach because at the moment we don't have a way how to stop the thread
    runnerThread.detach();
    stats.run.state = RunDirectoryObserver::State::STARTING;
    stats.fu.state = RunDirectoryObserver::State::STARTING;
}


void RunDirectoryObserver::stopAndWait()
{
    WRITE_ONCE(stopRequest, true);

    LOG(DEBUG) << "Waiting for InotifyRunner to stop";
    if (runnerThread.joinable()) {
        runnerThread.join();
    }
    // FIXME: Better is to use mutex, the thread will sleep automaticaly during wait...
    while ( ! READ_ONCE(isRunning) ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    };
}


std::tuple< FileInfo, RunDirectoryObserver::State, int > RunDirectoryObserver::popRunFile(int stopLS)
{
    static const FileInfo emptyFile; 
    FileInfo file;  // Is empty on construction
    RunDirectoryObserver::State state;
    int lastEoLS;

    std::lock_guard<std::mutex> lock(runDirectoryObserverLock);

    stats.fu.nbRequests++;

    if (isStopLS(stopLS)) {
        stats.fu.stopLS = stopLS;
        return std::make_tuple( emptyFile, RunDirectoryObserver::State::EOR, stopLS );
    }

    while (!queue.empty()) {
        const FileInfo& peekFile = queue.top();

        //LOG(DEBUG) << "peekFile=" << peekFile.fileName();

        // Check for the out of order files
        if (peekFile.type != FileInfo::FileType::EOR) {
            // Consistency checks
            assert( (int)peekFile.lumiSection > stats.fu.lastEoLS );

            // If we receive a file for a having larger LS than the expected, we keep it in the queue and wait for EoLS
            if ((int)peekFile.lumiSection > (stats.fu.lastEoLS + 1)) {
                stats.fu.nbWaitsForEoLS++;
                file.type = FileInfo::FileType::EMPTY;
                break;
            }
        }
        
        file = std::move( peekFile );
        queue.pop();

        // Consistency check
        if ( 
            stats.fu.lastPoppedFile.lumiSection > file.lumiSection &&
            stats.fu.lastPoppedFile.type != FileInfo::FileType::EMPTY &&
            file.type != FileInfo::FileType::EOR
        ) {
            std::ostringstream os;
            os  << "Consistency check failed, file order is broken:\n"
                << "  Going to give file:            " << file.fileName() << '\n' 
                << "  But the last file given to FU: " << stats.fu.lastPoppedFile.fileName();
            LOG(FATAL) << os.str();
            THROW( std::runtime_error, os.str() );
        }

        updateFUStats( file );

        // Is EoR then we can free the queue
        if (file.type == FileInfo::FileType::EOR) {
            // At this moment the queue has to be empty!
            assert( queue.empty() );

            // C++11 allows us to move in a new empty queue, as a side effect the memory occupied by the previous is freed...  
            FileQueue_t tempQueue;
            queue = std::move( tempQueue );
        }

        // Skip EoLS and EoR
        if (file.type == FileInfo::FileType::EOLS || file.type == FileInfo::FileType::EOR) {
            file.type = FileInfo::FileType::EMPTY;
            stats.nbJsnFilesOptimized++; 
            continue;
        }
        break;
    }
    state = stats.fu.state;
    lastEoLS = stats.fu.lastEoLS;
    

    if ( file.type == FileInfo::FileType::EMPTY ) {
        stats.fu.nbEmptyReplies++; 
    }

    return std::make_tuple( file, state, lastEoLS );
}


/**************************************************************************
 * FRIENDS
 */


std::ostream& operator<< (std::ostream& os, RunDirectoryObserver::State state)
{
    switch (state)
    {
        case RunDirectoryObserver::State::INIT:     return os << "INIT";
        case RunDirectoryObserver::State::STARTING: return os << "STARTING";
        case RunDirectoryObserver::State::READY:    return os << "READY";
        case RunDirectoryObserver::State::EOLS:     return os << "EOLS";
        case RunDirectoryObserver::State::EOR:      return os << "EOR";
        case RunDirectoryObserver::State::ERROR:    return os << "ERROR";
        case RunDirectoryObserver::State::NORUN:    return os << "NORUN";
        // Omit default case to trigger compiler warning for missing cases
    };
    return os;
}

std::ostream& operator<< (std::ostream& os, const RunDirectoryObserver::FileMode fileMode)
{
    switch (fileMode)
    {
        case RunDirectoryObserver::FileMode::JSN:   return os << "JSN";
        case RunDirectoryObserver::FileMode::RAW:   return os << "RAW";
        // Omit default case to trigger compiler warning for missing cases
    };
    return os;   
}



} // namespace bu
