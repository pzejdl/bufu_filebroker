#pragma once

#include <sys/inotify.h>    // For constants
#include <string>
#include <vector>
#include <ostream>

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

        Events_t read();
        //void read();


        void read_event();

    private:
        int fd_;
    };
}
