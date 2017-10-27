#include "salesman.h"

#include <assert.h>
#include <limits.h>
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

int PathCompare(const void* a, const void* b) {
  const Path* a_path = (const Path*)a;
  const Path* b_path = (const Path*)b;
  return a_path->fitness - b_path->fitness;
}

typedef struct MutateJob {
  RandomProvider* provider;
  Path* paths;
  const graph_t* graph;
  size_t paths_count;
} MutateJob;

typedef struct CrossoverJob {
  RandomProvider* provider;
  const Path* paths;
  size_t paths_count;
  Path* output;
  size_t output_count;
} CrossoverJob;

void StopTask(void* in) {
  ThreadPool* pool = in;
  ThreadPoolShutdown(pool);
}

int Fitness(const Path* path, const graph_t* graph) {
  int result = 0;
  assert(path->length > 1);
  int first = 0;
  int second = 1;
  for (; second < path->length; ++first, ++second) {
    result += graph_weight(graph, first, second);
  }
  result += graph_weight(graph, 0, path->length - 1);
  return result;
}

int VerifyPermutation(const Path* path) {
  int *used = calloc(path->length, sizeof(int));
  size_t i;
  for (i = 0; i < path->length; ++i) {
    used[path->path[i]] = 1;
  }
  for (i = 0; i < path->length; ++i) {
    if (!used[i]) {
      for (size_t j = 0; j < path->length; ++j)
        printf("%d ", used[j]);
      printf("\n");
      return 0;
    }
  }
  free(used);
  return 1;
}

void Mutate(Path* path, RandomChunk* chunk) {
  assert(VerifyPermutation(path));
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
  assert(VerifyPermutation(path));
}

void MutateTask(void* in) {
  size_t i;
  MutateJob* task = (MutateJob*)in;
  RandomChunk* chunk = RandomChunkCreate(task->provider);
  for (i = 0; i < task->paths_count; ++i) {
    Path* path = task->paths + i;
    Mutate(path, chunk);
    path->fitness = Fitness(path, task->graph);
    assert(path->fitness > 0);
  }
  RandomChunkDelete(chunk);
  free(task);
}

void Crossover(const Path* left, const Path* right, Path* result) {
  int* used = calloc(left->length, sizeof(size_t));
  size_t result_cursor;
  size_t right_cursor;
  result->length = left->length;
  assert(right->length == left->length);
  assert(VerifyPermutation(left));
  assert(VerifyPermutation(right));
  for (result_cursor = 0; result_cursor < result->length / 2; ++result_cursor) {
    result->path[result_cursor] = left->path[result_cursor];
    used[left->path[result_cursor]] = 1;
  }
  assert(VerifyPermutation(right));
  for (right_cursor = 0; right_cursor < right->length; ++right_cursor) {
    if (!used[right->path[right_cursor]]) {
      assert(VerifyPermutation(right));
      result->path[result_cursor] = right->path[right_cursor];
      used[right->path[right_cursor]] = 1;
      ++result_cursor;
      assert(VerifyPermutation(right));
    }
    assert(VerifyPermutation(right));
  }
  assert(VerifyPermutation(right));
  if (result_cursor != result->length) {
    for (int i = 0; i < left->length; i++) {
      printf("%d ", used[i]);
    }
    printf("\n");
    for (int i = 0; i < left->length; i++) {
      printf("%d ", right->path[i]);
    }
    printf("\n");
    for (int i = 0; i < left->length; i++) {
      printf("%d ", left->path[i]);
    }
    printf("\n");
    printf("ASSERT FAILED, %lu != %lu\n", result_cursor, result->length);
    assert(result_cursor == result->length);
  }
  free(used);
}

void CrossoverTask(void* in) {
  CrossoverJob* task = (CrossoverJob*)in;
  size_t cursor;
  RandomChunk* chunk = RandomChunkCreate(task->provider);
  printf("%p, %p\n", task->paths, task->paths + task->paths_count);
  for (cursor = 0; cursor < task->output_count; ++cursor) {
    size_t rand1 = RandomChunkPopRandomLong(chunk) % task->paths_count;
    size_t rand2 = RandomChunkPopRandomLong(chunk) % task->paths_count;
    Crossover(task->paths + rand1, task->paths + rand2, task->output + cursor);
  }
  RandomChunkDelete(chunk);
  free(task);
}

int* ShortestPath(const graph_t* graph,
                  size_t thread_count,
                  size_t population_size,
                  size_t same_fitness_for) {
  ThreadPool thread_pool;
  int* best = (int*) malloc(graph->n * sizeof(int));
  int best_fitness = INT_MAX;
  size_t children_size = population_size * kReproductionFactor;
  Path* population = malloc(population_size * sizeof(Path));
  Path* children = malloc(children_size * sizeof(Path));
  RandomProvider* provider = RandomProviderCreate();
  ThreadPoolInit(&thread_pool, thread_count);
  {
    size_t i;
    for (i = 0; i < population_size; ++i) {
      size_t j;
      population[i].length = graph->n;
      population[i].path = (int*)malloc(sizeof(int) * graph->n);
      for (j = 0; j < graph->n; ++j) {
        population[i].path[j] = j;
      }
    }
    for (i = 0; i < children_size; ++i) {
      children[i].path = (int*)malloc(sizeof(int) * graph->n);
    }
  }
  while (1) {
    printf("New iteration\n");
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
        CrossoverJob* job_task = (CrossoverJob*)malloc(sizeof(CrossoverJob));
        ThreadTask* pool_task = (ThreadTask*)malloc(sizeof(ThreadTask));
        job_task->provider = provider;
        job_task->paths = population;
        job_task->paths_count = population_size;
        job_task->output = children + child_offset;
        job_task->output_count = chunk_size;
        assert(job_task->output + job_task->output_count <= children + children_size);
        child_offset += chunk_size;
        ThreadPoolCreateTask(pool_task, job_task, CrossoverTask);
        ThreadPoolAddTask(&thread_pool, pool_task);
      }

      ThreadPoolShutdown(&thread_pool);
      ThreadPoolStart(&thread_pool);
      ThreadPoolJoin(&thread_pool);
    }
    printf("Passed the crossover stage\n");
    ThreadPoolReset(&thread_pool);
    // Mutation
    {
      size_t offset = 0;

      while (offset < population_size) {
        size_t chunk_size;
        if (population_size - offset < kPathsPerMutationTask) {
          chunk_size = population_size - offset;
        } else {
          chunk_size = kPathsPerMutationTask;
        }
        MutateJob* job_task = (MutateJob*)malloc(sizeof(CrossoverJob));
        ThreadTask* pool_task = (ThreadTask*)malloc(sizeof(ThreadTask));
        job_task->provider = provider;
        job_task->paths = population;
        job_task->paths_count = population_size;
        job_task->graph = graph;
        offset += chunk_size;
        ThreadPoolCreateTask(pool_task, job_task, MutateTask);
        ThreadPoolAddTask(&thread_pool, pool_task);
      }

      ThreadPoolShutdown(&thread_pool);
      ThreadPoolStart(&thread_pool);
      ThreadPoolJoin(&thread_pool);
    }
    printf("Passed the mutation stage\n");
    ThreadPoolReset(&thread_pool);
    // Selection
    {
      qsort(children, children_size, sizeof(Path), PathCompare);
      printf("%d\n", children[0].fitness);
      if (children[0].fitness < best_fitness) {
        memcpy(best, children[0].path, sizeof(int) * graph->n);
        best_fitness = children[0].fitness;
      }
      memcpy(population, children, population_size);
    }
  }
  RandomProviderDelete(provider);
  ThreadPoolDestroy(&thread_pool);
  {
    size_t i;
    for (i = 0; i < population_size; ++i) {
      free(population[i].path);
    }
    for (i = 0; i < children_size; ++i) {
      free(children[i].path);
    }
  }
  free(population);
  free(children);
  return best;
}