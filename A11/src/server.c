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

struct client_info *clients; // Needed to store all active clients to send message to all later
int client_count = 0;

pthread_mutex_t client_count_lock = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
  struct client_info *client = arg;
  int client_fd = client->cfd;
  struct sockaddr_in client_addr = client->addr;
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

  return NULL;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in server_addr;
  socklen_t client_addr_size;
  in_port_t server_port = atoi(argv[1]);
  int client_count = atoi(argv[2]);

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

    pthread_mutex_lock(&client_count_lock);
    client_count++;
    pthread_mutex_unlock(&client_count_lock);

    struct client_info *client = malloc(sizeof(struct client_info));
    if (client == NULL) {
      close(client_fd);
      continue;
    }

    client->cfd = client_fd;
    client->addr = client_addr;

    pthread_t thread;
    if (pthread_create(&thread, NULL, handle_client, client) != 0) {
      handle_error("pthread_create");
    }

    pthread_detach(thread);
  }

  if (close(server_fd) == -1) {
    handle_error("close");
  }

  for (int i = 0; i < client_count; i++) {
    if (close(clients[i].cfd) == -1) {
      handle_error("close");
    }
    free(&clients[i]);
  }
}
