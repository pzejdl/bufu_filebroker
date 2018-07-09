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

#include "config.h"

namespace fs = boost::filesystem;

/*
 * Note: 
 *   Boost up to ver 60 doesn't have move semantics for boost::filesystem::path !!!
 *   When necessary, I use std:::string.
 */


/**************************************************************************
 * Global variables
 */


// Global initialization for simplicity, at the moment
bu::RunDirectoryManager runDirectoryManager;

/*****************************************************************************/

namespace bu {

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


void RunDirectoryObserver::updateStats(const bu::FileInfo& file)
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
            updateStats(file);
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


void RunDirectoryObserver::run()
{
    try {
        main();
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Unhandled exception detected." );
    } 
}

} // namespace bu


unsigned long getParamUL(const http::server::request& req, const std::string& key)
{
    std::string strValue;
    unsigned long value;
    if (req.getParam(key, strValue)) {
        try {
            value = std::stoul(strValue);
        }
        catch(const std::exception& e) {
            throw std::invalid_argument("ERROR: Cannot parse query parameter: '" + key + '=' + strValue + '\'');  
        } 
    } else {
        throw std::out_of_range("ERROR: Parameter '" + key + "' was not found in the query.");
    }    
    return value;
}


#include <boost/chrono/io/time_point_io.hpp>
#include <boost/chrono/chrono.hpp>

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
            os  << "<html>\n"
                << "<head><title>BUFU File Server</title></head>\n"
                << "<body>\n"
                << "<h1>BUFU File Server is alive!</h1>\n"
                << "<p>v" BUFU_FILESERVER_VERSION "</p>\n"
                << "<p>" << std::asctime(std::localtime( &time )) << "</p>\n"
                << "</body>\n"
                << "</html>\n";

            rep.content = os.str();
        });


        s.request_handler().add_handler("/restart",
        [](const http::server::request& req, http::server::reply& rep)
        {
            rep.content_type = "text/plain";
            rep.content.append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

            int runNumber;
            try {
                runNumber = getParamUL(req, "runnumber");
            }
            catch (std::logic_error& e) {
                rep.content.append(e.what());
                rep.status = http::server::reply::bad_request;
                return;
            }
        
            runDirectoryManager.restartRunDirectoryObserver( runNumber );
            rep.content.append( runDirectoryManager.getStats(runNumber) );
        });


        s.request_handler().add_handler("/popfile",
        [](const http::server::request& req, http::server::reply& rep)
        {
            rep.content_type = "text/plain";
            rep.content.append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

            int runNumber;
            try {
                runNumber = getParamUL(req, "runnumber");
            }
            catch (std::logic_error& e) {
                rep.content.append(e.what());
                rep.status = http::server::reply::bad_request;
                return;
            }

            //Get file
            std::ostringstream os;

            bu::FileInfo file;
            bu::RunDirectoryManager::RunDirectoryStatus run;        

            std::tie( file, run ) = runDirectoryManager.popRunFile( runNumber );

            os << "runnumber=" << runNumber << '\n';
            os << "state=" << run.state << '\n';

            if (run.state == bu::RunDirectoryObserver::State::ERROR) {
                os << "errormessage=\"" << runDirectoryManager.getError( runNumber ) << "\"\n";
            }

            if (file.type != bu::FileInfo::FileType::EMPTY) { 
                assert( (uint32_t)runNumber == file.runNumber);

                os << "file=\"" << file.fileName()<< "\"\n";
                os << "lumisection=" << file.lumiSection << '\n';
                os << "index=" << file.index << '\n';
            } else { 
                os << "lumisection=" << run.lastEoLS << '\n';
            }
            os << "lasteols=" << run.lastEoLS << '\n';


            // TODO: DEBUG Make this optional
            if (false) { 
                // TODO: Move timestamts to tools
                std::cout << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ' ';
                //std::cout << boost::chrono::system_clock::now() << '\n';

                std::cout << "DEBUG POPFILE: " << run.state << ' ';
                if (file.type != bu::FileInfo::FileType::EMPTY) { 
                    std::cout << '\"' << file.fileName() << "\" ";
                    std::cout << "lumisection=" << file.lumiSection << ' ';
                } else {
                    std::cout << "lumisection=" << run.lastEoLS << ' ';
                }
                std::cout << "lasteols=" << run.lastEoLS << std::endl;
            }

            rep.content.append( os.str() );
        });


        s.request_handler().add_handler("/stats",
        [](const http::server::request& req, http::server::reply& rep)
        {
            rep.content_type = "text/plain";
            rep.content.append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

            int runNumber = -1;
            {
                std::string strValue;
                try {
                    if (req.getParam("runnumber", strValue)) {
                        runNumber = std::stoul(strValue);
                    }
                }
                catch(const std::exception& e) {
                    rep.content.append( "ERROR: Cannot parse query parameter: '" + strValue + '\'' );
                    rep.status = http::server::reply::bad_request;
                    return;
                }
            }

            if (runNumber >= 0) {    
                // Return statistics for one run
                rep.content.append( runDirectoryManager.getStats(runNumber) );
            } else {
                // Return statistics for all runs
                rep.content.append( runDirectoryManager.getStats() );
            }
        });  

        // HTML version of /stats
        s.request_handler().add_handler("/html/stats",
        [](const http::server::request& req, http::server::reply& rep)
        {
            rep.content_type = "text/html";

            rep.content.append( "<html>\n" );
            rep.content.append( "<head>\n" );
            rep.content.append( "<title>BUFU File Server</title>" );
            rep.content.append( "<meta http-equiv=\"refresh\" content=\"1\" />" );
            rep.content.append( "</head>\n" );
            rep.content.append( "<body>\n" );
            rep.content.append( "<pre>\n" );

            rep.content.append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

            int runNumber = -1;
            {
                std::string strValue;
                try {
                    if (req.getParam("runnumber", strValue)) {
                        runNumber = std::stoul(strValue);
                    }
                }
                catch(const std::exception& e) {
                    rep.content.append( "ERROR: Cannot parse query parameter: '" + strValue + '\'' );
                    rep.status = http::server::reply::bad_request;
                    return;
                }
            }

            if (runNumber >= 0) {    
                // Return statistics for one run
                rep.content.append( runDirectoryManager.getStats(runNumber) );
            } else {
                // Return statistics for all runs
                rep.content.append( runDirectoryManager.getStats() );
            }

            rep.content.append( "</pre>\n" );
            rep.content.append( "</body>\n" );
            rep.content.append( "</html>\n" );
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
            http::server::server s(address, port, docRoot, /*debug_http_requests*/ false);

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
            //bu::FileInfo file;

            // if ( getFileFromBU(runNumber, file) ) {
            //     std::cout << "Requester: " << runNumber << ": " << file << std::endl;
            // } else {
            //     std::cout << "Requester: " << runNumber << ": EMPTY" << std::endl;
            //     std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            // }          
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
    //int runNumber = 615052;

    //std::cout << boost::format("HOHOHO: %u\n") % 123;

    std::cout << "BUFU-FIleServer v" << BUFU_FILESERVER_VERSION << std::endl;

    try {
        std::thread server( fu::server );
        //server.detach();

        //std::thread requester( fu::requester, runNumber );
        //requester.detach();

        //std::this_thread::sleep_for(std::chrono::seconds(2));
        //std::this_thread::sleep_for(std::chrono::seconds(600));
        fu::done = true;

        server.join();

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

