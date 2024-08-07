#ifndef TFTPD_H
#define TFTPD_H

/* common header files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
// #include "thread.h"

/* OS specific */
#ifdef _WIN32
#define WINDOWS
#else
#define LINUX
#endif

#ifdef WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

/* Buffer size */
#define BUFFER_SIZE 600
#define FILE_BUFFER_SIZE 1048576 /* 1 MB */

/* tftp default values */
#define OCTET_MODE 1
#define NETASCII_MODE 2
#define DEFAULT_TIMEOUT 5       /* in sec */
#define MIN_BLKSIZE 8           /* in bytes */
#define DEFAULT_BLKSIZE 512     /* in bytes */
#define MAX_BLKSIZE 65464       /* in bytes */
#define MIN_WINDOW_SIZE 1       /* number of blocks in a window */
#define MAX_WINDOW_SIZE 65535   /* number of blocks in a window */
#define DEFAULT_TSIZE 0         /* null */
#define DEFAULT_RETRIES 3       /* max retry after timeout */
#define MAX_CONNECTION 3        /* max concurrent connection */
#define MAX_FILENAME 256        /* file name character limit */
#define MAX_MODE 15             /* netascii or octet */
#define DEFAULT_PORT 69         /* tftp well known port */
#define MAX_TFTP_SESSIONS 3     /* number of tftp sessions */
#define MAX_DIRECTORY_SIZE 1024 /* maximum directory size */
#define MIN_TIMEOUT 1
#define MAX_TIMEOUT 100
#define MIN_PORT 1
#define MAX_PORT 65535
#define MIN_RETRIES 1
#define MAX_RETRIES 10
#define BOX_WIDTH 60

/* directory related */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

/* packet types */
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERR 5
#define OACK 6

/*error msgs */
#define ERR_FILE_NOT_FOUND 1
#define ERR_ACSS_VIOLATION 2
#define ERR_DISK_FULL 3
#define ERR_ILLEGAL_OPT 4
#define ERR_UNKNOWN_TID 5
#define ERR_FILE_EXISTS 6
#define ERR_NO_USER 7
#define ERR_OPT_NEG_FAILED 8

/* tftp options */
typedef struct tftp_options_structure
{
    uint32_t timeout;
    uint32_t blocksize;
    uint32_t windowsize;
    uint32_t tsize;
} tftp_options;

/* tftp control strucutre*/
typedef struct tftpd_control_structure
{
    uint8_t transfer_mode;
    uint16_t port;
    uint32_t session_count;
    uint32_t timeout;
    uint32_t blocksize;
    uint32_t windowsize;
    uint32_t tsize;
    uint32_t retries;
    char directory[MAX_DIRECTORY_SIZE];
    int socket_fd;
    struct sockaddr_in6 server_addr;
} tftp_server, *p_tftp_server;

/* tftp serssion structure */
typedef struct tftpd_session_structure
{
    struct sockaddr_in6 server_addr;
    struct sockaddr_in6 client_addr;
    tftp_options options;
    FILE *file_fd;
    char filename[MAX_FILENAME];
    char path[MAX_PATH];
    int socket_fd;
    uint32_t block_counter;
    uint32_t offset;
    uint32_t bytes_transferred;
    uint16_t transfer_id;
    uint16_t opcode;
    uint8_t mode; /* 1 for binary , 2 for net ascii */
    uint8_t options_enabled;
} tftp_session, *p_tftp_session;

/* tftp packets */
#pragma pack(1)
typedef struct tftp_oack_packet
{
    uint16_t opcode;
    char options[0];
} OACK_PACKET;
#pragma pack(0)

#pragma pack(1)
typedef struct tftp_err_packet
{
    uint16_t opcode;
    uint16_t error_code;
    char error_msg[0];
} ERR_PACKET;
#pragma pack(0)

#pragma pack(1)
typedef struct tftp_ack_packet
{
    uint16_t opcode;
    uint16_t block_no;
} ACK_PACKET;
#pragma pack(0)

#pragma pack(1)
typedef struct tftp_data_packet
{
    uint16_t opcode;
    uint16_t block_no;
    uint8_t data[0];
} DATA_PACKET;
#pragma pack(0)

/* global variables */
extern tftp_server tftpd;

/* macros */
#define PRINT_ERROR(msg) fprintf(stderr, "%s: %s\n", msg, strerror(errno))

/* functions */
void tftpd_handle_write_request(p_tftp_session session);
void tftpd_handle_read_request(p_tftp_session session);
p_tftp_session tftpd_packet_parser(char *buff, int len);
int tftpd_packet_send(p_tftp_session session, uint8_t opcode, char *msg, uint8_t *data, uint32_t len);
void tftp_server_args_parser(p_tftp_server server, int argc, char **argv);

/*tftpd utils*/
int append_to_buffer(char *buff, int offset, const char *str);
long long get_file_size(const char *filename);
void print_client_address(struct sockaddr_in6 *src_addr);
uint32_t digit_counter(long long unsigned n);
int is_valid_directory(const char *path);
void update_progress_bar(p_tftp_session session);
void get_local_time(char *buffer, size_t buffer_size);

#endif