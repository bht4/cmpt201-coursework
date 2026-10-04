#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

int main(void) {
  char *path = NULL;
  size_t size = 0;

  while (1) {
    printf("Enter program to run.\n");
    printf("> ");

    if (getline(&path, &size, stdin) < 0) {
      break;
    }

    path[strlen(path) - 1] = '\0';

    pid_t pid = fork();

    if (pid < 0) {
      perror("fork failed");
      exit(EXIT_FAILURE);
    }

    if (pid == 0) {
      execl(path, path, (char *)NULL);

      perror("Exec failure");
      exit(EXIT_FAILURE);
    }

    if (waitpid(pid, NULL, 0) < 0) {
      perror("waitpid failed");
      exit(EXIT_FAILURE);
    }
  }

  free(path);

  return 0;
}
