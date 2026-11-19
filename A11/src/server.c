#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024

#define handle_error(msg)                                                                          \
  do {                                                                                             \
    perror(msg);                                                                                   \
    exit(EXIT_FAILURE);                                                                            \
  } while (0)

int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_size;
  in_port_t server_port = atoi(argv[0]);
  int client_count = atoi(argv[1]);

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    handle_error("socket");
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port =
      server_port; // Port must be below 1024 to be bindable according to man page
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to all IP addresses

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    handle_error("bind");
  }

  if (listen(server_fd, 0) == -1) { // May change backlog later
    handle_error("listen");
  }

  client_addr_size = sizeof(client_addr);
  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_size);
  if (client_fd == -1) {
    handle_error("accept");
  }

  char buf[BUF_SIZE];
  ssize_t msg_bytes_read;
  uint8_t msg_type;
  ssize_t msg_type_bytes_read;
  ssize_t bytes_write;
  msg_type_bytes_read = read(client_fd, &msg_type,
                             sizeof(msg_type)); // Read the first byte to identify the message type
  if (msg_type_bytes_read == -1) {
    handle_error("read");
  }

  if (msg_type == 0x00) {
    while ((msg_bytes_read = read(client_fd, &buf, BUF_SIZE - 1)) > 0) { // Preserve the last byte
                                                                         // for newline character
      buf[msg_bytes_read] = '\n';
      // Get the client's IP address and port through client_fd
      bytes_write = write(client_fd, buf, msg_bytes_read);

      if (bytes_write == -1) {
        handle_error("write");
      }
    }

    if (msg_bytes_read == -1) {
      handle_error("read");
    }
  } else {
  }

  if (close(server_fd) == -1 || close(client_fd) == -1) {
    handle_error("close");
  }
}
