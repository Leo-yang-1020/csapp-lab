#include "proxy.h"

int loop(int listenfd)
{
    int connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    init_pool(listenfd, &pool);

    while (1) {
        /* Wait for listening/connected descriptor(s) to become ready */
        pool.ready_set = pool.read_set;
        
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        /* If listening descriptor ready, add new client to pool */
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }

        /* check ready descriptor */
        check_clients(&pool);
    }
}

void init_pool(int listenfd, pool *p)
{
    /* Initially, there are no connected descriptors */
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++) {
        p->clientfd[i] = -1;
        p->serverfd[i] = -1;
    }
    
    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) /* Find an available slot */
        if (p->clientfd[i] < 0) {
            /* Add connected descriptor to the pool */
            p->clientfd[i] = connfd;

            /* Add the descriptor to descriptor set */
            FD_SET(connfd, &p->read_set);

            /* Update max descriptor and pool high water mark */
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    if (i == FD_SETSIZE) /* Couldn’t find an empty slot */
        app_error("add_client error: Too many clients");
}
/**
 * Update peer server fd to pool
*/
void add_peer_server(int serverfd, pool *p, int idx)
{
    printf("serverfd %d added \n", serverfd);
    p->serverfd[idx] = serverfd;
    FD_SET(serverfd, &p->read_set);
    if (serverfd > p->maxfd)
        p->maxfd = serverfd;
    if (idx > p->maxi)
        p->maxi = idx;
}

void check_clients(pool *p)
{
    int i, clientfd, serverfd;
    char buf[MAXLINE];
    char *user_request[MAX_HEADER_LINE];
    int request_idx = 0;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        clientfd = p->clientfd[i];
        serverfd = p->serverfd[i];
        /* client descriptor is ready */
        if ((clientfd > 0) && (FD_ISSET(clientfd, &p->ready_set))) {
            p->nready--;
            serverfd = handle_client(clientfd, user_request, &request_idx);  /* receive & construct request, connect to server */
            if (serverfd !=  ERROR) {
                add_peer_server(serverfd, p, i);  /* Add serverfd to set */
                send_request(serverfd, user_request, request_idx);
                printf("serverfd :%d\n", serverfd);
            }
            else  /* EOF detected */
                close_cli_conn(clientfd, p, i);
        }

        /* server descriptor is ready */
        if ((serverfd > 0) && (FD_ISSET(serverfd, &p->ready_set))) {
            p->nready--;
            printf("server fd %d detected\n", serverfd);
            handle_response(serverfd, clientfd);
            close_cli_conn(clientfd, p, i);
            close_srv_conn(serverfd, p, i);
        }
    }
}

void close_cli_conn(int fd , pool *pool, int idx)
{
    if (fd > 0) {
        Close(fd);
        FD_CLR(fd, &(pool->read_set));
        pool->clientfd[idx] = -1;
        printf("client fd %d closed \n", fd);
    }
}

void close_srv_conn(int fd , pool *pool, int idx)
{
    if (fd > 0) {
        Close(fd);
        FD_CLR(fd, &(pool->read_set));
        pool->serverfd[idx] = -1;
        printf("server fd %d closed \n", fd);
    }
}
