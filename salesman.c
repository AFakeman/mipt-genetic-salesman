#include "salesman.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

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
    result += graph_weight(graph, path->path[first], path->path[second]);
  }
  result += graph_weight(graph, 0, path->path[path->length - 1]);
  return result;
}

int VerifyPermutation(const Path* path) {
  int* used = calloc(path->length, sizeof(int));
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
  int* used = calloc(left->length, sizeof(int));
  size_t result_cursor;
  size_t right_cursor;
  result->length = left->length;
  assert(right->length == left->length);
  for (result_cursor = 0; result_cursor < result->length / 2; ++result_cursor) {
    result->path[result_cursor] = left->path[result_cursor];
    used[left->path[result_cursor]] = 1;
  }
  for (right_cursor = 0; right_cursor < right->length; ++right_cursor) {
    if (!used[right->path[right_cursor]]) {
      result->path[result_cursor] = right->path[right_cursor];
      used[right->path[right_cursor]] = 1;
      ++result_cursor;
    }
  }
  assert(VerifyPermutation(result));
  free(used);
}

void CrossoverTask(void* in) {
  CrossoverJob* task = (CrossoverJob*)in;
  size_t cursor;
  RandomChunk* chunk = RandomChunkCreate(task->provider);
  for (cursor = 0; cursor < task->output_count; ++cursor) {
    size_t rand1 = RandomChunkPopRandomLong(chunk) % task->paths_count;
    size_t rand2 = RandomChunkPopRandomLong(chunk) % task->paths_count;
    Crossover(task->paths + rand1, task->paths + rand2, task->output + cursor);
  }
  RandomChunkDelete(chunk);
  free(task);
}

// Swap |size| bytes at |a| and |b| (not overlapping);
void memswap(void* a, void* b, size_t size) {
  char* a_cast = (char*)a;
  char* b_cast = (char*)b;
  size_t i;
  for (i = 0; i < size; i++) {
    char tmp = a_cast[i];
    a_cast[i] = b_cast[i];
    b_cast[i] = tmp;
  }
}

void DumpPaths(const Path* arr, size_t count) {
  size_t path;
  for (path = 0; path < count; path++) {
    size_t coord;
    for (coord = 0; coord < arr[path].length; ++coord)
      printf("%d ", arr[path].path[coord]);
    printf("%d\n", arr[path].fitness);
  }
}

double timediff(struct timeval* a, struct timeval* b) {
  return ((a->tv_sec - b->tv_sec) * 1e6 + (a->tv_usec - b->tv_usec)) / 1.0e6;
}

int ShortestPath(const graph_t* graph,
                  size_t thread_count,
                  size_t population_size,
                  size_t same_fitness_for,
                  ShortestPathData *return_data) {
  ThreadPool thread_pool;
  int best_fitness = INT_MAX;
  size_t current_same_best = 0;
  size_t iterations = 0;
  size_t children_size = population_size * kReproductionFactor;
  Path* population = malloc(population_size * sizeof(Path));
  Path* children = malloc(children_size * sizeof(Path));
  RandomProvider* provider = RandomProviderCreate();
  struct timeval begin;
  struct timeval end;
  gettimeofday(&begin, NULL);
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
  while (current_same_best < same_fitness_for) {
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
        child_offset += chunk_size;
        ThreadPoolCreateTask(pool_task, job_task, CrossoverTask);
        ThreadPoolAddTask(&thread_pool, pool_task);
      }

      ThreadPoolShutdown(&thread_pool);
      ThreadPoolStart(&thread_pool);
      ThreadPoolJoin(&thread_pool);
    }
    ThreadPoolReset(&thread_pool);
    // Mutation
    {
      size_t child_offset = 0;

      while (child_offset < children_size) {
        size_t chunk_size;
        if (children_size - child_offset < kPathsPerMutationTask) {
          chunk_size = children_size - child_offset;
        } else {
          chunk_size = kPathsPerMutationTask;
        }
        MutateJob* job_task = (MutateJob*)malloc(sizeof(MutateJob));
        ThreadTask* pool_task = (ThreadTask*)malloc(sizeof(ThreadTask));
        job_task->provider = provider;
        job_task->paths = children + child_offset;
        job_task->paths_count = chunk_size;
        job_task->graph = graph;
        child_offset += chunk_size;
        ThreadPoolCreateTask(pool_task, job_task, MutateTask);
        ThreadPoolAddTask(&thread_pool, pool_task);
      }

      ThreadPoolShutdown(&thread_pool);
      ThreadPoolStart(&thread_pool);
      ThreadPoolJoin(&thread_pool);
    }
    ThreadPoolReset(&thread_pool);
    // Selection
    {
      size_t i;
      double average_fitness = 0;
      qsort(children, children_size, sizeof(Path), PathCompare);
      assert(children[0].fitness == Fitness(children, graph));
      for (i = 0; i < children_size; ++i) {
        average_fitness += children[i].fitness;
      }
      average_fitness /= children_size;
      printf("Iteration %lu best: %d worst: %d average: %lf\n", iterations,
             children[0].fitness, children[children_size - 1].fitness,
             average_fitness);
      if (children[0].fitness < best_fitness) {
        if (return_data) {
          memcpy(return_data->best_path, children[0].path, sizeof(int) * graph->n);
        }
        best_fitness = children[0].fitness;
        current_same_best = 0;
      } else {
        ++current_same_best;
      }
      memswap(population, children, population_size * sizeof(Path));
    }
    ++iterations;
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
  gettimeofday(&end, NULL);
  if (return_data) {
    return_data->iterations = iterations;
    return_data->time = timediff(&end, &begin);
  }
  return best_fitness;
}