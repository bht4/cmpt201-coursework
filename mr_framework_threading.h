#ifndef FR_THREADING
#define FR_THREADING

#include "interface.h"
#include "mr_framework_partition.h"

enum mr_stage { MR_MAP, MR_REDUCE };

struct mr_thread_args {
  enum mr_stage stage;
  union {
    void (*map)(const struct mr_in_kv *);
    void (*reduce)(const struct mr_out_kv *);
  } thread_fn;
  const void *items; // pointer to the first item in the chunk
  size_t count;      // number of key-value pairs in the chunk
};

void *thread_run(void *arg);

void execute_chunks_in_threads(size_t total_threads, const struct mr_chunk_t *chunks);

#endif
