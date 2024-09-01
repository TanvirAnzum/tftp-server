# TFTP Server

This project is a multithreaded Trivial File Transfer Protocol (TFTP) server that supports both IPv4 and IPv6. It is designed to be configurable through command-line arguments or by using a saved configuration file.

## Features

- **Compatibility:** The server supports both linux and windows operating systems.
- **Multi-threading:** Each TFTP session is handled in a separate thread, allowing for concurrent transfers. (currently disabled, you can enable by following comments on thread creation function on tftp.c file)
- **IPv6 Support:** The server supports both IPv4 and IPv6 protocols.
- **Configurable Parameters:**
  - Block size (`-b`)
  - Timeout (`-t`)
  - Transfer directory (`-d`)
  - Window size (`-w`)
  - Port number (`-p`)
  - Maximum retries (`-m`)
  - Transfer size (`-s`)
- **Error Handling:** Proper error handling with informative messages.
- **Option Negotiation:** Supports TFTP option negotiation (blksize, timeout, tsize).
- **Persistent Configuration:** The server can save its configuration to a file and preload it on startup.

## Getting Started

### Prerequisites

- **Operating System:** Linux or Windows
- **Compiler:** GCC for Linux, MinGW or Visual Studio for Windows
- **Libraries:** POSIX threads (Linux), Winsock (Windows)

### Building the Project

1. **Clone the Repository:**

   ```bash
   git clone https://github.com/TanvirAnzum/tftp-server
   cd tftp-server
   ```

2. **Compile the Project:**

   For **Linux**:

   ```bash
   gcc -o tftpd_server tftpd.c tftpd_cmd.c tftpd_rrq.c tftpd_wrq.c tftpd_utils.c tftpd_packet.c -lpthread -Wall
   ```

   For **Windows**:

   ```bash
   gcc -o tftpd_server tftpd.c tftpd_cmd.c tftpd_rrq.c tftpd_wrq.c tftpd_utils.c tftpd_packet.c -lws2_32 -Wall
   ```

   If **make** is installed (windows or linux):

   ```bash
   make
   ```

### Running the Server

You can start the TFTP server using the following command:

```bash
./tftp_server -b 2048 -t 5 -d /tftdirectory -w 1 -p 1069 -m 3 -s 0 -save
./tftp_server
/tftp_server.exe
```

#### Command-Line Options:

- `-b`: Block size (default: 512 bytes)
- `-t`: Timeout in seconds (default: 5 seconds)
- `-d`: Directory for storing/retrieving files (default: current directory)
- `-w`: Window size for transfers (default: 1 block)
- `-p`: Port number (default: 69)
- `-m`: Maximum retries for packet transmission (default: 3)
- `-s`: Transfer size (default: 0, no limit)
- `-save`: Save the current configuration to a file

### Configuration File

If the `-save` option is used, the server's configuration is saved to `tftpd_conf.bin` and can be automatically loaded on the next run.

### Example Usage

```bash
./tftpd -d /tftpboot -b 1024 -p 1069 -save
```

This command starts the server with a block size of 1024 bytes, listening on port 1069, and saves this configuration for future use.

### Error Handling

The server handles various errors such as invalid opcode, file not found, and session socket failures. Errors are logged with details like the source file, function name, and line number for easy debugging.

### Testing

This module is tested against normal cases. Formal test cases will be developed soon.

### Author

Md. Tanvir Anzum, R&D Engineer, BDCOM.
