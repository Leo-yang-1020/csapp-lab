#include "proxy.h"

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
   #if 0
   /* BIO implemented */
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             //line:netp:tiny:doit
	Close(connfd);                                            //line:netp:tiny:close
    }
    #endif
    /* Select implemented */
    loop(listenfd);
}

/*
 * doit - handle one HTTP request/response transaction
 *  1. read http request from client
 *  2. generate http request to server
 *  3. send http request to server
 *  4. receive http response from server
 *  5. send response back to client
 */
/* $begin doit */
void doit(int fd) 
{
    int request_idx = 0;
    int server_fd;
    char *user_request_hdr[MAX_HEADER_LINE];
    char buf[MAXLINE];
    char host[HOST_SIZE];
    char port[PORT_SIZE];
    char *path;
    rio_t rio;

    Rio_readinitb(&rio, fd);

    if (constct_reqline(&rio, user_request_hdr, &request_idx, fd) == ERROR)
        return ;

    constct_req(&rio, user_request_hdr, &request_idx, host, port);              // read & construct request header, establish connection
    printf("browser's new request \n");
    for (int i = 0; i < request_idx; i++) {
        printf("%s", user_request_hdr[i]);
    }

    server_fd = connect_to_srv(host, port);
    send_request(server_fd, user_request_hdr, request_idx);

    handle_response(server_fd, fd);
    Close(server_fd);
}
/* $end doit */

/* */
int handle_client(int fd, char **user_request_hdr, int *request_idx)
{
    int serverfd;
    char buf[MAXLINE];
    char host[HOST_SIZE];
    char port[PORT_SIZE];
    char *path;
    rio_t rio;

    Rio_readinitb(&rio, fd);

    if (constct_reqline(&rio, user_request_hdr, request_idx, fd) == ERROR)
        return ERROR;

    constct_req(&rio, user_request_hdr, request_idx, host, port);              // read & construct request header, establish connection
    printf("browser's new request \n");
    for (int i = 0; i < *request_idx; i++) {
        printf("%s", user_request_hdr[i]);
    }
    serverfd = connect_to_srv(host, port);
    return serverfd;
}

int constct_reqline(rio_t *rp, char *user_request_hdr[], int *idx, int fd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], request_line[MAXLINE];
    char *path;
    if (!Rio_readlineb(rp, buf, MAXLINE))  // Read HTTP first line
        return ERROR;
    sscanf(buf, "%s %s %s", method, uri, version);       // parse request line

    if (strcasecmp(method, "GET")) {                     // beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return ERROR;
    }           

    uri2path(uri, &path);
    if (path == NULL)
        path = uri;
    sprintf(request_line, "%s %s %s\r\n", method, path, http_version); // http1.0 only
    user_request_hdr[*idx] = (char *)malloc(sizeof(char)*sizeof(request_line));
    strcpy(user_request_hdr[(*idx)++], request_line);
    return 0;
}

/*
 *  Connect to target server
*/
int connect_to_srv(char *host, char *port)
{
    int conn_fd;
    conn_fd = Open_clientfd(host, port);
    return conn_fd;
}

/*
 * read_requesthdrs - read HTTP request headers and construct request header
 * return connect fd of coneection to target server
 */
/* $begin read_requesthdrs */
void  constct_req(rio_t *rp, char *user_request_hdr[], int *idx, char *host, char *port)
{
    char buf[MAXLINE];
    char *split_loc;
    int flag_conn = 0, flag_pxyconn = 0, flag_agent = 0, flag_host = 0;
    int conn_fd;

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    user_request_hdr[*idx] = (char *)malloc(sizeof(char)*sizeof(buf));
    strcpy(user_request_hdr[(*idx)++], buf);

    split_loc = strchr(buf + 6, ':');   // split host:port by ":"
    if (split_loc != NULL){
        *split_loc = '\0';
        strcpy(host, buf + 6);
        strncpy(port, split_loc+1, strlen(split_loc+1)-2);  // remove '\r\n'
    }

    user_request_hdr[(*idx)++] = user_agent_hdr;   
    user_request_hdr[(*idx)++] = connection_hdr;
    user_request_hdr[(*idx)++] = proxy_connection_hdr;
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
    if (!flag_agent && !strncasecmp(buf, usragent_str, strlen(usragent_str))) {
        flag_agent = 1; // judge once
        continue; // ignore originial user agent value
    }

    if (!flag_conn && !strncasecmp(buf, conn_str, strlen(conn_str))) {
        flag_conn = 1;  // judge once
        continue;  // ignore original connection value
    } 

    if (!flag_pxyconn && !strncasecmp(buf, pxyconn_str, strlen(pxyconn_str))) {
        flag_pxyconn = 1;  // judge once
        continue;  // ignore original pxyconnection value
    }

    user_request_hdr[*idx] = (char *)malloc(sizeof(char)*sizeof(buf));
    strcpy(user_request_hdr[(*idx)++], buf);
    }
    return ;
}
/* $end read_requesthdrs */

void uri2path(char *uri, char **path)
{
    char *start = strstr(uri, "://");
    if (start != NULL) {
        start += 3; // skip "://" part
    } else {
        start = uri;
    }

    // start location of path: after first slash
    *path = strchr(start, '/');
}

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
	strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
	strcat(filename, uri);                           //line:netp:parseuri:endconvert1
	if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
	return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	}
	else 
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	return 0;
    }
}
/* $end parse_uri */

void send_request(int fd, char *header[], int linesize)
{
    for (int i = 0; i < linesize; i++)
        Rio_writen(fd, header[i], strlen(header[i]));
}

/* 
 *  1. Read response from server
 *  2. Send response back to client
 */

void handle_response(int server_fd, int client_fd)
{
    rio_t rio;
    int linesize = 0, objsize;
    char buf[MAXLINE];
    char obj[MAX_OBJECT_SIZE];
    char *header[MAX_HEADER_LINE];

    Rio_readinitb(&rio, server_fd);
    while(strcmp(buf, "\r\n")) {          // Read response header
	Rio_readlineb(&rio, buf, MAXLINE);
    header[linesize] = (char *)malloc(sizeof(char) * strlen(buf));
    strcpy(header[linesize++], buf);
    }

    objsize = Rio_readnb(&rio, obj, MAX_OBJECT_SIZE);

    for (int i = 0; i < linesize; i+=1)
        Rio_writen(client_fd, header[i], strlen(header[i]));
    Rio_writen(client_fd, obj, objsize);
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

