#pragma once

#define EPOLL_NUM_EVENTS 8

struct app_s {
    int epoll_fd;
    int timerfd;
};

