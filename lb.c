#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


#define LISTEN_PORT     80
#define MAX_EVENTS      1024
#define BUF_SIZE        (16 * 1024)
#define POOL_SIZE       512
#define NUM_UPSTREAMS   2
#define BUSY_POLL_US    50


#ifndef SO_BUSY_POLL
#define SO_BUSY_POLL 46
#endif
#ifndef SO_PREFER_BUSY_POLL
#define SO_PREFER_BUSY_POLL 69
#endif

static const char *upstream_paths[NUM_UPSTREAMS] = {
    "/sockets/one.sock",
    "/sockets/two.sock",
};



typedef struct {
    char  buf[BUF_SIZE];
    int   rpos;      
    int   wpos;      
    bool  src_eof;
} Chan;

typedef struct Pair Pair;
struct Pair {
    int  client_fd;
    int  upstream_fd;
    int  upstream_idx;
    Chan c2u;    
    Chan u2c;    
};

typedef struct {
    int fds[POOL_SIZE];
    int count;
} ConnPool;

static ConnPool pools[NUM_UPSTREAMS];
static int epfd = -1;
static int next_upstream = 0;



static void die(const char *msg) {
    perror(msg);
    _exit(1);
}


static void chan_fill(Chan *ch, int fd) {
    if (ch->rpos > 0) {
        int len = ch->wpos - ch->rpos;
        if (len > 0) memmove(ch->buf, ch->buf + ch->rpos, len);
        ch->wpos = len;
        ch->rpos = 0;
    }
    while (ch->wpos < BUF_SIZE) {
        ssize_t n = read(fd, ch->buf + ch->wpos, BUF_SIZE - ch->wpos);
        if (n > 0) { ch->wpos += n; continue; }
        if (n == 0) ch->src_eof = true;
        return;
    }
}


static void chan_drain(Chan *ch, int fd) {
    while (ch->rpos < ch->wpos) {
        ssize_t n = write(fd, ch->buf + ch->rpos, ch->wpos - ch->rpos);
        if (n > 0) { ch->rpos += n; continue; }
        return;
    }
    ch->rpos = ch->wpos = 0;
}

static inline int chan_pending(const Chan *ch) { return ch->wpos - ch->rpos; }



static int connect_upstream(int idx) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, upstream_paths[idx], sizeof(sa.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        if (errno != EINPROGRESS) { close(fd); return -1; }
    }
    return fd;
}

static int pool_get(int idx) {
    ConnPool *p = &pools[idx];
    while (p->count > 0) {
        int fd = p->fds[--p->count];
        char tmp;
        ssize_t r = recv(fd, &tmp, 1, MSG_PEEK | MSG_DONTWAIT);
        if (r == 0) { close(fd); continue; }
        if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { close(fd); continue; }
        return fd;
    }
    return connect_upstream(idx);
}

static void pool_put(int idx, int fd) {
    ConnPool *p = &pools[idx];
    if (p->count < POOL_SIZE)
        p->fds[p->count++] = fd;
    else
        close(fd);
}

static void pool_prefill(void) {
    for (int i = 0; i < NUM_UPSTREAMS; i++)
        for (int j = 0; j < 64; j++) {
            int fd = connect_upstream(i);
            if (fd >= 0) pools[i].fds[pools[i].count++] = fd;
        }
}



#define TAG_CLIENT   0
#define TAG_UPSTREAM 1
#define MAKE_PTR(pair, tag) ((void*)((uintptr_t)(pair) | (tag)))
#define GET_PAIR(ptr)       ((Pair*)((uintptr_t)(ptr) & ~(uintptr_t)1))
#define GET_TAG(ptr)        ((int)((uintptr_t)(ptr) & 1))

static void pair_close(Pair *p) {
    if (p->client_fd >= 0) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, p->client_fd, NULL);
        close(p->client_fd);
    }
    if (p->upstream_fd >= 0) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, p->upstream_fd, NULL);
        if (!p->u2c.src_eof)
            pool_put(p->upstream_idx, p->upstream_fd);
        else
            close(p->upstream_fd);
    }
    free(p);
}

static void epoll_rearm(Pair *p) {
    uint32_t cev = EPOLLET;
    uint32_t uev = EPOLLET;

    if (!p->c2u.src_eof)           cev |= EPOLLIN;
    if (chan_pending(&p->u2c) > 0) cev |= EPOLLOUT;

    if (!p->u2c.src_eof)           uev |= EPOLLIN;
    if (chan_pending(&p->c2u) > 0) uev |= EPOLLOUT;

    struct epoll_event ee;
    ee.events = cev;
    ee.data.ptr = MAKE_PTR(p, TAG_CLIENT);
    epoll_ctl(epfd, EPOLL_CTL_MOD, p->client_fd, &ee);

    ee.events = uev;
    ee.data.ptr = MAKE_PTR(p, TAG_UPSTREAM);
    epoll_ctl(epfd, EPOLL_CTL_MOD, p->upstream_fd, &ee);
}



static void handle_event(uint32_t ev, void *ptr) {
    Pair *p = GET_PAIR(ptr);
    int tag  = GET_TAG(ptr);

    if (tag == TAG_CLIENT) {
        if (ev & EPOLLIN)             chan_fill(&p->c2u, p->client_fd);
        if (ev & EPOLLOUT)            chan_drain(&p->u2c, p->client_fd);
        if (ev & (EPOLLERR|EPOLLHUP)) p->c2u.src_eof = true;
    } else {
        if (ev & EPOLLIN)             chan_fill(&p->u2c, p->upstream_fd);
        if (ev & EPOLLOUT)            chan_drain(&p->c2u, p->upstream_fd);
        if (ev & (EPOLLERR|EPOLLHUP)) p->u2c.src_eof = true;
    }

    if (chan_pending(&p->c2u) > 0) chan_drain(&p->c2u, p->upstream_fd);
    if (chan_pending(&p->u2c) > 0) chan_drain(&p->u2c, p->client_fd);

    bool c2u_done = p->c2u.src_eof && chan_pending(&p->c2u) == 0;
    bool u2c_done = p->u2c.src_eof && chan_pending(&p->u2c) == 0;
    if (c2u_done && u2c_done) {
        pair_close(p);
        return;
    }

    epoll_rearm(p);
}



static int create_listener(void) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) die("socket");

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
    int busy_us = BUSY_POLL_US;
    setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_us, sizeof(busy_us));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(LISTEN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(fd, 16384) < 0) die("listen");
    return fd;
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
        int busy_us = BUSY_POLL_US;
        setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY,         &one,     sizeof(one));
        setsockopt(cfd, SOL_SOCKET,  SO_BUSY_POLL,        &busy_us, sizeof(busy_us));
        setsockopt(cfd, SOL_SOCKET,  SO_PREFER_BUSY_POLL, &one,     sizeof(one));

        int idx = next_upstream;
        next_upstream = (next_upstream + 1) % NUM_UPSTREAMS;

        int ufd = pool_get(idx);
        if (ufd < 0) {
            idx = (idx + 1) % NUM_UPSTREAMS;
            ufd = pool_get(idx);
            if (ufd < 0) { close(cfd); continue; }
        }

        Pair *p = malloc(sizeof(Pair));
        if (!p) { close(cfd); close(ufd); continue; }
        *p = (Pair){
            .client_fd    = cfd,
            .upstream_fd  = ufd,
            .upstream_idx = idx,
        };

        struct epoll_event ee;
        ee.events = EPOLLIN | EPOLLET;
        ee.data.ptr = MAKE_PTR(p, TAG_CLIENT);
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ee) < 0) { pair_close(p); continue; }

        ee.events = EPOLLIN | EPOLLET;
        ee.data.ptr = MAKE_PTR(p, TAG_UPSTREAM);
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, ufd, &ee) < 0) { pair_close(p); continue; }
    }
}



int main(void) {
    signal(SIGPIPE, SIG_IGN);
    memset(pools, 0, sizeof(pools));

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) die("epoll_create1");

    int lfd = create_listener();
    fprintf(stderr, "lb: listening on :%d\n", LISTEN_PORT);

    pool_prefill();

    struct epoll_event ee = {.events = EPOLLIN, .data.ptr = NULL};
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ee) < 0) die("epoll_ctl ADD lfd");

    struct epoll_event evs[MAX_EVENTS];
    for (;;) {
        int n = epoll_wait(epfd, evs, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            die("epoll_wait");
        }
        for (int i = 0; i < n; i++) {
            if (evs[i].data.ptr == NULL)
                accept_loop(lfd);
            else
                handle_event(evs[i].events, evs[i].data.ptr);
        }
    }
}
