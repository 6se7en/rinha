#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "lb_shared.h"


#define LISTEN_PORT    80
#define MAX_EVENTS     1024
#define NUM_UPSTREAMS  2

static const char *backend_socks[NUM_UPSTREAMS] = {
    "/sockets/" LB_TO_APP_ONE_SOCK,
    "/sockets/" LB_TO_APP_TWO_SOCK,
};

static int backend_ctrl_fds[NUM_UPSTREAMS];
static unsigned next_upstream = 0;

static void die(const char *m) { perror(m); _exit(1); }

static int connect_backend(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) die("socket unix");

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    for (;;) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) return fd;
        if (errno == ENOENT || errno == ECONNREFUSED) {
            
            usleep(100 * 1000);
            continue;
        }
        die("connect unix");
    }
}

static int create_listener(void) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) die("socket");

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
    setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &one, sizeof(one));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(LISTEN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(fd, 65535) < 0) die("listen");
    return fd;
}

static int try_handoff_fd(int ctrl, int client_fd, int flags) {
    char dummy = 'F';
    struct iovec iov = { .iov_base = &dummy, .iov_len = 1 };

    union {
        struct cmsghdr cmh;
        char buf[CMSG_SPACE(sizeof(int))];
    } u;
    memset(&u, 0, sizeof(u));

    struct msghdr m = {
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = u.buf,
        .msg_controllen = sizeof(u.buf),
    };
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&m);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &client_fd, sizeof(int));

    for (;;) {
        ssize_t n = sendmsg(ctrl, &m, MSG_NOSIGNAL | flags);
        if (n >= 0) return 0;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        perror("sendmsg SCM_RIGHTS");
        return -1;
    }
}

static void accept_loop(int lfd) {
    for (;;) {
        int cfd = accept4(lfd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            if (errno == EINTR) continue;
            return;
        }

        int one = 1;
        setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY,  &one, sizeof(one));
        setsockopt(cfd, IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));

        unsigned first = next_upstream++ & (NUM_UPSTREAMS - 1);
        int sent = 0;
        for (unsigned off = 0; off < NUM_UPSTREAMS; off++) {
            unsigned target = (first + off) & (NUM_UPSTREAMS - 1);
            int r = try_handoff_fd(backend_ctrl_fds[target], cfd, MSG_DONTWAIT);
            if (r == 0) { sent = 1; break; }
            if (r < 0)  { break; }  
        }
        if (!sent) {
            (void)try_handoff_fd(backend_ctrl_fds[first], cfd, 0);
        }
        close(cfd);  
    }
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    for (int i = 0; i < NUM_UPSTREAMS; i++) {
        backend_ctrl_fds[i] = connect_backend(backend_socks[i]);
        fprintf(stderr, "lb: connected to %s (fd %d)\n",
                backend_socks[i], backend_ctrl_fds[i]);
    }

    int lfd  = create_listener();
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) die("epoll_create1");

    struct epoll_event ee = { .events = EPOLLIN, .data.fd = lfd };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ee) < 0) die("epoll_ctl");

    fprintf(stderr, "lb: listening on :%d\n", LISTEN_PORT);

    struct epoll_event evs[MAX_EVENTS];
    for (;;) {
        int n = epoll_wait(epfd, evs, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            die("epoll_wait");
        }
        for (int i = 0; i < n; i++) {
            if (evs[i].data.fd == lfd) accept_loop(lfd);
        }
    }
}
