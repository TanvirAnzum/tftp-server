# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Output binary name
TARGET = tftp_server.exe

# Source files
SRCS = tftpd.c tftpd_cmd.c tftpd_rrq.c tftpd_wrq.c tftpd_utils.c tftpd_packet.c

# Object files (replace .c with .o in the source files)
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = tftpd.h

# Library for Winsock
LIBS = -lws2_32

# Default target to build the executable
all: $(TARGET) clean_objects

# Rule to build the executable by linking the object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Rule to compile the source files into object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(TARGET) $(OBJS)

# Remove object files after successful build
clean_objects:
	rm -f $(OBJS)

# Phony targets to avoid conflicts with files named 'all' or 'clean'
.PHONY: all clean clean_objects
