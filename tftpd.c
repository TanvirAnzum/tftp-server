#include "tftpd.h"

// void * thread_function() {
//     printf("thread testing.\n");
//     int i;
//     for(i = 0; i < 10; i++) {
//         printf("%d ", i);
//     }
//     return NULL;
// }

/* thread creation */
// unsigned long int thread_id;
// void* h_thread = NULL;
// rv = create_thread(&thread_id, &h_thread, thread_function, 0);       //thread_id, handle_id, args
// if(!rv) {
//     printf("Thread creation failed.\n");
//     goto error;
// }
void PrintClientAddress(struct sockaddr_in6 *src_addr)
{
    char ipstr[INET6_ADDRSTRLEN];
    int port;

    struct sockaddr_in6 *s = (struct sockaddr_in6 *)src_addr;
    port = ntohs(s->sin6_port);
    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
    printf("Client address: %s, Port: %d\n", ipstr, port);

    return;
}

void tftp_packet_parser(char *buff, int len, tftp_request_packet *tftp_packet) {
    int i = 0, j = 0;
    tftp_packet->opcode = ntohs(*(uint16_t *)buff);

    i = i + 2;
    while(buff[i] != 0x00 && i < len) {
        tftp_packet->filename[j++] = buff[i++];
    }
    tftp_packet->filename[j] = '\0';
    i++;
   
    j = 0;
    while(buff[i] != 0x00 && i < len) {
        tftp_packet->mode[j++] = buff[i++];
    }
    tftp_packet->mode[j] = '\0';
    i++;

    while(i < len) {
        char option[MAX_MODE];
        memset(option, 0, MAX_MODE);
        j = 0;
        while(buff[i] != 0x00 && i < len) {
            option[j++] = buff[i++];
        }
        option[j] = '\0';
        i++;

        char value[MAX_MODE];
        memset(value, 0, MAX_MODE);
        j = 0;
        while(buff[i] != 0x00 && i < len) {
            value[j++] = buff[i++];
        }
        value[j] = '\0';
        i++;

        if(strcmp(option, "blksize") == 0)
            tftp_packet->blksize = atoi(value);
        else if(strcmp(option, "tsize") == 0)
            tftp_packet->tsize = atoi(value);
        else if(strcmp(option, "timeout") == 0)
            tftp_packet->timeout = atoi(value);
        else if(strcmp(option, "windowsize") == 0)
            tftp_packet->windowsize = atoi(value);
    }

    return;
}
int main()
{
    tftp_server tftpd;
    tftp_request_packet tftp_packet;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in6 client_addr;
    int rv, len;

    tftp_server_init(&tftpd);

    /* winsock initialization */
#ifdef WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Win sock initialization failed. Error code: %d", WSAGetLastError());
        goto clean_up;
    }
#endif
    /* socket creation */

    tftpd.socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (tftpd.socket_fd < 0)
    {
        printf("Socket creation failed. Error code: %d\n", tftpd.socket_fd);
        goto clean_up;
    }

    /* set IPV6_ONLY false */
    int value = 0;
    rv = setsockopt(tftpd.socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, sizeof(value));
    if (rv == -1)
    {
        printf("setsockopt failed.\n");
        goto clean_up;
    }

    /* socket server information */
    memset(&tftpd.server_addr, 0, sizeof(tftpd.server_addr));
    tftpd.server_addr.sin6_family = AF_INET6;
    tftpd.server_addr.sin6_addr = in6addr_any;
    tftpd.server_addr.sin6_port = htons(tftpd.port);

    /* socket bind */
    rv = bind(tftpd.socket_fd, (struct sockaddr *)&tftpd.server_addr, sizeof(tftpd.server_addr));
    if (rv < 0)
    {
        printf("Socket bind failed. Error code: %d\n", rv);
        goto clean_up;
    }

    len = sizeof(client_addr);
    /* udp packet receiving loop */
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(&client_addr, 0, len);
        printf("TFTP server is listening on port: %d\n", tftpd.port);

        rv = recvfrom(tftpd.socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
        if (rv < 0)
            continue;

        PrintClientAddress(&client_addr);

        memset(&tftp_packet, 0, sizeof(tftp_packet));
        tftp_packet_parser(buffer, rv, &tftp_packet);

        printf("%d %s %u %u %u %u\n", tftp_packet.opcode, tftp_packet.mode, tftp_packet.blksize, tftp_packet.tsize, tftp_packet.windowsize, tftp_packet.timeout);
        /*
        if (access("tftp.exe", F_OK) != -1)
        {
            printf("The file exists in the current directory\n");
        }
        else
        {
            printf("The file does not exist in the current directory\n");
        }
        */
        // printf("New request received. ");
    }

    return 0;

/* cleaning up when error occured */
clean_up:
    if (tftpd.socket_fd > 0)
        close(tftpd.socket_fd);
#ifdef WINDOWS
    WSACleanup();
#endif
    return -1;
}

// int create_thread(unsigned long int *thread_id, void **h_thread, void *(*thread_function)(void *), void *args) {
//     *h_thread = CreateThread(NULL, 0, thread_function, args, 0, thread_id);
//     if(*h_thread == NULL)
//         return -1;
//     return 0;
// }

void tftp_server_init(p_tftp_server tftp_server)
{
    tftp_server->blocksize = DEFAULT_BLKSIZE;
    tftp_server->port = DEFAULT_PORT;
    tftp_server->session_count = 0;
    tftp_server->timeout = DEFAULT_TIMEOUT;
    tftp_server->tsize = 0;
    tftp_server->windowsize = 0;
    return;
}