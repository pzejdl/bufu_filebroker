#pragma once

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

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

    void setBaseDirectory(const fs::path& path);
    void setIndexFilePrefix(const std::string& prefix);
    const std::string& getIndexFilePrefix();
    const fs::path getRunDirectory(int runNumber);


    typedef std::vector<bu::FileInfo> files_t;
    
    files_t listFilesInRunDirectory(const std::string& runDirectory, const boost::regex& fileFilter);
}
