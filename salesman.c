#include "salesman.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "random_chunk.h"
#include "random_provider.h"
#include "thread_pool.h"

const size_t kReproductionFactor = 4;
const size_t kSwapsPerMutation = 1;
const size_t kPathsPerMutationTask = 16;
const size_t kPathsPerCrossoverTask = 64;

typedef struct Path {
  int* path;
  int fitness;
  size_t length;
} Path;

typedef struct MutateJob {
  RandomProvider* provider;
  Path* paths;
  const graph_t* graph;
  size_t count;
} MutateJob;

typedef struct CrossoverJob {
  RandomProvider* provider;
  const Path* paths;
  size_t paths_count;
  Path* output;
  size_t output_count;
} CrossoverJob;

int Fitness(const Path* path, const graph_t* graph) {
  int result;
  assert(path->length > 1);
  int first = 0;
  int second = 1;
  for (; second < path->length; ++first, ++second) {
    result += graph_weight(graph, first, second);
  }
  return result;
}

void Mutate(Path* path, RandomChunk* chunk) {
  size_t i;
  for (i = 0; i < kSwapsPerMutation; i++) {
    size_t rand1 = RandomChunkPopRandom(chunk);
    size_t rand2 = RandomChunkPopRandom(chunk);
    size_t pos1 = rand1 % path->length;
    size_t pos2 = rand2 % path->length;
    int temp = path->path[pos1];
    path->path[pos1] = path->path[pos2];
    path->path[pos2] = temp;
  }
}

void MutateTask(void* in) {
  size_t i;
  MutateJob* task = (MutateJob*)in;
  RandomChunk* chunk = RandomChunkCreate(task->provider);
  for (i = 0; i < task->count; ++i) {
    Path* path = task->paths + i;
    Mutate(path, chunk);
    path->fitness = Fitness(path, task->graph);
  }
  RandomChunkDelete(chunk);
  free(task);
}

void Crossover(const Path* left, const Path* right, Path* result) {
  int* used = calloc(left->length, sizeof(size_t));
  size_t result_cursor;
  size_t right_cursor;
  result->length = left->length;
  for (result_cursor = 0; result_cursor < result->length / 2; ++result_cursor) {
    result->path[result_cursor] = left->path[result_cursor];
    used[left->path[result_cursor]] = 1;
  }
  for (right_cursor = 0; right_cursor < right->length; ++right_cursor) {
    if (!used[right->path[right_cursor]]) {
      result->path[result_cursor] = right->path[right_cursor];
      ++result_cursor;
      used[right->path[right_cursor]] = 1;
    }
  }
  assert(result_cursor == result->length);
  free(used);
}

void CrossoverTask(void* in) {
  CrossoverJob* task = (CrossoverJob*)in;
  size_t cursor;
  RandomChunk* chunk = RandomChunkCreate(task->provider);
  for (cursor = 0; cursor < task->output_count; ++cursor) {
    size_t rand1 = RandomChunkPopRandomLong(chunk);
    size_t rand2 = RandomChunkPopRandomLong(chunk);
    Crossover(task->paths + rand1, task->paths + rand2, task->output + cursor);
  }
  RandomChunkDelete(chunk);
  free(task);
}

int* ShortestParth(const graph_t* graph,
                   size_t thread_count,
                   size_t population_size) {
  ThreadPool thread_pool;
  size_t children_size = population_size * kReproductionFactor;
  Path* population = malloc(population_size * sizeof(Path));
  Path* children = malloc(children_size * sizeof(Path));
  RandomProvider* provider = RandomProviderCreate();
  ThreadPoolInit(&thread_pool, thread_count);
  // Crossover
  {
    size_t child_offset = 0;
    while (child_offset < children_size) {
      size_t chunk_size;
      if (children_size - child_offset < kPathsPerCrossoverTask) {
        chunk_size = children_size - child_offset;
      } else {
        chunk_size = kPathsPerCrossoverTask;
      }
      CrossoverJob *job_task = (CrossoverJob*) malloc(sizeof(CrossoverJob));
      ThreadTask *pool_task = malloc(sizeof(ThreadTask));
      job_task->provider = provider;
      job_task->paths = population;
      job_task->paths_count = population_size;
      job_task->output = children + child_offset;
      job_task->output_count = chunk_size;
      child_offset += chunk_size;
      ThreadPoolCreateTask(pool_task, job_task, CrossoverTask);
    }
  }
  // Mutation
  {
    size_t i;
    size_t task_size = population_size / for (i = 0; i <)
  }

  ThreadPoolDestroy(&thread_pool);
  free(population);
  free(children);
}