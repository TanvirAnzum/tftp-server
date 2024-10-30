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
    socklen_t len;
    int rv;
    p_tftp_session tftp_session = NULL;
    char time[TIME_BUFFER];
    tftp_server_init();
    tftp_server_args_parser(&tftpd, argc, argv);
    print_info();
    /* winsock initialization */
#ifdef WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        PRINT_ERROR("Winsock initialization failed");
        goto clean_up;
    }
#endif
    /* socket creation */
    tftpd.socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (tftpd.socket_fd < 0)
    {
        PRINT_ERROR("socket initialization failed");
        goto clean_up;
    }

    /* set IPV6_ONLY false */
    int value = 0;
    rv = setsockopt(tftpd.socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, sizeof(value));
    if (rv == -1)
    {
        PRINT_ERROR("Socket option modification failed");
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
        PRINT_ERROR("Socket binding failed");
        goto clean_up;
    }

    CLR_SECONDARY;
    printf("TFTP server is listening on port: %d\n", tftpd.port);
    CLR_RESET;

    /* udp packet receiving loop */
    len = sizeof(client_addr);
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(&client_addr, 0, sizeof(client_addr));
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
        tftp_session->session_id = tftpd.session_count;
        get_local_time(time, TIME_BUFFER);

        CLR_SUCCESS;
        printf("%s: TFTP session: %u\n", time, tftpd.session_count);
        print_client_address(&client_addr);
        printf("File name: %s\n", tftp_session->filename);
        CLR_RESET;

        /* new thread creation */
        rv = tftpd_create_thread(tftp_session);
        if (!rv)
        {
            PRINT_ERROR("Thread creation failed");
            if (tftp_session)
            {
                free(tftp_session);
                tftp_session = NULL;
            }
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
    int rv, value;
    char time[TIME_BUFFER];

    if (session == NULL)
    {
        PRINT_ERROR("Thread parameter is NULL");
        return;
    }
    session->socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (session->socket_fd < 0)
    {
        PRINT_ERROR("Session socket initialization failed");
        goto session_err;
    }
    /* set IPV6_ONLY false */
    value = 0;
    rv = setsockopt(session->socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, sizeof(value));
    if (rv == -1)
    {
        PRINT_ERROR("Session socket option modification failed");
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
        PRINT_ERROR("Session socket binding failed");
        goto session_err;
    }

    /* check file for read request */
    /* getting the directory */
    if (strlen(tftpd.directory) > 0)
    {
        if (snprintf(session->path, MAX_PATH, "%s%s%s", tftpd.directory, PATH_SEPARATOR, session->filename) >= MAX_PATH)
        {
            PRINT_ERROR("Session directory too long");
            goto session_err; // or handle the error appropriately
        }
    }
    else
        strncpy(session->path, session->filename, MAX_PATH);

    if (session->opcode == RRQ)
    {
        if (access(session->path, F_OK) == -1)
        {
            PRINT_ERROR("File not found");
            tftpd_packet_send(session, ERR, NULL, ERR_FILE_NOT_FOUND);
            goto session_err;
        }
        session->options.tsize = get_file_size(session->path);
    }

    /* option negotiation */
    if (session->options_enabled)
        tftpd_packet_send(session, OACK, NULL, 0);

    /* opcode checking */
    switch (session->opcode)
    {
    case RRQ:
        rv = tftpd_handle_read_request(session);
        break;
    case WRQ:
        rv = tftpd_handle_write_request(session);
        break;
    default:
        PRINT_ERROR("Invalid opcode, Transfer aborted.");
        goto session_err;
        break;
    }

    if (rv == SESSION_SUCCEED)
    {
        memset(time, 0, TIME_BUFFER);
        get_local_time(time, TIME_BUFFER);
        CLR_SUCCESS;
        printf("%s: TFTP session %u has finished.\n", time, session->session_id);
        CLR_RESET;
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
        PRINT_ERROR("Thread creation failed");
        return 0;
    }

    /* 
        1. wait - for single transfer only
        2. no wait - for concurrent transfer
    */
    // WaitForSingleObject(hThread, INFINITE);

    // Close the thread handle
    CloseHandle(hThread);
#else
    pthread_t thread;
    int result;

    result = pthread_create(&thread, NULL, tftpd_thread_function, session);
    if (result)
    {
        PRINT_ERROR("Thread creation failed");
        return 0;
    }

    // Wait for the thread to complete
    // pthread_join(thread, NULL);
#endif
    return 1;
}

void print_info(void)
{
    char horizontal_line[BOX_WIDTH + 1];
    memset(horizontal_line, '-', BOX_WIDTH);
    horizontal_line[BOX_WIDTH] = '\0';
    CLR_PRIMARY;
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
    CLR_RESET;
    return;
}

void tftp_server_init(void)
{
    memset(&tftpd, 0, sizeof(tftp_server));
    FILE *fp = fopen(SAVE_FILE, "rb");
    if (fp)
    {
        size_t bytes_read = fread(&tftpd, sizeof(tftp_server), 1, fp);
        if (bytes_read == 1)
        {
            CLR_WARNING;
            printf("Old configuration preloaded.\n");
            CLR_RESET;
            tftpd.socket_fd = 0;
            memset(&tftpd.server_addr, 0, sizeof(tftpd.server_addr));
        }
        else
            memset(&tftpd, 0, sizeof(tftp_server));

        fclose(fp);
    }

    if (tftpd.blocksize < MIN_BLKSIZE || tftpd.blocksize > MAX_BLKSIZE)
        tftpd.blocksize = DEFAULT_BLKSIZE;

    if (tftpd.port < MIN_PORT || tftpd.port > MAX_PORT)
        tftpd.port = DEFAULT_PORT;

    if (tftpd.timeout < MIN_TIMEOUT || tftpd.timeout > MAX_TIMEOUT)
        tftpd.timeout = DEFAULT_TIMEOUT;

    if (!tftpd.tsize)
        tftpd.tsize = DEFAULT_TSIZE;

    if (tftpd.windowsize < MIN_WINDOW_SIZE || tftpd.windowsize > MAX_WINDOW_SIZE)
        tftpd.windowsize = MIN_WINDOW_SIZE;

    if (tftpd.retries < MIN_RETRIES || tftpd.retries > MAX_RETRIES)
        tftpd.retries = DEFAULT_RETRIES;

    tftpd.session_count = 0;
    tftpd.transfer_mode = OCTET_MODE;

    if (!is_valid_directory(tftpd.directory))
        tftpd.directory[0] = '\0';

    return;
}
