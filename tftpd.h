#ifndef TFTPD_H
#define TFTPD_H

/* common header files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
//#include "thread.h"

/* OS specific */
#ifdef _WIN32
#define WINDOWS
#else
#define LINUX
#endif

#ifdef WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib") 
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

/* Buffer size */
#define BUFFER_SIZE         600

/* tftp default values */
#define OCTET_MODE          1
#define NETASCII_MODE       2
#define DEFAULT_TIMEOUT     5       /* in sec */
#define DEFAULT_BLKSIZE     512     /* in bytes */
#define DEFAULT_WINDOW_SIZE 1       /* number of block in a window */
#define DEFAULT_TSIZE       0       /* null */
#define MAX_RETRY           3       /* max retry after timeout */
#define MAX_CONNECTION      3       /* max concurrent connection */
#define MAX_FILENAME        256     /* file name character limit */
#define MAX_MODE            15      /* netascii or octet */
#define DEFAULT_PORT        69      /* tftp well known port */
#define MAX_TFTP_SESSIONS   3       /* number of tftp sessions */

/* packet types */
#define RRQ                 1
#define WRQ                 2
#define DATA                3
#define ACK                 4
#define ERR                 5
#define OACK                6

/*error msgs */
#define ERR_FILE_NOT_FOUND  1
#define ERR_ACSS_VIOLATION  2
#define ERR_DISK_FULL       3
#define ERR_ILLEGAL_OPT     4
#define ERR_UNKNOWN_TID     5
#define ERR_FILE_EXISTS     6
#define ERR_NO_USER         7
#define ERR_OPT_NEG_FAILED  8

/* tftp control strucutre*/
typedef struct tftpd_control_structure {
    uint8_t transfer_mode;
    uint16_t port;
    uint32_t session_count;
    uint32_t timeout;
    uint32_t blocksize;
    uint32_t windowsize;
    uint32_t tsize;
    int socket_fd;
    struct sockaddr_in6 server_addr;
} tftp_server, *p_tftp_server;

/* tftp rrq and wrq structure with options*/
typedef struct tftp_rrq_wrq {
    uint8_t timeout;
    uint16_t opcode;
    uint16_t blksize;
    uint16_t windowsize;
    uint32_t tsize;
    char filename[MAX_FILENAME];
    char mode[MAX_MODE];
} tftp_request_packet;

/* tftp serssion structure */
typedef struct tftpd_session_structure {
    int socket_fd;
    int transfer_id;
    struct sockaddr_in6 client_addr;
    FILE *file_fd;
    uint32_t opcode;
    tftp_request_packet tftp_req_packet;
} tftp_session, *p_tftp_session;

/* functions */
void tftp_server_init(p_tftp_server);



#endif