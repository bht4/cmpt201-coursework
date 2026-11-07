#include "mr_framework_grouping.h"

static int compare_pairs(const void *a, const void *b) {
  const struct mr_emitted_pair *first_pair = a, *second_pair = b;
  int comp = strcmp(first_pair->key, second_pair->key);
  if (comp != 0)
    return comp; // Both keys are the same
  return (first_pair->seq > second_pair->seq) - (first_pair->seq < second_pair->seq);
}

int sort_and_group_by_keys(struct mr_emitted_pair *emitted_pairs, size_t emitted_count,
                           struct mr_output *grouped_output) {
  qsort(emitted_pairs, emitted_count, sizeof *emitted_pairs, compare_pairs);
  struct mr_out_kv *groups = grouped_output->kv_lst;
  size_t count = grouped_output->count;
  groups = NULL;
  count = 0;
  if (emitted_count == 0)
    return 0;

  groups = calloc(emitted_count, (sizeof *groups));
  if (groups == NULL)
    return -1;

  const char *target_key = NULL; // Track the current group
  for (size_t i = 0; i < emitted_count; i++) {
    const char *current_key = emitted_pairs[i].key;

    // New key or no key -> start a new group
    if (target_key == NULL || strcmp(current_key, target_key) != 0) {
      memcpy(groups[count++].key, current_key, MAX_KEY_SIZE);
      target_key = current_key;
    }

    // The current group is always the last one
    struct mr_out_kv *current_group = &groups[count - 1];
    char(*value)[MAX_VALUE_SIZE] =
        realloc(current_group->value, (current_group->count + 1) * MAX_VALUE_SIZE);
    if (value == NULL) {
      free_groups(groups, count);
      return -1;
    }
    current_group->value = value;
    memcpy(current_group->value[current_group->count], emitted_pairs[i].value, MAX_VALUE_SIZE);
    current_group->count++;
  }
  return 0;
}

void free_groups(struct mr_out_kv *groups, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(groups[i].value);
  }
  free(groups);
}
