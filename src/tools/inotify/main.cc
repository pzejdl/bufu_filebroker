#include <iostream>
#include <system_error>

#include "INotify.h"

void run()
{
    tools::INotify inotify;
    inotify.add_watch( "/afs/cern.ch/user/p/pzejdl/src/bufu_fileserver/tools/inotify/test",
        IN_MODIFY | IN_CREATE | IN_DELETE);

    while (true) {
        //inotify.read_event();
        for (auto&& event: inotify.read()) {
            /* Print event type */

            if (event.mask & IN_OPEN)
                std::cout << "IN_OPEN: ";
            if (event.mask & IN_CLOSE_NOWRITE)
                std::cout << "IN_CLOSE_NOWRITE: ";
            if (event.mask & IN_CLOSE_WRITE)
                std::cout << "IN_CLOSE_WRITE: ";

            /* Print type of filesystem object */

            if (event.mask & IN_ISDIR)
                std::cout << "[directory] ";
            else
                std::cout << "[file] ";            

            std::cout << event << std::endl;

        }
    }
}

int main()
{
    try {
        run();
    }
    catch (const std::system_error& ex) {
        std::cout << "Error: " << ex.code() << " - " << ex.what() << '\n';
    }

    return 0;
}
