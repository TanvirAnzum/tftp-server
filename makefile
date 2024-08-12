# RM command declaration
ifeq ($(OS),Windows_NT)
    RM = del /Q
	LIBS = -lws2_32
	TARGET = tftp_server.exe
else
    RM = rm -f
	LIBS = -lpthread
	TARGET = tftp_server
endif

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Source files
SRCS = tftpd.c tftpd_cmd.c tftpd_rrq.c tftpd_wrq.c tftpd_utils.c tftpd_packet.c

# Object files (replace .c with .o in the source files)
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = tftpd.h

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
	$(RM) $(TARGET) $(OBJS)

# Remove object files after successful build
clean_objects:
	$(RM) $(OBJS)

# Phony targets to avoid conflicts with files named 'all' or 'clean'
.PHONY: all clean clean_objects
