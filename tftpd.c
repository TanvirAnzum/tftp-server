#include "tftpd.h"

/* global variable */
tftp_server tftpd;

/* functions */
void tftpd_start_session(p_tftp_session session);
int tftpd_create_thread(p_tftp_session session);
void print_info(void);
void tftp_server_init(void);

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
    int rv, len;
    p_tftp_session tftp_session = NULL;
    char time[50];

    tftp_server_init();
    tftp_server_args_parser(&tftpd, argc, argv);
    print_info();
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
        printf("\nTFTP server is listening on port: %d\n", tftpd.port);

        rv = recvfrom(tftpd.socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
        if (rv < 0)
            continue;

        tftp_session = tftpd_packet_parser(buffer, rv);
        if (tftp_session == NULL)
        {
            printf("TFTP new session creation failed\n");
            continue;
        }

        /* client address copy */
        tftp_session->transfer_id = ntohs(client_addr.sin6_port);
        memcpy(&tftp_session->client_addr, &client_addr, sizeof(tftp_session->client_addr));
        tftpd.session_count++;
        get_local_time(time, 50);
        printf("%s: TFTP session: %u\n", time, tftpd.session_count);
        print_client_address(&client_addr);
        printf("File name: %s\n", tftp_session->filename);
        /* new thread creation */
        rv = tftpd_create_thread(tftp_session);
        if (!rv)
        {
            printf("TFTP new thread creation failed\n");
            free(tftp_session);
            tftp_session = NULL;
            continue;
        }
        get_local_time(time, 50);
        printf("\n%s: TFTP session %u has finished.\n", time, tftpd.session_count);
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
    int rv, value;
    if (session == NULL)
    {
        printf("Thread parameter invalid\n");
        return;
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
    /* getting the directory */
    if (strlen(tftpd.directory) > 0)
    {
        if (snprintf(session->path, PATH_MAX, "%s%s%s", tftpd.directory, PATH_SEPARATOR, session->filename) >= PATH_MAX)
        {
            fprintf(stderr, "Error: Path too long\n");
            goto session_err; // or handle the error appropriately
        }
    }
    else
        strncpy(session->path, session->filename, PATH_MAX);

    printf("d: %s\n", session->path);

    if (session->opcode == RRQ)
    {
        if (access(session->path, F_OK) == -1)
        {
            printf("The file does not exist in the current directory\n");
            tftpd_packet_send(session, ERR, "file not found", NULL, 0);
            goto session_err;
        }
        session->options.tsize = get_file_size(session->path);
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
    return;
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

void print_info(void)
{
    char horizontal_line[BOX_WIDTH + 1];
    memset(horizontal_line, '-', BOX_WIDTH);
    horizontal_line[BOX_WIDTH] = '\0';

    printf("+%s+\n", horizontal_line);
    printf("| %-59s|\n", "TFTP Server Information");
    printf("+%s+\n", horizontal_line);

    printf("| Transfer Mode: %-44s|\n", (tftpd.transfer_mode == OCTET_MODE) ? "octet" : "net-ascii");
    printf("| Port: %-53d|\n", tftpd.port);
    printf("| Timeout: %-50u|\n", tftpd.timeout);
    printf("| Block Size: %-47u|\n", tftpd.blocksize);
    printf("| Window Size: %-46u|\n", tftpd.windowsize);
    printf("| Retries: %-50u|\n", tftpd.retries);
    printf("| Directory: %-48s|\n", (tftpd.directory[0] == 0) ? "current" : tftpd.directory);
    printf("+%s+\n", horizontal_line);

    printf("| %-59s|\n", "Developed by:");
    printf("| %-59s|\n", "Md. Tanvir Anzum");
    printf("| %-59s|\n", "R&D Engineer, BDCOM");
    printf("+%s+\n", horizontal_line);
    return;
}

void tftp_server_init(void)
{
    tftpd.blocksize = DEFAULT_BLKSIZE;
    tftpd.port = DEFAULT_PORT;
    tftpd.session_count = 0;
    tftpd.timeout = DEFAULT_TIMEOUT;
    tftpd.tsize = DEFAULT_TSIZE;
    tftpd.windowsize = MIN_WINDOW_SIZE;
    tftpd.directory[0] = '\0';
    tftpd.retries = DEFAULT_RETRIES;
    tftpd.transfer_mode = OCTET_MODE;
    return;
}