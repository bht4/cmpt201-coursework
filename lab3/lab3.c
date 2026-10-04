#define _POSIX_C_SOURCE 200809
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char *lines[5] = {NULL};
  char *input = NULL;
  size_t input_size = 0;
  int count = 0;
  int next = 0;

  while (1) {
    printf("Enter input: ");
    fflush(stdout);

    if (getline(&input, &input_size, stdin) == -1)
      break;

    input[strlen(input) - 1] = '\0';

    free(lines[next]);
    lines[next] = strdup(input);

    next = (next + 1) % 5;

    if (count < 5) {
      count++;
    }

    if (strcmp(input, "print") == 0) {
      int start = (count == 5) ? next : 0;

      for (int i = 0; i < count; i++) {
        printf("%s\n", lines[(start + i) % 5]);
      }
    }
  }

  for (int i = 0; i < 5; i++) {
    free(lines[i]);
  }

  free(input);

  return 0;
}
