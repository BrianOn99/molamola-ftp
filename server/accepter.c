#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "request_handlers.h"
#include "accepter.h"
#include "common_utils/readwrite.h"

static int thread_sockfd[65536];

void read_head_C(int threadfd, struct message_s *msg)
{
        if (read_head(threadfd, msg) == -1)
                close_serving_thread(threadfd);
}

int wait_authenicated(int sockfd)
{
        /* authenicate.  Should only handle "open" and "auth" request */
        struct message_s head_recv;

        while (1) {
                read_head_C(sockfd, &head_recv);
                if (head_recv.type == TYPE_OPEN_REQ) {
                        req_open(sockfd, &head_recv);
                } else if (head_recv.type == TYPE_AUTH_REQ) {
                        if (req_auth(sockfd, &head_recv) == 0)
                                return 0;
                        else
                                return -1;
                } else {
                        fprintf(stderr,
                                "client unsuitably sneding Request %02x\n"
                                , head_recv.type);
                        return -1;
                }
        }
}

void *dedicated_serve(void *p)
{
        int sockfd = *((int*)p);
        if (wait_authenicated(sockfd) == -1) {
                printf("authenication has problem\n");
                close_serving_thread(sockfd);
        }

        /* now it is authenicated */
        struct message_s head_recv;

        while (1) {
                read_head_C(sockfd, &head_recv);
                req_handler handler = get_handler(head_recv.type);
                printf("Received request: %02x\n", head_recv.type);
                if (!handler) {
                        fprintf(stderr, "Request %02x is malformmated\n", head_recv.type);
                        continue;
                }
                handler(sockfd, &head_recv);
        }
        /* reach head means client closed */
        printf("closed connection\n");
        close_serving_thread(sockfd);
        return NULL;
}

void mkthread_serve(int sockfd)
{
        thread_sockfd[sockfd] = sockfd;
        pthread_t thread_id;
        pthread_create(&thread_id ,NULL, dedicated_serve, &thread_sockfd[sockfd]);
        pthread_detach(thread_id);
}

void accept_loop(int sockfd)
{
        struct sockaddr_in addr;
        socklen_t sin_size = sizeof(struct sockaddr_in);

        while (1) {
                int spawned_sockfd = accept(sockfd, (struct sockaddr *)&addr, &sin_size);
                if (spawned_sockfd == -1) {
                        perror("Error accept client");
                        continue;
                }

                puts("received connection");
                mkthread_serve(spawned_sockfd);
        }
}

void serve(int sockfd)
{
        if (listen(sockfd, 0) == -1) {
                perror("Cannot listen");
                exit(1);
        }

        accept_loop(sockfd);
}

void close_serving_thread(int sockfd)
{
        close(sockfd);
        pthread_exit(NULL);
}
