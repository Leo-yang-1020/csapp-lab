#include "proxy.h"

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define SERVER 1
#define CLIENT 2
#define LISTENER 3

int epoll_loop(int listenfd)
{
    struct epoll_event events[MAX_EVENTS];
    int epollfd, event_count, i, fd, type;
    flow *flow;
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("Error creating epoll\n");
        return -1;
    }

    add_listenfd(listenfd, epollfd);

    while (1) {
        event_count = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (event_count == -1) {
            perror("Error waiting for events");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < event_count; i++) {
            flow = (struct flow *)events[i].data.ptr;
            fd = flow->fd;
            type = flow->type;
            if (fd == listenfd)
                add_flow(listenfd, epollfd);
            else if (type == CLIENT) {
                client_event_handle(fd, epollfd, &events[i]);
            }
            else if (type == SERVER) {
                server_event_handle(fd, epollfd, &events[i]);
            }
        }
    }
}

void add_listenfd(int listenfd, int epollfd)
{
    struct epoll_event event;
    flow *flow;
    event_init(&event, flow, NULL, EPOLLIN, listenfd, LISTENER, 0);
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event) == -1) {
        perror("Error adding listen socket to epoll");
        exit(EXIT_FAILURE);
    }
    printf("listen fd added: %d\n", listenfd);
}

void event_init(struct epoll_event* event, flow *flow, struct flow *peer_flow, int events, int fd, int type, int peer_fd)
{
   flow = (struct flow *)malloc(sizeof(flow));
   flow->fd = fd;
   flow->type = type;
   flow->peer_fd = peer_fd;
   flow->peer_flow = peer_flow;
   event->events = events;
   event->data.ptr = flow;
}

void add_flow(int listenfd, int epollfd)
{
    int clientfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    struct epoll_event event;
    flow *flow;
    clientlen = sizeof(struct sockaddr_storage);
    clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    event_init(&event, flow, NULL, EPOLLIN, clientfd, CLIENT, 0);

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
        perror("Error adding client socket to epoll");
        exit(EXIT_FAILURE);
    }
    printf("new client added: %d\n", clientfd);
}

void client_event_handle(int clientfd, int epollfd, struct epoll_event *client_event)
{
    printf("client triggered : %d\n", clientfd);
    char *user_request_hdr[MAXLINE];
    int hdr_idx;
    int serverfd;
    struct epoll_event server_event;
    flow *flow, *peer_flow;
    peer_flow = client_event->data.ptr;
    serverfd = handle_client(clientfd, user_request_hdr, &hdr_idx);
    if (serverfd == ERROR) { /* EOF detected */
        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL) == -1) {
            perror("Error deleting client socket from epoll");
            exit(EXIT_FAILURE);
        }
        Close(clientfd);
    }

    event_init(&server_event, flow, peer_flow, EPOLLIN, serverfd, SERVER, clientfd);

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &server_event) == -1) {
        perror("Error adding server socket to epoll");
        exit(EXIT_FAILURE);
    }
    send_request(serverfd, user_request_hdr, hdr_idx);
    printf("new server added: %d\n", serverfd);

}

void server_event_handle(int serverfd, int epollfd, struct epoll_event *event)
{
    int clientfd;
    flow *flow, *peer_flow;
    printf("server triggered : %d\n", serverfd);
    flow = (struct flow *)event->data.ptr;
    peer_flow = flow->peer_flow;
    clientfd = flow->peer_fd;
    handle_response(serverfd, clientfd);
    delete_flow(epollfd, serverfd, flow);
    delete_flow(epollfd, clientfd, peer_flow);
}

void delete_flow(int epollfd, int fd, flow *flow)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
            perror("Error deleting socket from epoll");
            exit(EXIT_FAILURE);
    }
    Close(fd);
    free(flow);
    printf("deleted: %d\n", fd);
}
