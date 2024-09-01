#include "tftpd.h"

int tftpd_handle_write_request(p_tftp_session session)
{
    uint8_t last_block = FALSE;
    uint16_t opcode;
    uint32_t bytes_written = 0, bytes_received = 0, retries;
    socklen_t client_len;
    int rv;
    char buffer[session->options.blocksize + 100];
    struct sockaddr_in6 client_addr;
    fd_set write_fds;
    struct timeval timeout;
    ERR_PACKET *err_packet = NULL;
    DATA_PACKET *data_packet = NULL;

    uint8_t *file_buffer = (uint8_t *)malloc(FILE_BUFFER_SIZE);
    if (file_buffer == NULL)
    {
        PRINT_ERROR("Unable to allocate memory for session file buffer");
        tftpd_packet_send(session, ERR, NULL, ERR_DISK_FULL);
        return SESSION_FAILED;
    }

    session->file_fd = fopen(session->path, "wb");
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
                tftpd_packet_send(session, ACK, NULL, 0);
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
                        PRINT_ERROR("Unable to receive");
                        tftpd_packet_send(session, ERR, NULL, ERR_UNKNOWN);
                        goto write_err;
                    }
                    VALIDATE_TID(write_err);
                    opcode = ntohs(*(uint16_t *)buffer);
                    switch (opcode)
                    {
                    case DATA:
                        data_packet = (DATA_PACKET *)buffer;
                        data_packet->block_no = ntohs(data_packet->block_no);
                        if (data_packet->block_no != session->block_counter)
                        {
                            CLR_ERROR;
                            printf("Block missmatched, transfer aborted.\n");
                            CLR_RESET;
                            tftpd_packet_send(session, ERR, NULL, ERR_ILLEGAL_OPT);
                            goto write_err;
                        }
                        bytes_received = rv - 4; // Subtract header size
                        memcpy(file_buffer + bytes_written, data_packet->data, bytes_received);
                        bytes_written += bytes_received;
                        if (bytes_received < session->options.blocksize)
                            last_block = TRUE;

                        rv = tftpd_packet_send(session, ACK, NULL, 0);
                        if (rv < 0)
                        {
                            PRINT_ERROR("Unable to send");
                            goto write_err;
                        }
                        session->bytes_transferred += bytes_received;
                        update_progress_bar(session);
                        break;
                    case ERR:
                        err_packet = (ERR_PACKET *)buffer;
                        CLR_ERROR;
                        printf("ERROR packet recieved. ERROR %u: %s\n", ntohs(err_packet->error_code), err_packet->error_msg);
                        CLR_RESET;
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
            PRINT_ERROR("Timeout reached, transfer aborted");
            tftpd_packet_send(session, ERR, NULL, ERR_TIMEOUT);
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

    if (last_block)
        return SESSION_SUCCEED;
    return SESSION_FAILED;
}