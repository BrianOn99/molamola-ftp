#ifndef PROTOCOL_UTILS_H
#define PROTOCOL_UTILS_H

#if defined(sun) || defined(__sun)
  #include <sys/int_types.h>
#else
  #include <stdint.h>
#endif
#include <sys/types.h>

#define TYPE_OPEN_REQ 0xA1
#define TYPE_OPEN_REP 0xA2
#define TYPE_AUTH_REQ 0xA3
#define TYPE_AUTH_REP 0xA4
#define TYPE_GET_REQ 0xA7
#define TYPE_GET_REP 0xA8
#define TYPE_PUT_REQ 0xA9
#define TYPE_PUT_REP 0xAA
#define TYPE_LS_REQ 0xA5
#define TYPE_LS_REP 0xA6
#define TYPE_FILE_DATA 0xFF
#define TYPE_QUIT_REQ 0xAB
#define TYPE_QUIT_REP 0xAC

#define STATUS_UNUSED 0x00

extern char fingerprint[];

struct message_s {
        char protocol[6];
        unsigned char type;
        unsigned char status;
        unsigned int length;
}  __attribute__ ((packed));

int write_head(int sockfd, unsigned char type,
               unsigned char status, uint32_t payload_length);
int read_head(int sockfd, struct message_s *msg);
off_t payload_size(struct message_s *msg);

#endif
