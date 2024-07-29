#include "tftpd.h"

p_tftp_session tftpd_packet_parser(char *buff, int len)
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

    while (buff[i] != 0x00 && i < len)
    {
        new_session->filename[j++] = buff[i++];
    }
    new_session->filename[j] = '\0';
    i++;

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

    while (i < len)
    {
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

    return new_session;
}

/* it will return non zero value if successfull, for data it will return tftpd_packet_send */
int tftpd_packet_send(p_tftp_session session, uint8_t opcode, char *msg, uint8_t *data, uint32_t len /* error code for error packet*/)
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
    return rv;
}