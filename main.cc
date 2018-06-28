// #include <iostream>
#include <system_error>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

// #include <thread>
// #include <atomic>
// #include <unordered_map>
// #include <ctime>

// #include "tools/exception.h"
// #include "bu/FileInfo.h"

#include "tools/inotify/INotify.h"
#include "tools/tools.h"

#include "bu/bu.h"
#include "bu/RunDirectoryObserver.h"
#include "bu/RunDirectoryManager.h"

#include "http/server/server.hpp"


namespace fs = boost::filesystem;

/*
 * Note: 
 *   Boost up to ver 60 doesn't have move semantics for boost::filesystem::path !!!
 *   When necessary, I use std:::string.
 */

// Forward declaration
void directoryObserverRunner(bu::RunDirectoryObserver& observer);

// Global initialization for simplicity, at the moment
bu::RunDirectoryManager runDirectoryManager { directoryObserverRunner };



void pushFile(bu::RunDirectoryObserver& observer, bu::FileInfo file)
{
    observer.queue.push( std::move(file) );
    observer.stats.nbJsnFilesQueued++;
}

void updateStats(bu::RunDirectoryObserver& observer, const bu::FileInfo& file)
{
    // Sanity check. In principle always true.
    assert( (uint32_t)observer.runNumber == file.runNumber );

    if (file.isEoLS()) {
        observer.stats.lastEoLS = file.lumiSection;
    } else if (file.isEoR()) {
        observer.stats.isEoR = true;
    } else {
        assert( file.type == bu::FileInfo::FileType::INDEX );
        observer.stats.lastIndex = file.index;
    }
}

void optimizeAndPushFiles(bu::RunDirectoryObserver& observer, const bu::files_t& files) 
{
    for (auto&& file : files) {
        updateStats(observer, file);

        // Skip EoLS and EoR, they are already flagged in runDirectoryObserver
        if (file.type == bu::FileInfo::FileType::EOLS || file.type == bu::FileInfo::FileType::EOR) {
            continue;
        }

        pushFile( observer, std::move(file) );
    }
}



/*
 * The main function responsible for finding files on BU.
 * Is run in a separate thread for each run directory that FU asks. 
 */
void directoryObserverRunner_main(bu::RunDirectoryObserver& observer)
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

    //sleep(5);

    // List files in the run directory
    bu::files_t files = bu::listFilesInRunDirectory( runDirectoryPath, fileFilter );
    observer.stats.startup.nbJsnFiles = files.size();
    std::cout << "DirectoryObserver: Found " << files.size() << " files in run directory." << std::endl;

    //sleep(5);

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
                if ( std::find(files.cbegin(), files.cend(), file) == files.cend() ) {
                    files.push_back( std::move( file ));
                } else {
                    observer.stats.startup.inotify.nbJsnFilesDuplicated++;
                }
            }
        }
    }

    std::cout << "DirectoryObserver statistics:" << std::endl;
    std::cout << observer.getStats();

    // Sort the files according LS and INDEX numbers
    std::sort(files.begin(), files.end());

    /**************************************************************************
     * PHASE II - Optimize: Determine the first usable .jsn file (and skip empty lumisections)
     */

    optimizeAndPushFiles(observer, files);

    // FUs can start reading from our queue NOW
    observer.state = bu::RunDirectoryObserver::State::READY;

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
 
                pushFile( observer, std::move(file) );
            }
        }

        std::cout << "DirectoryObserver: Alive" << std::endl;
        std::cout << "DirectoryObserver statistics:" << std::endl;
        std::cout << observer.getStats();        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Hola, finito!
    std::cout << "DirectoryObserver: Finished" << std::endl;
    observer.state = bu::RunDirectoryObserver::State::STOP;
}

void directoryObserverRunner(bu::RunDirectoryObserver& observer)
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
    bu::RunDirectoryObserver& observer = runDirectoryManager.getRunDirectoryObserver( runNumber );

    if (observer.state == bu::RunDirectoryObserver::State::READY) {
        bu::FileQueue_t& queue = observer.queue;
        return queue.pop(file);     
    }

    std::cout << "Requester: RunDirectoryObserver " << runNumber << " is not READY, it is " << observer.state << std::endl;
    return false;
}



namespace fu {
    std::atomic<bool> done(false);

    void addServerHandlers(http::server::server& s)
    {
        s.request_handler().add_handler("/index.html",
        [](const http::server::request& req, http::server::reply& rep)
        {
            (void)req;
            rep.content_type = "text/html";

            std::time_t time = std::time(nullptr);

            std::ostringstream os;
            os
            << "<html>\n"
            << "<head><title>BUFU File Server</title></head>\n"
            << "<body>\n"
            << "<h1>BUFU File Server is alive!</h1>\n"
            << "<p>Version not yet known :)</p>\n"
            << "<p>" << std::asctime(std::localtime( &time )) << "</p>\n"
            << "</body>\n"
            << "</html>\n";

            rep.content = os.str();
        });

        s.request_handler().add_handler("/test",
        [](const http::server::request& req, http::server::reply& rep)
        {
            rep.content_type = "text/plain";
            rep.content.append("Test: Hello, I'm alive!\n\n");

            rep.content.append("Parameters:\n");
            for (const auto& pair: req.query_params) {
                rep.content.append( pair.first + '=' + pair.second + '\n');
            }
        });

        s.request_handler().add_handler("/stats",
        [](const http::server::request& req, http::server::reply& rep)
        {
            rep.content_type = "text/plain";

            std::string val;
            if (req.getParam("runnumber", val)) {
                int runNumber;
                try {
                    runNumber = std::stoul(val);
                }
                catch(const std::exception& e) {
                    rep.content.append( "ERROR: Cannot parse query parameter: '" + val + '\'' );
                    return;
                }    
                // Return statistics for one run
                rep.content.append( runDirectoryManager.getStats(runNumber) );

            } else {
                // Return statistics for all runs
                rep.content.append( runDirectoryManager.getStats() );
            }

            // int runNumber = 1000030354;
            // bu::RunDirectoryObserver& observer = runDirectoryManager.getRunDirectoryObserver( runNumber );

            // std::string stats = getStats( observer );

        });        
    }

    void server()
    {
        THREAD_DEBUG();
        try {
            std::string address = "0.0.0.0";
            std::string port = "8080";
            std::string docRoot = "/fff/ramdisk"; 

            // Initialise the server.
            http::server::server s(address, port, docRoot);

            // Add handlers
            addServerHandlers(s);

            std::cout << "Server: Starting HTTP server at " << address << ':' << port << docRoot << std::endl;

            // Run the server until stopped.
            s.run();

            std::cout << "Server: HTTP Finished" << std::endl;
        }
        catch(const std::exception& e) {
            BACKTRACE_AND_RETHROW( std::runtime_error, "Unhandled exception detected." );
        }
    }


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
    int runNumber = 1000030354;
    //int runNumber = 615052;

    std::cout << boost::format("HOHOHO: %u\n") % 123;

    try {
        std::thread server( fu::server );
        server.detach();

        //std::thread requester( fu::requester, runNumber );
        //requester.detach();

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

    std::cout << "main finished." << std::endl;
    return 0;
}

