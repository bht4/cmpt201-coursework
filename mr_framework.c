#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "mr_framework_emitting.h"
#include "mr_framework_grouping.h"
#include "mr_framework_partition.h"
#include "mr_framework_threading.h"

int mr_exec(const struct mr_input *input, void (*map)(const struct mr_in_kv *), size_t mapper_count,
            void (*reduce)(const struct mr_out_kv *), size_t reducer_count,
            struct mr_output *output) {
  // Reset all buffers
  mr_emit_reset();
  // Partition the input key-value pairs into multiple chunks
  struct mr_chunk_t map_chunks[mapper_count];
  partition(input->kv_lst, sizeof(struct mr_in_kv), input->count, mapper_count, map_chunks);

  // Create a set of threads to run each chunk
  union mr_thread_fn map_fn = {.map = map};
  execute_chunks_in_threads(MR_MAP, mapper_count, map_chunks, map_fn);

  // Group intermediate outputs by keys
  size_t mr_inter_count;
  struct mr_emitted_pair *mr_inter_pairs = mr_get_inter_pairs(&mr_inter_count);
  struct mr_output grouped_inter; // Intermediate and final output share the same structure
  mr_sort_and_group_by_keys(mr_inter_pairs, mr_inter_count, &grouped_inter);
  // Reduce
  struct mr_chunk_t reduce_chunks[reducer_count];
  partition(grouped_inter.kv_lst, sizeof(struct mr_out_kv), grouped_inter.count, reducer_count,
            reduce_chunks);

  union mr_thread_fn reduce_fn = {.reduce = reduce};
  execute_chunks_in_threads(MR_REDUCE, reducer_count, reduce_chunks, reduce_fn);

  size_t mr_final_count;
  struct mr_emitted_pair *mr_final_pairs = mr_get_final_pairs(&mr_final_count);
  mr_sort_and_group_by_keys(mr_final_pairs, mr_final_count, output);

  mr_free_groups(grouped_inter.kv_lst, grouped_inter.count);
  mr_emit_reset();
  return 0;
}
