#include "csapp.h"
#include "proxy.h"

int main()
{
    char *hostname = "localhost";
    char *port = "8899";

    int fd = Open_clientfd(hostname, port);
    return 0;
}