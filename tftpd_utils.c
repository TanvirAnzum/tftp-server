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