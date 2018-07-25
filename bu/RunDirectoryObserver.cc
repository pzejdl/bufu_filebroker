#include <thread>

#include "tools/synchronized/queue.h"
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
    os << sep << "run.state="                               << stats.run.state << '\n';
    os << sep << "run.lastProcessedFile=\""                 << stats.run.lastProcessedFile.fileName() << "\"\n";
    os << sep << "run.lastEoLS="                            << stats.run.lastEoLS << '\n';
    os << '\n';
    os << sep << "queueSizeMax="                            << stats.queueSizeMax << '\n';
    // NOTE: queue.size() is not protected by lock or memory barrier, therefore we will get a number that is not up-to-date, but it doesn't matter
    os << sep << "queueSize="                               << queue.size() << '\n';
    os << '\n';
    os << sep << "fu.state="                                << stats.fu.state << '\n';
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


// void RunDirectoryObserver::setError(const std::string& errorMessage)
// {
//     stats.run.state = bu::RunDirectoryObserver::State::ERROR;
//     stats.fu.state = bu::RunDirectoryObserver::State::ERROR;
//     this->errorMessage = errorMessage;
// }


void RunDirectoryObserver::run()
{
    try {
        runner();
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Exception detected." );
    } 
}


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


void RunDirectoryObserver::updateRunDirectoryStats(const bu::FileInfo& file)
{
    // Sanity check. In principle always OK.
    assert( (uint32_t)runNumber == file.runNumber );
    assert( file.type != FileInfo::FileType::EMPTY );

    if (file.isEoLS()) {
        stats.run.lastEoLS = file.lumiSection;
        stats.run.state = State::EOLS;
    } else if (file.isEoR()) {
        stats.run.state = bu::RunDirectoryObserver::State::EOR;
    } else {
        assert( file.type == bu::FileInfo::FileType::INDEX );
        stats.run.state = bu::RunDirectoryObserver::State::READY;
    }

    stats.run.lastProcessedFile = file;
}


void RunDirectoryObserver::updateFUStats(const bu::FileInfo& file)
{
    // Sanity check. In principle always OK.
    assert( (uint32_t)runNumber == file.runNumber );
    assert( file.type != FileInfo::FileType::EMPTY );

    if (file.isEoLS()) {
        stats.fu.lastEoLS = file.lumiSection;
        stats.fu.state = State::EOLS;
    } else if (file.isEoR()) {
        stats.fu.state = bu::RunDirectoryObserver::State::EOR;
    } else {
        assert( file.type == bu::FileInfo::FileType::INDEX );
        stats.fu.state = bu::RunDirectoryObserver::State::READY;
    }

    stats.fu.lastPoppedFile = file;
}


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


/*
 * The main function responsible for finding files on BU.
 * Is run in a separate thread for each run directory that FU asks. 
 */
void RunDirectoryObserver::runner()
{
    LOG(INFO) << TOOLS_THREAD_INFO();

    /*
     * .jsn file filter definition
     *  Examples:
     *    run1000030354_ls0000_EoR.jsn
     *    run1000030354_ls0017_EoLS.jsn
     *    run1000030348_ls0511_index020607.jsn
     * 
     * NOTE: std::regex is broken until gcc 4.9.0. Using boost::regex
     */
    static const boost::regex fileFilter( "run[0-9]+_ls[0-9]+_.*\\.jsn" );
    //const std::regex fileFilter( "run[0-9]+_ls[0-9]+_.*\\.jsn" );


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
    //TODO: Move to the end of this function and make a more general case
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

            if ( boost::regex_match( event.name, fileFilter) ) {
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

    // TODO: Check if FUs can start reading earlier...
    // FUs can start reading from our queue NOW

    if ( queue.empty() ) {
        // If the queue is empty then FU state is the same like the run directory state
        stats.fu.state = stats.run.state;
    } else {
        // If we have some files in the queue then we are ready for requests
        stats.fu.state = bu::RunDirectoryObserver::State::READY;
    }

    LOG(DEBUG) << "DirectoryObserver statistics:\n" 
        << getStats();

    /**************************************************************************
     * PHASE III - The main loop: Now, we rely on the Inotify
     */


    // Process any new file receives through INotify
    while (running) {

        for (auto&& event : inotify.read()) {
            stats.inotify.nbAllFiles++;

            //TODO: Make it optional
            //LOG(DEBUG) << "INOTIFY: " << event.name;

            if ( boost::regex_match( event.name, fileFilter) ) {
                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );
                //LOG(DEBUG) << file.fileName();

                stats.inotify.nbJsnFiles++;
                updateRunDirectoryStats( file );
                pushFile( std::move(file) );
            }
        }
        stats.inotify.nbInotifyReadCalls++;

        //std::cout << "DirectoryObserver: Alive" << std::endl;
        //std::cout << "DirectoryObserver statistics:" << std::endl;
        //std::cout << getStats();        
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Hola, finito!
    LOG(INFO) << "DirectoryObserver: Finished";
    stats.run.state = bu::RunDirectoryObserver::State::STOP;
}




std::ostream& operator<< (std::ostream& os, RunDirectoryObserver::State state)
{
    switch (state)
    {
        case RunDirectoryObserver::State::INIT:     return os << "INIT" ;
        case RunDirectoryObserver::State::STARTING: return os << "STARTING";
        case RunDirectoryObserver::State::READY:    return os << "READY";
        case RunDirectoryObserver::State::EOLS:     return os << "EOLS";
        case RunDirectoryObserver::State::EOR:      return os << "EOR";
        case RunDirectoryObserver::State::STOP:     return os << "STOP";
        case RunDirectoryObserver::State::ERROR:    return os << "ERROR";
        case RunDirectoryObserver::State::NORUN:    return os << "NORUN";
        // Omit default case to trigger compiler warning for missing cases
    };
    return os;
}

} // namespace bu


/*
class RunDirectoryObserver {

public:    

    RunDirectoryObserver(int runNumber) : runNumber_(runNumber) {}
    ~RunDirectoryObserver()
    {
        std::cout << "DEBUG: RunDirectoryObserver: Destructor called" << std::endl;
        running_ = false;
        if (runner_.joinable()) {
            runner_.join();
        }
    }

    RunDirectoryObserver(const RunDirectoryObserver&) = delete;
    RunDirectoryObserver& operator=(const RunDirectoryObserver&) = delete;


    void start() 
    {
        assert( state_ == State::INIT );
        runner_ = std::thread(&RunDirectoryObserver::run, this);
        runner_.detach();
        state_ = State::STARTING;
    }


    void stopRequest()
    {
        running_ = false;    
    }


    State state() const 
    {
        return state_;
    }

    FileNameQueue_t& queue()
    {
        return queue_;
    }

private:

    void run()
    {
        while (running_) {
            std::cout << "RunDirectoryObserver: Alive" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        std::cout << "RunDirectoryObserver: Finished" << std::endl;
        state_ = State::STOP;
    }

private:
    std::atomic<State> state_ { State::INIT };
    std::atomic<bool> running_{ true };

    int runNumber_;
    std::thread runner_ {};

    FileNameQueue_t queue_;
};
*/
