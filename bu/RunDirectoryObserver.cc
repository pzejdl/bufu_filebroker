#include <thread>

#include "tools/synchronized/queue.h"
#include "bu/FileInfo.h"
#include "bu/RunDirectoryObserver.h"

namespace bu {

RunDirectoryObserver::RunDirectoryObserver(int runNumber) : runNumber(runNumber) {}

std::string RunDirectoryObserver::getStats() const
{
    const char *sep = "  ";
    std::ostringstream os;

    os << "runNumber=" << runNumber << std::endl;
    os << sep << "state=" << runDirectory.state << std::endl;
    os << std::endl;
    os << sep << "startup.nbJsnFiles="                      << stats.startup.nbJsnFiles << std::endl;
    os << sep << "startup.inotify.nbAllFiles="              << stats.startup.inotify.nbAllFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFiles="              << stats.startup.inotify.nbJsnFiles << std::endl;
    os << sep << "startup.inotify.nbJsnFilesDuplicated="    << stats.startup.inotify.nbJsnFilesDuplicated << std::endl;
    os << std::endl;
    os << sep << "inotify.nbAllFiles="                      << stats.inotify.nbAllFiles << std::endl;
    os << sep << "inotify.nbJsnFiles="                      << stats.inotify.nbJsnFiles << std::endl;
    os << std::endl;
    os << sep << "nbJsnFilesQueued="                        << stats.nbJsnFilesQueued << std::endl;
    os << std::endl;
    os << sep << "lastEoLS="                                << runDirectory.lastEoLS << std::endl;
    os << sep << "lastIndex="                               << runDirectory.lastIndex << std::endl;

    return os.str();
}

std::ostream& operator<< (std::ostream& os, RunDirectoryObserver::State state)
{
    switch (state)
    {
        case RunDirectoryObserver::State::INIT:     return os << "INIT" ;
        case RunDirectoryObserver::State::STARTING: return os << "STARTING";
        case RunDirectoryObserver::State::READY:    return os << "READY";
/*
        case RunDirectoryObserver::State::EOLS:     return os << "EOLS";
        case RunDirectoryObserver::State::EOR:      return os << "EOR";
*/
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
