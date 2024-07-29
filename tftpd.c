#include "tftpd.h"

/* macros */
#define PRINT_ERROR(msg) fprintf(stderr, "%s: %s\n", msg, strerror(errno))

/* global variables */
tftp_server tftpd;

/* append to buffer */
int append_to_buffer(char *buff, int offset, const char *str)
{
    int len = strlen(str) + 1;
    memcpy(buff + offset, str, len);
    return offset + len;
}

/* file size checker */
long long get_file_size(const char *filename)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA file_info;
    if (GetFileAttributesEx(filename, GetFileExInfoStandard, &file_info) == 0)
    {
        fprintf(stderr, "Error getting file attributes\n");
        return -1;
    }
    LARGE_INTEGER size;
    size.HighPart = file_info.nFileSizeHigh;
    size.LowPart = file_info.nFileSizeLow;
    return size.QuadPart;
#else
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return (long long)st.st_size;
    }
    perror("Error getting file size");
    return -1;
#endif
}

/* digit count */
int digit_counter(int n)
{
    int digit = 0;
    while (n)
    {
        digit++;
        n /= 10;
    }
    return digit;
}

/* it will return non zero value if successfull, for data it will return blocksize */
int tftp_packet_send(p_tftp_session session, uint8_t opcode, char *msg, uint8_t *data, uint32_t len /* error code for error packet*/)
{
    OACK_PACKET *oack_packet = NULL;
    ERR_PACKET *err_packet = NULL;
    ACK_PACKET *ack_packet = NULL;
    DATA_PACKET *data_packet = NULL;
    char buff[session->options.blocksize + 100];
    memset(buff, 0, sizeof(buff));
    uint32_t packet_len = 0;
    int rv = 0;

    // window size porjnto pathabeo ekhan theke

    switch (opcode)
    {
    case OACK:
        if (!session->options_enabled)
            return 0;
        oack_packet = (OACK_PACKET *)buff;
        oack_packet->opcode = htons(OACK);
        packet_len += 2;
        printf("blocksize: %d\n", session->options.blocksize);
        if (session->options.blocksize != DEFAULT_BLKSIZE)
        {
            sprintf(buff + packet_len, "blksize");
            packet_len += 7 + 1; // 1 for null terminator
            sprintf(buff + packet_len, "%u", session->options.blocksize);
            packet_len += digit_counter(session->options.blocksize) + 1;
        }
        if (session->options.timeout != DEFAULT_TIMEOUT)
        {
            sprintf(buff + packet_len, "timeout");
            packet_len += 7 + 1; // 1 for null terminator
            sprintf(buff + packet_len, "%u", session->options.timeout);
            packet_len += digit_counter(session->options.timeout) + 1;
        }
        if (session->options.windowsize != DEFAULT_WINDOW_SIZE)
        {
            sprintf(buff + packet_len, "windowsize");
            packet_len += 9 + 1; // 1 for null terminator
            sprintf(buff + packet_len, "%u", session->options.windowsize);
            packet_len += digit_counter(session->options.windowsize) + 1;
        }

        sprintf(buff + packet_len, "tsize");
        packet_len += 5 + 1; // 1 for null terminator
        sprintf(buff + packet_len, "%llu", session->options.tsize);
        packet_len += digit_counter(session->options.tsize);

        /* code */
        break;
    case ERR:
        err_packet = (ERR_PACKET *)buff;
        err_packet->opcode = htons(ERR);
        err_packet->error_code = htons((uint16_t)len);
        packet_len += 4;
        strcpy(err_packet->error_msg, msg);
        packet_len += strlen(msg);
        break;
    case ACK:
        ack_packet = (ACK_PACKET *)buff;
        ack_packet->opcode = htons(ACK);
        ack_packet->block_no = htons(session->block_counter);
        packet_len += 4;
        break;
    case DATA:
        data_packet = (DATA_PACKET *)buff;
        data_packet->opcode = htons(DATA);
        data_packet->block_no = htons(session->block_counter);
        packet_len += 4;
        memcpy(data_packet->data, data, len);
        packet_len += len;
        break;
    default:
        break;
    }
    /* packet send */
    rv = sendto(session->socket_fd, buff, packet_len, 0, (const struct sockaddr *)&session->client_addr, sizeof(session->client_addr));
    printf("packet sent:opcode %u len %u rv %d\n", opcode, packet_len, rv);
    return rv;
}

void handle_read_request(p_tftp_session session)
{
    uint8_t last_file = FALSE;
    uint8_t last_block = FALSE;
    uint16_t opcode;
    uint32_t bytes_sent = 0, bytes_read = 0, retries, file_buffer_size;
    int rv, client_len;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in6 client_addr;
    fd_set read_fds;
    struct timeval timeout;
    ERR_PACKET *err_packet = NULL;
    ACK_PACKET *ack_packet = NULL;

    uint8_t *file_buffer = (uint8_t *)malloc(FILE_BUFFER_SIZE);
    if (file_buffer == NULL)
    {
        printf("%s: unable to allocate memory on file buffer\n", __FUNCTION__);
        return;
    }

    session->file_fd = fopen(session->filename, "rb");
    if (session->file_fd == NULL)
        goto read_err;

    file_buffer_size = (FILE_BUFFER_SIZE / session->options.blocksize) * session->options.blocksize;
    bytes_read = fread(file_buffer, 1, file_buffer_size, session->file_fd);
    if (bytes_read < file_buffer_size)
        last_file = TRUE;

    session->offset += bytes_read;
    client_len = sizeof(client_addr);
    timeout.tv_sec = session->options.timeout;
    timeout.tv_usec = 0;

    if (session->options_enabled)
    {
        rv = recvfrom(session->socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (rv < 0)
        {
            PRINT_ERROR("Unable to receive initial options");
            goto read_err;
        }
        ack_packet = (ACK_PACKET *)buffer;
        if (ntohs(ack_packet->opcode) != ACK || ntohs(ack_packet->block_no) != 0)
        {
            tftp_packet_send(session, ERR, "Unknown packet", NULL, 0);
            goto read_err;
        }
    }

    while (1)
    {
        if (last_block)
            break;

        retries = 0;
        session->block_counter++;

        if (bytes_sent == bytes_read)
        {
            if (fseek(session->file_fd, session->offset, SEEK_SET) != 0)
            {
                PRINT_ERROR("Unable to file seek");
                goto read_err;
            }

            if (!last_file)
            {
                bytes_read = fread(file_buffer, 1, file_buffer_size, session->file_fd);
                session->offset += bytes_read;
                bytes_sent = 0;

                if (bytes_read < file_buffer_size)
                    last_file = TRUE;
            }
        }

        if (bytes_read - bytes_sent < session->options.blocksize)
            last_block = TRUE;

        while (retries < MAX_RETRIES)
        {
            uint32_t data_size = !last_block ? session->options.blocksize : (bytes_read - bytes_sent);
            rv = tftp_packet_send(session, DATA, NULL, file_buffer + bytes_sent, data_size);
            if (rv < 0)
            {
                PRINT_ERROR("Unable to send packet");
                goto read_err;
            }
            bytes_sent += data_size;

            /* init the fd set */
            FD_ZERO(&read_fds);
            FD_SET(session->socket_fd, &read_fds);
            rv = select(session->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
            if (rv < 0)
            {
                PRINT_ERROR("Unable to select");
                goto read_err;
            }
            else if (rv == 0)
            {
                // timeout
                retries++;
                continue;
            }
            else
            {
                // success
                if (FD_ISSET(session->socket_fd, &read_fds))
                {
                    memset(buffer, 0, BUFFER_SIZE);
                    rv = recvfrom(session->socket_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
                    opcode = ntohs(*(uint16_t *)buffer);
                    switch (opcode)
                    {
                    case ACK:
                        ack_packet = (ACK_PACKET *)buffer;
                        ack_packet->block_no = ntohs(ack_packet->block_no);
                        if (ack_packet->block_no != session->block_counter)
                        {
                            tftp_packet_send(session, ERR, "packet block mismatch", NULL, 0);
                            goto read_err;
                        }
                        break;
                    case ERR:
                        err_packet = (ERR_PACKET *)buffer;
                        printf("ERROR CODE %u: %s\n", ntohs(err_packet->error_code), err_packet->error_msg);
                        goto read_err;
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
        }

        if (retries == MAX_RETRIES)
        {
            tftp_packet_send(session, ERR, "timeout", NULL, 0);
            goto read_err;
        }
    }

read_err:
    if (file_buffer)
        free(file_buffer);
    file_buffer = NULL;
    if (session->file_fd)
        fclose(session->file_fd);
    return;
}

void handle_write_request(p_tftp_session session)
{
    uint8_t last_block = FALSE;
    uint16_t opcode;
    uint32_t bytes_written = 0, bytes_received = 0, retries;
    int rv, client_len;
    char buffer[session->options.blocksize + 100];
    struct sockaddr_in6 client_addr;
    fd_set write_fds;
    struct timeval timeout;
    ERR_PACKET *err_packet = NULL;
    DATA_PACKET *data_packet = NULL;
    ACK_PACKET ack_packet;

    uint8_t *file_buffer = (uint8_t *)malloc(FILE_BUFFER_SIZE);
    if (file_buffer == NULL)
    {
        printf("%s: unable to allocate memory on file buffer\n", __FUNCTION__);
        return;
    }

    session->file_fd = fopen(session->filename, "wb");
    if (session->file_fd == NULL)
        goto write_err;

    client_len = sizeof(client_addr);
    timeout.tv_sec = session->options.timeout;
    timeout.tv_usec = 0;

    while (1)
    {
        if (last_block)
            break;

        retries = 0;
        session->block_counter++;
        bytes_received = 0;

        while (retries < MAX_RETRIES)
        {
            /* init the fd set */
            FD_ZERO(&write_fds);
            FD_SET(session->socket_fd, &write_fds);
            rv = select(session->socket_fd + 1, &write_fds, NULL, NULL, &timeout);
            if (rv < 0)
            {
                PRINT_ERROR("Unable to select");
                goto write_err;
            }
            else if (rv == 0)
            {
                // timeout
                tftp_packet_send(session, ACK, NULL, NULL, 0);
                retries++;
                continue;
            }
            else
            {
                // success
                if (FD_ISSET(session->socket_fd, &write_fds))
                {
                    memset(buffer, 0, session->options.blocksize + 100);
                    rv = recvfrom(session->socket_fd, buffer, session->options.blocksize + 100, 0, (struct sockaddr *)&client_addr, &client_len);
                    if (rv < 0)
                    {
                        perror("rcvfrom");
                        goto write_err;
                    }
                    opcode = ntohs(*(uint16_t *)buffer);
                    switch (opcode)
                    {
                    case DATA:
                        data_packet = (DATA_PACKET *)buffer;
                        data_packet->block_no = ntohs(data_packet->block_no);
                        if (data_packet->block_no != session->block_counter)
                        {
                            tftp_packet_send(session, ERR, "packet block mismatch", NULL, 0);
                            goto write_err;
                        }
                        bytes_received = rv - 4; // Subtract header size
                        memcpy(file_buffer + bytes_written, data_packet->data, bytes_received);
                        bytes_written += bytes_received;
                        if (bytes_received < session->options.blocksize)
                            last_block = TRUE;

                        tftp_packet_send(session, ACK, NULL, NULL, 0);
                        break;
                    case ERR:
                        err_packet = (ERR_PACKET *)buffer;
                        printf("ERROR CODE %u: %s\n", ntohs(err_packet->error_code), err_packet->error_msg);
                        goto write_err;
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
        }

        if (retries == MAX_RETRIES)
        {
            tftp_packet_send(session, ERR, "timeout", NULL, 0);
            goto write_err;
        }

        if (bytes_written >= FILE_BUFFER_SIZE || last_block)
        {
            fwrite(file_buffer, 1, bytes_written, session->file_fd);
            bytes_written = 0;
        }
    }

write_err:
    if (file_buffer)
        free(file_buffer);
    file_buffer = NULL;
    if (session->file_fd)
        fclose(session->file_fd);
    return;
}

/* thread clipping for windows and linux */

DWORD WINAPI thread_function(LPVOID param)
{
    int len, rv, value, i;
    struct sockaddr_in6 client_addr;
    p_tftp_session session = (p_tftp_session)param;
    if (session == NULL)
    {
        printf("Thread parameter invalid\n");
        return 0;
    }
    printf("%s\n", __FUNCTION__);

    /* tasks:
    1. socket initialization with a random port number for tid
    2. check the opcode, wrq hole ekta case rrq hole arekta case
    3. then wait for data packets
    */

    /* socket init */
    /* winsock initialization */
    // #ifdef WINDOWS
    //     WSADATA wsa;
    //     if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    //     {
    //         printf("Win sock initialization failed. Error code: %d", WSAGetLastError());
    //         goto clean_up;
    //     }
    // #endif
    /* socket creation */
    session->socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (session->socket_fd < 0)
    {
        printf("Socket creation failed on thread. Error code: %d\n", session->socket_fd);
        goto session_err;
    }
    printf("new socket created.\n");

    /* set IPV6_ONLY false */
    value = 0;
    rv = setsockopt(session->socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, sizeof(value));
    if (rv == -1)
    {
        printf("setsockopt failed.\n");
        goto session_err;
    }
    printf("scoket option created\n");
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

    printf("socket bind ready\n");
    /* check file for read request */
    if (session->opcode == RRQ)
    {
        if (access(session->filename, F_OK) == -1)
        {
            printf("The file does not exist in the current directory\n");
            tftp_packet_send(session, ERR, "file not found", NULL, 0);
            goto session_err;
        }
        session->options.tsize = get_file_size(session->filename);
        printf("file size: %llu", session->options.tsize);
    }

    /* option negotiation */
    if (session->options_enabled)
        tftp_packet_send(session, OACK, NULL, NULL, 0);

    printf("packet sent-> option enabled: %u\n", session->options_enabled);

    /* opcode checking */
    switch (session->opcode)
    {
    case RRQ:
        printf("RRQ\n");
        handle_read_request(session);
        break;
    case WRQ:
        printf("WRQ\n");
        handle_write_request(session);
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

int create_thread(p_tftp_session session)
{
    printf("%s\n", __FUNCTION__);
#ifdef _WIN32
    HANDLE hThread;
    DWORD dwThreadId;

    hThread = CreateThread(
        NULL,            // default security attributes
        0,               // default stack size
        thread_function, // thread function
        session,         // parameter to thread function
        0,               // default creation flags
        &dwThreadId);    // returns the thread identifier

    if (hThread == NULL)
    {
        printf("CreateThread failed\n");
        return 0;
    }

    printf("Thread created\n");

    // Wait for the thread to complete
    WaitForSingleObject(hThread, INFINITE);

    // Close the thread handle
    CloseHandle(hThread);
#else
    pthread_t thread;
    int result;

    result = pthread_create(&thread, NULL, ThreadFunction, session);
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

p_tftp_session tftp_packet_parser(char *buff, int len)
{
    uint8_t option_flag = FALSE;
    int i = 0, j = 0;
    uint32_t number;
    char temp_buf[MAX_MODE];
    p_tftp_session new_session = (p_tftp_session)malloc(sizeof(tftp_session));
    if (new_session == NULL)
        return NULL;
    memset(new_session, 0, sizeof(tftp_session));

    new_session->opcode = ntohs(*(uint16_t *)buff);
    i = i + 2;
    printf("opcode: %u\n", new_session->opcode);

    while (buff[i] != 0x00 && i < len)
    {
        new_session->filename[j++] = buff[i++];
    }
    new_session->filename[j] = '\0';
    i++;
    printf("filename: %s\n", new_session->filename);

    j = 0;
    memset(temp_buf, 0, sizeof(temp_buf));
    while (buff[i] != 0x00 && i < len)
    {
        temp_buf[j++] = tolower(buff[i++]);
    }
    temp_buf[j] = '\0';
    i++;
    if (strcmp(temp_buf, "netascii") == 0)
        new_session->mode = NETASCII_MODE;
    else
        new_session->mode = OCTET_MODE;

    printf("mode: %u\n", new_session->mode);
    while (i < len)
    {
        printf("entered\n");
        char option[MAX_MODE];
        memset(option, 0, MAX_MODE);
        j = 0;
        while (buff[i] != 0x00 && i < len)
        {
            option[j++] = buff[i++];
        }
        option[j] = '\0';
        i++;

        char value[MAX_MODE];
        memset(value, 0, MAX_MODE);
        j = 0;
        while (buff[i] != 0x00 && i < len)
        {
            value[j++] = buff[i++];
        }
        value[j] = '\0';
        i++;

        if (strcmp(option, "blksize") == 0)
        {
            number = atoi(value);
            if (number < 8)
                new_session->options.blocksize = MIN_BLKSIZE;
            else if (number > MAX_BLKSIZE)
                new_session->options.blocksize = MAX_BLKSIZE;
            else
                new_session->options.blocksize = number;

            printf("blocksize: %d %d\n", new_session->options.blocksize, number);
            option_flag = TRUE;
        }
        else if (strcmp(option, "tsize") == 0)
        {
            new_session->options.tsize = atoi(value);
            option_flag = TRUE;
        }
        else if (strcmp(option, "timeout") == 0)
        {
            new_session->options.timeout = atoi(value);
            option_flag = TRUE;
        }
        else if (strcmp(option, "windowsize") == 0)
        {
            number = atoi(value);
            if (number < DEFAULT_WINDOW_SIZE)
                new_session->options.windowsize = DEFAULT_WINDOW_SIZE;
            else if (number > MAX_WINDOW_SIZE)
                new_session->options.windowsize = MAX_WINDOW_SIZE;

            option_flag = TRUE;
        }
    }

    /* if there is no option available, populate with the default value */
    if (!new_session->options.blocksize)
        new_session->options.blocksize = DEFAULT_BLKSIZE;
    if (!new_session->options.tsize)
        new_session->options.tsize = DEFAULT_TSIZE;
    if (!new_session->options.timeout)
        new_session->options.timeout = DEFAULT_TIMEOUT;
    if (!new_session->options.windowsize)
        new_session->options.windowsize = DEFAULT_WINDOW_SIZE;

    if (option_flag)
        new_session->options_enabled = TRUE;

    printf("flag: %u\n", new_session->options_enabled);
    return new_session;
}

int main()
{
    char buffer[BUFFER_SIZE];
    struct sockaddr_in6 client_addr;
    int rv, len, session_counter;
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

        PrintClientAddress(&client_addr);
        printf("Bytes recieved: %u\n", rv);
        p_tftp_session tftp_new_session = tftp_packet_parser(buffer, rv);
        if (tftp_new_session == NULL)
        {
            printf("TFTP new session creation failed\n");
            continue;
        }

        /* client address copy */
        tftp_new_session->transfer_id = ntohs(client_addr.sin6_port);
        memcpy(&tftp_new_session->client_addr, &client_addr, sizeof(tftp_new_session->client_addr));

        /* new thread creation */
        rv = create_thread(tftp_new_session);
        if (!rv)
        {
            printf("TFTP new thread creation failed\n");
            free(tftp_new_session);
            tftp_new_session = NULL;
            continue;
        }
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