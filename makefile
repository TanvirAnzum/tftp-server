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

# Header files
HEADERS = tftpd.h

# Default target to build the executable
all: $(TARGET)

# Rule to build the executable directly from source files
$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

# Clean up generated files
clean:
	$(RM) $(TARGET)

# Phony targets to avoid conflicts with files named 'all' or 'clean'
.PHONY: all clean
