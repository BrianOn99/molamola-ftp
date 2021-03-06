#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void error_exit(char *msg)
{
        fprintf(stderr, "Error: %s\n", msg);
        exit(1);
}

long get_port(char *str)
{
        char *endptr;
        int port = strtol(str, &endptr, 10);
        if (!port || !endptr)
                port = 0;
        return port;
}

int make_socket(char *port_str, int reuse)
{
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1)
                error_exit(strerror(errno));

        if (reuse) {
                setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                           &(int){ 1 }, sizeof(int));
        }

        if (port_str) {
                /* bind local port to argv[1] */
                long given_port = get_port(port_str);
                if (given_port <= 0 || given_port > 65535)
                        error_exit("Invalid port number");

                struct sockaddr_in my_addr = {
                        .sin_family = AF_INET,
                        .sin_port = htons(given_port),
                        .sin_addr = {.s_addr = INADDR_ANY}
                };

                if (bind(sockfd, (struct sockaddr *)&my_addr,
                    sizeof(struct sockaddr))) {
                        error_exit(strerror(errno));
                }
        }

        return sockfd;
}

