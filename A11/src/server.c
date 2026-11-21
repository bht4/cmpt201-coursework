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

struct client_info **clients =
    NULL;                 // Needed to store all active clients to send message to all later
int client_count = 0;     // The number of clients who are currently active in the group chat
int expected_terms;       // The expected number of terminated clients before the server terminates
                          // itself
int terminated_count = 0; // The number of clients who terminated

pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;

void send_all_clients(char *msg, size_t msg_len) {
  for (size_t i = 0; i < client_count; i++) {
    // Send functions similarly to write. In this case, it ensures that writing to a
    // client that already left must not kill the server
    send(clients[i]->cfd, msg, msg_len, MSG_NOSIGNAL);
  }
}

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
      if (bytes_read <= 0) { // The client left or read failed
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

      // Send to all clients. Use lock here to ensure that all clients see identical arrival order
      // of messages
      pthread_mutex_lock(&server_lock);
      send_all_clients(response, response_len);
      pthread_mutex_unlock(&server_lock);
    } else if (buf[0] == 0x01) {
      char terminate_msg[2] = {0x01, '\n'};
      pthread_mutex_lock(&server_lock);
      terminated_count++;
      if (terminated_count == expected_terms) {
        send_all_clients(terminate_msg, sizeof(terminate_msg));
        exit(EXIT_SUCCESS); // All clients are done, server auto terminates
      }
      pthread_mutex_unlock(&server_lock);
    }

    // The next buffer starts from where the previous message ends in memory
    memmove(buf, buf + msg_len, current_buf_size - msg_len);
    current_buf_size -= msg_len;
  }

  // The client left, need to remove it from the tracked list before freeing
  pthread_mutex_lock(&server_lock);
  for (size_t i = 0; i < client_count; i++) {
    if (clients[i] == client) {
      clients[i] = clients[client_count - 1]; // Whenever a client left, we fill the gap with the
                                              // one who most recently joined
      client_count--;
    }
  }
  pthread_mutex_unlock(&server_lock);

  close(client->cfd);
  free(client);

  return NULL;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr;
  socklen_t client_addr_size;
  in_port_t server_port = atoi(argv[1]);
  expected_terms = atoi(argv[2]);

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

    pthread_mutex_lock(&server_lock);
    struct client_info **resized_clients =
        realloc(clients, sizeof(struct client_info *) * (client_count + 1));
    if (resized_clients == NULL) {
      pthread_mutex_unlock(&server_lock);
      close(client_fd);
      free(client);
      continue;
    }
    clients = resized_clients;
    clients[client_count++] = client;
    pthread_mutex_unlock(&server_lock);

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
