#include "tftpd.h"

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

void print_client_address(struct sockaddr_in6 *src_addr)
{
    char ipstr[INET6_ADDRSTRLEN];
    int port;

    struct sockaddr_in6 *s = (struct sockaddr_in6 *)src_addr;
    port = ntohs(s->sin6_port);
    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
    printf("Client address: %s, Port: %d\n", ipstr, port);

    return;
}

/* digit count */
uint32_t digit_counter(long long unsigned n)
{
    uint32_t digit = 0;
    while (n)
    {
        digit++;
        n /= 10;
    }
    return digit;
}

/* is valid directory */
int is_valid_directory(const char *path)
{
    if (path == NULL || strlen(path) == 0)
        return FALSE;

    char full_path[MAX_PATH];

    // If path is relative, make it absolute
    if (path[0] != '/' && (path[0] == '\0' || path[1] != ':'))
    {
        if (getcwd(full_path, sizeof(full_path)) == NULL)
            return FALSE;

#ifdef WINDOWS
        strcat(full_path, "\\");
#else
        strcat(full_path, "/");
#endif

        strcat(full_path, path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

// Check if directory exists and is accessible
#ifdef WINDOWS
    DWORD attributes = GetFileAttributesA(full_path);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        return FALSE;
    }
#else
    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        return FALSE;
    }
#endif

// Check if directory is writable
#ifdef WINDOWS
    if (_access(full_path, 06) == -1)
    {
        return FALSE;
    }
#else
    if (access(full_path, W_OK | X_OK) == -1)
    {
        return FALSE;
    }
#endif

    return TRUE;
}

/* progress bar */
void update_progress_bar(p_tftp_session session)
{
    char time[TIME_BUFFER];

    if(!session->block_counter)
        return;

    if(session->block_counter % session->blocks_per_mb == 0) {
        memset(time, 0, TIME_BUFFER);
        get_local_time(time, TIME_BUFFER);
        CLR_WARNING;
        printf("%s: TFTP session %u has transferred %u bytes.\n", time, session->session_id, session->bytes_transferred);
        CLR_RESET;
    }

    return;
}

void get_local_time(char *buffer, size_t buffer_size)
{
    if (buffer == NULL)
    {
        return;
    }

#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, buffer_size, "%02d:%02d:%02d",
             st.wHour, st.wMinute, st.wSecond);
#else
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(buffer, buffer_size, "%H:%M:%S", time_info);
#endif
}
