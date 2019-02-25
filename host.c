
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

int connect_client (const char *hostname,
                const char *service,
                int         family,
                int         socktype)
{
    struct addrinfo hints, *res, *ressave;
    int n, sockfd;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = family;
    hints.ai_socktype = socktype;

    n = getaddrinfo(hostname, service, &hints, &res);

    if (n <0) {
        fprintf(stderr,
                "getaddrinfo error:: [%s]\n",
                gai_strerror(n));
        return -1;
    }

    ressave = res;

    sockfd=-1;
    while (res) {
        sockfd = socket(res->ai_family,
                        res->ai_socktype,
                        res->ai_protocol);

        if (!(sockfd < 0)) {
            if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
                break;

            close(sockfd);
            sockfd=-1;
        }
//        char straddr[INET6_ADDRSTRLEN] = {0, };
//        inet_ntop(res->ai_family, res->ai_addr, straddr, sizeof(straddr));
//        printf("addr: %s\n", straddr);
                res=res->ai_next;
                    }



                        freeaddrinfo(ressave);
                            return sockfd;
                            }




const char *DAYTIME_PORT="3268";


int
main(int argc, char *argv[])
{
    int connfd;
    char *myhost;
    char timeStr[52] = {0,};

    myhost = "localhost";
    if (argc > 1)
        myhost = argv[1];

    connfd= connect_client(myhost, DAYTIME_PORT, AF_UNSPEC, SOCK_STREAM);

    if (connfd < 0) {
         fprintf(stderr,
                 "client error:: could not create connected socket "
                 "socket\n");
         return -1;
    }

    memset(timeStr, 0, sizeof(timeStr));

    while (read(connfd, timeStr, sizeof(timeStr)-1) > 0) {
	timeStr[51] = 0;
	printf("%s", timeStr);
	memset(timeStr, 0, 52);
    }

    close(connfd);

    return 0;
}                            
