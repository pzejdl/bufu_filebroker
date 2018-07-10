#include <iostream>

#include <boost/filesystem.hpp>

#include "tools/tools.h"
#include "bu/RunDirectoryManager.h"
#include "http/server/server.hpp"

#include "config.h"

//namespace fs = boost::filesystem;

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

#include <stdexcept>

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


int main()
{
    //int runNumber = 1000030354;
    //int runNumber = 615052;

    std::cout << "BUFU-FIleServer v" << BUFU_FILESERVER_VERSION << std::endl;

    try {
        std::thread server( ::server );
        //server.detach();

        //std::this_thread::sleep_for(std::chrono::seconds(600));

        server.join();
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

