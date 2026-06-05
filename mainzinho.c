#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "logs.h"
#include "errors.h"
#include "handler.h"
#include "obj_pool.h"
#include "lb_shared.h"


#define MAX_FD       65536
#define MAX_EVENTS   1024
#define BUF_SIZE     2048



#define HTTP_200_JSON(_str, _cl) \
     "HTTP/1.1 200 OK\r\n" \
     "Content-Length: " #_cl "\r\n" \
     "\r\n" _str

#define NUM_FRAUD_RESPS 6
static const char *fraud_resps[NUM_FRAUD_RESPS] = {
    HTTP_200_JSON("{\"approved\":true,\"fraud_score\":0.0}",  35),
    HTTP_200_JSON("{\"approved\":true,\"fraud_score\":0.2}",  35),
    HTTP_200_JSON("{\"approved\":true,\"fraud_score\":0.4}",  35),
    HTTP_200_JSON("{\"approved\":false,\"fraud_score\":0.6}", 36),
    HTTP_200_JSON("{\"approved\":false,\"fraud_score\":0.8}", 36),
    HTTP_200_JSON("{\"approved\":false,\"fraud_score\":1.0}", 36),
};

static const char *empty_200_alive =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

static const char *empty_500 =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

#define RESP_IDX_ALIVE  NUM_FRAUD_RESPS
#define RESP_IDX_ERR    (NUM_FRAUD_RESPS + 1)
#define NUM_RESP_KINDS  (NUM_FRAUD_RESPS + 2)

static const char *resp_data[NUM_RESP_KINDS];
static size_t      resp_lens[NUM_RESP_KINDS];

static void init_responses(void) {
    for (int i = 0; i < NUM_FRAUD_RESPS; i++) {
        resp_data[i] = fraud_resps[i];
        resp_lens[i] = strlen(fraud_resps[i]);
    }
    resp_data[RESP_IDX_ALIVE] = empty_200_alive;
    resp_lens[RESP_IDX_ALIVE] = strlen(empty_200_alive);
    resp_data[RESP_IDX_ERR]   = empty_500;
    resp_lens[RESP_IDX_ERR]   = strlen(empty_500);
}

static bool send_response(int fd, int idx) {
    const char *p   = resp_data[idx];
    size_t      rem = resp_lens[idx];
    while (rem > 0) {
        ssize_t n = send(fd, p, rem, MSG_NOSIGNAL);
        if (n > 0) { p += n; rem -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}



typedef struct {
    int      fd;
    uint16_t wpos;
    uint8_t  buf[BUF_SIZE];
} Client;

static Client  *client_by_fd[MAX_FD];
static ObjPool  client_pool;
static int      epfd;

static void client_close(Client *c) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    client_by_fd[c->fd] = NULL;
    obj_pool_release(&client_pool, c);
}

static int parse_request(Client *c, int *resp_idx) {
    const char *data = (const char *)c->buf;
    size_t      n    = c->wpos;
    if (n < 5) return 0;

    
    if (memcmp(data, "POST ", 5) != 0) {
        const char *end = memmem(data, n, "\r\n\r\n", 4);
        if (!end) return 0;
        *resp_idx = RESP_IDX_ALIVE;
        return (int)((end - data) + 4);
    }

    const char *cl_hdr = memmem(data, n, "Content-Length: ", 16);
    if (!cl_hdr) return 0;
    const char *p = cl_hdr + 16;
    size_t cl = 0;
    while (p < data + n && *p >= '0' && *p <= '9') {
        cl = cl * 10 + (size_t)(*p - '0');
        p++;
    }
    if (p >= data + n) return 0;
    if (cl == 0) { *resp_idx = RESP_IDX_ERR; return (int)n; }

    const char *body_start = memmem(p, n - (size_t)(p - data), "\r\n\r\n", 4);
    if (!body_start) return 0;
    body_start += 4;
    size_t body_off = (size_t)(body_start - data);
    if (body_off + cl > n) return 0; 

    uint32_t num_fraudes = 0;
    const char *err = handle_fraud_check(body_start, cl, &num_fraudes);
    if (fail(err)) {
        LOG(LOG_LEVEL_ERROR, "Fraud check failed: %s", err);
        *resp_idx = RESP_IDX_ERR;
    } else {
        *resp_idx = (int)num_fraudes;
    }
    return (int)(body_off + cl);
}

static void handle_client(Client *c) {
    for (;;) {
        for (;;) {
            int resp_idx;
            int consumed = parse_request(c, &resp_idx);
            if (consumed == 0) break;
            if (consumed < 0)  { client_close(c); return; }

            if (!send_response(c->fd, resp_idx)) { client_close(c); return; }

            if ((uint16_t)consumed < c->wpos) {
                memmove(c->buf, c->buf + consumed, c->wpos - consumed);
                c->wpos -= (uint16_t)consumed;
            } else {
                c->wpos = 0;
            }
        }

        if (c->wpos >= BUF_SIZE) { client_close(c); return; }

        ssize_t r = read(c->fd, c->buf + c->wpos, BUF_SIZE - c->wpos);
        if (r > 0)  { c->wpos += (uint16_t)r; continue; }
        if (r == 0) { client_close(c); return; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        if (errno == EINTR) continue;
        client_close(c); return;
    }
}


static int receive_fd(int ctrl) {
    char dummy;
    struct iovec iov = { .iov_base = &dummy, .iov_len = 1 };

    union {
        struct cmsghdr cmh;
        char buf[CMSG_SPACE(sizeof(int))];
    } u;

    struct msghdr m = {
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = u.buf,
        .msg_controllen = sizeof(u.buf),
    };

    ssize_t n = recvmsg(ctrl, &m, MSG_CMSG_CLOEXEC);
    if (n <= 0) return -1;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&m);
    if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
        return -1;

    int fd;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    return fd;
}


static void handle_control(int ctrl) {
    for (;;) {
        int fd = receive_fd(ctrl);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            perror("recvmsg control");
            return;
        }
        if (fd >= MAX_FD) { close(fd); continue; }

        Client *c;
        panic(obj_pool_get(&client_pool, (void **)&c));
        *c = (Client){ .fd = fd, .wpos = 0 };
        client_by_fd[fd] = c;

        struct epoll_event ee = { .events = EPOLLIN | EPOLLET, .data.fd = fd };
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee) < 0) {
            client_by_fd[fd] = NULL;
            obj_pool_release(&client_pool, c);
            close(fd);
            continue;
        }
        handle_client(c);
    }
}

static void on_sig(int s) { (void)s; _exit(0); }

static const char WARMUP_BODY[] =
    "{\"id\":\"tx-3576980410\","
    "\"transaction\":{\"amount\":384.88,\"installments\":3,\"requested_at\":\"2026-03-11T20:23:35Z\"},"
    "\"customer\":{\"avg_amount\":769.76,\"tx_count_24h\":3,\"known_merchants\":[\"MERC-009\",\"MERC-009\",\"MERC-001\",\"MERC-001\"]},"
    "\"merchant\":{\"id\":\"MERC-001\",\"mcc\":\"5912\",\"avg_amount\":298.95},"
    "\"terminal\":{\"is_online\":false,\"card_present\":true,\"km_from_home\":13.7090520965},"
    "\"last_transaction\":{\"timestamp\":\"2026-03-11T14:58:35Z\",\"km_from_current\":18.8626479774}}";

static void warmup(void) {
    uint32_t sink = 0;
    for (int i = 0; i < 1024; i++) {
        uint32_t n = 0;
        const char *err = handle_fraud_check(WARMUP_BODY, sizeof(WARMUP_BODY) - 1, &n);
        sink ^= (uint32_t)(uintptr_t)err ^ n;
    }
    __asm__ volatile("" :: "r"(sink) : "memory");
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);

    const char *mode = getenv("MODE");
    if (!mode) { fprintf(stderr, "MODE not set\n"); return 1; }
    const bool use_ivf = strcmp(mode, "A") == 0;

    panic(handler_init(use_ivf));
    init_responses();
    panic(obj_pool_init(&client_pool, sizeof(Client), MAX_CLIENTS));

    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0 && errno != EPERM) {
        perror("mlockall");
    }

    warmup();

    const char *app_id = getenv("APP_ID");
    if (!app_id) { fprintf(stderr, "APP_ID not set\n"); return 1; }
    const bool is_one = strcmp(app_id, "APP_1") == 0;
    const char *sock_path = is_one
        ? "sockets/" LB_TO_APP_ONE_SOCK
        : "sockets/" LB_TO_APP_TWO_SOCK;

    unlink(sock_path);
    int listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd < 0) { perror("socket unix"); return 1; }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind unix"); return 1;
    }
    if (listen(listen_fd, 1) < 0) { perror("listen unix"); return 1; }

    fprintf(stderr, "backend %s: listening on %s\n", app_id, sock_path);
    int ctrl_fd = accept(listen_fd, NULL, NULL);
    if (ctrl_fd < 0) { perror("accept ctrl"); return 1; }
    close(listen_fd);
    fprintf(stderr, "backend %s: lb connected (ctrl_fd %d)\n", app_id, ctrl_fd);

    int fl = fcntl(ctrl_fd, F_GETFL);
    fcntl(ctrl_fd, F_SETFL, fl | O_NONBLOCK);

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { perror("epoll_create1"); return 1; }

    struct epoll_event ee = { .events = EPOLLIN | EPOLLET, .data.fd = ctrl_fd };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, ctrl_fd, &ee) < 0) {
        perror("epoll_ctl ctrl"); return 1;
    }

    struct epoll_event evs[MAX_EVENTS];
    for (;;) {
        int n = epoll_wait(epfd, evs, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait"); return 1;
        }
        for (int i = 0; i < n; i++) {
            int fd = evs[i].data.fd;
            if (fd == ctrl_fd) {
                handle_control(ctrl_fd);
            } else if (fd >= 0 && fd < MAX_FD && client_by_fd[fd]) {
                if (evs[i].events & (EPOLLERR | EPOLLHUP)) {
                    client_close(client_by_fd[fd]);
                } else {
                    handle_client(client_by_fd[fd]);
                }
            }
        }
    }
}
