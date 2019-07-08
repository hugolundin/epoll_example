#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 201805L

#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "epoll_example.h"



static int epoll_event_timerfd (
    struct app_s * app,
    struct epoll_event * event
)
{
    int ret = 0;
    
    syslog(LOG_DEBUG,
        "%s:%d:%s: hi! app=%p, event=%p, event->data.fd=%d",
        __FILE__, __LINE__, __func__,
        app, event, event->data.fd
    );

    ret = timerfd_settime(
        /* fd = */ app->timerfd,
        /* opt = */ 0,
        /* timerspec = */ &(struct itimerspec) {
            .it_interval = {0},
            .it_value = {
                .tv_sec = 2,
                .tv_nsec = 0
            }
        },
        /* old_timerspec = */ NULL
    );
    if (-1 == ret) {
        syslog(LOG_ERR,
            "%s:%d:%s: timerfd_settime: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );

        return -1;
    }

    ret = epoll_ctl(
        /* epoll_fd = */ app->epoll_fd,
        /* op = */ EPOLL_CTL_MOD,
        /* fd = */ app->timerfd,
        /* event = */ &(struct epoll_event) {
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = app->timerfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR,
            "%s:%d:%s: epoll_ctl: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );

        return -1;
    }

    return 0;
}

static int epoll_event_dispatch (
    struct app_s * app,
    struct epoll_event * event
)
{
    syslog(LOG_DEBUG,
        "%s:%d:%s: hi! app=%p, event=%p, event->data.fd=%d",
        __FILE__, __LINE__, __func__,
        app, event, event->data.fd
    );

    if (event->data.fd == app->timerfd) {
        return epoll_event_timerfd(app, event);
    }

    syslog(LOG_ERR,
        "%s:%d:%s: no match for epoll event!",
        __FILE__, __LINE__, __func__
    );

    return -1;
}

static int epoll_handle_events (
    struct app_s * app, 
    struct epoll_event epoll_events[EPOLL_NUM_EVENTS],
    int ep_num_events
)
{
    syslog(LOG_DEBUG,
        "%s:%d:%s: hi! app=%p, events=%p, events_len=%d",
        __FILE__, __LINE__, __func__,
        app, epoll_events, ep_num_events
    );

    int ret = 0;
    for (int i = 0; i < ep_num_events; i++) {
        ret = epoll_event_dispatch(app, &epoll_events[i]);
        if (0 != ret) {
            return ret;
        }
    }

    return 0;
}

int main(int argc, char const *argv[])
{
    int ret = 0;
    struct app_s app = {0};

    app.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (-1 == app.epoll_fd) {
        syslog(LOG_ERR,
            "%s:%d:%s: epoll_create1: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );


        exit(EXIT_FAILURE);
    }

    app.timerfd = timerfd_create(
        /* clockid = */ CLOCK_MONOTONIC,
        /* flags = */ TFD_CLOEXEC
    );
    if (-1 == app.timerfd) {
        syslog(LOG_ERR,
            "%s:%d:%s: timerfd_create: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );

        exit(EXIT_FAILURE);
    }

    ret = timerfd_settime(
        /* fd = */ app.timerfd,
        /* opt = */ 0,
        /* timerspec = */
        &(struct itimerspec) {
            .it_interval = {0},
            .it_value = {
                .tv_sec = 2,
                .tv_nsec = 0
            }
        },
        /* old_timerspec = */ NULL
    );
    if (-1 == ret) {
        syslog(LOG_ERR,
            "%s:%d:%s: timerfd_settime: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );

        exit(EXIT_FAILURE);
    }

    ret = epoll_ctl(
        /* epoll_fd = */ app.epoll_fd,
        /* op = */ EPOLL_CTL_ADD,
        /* fd = */ app.timerfd,
        /* event = */ &(struct epoll_event) {
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = app.timerfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR,
            "%s:%d:%s: epoll_ctl: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );

        exit(EXIT_FAILURE);
    }

    syslog(LOG_DEBUG,
        "%s:%d:%s: epoll_wait...",
        __FILE__, __LINE__, __func__
    );

    int ep_num_events = 0;
    struct epoll_event events[EPOLL_NUM_EVENTS];
    for (ep_num_events = epoll_wait(app.epoll_fd, events, EPOLL_NUM_EVENTS, -1);
         ep_num_events > 0;
         ep_num_events = epoll_wait(app.epoll_fd, events, EPOLL_NUM_EVENTS, -1))
    {
        ret = epoll_handle_events(&app, events, ep_num_events);
        if (-1 == ret) {
            break;
        }

        syslog(LOG_DEBUG,
            "%s:%d:%s: epoll_wait...",
            __FILE__, __LINE__, __func__
        );  
    }
    if (-1 == ep_num_events) {
        syslog(LOG_DEBUG,
            "%s:%d:%s: epoll_wait: %s",
            __FILE__, __LINE__, __func__,
            strerror(errno)
        );
        
        exit(EXIT_FAILURE);
    }

    syslog(LOG_DEBUG,
        "%s:%d:%s: bye!",
        __FILE__, __LINE__, __func__
    );
    return 0;
}
