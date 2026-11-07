#include "mr_framework_emitting.h"
#include "interface.h"
#include <stdlib.h>
#include <string.h>

static struct mr_emit_buffer mr_inter_buffer = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static struct mr_emit_buffer mr_final_buffer = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};

int mr_buffer_append(struct mr_emit_buffer *buffer, const char *key, const char *value) {
  pthread_mutex_t lock = buffer->lock;
  pthread_mutex_lock(&lock);
  struct mr_emitted_pair *pairs = buffer->pairs;
  size_t count = buffer->pair_count;
  size_t cap = buffer->capacity;

  if (count == cap) {                    // Used up the current memory available
    size_t new_cap = cap ? cap * 2 : 64; // Double the heap size to allocate more memory
    struct mr_emitted_pair *new_pairs = realloc(pairs, new_cap * (sizeof *new_pairs));
    if (new_pairs == NULL) { // realloc fails
      pthread_mutex_unlock(&lock);
      return -1;
    }
    pairs = new_pairs;
    cap = new_cap;
  }

  struct mr_emitted_pair *new_pair = &pairs[count];
  strncpy(new_pair->key, key, MAX_KEY_SIZE - 1); // Safely populate the key so thtat it can be
                                                 // terminated
  new_pair->key[MAX_KEY_SIZE - 1] = '\0';
  strncpy(new_pair->value, value, MAX_VALUE_SIZE - 1);
  new_pair->value[MAX_KEY_SIZE - 1] = '\0';
  new_pair->seq = count; // arrival order
  count++;

  buffer->pairs = pairs;
  buffer->pair_count = count;
  buffer->capacity = cap;
  pthread_mutex_unlock(&lock);
  return 0;
}

static void mr_reset_buffer(struct mr_emit_buffer *buffer) {
  free(buffer->pairs);
  buffer->pairs = NULL;
  buffer->pair_count = 0;
  buffer->capacity = 0;
}

void mr_emit_reset(void) {
  mr_reset_buffer(&mr_inter_buffer);
  mr_reset_buffer(&mr_final_buffer);
}

struct mr_emitted_pair *mr_get_inter_pairs(size_t *pair_count) {
  *pair_count = mr_inter_buffer.pair_count;
  return mr_inter_buffer.pairs;
}

struct mr_emitted_pair *mr_get_final_pairs(size_t *pair_count) {
  *pair_count = mr_final_buffer.pair_count;
  return mr_final_buffer.pairs;
}

int mr_emit_i(const char *key, const char *value) {
  return mr_buffer_append(&mr_inter_buffer, key, value);
}

int mr_emit_f(const char *key, const char *value) {
  return mr_buffer_append(&mr_final_buffer, key, value);
}
