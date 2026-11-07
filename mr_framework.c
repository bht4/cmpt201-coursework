#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "mr_framework_grouping.h"
#include "mr_framework_partition.h"
#include "mr_framework_threading.h"

static struct mr_emitted_pairs *mr_inter_pairs;
static size_t mr_inter_count;

static struct mr_emitted_pairs *mr_final_pairs;
static size_t mr_final_count;

int mr_exec(const struct mr_input *input, void (*map)(const struct mr_in_kv *), size_t mapper_count,
            void (*reduce)(const struct mr_out_kv *), size_t reducer_count,
            struct mr_output *output) {
  // Partition the input key-value pairs into multiple chunks
  struct mr_chunk_t map_chunks[mapper_count];
  partition(input->kv_lst, sizeof(struct mr_in_kv), input->count, mapper_count, map_chunks);

  // Create a set of threads to run each chunk
  union mr_thread_fn map_fn = {.map = map};
  execute_chunks_in_threads(MR_MAP, mapper_count, map_chunks, map_fn);

  // Group intermediate outputs by keys
  struct mr_output grouped_inter; // Intermediate and final output share the same structure
  mr_sort_and_group_by_keys(mr_inter_pairs, mr_inter_count, &grouped_inter);
  // Reduce
  struct mr_chunk_t reduce_chunks[reducer_count];
  partition(grouped_inter.kv_lst, sizeof(struct mr_out_kv), grouped_inter.count, reducer_count,
            reduce_chunks);

  union mr_thread_fn reduce_fn = {.reduce = reduce};
  execute_chunks_in_threads(MR_REDUCE, reducer_count, reduce_chunks, reduce_fn);

  mr_sort_and_group_by_keys(mr_final_pairs, mr_final_count, output);
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

int mr_emit_f(const char *key, const char *value) { return 0; }
