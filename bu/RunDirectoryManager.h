#pragma once

#include <thread>
#include <unordered_map>

#include "bu/RunDirectoryObserver.h"


namespace bu {

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

} // namespace bu