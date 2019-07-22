//#include <regex>
#include <boost/regex.hpp>


#include <boost/range.hpp>      // For boost::make_iterator_range
#include <boost/filesystem.hpp>

#include "bu.h"

namespace fs = boost::filesystem;

static fs::path baseDirectory = "/fff/ramdisk";
static std::string indexFilePrefix = "fu/";

void bu::setBaseDirectory(const fs::path& path)
{
    baseDirectory = path;
}

void bu::setIndexFilePrefix(const std::string& prefix) {
    fs::path prefixPath = prefix;
    fs::path path = prefixPath / "f";
    indexFilePrefix = prefix;
}

const std::string& bu::getIndexFilePrefix() {
    return indexFilePrefix;
}

const fs::path bu::getRunDirectory(int runNumber) {
    return baseDirectory / ("run" + std::to_string(runNumber)); 
}


/* 
 * This will iterate over run directory and return files matching regular expressing in fileFilter.
 */
bu::files_t bu::listFilesInRunDirectory(const std::string& runDirectory, const boost::regex& fileFilter)
{
    files_t result;

    try {
        // Iterate over the run directory
        for ( const auto& dirEntry: boost::make_iterator_range(fs::directory_iterator( runDirectory ), {}) ) {
            // With C++17 we don't need boost:make_iterator_range

            const std::string fileName = std::move( dirEntry.path().filename().string() );

            if ( boost::regex_match( fileName, fileFilter) ) {
                bu::FileInfo file = bu::temporary::parseFileName( fileName.c_str() );
                std::cout << fileName << " : " << file << '\n';

                // Consistency check for the moment
                //assert( fileName == (file.fileName() + ".jsn") );
                //TODO: HACK: Allow raw files
                assert( fileName == (file.fileName() + ".jsn") || fileName == (file.fileName() + ".raw") );


                result.push_back( std::move(file) );
            }
        }
    }
    catch (const std::exception &e) {
        RETHROW( std::runtime_error, "Error during directory listing: '" + runDirectory + "'.");
    }

    // typedef std::vector<fs::path> vec;             // store paths,
    // vec v;                                // so we can sort them later
    //
    // std::copy_if(fs::directory_iterator(path), fs::directory_iterator(), std::back_inserter(v), [&fileFilter](fs::directory_entry& entry) {
    //     return std::regex_match( entry.path().filename().string(), fileFilter);
    // });

    //while (dirIt != end) {
    //    std::cout << "   " << *dirIt++ << '\n';
    //}

    return result;
}