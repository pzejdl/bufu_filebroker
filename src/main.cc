#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "bu/RunDirectoryManager.h"
#include "http/1.1/server/server.hpp"

#include "tools/tools.h"
#include "tools/log.h"
#include "tools/time.h"

#include "config.h"

namespace po = boost::program_options;


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

#define EXISTS(b)   (b ? "yes" : "NO !!!")

void diagnoseRenameFailure(const std::string& fileName, const std::string& filePrefix, const fs::path& runDirectoryPath, const fs::path& fileFrom, const fs::path fileTo)
{
    LOG(DEBUG) << "---- DIAGNOSE ----";
    try {
        LOG(DEBUG) << "fileName         = \"" << fileName << '\"';
        LOG(DEBUG) << "filePrefix       = \"" << filePrefix << '\"';
        LOG(DEBUG) << "runDirectoryPath = " << runDirectoryPath << ", exists = " << EXISTS(fs::is_directory(runDirectoryPath));
        LOG(DEBUG) << "fileFrom         = " << fileFrom << ", exists = " << EXISTS(fs::is_regular_file(fileFrom));
        LOG(DEBUG) << "fileTo           = " << fileTo;
        fs::path fuDirectory = fileTo.parent_path();
        LOG(DEBUG) << "fuDirectory      = " << fuDirectory << ", exists = " << EXISTS(fs::is_directory(fuDirectory));
    }
    catch (fs::filesystem_error& e) {
        LOG(ERROR) << "DIAGNOSE failed with: " << e.what();
    }
}

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
            diagnoseRenameFailure( fileName, filePrefix, runDirectoryPath, fileFrom, fileTo);
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


/*
 * The web application(s) are defined here
 */
void createWebApplications(http_server::request_handler& app)
{
    app.add("/popfile",
    [](const http_server::request_t& req, http_server::response_t& res)
    {
        res.set(http::field::content_type, "text/plain");
        res.body().append("version=\"" BUFU_FILEBROKER_VERSION "\"\n");

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
        //TODO: HACK: Raw file mode is hardcoded
        bu::RunDirectoryObserver::FileMode fileMode = bu::RunDirectoryObserver::FileMode::RAW;
        int lastEoLS;

        std::tie( file, state, lastEoLS ) = runDirectoryManager.popRunFile( runNumber, stopLS );

        std::string fileExtension;
        // Rename the file before it is given to FU
        if (file.type != bu::FileInfo::FileType::EMPTY) { 
            // TODO: Make file rename it optional
            switch (fileMode) {
                case bu::RunDirectoryObserver::FileMode::JSN:   fileExtension = ".jsn"; break;
                case bu::RunDirectoryObserver::FileMode::RAW:   fileExtension = ".raw"; break;
            }
            renameIndexFile( runNumber, filePrefix, file.fileName() + fileExtension );
        }

        os << "runnumber="  << runNumber << '\n';
        os << "filemode="   << fileMode << '\n';
        os << "state="      << state << '\n';

        if (state == bu::RunDirectoryObserver::State::ERROR || state == bu::RunDirectoryObserver::State::NORUN) {
            os << "errormessage=\"" << runDirectoryManager.getError( runNumber ) << "\"\n";
        }

        if (file.type != bu::FileInfo::FileType::EMPTY) { 
            assert( (uint32_t)runNumber == file.runNumber );
            os << "file=\""         << file.fileName() << "\"\n";
            os << "fileprefix=\""   << filePrefix << "\"\n";
            os << "fileextension=\""<< fileExtension << "\"\n";
            os << "lumisection="    << file.lumiSection << '\n';
            os << "index="          << file.index << '\n';
        } else { 
            os << "lumisection="    << lastEoLS << '\n';
        }
        os << "lasteols="           << lastEoLS << '\n';


        // TODO: DEBUG Make this optional
        if (false) { 
            std::cout << tools::time::localtime() << ' ';

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
        res.body().append("version=\"" BUFU_FILEBROKER_VERSION "\"\n");

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

        res.body().append("version=\"" BUFU_FILEBROKER_VERSION "\"\n");

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
            << "<head><title>BUFU File Broker</title></head>\n"
            << "<body>\n"
            << "<h1>BUFU File Broker is alive!</h1>\n"
            << "<p>v" BUFU_FILEBROKER_VERSION "</p>\n"
            << "<p>" << std::asctime(std::localtime( &time )) << "</p>\n"
            << "</body>\n"
            << "</html>\n";

        res.body() = os.str();
    });


    app.add("/restart",
    [](const http_server::request_t& req, http_server::response_t& res)
    {
        res.set(http::field::content_type, "text/plain");
        res.body().append("version=\"" BUFU_FILEBROKER_VERSION "\"\n");

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


void serverRunner(http_server::server& s)
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

int main(int argc, char *argv[])
{
    LOG(INFO) << "BUFU File Broker v" << BUFU_FILEBROKER_VERSION;

    std::string address;
    std::string port;
    int nbThreads;
    std::string docRoot; 
    std::string indexFilePrefix;
    bool debugHTTPRequests = false;

    try {
        po::options_description desc("Options (default values are in brackets)");
        desc.add_options()
            ("help,h", "this help message.")
            ("bind", po::value<std::string>(&address)->default_value("0.0.0.0"), "bind to a specific address.")
            ("port", po::value<std::string>(&port)->default_value("8080"), "listen on a port.")
            ("threads", po::value<int>(&nbThreads)->default_value(1), "number of threads serving HTTP requests.")
            ("docroot", po::value<std::string>(&docRoot)->default_value("/fff/ramdisk"), "path from where the files are served.")
            ("index-file-prefix", po::value<std::string>(&indexFilePrefix)->default_value("fu/"), "file prefix used when index files are renamed.")
            ("debug-http-requests", po::bool_switch(&debugHTTPRequests), "print debug information when HTTP request is received.")
        ;

        po::variables_map vm;        
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);    

        if (vm.count("help")) {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << desc << '\n';
            return 0;
        }

        bu::setBaseDirectory( docRoot );
        bu::setIndexFilePrefix( indexFilePrefix );
    }
    catch(std::exception& e) {
        LOG(ERROR) << "ERROR: " << e.what();
        return 1;
    }

    // Initialise the server.
    // Note: docRoot is not used here
    http_server::server s(address, port, docRoot, nbThreads, debugHTTPRequests);

    // Add handlers
    createWebApplications( s.request_handler() );

    LOG(INFO) << "Server: Starting HTTP server with " << nbThreads << " thread(s) at " << address << ':' << port << docRoot << " and using " << indexFilePrefix << " as index file prefix."; 

    try {
        std::thread serverThread( serverRunner, std::ref(s) );
        serverThread.join();
    }
    catch (const std::system_error& e) {
        LOG(ERROR) << "ERROR: " << e.code() << " - " << e.what();
    }
    catch(const std::exception& e) {
        BACKTRACE_AND_RETHROW( std::runtime_error, "Exception detected." );
    }

    LOG(INFO) << "main finished.";
    return 0;
}

