/*
 * Functions that handle specific commands
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include "request_handlers.h"
#include "accepter.h"
#include "server_main.h"
#include "common_utils/readwrite.h"
#include "common_utils/protocol_utils.h"

int req_get(int sockfd, struct message_s *msg);
int req_put(int sockfd, struct message_s *msg);
int req_quit(int sockfd, struct message_s *msg);
int req_ls(int sockfd, struct message_s *msg);

struct req_info {
        char type_code;
        req_handler handler;
};

struct req_info req_list[] = {
        {TYPE_GET_REQ, req_get},
        {TYPE_PUT_REQ, req_put},
        {TYPE_QUIT_REQ, req_quit},
        {TYPE_LS_REQ, req_ls},
};

/*
 * ending in C, meaning they Close thread on error or remote socket close
 */
void sread_C(int threadfd, void *buf, unsigned int len)
{
        if (sread(threadfd, buf, len) == -1)
                close_serving_thread(threadfd);
}


char *make_path(char *fpath)
{
        char *p = malloc(strlen(ROOT_DIR) + strlen(fpath) + 2);
        strcat(p, ROOT_DIR);
        strcat(p, "/");
        strcat(p, fpath);
        return p;
}

req_handler get_handler(char type_code)
{
        for (int i=0; i < sizeof(req_list)/sizeof(struct req_info); i++) {
                if (req_list[i].type_code == type_code)
                        return req_list[i].handler;
        }

        return NULL;
}

char *payload_malloc(int sockfd, struct message_s *msg, bool is_str)
{
        /*
         * load a payload from socket.
         * WARNING: Don't use this function to read ~MB from socket
         */
        ssize_t len = msg->length - sizeof(struct message_s);
        size_t buf_size = is_str ? len + 1 : len;
        char *payload = calloc(buf_size, 1);
        if (!payload) {
                perror("malloc error");
                exit(1);
        }
        sread_C(sockfd, payload, len);
        return payload;
}

int req_open(int sockfd, struct message_s *msg)
{
        write_head(sockfd, TYPE_OPEN_REP, 1, 0);
        return 0;
}


int req_quit(int sockfd, struct message_s *msg)
{
        write_head(sockfd, TYPE_QUIT_REP, STATUS_UNUSED, 0);
        puts("client request to quit");
        close_serving_thread(sockfd);
        return 0;
}

int req_auth(int sockfd, struct message_s *msg)
{
        char *payload = payload_malloc(sockfd, msg, true);

        struct user guest;
        if (!parse_user(payload, &guest))
                return -1;

        int res = -1;
        for (int i=0; i < USER_MAX; i++) {
                if (!user_list[i].id) {
                        /* reach the end */
                        res = -1;
                        break;
                }
                if (strcmp(user_list[i].id, guest.id) == 0 &&
                   (strcmp(user_list[i].passwd, guest.passwd) == 0)) {
                        /* match */
                        printf("%s logged in\n", guest.id);
                        res = 0;
                        break;
                }
        }
        free(payload);

        write_head(sockfd, TYPE_AUTH_REP, (res == 0 ? 1 : 0), 0);

        return res;
}

int req_get(int sockfd, struct message_s *msg)
{
        char *filepath = payload_malloc(sockfd, msg, true);
        char *repopath = make_path(filepath);
        printf("client wants %s\n", repopath);
        int local_fd = open(repopath, O_RDONLY);
        free(filepath);
        free(repopath);
        if (local_fd == -1) {
                write_head(sockfd, TYPE_GET_REP, 0, 0);
                /* though not readable, don't terminate because it is legal */
                return -1;
        }
        /* send GET_REPLY success header */
        write_head(sockfd, TYPE_GET_REP, 1, 0);

        struct stat st;
        fstat(local_fd, &st);
        /* send FILE_DATA header */
        write_head(sockfd, TYPE_FILE_DATA, STATUS_UNUSED, st.st_size);
#ifdef __linux__
        int ret = transfer_file_sys(sockfd, local_fd, st.st_size, NULL);
#else
        int ret = transfer_file_copy(sockfd, local_fd, st.st_size, NULL);
#endif
        close(local_fd);
        if (ret == -1)
                close_serving_thread(sockfd);

        return 0;
}

int req_put(int sockfd, struct message_s *msg)
{
        char *filepath = payload_malloc(sockfd, msg, true);
        char *repopath = make_path(filepath);
        int saveto_fd = open(repopath, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
        free(filepath);
        free(repopath);
        if (saveto_fd == -1) {
                perror("cannot open file to write");
                close_serving_thread(sockfd);
        }
        /* always says that you can upload */
        write_head(sockfd, TYPE_PUT_REP, 1, 0);

        /* receive DATA_FILE */
        struct message_s recv_msg;
        read_head(sockfd, &recv_msg);

        off_t size = payload_size(&recv_msg);
        int ret = transfer_file_copy(saveto_fd, sockfd, size, NULL);
        close(saveto_fd);
        if (ret == -1)
                close_serving_thread(sockfd);

        return 0;
}

int req_ls(int sockfd, struct message_s *msg)
{
        char buff[2048];
        DIR *dirp;
        if (!(dirp = opendir(ROOT_DIR))) {
                perror(ROOT_DIR " not exist");
                return -1;
        }

        struct dirent *dp;
        int char_len = 0;
        while ((dp = readdir(dirp))) {
                int new_len = char_len + (strlen(dp->d_name) + 1);
                if (new_len >= (2048-1))
                        break;
                strcpy(buff + char_len, dp->d_name);
                strcpy(buff + char_len + strlen(dp->d_name), "\n");
                printf("%d\n", char_len);
                char_len = new_len;
        }
        closedir(dirp);
        /* not sending terminate '\0' */

        printf("%d\n", char_len);
        write_head(sockfd, TYPE_LS_REP, 1, char_len);
        swrite(sockfd, buff, char_len);
        return 0;
}
