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
 *   When necessary, I use std:string.
 */


struct Statistics {
    int item1;  // Something

    tools::synchronized::spinlock lock;
} stats;



//TODO: We have to put queues for directories into map...
//std::map< int, std::string > watched_files_;



// Forward declaration
void directoryObserverRunner(RunDirectoryObserver& observer);

RunDirectoryManager runDirectoryManager { directoryObserverRunner };



void testINotify(const std::string& runDirectory)
{
    tools::INotify inotify;
    inotify.add_watch( runDirectory,
        IN_MODIFY | IN_CREATE | IN_DELETE);

    while (true) {
        for (auto&& event : inotify.read()) {
            std::cout << event;
        }
    }
}


void processFiles(RunDirectoryObserver& observer, bu::files_t& files) 
{
    // Move to the queue
    for (auto&& file : files) {
        //bu::FileInfo file = bu::parseFileName( fileName.c_str() );

        // Consistency check for the moment
        //assert( fileName == (file.fileName() + ".jsn") );

        observer.queue.push( std::move(file) );
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
     */

    // INotify has to be set before we list the directory content, otherwise we have a race condition...
    tools::INotify inotify;
    inotify.add_watch( runDirectoryPath, IN_CREATE );
    std::cout << "DirectoryObserver: INotify started." << std::endl;

    // List files in run directory
    bu::files_t files = bu::listFilesInRunDirectory( runDirectoryPath, fileFilter );
    std::cout << "DirectoryObserver: Found " << files.size() << " files in run directory." << std::endl;

    // Now, we have to read the first batch events from INotify. 
    // And we have to make sure they are not duplicated in the directory listing obtained before.
    if ( inotify.hasEvent() ) {
        int nbFilesSeen = 0;
        int nbFilesAdded = 0;
        std::cout << "DirectoryObserver: INotify has something." << std::endl;
        
        for (auto&& event : inotify.read()) {
            if ( boost::regex_match( event.name, fileFilter) ) {
                nbFilesSeen++;

                bu::FileInfo file = bu::temporary::parseFileName( event.name.c_str() );

                // Add files that are not duplicates
                if ( std::find(files.begin(), files.end(), file) == files.end() ) {
                    files.push_back( std::move( file ));
                    nbFilesAdded++;
                } 
            }
        }
        std::cout << "DirectoryObserver: INotify saw:   " << nbFilesSeen << " files." << std::endl;
        std::cout << "DirectoryObserver: INotify added: " << nbFilesAdded << " files." << std::endl;
    }

    // Sort the files according LS and INDEX numbers
    std::sort(files.begin(), files.end());

    /**************************************************************************
     */

    processFiles(observer, files);

    // FUs can start reading from our queue NOW
    observer.state = RunDirectoryObserver::State::READY;

    /*
     **************************************************************************/



    // Process any new file receives through INotify
    while (observer.running) {

        for (auto&& event : inotify.read()) {
            std::cout << event << std::endl;
        }

        std::cout << "DirectoryObserver: Alive" << std::endl;
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
                std::this_thread::sleep_for(std::chrono::milliseconds(700));
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
        fu::done = true;
        std::this_thread::sleep_for(std::chrono::seconds(200));

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

