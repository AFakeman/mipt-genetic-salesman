#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "salesman.h"

const char* kGenerateFlag = "--generate";
const char* kFileFlag = "--file";
const size_t kGraphWeightMax = 16;

int main(int argc, char* argv[]) {
  graph_t* graph;
  size_t t;
  size_t N;
  size_t S;
  ShortestPathData result;
  FILE* stats;
  int best_fitness;
  assert(argc == 6);
  assert(sscanf(argv[1], "%lu", &t));
  assert(sscanf(argv[2], "%lu", &N));
  assert(sscanf(argv[3], "%lu", &S));
  srand(time(NULL));

  if (!strcmp(argv[4], kFileFlag)) {
    graph = graph_read_file(argv[5]);
  } else {
    assert(!strcmp(argv[4], kGenerateFlag));
    graph = graph_generate(atoi(argv[5]), kGraphWeightMax);
  }
  result.best_path = malloc(graph->n * sizeof(int));
  best_fitness = ShortestPath(graph, t, N, S, &result);
  stats = fopen("stats.txt", "w");
  fprintf(stats, "%lu %lu %lu %d %lu %lf %d\n", t, N, S, graph->n,
          result.iterations, result.time, best_fitness);
  for (int i = 0; i < graph->n; i++) {
    fprintf(stats, "%d ", result.best_path[i]);
  }
  fprintf(stats, "\n");
  free(result.best_path);
  graph_destroy(graph);
}