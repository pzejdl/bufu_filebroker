#pragma once

#include <sys/inotify.h>    // For constants
#include <string>
#include <vector>
#include <ostream>


       #include <errno.h>
       #include <poll.h>
       #include <stdio.h>
       #include <stdlib.h>
       #include <sys/inotify.h>
       #include <unistd.h>

#include <cassert>

namespace tools {

    class INotify {

    public:
        struct Event;
        typedef std::vector<Event> Events_t;

        struct Event {
            int         wd;       /* Watch descriptor */
            uint32_t    mask;     /* Mask describing event */
            uint32_t    cookie;   /* Unique cookie associating related events (for rename(2)) */
            std::string name;  /* Optional null-terminated name */

            friend std::ostream& operator<<(std::ostream& os, const Event& e) {
                os << "Event(wd=" << e.wd << ", mask=0x" << std::hex << e.mask << std::dec << ", cookie=" << e.cookie << ", name='" << e.name << "')";
                return os;
            }

        };

    public:
        INotify();
        ~INotify();

        int add_watch(const std::string& pathname, uint32_t mask);

        bool hasEvent()
        {
            assert( fd_ > 0 );
            nfds_t nfds = 1;
            struct pollfd fds = { fd_, POLLIN, 0 };
            int nbPoll;

            while (true) {
                nbPoll = poll(&fds, nfds, 0);
                if (nbPoll < 0) {
                    if (errno == EINTR) {
                        // Interrupted by signal, has to restart
                        continue;
                    }
                    // Error happened
                    throw std::system_error(errno, std::system_category(), "poll");
                }
                break; 
            }
            return (nbPoll > 0) && (fds.revents & POLLIN);
        }

        Events_t read();
        //void read();


        void read_event();

    private:
        int fd_ = -1;
    };
}
