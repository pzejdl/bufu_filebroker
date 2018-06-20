#include <boost/range.hpp>      // For boost::make_iterator_range
#include <boost/filesystem.hpp>

#include "bu.h"

namespace fs = boost::filesystem;


/* Temporarily put here, but probably make configurable */
static const fs::path baseDirectory = "/fff/ramdisk";

fs::path bu::getRunDirectory(int runNumber) {
    return baseDirectory / ("run" + std::to_string(runNumber)); 
}

bu::files_t bu::listFilesInRunDirectory(const std::string& runDirectory)
{
    files_t result;

    /* 
     * This will iterate over run directory and return sorted *.jsn.
     * The only exception is EoR (end of the run) file which if detected 
     * is moved to the end of the list.
     */ 
    const std::regex fileFilter( "run.*\\.jsn" );

    // Check if EoR was found
    std::string fileEoR;

    // Iterate over the run directory
    for ( auto&& dirEntry: boost::make_iterator_range(fs::directory_iterator( runDirectory ), {}) ) {
        // With C++17 we don't need boost:make_iterator_range

        std::string fileName = dirEntry.path().filename().string();

        if ( std::regex_match( fileName, fileFilter) ) {
            if ( File::isEoR(fileName) ) {
                fileEoR = fileName;
            } else {
                result.push_back( std::move(fileName) );
            }
        }
    }

    // Sort the files according LS and INDEX numbers
    std::sort(result.begin(), result.end());

    // Put the EoR at the end
    if (!fileEoR.empty()) {
        result.push_back( fileEoR );
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