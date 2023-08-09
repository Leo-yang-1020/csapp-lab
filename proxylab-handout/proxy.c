#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HEADER_LINE 100
#define PORT_SIZE 7
#define HOST_SIZE 100

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *http_version = "HTTP/1.0";
static const char *conn_str = "Connection";
static const char *pxyconn_str = "Proxy-Connection";
static const char *usragent_str = "User-Agent";


void doit(int fd);
int constct_req(rio_t *rp, char *user_request_hdr[], int *idx);
void send_request(int fd, char *header[],int linesize);
void handle_response(int server_fd, int client_fd);
void uri2path(char *uri, char ** path);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

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
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             //line:netp:tiny:doit
	Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int request_idx = 0;
    int server_fd;
    char *user_request_hdr[MAX_HEADER_LINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], request_line[MAXLINE];
    char *path;
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  // Read HTTP first line
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);       // parserequest

    if (strcasecmp(method, "GET")) {                     // beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                             
    
    uri2path(uri, &path);
    if (path == NULL)
        path = uri;
    sprintf(request_line, "%s %s %s\r\n", method, path, http_version); // http1.0 only
    user_request_hdr[request_idx++] = request_line; // request line modified

    server_fd = constct_req(&rio, user_request_hdr, &request_idx);              // readrequesthdrs & construct request header
    printf("browser's new request \n");
    for (int i = 0; i < request_idx; i++) {
        printf("%s", user_request_hdr[i]);
    }

    send_request(server_fd, user_request_hdr, request_idx);

#if 0
    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	return;
    }                                                    //line:netp:doit:endnotfound

    if (is_static) { /* Serve static content */          
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	serve_static(fd, filename, sbuf.st_size);       //line:netp:doit:servestatic
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
	serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    }
#endif
    handle_response(server_fd, fd);
    
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers and construct request header
 * return connect fd of coneection to target server
 */
/* $begin read_requesthdrs */
int  constct_req(rio_t *rp, char *user_request_hdr[], int *idx)
{
    char buf[MAXLINE];
    char host[HOST_SIZE];
    char port[PORT_SIZE];
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
    conn_fd = Open_clientfd(host, port);
    return conn_fd;
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
    Close(server_fd);
}

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    Close(srcfd);                       //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

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

