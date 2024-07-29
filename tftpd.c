#include "tftpd.h"

/* functions */
void tftp_server_init(p_tftp_server tftp_server);
void tftpd_start_session(p_tftp_session session);
int tftpd_create_thread(p_tftp_session session);

#ifdef _WIN32
DWORD WINAPI tftpd_thread_function(LPVOID param);
#else
void *tftpd_thread_function(void *param);
#endif

/* tftpd main function */
int main(int argc, char **argv)
{
    char buffer[BUFFER_SIZE];
    struct sockaddr_in6 client_addr;
    int rv, len, session_counter;
    p_tftp_session tftp_session = NULL;

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
    /* udp packet receiving loop */
    len = sizeof(client_addr);
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(&client_addr, 0, sizeof(client_addr));
        printf("TFTP server is listening on port: %d\n", tftpd.port);

        rv = recvfrom(tftpd.socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
        if (rv < 0)
            continue;

        print_client_address(&client_addr);
        tftp_session = tftpd_packet_parser(buffer, rv);
        if (tftp_session == NULL)
        {
            printf("TFTP new session creation failed\n");
            continue;
        }

        /* client address copy */
        tftp_session->transfer_id = ntohs(client_addr.sin6_port);
        memcpy(&tftp_session->client_addr, &client_addr, sizeof(tftp_session->client_addr));

        /* new thread creation */
        rv = tftpd_create_thread(tftp_session);
        if (!rv)
        {
            printf("TFTP new thread creation failed\n");
            free(tftp_session);
            tftp_session = NULL;
            continue;
        }
    }

/* cleaning up when error occured */
clean_up:
    if (tftpd.socket_fd > 0)
        close(tftpd.socket_fd);
    if (tftp_session != NULL)
        free(tftp_session);
    tftp_session = NULL;
#ifdef WINDOWS
    WSACleanup();
#endif
    return 0;
}

/* tftpd start session */
void tftpd_start_session(p_tftp_session session)
{
    int len, rv, value, i;
    struct sockaddr_in6 client_addr;
    if (session == NULL)
    {
        printf("Thread parameter invalid\n");
        return 0;
    }
    session->socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (session->socket_fd < 0)
    {
        printf("Socket creation failed on thread. Error code: %d\n", session->socket_fd);
        goto session_err;
    }
    /* set IPV6_ONLY false */
    value = 0;
    rv = setsockopt(session->socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, sizeof(value));
    if (rv == -1)
    {
        printf("setsockopt failed.\n");
        goto session_err;
    }

    /* socket server information */
    memset(&session->server_addr, 0, sizeof(session->server_addr));
    session->server_addr.sin6_family = AF_INET6;
    session->server_addr.sin6_addr = in6addr_any;
    session->server_addr.sin6_port = htons(0);

    /* socket bind */
    rv = bind(session->socket_fd, (struct sockaddr *)&session->server_addr, sizeof(session->server_addr));
    if (rv < 0)
    {
        printf("Socket bind failed. Error code: %d\n", rv);
        goto session_err;
    }

    /* check file for read request */
    if (session->opcode == RRQ)
    {
        if (access(session->filename, F_OK) == -1)
        {
            printf("The file does not exist in the current directory\n");
            tftpd_packet_send(session, ERR, "file not found", NULL, 0);
            goto session_err;
        }
        session->options.tsize = get_file_size(session->filename);
        printf("file size: %llu", session->options.tsize);
    }

    /* option negotiation */
    if (session->options_enabled)
        tftpd_packet_send(session, OACK, NULL, NULL, 0);

    /* opcode checking */
    switch (session->opcode)
    {
    case RRQ:
        tftpd_handle_read_request(session);
        break;
    case WRQ:
        tftpd_handle_write_request(session);
        break;
    default:
        printf("invalid opcode: %d\n", session->opcode);
        goto session_err;
        break;
    }

session_err:
    if (session->socket_fd > 0)
        close(session->socket_fd);
    if (session != NULL)
    {
        free(session);
        session = NULL;
    }
    return 0;
}
#ifdef _WIN32
DWORD WINAPI tftpd_thread_function(LPVOID param)
{
    p_tftp_session session = (p_tftp_session)param;
    tftpd_start_session(session);
    return 0;
}
#else
void *tftpd_thread_function(void *param)
{
    p_tftp_session session = (p_tftp_session)param;
    tftpd_start_session(session);
    return NULL;
}
#endif

int tftpd_create_thread(p_tftp_session session)
{
#ifdef _WIN32
    HANDLE hThread;
    DWORD dwThreadId;

    hThread = CreateThread(NULL, 0, tftpd_thread_function, session, 0, &dwThreadId);
    if (hThread == NULL)
    {
        printf("CreateThread failed\n");
        return 0;
    }

    // Wait for the thread to complete
    WaitForSingleObject(hThread, INFINITE);

    // Close the thread handle
    CloseHandle(hThread);
#else
    pthread_t thread;
    int result;

    result = pthread_create(&thread, NULL, tftpd_thread_function, session);
    if (result)
    {
        printf("pthread_create failed: %d\n", result);
        return 0;
    }

    // Wait for the thread to complete
    pthread_join(thread, NULL);
#endif
    return 1;
}

void tftp_server_init(p_tftp_server tftp_server)
{
    tftp_server->blocksize = DEFAULT_BLKSIZE;
    tftp_server->port = DEFAULT_PORT;
    tftp_server->session_count = 0;
    tftp_server->timeout = DEFAULT_TIMEOUT;
    tftp_server->tsize = 0;
    tftp_server->windowsize = 1;
    return;
}