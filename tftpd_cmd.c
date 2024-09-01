#include "tftpd.h"

/* list of commands
-b : block size
-t : timeout
-d : directory
-w : window size
-p : port number
-m : maximum retries
-s : tsize
*/

void tftp_server_args_parser(p_tftp_server server, int argc, char **argv)
{
    int i, save_config;
    uint32_t value;
    if (server == NULL)
        return;

    save_config = FALSE;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-b") == 0)
        {
            if (++i < argc)
            {
                value = (uint16_t)atoi(argv[i]);
                if (value >= MIN_BLKSIZE && value <= MAX_BLKSIZE)
                    server->blocksize = value;
            }
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value >= MIN_TIMEOUT && value <= MAX_TIMEOUT)
                    server->timeout = value;
            }
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            if (++i < argc)
            {
                value = strlen(argv[i]);
                if (value && is_valid_directory(argv[i]))
                {
                    memset(server->directory, 0, sizeof(server->directory));
                    strncpy(server->directory, argv[i], value);
                }
            }
        }
        else if (strcmp(argv[i], "-w") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value >= MIN_WINDOW_SIZE && value <= MAX_WINDOW_SIZE)
                    server->windowsize = value;
            }
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            if (++i < argc)
            {
                value = (uint16_t)atoi(argv[i]);
                if (value >= MIN_PORT && value <= MAX_PORT)
                    server->port = value;
            }
        }
        else if (strcmp(argv[i], "-m") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value >= MIN_RETRIES && value <= MAX_RETRIES)
                    server->retries = value;
            }
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value < UINT_MAX)
                    server->tsize = value;
            }
        }
        else if (strcmp(argv[i], "-save") == 0)
        {
            save_config = TRUE;
            i++;
        }
    }

    if (save_config)
    {
        FILE *fp = fopen(SAVE_FILE, "wb");
        if (fp == NULL)
            PRINT_ERROR("Couldn't create config file, config won't be saved");
        else
        {
            i = fwrite(server, sizeof(tftp_server), 1, fp);
            if (i < 0)
                PRINT_ERROR("Couldn't write config, config won't be saved");
            fclose(fp);
            fp = NULL;
        }
    }

    return;
}