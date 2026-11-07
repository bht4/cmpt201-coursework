#include "interface.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mr_chunk_t {
  const void *kv_list;
  size_t count;
};

static void partition(const void *pairs, size_t pair_size, size_t total_pairs, size_t total_chunks,
                      struct mr_chunk_t *chunks) {
  size_t pairs_per_chunk = total_pairs / total_chunks; // The top chunks hold pairs_per_chunk + 1
  size_t top_chunks = total_pairs % total_chunks;
  int checkpoint = 0;
  for (size_t i = 0; i < total_chunks; i++) {
    chunks[i].kv_list = (const char *)pairs + checkpoint * pair_size;
    chunks[i].count = pairs_per_chunk + (i < top_chunks ? 1 : 0);
    checkpoint += chunks[i].count;
  }
}

struct mr_thread_map_args {
  void (*map)(const struct mr_in_kv *);
  struct mr_chunk_t chunk;
};

static void *thread_map(void *arg) {
  struct mr_thread_map_args *map_args = (struct mr_thread_map_args *)arg;
  for (size_t i = 0; i < map_args->chunk.count; i++) {
    map_args->map(&map_args->chunk.kv_list[i]);
  }

  return NULL;
}

struct mr_thread_reduce_args {
  void (*reduce)(const struct mr_out_kv *);
  struct mr_chunk_t chunk;
};

static void *thread_reduce(void *arg) {
  struct mr_thread_reduce_args *reduce_args = (struct mr_thread_reduce_args *)arg;
  for (size_t i = 0; i < reduce_args->chunk.count; i++) {
    reduce_args->reduce(&reduce_args->chunk.kv_list[i]);
  }

  return NULL;
}

struct mr_inter_pair {
  char key[MAX_KEY_SIZE];
  char value[MAX_KEY_SIZE];
  size_t seq; // Used for sorting
};

static struct mr_inter_pair *mr_inter_pairs = NULL;
static size_t mr_inter_count = 0, mr_inter_cap = 0;

static int compare_pairs(const void *a, const void *b) {
  const struct mr_inter_pair *first_pair = a, *second_pair = b;
  int comp = strcmp(first_pair->key, second_pair->key);
  if (comp != 0)
    return comp; // Both keys are the same
  return (first_pair->seq > second_pair->seq) - (first_pair->seq < second_pair->seq);
}

// We allocate for the worst case where each pair is completely different from one and another
static struct mr_out_kv *mr_groups = NULL;
static size_t mr_group_count = 0;

static int group_by_keys(void) {
  mr_groups = NULL;
  mr_group_count = 0;
  if (mr_inter_count == 0)
    return 0;

  mr_groups = calloc(mr_inter_count, (sizeof *mr_groups));
  if (mr_groups == NULL)
    return -1;

  const char *target_key = NULL; // Track the current group
  for (size_t i = 0; i < mr_inter_count; i++) {
    const char *current_key = mr_inter_pairs[i].key;

    // New key or no key -> start a new group
    if (target_key == NULL || strcmp(current_key, target_key) != 0) {
      memcpy(mr_groups[mr_group_count++].key, current_key, MAX_KEY_SIZE);
      target_key = current_key;
    }

    // The current group is always the last one
    struct mr_out_kv *current_group = &mr_groups[mr_group_count - 1];
    char(*value)[MAX_VALUE_SIZE] =
        realloc(current_group->value, (current_group->count + 1) * MAX_VALUE_SIZE);
    if (value == NULL)
      return -1;
    current_group->value = value;
    memcpy(current_group->value[current_group->count], mr_inter_pairs[i].value, MAX_VALUE_SIZE);
    current_group->count++;
  }
  return 0;
}

int mr_exec(const struct mr_input *input, void (*map)(const struct mr_in_kv *), size_t mapper_count,
            void (*reduce)(const struct mr_out_kv *), size_t reducer_count,
            struct mr_output *output) {
  // Partition the input key-value pairs into multiple chunks
  struct mr_chunk_t map_chunks[mapper_count];
  partition(input->kv_lst, sizeof(struct mr_in_kv), input->count, mapper_count, map_chunks);

  // Create a set of threads to run each chunk
  pthread_t map_threads[mapper_count];
  struct mr_thread_map_args map_args[mapper_count];
  for (size_t i = 0; i < mapper_count; i++) {
    map_args[i].map = map;
    map_args[i].chunk = map_chunks[i];
    pthread_create(&map_threads[i], NULL, thread_map, &map_args[i]);
  }

  for (size_t i = 0; i < mapper_count; i++) {
    pthread_join(map_threads[i], NULL);
  }

  // Group intermediate outputs by keys
  qsort(mr_inter_pairs, mr_inter_count, sizeof *mr_inter_pairs, compare_pairs);
  group_by_keys();

  // Reduce
  struct mr_chunk_t reduce_chunks[reducer_count];
  partition(mr_groups, sizeof(struct mr_out_kv), mr_group_count, reducer_count, reduce_chunks);
  pthread_t reduce_threads[reducer_count];
  struct mr_thread_reduce_args reduce_args[reducer_count];
  for (size_t i = 0; i < reducer_count; i++) {
    reduce_args[i].reduce = reduce;
    reduce_args[i].chunk = reduce_chunks[i];
    pthread_create(&reduce_threads[i], NULL, thread_reduce, &reduce_args[i]);
  }

  for (size_t i = 0; i < reducer_count; i++) {
    pthread_join(reduce_threads[i], NULL);
  }
  return 0;
}

static pthread_mutex_t mr_inter_lock = PTHREAD_MUTEX_INITIALIZER;

int mr_emit_i(const char *key, const char *value) {
  pthread_mutex_lock(&mr_inter_lock);

  if (mr_inter_count == mr_inter_cap) { // Used up the current memory available
    size_t new_cap =
        mr_inter_cap ? mr_inter_cap * 2 : 64; // Double the heap size to allocate more memory
    struct mr_inter_pair *new_inter = realloc(mr_inter_pairs, new_cap * (sizeof *new_inter));
    if (new_inter == NULL) { // realloc fails
      pthread_mutex_unlock(&mr_inter_lock);
      return -1;
    }
    mr_inter_pairs = new_inter;
    mr_inter_cap = new_cap;
  }

  struct mr_inter_pair *slot = &mr_inter_pairs[mr_inter_count];
  strncpy(slot->key, key, MAX_KEY_SIZE - 1); // Safely populate the key so thtat it can be
                                             // terminated
  slot->key[MAX_KEY_SIZE - 1] = '\0';
  strncpy(slot->value, value, MAX_KEY_SIZE - 1);
  slot->value[MAX_KEY_SIZE - 1] = '\0';
  slot->seq = mr_inter_count; // arrival order
  mr_inter_count++;

  pthread_mutex_unlock(&mr_inter_lock);
  return 0;
}

static pthread_mutex_t mr_final_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mr_final_pair *mr_final_pairs = NULL;
static size_t mr_final_count = 0, mr_final_cap = 0;

int mr_emit_f(const char *key, const char *value) {
  pthread_mutex_lock(&mr_final_lock);

  if (mr_final_count == mr_final_cap) { // Used up the current memory available
    size_t new_cap =
        mr_final_cap ? mr_final_cap * 2 : 64; // Double the heap size to allocate more memory
    struct mr_final_pair *new_final = realloc(mr_final_pairs, new_cap * (sizeof *new_final));
    if (new_final == NULL) { // realloc fails
      pthread_mutex_unlock(&mr_final_lock);
      return -1;
    }
    mr_final_pairs = new_final;
    mr_final_cap = new_cap;
  }

  struct mr_final_pair *slot = &mr_final_pairs[mr_final_count];
  strncpy(slot->key, key, MAX_KEY_SIZE - 1); // Safely populate the key so thtat it can be
                                             // terminated
  slot->key[MAX_KEY_SIZE - 1] = '\0';
  strncpy(slot->value, value, MAX_KEY_SIZE - 1);
  slot->value[MAX_KEY_SIZE - 1] = '\0';
  slot->seq = mr_final_count; // arrival order
  mr_final_count++;

  pthread_mutex_unlock(&mr_final_lock);
  return 0;

  return 0;
}
