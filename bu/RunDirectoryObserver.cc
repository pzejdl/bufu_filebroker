#include <thread>

#include "tools/synchronized/queue.h"
#include "bu/FileInfo.h"
#include "bu/RunDirectoryObserver.h"

namespace bu {

RunDirectoryObserver::RunDirectoryObserver(int runNumber) : runNumber(runNumber) {}

RunDirectoryObserver::~RunDirectoryObserver()
{
    std::cout << "DEBUG: RunDirectoryObserver::~RunDirectoryObserver()." << std::endl;
}


std::string RunDirectoryObserver::getStats() const
{
    const char *sep = "  ";
    std::ostringstream os;

    os << "runNumber=" << runNumber << std::endl;
    os << sep << "state=" << runDirectory.state << std::endl;
    os << '\n';
    os << sep << "startup.nbJsnFiles="                      << stats.startup.nbJsnFiles << std::endl;
    os << sep << "startup.nbJsnFilesOptimized="             << stats.startup.nbJsnFilesOptimized << '\n';
    os << sep << "startup.inotify.nbAllFiles="              << stats.startup.inotify.nbAllFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFiles="              << stats.startup.inotify.nbJsnFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFilesDuplicated="    << stats.startup.inotify.nbJsnFilesDuplicated << std::endl;
    os << '\n';
    os << sep << "inotify.nbAllFiles="                      << stats.inotify.nbAllFiles << std::endl;
    os << sep << "inotify.nbJsnFiles="                      << stats.inotify.nbJsnFiles << std::endl;
    os << '\n';
    os << sep << "nbJsnFilesProcessed="                     << stats.nbJsnFilesProcessed << std::endl;
    os << sep << "nbJsnFilesOptimized="                     << stats.nbJsnFilesOptimized << '\n';
    os << '\n';
    os << sep << "queueSizeMax="                            << stats.queueSizeMax << '\n';
    // TODO: NOTE: queue.size() is not protected by lock or memory barrier, therefore we will get a number that is not up-to-date, but it doesn't matter
    //             In principle, we should protect all statistics variables here...
    os << sep << "queueSize="                               << queue.size() << '\n';
    os << '\n';
    if (stats.fuLastPoppedFile.type != FileInfo::FileType::EMPTY) {
        os << sep << "fuLastPoppedFile=\""                         << stats.fuLastPoppedFile.fileName() << "\"\n";
    } else {
                os << sep << "fuLastGivenFile=NONE\n";
    }
    os << '\n';
    os << sep << "lastEoLS="                                << runDirectory.lastEoLS << '\n';

    return os.str();
}


const std::string& RunDirectoryObserver::getError() const
{
    return errorMessage;
}



std::ostream& operator<< (std::ostream& os, RunDirectoryObserver::State state)
{
    switch (state)
    {
        case RunDirectoryObserver::State::INIT:     return os << "INIT" ;
        case RunDirectoryObserver::State::STARTING: return os << "STARTING";
        case RunDirectoryObserver::State::READY:    return os << "READY";
        case RunDirectoryObserver::State::EOLS:     return os << "EOLS";
        case RunDirectoryObserver::State::EOR:      return os << "EOR";
        case RunDirectoryObserver::State::STOP:     return os << "STOP";
        case RunDirectoryObserver::State::ERROR:    return os << "ERROR";
        // Omit default case to trigger compiler warning for missing cases
    };
    return os;
}

} // namespace bu


/*
class RunDirectoryObserver {

public:    

    RunDirectoryObserver(int runNumber) : runNumber_(runNumber) {}
    ~RunDirectoryObserver()
    {
        std::cout << "DEBUG: RunDirectoryObserver: Destructor called" << std::endl;
        running_ = false;
        if (runner_.joinable()) {
            runner_.join();
        }
    }

    RunDirectoryObserver(const RunDirectoryObserver&) = delete;
    RunDirectoryObserver& operator=(const RunDirectoryObserver&) = delete;


    void start() 
    {
        assert( state_ == State::INIT );
        runner_ = std::thread(&RunDirectoryObserver::run, this);
        runner_.detach();
        state_ = State::STARTING;
    }


    void stopRequest()
    {
        running_ = false;    
    }


    State state() const 
    {
        return state_;
    }

    FileNameQueue_t& queue()
    {
        return queue_;
    }

private:

    void run()
    {
        while (running_) {
            std::cout << "RunDirectoryObserver: Alive" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        std::cout << "RunDirectoryObserver: Finished" << std::endl;
        state_ = State::STOP;
    }

private:
    std::atomic<State> state_ { State::INIT };
    std::atomic<bool> running_{ true };

    int runNumber_;
    std::thread runner_ {};

    FileNameQueue_t queue_;
};
*/
