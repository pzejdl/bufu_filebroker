#include <thread>

#include "tools/synchronized/queue.h"
#include "tools/inotify/INotify.h"
#include "tools/tools.h"
#include "bu/FileInfo.h"
#include "bu/RunDirectoryObserver.h"


namespace bu {

RunDirectoryObserver::RunDirectoryObserver(int runNumber) : runNumber(runNumber) {}

RunDirectoryObserver::~RunDirectoryObserver()
{
    std::cout << "DEBUG: RunDirectoryObserver::~RunDirectoryObserver()." << std::endl;
}


std::string RunDirectoryObserver::getStats() const
{
    const char *sep = "  ";
    std::ostringstream os;

    os << "runNumber=" << runNumber << std::endl;
    os << sep << "state=" << runDirectory.state << std::endl;
    os << '\n';
    os << sep << "startup.nbJsnFiles="                      << stats.startup.nbJsnFiles << std::endl;
    os << sep << "startup.nbJsnFilesOptimized="             << stats.startup.nbJsnFilesOptimized << '\n';
    os << sep << "startup.inotify.nbAllFiles="              << stats.startup.inotify.nbAllFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFiles="              << stats.startup.inotify.nbJsnFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFilesDuplicated="    << stats.startup.inotify.nbJsnFilesDuplicated << std::endl;
    os << '\n';
    os << sep << "inotify.nbAllFiles="                      << stats.inotify.nbAllFiles << std::endl;
    os << sep << "inotify.nbJsnFiles="                      << stats.inotify.nbJsnFiles << std::endl;
    os << '\n';
    os << sep << "nbJsnFilesProcessed="                     << stats.nbJsnFilesProcessed << std::endl;
    os << sep << "nbJsnFilesOptimized="                     << stats.nbJsnFilesOptimized << '\n';
    os << '\n';
    os << sep << "queueSizeMax="                            << stats.queueSizeMax << '\n';
    // TODO: NOTE: queue.size() is not protected by lock or memory barrier, therefore we will get a number that is not up-to-date, but it doesn't matter
    //             In principle, we should protect all statistics variables here...
    os << sep << "queueSize="                               << queue.size() << '\n';
    os << '\n';
    if (stats.fu.lastPoppedFile.type != FileInfo::FileType::EMPTY) {
        os << sep << "fu.lastPoppedFile=\""                 << stats.fu.lastPoppedFile.fileName() << "\"\n";
    } else {
        os << sep << "fu.lastPoppedFile=NONE\n";
    }
    os << sep << "fu.stopLS="                               << stats.fu.stopLS << '\n';
    os << '\n';
    os << sep << "lastEoLS="                                << runDirectory.lastEoLS << '\n';

    return os.str();
}


const std::string& RunDirectoryObserver::getError() const
{
    return errorMessage;
}


void RunDirectoryObserver::run()
{
    try {
        main();
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Unhandled exception detected." );
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

// TODO: Split into two functions:
// - updateRunDirectoryStats
// - updateFUStats
void RunDirectoryObserver::updateStats(const bu::FileInfo& file, bool updateFU)
{
    // Sanity check. In principle always OK.
    assert( (uint32_t)runNumber == file.runNumber );
    assert( file.type != FileInfo::FileType::EMPTY );

    if (file.isEoLS()) {
        runDirectory.lastEoLS = file.lumiSection;
        runDirectory.state = State::EOLS;
    } else if (file.isEoR()) {
        runDirectory.state = bu::RunDirectoryObserver::State::EOR;
    } else {
        assert( file.type == bu::FileInfo::FileType::INDEX );
        runDirectory.state = bu::RunDirectoryObserver::State::READY;
    }

    // WHen update is run from FU request
    if (updateFU) {
        stats.fu.lastPoppedFile = file;
    }
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
            updateStats(file, /*updateFU*/ false);
            continue;
        }
        sawIndexFile = true;

        pushFile( std::move(file) );
    }
}


/*
 * The main function responsible for finding files on BU.
 * Is run in a separate thread for each run directory that FU asks. 
 */
void RunDirectoryObserver::main()
{
    THREAD_DEBUG();

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
    std::cout << "DirectoryObserver: Watching: " << runDirectoryPath << std::endl;

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
        runDirectory.state = bu::RunDirectoryObserver::State::ERROR;
        errorMessage = e.what();
        std::cout << "DirectoryObserver: ERROR: " << errorMessage << std::endl;
        std::cout << "DirectoryObserver: Finished" << std::endl;
        return;
    }
    std::cout << "DirectoryObserver: INotify started." << std::endl;

    //sleep(5);

    // List files in the run directory
    bu::files_t files = bu::listFilesInRunDirectory( runDirectoryPath, fileFilter );
    stats.startup.nbJsnFiles = files.size();
    std::cout << "DirectoryObserver: Found " << files.size() << " files in run directory." << std::endl;

    //sleep(5);

    // Now, we have to read the first batch events from INotify. 
    // And we have to make sure they are not the same we obtained in listing the run directory before.
    if ( inotify.hasEvent() ) {
        std::cout << "DirectoryObserver: INotify has something." << std::endl;
        
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
                }
            }
        }
    }

    std::cout << "DirectoryObserver statistics:" << std::endl;
    std::cout << getStats();

    // Sort the files according LS and INDEX numbers
    std::sort(files.begin(), files.end());

    /**************************************************************************
     * PHASE II - Optimize: Determine the first usable .jsn file (and skip empty lumisections)
     */

    optimizeAndPushFiles(files);

    // TODO: Prevent double updates on statistics!!!
    // TODO: Check if FUs can start reading earlier...
    // FUs can start reading from our queue NOW
    runDirectory.state = bu::RunDirectoryObserver::State::READY;

    /**************************************************************************
     * PHASE III - The main loop: Now, we rely on the Inotify
     */


    // Process any new file receives through INotify
    while (running) {

        for (auto&& event : inotify.read()) {
            stats.inotify.nbAllFiles++;

            //TODO: Make it optional
            //std::cout << "DEBUG INOTIFY: " << event.name << std::endl;

            if ( boost::regex_match( event.name, fileFilter) ) {
                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );
                stats.inotify.nbJsnFiles++;
                pushFile( std::move(file) );
            }
        }

        //std::cout << "DirectoryObserver: Alive" << std::endl;
        //std::cout << "DirectoryObserver statistics:" << std::endl;
        //std::cout << getStats();        
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Hola, finito!
    std::cout << "DirectoryObserver: Finished" << std::endl;
    runDirectory.state = bu::RunDirectoryObserver::State::STOP;
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
