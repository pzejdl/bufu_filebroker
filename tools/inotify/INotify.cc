/*
 * Very simple inotify wrapper
 * 
 * TODO: Make it more c++
 * 
 * If necessary, polling code can use: http://man7.org/linux/man-pages/man7/inotify.7.html
 * 
 */ 

#include <iostream>
#include <cerrno>
#include <system_error>
#include <unistd.h>           // read,close

#include "INotify.h"


tools::INotify::INotify()
{
    fd_ = ::inotify_init();
    if (fd_ < 0) {
        throw std::system_error(errno, std::system_category(), "inotify_init");
    }
}


tools::INotify::~INotify()
{
    std::cerr << "INotify: Closing" << std::endl;
    ::close(fd_);     
}


int tools::INotify::add_watch(const std::string& pathname, uint32_t mask)
{
    int wd;
    if ( (wd = inotify_add_watch(fd_, pathname.c_str(), mask)) < 0 ) {
        throw std::system_error(errno, std::system_category(), "inotify_add_watch");
    }
    return wd;
}


#define TODO    { printf("NOT IMPLEMENTED\n"); exit(-1); }

/* 
 * TODO: 
 * - Fix EINTR
 * - Better would be iterator
 * - Move buffer to the class
 * - Use emplace insted of push_back, interessting would be to have a look at the assembly code 
 * - Detect IN_Q_OVERFLOW
 * - Turn perrors into exceptions
 */
tools::INotify::Events_t tools::INotify::read()
{
    Events_t events;

    /* Some systems cannot read integer variables if they are not
       properly aligned. On other systems, incorrect alignment may
       decrease performance. Hence, the buffer used for reading from
       the inotify file descriptor should have the same alignment as
       struct inotify_event. */

    char buf[4096]
        __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event* event;
    ssize_t len;
    char* ptr;

    /* Loop while events can be read from inotify file descriptor. */

    /* Read some events. */

    len = ::read(fd_, buf, sizeof buf);
    if (len == -1 && errno != EAGAIN) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    /* If the nonblocking read() found no events to read, then
           it returns -1 with errno set to EAGAIN. In that case,
           we exit the loop. */

    if (len <= 0) {
        TODO;
    }

    /* Loop over all events in the buffer */

    for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {

        event = (const struct inotify_event*) ptr;

        tools::INotify::Event ev = { event->wd, event->mask, event->cookie, ( (event->len > 0) ? event->name : "") };
        //events.emplace( ev );
        events.push_back( std::move(ev) );
    }

    return events;
}



void tools::INotify::read_event()
{
    /* size of the event structure, not counting name */
    #define EVENT_SIZE      (sizeof (struct inotify_event))

    /* reasonable guess as to size of 1024 events */
    #define BUF_LEN         (1024 * (EVENT_SIZE + 16))

    char buf[BUF_LEN];
    int len, i = 0;

    len = ::read (fd_, buf, BUF_LEN);
    if (len < 0) {
        if (errno == EINTR) {
            /* need to reissue system call */
            TODO;
        } else {
            perror ("read");
            exit(-1);
        }
    } else if (!len) {
        /* BUF_LEN too small? */
        TODO;
    }

    while (i < len) {
        struct inotify_event *event;

        event = (struct inotify_event *) &buf[i];

        printf ("wd=%d mask=%u cookie=%u len=%u\n",
                event->wd, event->mask,
                event->cookie, event->len);

        if (event->len) {
                printf ("name=%s\n", event->name);
        }

        i += EVENT_SIZE + event->len;
    }
}
