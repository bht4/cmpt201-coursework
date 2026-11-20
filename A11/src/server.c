#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
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

struct client_info {
  int cfd;
  struct sockaddr_in addr;
};

struct client_info **clients; // Needed to store all active clients to send message to all later
int client_count = 0;         // The number of clients who are currently active in the group chat
int terminated_count;         // The number of clients who terminated

pthread_mutex_t client_count_lock = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
  struct client_info *client = arg;
  char buf[BUF_SIZE];
  ssize_t bytes_read;
  size_t current_buf_size = 0, msg_len;

  while (1) { // Need multiple reads to process all messages
    char *newline_position = memchr(buf, '\n', current_buf_size); // Search for \n inside the
                                                                  // currently read buffer
    if (newline_position == NULL) {                               // No complete string found
      if (current_buf_size == BUF_SIZE) { // 1024 bytes of characters without \n
        break;
      }

      // Read more, hoping to get a complete string
      bytes_read = read(client->cfd, buf + current_buf_size, BUF_SIZE - current_buf_size);
      if (bytes_read < 0) {
        break;
      }

      current_buf_size += bytes_read;
      continue; // Start the loop again to check for newline character after reading more bytes
    }

    msg_len = newline_position - buf + 1; // include \n
    // Check whether the first byte is of type 0 or 1
    if (buf[0] == 0x00) {
      char response[BUF_SIZE + sizeof(uint32_t) +
                    sizeof(uint16_t)]; // 1 for type, 4 for IP address, 2 for port and 1 for newline
      size_t response_len = msg_len + sizeof(uint32_t) + sizeof(uint16_t);
      response[0] = buf[0];
      memcpy(response + 1, &client->addr.sin_addr.s_addr, sizeof(uint32_t));
      memcpy(response + 5, &client->addr.sin_port, sizeof(uint16_t));
      memcpy(response + 7, buf + 1, msg_len - 1);

      // Send to all clients
      for (int i = 0; i < client_count; i++) {
        if (write(clients[i]->cfd, response, response_len) == -1) {
          break;
        }
      }
    } else {
    }

    // The next buffer starts from where the previous message ends in memory
    memmove(buf, buf + msg_len, current_buf_size - msg_len);
    current_buf_size -= msg_len;
  }

  close(client->cfd);
  free(client);

  return NULL;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr;
  socklen_t client_addr_size;
  in_port_t server_port = atoi(argv[1]);
  clients = malloc(sizeof(struct client_info) * client_count);

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    handle_error("socket");
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to all IP addresses

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    handle_error("bind");
  }

  if (listen(server_fd, SOMAXCONN) == -1) {
    handle_error("listen");
  }

  while (1) { // Accept multiple client requests
    struct sockaddr_in client_addr;
    client_addr_size = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_size);
    if (client_fd == -1) {
      handle_error("accept");
    }

    struct client_info *client = malloc(sizeof(struct client_info));
    if (client == NULL) {
      close(client_fd);
      continue;
    }

    client->cfd = client_fd;
    client->addr = client_addr;

    pthread_mutex_lock(&client_count_lock);
    clients[client_count++] = client;
    pthread_mutex_unlock(&client_count_lock);

    pthread_t thread;
    if (pthread_create(&thread, NULL, handle_client, client) != 0) {
      handle_error("pthread_create");
    }

    pthread_detach(thread);
  }

  if (close(server_fd) == -1) {
    handle_error("close");
  }
}
