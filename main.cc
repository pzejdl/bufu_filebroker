#include <iostream>

#include <boost/filesystem.hpp>

#include "tools/tools.h"
#include "bu/RunDirectoryManager.h"
#include "http/server/server.hpp"

#include "tools/log.h"

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


/*
 * This will rename the index file based on filePrefix variable and if necessary create output directory specified in filePrefix.
 */
void renameIndexFile(int runNumber, const std::string& filePrefix, const std::string& fileName)
{
    const fs::path runDirectoryPath = bu::getRunDirectory( runNumber ); 
    const fs::path fileFrom = runDirectoryPath / fileName;
    const fs::path fileTo = runDirectoryPath / ( filePrefix + fileName );
    bool retry = false;
    do {
        try {
            fs::rename( fileFrom, fileTo );
            if (retry) {
                LOG(DEBUG) << "Index file rename was succesfull.";
            }
            break;
        }
        catch (fs::filesystem_error& e) {
            if (e.code() == boost::system::errc::no_such_file_or_directory) {
                if (!retry) {
                    const fs::path fuPath = runDirectoryPath / filePrefix;
                    LOG(DEBUG) << "Index file rename failed, probably the directory for renamed files is missing, trying to create it: " << fuPath << '.';
                    try {
                        fs::create_directory( fuPath );
                        LOG(DEBUG) << "Directory was created.";
                        retry = true;
                        continue;
                    }
                    catch (fs::filesystem_error& e) {
                        LOG(ERROR) << "Creating directory " << fuPath << " failed.";
                    }
                } 
            }

            // Changing global state for the ERROR in thread unsafe. So for the moment we just abort.
            //runDirectoryManager.setError( runNumber, e.what() );

            std::string errorStr { "Index file rename failed: " };
            errorStr += e.what(); 
            LOG(FATAL) << errorStr << '.';
            RETHROW( std::runtime_error, errorStr );
        }
    } while (retry);
}


unsigned long getParamUL(const http::server::request& req, const std::string& key, bool isOptional = false, unsigned long defaultValue = -1)
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
        if ( (long)value < 0) {
            throw std::invalid_argument("ERROR: Negative value present in the query parameter: '" + key + '=' + strValue + '\'');     
        } 
    } else if (!isOptional) {
        throw std::out_of_range("ERROR: Parameter '" + key + "' was not found in the query.");
    } else {
        return defaultValue;
    }
    return value;
}


#include <boost/chrono/io/time_point_io.hpp>
#include <boost/chrono/chrono.hpp>
	

/*
 * The web application(s) are defined here
 */
void createWebApplications(http::server::request_handler& app)
{
    app.add("/popfile",
    [](const http::server::request& req, http::server::reply& rep)
    {
        rep.content_type = "text/plain";
        rep.content.append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

        // Parse query parameters
        int runNumber;
        int stopLS = -1;
        try {
            runNumber   = getParamUL(req, "runnumber");
            stopLS      = getParamUL(req, "stopls", /*isOptional*/ true);
        }
        catch (std::logic_error& e) {
            rep.content.append(e.what());
            rep.status = http::server::reply::bad_request;
            return;
        }

        // Get file
        std::ostringstream os;
        const std::string filePrefix = bu::getIndexFilePrefix();
        bu::FileInfo file;
        bu::RunDirectoryObserver::State state;
        int lastEoLS;

        std::tie( file, state, lastEoLS ) = runDirectoryManager.popRunFile( runNumber, stopLS );

        // Rename the file before it is given to FU
        if (file.type != bu::FileInfo::FileType::EMPTY) { 
            renameIndexFile( runNumber, filePrefix, file.fileName() + ".jsn" );
	    }

        os << "runnumber="  << runNumber << '\n';
        os << "state="      << state << '\n';

        if (state == bu::RunDirectoryObserver::State::ERROR || state == bu::RunDirectoryObserver::State::NORUN) {
            os << "errormessage=\"" << runDirectoryManager.getError( runNumber ) << "\"\n";
        }

        if (file.type != bu::FileInfo::FileType::EMPTY) { 
            assert( (uint32_t)runNumber == file.runNumber );
            os << "file=\""         << file.fileName() << "\"\n";
            os << "fileprefix=\""   << filePrefix << "\"\n";
            os << "lumisection="    << file.lumiSection << '\n';
            os << "index="          << file.index << '\n';
        } else { 
            os << "lumisection="    << lastEoLS << '\n';
        }
        os << "lasteols="           << lastEoLS << '\n';


        // TODO: DEBUG Make this optional
        if (false) { 
            // TODO: Move timestamps to tools
            std::cout << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ' ';
            //std::cout << boost::chrono::system_clock::now() << '\n';

            std::cout << "DEBUG POPFILE: " << state << ' ';
            if (file.type != bu::FileInfo::FileType::EMPTY) { 
                std::cout << '\"'           << file.fileName() << "\" ";
                std::cout << "lumisection=" << file.lumiSection << ' ';
            } else {
                std::cout << "lumisection=" << lastEoLS << ' ';
            }
            std::cout << "lasteols="        << lastEoLS << std::endl;
        }

        rep.content.append( os.str() );
    });


    auto getStats = [](const http::server::request& req, http::server::reply& rep)
    {
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
    };


    app.add("/stats",
    [getStats](const http::server::request& req, http::server::reply& rep)
    {
        rep.content_type = "text/plain";
        rep.content.append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

        getStats( req, rep );
    });  


    // HTML version of /stats
    app.add("/html/stats",
    [getStats](const http::server::request& req, http::server::reply& rep)
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

        getStats( req, rep );

        rep.content.append( "</pre>\n" );
        rep.content.append( "</body>\n" );
        rep.content.append( "</html>\n" );
    });        


    app.add("/index.html",
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


    app.add("/restart",
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
        createWebApplications( s.request_handler() );

        std::cout << "Server: Starting HTTP server at " << address << ':' << port << docRoot << std::endl;

        // Run the server until stopped.
        s.run();

        std::cout << "Server: HTTP Finished" << std::endl;
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Exception detected." );
    }
}

/*
 * This is the main function where everything start and should end... :)
 */

int main()
{
    //int runNumber = 1000030354;
    //int runNumber = 615052;

    std::cout << "BUFU-FileServer v" << BUFU_FILESERVER_VERSION << std::endl;

    try {
        std::thread server( ::server );
        server.join();
    }
    catch (const std::system_error& e) {
        std::cout << "Error: " << e.code() << " - " << e.what() << '\n';
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Exception detected." );
    }

    std::cout << "main finished." << std::endl;
    return 0;
}

