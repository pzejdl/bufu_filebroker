#pragma once

#include <regex>
#include <boost/filesystem.hpp>

#include "bu/FileInfo.h"

namespace fs = boost::filesystem;


namespace bu {
    /*
    * Helper class detecting various file types generated by BU
    */
    struct File {
        static inline bool isEoR(const std::string& file) {
            return file.find("_EoR.jsn") != std::string::npos;
        }
        static inline bool isEoLS(const std::string& file) {
            return file.find("_EoLS.jsn") != std::string::npos;  
        }
    };

    fs::path getRunDirectory(int runNumber);

    typedef std::vector<bu::FileInfo> files_t;
    
    files_t listFilesInRunDirectory(const std::string& runDirectory);
}
