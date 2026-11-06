#include "interface.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct chunk_t {
  struct mr_in_kv *kv_list;
  size_t count;
};

struct thread_map_args {
  void (*map)(const struct mr_in_kv *);
  struct chunk_t *chunk;
};

static void *thread_map(void *arg) {
  struct thread_map_args *map_args = (struct thread_map_args *)arg;
  for (int i = 0; i < map_args->chunk->count; i++) {
    map_args->map(&map_args->chunk->kv_list[i]);
  }

  return NULL;
}

static struct chunk_t **partition(struct mr_in_kv *pairs, size_t total_pairs, size_t total_chunks) {
  // Predetermine how many pairs each chunk should hold in advance
  // E.g. 10 pairs and 3 chunks => chunk 0 holds 4 pairs, chunk 1 and chunk 2 each hold 3 pairs
  int top_chunks = total_pairs / total_chunks; // The top chunks hold pairs_per_chunk + 1
  int pairs_per_chunk = total_pairs % total_chunks;
  struct chunk_t **chunks = malloc(total_chunks * sizeof(struct chunk_t));
  int checkpoint = 0;
  for (int i = 0; i < total_chunks; i++) {
    struct chunk_t *chunk = malloc(sizeof(struct chunk_t));
    if (i < top_chunks) {
      chunk->count = pairs_per_chunk + 1;
    } else {
      chunk->count = pairs_per_chunk;
    }
    for (int j = 0; j < chunk->count; j++) {
      chunk->kv_list[j] = pairs[j + checkpoint];
    }
    checkpoint += chunk->count;
    chunks[i] = chunk;
  }

  return chunks;
}

int mr_exec(const struct mr_input *input, void (*map)(const struct mr_in_kv *), size_t mapper_count,
            void (*reduce)(const struct mr_out_kv *), size_t reducer_count,
            struct mr_output *output) {
  // Partition the input key-value pairs into multiple chunks
  struct chunk_t **chunks = partition(input->kv_lst, input->count, mapper_count);

  // Create a set of threads to run each chunk
  pthread_t thread_ids[mapper_count];

  for (int i = 0; i < mapper_count; i++) {
    pthread_create(&threads[i], NULL, thread_map, chunks[i]);
  }
