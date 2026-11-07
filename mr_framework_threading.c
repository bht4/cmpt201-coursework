#include "mr_framework_threading.h"
#include <pthread.h>

void *thread_run(void *arg) {
  const struct mr_thread_args *thread_args = arg;

  if (thread_args->stage == MR_MAP) {
    const struct mr_in_kv *kv_list = thread_args->items;
    for (size_t i = 0; i < thread_args->count; i++) {
      thread_args->thread_fn.map(&kv_list[i]);
    }
  } else {
    const struct mr_out_kv *kv_list = thread_args->items;
    for (size_t i = 0; i < thread_args->count; i++) {
      thread_args->thread_fn.reduce(&kv_list[i]);
    }
  }

  return NULL;
}

void execute_chunks_in_threads(enum mr_stage stage, size_t total_threads,
                               const struct mr_chunk_t *chunks, union mr_thread_fn thread_fn) {
  pthread_t threads[total_threads];
  struct mr_thread_args thread_args[total_threads];

  for (size_t i = 0; i < total_threads; i++) {
    thread_args[i].stage = stage;
    thread_args[i].items = chunks[i].kv_list;
    thread_args[i].count = chunks[i].count;
    thread_args[i].thread_fn = thread_fn;
    pthread_create(&threads[i], NULL, thread_run, &thread_args[i]);
  }

  for (size_t i = 0; i < total_threads; i++) {
    pthread_join(threads[i], NULL);
  }
}
