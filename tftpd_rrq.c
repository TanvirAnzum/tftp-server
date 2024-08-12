#include "tftpd.h"

void tftpd_handle_read_request(p_tftp_session session)
{
    uint8_t last_file = FALSE;
    uint8_t last_block = FALSE;
    uint16_t opcode;
    uint32_t bytes_sent = 0, bytes_read = 0, retries, file_buffer_size;
    socklen_t client_len;
    int rv;
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

    session->file_fd = fopen(session->path, "rb");
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
            tftpd_packet_send(session, ERR, "Unknown packet", NULL, 0);
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

        while (retries < tftpd.retries)
        {
            uint32_t data_size = !last_block ? session->options.blocksize : (bytes_read - bytes_sent);
            rv = tftpd_packet_send(session, DATA, NULL, file_buffer + bytes_sent, data_size);
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
                            tftpd_packet_send(session, ERR, "packet block mismatch", NULL, 0);
                            goto read_err;
                        }
                        session->bytes_transferred += data_size;
                        update_progress_bar(session);
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

        if (retries == tftpd.retries)
        {
            tftpd_packet_send(session, ERR, "timeout", NULL, 0);
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