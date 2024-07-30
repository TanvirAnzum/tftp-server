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

void tftp_server_args_parser(tftpd_commands *cmds, int argc, char **argv)
{
    int i;
    uint32_t value;
    if (cmds == NULL)
        return;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-b") == 0)
        {
            if (++i < argc)
            {
                value = (uint16_t)atoi(argv[i]);
                if (value >= MIN_BLKSIZE && value <= MAX_BLKSIZE)
                    cmds->block_size = value;
            }
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value >= MIN_TIMEOUT && value <= MAX_TIMEOUT)
                    cmds->timeout = value;
            }
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            if (++i < argc)
            {
                value = strlen(argv[i]);
                if (value)
                    strncpy(cmds->directory, argv[i], value);
            }
        }
        else if (strcmp(argv[i], "-w") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value >= MIN_WINDOW_SIZE && value <= MAX_WINDOW_SIZE)
                    cmds->window_size = value;
            }
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            if (++i < argc)
            {
                value = (uint16_t)atoi(argv[i]);
                if (value >= MIN_PORT && value <= MAX_PORT)
                    cmds->port = value;
            }
        }
        else if (strcmp(argv[i], "-m") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value >= MIN_RETRIES && value <= MAX_RETRIES)
                    cmds->max_retries = value;
            }
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if (++i < argc)
            {
                value = atoi(argv[i]);
                if (value < UINT_MAX)
                    cmds->tsize = value;
            }
        }
    }

    return;
}