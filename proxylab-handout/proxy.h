#include <stdio.h>
#include <time.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HEADER_LINE 100
#define PORT_SIZE 7
#define HOST_SIZE 100
#define ERROR -1

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *http_version = "HTTP/1.0";
static const char *conn_str = "Connection";
static const char *pxyconn_str = "Proxy-Connection";
static const char *usragent_str = "User-Agent";

typedef struct { /* Represents a pool of connected descriptors */
    int maxfd;        /* Largest descriptor in read_set */
    fd_set read_set;  /* Set of all active descriptors */
    fd_set ready_set; /* Subset of descriptors ready for reading */
    int nready;       /* Number of ready descriptors from select */
    int maxi;         /* High water index into client array */
    int clientfd[FD_SETSIZE];    /* Set of active descriptors of client */
    int serverfd[FD_SETSIZE];  /* Set of active descriptors of serrver */
    rio_t clientrio[FD_SETSIZE]; /* Set of active buffers of client*/
    rio_t serverrio[FD_SETSIZE]; /* Set of active buffers of server*/
} pool;

extern void doit(int fd);
extern void  constct_req(rio_t *rp, char *user_request_hdr[], int *idx, char *host, char *port);
extern int constct_reqline(rio_t *rp, char *user_request_hdr[], int *idx, int fd);
extern void send_request(int fd, char *header[],int linesize);
extern void handle_response(int server_fd, int client_fd);
extern void uri2path(char *uri, char ** path);
extern int parse_uri(char *uri, char *filename, char *cgiargs);
extern void serve_static(int fd, char *filename, int filesize);
extern void get_filetype(char *filename, char *filetype);
extern void serve_dynamic(int fd, char *filename, char *cgiargs);
extern void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
extern void add_peer_server(int serverfd, pool *p, int idx);
extern int handle_client(int fd, char **user_request_hdr, int *request_idx);
extern int connect_to_srv(char *host, char *port);


extern int loop(int listenfd);
extern void init_pool(int listenfd, pool *p);
extern void add_client(int connfd, pool *p);
extern void check_clients(pool *p);
extern void close_cli_conn(int fd , pool *pool, int idx);
extern void close_srv_conn(int fd , pool *pool, int idx);

