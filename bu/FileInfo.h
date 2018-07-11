#pragma once
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cassert>
#include <stdexcept>

#include "tools/exception.h"

namespace bu {

    struct FileInfo {
        enum class FileType : uint32_t { INDEX, EOLS, EOR, EMPTY };

        friend std::ostream& operator<< (std::ostream& os, const FileInfo::FileType type)
        {
            switch (type) {
                case FileInfo::FileType::INDEX: return os << "index" ;
                case FileInfo::FileType::EOLS:  return os << "EoLS";
                case FileInfo::FileType::EOR:   return os << "EoR";

                case FileInfo::FileType::EMPTY: THROW(std::domain_error, "FileType is not initialized (is EMPTY).");
                // Omit default case to trigger compiler warning for missing cases
            };
            return os;
        }

        // Creates empty object so a proper one can be moved in later
        FileInfo() : type(FileType::EMPTY) {}

        // Create an index file
        FileInfo(uint32_t runNumber, uint32_t lumiSection, uint32_t index)
            : runNumber(runNumber), lumiSection(lumiSection), index(index), type(FileType::INDEX) {}

        // Create a meta file
        FileInfo(uint32_t runNumber, uint32_t lumiSection, FileType type)
            : runNumber(runNumber), lumiSection(lumiSection), index(0), type(type) {}    


        bool isEoLS() const {
            return type == FileType::EOLS;
        }

        bool isEoR() const {
            return type == FileType::EOR;
        }

        /*
         * Note: A filename returned by this method is re-constructed from information contained in this object,
         *       it is not the original filename. But it must have the same name.
         */
        std::string fileName() const {
            if (type == FileType::EMPTY) {
                return "";
            } else {
                std::ostringstream os;
                // From Remi's DiskWriter.cc
                os << std::setfill('0') <<
                    "run"<< std::setw(6) << runNumber <<
                    "_ls" << std::setw(4) << lumiSection <<
                    '_' << type;
                if (type == FileType::INDEX) {
                    os << std::setw(6) << index;        
                }     
                return os.str();
            }
        }

        friend std::ostream& operator<<(std::ostream& os, const FileInfo file) {
            os  << "FIleInfo(" 
                << "filename='" << file.fileName()
                << "', runNumber=" << file.runNumber 
                << ", lumiSection=" << file.lumiSection 
                << ", index=" << file.index
                << ", type=" << file.type
                << ')';
            return os;
        }

        inline bool operator<(const FileInfo& other) const {
            #define SCORE(o)    ( (uint64_t) ( \
                                ((uint64_t)(o).lumiSection << 32) | (uint64_t)(o).index | \
                                ((o).isEoLS() ? 0x80000000 : 0) | \
                                ((o).isEoR()  ? 0x8000000000000000 : 0) \
                                ) )

            // It must never happen that we are comparing files with different run numbers 
            assert( runNumber == other.runNumber );

            return SCORE( *this ) < SCORE( other );
        }


        // Keeping it aligned to 16 bytes
        uint32_t runNumber;
        uint32_t lumiSection;
        uint32_t index;
        FileType type;    
    };

    // Operator for std::sort
    //inline bool operator< (const FileInfo& l, const FileInfo& r) {}

    // Operator for std::find
    inline bool operator==(const FileInfo& l, const FileInfo& r) {
        return ( 
            l.index == r.index &&
            l.lumiSection == r.lumiSection &&
            l.runNumber == r.runNumber &&
            l.type == r.type
        );
    }

    struct temporary {
        // TODO: Move this function somewhere else (FileInfo.cc), or actually make a constructor 
        /*
        * Parses a filename.
        * Returns: FileInfo object
        * NOTE: Can throw std::runtime_error
        */
        static FileInfo parseFileName(const char* fileName)
        {
            // Examples:
            //   run1000030354_ls0000_EoR.jsn
            //   run1000030354_ls0017_EoLS.jsn
            //   run1000030348_ls0511_index020607.jsn

            uint32_t run = 0;
            uint32_t ls = 0;
            uint32_t index = 0;

            int found = std::sscanf(fileName, "run%d_ls%d_index%d", &run, &ls, &index);

            if (found == 3) {
                // Only index file has three parts
                return FileInfo(run, ls, index);
            } 
            
            if (found == 2) {
                // This can be either EoR or EoLS file only

                if (std::strstr(fileName, "EoR") != nullptr) {
                    return FileInfo(run, ls, FileInfo::FileType::EOR);      
                }
                if (std::strstr(fileName, "EoLS") != nullptr) {
                    return FileInfo(run, ls, FileInfo::FileType::EOLS);      
                }
            }

            // Unexpected error during parsing occurred
            std::ostringstream os;
            os  << "sscanf error when parsing '" << fileName << "', return value is " << found 
                << ", returned parts are [" << run << ", " << ls << ", " << index << "].";
            THROW(std::runtime_error, os.str());    
        }
    };
};
