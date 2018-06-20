#include <iostream>
#include <system_error>
#include <regex>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <thread>
#include <atomic>
#include <unordered_map>

#include "tools/inotify/INotify.h"
#include "tools/synchronized/queue.h"
#include "tools/tools.h"
#include "bu.h"

namespace fs = boost::filesystem;

/*
 * Note: boost up to ver 60 doesn't have move semantics for boost::filesystem::path !!!
 */


struct Statistics {
    int item1;  // Something

    tools::synchronized::spinlock lock;
} stats;



//TODO: We have to put queues for directories into map...
//std::map< int, std::string > watched_files_;



/* 
 * The main queue is here.
 * 
 * Actually, we need a SPMS (single producer - multiple consumers) queue,
 * but because I'm lazy and before I find a better solution I use a simple wrapper
 * of std::dequeue. 
 * 
 * Notes about boost: Unfortunately, boost::lockfree::queue cannot be used
 * because it requires trivial assignment operator and trivial destructor.
 * And that std::string doesn't have.
 *
 * TODO: check this: http://moodycamel.com/blog/2014/detailed-design-of-a-lock-free-queue
 */
typedef tools::synchronized::queue<std::string> FileNameQueue_t;


struct RunDirectoryObserver {
    // Note that this class cannot be copied or moved because of queue

    enum class State { INIT, STARTING, READY, STOP };

    friend std::ostream& operator<< (std::ostream& os, RunDirectoryObserver::State state)
    {
        switch (state)
        {
            case RunDirectoryObserver::State::INIT:       return os << "INIT" ;
            case RunDirectoryObserver::State::STARTING:   return os << "STARTING";
            case RunDirectoryObserver::State::READY:      return os << "READY";
            case RunDirectoryObserver::State::STOP:       return os << "STOP";
            // omit default case to trigger compiler warning for missing cases
        };
        return os;
    }

    RunDirectoryObserver(int runNumber) : runNumber(runNumber) {}

    int runNumber;
    FileNameQueue_t queue;

    std::atomic<State> state { State::INIT };
    std::atomic<bool> running { true };
    std::thread runner;
};



typedef std::function<void(RunDirectoryObserver&)> RunDirectoryRunner_t;


class RunDirectoryManager {

public:
    RunDirectoryManager(RunDirectoryRunner_t runner) : runner_(runner) {}

public:
    RunDirectoryObserver& getRunDirectoryObserver(int runNumber)
    {
        {
            std::lock_guard<tools::synchronized::spinlock> lock(runDirectoryObserversLock_);

            // Check if we already have observer for that run
            const auto iter = runDirectoryObservers_.find( runNumber );
            if (iter != runDirectoryObservers_.end()) {
                return iter->second;
            }
        }

        // No, we don't have. We create a new one
        return createRunDirectoryObserver( runNumber );
    }

private:

    void startRunner(RunDirectoryObserver& observer) 
    {
        assert( observer.state == RunDirectoryObserver::State::INIT );
        observer.runner = std::thread(runner_, std::ref(observer));
        observer.runner.detach();
        observer.state = RunDirectoryObserver::State::STARTING;
    }


    RunDirectoryObserver& createRunDirectoryObserver(int runNumber)
    {
        std::unique_lock<tools::synchronized::spinlock> lock(runDirectoryObserversLock_);
        //{
            // Constructs a new runDirectoryObserver in the map directly
            auto emplaceResult = runDirectoryObservers_.emplace(std::piecewise_construct,
                std::forward_as_tuple(runNumber), 
                std::forward_as_tuple(runNumber)
            );
            // Normally, the runNumber was not in the map before, but better be safe
            assert( emplaceResult.second == true );

            auto iter = emplaceResult.first;
            RunDirectoryObserver& observer = iter->second;
        //}
        lock.unlock();

        std::cout << "DEBUG: runDirectoryObserver created for runNumber: " << iter->first << std::endl;

        //TODO: observer.start();
        startRunner(observer);

        return observer;
    }


private:
    RunDirectoryRunner_t runner_;

    std::unordered_map< int, RunDirectoryObserver > runDirectoryObservers_;

    // Would be better to use shared_mutex, but for the moment there is only one reader, so the spinlock is the best
    tools::synchronized::spinlock runDirectoryObserversLock_;    
};


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



// Maps runNumber to FileNameQueue
std::unordered_map< int, FileNameQueue_t > runQueues;
std::mutex runQueuesLock;

// Forward declaration
void directoryObserverRunner(RunDirectoryObserver& observer);

RunDirectoryManager runDirectoryManager { directoryObserverRunner };



void testINotify(const std::string& runDirectory)
{
    tools::INotify inotify;
    inotify.add_watch( runDirectory,
        IN_MODIFY | IN_CREATE | IN_DELETE);

    while (true) {
        for (auto&& event : inotify.read()) {
            std::cout << event;
        }
    }
}




std::atomic<bool> done(false);

void runDirectoryObserver(int runNumber)
{
    THREAD_DEBUG();

    // Add a new queue into runQueues
    auto emplaceResult = runQueues.emplace(std::piecewise_construct,
        std::forward_as_tuple(runNumber), 
        std::forward_as_tuple()
    );
    // Normally, the runNumber is not in the map, but better be safe
    assert( emplaceResult.second == true );

    auto it = emplaceResult.first;
    std::cout << "DEBUG: queue runNumber: " << it->first << std::endl;
    FileNameQueue_t& queue = it->second;



    const fs::path runDirectory = bu::getRunDirectory( runNumber ); 
    std::cout << "Watching: " << runDirectory << std::endl;

    //FileNameQueue_t& queue = runQueues.find(runNumber)->second;
    //assert( runQueues.find(runNumber) == runQueues.end() );

    // List directory
    bu::files_t result = bu::getFilesInRunDirectory( runDirectory.string() );
    std::cout << "runDirectoryObserver found " << result.size() << " files" << std::endl;
    for (auto&& item : result) {
        queue.push( std::move(item) );
    }


    while (!done) {
	    //std::cout << "runDirectoryObserver: alive" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    }
    std::cout << "runDirectoryObserver: Finished" << std::endl;
}



void directoryObserverRunner(RunDirectoryObserver& observer)
{
    while (observer.running) {
        std::cout << "RunDirectoryObserver: Alive" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "RunDirectoryObserver: Finished" << std::endl;
    observer.state = RunDirectoryObserver::State::STOP;
}



//TODO: Split into two functions
bool getFileFromBU(int runNumber, std::string& fileName)
{
    RunDirectoryObserver& observer = runDirectoryManager.getRunDirectoryObserver( runNumber );

    if (observer.state == RunDirectoryObserver::State::READY) {
        FileNameQueue_t& queue = observer.queue;
        return queue.pop(fileName);     
    }

    std::cout << "DEBUG: RunDirectoryObserver " << runNumber << " is not READY, it is " << observer.state << std::endl;
    return false;
}




namespace fu {
    std::atomic<bool> done(false);


    void requester(int runNumber)
    {
        THREAD_DEBUG();

        while (!done) {
            std::string fileName;

            if ( getFileFromBU(runNumber, fileName) ) {
                std::cout << "Requester: " << runNumber << ": " << fileName << std::endl;
            } else {
                std::cout << "Requester: " << runNumber << ": EMPTY" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(700));
            }          
        }
        std::cout << "Requester: Finished" << std::endl;
    }
}


int main()
{
    int runNumber = 1000030354;

    std::cout << boost::format("HOHOHO: %u\n") % 123;

    try {
	    //std::thread runDirectoryObserver( ::runDirectoryObserver, runNumber );
        std::thread requester( fu::requester, runNumber );


        //runDirectoryObserver.detach();
        requester.detach();

        std::this_thread::sleep_for(std::chrono::seconds(3));
        fu::done = true;
	    done = true;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        //runDirectoryObserver.join();
        //requester.join();
    }
    catch (const std::system_error& ex) {
        std::cout << "Error: " << ex.code() << " - " << ex.what() << '\n';
    }

    return 0;
}

