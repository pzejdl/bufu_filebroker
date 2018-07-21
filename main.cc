#include <iostream>

#include <boost/filesystem.hpp>

#include "tools/tools.h"
#include "bu/RunDirectoryManager.h"
#include "http/1.1/server/server.hpp"

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
                        // TODO: fix this with boost properly
                        chmod( fuPath.string().c_str(), 0777 );
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


unsigned long getParamUL(const http_server::request_t& req, const std::string& key, bool isOptional = false, unsigned long defaultValue = -1)
{
    std::string strValue;
    unsigned long value;
    if (req.query(key, strValue)) {
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
void createWebApplications(http_server::request_handler& app)
{
    app.add("/popfile",
    [](const http_server::request_t& req, http_server::response_t& res)
    {
        res.set(http::field::content_type, "text/plain");
        res.body().append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

        // Parse query parameters
        int runNumber;
        int stopLS = -1;
        try {
            runNumber   = getParamUL(req, "runnumber");
            stopLS      = getParamUL(req, "stopls", /*isOptional*/ true);
        }
        catch (std::logic_error& e) {
            res.body().append(e.what());
            res.result(http::status::bad_request);
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
            // TODO: Make it optional
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

        res.body().append( os.str() );
    });


    auto getStats = [](const http_server::request_t& req, http_server::response_t& res)
    {
        int runNumber = -1;
        {
            std::string strValue;
            try {
                if (req.query("runnumber", strValue)) {
                    runNumber = std::stoul(strValue);
                }
            }
            catch(const std::exception& e) {
                res.body().append( "ERROR: Cannot parse query parameter: '" + strValue + '\'' );
                res.result(http::status::bad_request);
                return;
            }
        }

        if (runNumber >= 0) {    
            // Return statistics for one run
            res.body().append( runDirectoryManager.getStats(runNumber) );
        } else {
            // Return statistics for all runs
            res.body().append( runDirectoryManager.getStats() );
        }
    };


    app.add("/stats",
    [getStats](const http_server::request_t& req, http_server::response_t& res)
    {
        res.set(http::field::content_type, "text/plain");
        res.body().append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

        getStats( req, res );
    });  


    // HTML version of /stats
    app.add("/html/stats",
    [getStats](const http_server::request_t& req, http_server::response_t& res)
    {
        res.body().append( "<html>\n" );
        res.body().append( "<head>\n" );
        res.body().append( "<title>BUFU File Server</title>" );
        res.body().append( "<meta http-equiv=\"refresh\" content=\"1\" />" );
        res.body().append( "</head>\n" );
        res.body().append( "<body>\n" );
        res.body().append( "<pre>\n" );

        res.body().append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

        getStats( req, res );

        res.body().append( "</pre>\n" );
        res.body().append( "</body>\n" );
        res.body().append( "</html>\n" );
    });        


    app.add("/index.html",
    [](const http_server::request_t& req, http_server::response_t& res)
    {
        (void)req;
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

        res.body() = os.str();
    });


    app.add("/restart",
    [](const http_server::request_t& req, http_server::response_t& res)
    {
        res.set(http::field::content_type, "text/plain");
        res.body().append("version=\"" BUFU_FILESERVER_VERSION "\"\n");

        int runNumber;
        try {
            runNumber = getParamUL(req, "runnumber");
        }
        catch (std::logic_error& e) {
            res.body().append(e.what());
            res.result(http::status::bad_request);
            return;
        }
    
        runDirectoryManager.restartRunDirectoryObserver( runNumber );
        res.body().append( runDirectoryManager.getStats(runNumber) );
    });
}


void server(http_server::server& s)
{
    LOG(INFO) << TOOLS_THREAD_INFO();
    try {
        // Run the server until stopped.
        s.run();
        LOG(INFO) << "Server: HTTP Service IO Thread Finished";
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

    LOG(INFO) << "BUFU-FileServer v" << BUFU_FILESERVER_VERSION;

    std::string address = "0.0.0.0";
    std::string port = "8080";
    std::string docRoot = "/fff/ramdisk"; 

    // Initialise the server.
    http_server::server s(address, port, docRoot, /*threads*/ 1, /*debug_http_requests*/ false);

    // Add handlers
    createWebApplications( s.request_handler() );

    LOG(INFO) << "Server: Starting HTTP server at " << address << ':' << port << docRoot;

    try {
        std::thread server1( ::server, std::ref(s) );
        //std::thread server2( ::server, std::ref(s) );

        server1.join();
        //server2.join();
    }
    catch (const std::system_error& e) {
        LOG(ERROR) << "Error: " << e.code() << " - " << e.what();
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Exception detected." );
    }

    LOG(INFO) << "main finished.";
    return 0;
}

