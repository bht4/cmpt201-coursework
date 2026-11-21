#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 10
#define MSG_SIZE 1024
#define HEADER_SIZE (1 + sizeof(uint32_t) + sizeof(uint16_t))
#define RESPONSE_SIZE (BUF_SIZE + sizeof(uint32_t) + sizeof(uint16_t))
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

size_t generate_random_msg(char *msg) {
  uint8_t buf[BUF_SIZE];
  char hex_str[BUF_SIZE * 2 + 1];
  if (getentropy(buf, BUF_SIZE) == -1) { // 256 is the maximum permitted value
    return -1;
  }

  if (convert(buf, sizeof(buf), hex_str, sizeof(hex_str)) == -1) {
    return -1;
  }

  size_t hex_len = sizeof(hex_str) - 1; // Exclude '/0' at the end

  msg[0] = 0x00;
  memcpy(msg + 1, hex_str, hex_len);
  msg[hex_len + 1] = '\n';

  return hex_len + 2;
}

struct log_info {
  int server_fd;
  int log_fd;
};

void *logger(void *arg) {
  struct log_info *log = (struct log_info *)arg;
  char response[RESPONSE_SIZE];
  size_t current_response_size = 0;
  ssize_t bytes_read;
  char *newline_position;
  size_t msg_len;
  uint32_t sender_ip;
  uint16_t sender_port;
  char sender_ip_str[INET_ADDRSTRLEN];
  char chat_msg[MSG_SIZE];
  char log_line[MSG_SIZE + 32];
  int log_line_len;

  while (1) {
    if (response[0] == 0x01) { // The server terminates when all clients have done
      break;
    }

    // Message interpretation should be similar to server
    newline_position = NULL;
    if (current_response_size > HEADER_SIZE) {
      newline_position = memchr(log, '\n', current_response_size);
    }

    if (newline_position == NULL) { // No complete response
      if (current_response_size == sizeof(response)) {
        break;
      }

      bytes_read = read(log->server_fd, response + current_response_size,
                        sizeof(response) - current_response_size);
      if (bytes_read <= 0) {
        break;
      }

      current_response_size += bytes_read;
      continue;
    }

    msg_len = newline_position - response + 1;
    memcpy(&sender_ip, response + 1, sizeof(uint32_t));
    memcpy(&sender_port, response + 5, sizeof(uint16_t));
    chat_msg[msg_len - HEADER_SIZE] = '\n';

    log_line_len = snprintf(log_line, sizeof(log_line), "%-15s%-10u%s", sender_ip_str,
                            ntohs(sender_port), chat_msg);
    if (write(STDOUT_FILENO, log_line, log_line_len) != log_line_len ||
        write(log->log_fd, log_line, log_line_len) != log_line_len) {
      handle_error("write");
    }

    // The next buffer starts from where the previous message ends in memory
    memmove(response, response + msg_len, current_response_size - msg_len);
    current_response_size -= msg_len;
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  in_port_t server_port = atoi(argv[2]);
  size_t message_count = atoi(argv[3]);
  struct sockaddr_in server_addr;
  char msg[MSG_SIZE];
  ssize_t msg_len;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    handle_error("socket");
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    handle_error("connect");
  }

  struct log_info log_args;
  log_args.server_fd = server_fd;
  log_args.log_fd = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 00644);
  if (log_args.log_fd == -1) {
    handle_error("open");
  }
  // Must run logging in a dedicated thread to achieve concurrency
  pthread_t log_thread;
  if (pthread_create(&log_thread, NULL, logger, &log_args) != 0) {
    handle_error("pthread_create");
  }

  // Generate random messages
  for (int i = 0; i < message_count; i++) {
    if ((msg_len = generate_random_msg(msg)) == -1) {
      handle_error("Failed to generate messages");
    }

    // Send generated type 0 messages to the server
    if (write(server_fd, msg, msg_len) != msg_len) {
      handle_error("write");
    }
  }

  // After sending all type 0 messages, send type 1 message
  char terminate_msg[2] = {0x01, '\n'};
  size_t terminate_msg_len = sizeof(terminate_msg);
  if (write(server_fd, terminate_msg, terminate_msg_len) != terminate_msg_len) {
    handle_error("write");
  }

  if (pthread_join(log_thread, NULL) != 0) {
    handle_error("pthread_join");
  }

  close(log_args.log_fd);
  close(server_fd);
}
