#include "csapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int main() {
    int listen_sock, conn_sock, epoll_fd, event_count, i;
    struct epoll_event event, events[MAX_EVENTS];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    // 创建监听套接字
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8888);

    // 绑定监听套接字到指定地址和端口
    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding");
        exit(EXIT_FAILURE);
    }

    // 开始监听连接
    if (listen(listen_sock, SOMAXCONN) == -1) {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }

    // 创建 epoll 实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating epoll");
        exit(EXIT_FAILURE);
    }

    // 添加监听套接字到 epoll 实例中
    event.events = EPOLLIN;
    event.data.fd = listen_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event) == -1) {
        perror("Error adding listen socket to epoll");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Listening for connections...\n");

    while (1) {
        // 等待事件发生
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (event_count == -1) {
            perror("Error waiting for events");
            exit(EXIT_FAILURE);
        }

        // 处理所有已就绪的事件
        for (i = 0; i < event_count; ++i) {
            if (events[i].data.fd == listen_sock) {
                // 有新的连接请求
                client_len = sizeof(client_addr);
                conn_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
                if (conn_sock == -1) {
                    perror("Error accepting connection");
                    exit(EXIT_FAILURE);
                }
                printf("New connection accepted. Client address: %s, port: %d\n",
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                // 将新的连接套接字添加到 epoll 实例中
                event.events = EPOLLIN;
                event.data.fd = conn_sock;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &event) == -1) {
                    perror("Error adding connection socket to epoll");
                    exit(EXIT_FAILURE);
                }
            } else {
                // 有数据可读
                int fd = events[i].data.fd;
                ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE);
                if (bytes_read == -1) {
                    perror("Error reading from socket");
                    exit(EXIT_FAILURE);
                }

                if (bytes_read == 0) {
                    // 连接已关闭
                    printf("Connection closed. Socket %d\n", fd);
                    close(fd);
                } else {
                    // 打印接收到的数据
                    buffer[bytes_read] = '\0';
                    printf("Received data from socket %d: %s", fd, buffer);

                    // 可选：回送数据给客户端
                    write(fd, buffer, strlen(buffer));
                }
            }
        }
    }

    // 关闭监听套接字和 epoll 实例
    close(listen_sock);
    close(epoll_fd);

    return 0;
}