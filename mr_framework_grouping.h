#ifndef FR_GROUPING
#define FR_GROUPING

#include "interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mr_emitted_pair {
  char key[MAX_KEY_SIZE];
  char value[MAX_KEY_SIZE];
  size_t seq; // Used for sorting
};

static int compare_pairs(const void *a, const void *b);

int sort_and_group_by_keys(struct mr_emitted_pair *emitted_pairs, size_t emitted_count,
                           struct mr_output *grouped_output);

void free_groups(struct mr_out_kv *groups, size_t count);

#endif
