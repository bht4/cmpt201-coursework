#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 10
#define MSG_SIZE 1024
#define CLIENT_IP "127.0.0.1"
#define handle_error(msg)                                                                          \
  do {                                                                                             \
    perror(msg);                                                                                   \
    exit(EXIT_FAILURE);                                                                            \
  } while (1)

int convert(uint8_t *buf, ssize_t buf_size, char *str, ssize_t str_size) {
  if (buf == NULL || str == NULL || buf_size <= 0 || str_size < (buf_size * 2 + 1)) {
    return -1;
  }

  for (int i = 0; i < buf_size; i++) {
    sprintf(str + i * 2, "%02X", buf[i]);
  }
  str[buf_size * 2] = '\0';

  return 0;
}

int generate_random_msg(char *msg, ssize_t msg_bytes) {
  uint8_t buf[BUF_SIZE];
  char hex_str[BUF_SIZE * 2 + 1];
  if (getentropy(buf, BUF_SIZE) == -1) { // 256 is the maximum permitted value
    return -1;
  }

  if (convert(buf, sizeof(buf), hex_str, sizeof(hex_str)) == -1) {
    return -1;
  }

  msg[0] = 0x00;
  memcpy(msg + 1, hex_str, sizeof(hex_str));
  msg[sizeof(hex_str) + 1] = '\n';

  msg_bytes = BUF_SIZE * 2 + 3;

  return 0;
}

int main(int argc, char *argv[]) {
  in_port_t server_port = atoi(argv[2]);
  struct sockaddr_in server_addr;
  char msg[MSG_SIZE];
  ssize_t bytes_generated;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    handle_error("socket");
  }

  // Enable SO_REUSEADDR to bypass TIME_WAIT
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    handle_error("setsockopt");
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  if (inet_pton(AF_INET, CLIENT_IP, &server_addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    handle_error("connect");
  }

  // Generate random message
  while (generate_random_msg(msg, bytes_generated) == 0) {
    // Send user inputs to server
    if (write(server_fd, msg, bytes_generated) != bytes_generated) {
      handle_error("write");
    }
  }

  close(server_fd);
}
