#include <iostream>
#include <system_error>
//#include <regex>
#include <boost/regex.hpp>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <thread>
#include <atomic>
#include <unordered_map>

#include "tools/inotify/INotify.h"
#include "tools/exception.h"
#include "tools/tools.h"

#include "bu/FileInfo.h"

#include "bu.h"
#include "RunDirectory.h"

namespace fs = boost::filesystem;

/*
 * Note: 
 *   Boost up to ver 60 doesn't have move semantics for boost::filesystem::path !!!
 *   When necessary, I use std:::string.
 */

// Forward declaration
void directoryObserverRunner(RunDirectoryObserver& observer);

RunDirectoryManager runDirectoryManager { directoryObserverRunner };



std::string getStats(RunDirectoryObserver& observer, const std::string sep = "")
{
    RunDirectoryObserver::Statistics& stats = observer.stats;
    std::ostringstream os;

    os << sep << "startup.nbJsnFiles="                      << stats.startup.nbJsnFiles << std::endl;
    os << sep << "startup.inotify.nbAllFiles="              << stats.startup.inotify.nbAllFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFiles="              << stats.startup.inotify.nbJsnFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFilesDuplicated="    << stats.startup.inotify.nbJsnFilesDuplicated << std::endl;
    os << std::endl;
    os << sep << "inotify.nbAllFiles="                      << stats.inotify.nbAllFiles << std::endl;
    os << sep << "inotify.nbJsnFiles="                      << stats.inotify.nbJsnFiles << std::endl;
    os << std::endl;
    os << sep << "nbJsnFilesQueued="                        << stats.nbJsnFilesQueued << std::endl;

    return os.str();
}



void optimizeAndPushFiles(RunDirectoryObserver& observer, bu::files_t& files) 
{
    for (auto&& file : files) {

        // At the moment we add all files

        //bu::FileInfo file = bu::parseFileName( fileName.c_str() );

        // Consistency check for the moment
        //assert( fileName == (file.fileName() + ".jsn") );

        observer.queue.push( std::move(file) );
        observer.stats.nbJsnFilesQueued++;
    }
}



/*
 * The main function responsible for finding files on BU.
 * Is run in a separate thread for each run directory that FU asks. 
 */
void directoryObserverRunner_main(RunDirectoryObserver& observer)
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


    const std::string runDirectoryPath = bu::getRunDirectory( observer.runNumber ).string(); 
    std::cout << "DirectoryObserver: Watching: " << runDirectoryPath << std::endl;

    /**************************************************************************
     * PHASE I - Startup: Inotify is started and the run directory is searched for existing .jsn files
     */

    // INotify has to be set before we list the directory content, otherwise we have a race condition...
    tools::INotify inotify;
    inotify.add_watch( runDirectoryPath, IN_CREATE );
    std::cout << "DirectoryObserver: INotify started." << std::endl;

    sleep(5);

    // List files in the run directory
    bu::files_t files = bu::listFilesInRunDirectory( runDirectoryPath, fileFilter );
    observer.stats.startup.nbJsnFiles = files.size();
    std::cout << "DirectoryObserver: Found " << files.size() << " files in run directory." << std::endl;

    sleep(5);

    // Now, we have to read the first batch events from INotify. 
    // And we have to make sure they are not thw same we obtained in listing the run directory before.
    if ( inotify.hasEvent() ) {
        std::cout << "DirectoryObserver: INotify has something." << std::endl;
        
        for (auto&& event : inotify.read()) {
            observer.stats.startup.inotify.nbAllFiles++;

            if ( boost::regex_match( event.name, fileFilter) ) {
                observer.stats.startup.inotify.nbJsnFiles++;

                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );

                // Add files that are not duplicates
                if ( std::find(files.begin(), files.end(), file) == files.end() ) {
                    files.push_back( std::move( file ));
                } else {
                    observer.stats.startup.inotify.nbJsnFilesDuplicated++;
                }
            }
        }
    }

    std::cout << "DirectoryObserver statistics:" << std::endl;
    std::cout << getStats(observer, "    ");

    // Sort the files according LS and INDEX numbers
    std::sort(files.begin(), files.end());

    /**************************************************************************
     * PHASE II - Optimize: Determine the first usable .jsn file (and skip empty lumisections)
     */

    optimizeAndPushFiles(observer, files);

    // FUs can start reading from our queue NOW
    observer.state = RunDirectoryObserver::State::READY;

    /**************************************************************************
     * PHASE III - The main loop: Now, we rely on the Inotify
     */


    // Process any new file receives through INotify
    while (observer.running) {

        for (auto&& event : inotify.read()) {
            observer.stats.inotify.nbAllFiles++;

            if ( boost::regex_match( event.name, fileFilter) ) {
                observer.stats.inotify.nbJsnFiles++;

                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );
 
                observer.queue.push( std::move(file) );
                observer.stats.nbJsnFilesQueued++;
            }
        }

        std::cout << "DirectoryObserver: Alive" << std::endl;
        std::cout << "DirectoryObserver statistics:" << std::endl;
        std::cout << getStats(observer, "    ");        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Hola, finito!
    std::cout << "DirectoryObserver: Finished" << std::endl;
    observer.state = RunDirectoryObserver::State::STOP;
}

void directoryObserverRunner(RunDirectoryObserver& observer)
{
    try {
        directoryObserverRunner_main(observer);
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Unhandled exception detected." );
    } 
}



//TODO: Split into two functions
bool getFileFromBU(int runNumber, bu::FileInfo& file)
{
    RunDirectoryObserver& observer = runDirectoryManager.getRunDirectoryObserver( runNumber );

    if (observer.state == RunDirectoryObserver::State::READY) {
        FileQueue_t& queue = observer.queue;
        return queue.pop(file);     
    }

    std::cout << "Requester: RunDirectoryObserver " << runNumber << " is not READY, it is " << observer.state << std::endl;
    return false;
}




namespace fu {
    std::atomic<bool> done(false);

    void requester_main(int runNumber)
    {
        THREAD_DEBUG();

        while (!done) {
            bu::FileInfo file;

            if ( getFileFromBU(runNumber, file) ) {
                std::cout << "Requester: " << runNumber << ": " << file << std::endl;
            } else {
                std::cout << "Requester: " << runNumber << ": EMPTY" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }          
        }
        std::cout << "Requester: Finished" << std::endl;
    }

    void requester(int runNumber)
    {
        try {
            requester_main(runNumber);
        }
        catch(const std::exception& e) {
            BACKTRACE_AND_RETHROW( std::runtime_error, "Unhandled exception detected." );
        }
    }

}


int main()
{
    //int runNumber = 1000030354;
    int runNumber = 615052;

    std::cout << boost::format("HOHOHO: %u\n") % 123;

    try {
        std::thread requester( fu::requester, runNumber );
        requester.detach();

        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::this_thread::sleep_for(std::chrono::seconds(200));
                fu::done = true;


        //requester.join();
    }
    catch (const std::system_error& e) {
        std::cout << "Error: " << e.code() << " - " << e.what() << '\n';
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Unhandled exception detected." );
    }

    return 0;
}

